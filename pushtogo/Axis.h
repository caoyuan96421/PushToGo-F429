/*
 * Axis.h
 *
 *  Created on: 2018Äê2ÔÂ24ÈÕ
 *      Author: caoyuan9642
 */

#ifndef PUSHTOGO_AXIS_H_
#define PUSHTOGO_AXIS_H_

#include "StepperMotor.h"
#include <math.h>
#include "mbed.h"

static const double sidereal_speed = 0.00417807462f; /* deg / s */
//#define AXIS_SLEW_SIGNAL			0x00010000
#define AXIS_GUIDE_SIGNAL			0x00020000
#define AXIS_STOP_SIGNAL			0x00040000
#define AXIS_EMERGE_STOP_SIGNAL		0x00080000

/**
 * status of the Axis object
 */
typedef enum
{
	AXIS_STOPPED = 0, AXIS_SLEWING, AXIS_TRACKING,
} axisstatus_t;

/** Define the rotation direction
 * AXIS_ROTATE_POSITIVE: +angle
 * AXIS_ROTATE_NEGATIVE: -angle
 */
typedef enum
{
	AXIS_ROTATE_POSITIVE = 0, AXIS_ROTATE_NEGATIVE = 1
} axisrotdir_t;

/** General Rotating Axis class
 * Handles low-level stepper timing, calculates the speed and distance to rotate
 * API provides comprehensive slewing, tracking and guiding.
 */
class Axis
{
public:

	/**
	 * Create a RotationAxis object
	 * @param stepsPerDeg Steps per degree of the stepper motor
	 * @param stepper Pointer to stepper driver to use
	 * @param invert Whether the stepper direction should be inverted
	 */
	Axis(double stepsPerDeg, StepperMotor *stepper, bool invert = false,
			const char *name = "Axis") :
			stepsPerDeg(stepsPerDeg), stepper(stepper), invert(invert), axisName(
					name), currentSpeed(0), currentDirection(
					AXIS_ROTATE_POSITIVE), slewSpeed(2), trackSpeed(
					sidereal_speed), correctionSpeed(32.0f * sidereal_speed), guideSpeed(
					0.5f * sidereal_speed), acceleration(1), status(
					AXIS_STOPPED), task_thread(osPriorityRealtime,
			OS_STACK_SIZE, NULL, "Axis_task"), slew_finish_sem(0, 1)
	{
		if (stepsPerDeg <= 0)
			error("Axis: steps per degree must be > 0");

		if (!stepper)
			error("Axis: stepper must be defined");

		/*Start the task-handling thread*/
		task_thread.start(callback(this->task, this));
	}

	virtual ~Axis();

	/**
	 * Perform a goto to a specified angle (in Radian) in the specified direction with slewing rate
	 * @param dir Rotation direction
	 * @param angleDeg Angle to rotate
	 * @return osStatus
	 */
	osStatus startSlewTo(axisrotdir_t dir, double angle)
	{
		msg_t *message = task_pool.alloc();
		if (!message)
		{
			return osErrorNoMemory;
		}
		message->signal = msg_t::SIGNAL_SLEW_TO;
		message->value = angle;
		message->dir = dir;
		osStatus s;

		debug_if(0, "%s: CLR SLEW 0x%08x\n", axisName, Thread::gettid());
		slew_finish_sem.wait(0); // Make sure the semaphore is cleared. THIS MUST BE DONE BEFORE THE MESSAGE IS ENQUEUED

		if ((s = task_queue.put(message)) != osOK)
		{
			task_pool.free(message);
			return s;
		}

		return osOK;
	}

	/**
	 * Wait for a slew to finish. Must be called after and only once after a call to startSlewTo, from the same thread
	 */
	osStatus waitForSlew()
	{
		debug_if(0, "%s: WAIT SLEW 0x%08x\n", axisName, Thread::gettid());
		return slew_finish_sem.wait();
	}

	/** BLOCKING
	 * Perform a goto to a specified angle (in Radian) in the specified direction with slewing rate
	 * It will perform an acceleration, a GoTo, and a deceleration before returning
	 * @param dir Rotation direction
	 * @param angleDeg Angle to rotate
	 * @return osStatus
	 */
	osStatus slewTo(axisrotdir_t dir, double angle)
	{
		osStatus s;
		if ((s = startSlewTo(dir, angle)) != osOK)
			return s;
		return waitForSlew();
	}

	/** Perform a continuous slewing, until stop() is called
	 * @param dir Direction to start continuous slewing
	 * @return osStatus
	 * @sa{RotationAxis::stop}
	 */
	osStatus startSlewingIndefinite(axisrotdir_t dir)
	{
		msg_t *message = task_pool.alloc();
		if (!message)
		{
			return osErrorNoMemory;
		}
		message->signal = msg_t::SIGNAL_SLEW_INDEFINITE;
		message->dir = dir;
		osStatus s;
		if ((s = task_queue.put(message)) != osOK)
		{
			task_pool.free(message);
			return s;
		}

		return osOK;
	}

	/** Start tracking, until stop() is called
	 * @param dir Direction to start continuous slewing
	 * @return osStatus
	 * @sa{RotationAxis::stop}
	 */
	osStatus startTracking(axisrotdir_t dir)
	{
		msg_t *message = task_pool.alloc();
		if (!message)
		{
			return osErrorNoMemory;
		}
		message->signal = msg_t::SIGNAL_TRACK;
		message->dir = dir;
		osStatus s;
		if ((s = task_queue.put(message)) != osOK)
		{
			task_pool.free(message);
			return s;
		}

		return osOK;
	}

