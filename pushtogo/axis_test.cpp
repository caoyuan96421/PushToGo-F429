/*
 * SimulatedStepper_test.cpp
 *
 *  Created on: 2018��2��11��
 *      Author: caoyuan9642
 */

#include <SimulatedStepper_HW.h>
#include <AdaptiveAxis.h>
#include <AMIS30543StepperDriver.h>
#include "mbed.h"

Thread report;
SPI spi3(PC_12, PC_11, PB_3_ALT0);
AMIS30543StepperDriver stepper_real(&spi3, PE_3, PB_7);

//SPI spi4(PE_6, PE_5, PE_2);
//AMIS30543StepperDriver stepper_real(&spi4, PE_4, PC_8);

const float stepsPerDeg = MBED_CONF_PUSHTOGO_STEPS_PER_REVOLUTION
		* MBED_CONF_PUSHTOGO_REDUCTION_FACTOR / 360.0f;

AdaptiveAxis axis(stepsPerDeg, &stepper_real, false, "DEC Axis");

InterruptIn button(USER_BUTTON);

void xprintf(const char *, ...);

void report_stepper()
{
	while (1)
	{
		xprintf("Position: %f deg. Status: %d. Speed: %f", axis.getAngleDeg(),
				axis.getStatus(), axis.getCurrentSpeed());
		wait(0.1);
	}
}

void emerg_stop(Axis *pa)
{
	pa->emergency_stop();
}

void test_stepper()
{
	Timeout to, t2;
	report.start(report_stepper);
	button.rise(callback(emerg_stop, &axis));

	// Test 1
//	wait(2);
//	xprintf("Test 1: Slew to -10deg at 1 deg/s");
//
//	axis.slewTo(AXIS_ROTATE_NEGATIVE, -10);
//
//	// Test 2
//	wait(2);
//	xprintf("Test 2: Slew to 10deg at 4 deg/s");
//
//	axis.setSlewSpeed(4);
//	axis.slewTo(AXIS_ROTATE_POSITIVE, 10);
//
//	// Test 3
//	wait(2);
//	xprintf(
//			"Test 3: Slew to 20deg at 4 deg/s and emergency stop after 5s, then to 0 degree");
//	axis.setSlewSpeed(4);
//	to.attach(callback(emerg_stop, &axis), 5);
//	t2.attach(callback(emerg_stop, &axis), 4.7);
////	to.attach(callback(emerg_stop, &axis), 10);
//	axis.slewTo(AXIS_ROTATE_POSITIVE, 20);
//	wait(0.31);
//	axis.slewTo(AXIS_ROTATE_NEGATIVE, 0);
//
//	// Test 4
//	wait(2);
//	xprintf("Test 4: Slew to 2deg at 4 deg/s");
//	axis.setSlewSpeed(4);
//	axis.slewTo(AXIS_ROTATE_POSITIVE, 2);
//
//	// Test 5
//	wait(2);
//	xprintf("Test 5: Slew to 1.8deg");
//	axis.setSlewSpeed(4);
//	axis.slewTo(AXIS_ROTATE_NEGATIVE, 1.8);
//
//	// Test 6
//	wait(2);
//	xprintf("Test 6: Slew to 1.2deg");
//	axis.setSlewSpeed(4);
//	axis.slewTo(AXIS_ROTATE_NEGATIVE, 1.2);
//
//	// Test 7
//	wait(2);
//	xprintf("Test 7: Slew to 0deg");
//	axis.setSlewSpeed(4);
//	axis.slewTo(AXIS_ROTATE_NEGATIVE, 0);
//
//	// Test 8
//	wait(2);
//	xprintf("Test 8: Slew for 15s in async mode");
//	axis.setSlewSpeed(8);
////	axis.setAcceleration(2);
//	axis.startSlewingIndefinite(AXIS_ROTATE_POSITIVE);
//
//	wait(15);
//
//	axis.stop();
//
//	wait(2);
//	axis.slewTo(AXIS_ROTATE_NEGATIVE, 0);

	xprintf("Test 9: Guiding");
	wait(2);
	axis.startTracking(AXIS_ROTATE_POSITIVE);

	for (int i = 1; i <= 3; i++)
	{
		axis.guide(AXIS_ROTATE_POSITIVE, (double) i / 10 * 4);
		wait(1);
		axis.guide(AXIS_ROTATE_NEGATIVE, (double) i / 10 * 4);
		wait(1);
	}

	axis.stop();

	wait(1);

	axis.startTracking(AXIS_ROTATE_NEGATIVE);

	// Test the queue
	axis.guide(AXIS_ROTATE_POSITIVE, 1.0);
	wait(2);
	axis.guide(AXIS_ROTATE_POSITIVE, 1.0);
	axis.guide(AXIS_ROTATE_POSITIVE, 1.0);
	wait(4);
	axis.guide(AXIS_ROTATE_POSITIVE, 0.5);
	axis.guide(AXIS_ROTATE_NEGATIVE, 0.5);
	axis.guide(AXIS_ROTATE_POSITIVE, 0.5);
	axis.guide(AXIS_ROTATE_NEGATIVE, 0.5);
	axis.guide(AXIS_ROTATE_POSITIVE, 0.5);
	axis.guide(AXIS_ROTATE_NEGATIVE, 0.5);
	axis.guide(AXIS_ROTATE_POSITIVE, 0.5);
	axis.guide(AXIS_ROTATE_NEGATIVE, 0.5);
	axis.guide(AXIS_ROTATE_POSITIVE, 0.5);
	axis.guide(AXIS_ROTATE_NEGATIVE, 0.5);
	axis.guide(AXIS_ROTATE_POSITIVE, 0.5);
	axis.guide(AXIS_ROTATE_NEGATIVE, 0.5);
	wait(10);

	xprintf("Test finished");
	report.terminate();
	axis.stop();

}
