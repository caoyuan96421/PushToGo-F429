/*
 * Axis.cpp
 *
 *  Created on: 2018Äê2ÔÂ24ÈÕ
 *      Author: caoyuan9642
 */

#include <Axis.h>

#define AXIS_DEBUG 0

static inline double min(double x, double y)
{
	return (x < y) ? x : y;
}
Axis::Axis(double stepsPerDeg, StepperMotor *stepper, const char *name) :
		stepsPerDeg(stepsPerDeg), stepper(stepper), axisName(name), currentSpeed(
				0), currentDirection(AXIS_ROTATE_POSITIVE), slewSpeed(
				TelescopeConfiguration::getDouble("default_slew_speed")), trackSpeed(
				TelescopeConfiguration::getDouble(
						"default_track_speed_sidereal") * sidereal_speed), guideSpeed(
				TelescopeConfiguration::getDouble(
						"default_guide_speed_sidereal") * sidereal_speed), status(
				AXIS_STOPPED), slewState(AXIS_NOT_SLEWING), slew_finish_sem(0,
				1)
{
	if (stepsPerDeg <= 0)
		error("Axis: steps per degree must be > 0");

	if (!stepper)
		error("Axis: stepper must be defined");

	taskName = new char[strlen(name) + 10];
	strcpy(taskName, name);
	strcat(taskName, " task");
	/*Start the task-handling thread*/
	task_thread = new Thread(osPriorityAboveNormal,
	OS_STACK_SIZE, NULL, taskName);
	task_thread->start(callback(this, &Axis::task));
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
	task_thread->terminate();
	delete task_thread;
	delete taskName;
}

