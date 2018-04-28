/*
 * AdaptiveAxis.cpp
 *
 *  Created on: 2018Äê2ÔÂ24ÈÕ
 *      Author: caoyuan9642
 */

#include <AdaptiveAxis.h>

void AdaptiveAxis::slew_mode()
{
	this->stepper->poweron();
	this->stepper->setMicroStep(config.getMicrostepSlew());
	this->stepper->setCurrent(config.getCurrentSlew());
}

void AdaptiveAxis::track_mode()
{
	this->stepper->poweron();
	this->stepper->setMicroStep(config.getMicrostepTrack());
	this->stepper->setCurrent(config.getCurrentTrack());
}

void AdaptiveAxis::correction_mode()
{
	this->stepper->poweron();
	this->stepper->setMicroStep(config.getMicrostepCorrection());
	this->stepper->setCurrent(config.getCurrentTrack());
}

void AdaptiveAxis::idle_mode()
{
	if (config.getCurrentIdle() != 0)
		this->stepper->setCurrent(config.getCurrentIdle());
	else
		this->stepper->poweroff();
}
