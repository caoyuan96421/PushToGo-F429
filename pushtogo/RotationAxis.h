/*
 * RotationAxis.h
 *
 *  Created on: 2018/2/7
 *      Author: caoyuan9642
 */

#ifndef TELESCOPE_ROTATIONAXIS_H_
#define TELESCOPE_ROTATIONAXIS_H_

#include "StepperMotor.h"
#include <math.h>
#include "mbed.h"

static const float sidereal_speed = 0.00417807462f; /* deg / s */

/** General Rotating Axis class
* Handles low-level stepper timing, calculates the speed and distance to rotate
* API provides comprehensive slewing, tracking and guiding.
*/
class RotationAxis
{
public:
    /**
    * status of the Axis object
    */
    typedef enum {
        AXIS_STOPPED = 0,
        AXIS_ACCELERATING,
        AXIS_DECELERATING,
        AXIS_SLEWING,
        AXIS_SLEWING_CONTINUOUS,
        AXIS_TRACKING,
        AXIS_CORRECTING
    } axisstatus_t;

    /** Define the rotation direction
     * AXIS_ROTATE_POSITIVE: +angle
     * AXIS_ROTATE_NEGATIVE: -angle
     */
    typedef enum {
        AXIS_ROTATE_POSITIVE = 0, AXIS_ROTATE_NEGATIVE = 1
    } axisrotdir_t;

    /**
    * Create a RotationAxis object
    * @param stepsPerDeg Steps per degree of the stepper motor
    * @param stepper Pointer to stepper driver to use
    * @param invert Whether the stepper direction should be inverted
    */
    RotationAxis(float stepsPerDeg, StepperMotor *stepper,
                 bool invert = false) :
        stepsPerDeg(stepsPerDeg), stepper(stepper), invert(invert), angleDeg(
            0), currentSpeed(0), currentDirection(AXIS_ROTATE_POSITIVE), slewSpeed(
                2), trackSpeed(sidereal_speed), correctionSpeed(
                    32.0f * sidereal_speed), guideSpeed(0.5f * sidereal_speed), acceleration(
                        1), status(AXIS_STOPPED), task_thread(osPriorityRealtime,
                                OS_STACK_SIZE, NULL, "Axis_task") {
        if (stepsPerDeg <= 0)
            error("Axis: steps per degree must be > 0");

        if (!stepper)
            error("Axis: stepper must be defined");

        /*Start the task-handling thread*/
        task_timer.start();
        task_thread.start(callback(this->task, this));
    }

    virtual ~RotationAxis() {
    }

    /**
     * Perform a goto to a specified angle (in Radian) in the specified direction with slewing rate
     * It will perform an acceleration, a GoTo, and a deceleration before returning
     * @note If a stop() is called during acceleration or slewing (from ISR or another thread), it will start deceleration immediately and stop
     * @param dir Rotation direction
     * @param angleDeg Angle to rotate
     * @return true Error
     * 		   false No Error
     */
    bool slewTo(axisrotdir_t dir, float angleDeg) {
        msg_t *message = task_pool.alloc();
        if (!message) {
            fprintf(stderr, "Axis: out of memory");
            return true;
        }
        message->signal = msg_t::SIGNAL_SLEW_TO;
        message->value = angleDeg;
        message->dir = dir;
        message->tid = Thread::gettid();
        if (task_queue.put(message) != osOK) {
            fprintf(stderr, "Axis: failed to queue the operation");
            task_pool.free(message);
            return true;
        }
        /*Wait for the specified action to finish*/
        Thread::signal_clr(0x7FFFFFFF);
        Thread::signal_wait(0);

        return false;
    }

    /** Perform a continuous slewing, until stop() is called
    * @param dir Direction to start continuous slewing
    * @return true Error
    * 		  false No Error
    * @sa{RotationAxis::stop}
    */
    bool startSlewingContinuous(axisrotdir_t dir) {
        msg_t *message = task_pool.alloc();
        if (!message) {
            fprintf(stderr, "Axis: out of memory");
            return true;
        }
        message->signal = msg_t::SIGNAL_SLEW_CONT;
        message->value = angleDeg;
        message->dir = dir;
        if (task_queue.put(message) != osOK) {
            fprintf(stderr, "Axis: failed to queue the operation");
            task_pool.free(message);
            return true;
        }

        return false;
    }

    /** Start tracking, until stop() is called
    * @param dir Direction to start continuous slewing
    * @return true Error
    * 		  false No Error
    * @sa{RotationAxis::stop}
    */
    bool startTracking(axisrotdir_t dir) {
        msg_t *message = task_pool.alloc();
        if (!message) {
            fprintf(stderr, "Axis: out of memory");
            return true;
        }
        message->signal = msg_t::SIGNAL_TRACK_CONT;
        message->dir = dir;
        if (task_queue.put(message) != osOK) {
            fprintf(stderr, "Axis: failed to queue the operation");
            task_pool.free(message);
            return true;
        }

        return false;
    }

    /**
     * Guide on the specified direction for specified time. Will block before the guide finishes
     * The axis must be in TRACKING mode
     * @param dir Direction to guide
     * @param time Time to guide, in second
     * @return true Error
     *		   false No Error
     */
    bool guide(axisrotdir_t dir, float time) {
        msg_t *message = task_pool.alloc();
        if (!message) {
            fprintf(stderr, "Axis: out of memory");
            return true;
        }
        message->signal = msg_t::SIGNAL_GUIDE;
        message->dir = dir;
        message->value = time;
        message->tid = Thread::gettid();
        if (task_queue.put(message) != osOK) {
            fprintf(stderr, "Axis: failed to queue the operation");
            task_pool.free(message);
            return true;
        }
        Thread::signal_clr(0x7FFFFFFF);
        Thread::signal_wait(0);

        return false;
    }

