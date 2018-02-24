
/**
* Provides location information. Can be overriden if a GPS is installed for example.
*/
class LocationProvider
{
protected:
    double longtitude;
    double latitude;
public:
    LocationProvider() : longtitude(0), latitude(0) {
    }
    LocationProvider(double x, double y): longtitude(x), latitude(y) {
    }
    ~LocationProvider() {
    }

    virtual double getLongtitude() {
        return longtitude;
    }

    virtual double getLatitude() {
        return latitude;
    }
};