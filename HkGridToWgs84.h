#pragma once

// Approximate HK 1980 grid (easting E, northing N in metres) → WGS84 degrees.
// Calibrated with STAR FERRY (4025) and CHUK YUEN ESTATE (4001); good enough for map polylines.
inline void hkGridToWgs84Approx(double easting, double northing, double *latDeg, double *lngDeg)
{
    constexpr double E0 = 835463.0;
    constexpr double N0 = 817244.0;
    constexpr double refLat = 22.29339;
    constexpr double refLng = 114.16876;
    constexpr double dLngdE = 1.018e-5;
    constexpr double dLatdN = 9.13e-6;
    *lngDeg = refLng + dLngdE * (easting - E0);
    *latDeg = refLat + dLatdN * (northing - N0);
}
