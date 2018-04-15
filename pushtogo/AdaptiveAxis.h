/*
 * AdaptiveAxis.h
 *
 *  Created on: 2018Äê2ÔÂ24ÈÕ
 *      Author: caoyuan9642
 */

#ifndef PUSHTOGO_ADAPTIVEAXIS_H_
#define PUSHTOGO_ADAPTIVEAXIS_H_

#include <Axis.h>

/**
 * Implements class Axis that allows different modes for slewing and tracking
 */
class AdaptiveAxis: public Axis
{
public:
	AdaptiveAxis(double stepsPerDeg, StepperMotor *stepper,
			TelescopeConfiguration &config, const char *name = "Axis") :
			Axis(stepsPerDeg, stepper, config, name)
	{
		idle_mode(); // Initialize as IDLE
	}
	virtual ~AdaptiveAxis()
	{
	}

protected:

	void slew_mode();
	void track_mode();
	void correction_mode();
	void idle_mode();
};

#endif /* PUSHTOGO_ADAPTIVEAXIS_H_ */
