
#include "RotationAxis.h"
#include "Mount.h"
#include "UTCClock.h"
#include "LocationProvider.h"
#include "CelestialMath.h"

/**
* Object that represents an equatorial mount with two perpendicular axis called RA and Dec.
*/
class EquatorialMount : public Mount
{

protected:
    RotationAxis *ra;   /// RA Axis
    RotationAxis *dec;  /// DEC Axis
    
    LocationCoordinates location;   /// Current location (GPS coordinates)

    pier_side_t pier_side;      /// Side of pier. 1: East
    AzimuthalCoordinates pa;    /// Alt-azi coordinate of the actual polar axis
    LocalEquatorialCoordinates offset; /// Offset in DEC and RA(HA) axis
    double cone_value;   /// Non-orthogonality between the two axis, or cone value. This should be a value < 90 deg

public:

    typedef enum {
        PIER_SIDE_EAST,
        PIER_SIDE_WEST
    } pier_side_t;

    class AlignmentStar
    {
    public:
        double ra;
        double dec;
        double ra_actual;
        double dec_actual;
        time_t utc_timestamp;

        AlignmentStar() {}
        ~AlignmentStar() {}
    };
    /**
    * Create an EquatorialMount object which controls two axis
    * @param ra RA Axis
    * @param dec DEC Axis
    * @note cone_value, ma_alt, ma_azi,off_ra, off_dec will be init to zero, i.e. assuming a perfectly aligned mount pointing at RA=DEC=0
    * @note This class assumes that the rotating direction of both axis are correct.
    *       This should be done using the invert option when initializing the RotationAxis objects
    * @sa RotationAxis
    */
    EquatorialMount(RotationAxis *ra=0, RotationAxis *dec=0, UTCClock *clk=0, LocationProvider *loc=0);
    virtual ~EquatorialMount() {
    }

    /**
    *   Perform a Go-To to specified equatorial coordinates in the sky
    *   @param  ra_dest RA coordinate in degree.
    *   @return true Error \n false No Error
    */
    bool goTo(double ra_dest, double dec_dest);


    void alignTwoStar(AlignmentStar *stars);

};