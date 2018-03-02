/*
 * AdaptiveAxis.cpp
 *
 *  Created on: 2018Äê2ÔÂ24ÈÕ
 *      Author: caoyuan9642
 */

#include <AdaptiveAxis.h>

void AdaptiveAxis::slew_mode()
{
	this->stepper->setMicroStep(config.getMicrostepSlew());
	this->stepper->setCurrent(config.getCurrentSlew());
}

void AdaptiveAxis::track_mode()
{
	this->stepper->setMicroStep(config.getMicrostepTrack());
	this->stepper->setCurrent(config.getCurrentTrack());
}

void AdaptiveAxis::correction_mode()
{
	this->stepper->setMicroStep(config.getMicrostepCorrection());
	this->stepper->setCurrent(config.getCurrentTrack());
}

void AdaptiveAxis::idle_mode()
{
	this->stepper->setCurrent(config.getCurrentIdle());
}