	/**
	 * Perform a goto to a specified angle (in Radian) in the specified direction with slewing rate
	 * It will perform an acceleration, a GoTo, and a deceleration before returning
	 * @param dir Rotation direction
	 * @param angleDeg Angle to rotate
	 * @return osStatus
	 */
	osStatus guide(axisrotdir_t dir, double time)
	{
		if (dir == AXIS_ROTATE_NEGATIVE)
			time = -time;
		int32_t time_ms = (int) (time * 1000);
		// Put the guide pulse into the queue
		osStatus s;
		if ((s = guide_queue.put((void*) (time_ms))) != osOK)
		{
			return s;
		}
		task_thread.signal_set(AXIS_GUIDE_SIGNAL); // Signal the task thread to read the queue
		return osOK;
	}

	/**
	 * Stop slewing or tracking. Calling this function will stop the axis from indefinite slewing, or tracking
	 * @note In the case of indefinite slewing, the axis will perform a deceleration and then stop
	 */
	void stop()
	{
		task_thread.signal_set(AXIS_STOP_SIGNAL);
	}

	/**
	 * Perform an emergency stop. This should stop in ALL situations IMMEDIATELY without performing deceleration.
	 */
	void emergency_stop()
	{
		task_thread.signal_set(AXIS_EMERGE_STOP_SIGNAL);
	}

	/** @param new angle
	 * @note Must be called only when the axis is stopped
	 */
	void setAngleDeg(double angle)
	{
		stepper->setStepCount(angle * stepsPerDeg);
	}

	/** @return new angle
	 * @note Can be called anywhere
	 */
	double getAngleDeg()
	{
		return remainder(stepper->getStepCount() / stepsPerDeg, 360);
	}

	axisstatus_t getStatus()
	{
		return status;
	}

	double getAcceleration() const
	{
		return acceleration;
	}

	/** @param acceleration in deg/s^2
	 * @note Must be called only when the axis is stopped
	 */
	void setAcceleration(double acceleration)
	{
		if (acceleration > 0)
			this->acceleration = acceleration;
	}

	double getSlewSpeed() const
	{
		return slewSpeed;
	}

	/** @param new slew speed in deg/s
	 * @note Must be called only when the axis is stopped
	 */
	void setSlewSpeed(double slewSpeed)
	{
		if (slewSpeed > 0)
			this->slewSpeed = slewSpeed;
	}

	double getTrackSpeedSidereal() const
	{
		return trackSpeed / sidereal_speed;
	}

	/** @param new track speed in sidereal rate
	 * @note Must be called only when the axis is stopped
	 */
	void setTrackSpeedSidereal(double trackSpeed)
	{
		if (trackSpeed > 0)
		{
			this->trackSpeed = trackSpeed * sidereal_speed;
		}
	}

	double getGuideSpeedSidereal() const
	{
		return guideSpeed / sidereal_speed;
	}

	/** @param new guide speed in sidereal rate. Must be >=0.01 * trackSpeed and <=0.09 * trackSpeed
	 * @note Must be called only when the axis is stopped
	 */
	void setGuideSpeedSidereal(double guideSpeed)
	{
		guideSpeed *= sidereal_speed;
		if (guideSpeed <= 0.01 * trackSpeed)
			this->guideSpeed = 0.01 * trackSpeed;
		else if (guideSpeed >= 0.99 * trackSpeed)
			this->guideSpeed = 0.99 * trackSpeed;
		else
			this->guideSpeed = guideSpeed;
	}

	double getCorrectionSpeedSidereal() const
	{
		return correctionSpeed / sidereal_speed;
	}

	/** @param new correction speed in sidereal rate
	 * @note Must be called only when the axis is stopped
	 */
	void setCorrectionSpeedSidereal(double correctionSpeed)
	{
		if (correctionSpeed > 0)
			this->correctionSpeed = correctionSpeed * sidereal_speed;
	}

	double getCurrentSpeed() const
	{
		return currentSpeed;
	}

protected:

	typedef struct
	{
		enum sig_t
		{
			SIGNAL_SLEW_TO = 0, SIGNAL_SLEW_INDEFINITE, SIGNAL_TRACK
		} signal;
		double value;
		axisrotdir_t dir;
	} msg_t;

	/*Configurations*/
	double stepsPerDeg; ///steps per degree
	StepperMotor *stepper; ///Pointer to stepper motor
	bool invert;
	const char *axisName;

	/*Runtime values*/
	volatile double currentSpeed; /// Current speed in deg/s
	volatile axisrotdir_t currentDirection; // Current direction
	double slewSpeed; /// Slewing speed in deg/s
	double trackSpeed; /// Tracking speed in deg/s (no accel/deceleration)
	double correctionSpeed; /// Correction speed in deg/s (no accel/deceleration)
	double guideSpeed; /// Guide speed in deg/s. this amount will be subtracted/added to the trackSpeed, and so must be less than track speed
	double acceleration; /// Acceleration in deg/s^2
	volatile axisstatus_t status;
	Thread task_thread; ///Thread for executing all lower-level tasks
	Queue<msg_t, 16> task_queue; ///Queue of messages
	Queue<void, 16> guide_queue; ///Guide pulse queue
	MemoryPool<msg_t, 16> task_pool; ///MemoryPool for allocating messages
	Semaphore slew_finish_sem;

	static void task(Axis *p);

	/*Low-level functions for internal use*/
	void slew(axisrotdir_t dir, double dest, bool indefinite);
	void track(axisrotdir_t dir);

	/*These functions can be overriden to provide mode selection before each type of operation is performed, such as microstepping and current setting*/
	virtual void slew_mode()
	{
	}
	virtual void track_mode()
	{
	}
	virtual void correction_mode()
	{
	}
	virtual void idle_mode()
	{
	}
}
;

#endif /* PUSHTOGO_AXIS_H_ */
