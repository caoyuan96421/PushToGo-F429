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
	this->stepper->setMicroStep(
			TelescopeConfiguration::getInt("microstep_slew"));
	this->stepper->setCurrent(
			TelescopeConfiguration::getDouble("current_slew"));
}

void AdaptiveAxis::track_mode()
{
	this->stepper->poweron();
	this->stepper->setMicroStep(
			TelescopeConfiguration::getInt("microstep_track"));
	this->stepper->setCurrent(
			TelescopeConfiguration::getDouble("current_track"));
}

void AdaptiveAxis::correction_mode()
{
	this->stepper->poweron();
	this->stepper->setMicroStep(
			TelescopeConfiguration::getInt("microstep_correction"));
	this->stepper->setCurrent(
			TelescopeConfiguration::getDouble("current_correction"));
}

void AdaptiveAxis::idle_mode()
{
	double idle_current = TelescopeConfiguration::getDouble("current_idle");
	if (idle_current != 0)
		this->stepper->setCurrent(idle_current);
	else
		this->stepper->poweroff();
}