    /**
     * Stop slewing or tracking. Note that it takes effect only when the axis is
     * accelerating, slewing, or tracking. Stopping when guiding or deceleration will not
     * take effect before the action finishes.
     * Can be called from another thread or ISR to force stop the slewing (emergency stop)
     * @return true Error
     *		   false No Error
     */
    bool stop() {
        msg_t *message = task_pool.alloc();
        if (!message) {
//			fprintf(stderr, "Axis: out of memory");
            return true;
        }
        message->signal = msg_t::SIGNAL_STOP;
        if (task_queue.put(message, 0, 0xFF) != osOK) { /*Stop signal has a higher priority*/
//			fprintf(stderr, "Axis: failed to queue the operation");
            task_pool.free(message);
            return true;
        }
        return false;
    }

	/** @param new angle
	* @note Must be called only when the axis is stopped
	*/
    void setAngleDeg(float angle) {
        angleDeg = angle;
    }

	/** @return new angle
	* @note Can be called anywhere
	*/
    float getAngleDeg() {
        _update_angle_from_step_count();
        return angleDeg;
    }


    axisstatus_t getStatus() {
        return status;
    }

    float getAcceleration() const {
        return acceleration;
    }

	/** @param acceleration in deg/s^2
	* @note Must be called only when the axis is stopped
	*/
    void setAcceleration(float acceleration) {
        if (acceleration <= 0) {
            fprintf(stderr, "Axis: acceleration must be >0");
        } else {
            this->acceleration = acceleration;
        }
    }

    float getSlewSpeed() const {
        return slewSpeed;
    }

	/** @param new slew speed in deg/s
	* @note Must be called only when the axis is stopped
	*/
    void setSlewSpeed(float slewSpeed) {
        if (slewSpeed <= 0) {
            fprintf(stderr, "Axis: slew speed must be >0");
        } else {
            this->slewSpeed = slewSpeed;
        }
    }

    float getTrackSpeedSidereal() const {
        return trackSpeed / sidereal_speed;
    }

	/** @param new track speed in sidereal rate
	* @note Must be called only when the axis is stopped
	*/
    void setTrackSpeedSidereal(float trackSpeed) {
        if (trackSpeed <= 0) {
            fprintf(stderr, "Axis: track speed must be >0");
        } else {
            this->trackSpeed = trackSpeed * sidereal_speed;
        }
    }

    float getGuideSpeedSidereal() const {
        return guideSpeed / sidereal_speed;
    }

	/** @param new guide speed in sidereal rate. Must be >=0 and <=trackSpeed
	* @note Must be called only when the axis is stopped
	*/
    void setGuideSpeedSidereal(float guideSpeed) {
        if (guideSpeed <= 0 || guideSpeed >= trackSpeed) {
            fprintf(stderr, "Axis: guide speed must be > 0 and < track speed");
        } else {
            this->guideSpeed = guideSpeed * sidereal_speed;
        }
    }

    float getCorrectionSpeedSidereal() const {
        return correctionSpeed / sidereal_speed;
    }

	/** @param new correction speed in sidereal rate
	* @note Must be called only when the axis is stopped
	*/
    void setCorrectionSpeedSidereal(float correctionSpeed) {
        if (correctionSpeed <= 0) {
            fprintf(stderr, "Axis: correction speed must be >0");
        } else {
            this->correctionSpeed = correctionSpeed * sidereal_speed;
        }
    }

    float getCurrentSpeed() const {
        return currentSpeed;
    }

protected:

    typedef struct {
        enum sig_t {
            SIGNAL_SLEW_TO = 0,
            SIGNAL_SLEW_CONT,
            SIGNAL_TRACK_CONT,
            SIGNAL_GUIDE,
            SIGNAL_STOP
        } signal;
        float value;
        axisrotdir_t dir;
        osThreadId_t tid;
    } msg_t;

    /*Configurations*/
    float stepsPerDeg; /*speedFactor * speed(deg/s) = step per second (Hz)*/
    StepperMotor *stepper; /*Pointer to stepper motor*/
    bool invert;

    /*Runtime values*/
    volatile float angleDeg;
    volatile float currentSpeed; /*Current speed in deg/s*/
    volatile axisrotdir_t currentDirection;
    float slewSpeed; /* Slewing speed in deg/s */
    float trackSpeed; /* Tracking speed in deg/s (no accel/deceleration) */
    float correctionSpeed; /* Correction speed in deg/s (no accel/deceleration) */
    float guideSpeed; /* Guide speed in deg/s. this amount will be subtracted/added to the trackSpeed, and so must be less than track speed */
    float acceleration; /* Acceleration in deg/s^2*/
    volatile axisstatus_t status;
    Thread task_thread; /*Thread for executing all lower-level tasks*/
    Queue<msg_t, 16> task_queue; /*Queue of messages*/
    MemoryPool<msg_t, 16> task_pool; /*MemoryPool for allocating messages*/
    Timer task_timer;

    static void task(RotationAxis *p);

    /*Low-level functions for internal use*/
    void _slewto(axisrotdir_t dir, float dest);
    void _slewcont(axisrotdir_t dir);
    void _track(axisrotdir_t dir);
    void _guide(axisrotdir_t dir, float duration);
    void _update_angle_from_step_count();
    void _decel_stop();
    void _stop();
};

#endif /* TELESCOPE_ROTATIONAXIS_H_ */

