#include <math.h>
#include "EquatorialMount.h"

#define EM_DEBUG 1

EquatorialMount::EquatorialMount(Axis& ra, Axis& dec, UTCClock& clk,
		LocationCoordinates loc) :
		ra(ra), dec(dec), clock(clk), location(loc), curr_pos(0, 0), curr_nudge_dir(
				NUDGE_NONE), pier_side(PIER_SIDE_EAST), offset(0, 0), pa(
				loc.lat, 0), cone_value(0)
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
	ra.startSlewTo(ra_dir, dest_mount.ra_delta);
	dec.startSlewTo(dec_dir, dest_mount.dec_delta);

	ra.waitForSlew();
	dec.waitForSlew();
	debug_if(EM_DEBUG, "EM: slewing finished\n");

	updatePosition(); // Update current position

	if (EM_DEBUG)
		printPosition();

	return osOK;
}

void EquatorialMount::updatePosition()
{
	// Lock the mutex to avoid race condition on the current position values
	mutex.lock();
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
	mutex.unlock();
}

void EquatorialMount::emergencyStop()
{
	ra.emergency_stop();
	dec.emergency_stop();
	curr_nudge_dir = NUDGE_NONE; // just to make sure
}

void EquatorialMount::stop()
{
	if (ra.getStatus() != AXIS_STOPPED)
		ra.stop();
	if (dec.getStatus() != AXIS_STOPPED)
		dec.stop();
	curr_nudge_dir = NUDGE_NONE; // just to make sure
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
	curr_nudge_dir = NUDGE_NONE; // just to make sure
}

osStatus EquatorialMount::nudgeOn(nudgedir_t newdir)
{
	osStatus s;
	updatePosition(); // Update current position, because we need to know the current pier side
	// see what has changed
	if ((curr_nudge_dir & (NUDGE_WEST | NUDGE_EAST))
			!= (newdir & (NUDGE_WEST | NUDGE_EAST)))
	{
		// If something on east/west has changed, we need to stop the RA axis (or maybe enable it later to switch direction)
		if (ra.getStatus() != AXIS_STOPPED)
			ra.stop();
		while (ra.getStatus() != AXIS_STOPPED)
		{
			Thread::yield();
		}
		if (newdir & NUDGE_EAST)
		{
			// Nudge east
			if ((s = ra.startSlewingIndefinite(AXIS_ROTATE_NEGATIVE)) != osOK)
				return s;
		}
		else if (newdir & NUDGE_WEST)
		{
			// Nudge west
			if ((s = ra.startSlewingIndefinite(AXIS_ROTATE_POSITIVE)) != osOK)
				return s;
		}
	}
	if ((curr_nudge_dir & (NUDGE_SOUTH | NUDGE_NORTH))
			!= (newdir & (NUDGE_SOUTH | NUDGE_NORTH)))
	{
		// If something on east/west has changed, we need to stop the RA axis (or maybe enable it later to switch direction)
		if (dec.getStatus() != AXIS_STOPPED)
			dec.stop();
		while (dec.getStatus() != AXIS_STOPPED)
		{
			Thread::yield();
		}
		if (newdir & NUDGE_NORTH)
		{
			axisrotdir_t dir =
					(curr_pos.side == PIER_SIDE_WEST) ?
							AXIS_ROTATE_NEGATIVE : AXIS_ROTATE_POSITIVE;
			// Nudge east
			if ((s = dec.startSlewingIndefinite(dir)) != osOK)
				return s;
		}
		else if (newdir & NUDGE_SOUTH)
		{
			axisrotdir_t dir =
					(curr_pos.side == PIER_SIDE_WEST) ?
							AXIS_ROTATE_POSITIVE : AXIS_ROTATE_NEGATIVE;
			// Nudge west
			if ((s = dec.startSlewingIndefinite(dir)) != osOK)
				return s;
		}
	}

	curr_nudge_dir = newdir;
	return osOK;
}

osStatus EquatorialMount::align(int n, const AlignmentStar as[])
{
	// TODO
	return osOK;
}
