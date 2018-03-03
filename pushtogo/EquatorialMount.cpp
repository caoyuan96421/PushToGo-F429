#include <math.h>
#include "EquatorialMount.h"

#define EM_DEBUG 1

EquatorialMount::EquatorialMount(Axis& ra, Axis& dec, UTCClock& clk,
		LocationCoordinates loc) :
		ra(ra), dec(dec), clock(clk), location(loc), curr_pos(0, 0), curr_nudge_dir(
				NUDGE_NONE), nudgeSpeed(0), pier_side(PIER_SIDE_EAST), offset(0,
				0), pa(loc.lat, 0), cone_value(0)
{
	south = loc.lat < 0.0;
	// Get initial transformation
	CelestialMath::getMisalignedPolarAxisTransformation(pa_trans, pa, location);
	// Set RA and DEC positions to zero
	ra.setAngleDeg(0);
	dec.setAngleDeg(0);
	updatePosition();
}

osStatus EquatorialMount::goTo(double ra_dest, double dec_dest)
{
	return goTo(EquatorialCoordinates(dec_dest, ra_dest));
}

osStatus EquatorialMount::goTo(EquatorialCoordinates dest)
{
	if (status != MOUNT_STOPPED)
	{
		debug("EM: goTo requested while mount is not stopped.\n");
		return osErrorParameter;
	}
	debug_if(EM_DEBUG, "EM: goTo\n");
	debug_if(EM_DEBUG, "dest ra=%.2f, dec=%.2f\n", dest.ra, dest.dec);
	LocalEquatorialCoordinates dest_local =
			CelestialMath::equatorialToLocalEquatorial(dest, clock.getTime(),
					location);
	// Apply PA misalignment
	dest_local = CelestialMath::applyMisalignment(pa_trans, dest_local);
	// Apply Cone error
	dest_local = CelestialMath::applyConeError(dest_local, cone_value);
	// Convert to Mount coordinates. Automatically determine the pier side, then apply offset
	MountCoordinates dest_mount = CelestialMath::localEquatorialToMount(
			dest_local, PIER_SIDE_AUTO) + offset;
	debug_if(EM_DEBUG, "dstmnt ra=%.2f, dec=%.2f\n", dest_mount.ra_delta,
			dest_mount.dec_delta);

	updatePosition(); // Get the latest position information

	if (EM_DEBUG)
		printPosition();

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
	ra.startSlewTo(ra_dir, dest_mount.ra_delta);
	dec.startSlewTo(dec_dir, dest_mount.dec_delta);

	ra.waitForSlew();
	dec.waitForSlew();
	status = MOUNT_STOPPED;
	debug_if(EM_DEBUG, "EM: slewing finished\n");

	updatePosition(); // Update current position

	if (EM_DEBUG)
		printPosition();

	return osOK;
}

osStatus EquatorialMount::startTracking()
{
	if (status != MOUNT_STOPPED)
	{
		debug("EM: tracking requested while mount is not stopped.\n");
		return osErrorParameter;
	}
	axisrotdir_t ra_dir = AXIS_ROTATE_POSITIVE; // Tracking is always going to positive hour angle direction, which is defined as positive.
	status = MOUNT_TRACKING;
	return ra.startTracking(ra_dir);
}

