/*
 * AdaptiveAxis.cpp
 *
 *  Created on: 2018Äê2ÔÂ24ÈÕ
 *      Author: caoyuan9642
 */

#include <AdaptiveAxis.h>

void AdaptiveAxis::slew_mode()
{
	this->stepper->setMicroStep(MBED_CONF_PUSHTOGO_MICROSTEP_SLEW);
	this->stepper->setCurrent(MBED_CONF_PUSHTOGO_CURRENT_SLEW);
}

void AdaptiveAxis::track_mode()
{
	this->stepper->setMicroStep(MBED_CONF_PUSHTOGO_MICROSTEP_TRACK);
	this->stepper->setCurrent(MBED_CONF_PUSHTOGO_CURRENT_TRACK);
}

void AdaptiveAxis::correction_mode()
{
	this->stepper->setMicroStep(MBED_CONF_PUSHTOGO_MICROSTEP_CORRECTION);
	this->stepper->setCurrent(MBED_CONF_PUSHTOGO_CURRENT_CORRECTION);
}

void AdaptiveAxis::idle_mode()
{
	this->stepper->setCurrent(MBED_CONF_PUSHTOGO_CURRENT_IDLE);
}
