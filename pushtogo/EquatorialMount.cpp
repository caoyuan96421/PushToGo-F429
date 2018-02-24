
#include <math.h>
#include "EquatorialMount.h"

#ifndef M_PI
#define M_PI 3.141592653589793
#endif

static const double to_radian = M_PI / 180.0;
static const double to_deg = 180.0 / M_PI;
static const double sidereal_day = 86164.09;

void xprintf(const char *, ...);

static inline double sqr(double x)
{
    return x*x;
}

EquatorialMount::EquatorialMount(RotationAxis *ra, RotationAxis *dec, UTCClock *clk, LocationProvider *loc) : ra(ra), dec(dec), clock(clk), location(loc),
    cone_value(0), ma_alt(0), ma_azi(0), off_ra(0), off_dec(0)
{
}

void EquatorialMount::correct_for_misalignment(double *ra, double *dec)
{
    /**
    * This function must be performed using double values, since some of the operation can generate a large amount of error using float
    * On 168MHz STM32F4, it takes about 300us to complete the entire computation. Expect to be much faster on F7 with dp-FPU
    */
    double x,y,z;
    double xa,ya,za;
    double ra_perfect = *ra;
    double dec_perfect = *dec;
    double ra_zenith = get_ra_zenith(); // get the theoretical RA of the local zenith
    double dec_zenith = get_dec_zenith(); // get the theoretical DEC of the local zenith
    // Calculate Cartesian coordinate on the equatorial reference frame (unit sphere), in the perfect world
    // x axis points to RA=DEC=0, y axis points to RA=6h DEC=0, z axis points to DEC=90 deg
    sphere_to_cart(&x, &y, &z, dec_perfect, ra_perfect);

    /*************************/
    // Now, we calculate the real polar axis (PA) in the RA-DEC coordinate
    // But before that, let us switch first to the local alt-az coordinate system
    // And define the real polar axis in this coordinate system
    double alt_pa = dec_zenith + ma_alt; // Misaligned PA altitude. This value will be >0 for north semisphere and <0 for south
    double azi_pa = ma_azi; // Misaligned PA azimuth
    // Calculate Cartesian coordinate of the real polar axis (PA) in alt-az coordinate system.
    // x axis is local North, y axis is local East, z axis is local zenith
    sphere_to_cart(&xa, &ya, &za, alt_pa, azi_pa);

    // Now we can use point product to transform the real PA into RA-DEC system
    double dec_pa, ra_pa;
    double cos_dec_zenith = cos(dec_zenith*to_radian);
    double sin_dec_zenith = sin(dec_zenith*to_radian);
    double pa_dot = xa*cos(dec_zenith*to_radian) + za*sin(dec_zenith*to_radian); // Dot product between misaligned axis and perfect axis
    if(pa_dot > 1) pa_dot = 1;
    if(pa_dot < -1) pa_dot = -1;
    dec_pa = asin(pa_dot) * to_deg; // Calculate DEC of the misaligned axis. This is simple
    // Calculate RA of the misaligned axis. This is slightly involved. Everything is still in the alt-az local coordinate system
    double xP = cos_dec_zenith, yP = 0, zP = sin_dec_zenith; // The perfect polar axis is at (cos(dec_zenith), 0, sin(dec_zenith))
    double xA = 1-sqr(cos_dec_zenith), /*yA = 0,*/ zA = -cos_dec_zenith*sin_dec_zenith; // Projection of (1,0,0) onto the perfect equatorial plane
    double xB = xa - pa_dot * xP, yB = ya - pa_dot * yP, zB = za - pa_dot * zP; // Projection of misaligned axis onto the equatorial plane
    double cosRA = xA*xB + /*yA*yB +*/ zA*zB; // dot-product between the A and B vectors define the cos of the RA
    double sinRA = xP*(/*yA*zB*/-zA*yB) + yP*(zA*xB-xA*zB) + zP*(xA*yB/*-*yA*xB*/); // cross-product between the A and B, then dotted onto P gives the sin of the RA
    ra_pa = remainder(ra_zenith + 180.0 + atan2(sinRA, cosRA) * to_deg, 360.0); // atan2f of the sin and cos takes care of the sign of the angles

    /***********/
    // Now we are back to the RA-DEC coordinate system
    double xmp, ymp, zmp;
    sphere_to_cart(&xmp, &ymp, &zmp, dec_pa, ra_pa); // Calculate the Cartesian coordinate of misaligned PA

    double q_dot = x*xmp + y*ymp + z*zmp; // Dot product between the misaligned PA and the target position in RA-DEC space
    if(q_dot > 1)q_dot = 1;
    if(q_dot < -1)q_dot = -1;
    double dec_target = asin(q_dot) * to_deg; // Target DEC position
    double xI = -sin(ra_pa*to_radian), yI=cos(ra_pa*to_radian); // Intersection between misaligned equatorial plane and perfect equatorial plane
    double xT = x - q_dot * xmp, yT = y - q_dot * ymp, zT = z - q_dot * zmp; // Projection of target point on the equatorial plane of the misaligned PA
    double cosT = xI*xT + yI*yT; // zI=0, so omitted. dot-product between the two vectors
    double sinT = xmp*(yI*zT/*-zI*yT*/) + ymp*(/*zI*xT*/-xI*zT) + zmp*(xI*yT-yI*xT); // Cross-product of the two vectors dotted onto the misaligned PA
    double ra_target = remainder(ra_pa + 90.0 + atan2(sinT, cosT) * to_deg, 360.0); // atan2. The 90.0 degree is from the fact that the intersection is 90 degrees ahead of the ra_pa in the RA direction

    /*Update the final value*/
    *ra = ra_target;
    *dec = dec_target;
}

