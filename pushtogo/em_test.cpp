/*
 * em_test.cpp
 *
 *  Created on: 2018��2��27��
 *      Author: caoyuan9642
 */

#include "mbed.h"
#include "AMIS30543StepperDriver.h"
#include "EquatorialMount.h"
#include "AdaptiveAxis.h"
#include "RTCClock.h"
#include "telescope_hardware.h"

void stop(EquatorialMount *eq)
{
	eq->stop();
}

void test_em()
{

	EquatorialMount &eq = telescopeHardwareInit();

	InterruptIn button(USER_BUTTON);

	button.rise(callback(stop, &eq));

	eq.setSlewSpeed(4);
	eq.setAcceleration(4.0);

	debug("****************\nEQ Test 1: Slew to RA=0, DEC=0\n");

	eq.goTo(0, 0);

	wait(1);
	debug("****************\nEQ Test 2: Slew fast to RA=0, DEC=0\n");

	eq.goTo(40, 0);

	wait(1);
	debug("****************\nEQ Test 3: Nudge east for 5s\n");
	eq.startNudge(NUDGE_EAST);
	wait(5);
	eq.stopWait();
	eq.updatePosition();
	eq.printPosition();

	wait(1);
	debug("****************\nEQ Test 4: Nudge E/W/S/N for 5s\n");
	debug("E\n");
	eq.startNudge(NUDGE_EAST);
	wait(5);
	debug("NE\n");
	eq.startNudge(NUDGE_NORTHEAST);
	wait(5);
	debug("SE\n");
	eq.startNudge(NUDGE_SOUTHEAST);
	wait(5);
	debug("NW\n");
	eq.startNudge(NUDGE_NORTHWEST);
	wait(5);
	debug("S\n");
	eq.startNudge(NUDGE_SOUTH);
	wait(5);

//	eq.nudgeOn(NUDGE_NONE);
	eq.stopNudge();
	eq.updatePosition();
	eq.printPosition();

	wait(1);
	debug("****************\nEQ Test 5: Tracking and nudging\n");
	eq.setSlewSpeed(2 * sidereal_speed);
	eq.startTracking();
	wait(3);
	eq.startNudge(NUDGE_EAST);
	wait(3);
	eq.startNudge(NUDGE_WEST);
	wait(3);
	wait(3);
	eq.startNudge(NUDGE_EAST);
	wait(3);
	eq.startNudge(NUDGE_WEST);
	eq.stopNudge();
	wait(4);
	eq.stopWait();

	wait(2);
	eq.setSlewSpeed(0.5 * sidereal_speed);
	eq.startTracking();
	wait(3);
	eq.startNudge(NUDGE_EAST);
	wait(3);
	eq.startNudge(NUDGE_WEST);
	wait(3);
	eq.startNudge(NUDGE_EAST);
	wait(3);
	eq.startNudge(NUDGE_WEST);
	wait(3);
	eq.stopNudge();
	wait(4);
	eq.stopWait();

	wait(2);
	eq.setSlewSpeed(16 * sidereal_speed);
	eq.startTracking();
	wait(3);
	eq.startNudge(NUDGE_EAST);
	wait(3);
	eq.startNudge(NUDGE_WEST);
	wait(3);
	eq.startNudge(NUDGE_EAST);
	wait(3);
	eq.startNudge(NUDGE_WEST);
	wait(3);
	eq.stopNudge();
	wait(4);
	eq.stopWait();

	wait(1);
	debug("****************\nEQ Test Finished.\n");

//	while(1){
//		Thread::wait(1000);
//		Thread::yield();
//	}
}
