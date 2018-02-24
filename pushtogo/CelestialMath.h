/*
 * CelestialMath.h
 *
 *  Created on: Feb 21, 2018
 *      Author: Yuan
 */

#ifndef CELESTIALMATH_H_
#define CELESTIALMATH_H_

#include <time.h>

struct EquatorialCoordinates {
	double dec;		// Declination
	double ra;		// Right ascension
	EquatorialCoordinates(double d = 0, double r = 0) :
			dec(d), ra(r) {
	}
};

struct LocalEquatorialCoordinates {
	double dec;		// Declination
	double ha;		// Hour angle
	LocalEquatorialCoordinates(double d = 0, double h = 0) :
			dec(d), ha(h) {
	}
	LocalEquatorialCoordinates operator+(const LocalEquatorialCoordinates &b) const {
		return LocalEquatorialCoordinates(dec + b.dec, ha + b.ha);
	}
	LocalEquatorialCoordinates operator-(const LocalEquatorialCoordinates &b) const {
		return LocalEquatorialCoordinates(dec - b.dec, ha - b.ha);
	}
};

struct AzimuthalCoordinates {
	double alt;		// Altitude
	double azi;		// Azimuth
	AzimuthalCoordinates(double a1 = 0, double a2 = 0) :
			alt(a1), azi(a2) {
	}
};

struct LocationCoordinates {
	double lat;		// Latitude
	double lon;		// Longtitude
	LocationCoordinates(double l1 = 0, double l2 = 0) :
			lat(l1), lon(l2) {
	}
};

struct Transformation;

struct CartesianVector {
	double x, y, z;
	CartesianVector(double x = 0, double y = 0, double z = 0) :
			x(x), y(y), z(z) {
	}
	CartesianVector operator*(const Transformation &t);
};

struct Transformation {
	double a11, a12, a13;
	double a21, a22, a23;
	double a31, a32, a33;
	CartesianVector operator*(const CartesianVector &vec);
};

/**
 * Utility functions for doing math on coordinates of the celestial sphere
 */
class CelestialMath {
public:
	CelestialMath() {
	}
	~CelestialMath() {
	}

	/*Basic conversion between reference frames*/
	static AzimuthalCoordinates localEquatorialToAzimuthal(const LocalEquatorialCoordinates &a, const LocationCoordinates &loc);
	static LocalEquatorialCoordinates azimuthalToLocalEquatorial(const AzimuthalCoordinates &b, const LocationCoordinates &loc);
	static double getGreenwichMeanSiderealTime(time_t timestamp);
	static double getLocalSiderealTime(time_t timestamp, const LocationCoordinates &loc);
	static LocalEquatorialCoordinates equatorialToLocalEquatorial(const EquatorialCoordinates &e, time_t timestamp, const LocationCoordinates &loc);
	static EquatorialCoordinates localEquatorialToEquatorial(const LocalEquatorialCoordinates &a, time_t timestamp, const LocationCoordinates &loc);

	/*Misalignment correction functions*/
	static Transformation &getMisalignedPolarAxisTransformation(Transformation &t, const AzimuthalCoordinates &mpa, const LocationCoordinates &loc);
	static LocalEquatorialCoordinates applyMisalignment(const Transformation &t, const LocalEquatorialCoordinates &a);
	static LocalEquatorialCoordinates applyConeError(const LocalEquatorialCoordinates &a, double cone);

	/*Alignment procedures*/

	/**
	 * One-star alignment (only for testing), to find the PA misalignment
	 */
	static AzimuthalCoordinates alignOneStars(const LocalEquatorialCoordinates &star_ref, const LocalEquatorialCoordinates &star_meas,
			const LocationCoordinates &loc, const AzimuthalCoordinates &pa_start);

	/**
	 * Two-star alignment for finding PA misalignment as well as offset in both axis
	 * @param star_ref Reference stars (array of 2)
	 * @param star_meas Measured stars (array of 2)
	 * @param loc Location
	 * @param pa Initial PA alt-az coordinates. This parameter will be updated with new values
	 * @param offset Initial offset values. This parameter will be updated with new values
	 */
	static void alignTwoStars(const LocalEquatorialCoordinates star_ref[], const LocalEquatorialCoordinates star_meas[], const LocationCoordinates &loc,
			AzimuthalCoordinates &pa, LocalEquatorialCoordinates &offset);

	/**
	 * N-star alignment for finding PA misalignment, offset, and cone error
	 * This function will first call alignTwoStars with the first two stars assuming no cone error, then run an optimization algorithm to minimize the residual error by tweaking all 5 parameters.
	 * @param N number of alignment stars
	 * @param star_ref Reference stars
	 * @param star_meas Measured stars
	 * @param loc Location
	 * @param pa Initial PA alt-az coordinates. This parameter will be updated with new values
	 * @param offset Initial offset values. This parameter will be updated with new values
	 * @param cone Initial cone error. This parameter will be updated with new values
	 */
	static void alignNStars(const int N, const LocalEquatorialCoordinates star_ref[], const LocalEquatorialCoordinates star_meas[], const LocationCoordinates &loc,
			AzimuthalCoordinates &pa, LocalEquatorialCoordinates &offset, double &cone);
};

#endif /* CELESTIALMATH_H_ */

