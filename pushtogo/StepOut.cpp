/*
 #include <StepOut.h>
 * ControllablePWMOut.cpp
 *
 *  Created on: 2018��2��9��
 *      Author: caoyuan9642
 */

#include "mbed.h"
#include <StepOut.h>

void StepOut::start()
{
	if (status == IDLE && freq > 0) // Start only when idle and frequency is not zero
	{
		core_util_critical_section_enter();
		status = STEPPING;
		this->write(0.5f);
		tim.reset();
		core_util_critical_section_exit();
	}
}

void StepOut::stop()
{
	if (status == STEPPING)
	{
		core_util_critical_section_enter();
		status = IDLE;
		this->write(0);
		stepCount += (int64_t) (((double) tim.read_us()) / 1e6 * freq);
		core_util_critical_section_exit();
	}
}

double StepOut::setFrequency(double frequency)
{
	if (frequency > 0)
	{
		int us_period = ceil(1.0E6 / frequency); /*Ceil to the next microsecond*/
		if (status == IDLE)
			this->period_us(us_period);
		else
		{
			core_util_critical_section_enter();
			stop(); /*Stop to correctly update the stepCount*/
			this->period_us(us_period);
			start();
			core_util_critical_section_exit();
		}
		freq = 1.0E6 / us_period; // get CORRECT frequency!
	}
	else
	{
		// frequency=0 effectively means stop
		if (status == STEPPING)
		{
			stop();
		}
		freq = 0;
	}
	return freq; // Return the accurate period
}

void StepOut::resetCount()
{
	core_util_critical_section_enter();
	stepCount = 0;
	if (status == STEPPING)
		tim.reset();
	core_util_critical_section_exit();
}

int64_t StepOut::getCount()
{
	if (status == IDLE)
		return stepCount;
	else
	{
		return stepCount + (int64_t) (freq * tim.read_us() / 1.0E6); /*Calculate count at now*/
	}
}

