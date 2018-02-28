/*
 * Axis.cpp
 *
 *  Created on: 2018Äê2ÔÂ24ÈÕ
 *      Author: caoyuan9642
 */

#include <Axis.h>

#define AXIS_DEBUG 1

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

		// Wait for next message
		osEvent evt = p->task_queue.get();
		if (evt.status == osEventMessage)
		{
			/*New message arrived. Copy the data and free is asap*/
			message = (msg_t*) evt.value.p;
			signal = message->signal;
			value = message->value;
			dir = message->dir;
			p->task_pool.free(message);
		}
		else
		{
			/*Error*/
			debug("%s: Error fetching the task queue.\n", p->axisName);
			continue;
		}
		debug_if(AXIS_DEBUG, "%s: MSG %d %f %d 0x%8x\n", p->axisName, signal,
				value, dir);

		/*Check the type of the signal, and start corresponding operations*/
		switch (signal)
		{
		case msg_t::SIGNAL_SLEW_TO:
			if (p->status == AXIS_STOPPED)
			{
				p->slew(dir, value, false);
			}
			else
			{
				debug("%s: being slewed while not in STOPPED mode.\n",
						p->axisName);
			}
			debug_if(0, "%s: SIG SLEW 0x%08x\n", p->axisName, Thread::gettid());
			p->slew_finish_sem.release(); /*Send a signal so that the caller is free to run*/
			break;
		case msg_t::SIGNAL_SLEW_INDEFINITE:
			if (p->status == AXIS_STOPPED)
			{
				p->slew(dir, 0.0, true);
			}
			else
			{
				debug("%s: being slewed while not in STOPPED mode.\n",
						p->axisName);
			}
			break;
		case msg_t::SIGNAL_TRACK:
			if (p->status == AXIS_STOPPED)
			{
				p->track(dir);
			}
			else
			{
				debug("%s: trying to track while not in STOPPED mode.\n",
						p->axisName);
			}
			break;
		default:
			debug("%s: undefined signal %d\n", p->axisName, message->signal);
		}
	}
}

void Axis::slew(axisrotdir_t dir, double dest, bool indefinite)
{
	if (!indefinite && (isnan(dest) || isinf(dest)))
	{
		debug("%s: invalid angle.\n", axisName);
		return;
	}

	slew_mode(); // Switch to slew mode
	Thread::signal_clr(AXIS_STOP_SIGNAL | AXIS_EMERGE_STOP_SIGNAL); // Clear flags
	status = AXIS_SLEWING;
	currentDirection = dir;
	stepdir_t sd = (stepdir_t) (dir ^ invert);

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

			debug_if(AXIS_DEBUG, "%s: endspeed = %f deg/s, time=%f, acc=%f\n",
					axisName, endSpeed, waitTime, acceleration);
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
		int wait_ms;
		uint32_t flags;
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
			uint32_t flags = osThreadFlagsWait(
			AXIS_STOP_SIGNAL | AXIS_EMERGE_STOP_SIGNAL, osFlagsWaitAny,
			MBED_CONF_PUSHTOGO_ACCELERATION_STEP_TIME * 1000);

			if (flags == osFlagsErrorTimeout)
			{
				/*Nothing happened, we're good*/
				continue;
			}
			else if ((flags & osFlagsError) == 0)
			{
				// We're stopped!
				skip_correction = true;
				if (flags & AXIS_EMERGE_STOP_SIGNAL)
				{
					goto emerge_stop;
				}
				else if (flags & AXIS_STOP_SIGNAL)
				{
					goto stop;
				}
			}
		}

		/*Keep slewing and wait*/
		debug_if(AXIS_DEBUG, "%s: wait for %f\n", axisName, waitTime); // TODO
		wait_ms = (isinf(waitTime)) ? osWaitForever : (int) (waitTime * 1000);

		flags = osThreadFlagsWait(
		AXIS_STOP_SIGNAL | AXIS_EMERGE_STOP_SIGNAL, osFlagsWaitAny, wait_ms); /*Wait the remaining time*/
		if (flags != osFlagsErrorTimeout)
		{	// We're stopped!
			skip_correction = true;
			if (flags & AXIS_EMERGE_STOP_SIGNAL)
			{
				goto emerge_stop;
			}
			// Normal stop will be handled automatically
		}

		stop:
		/*Now deceleration*/
		endSpeed = currentSpeed;
		ramp_steps = (unsigned int) (currentSpeed
				/ MBED_CONF_PUSHTOGO_ACCELERATION_STEP_TIME / acceleration);

		if (ramp_steps < 1)
			ramp_steps = 1;

		debug_if(AXIS_DEBUG, "%s: decelerate in %d steps\n", axisName,
				ramp_steps); // TODO: DEBUG

		for (unsigned int i = ramp_steps - 1; i >= 1; i--)
		{
			currentSpeed = stepper->setFrequency(
					stepsPerDeg * endSpeed / ramp_steps * i) / stepsPerDeg; // set and update accurate speed
			// Wait. Now we only handle EMERGENCY STOP signal, since stop has been handled already
			flags = osThreadFlagsWait(AXIS_EMERGE_STOP_SIGNAL, osFlagsWaitAny,
			MBED_CONF_PUSHTOGO_ACCELERATION_STEP_TIME * 1000);

			if (flags != osFlagsErrorTimeout)
			{
				// We're stopped!
				skip_correction = true;
				goto emerge_stop;

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
		correction_mode();
		/*Use correction to goto the final angle with high resolution*/
		angleDeg = getAngleDeg();
		debug_if(AXIS_DEBUG, "%s: correct from %f to %f deg\n", axisName,
				angleDeg, dest); // TODO: DEBUG

		double diff = remainder(angleDeg - dest, 360.0);
		if (diff > MBED_CONF_PUSHTOGO_MAX_CORRECTION_ANGLE)
		{
			debug(
					"%s: correction too large: %f. Check hardware configuration.\n",
					axisName, diff);
			status = AXIS_STOPPED;
			return;
		}

		int nTry = 3; // Try 3 corrections at most
		while (--nTry && fabsf(diff) > MBED_CONF_PUSHTOGO_CORRECTION_TOLERANCE)
		{
			/*Determine correction direction and time*/
			sd = (stepdir_t) ((diff > 0.0) ^ invert);

			/*Perform correction*/
			currentSpeed = stepper->setFrequency(stepsPerDeg * correctionSpeed)
					/ stepsPerDeg; // Set and update actual speed

			int correctionTime_ms = (int) (fabs(diff) / currentSpeed * 1000); // Use the accurate speed for calculating time

			debug_if(AXIS_DEBUG,
					"%s: correction: from %f to %f deg. time=%d ms\n", axisName,
					angleDeg, dest, correctionTime_ms); //TODO: DEBUG
			if (correctionTime_ms < MBED_CONF_PUSHTOGO_MIN_CORRECTION_TIME)
			{
				break;
			}

			/*Start, wait, stop*/
			stepper->start(sd);
			uint32_t flags = osThreadFlagsWait(AXIS_EMERGE_STOP_SIGNAL,
			osFlagsWaitAny, correctionTime_ms);
			stepper->stop();
			if (flags != osFlagsErrorTimeout)
			{
				// Emergency stop!
				goto emerge_stop2;
			}

			angleDeg = getAngleDeg();
			diff = remainder(angleDeg - dest, 360.0);
		}

		if (!nTry)
		{
			debug("%s: correction failed. Check hardware configuration.\n",
					axisName);
		}

		debug_if(AXIS_DEBUG, "%s: correction finished: %f deg\n", axisName,
				angleDeg); //TODO:DEBUG
	}
	emerge_stop2:
// Set status to stopped
	currentSpeed = 0;
	status = AXIS_STOPPED;
	idle_mode();
}

