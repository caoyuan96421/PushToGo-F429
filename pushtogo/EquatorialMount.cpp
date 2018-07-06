#include <math.h>
#include "EquatorialMount.h"

#define EM_DEBUG 1

EquatorialMount::EquatorialMount(Axis& ra, Axis& dec, UTCClock& clk,
		LocationCoordinates loc) :
		ra(ra), dec(dec), clock(clk), location(loc), curr_pos(0, 0), curr_nudge_dir(
				NUDGE_NONE), nudgeSpeed(0), pier_side(PIER_SIDE_EAST), num_alignment_stars(
				0)
{
	south = loc.lat < 0.0;
	// Get initial transformation
	calibration.pa = AzimuthalCoordinates(loc.lat, 0);
	// Set RA and DEC positions to zero
	ra.setAngleDeg(0);
	dec.setAngleDeg(0);
	dec.setTrackSpeedSidereal(0); // Make sure dec doesn't move during tracking
	updatePosition();

	// Set in tracking mode
	startTracking();
}

osStatus EquatorialMount::goTo(double ra_dest, double dec_dest)
{
	return goTo(EquatorialCoordinates(dec_dest, ra_dest));
}

osStatus EquatorialMount::goTo(EquatorialCoordinates dest)
{

	debug_if(EM_DEBUG, "dest ra=%.2f, dec=%.2f\n", dest.ra, dest.dec);

	updatePosition(); // Get the latest position information

	if (EM_DEBUG)
		printPosition();

	for (int i = 0; i < 2; i++)
	{
		// Convert to Mount coordinates. Automatically determine the pier side, then apply offset
		MountCoordinates dest_mount = convertToMountCoordinates(dest);

		osStatus s = goToMount(dest_mount, (i > 0)); // Use correction only for the second time
		if (s != osOK)
			return s;
	}

	if (EM_DEBUG)
		printPosition();

	return osOK;
}

osStatus EquatorialMount::goToMount(MountCoordinates dest_mount,
bool withCorrection)
{
	mutex_execution.lock();
	bool was_tracking = false;
	if (status == MOUNT_TRACKING)
	{
		was_tracking = true;
		stopSync();
	}
	else if (status != MOUNT_STOPPED)
	{
		debug("EM: goTo requested while mount is not stopped.\n");
		mutex_execution.unlock();
		return osErrorParameter;
	}
	debug_if(EM_DEBUG, "EM: goTo\n");
	debug_if(EM_DEBUG, "dstmnt ra=%.2f, dec=%.2f\n", dest_mount.ra_delta,
			dest_mount.dec_delta);

	axisrotdir_t dec_dir, ra_dir;
	ra_dir =
			(remainder(dest_mount.ra_delta, 360.0)
					> remainder(curr_pos.ra_delta, 360.0)) ?
					AXIS_ROTATE_POSITIVE : AXIS_ROTATE_NEGATIVE;
	dec_dir =
			(remainder(dest_mount.dec_delta, 360.0)
					> remainder(curr_pos.dec_delta, 360.0)) ?
					AXIS_ROTATE_POSITIVE : AXIS_ROTATE_NEGATIVE;

	debug_if(EM_DEBUG, "EM: start slewing\n");
	status = MOUNT_SLEWING;
	ra.startSlewTo(ra_dir, dest_mount.ra_delta, withCorrection);
	dec.startSlewTo(dec_dir, dest_mount.dec_delta, withCorrection);

	int ret = (int) ra.waitForSlew();
	ret |= (int) dec.waitForSlew();

	debug_if(EM_DEBUG, "EM: slewing finished\n");

	status = MOUNT_STOPPED;

	mutex_execution.unlock();

	if (was_tracking && !(ret & (FINISH_ERROR | FINISH_EMERG_STOPPED)))
	{
		startTracking();
	}

	updatePosition(); // Update current position

	if (ret)
	{
		// Stopped during slew
		return ret;
	}
	else
		return osOK;
}

osStatus EquatorialMount::startTracking()
{
	if (status != MOUNT_STOPPED)
	{
		debug("EM: tracking requested while mount is not stopped.\n");
		return osErrorParameter;
	}

	mutex_execution.lock();
	axisrotdir_t ra_dir = AXIS_ROTATE_POSITIVE; // Tracking is always going to positive hour angle direction, which is defined as positive.
	status = MOUNT_TRACKING;
	osStatus sr, sd;
	sr = ra.startTracking(ra_dir);
	sd = dec.startTracking(AXIS_ROTATE_STOP);
	mutex_execution.unlock();
	if (sr != osOK || sd != osOK)
		return osErrorResource;
	else
		return osOK;
}

