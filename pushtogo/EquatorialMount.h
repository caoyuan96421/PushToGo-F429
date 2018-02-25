#include "Axis.h"
#include "Mount.h"
#include "UTCClock.h"
#include "LocationProvider.h"
#include "CelestialMath.h"

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
	MountCoordinates curr_pos; /// Current Position

	pierside_t pier_side;      /// Side of pier. 1: East
	AzimuthalCoordinates pa;    /// Alt-azi coordinate of the actual polar axis
	Transformation pa_trans;
	IndexOffset offset; /// Offset in DEC and RA(HA) axis index position
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
	 *   @return true Error \n false No Error
	 */
	osStatus goTo(double ra_dest, double dec_dest);
	osStatus goTo(EquatorialCoordinates dest);


	void emergencyStop();

};