void Axis::task()
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
		osEvent evt = task_queue.get();
		if (evt.status == osEventMessage)
		{
			/*New message arrived. Copy the data and free is asap*/
			message = (msg_t*) evt.value.p;
			signal = message->signal;
			value = message->value;
			dir = message->dir;
			task_pool.free(message);
		}
		else
		{
			/*Error*/
			debug("%s: Error fetching the task queue.\n", axisName);
			continue;
		}
		debug_if(AXIS_DEBUG, "%s: MSG %d %f %d 0x%8x\n", axisName, signal,
				value, dir);

		/*Check the type of the signal, and start corresponding operations*/
		switch (signal)
		{
		case msg_t::SIGNAL_SLEW_TO:
			if (status == AXIS_STOPPED)
			{
				slew(dir, value, false);
			}
			else
			{
				debug("%s: being slewed while not in STOPPED mode.\n",
						axisName);
			}
			debug_if(0, "%s: SIG SLEW 0x%08x\n", axisName, Thread::gettid());
			slew_finish_sem.release(); /*Send a signal so that the caller is free to run*/
			break;
		case msg_t::SIGNAL_SLEW_INDEFINITE:
			if (status == AXIS_STOPPED || status == AXIS_INERTIAL)
			{
				slew(dir, 0.0, true);
			}
			else
			{
				debug("%s: being slewed while not in STOPPED mode.\n",
						axisName);
			}
			break;
		case msg_t::SIGNAL_TRACK:
			if (status == AXIS_STOPPED)
			{
				track(dir);
			}
			else
			{
				debug("%s: trying to track while not in STOPPED mode.\n",
						axisName);
			}
			break;
		default:
			debug("%s: undefined signal %d\n", axisName, message->signal);
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
	if (dir == AXIS_ROTATE_STOP)
	{
		debug("%s: invalid direction.\n", axisName);
	}

	slew_mode(); // Switch to slew mode
	Thread::signal_clr(AXIS_STOP_SIGNAL | AXIS_EMERGE_STOP_SIGNAL); // Clear flags
	bool isInertial = (status == AXIS_INERTIAL);
	status = AXIS_SLEWING;
	slewState = AXIS_NOT_SLEWING;
	currentDirection = dir;
	stepdir_t sd = (dir == AXIS_ROTATE_POSITIVE) ? STEP_FORWARD : STEP_BACKWARD;

	/* Calculate the angle to rotate*/
	bool skip_slew = false;
	bool skip_correction = false;
	double angleDeg = getAngleDeg();
	double delta;
	delta = (dest - angleDeg) * (dir == AXIS_ROTATE_POSITIVE ? 1 : -1); /*delta is the actual angle to rotate*/
	delta = remainder(delta - 180.0, 360.0) + 180.0; /*Shift to 0-360 deg*/

	double startSpeed = 0;
	double endSpeed = slewSpeed, waitTime;
	unsigned int ramp_steps;
	double acceleration = TelescopeConfiguration::getDouble("acceleration");

	if (!indefinite)
	{
		// Ensure that delta is more than the minimum slewing angle, calculate the correct endSpeed and waitTime
		if (delta > TelescopeConfiguration::getDouble("min_slew_angle"))
		{
			/*The motion angle is decreased to ensure the correction step is in the same direction*/
			delta = delta
					- 0.5 * TelescopeConfiguration::getDouble("min_slew_angle");

			double angleRotatedDuringAcceleration;

			/* The actual endSpeed we get might be different than this due to the finite time resolution of the stepper driver
			 * Here we set the dummy frequency and obtain the actual frequency it will be set to, so that the slewing time will be more accurate
			 */

			// Calculate the desired endSpeed. If delta is small, then endSpeed will correspondingly be reduced
			endSpeed = min(sqrt(delta * acceleration),
					stepper->setFrequency(stepsPerDeg * endSpeed)
							/ stepsPerDeg);
			ramp_steps = (unsigned int) (endSpeed
					/ (TelescopeConfiguration::getInt("acceleration_step_time")
							/ 1000.0) / acceleration);
			if (ramp_steps < 1)
				ramp_steps = 1;

			angleRotatedDuringAcceleration = 0;
			/*Simulate the slewing process to get an accurate estimate of the actual angle that will be slewed*/
			for (unsigned int i = 1; i <= ramp_steps; i++)
			{
				double speed = stepper->setFrequency(
						stepsPerDeg * endSpeed / ramp_steps * i) / stepsPerDeg;
				angleRotatedDuringAcceleration += speed
						* (TelescopeConfiguration::getInt(
								"acceleration_step_time") / 1000.0)
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
		// If was in inertial mode, use startSpeed
		if (isInertial)
			startSpeed = currentSpeed;
		// No need for correction
		skip_correction = true;
	}

	/*Slewing -> accel, wait, decel*/
	if (!skip_slew)
	{
		int wait_ms;
		uint32_t flags;
		/*Acceleration*/
		slewState = AXIS_SLEW_ACCELERATING;
		ramp_steps = (unsigned int) ((endSpeed - startSpeed)
				/ (TelescopeConfiguration::getInt("acceleration_step_time")
						/ 1000.0) / acceleration);

		if (ramp_steps < 1)
			ramp_steps = 1;

		debug_if(AXIS_DEBUG, "%s: accelerate in %d steps\n", axisName,
				ramp_steps); // TODO: DEBUG

		for (unsigned int i = 1; i <= ramp_steps; i++)
		{
			currentSpeed = stepper->setFrequency(
					stepsPerDeg
							* ((endSpeed - startSpeed) / ramp_steps * i
									+ startSpeed)) / stepsPerDeg; // Set and update currentSpeed with actual speed

			if (i == 1)
				stepper->start(sd);

			/*Monitor whether there is a stop/emerge stop signal*/
			uint32_t flags = osThreadFlagsWait(
			AXIS_STOP_SIGNAL | AXIS_EMERGE_STOP_SIGNAL, osFlagsWaitAny,
					TelescopeConfiguration::getInt("acceleration_step_time"));

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
		slewState = AXIS_SLEW_CONSTANT_SPEED;
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
		slewState = AXIS_SLEW_DECELERATING;
		endSpeed = currentSpeed;
		ramp_steps = (unsigned int) (currentSpeed
				/ (TelescopeConfiguration::getInt("acceleration_step_time")
						/ 1000.0) / acceleration);

		if (ramp_steps < 1)
			ramp_steps = 1;

		debug_if(AXIS_DEBUG, "%s: decelerate in %d steps from %f\n", axisName,
				ramp_steps, endSpeed); // TODO: DEBUG

		for (unsigned int i = ramp_steps - 1; i >= 1; i--)
		{
			currentSpeed = stepper->setFrequency(
					stepsPerDeg * endSpeed / ramp_steps * i) / stepsPerDeg; // set and update accurate speed
			// Wait. Now we only handle EMERGENCY STOP signal, since stop has been handled already
			flags = osThreadFlagsWait(
			AXIS_EMERGE_STOP_SIGNAL | AXIS_STOP_KEEPSPEED_SIGNAL,
			osFlagsWaitAny,
					TelescopeConfiguration::getInt("acceleration_step_time"));

			if (flags != osFlagsErrorTimeout)
			{
				if (flags & AXIS_EMERGE_STOP_SIGNAL)
				{
					// We're stopped!
					skip_correction = true;
					goto emerge_stop;
				}
				else if (flags & AXIS_STOP_KEEPSPEED_SIGNAL)
				{
					// Keep current speed
					status = AXIS_INERTIAL;
					slewState = AXIS_NOT_SLEWING;
					return;
				}
			}
		}

		emerge_stop:
		/*Fully pull-over*/
		slewState = AXIS_NOT_SLEWING;
		stepper->stop();
		currentSpeed = 0;
	}

	if (!skip_correction)
	{
		// Switch mode
		correction_mode();
		double correctionSpeed = TelescopeConfiguration::getDouble(
				"correction_speed_sidereal") * sidereal_speed;
		/*Use correction to goto the final angle with high resolution*/
		angleDeg = getAngleDeg();
		debug_if(AXIS_DEBUG, "%s: correct from %f to %f deg\n", axisName,
				angleDeg, dest); // TODO: DEBUG

		double diff = remainder(angleDeg - dest, 360.0);
		if (diff > TelescopeConfiguration::getDouble("max_correction_angle"))
		{
			debug(
					"%s: correction too large: %f. Check hardware configuration.\n",
					axisName, diff);
			status = AXIS_STOPPED;
			return;
		}

		int nTry = 3; // Try 3 corrections at most
		while (--nTry
				&& fabsf(diff)
						> TelescopeConfiguration::getDouble(
								"correction_tolerance"))
		{
			/*Determine correction direction and time*/
			sd = (diff > 0.0) ? STEP_BACKWARD : STEP_FORWARD;

			/*Perform correction*/
			currentSpeed = stepper->setFrequency(stepsPerDeg * correctionSpeed)
					/ stepsPerDeg; // Set and update actual speed

			int correctionTime_ms = (int) (fabs(diff) / currentSpeed * 1000); // Use the accurate speed for calculating time

			debug_if(AXIS_DEBUG,
					"%s: correction: from %f to %f deg. time=%d ms\n", axisName,
					angleDeg, dest, correctionTime_ms); //TODO: DEBUG
			if (correctionTime_ms
					< TelescopeConfiguration::getInt("min_correction_time"))
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
	if (trackSpeed != 0 && dir != AXIS_ROTATE_STOP)
	{
		stepdir_t sd =
				(dir == AXIS_ROTATE_POSITIVE) ? STEP_FORWARD : STEP_BACKWARD;
		currentSpeed = stepper->setFrequency(trackSpeed * stepsPerDeg)
				/ stepsPerDeg;
		currentDirection = dir;
		stepper->start(sd);
	}
	else
	{
		// For DEC axis
		dir = AXIS_ROTATE_STOP;
		trackSpeed = 0;
		currentSpeed = 0;
		currentDirection = AXIS_ROTATE_POSITIVE;
	}
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

						double newSpeed =
								(guide_dir == currentDirection) ?
										trackSpeed + guideSpeed :
										trackSpeed - guideSpeed; /*Determine speed in the original direction (currentDirection)*/

						// Clamp to maximum guide time
						guideTime_ms = abs(guideTime_ms);
						if (guideTime_ms
								> TelescopeConfiguration::getInt(
										"max_guide_time"))
						{
							debug("Axis: Guiding time too long: %d ms\n",
									abs(guideTime_ms));
							guideTime_ms = TelescopeConfiguration::getInt(
									"max_guide_time");
						}

						bool dirswitch = false;
						if (newSpeed > 0)
						{
							currentSpeed = stepper->setFrequency(
									newSpeed * stepsPerDeg) / stepsPerDeg; //set and update accurate speed
							if (trackSpeed == 0)
							{
								// For DEC, we also need to start the motor
								stepper->start(STEP_FORWARD); // Reverse direction
							}
						}
						else if (newSpeed < 0)
						{
							//
							stepper->stop();
							currentSpeed = stepper->setFrequency(
									-newSpeed * stepsPerDeg) / stepsPerDeg; //set and update accurate speed
							stepper->start(
									(currentDirection == AXIS_ROTATE_POSITIVE) ?
											STEP_BACKWARD : STEP_FORWARD); // Reverse direction
							dirswitch = true;
						}
						else
						{
							// newSpeed == 0, just stop the motor
							currentSpeed = 0;
							stepper->stop();
							dirswitch = true; // Make sure to recover the original speed
						}

						uint32_t flags = osThreadFlagsWait(
						AXIS_STOP_SIGNAL | AXIS_EMERGE_STOP_SIGNAL,
						osFlagsWaitAny, guideTime_ms);
						if (flags != osFlagsErrorTimeout)
						{
							//break and stop;
							stopped = true;
							break;
						}
						if (dirswitch || trackSpeed == 0)
						{
							stepper->stop();
							// Restart the motor in original direction
							if (trackSpeed != 0)
							{
								stepper->start(
										(currentDirection
												== AXIS_ROTATE_POSITIVE) ?
												STEP_FORWARD : STEP_BACKWARD); // Reverse direction
							}
						}
						// Restore to normal speed
						if (trackSpeed != 0)
							currentSpeed = stepper->setFrequency(
									trackSpeed * stepsPerDeg) / stepsPerDeg;
						else
							currentSpeed = 0;

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