osStatus EquatorialMount::startNudge(nudgedir_t newdir)
{ // Update new status
	if (status != MOUNT_STOPPED && status != MOUNT_TRACKING
			&& status != MOUNT_NUDGING && status != MOUNT_NUDGING_TRACKING)
	{
		return osErrorParameter;
	}
	osStatus s = osOK;
	mutex_execution.lock();
	if (newdir == NUDGE_NONE) // Stop nudging if being nudged
	{
		if (status & MOUNT_NUDGING)
		{
			mountstatus_t oldstatus = status;
			stopAsync(); // Stop the mount
			if (oldstatus == MOUNT_NUDGING)
			{
				// Stop the mount
			}
			else if (oldstatus == MOUNT_NUDGING_TRACKING)
			{
				// Get to tracking state
				ra.setSlewSpeed(nudgeSpeed); // restore the slew rate of RA
				startTracking();
			}
		}
	}
	else
	{ // newdir is not NUDGE_NONE
		updatePosition(); // Update current position, because we need to know the current pier side
		bool ra_changed = false, dec_changed = false;
		axisrotdir_t ra_dir, dec_dir;
		if ((status & MOUNT_NUDGING) == 0)
		{
			// Initial nudge
			curr_nudge_dir = NUDGE_NONE; //Make sure the current nudging direction is cleared
			nudgeSpeed = ra.getSlewSpeed(); // Get nudge speed and use it for ALL following nudge operations, until the nudge finishes
		}
		// see what has changed in RA
		if ((curr_nudge_dir & (NUDGE_WEST | NUDGE_EAST))
				!= (newdir & (NUDGE_WEST | NUDGE_EAST)))
		{
			// If something on east/west has changed, we need to stop the RA axis (or maybe enable it later to switch direction)
			ra_changed = true;
			if (newdir & NUDGE_EAST)
			{
				// Nudge east
				ra_dir = AXIS_ROTATE_NEGATIVE;
			}
			else if (newdir & NUDGE_WEST)
			{
				// Nudge west
				ra_dir = AXIS_ROTATE_POSITIVE;
			}
			else
			{
				ra_dir = AXIS_ROTATE_STOP;
			}
		}
		// see what has changed in DEC
		if ((curr_nudge_dir & (NUDGE_SOUTH | NUDGE_NORTH))
				!= (newdir & (NUDGE_SOUTH | NUDGE_NORTH)))
		{
			// If something on east/west has changed, we need to stop the RA axis (or maybe enable it later to switch direction)
			dec_changed = true;
			if (newdir & NUDGE_NORTH)
			{
				// Nudge north
				dec_dir =
						(curr_pos.side == PIER_SIDE_WEST) ?
								AXIS_ROTATE_NEGATIVE : AXIS_ROTATE_POSITIVE;
			}
			else if (newdir & NUDGE_SOUTH)
			{
				// Nudge south
				dec_dir =
						(curr_pos.side == PIER_SIDE_WEST) ?
								AXIS_ROTATE_POSITIVE : AXIS_ROTATE_NEGATIVE;
			}
			else
			{
				dec_dir = AXIS_ROTATE_STOP;
			}
		}
		curr_nudge_dir = newdir;

		// Request stop as necessary
		if (ra_changed)
		{
			ra.flushCommandQueue();
			if (ra.getSlewState() == AXIS_SLEW_DECELERATING
					&& ra_dir == ra.getCurrentDirection())
			{
				ra.stopKeepSpeed();
			}
			else
			{
				ra.stop();
			}
		}

		if (dec_changed)
		{
			dec.flushCommandQueue();
			if (dec.getSlewState() == AXIS_SLEW_DECELERATING
					&& dec_dir == dec.getCurrentDirection())
			{
				dec.stopKeepSpeed();
			}
			else
			{
				dec.stop();
			}
		}

		// Wait for stop together
//		while ((ra_changed && ra.getStatus() != AXIS_STOPPED
//				&& ra.getStatus() != AXIS_INERTIAL)
//				|| (dec_changed && dec.getStatus() != AXIS_STOPPED
//						&& dec.getStatus() != AXIS_INERTIAL))
//		{
//			Thread::yield();
//		}

		// DEC axis is ok to start regardless of tracking state
		if (dec_changed && dec_dir != AXIS_ROTATE_STOP)
		{
			s = dec.startSlewingIndefinite(dec_dir);
		}

		// Now RA
		if (ra_changed && s == osOK)
		{
			if (status & MOUNT_TRACKING)
			{ // In tracking mode now
				if (ra_dir == AXIS_ROTATE_STOP)
				{ // resume tracking
					s = ra.startTracking(AXIS_ROTATE_POSITIVE);
				}
				else
				{
					// This is the complicated part
					double trackSpeed = ra.getTrackSpeedSidereal()
							* sidereal_speed;
					debug_if(EM_DEBUG > 1, "EM: ra, ns=%f, ts=%f\n", nudgeSpeed,
							trackSpeed);
					if (ra_dir == AXIS_ROTATE_POSITIVE)
					{
						// Same direction as tracking
						ra.setSlewSpeed(nudgeSpeed + trackSpeed);
						s = ra.startSlewingIndefinite(AXIS_ROTATE_POSITIVE);
					}
					else if (nudgeSpeed < trackSpeed)
					{ // ra_dir == AXIS_ROTATE_NEGATIVE
					  // Partially canceling the tracking speed
						ra.setSlewSpeed(trackSpeed - nudgeSpeed);
						s = ra.startSlewingIndefinite(AXIS_ROTATE_POSITIVE);
					}
					else if (nudgeSpeed > trackSpeed)
					{					// ra_dir == AXIS_ROTATE_NEGATIVE
										// Direction inverted
						ra.setSlewSpeed(nudgeSpeed - trackSpeed);
						ra.startSlewingIndefinite(AXIS_ROTATE_NEGATIVE);
					}
					// else would be nudgeSpeed == trackSpeed, and we don't need to start the RA Axis
				}
			}
			else
			{
				// In non-tracking mode
				if (ra_dir != AXIS_ROTATE_STOP)
				{
					s = ra.startSlewingIndefinite(ra_dir);
				}
			}
		}

		// Update status
		if (status & MOUNT_TRACKING)
			status = MOUNT_NUDGING_TRACKING;
		else
			status = MOUNT_NUDGING;

		debug_if(EM_DEBUG, "status=%d, ra_dir=%d, dec_dir=%d\r\n", (int) status,
				(int) ra_dir, (int) dec_dir);
	}
	mutex_execution.unlock();

	return s;
}

