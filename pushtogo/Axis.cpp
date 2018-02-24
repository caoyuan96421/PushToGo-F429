/*
 * Axis.cpp
 *
 *  Created on: 2018Äê2ÔÂ24ÈÕ
 *      Author: caoyuan9642
 */

#include <Axis.h>

extern void xprintf(const char *, ...);

static inline double min(double x, double y)
{
	return (x < y) ? x : y;
}

void Axis::task(Axis *p)
{

	/*Main loop of RotationAxis*/
	while (true)
	{
		/*Get next message*/
		msg_t *message;
		enum msg_t::sig_t signal;
		float value;
		axisrotdir_t dir;
		osThreadId_t tid;

		osEvent evt = p->task_queue.get(MBED_CONF_PUSHTOGO_UPDATE_ANGLE_PERIOD);
		if (evt.status == osEventMessage)
		{
			/*New message arrived. Copy the data and free is asap*/
			message = (msg_t*) evt.value.p;
			signal = message->signal;
			value = message->value;
			dir = message->dir;
			tid = message->tid;
			p->task_pool.free(message);
		}
		else
		{
			/*Error*/
			fprintf(stderr, "Axis: Error fetching the task queue.");
			continue;
		}

		/*Check the type of the signal, and start corresponding operations*/
		switch (signal)
		{
		case msg_t::SIGNAL_SLEW_TO:
			if (p->status == AXIS_STOPPED)
			{
				p->_slew(dir, value, false);
			}
			else
			{
				fprintf(stderr,
						"Axis: being slewed while not in STOPPED mode. ");
			}
			osThreadFlagsSet(tid, AXIS_SLEW_SIGNAL); /*Send a signal so that the caller is free to run*/
			break;
		case msg_t::SIGNAL_SLEW_INDEFINITE:
			if (p->status == AXIS_STOPPED)
			{
				p->_slew(dir, 0.0, true);
			}
			else
			{
				fprintf(stderr,
						"Axis: being slewed while not in STOPPED mode. ");
			}
			break;
		case msg_t::SIGNAL_TRACK_CONT:
			if (p->status == AXIS_STOPPED)
			{
				p->_track(dir);
			}
			else
			{
				fprintf(stderr,
						"Axis: trying to track while not in STOPPED mode. ");
			}
			break;
		case msg_t::SIGNAL_GUIDE:
			if (p->status == AXIS_TRACKING)
			{
				p->_guide(dir, value);
			}
			else
			{
				fprintf(stderr,
						"Axis: trying to guide while not in TRACKING mode. ");
			}
			osThreadFlagsSet(tid, AXIS_GUIDE_SIGNAL); /*Send a signal so that the caller is free to run*/
			break;
		default:
			fprintf(stderr, "Axis: undefined signal %d", message->signal);
		}
	}
}

void Axis::_stop()
{
	// Simply stop the motor
	stepper->stop();
}

