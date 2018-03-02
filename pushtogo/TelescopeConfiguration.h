/*
 * TelescopeConfiguration.h
 *
 *  Created on: 2018Äê3ÔÂ1ÈÕ
 *      Author: caoyuan9642
 */

#ifndef PUSHTOGO_TELESCOPECONFIGURATION_H_
#define PUSHTOGO_TELESCOPECONFIGURATION_H_

#include <stdio.h>
#include "CelestialMath.h"

class TelescopeConfiguration
{
private:
	// Basic parameters
	double motor_steps; /// Steps per rotation of the stepper motor
	double gear_reduction; /// Gear reduction ratio from motor to worm
	double worm_teeth; /// Number of worm teeth
	bool ra_invert; /// Reverse RA axis?
	bool dec_invert; /// Reverse DEC axis?
	LocationCoordinates location;

	// Axis behavior, for goto fine tuning
	double default_slew_speed;
	double default_track_speed_sidereal;
	double default_correction_speed_sidereal;
	double default_acceleration;
	double max_speed;
	int acceleration_step_time_ms;
	double min_slew_angle;
	double correction_tolerance;
	int min_correction_time;
	double max_correction_angle;
	double max_guide_time;

	// AdaptiveAxis behavior, microstep and current control
	int microstep_slew;
	double current_slew;
	int microstep_track;
	double current_track;
	int microstep_correction;
	double current_correction;
	double current_idle;

public:
	TelescopeConfiguration()
	{
		// Default values
		motor_steps = 400;
		gear_reduction = 4;
		worm_teeth = 180;
		ra_invert = false;
		dec_invert = false;
		location = LocationCoordinates(42, -73);

		default_slew_speed = 2;
		default_track_speed_sidereal = 1;
		default_correction_speed_sidereal = 32;
		default_acceleration = 2;

		max_speed = 4;
		acceleration_step_time_ms = 5;
		min_slew_angle = 0.3;
		correction_tolerance = 0.05;
		min_correction_time = 5;
		max_correction_angle = 5.0;
		max_guide_time = 5000;

		microstep_slew = 32;
		current_slew = 0.5;
		microstep_track = 128;
		current_track = 0.5;
		microstep_correction = 128;
		current_correction = 0.5;
		current_idle = 0.1;

	}
	virtual ~TelescopeConfiguration()
	{
	}

	/**
	 * Get default telescope configuration
	 */
	static TelescopeConfiguration &getDefaultConfiguration()
	{
		static TelescopeConfiguration default_config = TelescopeConfiguration();
		return default_config;
	}

	static TelescopeConfiguration &readFromFile(FILE *fp);

	bool isDECInvert() const
	{
		return dec_invert;
	}

	void setDECInvert(bool decInvert)
	{
		dec_invert = decInvert;
	}

	double getGearReduction() const
	{
		return gear_reduction;
	}

	void setGearReduction(double gearReduction)
	{
		gear_reduction = gearReduction;
	}

	double getMotorSteps() const
	{
		return motor_steps;
	}

	void setMotorSteps(double motorSteps)
	{
		motor_steps = motorSteps;
	}

	bool isRAInvert() const
	{
		return ra_invert;
	}

	void setRAInvert(bool raInvert)
	{
		ra_invert = raInvert;
	}

	double getWormTeeth() const
	{
		return worm_teeth;
	}

	void setWormTeeth(double wormTeeth)
	{
		worm_teeth = wormTeeth;
	}

	double getStepsPerDeg()
	{
		return motor_steps * gear_reduction * worm_teeth / 360.0;
	}

	const LocationCoordinates& getLocation() const
	{
		return location;
	}

	void setLocation(const LocationCoordinates& location)
	{
		this->location = location;
	}

	int getAccelerationStepTimeMs() const
	{
		return acceleration_step_time_ms;
	}

	void setAccelerationStepTimeMs(int accelerationStepTimeMs)
	{
		acceleration_step_time_ms = accelerationStepTimeMs;
	}

	double getCorrectionTolerance() const
	{
		return correction_tolerance;
	}

	void setCorrectionTolerance(double correctionTolerance)
	{
		correction_tolerance = correctionTolerance;
	}

	double getDefaultAcceleration() const
	{
		return default_acceleration;
	}

	void setDefaultAcceleration(double defaultAcceleration)
	{
		default_acceleration = defaultAcceleration;
	}

	double getDefaultCorrectionSpeedSidereal() const
	{
		return default_correction_speed_sidereal;
	}

	void setDefaultCorrectionSpeedSidereal(
			double defaultCorrectionSpeedSidereal)
	{
		default_correction_speed_sidereal = defaultCorrectionSpeedSidereal;
	}

	double getDefaultSlewSpeed() const
	{
		return default_slew_speed;
	}

	void setDefaultSlewSpeed(double defaultSlewSpeed)
	{
		default_slew_speed = defaultSlewSpeed;
	}

	double getDefaultTrackSpeedSidereal() const
	{
		return default_track_speed_sidereal;
	}

	void setDefaultTrackSpeedSidereal(double defaultTrackSpeedSidereal)
	{
		default_track_speed_sidereal = defaultTrackSpeedSidereal;
	}

	double getMaxCorrectionAngle() const
	{
		return max_correction_angle;
	}

	void setMaxCorrectionAngle(double maxCorrectionAngle)
	{
		max_correction_angle = maxCorrectionAngle;
	}

	double getMaxGuideTime() const
	{
		return max_guide_time;
	}

	void setMaxGuideTime(double maxGuideTime)
	{
		max_guide_time = maxGuideTime;
	}

	double getMaxSpeed() const
	{
		return max_speed;
	}

	void setMaxSpeed(double maxSpeed)
	{
		max_speed = maxSpeed;
	}

	int getMinCorrectionTime() const
	{
		return min_correction_time;
	}

	void setMinCorrectionTime(int minCorrectionTime)
	{
		min_correction_time = minCorrectionTime;
	}

	double getMinSlewAngle() const
	{
		return min_slew_angle;
	}

	void setMinSlewAngle(double minSlewAngle)
	{
		min_slew_angle = minSlewAngle;
	}

	double getCurrentCorrection() const
	{
		return current_correction;
	}

	void setCurrentCorrection(double currentCorrection)
	{
		current_correction = currentCorrection;
	}

	double getCurrentIdle() const
	{
		return current_idle;
	}

	void setCurrentIdle(double currentIdle)
	{
		current_idle = currentIdle;
	}

	double getCurrentSlew() const
	{
		return current_slew;
	}

	void setCurrentSlew(double currentSlew)
	{
		current_slew = currentSlew;
	}

	double getCurrentTrack() const
	{
		return current_track;
	}

	void setCurrentTrack(double currentTrack)
	{
		current_track = currentTrack;
	}

	int getMicrostepCorrection() const
	{
		return microstep_correction;
	}

	void setMicrostepCorrection(int microstepCorrection)
	{
		microstep_correction = microstepCorrection;
	}

	int getMicrostepSlew() const
	{
		return microstep_slew;
	}

	void setMicrostepSlew(int microstepSlew)
	{
		microstep_slew = microstepSlew;
	}

	int getMicrostepTrack() const
	{
		return microstep_track;
	}

	void setMicrostepTrack(int microstepTrack)
	{
		microstep_track = microstepTrack;
	}
};

#endif /* PUSHTOGO_TELESCOPECONFIGURATION_H_ */
