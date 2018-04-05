#ifndef EQUATORIALMOUNT_H_
#define EQUATORIALMOUNT_H_

#include "Axis.h"
#include "Mount.h"
#include "UTCClock.h"
#include "LocationProvider.h"
#include "CelestialMath.h"

/**
 * EquatorialMount status
 */

/**
 * Direction of nudge
 */
typedef enum
{
	NUDGE_NONE = 0,
	NUDGE_EAST = 1,
	NUDGE_WEST = 2,
	NUDGE_NORTH = 4,
	NUDGE_SOUTH = 8,

	NUDGE_NORTHWEST = NUDGE_NORTH | NUDGE_WEST,
	NUDGE_SOUTHWEST = NUDGE_SOUTH | NUDGE_WEST,
	NUDGE_NORTHEAST = NUDGE_NORTH | NUDGE_EAST,
	NUDGE_SOUTHEAST = NUDGE_SOUTH | NUDGE_EAST,
} nudgedir_t;

/**
 * Alignment star object
 */
struct AlignmentStar
{
	EquatorialCoordinates star_ref; /// Reference position of the star in the sky (in current epoch)
	MountCoordinates star_meas;	/// Measured position of the star in mount coordinates
	time_t timestamp;				/// UTC timestamp of the measurement
	AlignmentStar(const EquatorialCoordinates & ref, MountCoordinates meas,
			time_t t) :
			star_ref(ref), star_meas(meas), timestamp(t)
	{
	}
};

struct EqCalibration
{
	LocationCoordinates location;
	IndexOffset offset;
	AzimuthalCoordinates pa;
	double cone;
	EqCalibration(const LocationCoordinates &loc, const IndexOffset &off,
			const AzimuthalCoordinates p, double c) :
			location(loc), offset(off), pa(p), cone(c)
	{
	}
};

/**
 * Object that represents an equatorial mount with two perpendicular axis called RA and Dec.
 */
class EquatorialMount: public Mount
{

protected:
	Axis &ra;   /// RA Axis
	Axis &dec;  /// DEC Axis

	UTCClock &clock; /// Clock

	Mutex mutex_update; /// Mutex to lock position updating
	Mutex mutex_motion; /// Mutex to lock motion related functions

	LocationCoordinates location;   /// Current location (GPS coordinates)
	bool south;	/// If we are in south semisphere
	MountCoordinates curr_pos; /// Current Position in mount coordinates (offset from the index positions)
	EquatorialCoordinates curr_pos_eq; /// Current Position in the equatorial coordinates (absolute pointing direction in the sky)
	nudgedir_t curr_nudge_dir;
	double nudgeSpeed;

	pierside_t pier_side;      /// Side of pier. 1: East
	IndexOffset offset; /// Offset in DEC and RA(HA) axis index position
	AzimuthalCoordinates pa;    /// Alt-azi coordinate of the actual polar axis
	Transformation pa_trans;
	double cone_value; /// Non-orthogonality between the two axis, or cone value.

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
	osStatus goToMount(MountCoordinates mc);
	osStatus goToIndex()
	{
		return goToMount(MountCoordinates(0, 0));
	}

	osStatus startNudge(nudgedir_t);
	osStatus stopNudge();

	osStatus startTracking();

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

	/** BLOCKING. Cannot be called in ISR.
	 * Call stop of the Axis objects and wait until they are stopped.
	 * @note This function can be called from any context (including ISR) to perform a soft stop of the mount
	 */
	void stopWait();

	/**
	 * Get current equatorial coordinates
	 * @return current equatorial coordinates
	 */
	EquatorialCoordinates getEquatorialCoordinates()
	{
		updatePosition();
		return curr_pos_eq;
	}

	/**
	 * Get current mount coordinates
	 * @return current mount coordinates
	 */
	MountCoordinates getMountCoordinates()
	{
		updatePosition();
		return curr_pos;
	}

	/**
	 * Make an alignment star object using the provided reference star, current mount position, and current time
	 * @param star_ref Reference star position
	 * @return AlignmentStar object representing the alignment star
	 */
	AlignmentStar makeAlignmentStar(const EquatorialCoordinates star_ref)
	{
		updatePosition();
		return AlignmentStar(star_ref, curr_pos, clock.getTime());
	}

	/**
	 * Align the current mount using an array of alignment stars. Support up to 10 stars.
	 * @note If n=1, will only correct for Index offset
	 * If n=2, will correct for index offset and polar misalignment
	 * If n>=3, will correct for index offset, pa misalignment and cone error
	 * @param n # of alignment stars to use
	 * @param as Array of alignment stars
	 * @return osOK if successfully converged and updated the values
	 */
	osStatus align(int n, const AlignmentStar as[]);

	/**
	 * Set slew rate of both axis
	 * @param rate new speed
	 */
	void setSlewSpeed(double rate)
	{
		ra.setSlewSpeed(rate);
		dec.setSlewSpeed(rate);
	}

	/**
	 * Set slew rate of both axis
	 * @param rate new speed
	 */
	void setAcceleration(double acc)
	{
		ra.setAcceleration(acc);
		dec.setAcceleration(acc);
	}

	/**
	 * Print current position to STDOUT. Should call updatePosition to update the current position
	 */
	void printPosition(FILE *stream = stdout)
	{
		fprintf(stream, "Mount: RA=%7.2f, DEC=%7.2f %c\n", curr_pos.ra_delta,
				curr_pos.dec_delta,
				(curr_pos.side == PIER_SIDE_WEST) ? 'W' : 'E');
		fprintf(stream, "EQ:    RA=%7.2f, DEC=%7.2f\n", curr_pos_eq.ra,
				curr_pos_eq.dec);
	}

	void updatePosition();

	UTCClock& getClock() const
	{
		return clock;
	}

	const LocationCoordinates& getLocation() const
	{
		return location;
	}
};

#endif /*EQUATORIALMOUNT_H_*/