void Axis::_slew(axisrotdir_t dir, double dest, bool indefinite)
{
	if (!indefinite && (isnan(dest) || isinf(dest)))
	{
		fprintf(stderr, "Axis: invalid angle.");
		return;
	}

	slew_mode(); // Switch to slew mode
	Thread::signal_clr(AXIS_STOP_SIGNAL | AXIS_EMERGE_STOP_SIGNAL); // Clear flags
	status = AXIS_SLEWING;
	currentDirection = dir;
	StepperMotor::stepdir_t sd = (StepperMotor::stepdir_t) (dir ^ invert);

	/* Calculate the angle to rotate*/
	bool skip_slew = false;
	bool skip_correction = false;
	double angleDeg = getAngleDeg();
	double delta;
	delta = (dest - angleDeg) * (dir == AXIS_ROTATE_POSITIVE ? 1 : -1); /*delta is the actual angle to rotate*/
	delta = remainder(delta - 180.0, 360.0) + 180.0; /*Shift to 0-360 deg*/

	double endSpeed = slewSpeed, waitTime;
	unsigned int ramp_steps;

	if (!indefinite)
	{
		// Ensure that delta is more than the minimum slewing angle, calculate the correct endSpeed and waitTime
		if (delta > MBED_CONF_PUSHTOGO_MIN_SLEW_ANGLE)
		{
			/*The motion angle is decreased to ensure the correction step is in the same direction*/
			delta = delta - 0.5 * MBED_CONF_PUSHTOGO_MIN_SLEW_ANGLE;

			double angleRotatedDuringAcceleration;

			/* The actual endSpeed we get might be different than this due to the finite time resolution of the stepper driver
			 * Here we set the dummy frequency and obtain the actual frequency it will be set to, so that the slewing time will be more accurate
			 */

			// Calculate the desired endSpeed. If delta is small, then endSpeed will correspondingly be reduced
			endSpeed = min(sqrt(delta * acceleration),
					stepper->setFrequency(stepsPerDeg * endSpeed)
							/ stepsPerDeg);
			ramp_steps = (unsigned int) (endSpeed
					/ MBED_CONF_PUSHTOGO_ACCELERATION_STEP_TIME / acceleration);
			if (ramp_steps < 1)
				ramp_steps = 1;

			angleRotatedDuringAcceleration = 0;
			/*Simulate the slewing process to get an accurate estimate of the actual angle that will be slewed*/
			for (unsigned int i = 1; i <= ramp_steps; i++)
			{
				double speed = stepper->setFrequency(
						stepsPerDeg * endSpeed / ramp_steps * i) / stepsPerDeg;
				angleRotatedDuringAcceleration += speed
						* MBED_CONF_PUSHTOGO_ACCELERATION_STEP_TIME
						* (i == ramp_steps ? 1 : 2); // Count both acceleration and deceleration
			}

			waitTime = (delta - angleRotatedDuringAcceleration) / endSpeed;
			if (waitTime < 0.0)
				waitTime = 0.0; // With the above calculations, waitTime should no longer be zero. But if it happens to be so, let the correction do the job

			xprintf("Axis: endspeed = %f deg/s, time=%f, acc=%f", endSpeed,
					waitTime, acceleration);
		}
		else
		{
			// Angle difference is too small, skip slewing
			skip_slew = true;
		}
	}
	else
	{
		// Indefinite slewing mode
		waitTime = INFINITY;
		// No need for correction
		skip_correction = true;
	}

	/*Slewing -> accel, wait, decel*/
	if (!skip_slew)
	{
		/*Acceleration*/
		ramp_steps = (unsigned int) (endSpeed
				/ MBED_CONF_PUSHTOGO_ACCELERATION_STEP_TIME / acceleration);

		if (ramp_steps < 1)
			ramp_steps = 1;

		for (unsigned int i = 1; i <= ramp_steps; i++)
		{
			currentSpeed = stepper->setFrequency(
					stepsPerDeg * endSpeed / ramp_steps * i) / stepsPerDeg; // Set and update currentSpeed with actual speed

			if (i == 1)
				stepper->start(sd);

			/*Monitor whether there is a stop/emerge stop signal*/
			osEvent ev = Thread::signal_wait(
			AXIS_STOP_SIGNAL | AXIS_EMERGE_STOP_SIGNAL,
			MBED_CONF_PUSHTOGO_ACCELERATION_STEP_TIME * 1000);

			if (ev.status == osEventTimeout)
			{
				/*Nothing happened, we're good*/
				continue;
			}
			else if (ev.status == osEventSignal)
			{
				// We're stopped!
				skip_correction = true;
				if (ev.value.signals & AXIS_EMERGE_STOP_SIGNAL)
				{
					goto emerge_stop;
				}
				else if (ev.value.signals & AXIS_STOP_SIGNAL)
				{
					goto stop;
				}
			}
		}

		/*Keep slewing and wait*/
		xprintf("Axis: wait for %f", waitTime);
		int wait_ms =
				(isinf(waitTime)) ? osWaitForever : (int) (waitTime * 1000);

		osEvent ev = Thread::signal_wait(
		AXIS_STOP_SIGNAL | AXIS_EMERGE_STOP_SIGNAL, wait_ms); /*Wait the remaining time*/
		if (ev.status == osEventSignal)
		{	// We're stopped!
			skip_correction = true;
			if (ev.value.signals & AXIS_EMERGE_STOP_SIGNAL)
			{
				goto emerge_stop;
			}
			// Normal stop will be handled automatically
		}

		stop:
		/*Now deceleration*/
		endSpeed = currentSpeed;
		unsigned int ramp_steps = (unsigned int) (currentSpeed
				/ MBED_CONF_PUSHTOGO_ACCELERATION_STEP_TIME / acceleration);

		if (ramp_steps < 1)
			ramp_steps = 1;

		xprintf("Axis: decelerate in %d steps", ramp_steps); // TODO: DEBUG

		for (unsigned int i = ramp_steps - 1; i >= 1; i--)
		{
			currentSpeed = stepper->setFrequency(
					stepsPerDeg * endSpeed / ramp_steps * i) / stepsPerDeg; // set and update accurate speed
			// Wait. Now we only handle EMERGENCY STOP signal, since stop has been handled already
			osEvent ev = Thread::signal_wait(AXIS_EMERGE_STOP_SIGNAL,
			MBED_CONF_PUSHTOGO_ACCELERATION_STEP_TIME * 1000);

			if (ev.status == osEventTimeout)
			{
				/*Nothing happened, we're good*/
				continue;
			}
			else if (ev.status == osEventSignal)
			{
				// We're stopped!
				skip_correction = true;
				if (ev.value.signals & AXIS_EMERGE_STOP_SIGNAL)
				{
					goto emerge_stop;
				}
			}
		}

		emerge_stop:
		/*Fully pull-over*/
		stepper->stop();
		currentSpeed = 0;
	}

	if (!skip_correction)
	{
		// Switch mode
		correct_mode();
		/*Use correction to goto the final angle with high resolution*/
		angleDeg = getAngleDeg();
		xprintf("Axis: correct from %f to %f deg", angleDeg, dest); // TODO: DEBUG

		double diff = remainder(angleDeg - dest, 360.0);
		if (diff > MBED_CONF_PUSHTOGO_MAX_CORRECTION_ANGLE)
		{
			fprintf(stderr,
					"Axis: correction too large: %f. Check hardware configuration.",
					diff);
			status = AXIS_STOPPED;
			return;
		}

		int nTry = 3; // Try 3 corrections at most
		while (--nTry && fabsf(diff) > MBED_CONF_PUSHTOGO_CORRECTION_TOLERANCE)
		{
			/*Determine correction direction and time*/
			sd = (StepperMotor::stepdir_t) ((diff > 0.0) ^ invert);

			/*Perform correction*/
			currentSpeed = stepper->setFrequency(stepsPerDeg * correctionSpeed)
					/ stepsPerDeg; // Set and update actual speed

			float correctionTime = (float) (fabsf(diff)) / currentSpeed; // Use the accurate speed for calculating time

			xprintf("Axis: correction: from %f to %f deg. time=%f", angleDeg,
					dest, correctionTime); //TODO: DEBUG
			if (correctionTime < MBED_CONF_PUSHTOGO_MIN_CORRECTION_TIME)
			{
				break;
			}

			/*Start, wait, stop*/
			stepper->start(sd);
			osEvent ev = Thread::signal_wait(AXIS_EMERGE_STOP_SIGNAL,
					(int) (correctionTime * 1000));
			stepper->stop();
			if (ev.status == osEventSignal)
			{
				// Emergency stop!
				goto emerge_stop2;
			}

			angleDeg = getAngleDeg();
			diff = remainder(angleDeg - dest, 360.0);
		}

		if (!nTry)
		{
			fprintf(stderr,
					"Axis: correction failed. Check hardware configuration.");
		}

		xprintf("Axis: correction finished: %f deg", angleDeg); //TODO:DEBUG
	}
	emerge_stop2:
	// Set status to stopped
	currentSpeed = 0;
	status = AXIS_STOPPED;
}

void Axis::_track(axisrotdir_t dir)
{
	track_mode();
	StepperMotor::stepdir_t sd = (StepperMotor::stepdir_t) (dir ^ invert);
	currentSpeed = stepper->setFrequency(trackSpeed * stepsPerDeg)
			/ stepsPerDeg;
	currentDirection = dir;
	stepper->start(sd);
	status = AXIS_TRACKING;
}

void Axis::_guide(axisrotdir_t dir, float duration)
{
	if (duration > MBED_CONF_PUSHTOGO_MAX_GUIDE_TIME)
	{
		fprintf(stderr, "Axis: Guiding time too long: %f seconds", duration);
		return;
	}

	currentSpeed =
			(currentDirection == dir) ?
					trackSpeed + guideSpeed : trackSpeed - guideSpeed; /*Determine speed based on direction*/

	currentSpeed = stepper->setFrequency(currentSpeed * stepsPerDeg)
			/ stepsPerDeg; //set and update accurate speed

	wait(duration);

	currentSpeed = stepper->setFrequency(trackSpeed * stepsPerDeg)
			/ stepsPerDeg;
}