void EquatorialMount::sphere_to_cart(double *x, double *y, double *z, double alt, double azi)
{
    *x = cos(azi*to_radian) * cos(alt*to_radian);
    *y = sin(azi*to_radian) * cos(alt*to_radian);
    *z = sin(alt*to_radian);
}

double EquatorialMount::get_ra_zenith()
{
    time_t timeNow = clock->getTime(); // This is UTC time, which is time at 0 degree longtitude
    double jd = (double)timeNow*1.1574074074074E-5 + 2440587.5;// Julian Date (J2000)
    double gmst = 280.46061837 + 360.985647366 * jd; // Greenwich mean sidereal time (angle)
    double lst = gmst + location->getLongtitude() * 1.00273790935; // Local sidereal time (angle)
    return remainder(lst, 360.0);
}

double EquatorialMount::get_dec_zenith()
{
    // DEC of the Zenith is always equal to the local latitude
    return location->getLatitude();
}

void EquatorialMount::correct_for_cone(double *ra, double *dec)
{
    /*
    * RA, DEC are the coordinates in the mount reference frame, with perfectly perpendicular axis
    * Now we need to put in the cone value
    * cone_value > 0 means the angle between the two axis is > 90 deg
    * cone_value < 0.........................................< 90 deg
    * This means that there is a small patch of region near the poles that we CANNOT access
    */
    
    // Equation of a great circle: -x*sin(phi0)+y*cos(phi0)+z*tan(cone_value)=0
    // Or : sin(ra - phi0) = -tan(dec) * tan(cone_value)
    // cone_value>0 -> positive dec ~ smaller ra
    // phi0 is the angle of intersection between this circle and the equator
    // cone_value=0 -> meridian, cone_value = 90 -> equator
    // When dec>90-abs(cone_value) there will be no solution
    // because tan(dec)>tan(90-abs(cone_value))=cot(abs(cone_value)), 
    // abs(sin(ra-phi0)) = abs(tan(dec)*tan(cone_value)) > cot(abs(cone_value))*tan(abs(cone_value)) = 1

    double s1 = tan(*dec) * tan(cone_value); // = -sin(ra-phi0)
    if (s1 > 1) s1 = 1;
    if (s1 < -1)s1 = -1;
    double phi0 = *ra + asin(s1); // This is the apparent RA on a mount with cone error
    
    // Now get the apparent DEC
    // To do this, again look at the great circle
    // let the direction of phi0 be x' axis, and deduce the corresponding y' and z' axis
    // then the great circle is written
    // y' = -z' * tan(cone_value), x'^2+y'^2+z'^2=1
    // the angle on the circle relative to the x' axis is
    // dec' = arcsin(z'/cos(cone_value))
    // since z'=z (we only did a rotation by phi0 around the z axis), 
    // and z=sin(dec), we have
    double s2 = sin(*dec) / cos(cone_value); // =sin(dec')
    if (s2 > 1) s2 = 1;
    if (s2 < -1) s2 = -1;
    double newdec = asin(s2); // This is the apparent DEC on a mount with cone error
    
    // update the values
    *ra = phi0;
    *dec = newdec;
}

void EquatorialMount::correct_for_offset(double *ra, double *dec)
{
    *ra = *ra + off_ra;
    *dec = *dec + off_dec;
    // Note that here we are simpling adding the offset
    // THis means that DEC can be >90 or <-90
}
/*
void EquatorialMount::correct_for_all(double *ra, double *dec){
    correct_for_misalignment(ra, dec);
    correct_for_cone(ra, dec);
    correct_for_offset(ra, dec);
}
*/