/*
 * em_test.cpp
 *
 *  Created on: 2018Äê2ÔÂ27ÈÕ
 *      Author: caoyuan9642
 */

#include "mbed.h"
#include "AMIS30543StepperDriver.h"
#include "EquatorialMount.h"
#include "AdaptiveAxis.h"
#include "RTCClock.h"

const float stepsPerDeg = MBED_CONF_PUSHTOGO_STEPS_PER_REVOLUTION
		* MBED_CONF_PUSHTOGO_REDUCTION_FACTOR / 360.0f;

void stop(EquatorialMount *eq)
{
	eq->stop();
}

void test_em()
{
	SPI ra_spi(PC_12, PC_11, PB_3_ALT0);
	SPI dec_spi(PE_6, PE_5, PE_2);
	AMIS30543StepperDriver ra_stepper(&ra_spi, PE_3, PB_7);
	AMIS30543StepperDriver dec_stepper(&dec_spi, PE_4, PC_8);

	AdaptiveAxis ra_axis(stepsPerDeg, &ra_stepper, false, "RA_Axis");
	AdaptiveAxis dec_axis(stepsPerDeg, &dec_stepper, false, "DEC_Axis");

	RTCClock clock;

	EquatorialMount eq(ra_axis, dec_axis, clock,
			LocationCoordinates(42.0, -73.0));

	InterruptIn button(USER_BUTTON);

	button.rise(callback(stop, &eq));

	debug("****************\nEQ Test 1: Slew to RA=0, DEC=0\n");

	eq.goTo(0, 0);

	debug("****************\nEQ Test 2: Slew fast to RA=0, DEC=0\n");

	eq.setSlewSpeed(4.0);
	eq.goTo(40, 0);

	wait(1);
	debug("****************\nEQ Test Finished.\n");
}
