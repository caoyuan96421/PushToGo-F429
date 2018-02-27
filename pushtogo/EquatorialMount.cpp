#include <math.h>
#include "EquatorialMount.h"

#define EM_DEBUG 1

EquatorialMount::EquatorialMount(Axis& ra, Axis& dec, UTCClock& clk,
		LocationCoordinates loc) :
		ra(ra), dec(dec), clock(clk), location(loc), curr_pos(0, 0), pier_side(
				PIER_SIDE_EAST), offset(0, 0), pa(loc.lat, 0), cone_value(0)
{
	south = loc.lat < 0.0;
	// Get initial transformation
	CelestialMath::getMisalignedPolarAxisTransformation(pa_trans, pa, location);
	// Set RA and DEC positions to zero
	ra.setAngleDeg(0);
	dec.setAngleDeg(0);
	update_position();
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
	// Convert to Mount coordinates. Automatically determine the pier side
	MountCoordinates mount_coord = CelestialMath::localEquatorialToMount(
			dest_local, PIER_SIDE_AUTO);
	debug_if(EM_DEBUG, "mountdest ra=%.2f, dec=%.2f\n", mount_coord.ra_delta,
			mount_coord.dec_delta);

	update_position();
	debug_if(EM_DEBUG, "mountcurr: ra=%.2f, dec=%.2f\n", curr_pos.ra_delta,
			curr_pos.dec_delta);
	debug_if(EM_DEBUG, "eqcurr: ra=%.2f, dec=%.2f\n", curr_pos_eq.ra,
			curr_pos_eq.dec);

	axisrotdir_t dec_dir, ra_dir;
	ra_dir =
			(remainder(mount_coord.ra_delta, 360.0)
					> remainder(curr_pos.ra_delta, 360.0)) ?
					AXIS_ROTATE_POSITIVE : AXIS_ROTATE_NEGATIVE;
	dec_dir =
			(remainder(mount_coord.dec_delta, 360.0)
					> remainder(curr_pos.dec_delta, 360.0)) ?
					AXIS_ROTATE_POSITIVE : AXIS_ROTATE_NEGATIVE;

	debug_if(EM_DEBUG, "EM: start slewing\n");
	ra.startSlewTo(ra_dir, mount_coord.ra_delta);
	dec.startSlewTo(dec_dir, mount_coord.dec_delta);

	ra.waitForSlew();
	dec.waitForSlew();
	debug_if(EM_DEBUG, "EM: slewing finished\n");

	// Update current position
	update_position();
	debug_if(EM_DEBUG, "mount: ra=%.2f, dec=%.2f\n", curr_pos.ra_delta,
			curr_pos.dec_delta);
	debug_if(EM_DEBUG, "eq: ra=%.2f, dec=%.2f\n", curr_pos_eq.ra,
			curr_pos_eq.dec);

	return osOK;
}

void EquatorialMount::update_position()
{
	curr_pos = MountCoordinates(dec.getAngleDeg(), ra.getAngleDeg());

	// Convert back from MountCoordinates to equatorial coordinates
	LocalEquatorialCoordinates pos_local =
			CelestialMath::mountToLocalEquatorial(curr_pos);
	pos_local = CelestialMath::deapplyConeError(pos_local, cone_value);
	pos_local = CelestialMath::deapplyMisalignment(pa_trans, pos_local);
	curr_pos_eq = CelestialMath::localEquatorialToEquatorial(pos_local,
			clock.getTime(), location);

}

void EquatorialMount::emergencyStop()
{
	ra.emergency_stop();
	dec.emergency_stop();
}

void EquatorialMount::stop()
{
	ra.stop();
	dec.stop();
}