void Axis::track(axisrotdir_t dir)
{
	track_mode();
	stepdir_t sd = (stepdir_t) (dir ^ invert);
	currentSpeed = stepper->setFrequency(trackSpeed * stepsPerDeg)
			/ stepsPerDeg;
	currentDirection = dir;
	stepper->start(sd);
	status = AXIS_TRACKING;
	Thread::signal_clr(
	AXIS_STOP_SIGNAL | AXIS_EMERGE_STOP_SIGNAL | AXIS_GUIDE_SIGNAL);
	// Empty the guide queue
	while (!guide_queue.empty())
		guide_queue.get();

	while (true)
	{
		// Now we wait for SOMETHING to happen - either STOP, EMERGE_STOP or GUIDE
		uint32_t flags = osThreadFlagsWait(
		AXIS_GUIDE_SIGNAL | AXIS_STOP_SIGNAL | AXIS_EMERGE_STOP_SIGNAL,
		osFlagsWaitAny, osWaitForever);
		if ((flags & osFlagsError) == 0) // has flag
		{
			if (flags & (AXIS_STOP_SIGNAL | AXIS_EMERGE_STOP_SIGNAL))
			{
				// We stop tracking
				break;
			}
			else if (flags & AXIS_GUIDE_SIGNAL)
			{
				bool stopped = false;
				// Guide. Process all commands in the queue
				while (true)
				{
					osEvent evt = guide_queue.get(0); // try to get a message
					if (evt.status == osEventMessage)
					{
						int guideTime_ms = (int) evt.value.p;
						if (guideTime_ms == 0)
							continue; // Nothing to guide
						// Determine guide direction
						axisrotdir_t guide_dir =
								(guideTime_ms > 0) ?
										AXIS_ROTATE_POSITIVE :
										AXIS_ROTATE_NEGATIVE;
						currentSpeed =
								(currentDirection == guide_dir) ?
										trackSpeed + guideSpeed :
										trackSpeed - guideSpeed; /*Determine speed based on direction*/

						// Clamp to maximum guide time
						guideTime_ms = abs(guideTime_ms);
						if (guideTime_ms > MBED_CONF_PUSHTOGO_MAX_GUIDE_TIME)
						{
							debug("Axis: Guiding time too long: %d ms\n",
									abs(guideTime_ms));
							guideTime_ms = MBED_CONF_PUSHTOGO_MAX_GUIDE_TIME;
						}

						currentSpeed = stepper->setFrequency(
								currentSpeed * stepsPerDeg) / stepsPerDeg; //set and update accurate speed

						uint32_t flags = osThreadFlagsWait(
						AXIS_STOP_SIGNAL | AXIS_EMERGE_STOP_SIGNAL,
						osFlagsWaitAny, guideTime_ms);
						if (flags != osFlagsErrorTimeout)
						{
							//break and stop;
							stopped = true;
							break;
						}
						else
						{
							// Restore to normal speed
							currentSpeed = stepper->setFrequency(
									trackSpeed * stepsPerDeg) / stepsPerDeg;
						}
						// End guiding
					}
					else
					{
						// No more message to get. Break out
						break;
					}
				}
				if (stopped)
				{
					//Complete break out
					break;
				}
			}
		}
	}

// Stop
	currentSpeed = 0;
	stepper->stop();
	status = AXIS_STOPPED;
	idle_mode();
}

Axis::~Axis()
{
	// Wait until the axis is stopped
	if (status != AXIS_STOPPED)
	{
		stop();
		while (status != AXIS_STOPPED)
		{
			Thread::wait(100);
		}
	}

	// Terminate the task thread to prevent illegal access to destroyed objects.
	task_thread.terminate();
}