osStatus EquatorialMount::stopNudge()
{
	return startNudge(NUDGE_NONE);
}

double EquatorialMount::getSlewSpeed()
{
	return ra.getSlewSpeed();
}

double EquatorialMount::getTrackSpeedSidereal()
{
	return ra.getTrackSpeedSidereal();
}

double EquatorialMount::getGuideSpeedSidereal()
{
	return ra.getGuideSpeedSidereal();
}

osStatus EquatorialMount::stopTracking()
{
	if ((status & MOUNT_TRACKING) == 0)
	{
		return osErrorParameter;
	}
	mutex_execution.lock();
	if (status == MOUNT_TRACKING)
	{
		stopSync();
	}
	else if (status == MOUNT_NUDGING_TRACKING)
	{
		status = MOUNT_NUDGING;
	}
	mutex_execution.unlock();
	return osOK;
}

void EquatorialMount::updatePosition()
{
	// Lock the mutex to avoid race condition on the current position values
	mutex_update.lock();
	curr_pos = MountCoordinates(dec.getAngleDeg(), ra.getAngleDeg());
	// Update location
	location.lat = TelescopeConfiguration::getDouble("latitude");
	location.lon = TelescopeConfiguration::getDouble("longitude");
	// Update Eq coordinates
	curr_pos_eq = this->convertToEqCoordinates(curr_pos);
	mutex_update.unlock();
}

void EquatorialMount::emergencyStop()
{
	ra.emergency_stop();
	dec.emergency_stop();
	status = MOUNT_STOPPED;
	curr_nudge_dir = NUDGE_NONE;
}

void EquatorialMount::stopAsync()
{
	ra.stop();
	dec.stop();
	status = MOUNT_STOPPED;
}

void EquatorialMount::stopSync()
{
	// Wait until they're fully stopped
	while (ra.getStatus() != AXIS_STOPPED || dec.getStatus() != AXIS_STOPPED)
	{
		ra.stop();
		dec.stop();
		Thread::yield();
	}
	status = MOUNT_STOPPED;
}

osStatus EquatorialMount::recalibrate()
{
	bool diverge = false;

	if (num_alignment_stars == 0)
	{
		return osOK;
	}
	EqCalibration newcalib = CelestialMath::align(num_alignment_stars,
			alignment_stars, location, diverge);
	if (diverge)
	{
		return osErrorParameter;
	}

	calibration = newcalib;

	return osOK;
}

osStatus EquatorialMount::guide(guidedir_t dir, int ms)
{
	// Check we are in tracking mode
	if (status != MOUNT_TRACKING)
	{
		return osErrorResource;
	}
	switch (dir)
	{
	case GUIDE_EAST:
		return ra.guide(AXIS_ROTATE_NEGATIVE, ms);
	case GUIDE_WEST:
		return ra.guide(AXIS_ROTATE_POSITIVE, ms);
	case GUIDE_NORTH:
		return dec.guide(
				(curr_pos.side == PIER_SIDE_WEST) ?
						AXIS_ROTATE_NEGATIVE : AXIS_ROTATE_POSITIVE, ms);
	case GUIDE_SOUTH:
		return dec.guide(
				(curr_pos.side == PIER_SIDE_WEST) ?
						AXIS_ROTATE_POSITIVE : AXIS_ROTATE_NEGATIVE, ms);
	default:
		return osErrorParameter;
	}
}

void EquatorialMount::setSlewSpeed(double rate)
{
//	mutex_execution.lock();
	ra.setSlewSpeed(rate);
	dec.setSlewSpeed(rate);
//	mutex_execution.unlock();
}

void EquatorialMount::setTrackSpeedSidereal(double rate)
{
//	mutex_execution.lock();
	ra.setTrackSpeedSidereal(rate);
//	mutex_execution.unlock();
}

void EquatorialMount::setGuideSpeedSidereal(double rate)
{
//	mutex_execution.lock();
	ra.setGuideSpeedSidereal(rate);
	dec.setGuideSpeedSidereal(rate);
//	mutex_execution.unlock();
}

