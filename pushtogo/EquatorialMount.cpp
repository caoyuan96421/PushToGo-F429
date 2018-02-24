#include <math.h>
#include "EquatorialMount.h"

static const double sidereal_day = 86164.09;

void xprintf(const char *, ...);

EquatorialMount::EquatorialMount(RotationAxis& ra, RotationAxis& dec,
		UTCClock& clk, LocationCoordinates loc) :
		ra(ra), dec(dec), clock(clk), location(loc), curr_pos(0, 0), pier_side(
				PIER_SIDE_EAST), offset(0, 0), pa(loc.lat, 0), cone_value(0)
{
	// Get initial transformation
	CelestialMath::getMisalignedPolarAxisTransformation(pa_trans, pa, location);
	// Set RA and DEC positions to zero
	ra.setAngleDeg(0);
	dec.setAngleDeg(0);
}

bool EquatorialMount::goTo(double ra_dest, double dec_dest)
{
	return goTo(EquatorialCoordinates(dec_dest, ra_dest));
}

bool EquatorialMount::goTo(EquatorialCoordinates dest)
{
	LocalEquatorialCoordinates dest_local =
			CelestialMath::equatorialToLocalEquatorial(dest, clock.getTime(),
					location);
	// Apply PA misalignment
	LocalEquatorialCoordinates dest_local_misaligned =
			CelestialMath::applyMisalignment(pa_trans, dest_local);
	// Apply Cone error
	LocalEquatorialCoordinates dest_local_misaligned_cone =
			CelestialMath::applyConeError(dest_local_misaligned, cone_value);
	// Convert to Mount coordinates. Automatically determine the pier side
	MountCoordinates mount_coord = CelestialMath::localEquatorialToMount(
			dest_local_misaligned_cone, PIER_SIDE_AUTO);

	axisrotdir_t dec_dir, ra_dir;
	ra_dir =
			(remainder(mount_coord.ra_delta, 360.0)
					> remainder(ra.getAngleDeg(), 360.0)) ?
					AXIS_ROTATE_POSITIVE : AXIS_ROTATE_NEGATIVE;
	dec_dir =
			(remainder(mount_coord.dec_delta, 360.0)
					> remainder(dec.getAngleDeg(), 360.0)) ?
					AXIS_ROTATE_POSITIVE : AXIS_ROTATE_NEGATIVE;

	ra.slewTo(ra_dir, mount_coord.ra_delta);
	dec.slewTo(dec_dir, mount_coord.dec_delta);
}