osStatus EquatorialMount::startNudge(nudgedir_t newdir)
{ // Update new status
	if (newdir == NUDGE_NONE) // Stop nudging
	{
		mountstatus_t oldstatus = status;
		stopWait(); // Stop the mount
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
	else
	{
		osStatus s;
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
			ra.stop();
		if (dec_changed)
			dec.stop();

		// Wait for stop together
		while ((ra_changed && ra.getStatus() != AXIS_STOPPED)
				|| (dec_changed && dec.getStatus() != AXIS_STOPPED))
		{
			Thread::yield();
		}

		// DEC axis is ok to start regardless of tracking state
		if (dec_changed && dec_dir != AXIS_ROTATE_STOP)
		{
			if ((s = dec.startSlewingIndefinite(dec_dir)) != osOK)
				return s;
		}

		// Now RA
		if (ra_changed)
		{
			if (status & MOUNT_TRACKING)
			{ // In tracking mode now
				if (ra_dir == AXIS_ROTATE_STOP)
				{ // resume tracking
					if ((s = ra.startTracking(AXIS_ROTATE_POSITIVE)) != osOK)
						return s; // Tracking is always in positive RA
				}
				else
				{
					// This is the complicated part
					double trackSpeed = ra.getTrackSpeedSidereal()
							* sidereal_speed;
					debug_if(EM_DEBUG, "EM: ra, ns=%f, ts=%f\n", nudgeSpeed,
							trackSpeed);
					if (ra_dir == AXIS_ROTATE_POSITIVE)
					{
						// Same direction as tracking
						ra.setSlewSpeed(nudgeSpeed + trackSpeed);
						if ((s = ra.startSlewingIndefinite(AXIS_ROTATE_POSITIVE))
								!= osOK)
							return s;
					}
					else if (nudgeSpeed < trackSpeed)
					{ // ra_dir == AXIS_ROTATE_NEGATIVE
					  // Partially canceling the tracking speed
						ra.setSlewSpeed(trackSpeed - nudgeSpeed);
						if ((s = ra.startSlewingIndefinite(AXIS_ROTATE_POSITIVE))
								!= osOK)
							return s;
					}
					else if (nudgeSpeed > trackSpeed)
					{						// ra_dir == AXIS_ROTATE_NEGATIVE
											// Direction inverted
						ra.setSlewSpeed(nudgeSpeed - trackSpeed);
						if ((s = ra.startSlewingIndefinite(AXIS_ROTATE_NEGATIVE))
								!= osOK)
							return s;
					}
					// else would be nudgeSpeed == trackSpeed, and we don't need to start the RA Axis
				}
			}
			else
			{
				// In non-tracking mode
				if (ra_dir != AXIS_ROTATE_STOP)
				{
					if ((s = ra.startSlewingIndefinite(ra_dir)) != osOK)
						return s;
				}
			}
		}

		// Update status
		if (status & MOUNT_TRACKING)
			status = MOUNT_NUDGING_TRACKING;
		else
			status = MOUNT_NUDGING;
	}

	return osOK;
}

osStatus EquatorialMount::stopNudge()
{
	return startNudge(NUDGE_NONE);
}

void EquatorialMount::updatePosition()
{
	// Lock the mutex to avoid race condition on the current position values
	mutex_update.lock();
	curr_pos = MountCoordinates(dec.getAngleDeg(), ra.getAngleDeg());

	// Determine the side of pier based on (offset) DEC axis pointing direction
	MountCoordinates curr_pos_off = curr_pos - offset; // mount position with index offset corrected (in principle)
	if (curr_pos_off.dec_delta > 0)
	{
		curr_pos.side = PIER_SIDE_WEST;
		curr_pos_off.side = PIER_SIDE_WEST;
	}
	else
	{
		curr_pos.side = PIER_SIDE_EAST;
		curr_pos_off.side = PIER_SIDE_EAST;
	}

	// Convert back from MountCoordinates to equatorial coordinates
	LocalEquatorialCoordinates pos_local =
			CelestialMath::mountToLocalEquatorial(curr_pos_off);
	pos_local = CelestialMath::deapplyConeError(pos_local, cone_value);
	pos_local = CelestialMath::deapplyMisalignment(pa_trans, pos_local);
	curr_pos_eq = CelestialMath::localEquatorialToEquatorial(pos_local,
			clock.getTime(), location);
	mutex_update.unlock();
}

void EquatorialMount::emergencyStop()
{
	ra.emergency_stop();
	dec.emergency_stop();
	status = MOUNT_STOPPED;
	curr_nudge_dir = NUDGE_NONE;
}

void EquatorialMount::stop()
{
	if (ra.getStatus() != AXIS_STOPPED)
		ra.stop();
	if (dec.getStatus() != AXIS_STOPPED)
		dec.stop();
	status = MOUNT_STOPPED;
}

void EquatorialMount::stopWait()
{
	if (ra.getStatus() != AXIS_STOPPED)
		ra.stop();
	if (dec.getStatus() != AXIS_STOPPED)
		dec.stop();
	// Wait until they're fully stopped
	while (ra.getStatus() != AXIS_STOPPED || dec.getStatus() != AXIS_STOPPED)
	{
		Thread::yield();
	}
	status = MOUNT_STOPPED;
}

osStatus EquatorialMount::align(int n, const AlignmentStar as[])
{
	// TODO
	return osOK;
}
