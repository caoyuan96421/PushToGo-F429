#ifndef EQUATORIALMOUNT_H_
#define EQUATORIALMOUNT_H_

#include "Axis.h"
#include "Mount.h"
#include "UTCClock.h"
#include "LocationProvider.h"
#include "CelestialMath.h"

typedef enum
{
	NUDGE_EAST, NUDGE_WEST, NUDGE_NORTH, NUDGE_SOUTH,
} nudgedir_t;

/**
 * Object that represents an equatorial mount with two perpendicular axis called RA and Dec.
 */
class EquatorialMount: public Mount
{

protected:
	Axis &ra;   /// RA Axis
	Axis &dec;  /// DEC Axis

	UTCClock &clock; /// Clock

	LocationCoordinates location;   /// Current location (GPS coordinates)
	bool south;	/// If we are in south semisphere
	MountCoordinates curr_pos; /// Current Position in mount coordinates (offset from the index positions)
	EquatorialCoordinates curr_pos_eq; /// Current Position in the equatorial coordinates (absolute pointing direction in the sky)

	pierside_t pier_side;      /// Side of pier. 1: East
	IndexOffset offset; /// Offset in DEC and RA(HA) axis index position
	AzimuthalCoordinates pa;    /// Alt-azi coordinate of the actual polar axis
	Transformation pa_trans;
	double cone_value; /// Non-orthogonality between the two axis, or cone value.

	void update_position();

public:

	/**
	 * Create an EquatorialMount object which controls two axis
	 * @param ra RA Axis
	 * @param dec DEC Axis
	 * @note cone_value, ma_alt, ma_azi,off_ra, off_dec will be init to zero, i.e. assuming a perfectly aligned mount pointing at RA=DEC=0
	 * @note This class assumes that the rotating direction of both axis are correct.
	 *       This should be done using the invert option when initializing the RotationAxis objects
	 * @sa RotationAxis
	 */
	EquatorialMount(Axis &ra, Axis &dec, UTCClock &clk,
			LocationCoordinates loc);
	virtual ~EquatorialMount()
	{
	}

	/**
	 *   Perform a Go-To to specified equatorial coordinates in the sky
	 *   @param  ra_dest RA coordinate in degree.
	 *   @return osOK if no error
	 */
	osStatus goTo(double ra_dest, double dec_dest);
	osStatus goTo(EquatorialCoordinates dest);

	osStatus nudgeOn(nudgedir_t);

	/**
	 * Call emergency stop of the Axis objects
	 * @note This function can be called from any context (including ISR) to perform a hard stop of the mount
	 */
	void emergencyStop();

	/**
	 * Call stop of the Axis objects
	 * @note This function can be called from any context (including ISR) to perform a soft stop of the mount
	 */
	void stop();

	/**
	 * Get current equatorial coordinates
	 */
	EquatorialCoordinates getEquatorialCoordinates()
	{
		update_position();
		return curr_pos_eq;
	}

	/**
	 * Set slew rate of both axis
	 */
	void setSlewSpeed(double rate)
	{
		ra.setSlewSpeed(rate);
		dec.setSlewSpeed(rate);
	}
};

#endif /*EQUATORIALMOUNT_H_*/
