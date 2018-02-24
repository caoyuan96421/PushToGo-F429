/*
 * main.cpp
 *
 *  Created on: Feb 21, 2018
 *      Author: Yuan
 */

#include <stdio.h>
#include "CelestialMath.h"
#include <math.h>
#include <stdlib.h>

void xprintf(const char *, ...);

double noise() {
	return (double) rand() / RAND_MAX * 0.5;
}

int test1(double dec, double ha) {
	LocalEquatorialCoordinates coord(dec, ha);
	LocationCoordinates loc(42.0, 0);
	AzimuthalCoordinates ac = CelestialMath::localEquatorialToAzimuthal(coord, loc);
	xprintf("Alt = %f,\tAzi = %f\n", ac.alt, ac.azi);
	coord = CelestialMath::azimuthalToLocalEquatorial(ac, loc);
	xprintf("Dec = %.10f,\tHA = %.10f\n", coord.dec, coord.ha);

	return (fabs(coord.dec - dec) < 1e-7 && (fabs(dec) > 90.0 - 1e-7 || fabs(remainder(coord.ha - ha, 360.0)) < 1e-7));
}

void test2() {
	double dec = 90;
	double ha = 0;
	LocationCoordinates loc(42, 124);
	AzimuthalCoordinates mpa(42, 4); // Misaligned polar axis (too high by 0.1 degree)

	Transformation t;
	CelestialMath::getMisalignedPolarAxisTransformation(t, mpa, loc);

	LocalEquatorialCoordinates mc = CelestialMath::applyMisalignment(t, LocalEquatorialCoordinates(dec, ha));

	xprintf("In misaligned coordinates: DEC=%f, HA=%f\n", mc.dec, mc.ha);

	xprintf("T=\n%f\t%f\t%f\n%f\t%f\t%f\n%f\t%f\t%f\n", t.a11, t.a12, t.a13, t.a21, t.a22, t.a23, t.a31, t.a32, t.a33);
}

void test3() {
	LocalEquatorialCoordinates star(-50, 60);
	LocationCoordinates loc(42, 0);
	AzimuthalCoordinates mpa(42.0 - 0.01, -13.3); // Misaligned PA
	Transformation t;
	CelestialMath::getMisalignedPolarAxisTransformation(t, mpa, loc);
	LocalEquatorialCoordinates star_meas = CelestialMath::applyMisalignment(t, star);

	AzimuthalCoordinates fit_pa = CelestialMath::alignOneStars(star, star_meas, loc, AzimuthalCoordinates(42, 0));

	xprintf("Fitted PA: ALT=%f, AZI=%f\n", fit_pa.alt, fit_pa.azi);
}

void test4() {
	LocalEquatorialCoordinates star[2] = { LocalEquatorialCoordinates(-20, 60), LocalEquatorialCoordinates(45, 175) };
	LocationCoordinates loc(42, 0);
	AzimuthalCoordinates mpa(42.0 - 4.1234, -1.567); // Misaligned PA
	LocalEquatorialCoordinates offset(2, 5); // Offset in the two axis.
	Transformation t;
	CelestialMath::getMisalignedPolarAxisTransformation(t, mpa, loc);
	LocalEquatorialCoordinates star_meas[2] = { CelestialMath::applyMisalignment(t, star[0]) + offset + noise(), CelestialMath::applyMisalignment(t, star[1])
			+ offset + noise() };

	AzimuthalCoordinates fit_pa(42, 0);
	LocalEquatorialCoordinates fit_offset(0, 0);

	CelestialMath::alignTwoStars(star, star_meas, loc, fit_pa, fit_offset);

	xprintf("Fitted PA: ALT=%f, AZI=%f\n", remainder(fit_pa.alt, 360), remainder(fit_pa.azi, 360));
	xprintf("Fitted OFF: DEC=%f, HA=%f\n", fit_offset.dec, fit_offset.ha);
}

void test5() {
	LocalEquatorialCoordinates star[3] = { LocalEquatorialCoordinates(-20, 60), LocalEquatorialCoordinates(45, 175), LocalEquatorialCoordinates(62, 35) };
	LocationCoordinates loc(42, 0);
	AzimuthalCoordinates mpa(42.0 - 14.1234, -11.567); // Misaligned PA
	LocalEquatorialCoordinates offset(7, 20); // Offset in the two axis.
	double cone = 5;
	Transformation t;
	CelestialMath::getMisalignedPolarAxisTransformation(t, mpa, loc);
	LocalEquatorialCoordinates star_meas[3] = { CelestialMath::applyConeError(CelestialMath::applyMisalignment(t, star[0]), cone) + offset + noise(),
			CelestialMath::applyConeError(CelestialMath::applyMisalignment(t, star[1]), cone) + offset + noise(), CelestialMath::applyConeError(
					CelestialMath::applyMisalignment(t, star[2]), cone) + offset + noise() };

	AzimuthalCoordinates fit_pa(42, 0);
	LocalEquatorialCoordinates fit_offset(0, 0);
	double fit_cone = 1;

	CelestialMath::alignNStars(3, star, star_meas, loc, fit_pa, fit_offset, fit_cone);

	xprintf("Fitted PA: ALT=%f, AZI=%f\n", fit_pa.alt, fit_pa.azi);
	xprintf("Fitted OFF: DEC=%f, HA=%f\n", fit_offset.dec, fit_offset.ha);
	xprintf("Fitted cone: %f\n", fit_cone);
}

int testmath() {
	xprintf("%d\n", test1(0, 0));
	xprintf("%d\n", test1(90, 0));
	xprintf("%d\n", test1(-90, 0));
	xprintf("%d\n", test1(35, 360));
	xprintf("%d\n", test1(46.12345, 720));
	xprintf("%d\n", test1(89.99998, 187));

	test2();
	test3();
	test4();
	test5();

	return 0;
}


