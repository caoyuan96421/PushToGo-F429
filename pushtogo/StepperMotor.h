/*
 * GenericStepperMotor.h
 *
 *  Created on: 2018Äê2ÔÂ8ÈÕ
 *      Author: caoyuan9642
 */

#ifndef TELESCOPE_STEPPERMOTOR_H_
#define TELESCOPE_STEPPERMOTOR_H_

#include <stdint.h>

/**
 * Interface of a generic stepper motor
 */
class StepperMotor
{
public:
	typedef enum
	{
		STEP_FORWARD = 0, STEP_BACKWARD = 1
	} stepdir_t;

public:
	StepperMotor()
	{
	}
	virtual ~StepperMotor()
	{
	}

	virtual void start(stepdir_t dir) = 0;
	virtual void stop() = 0;


	/*Get the fractional step count. */
	virtual double getStepCount() = 0;


	virtual void setStepCount(double) = 0;

	/** Set frequency of the stepping. In unit of full steps per second
	 * @param freq Target frequency
	 * @return Actual frequency been set to
	 */
	double setFrequency(double freq);

};

inline StepperMotor::stepdir_t operator!(const StepperMotor::stepdir_t dir)
{
	if (dir == StepperMotor::STEP_FORWARD)
		return StepperMotor::STEP_BACKWARD;
	else
		return StepperMotor::STEP_FORWARD;
}

#endif /* TELESCOPE_STEPPERMOTOR_H_ */

