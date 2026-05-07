#include "coordinate_frame.h"
#include <math.h>

CoordinateFrame::CoordinateFrame()
    : origin_lat_deg_(0.0),
      origin_lon_deg_(0.0),
      origin_alt_m_(0.0),
      origin_lat_rad_(0.0),
      sin_lat_(0.0),
      cos_lat_(1.0),
      origin_ecef_(0.0, 0.0, 0.0),
      is_initialized_(false) {
}

bool CoordinateFrame::initialize(double lat_deg, double lon_deg, double alt_m) {

    // Validate input coordinates
    if (!is_valid_gps(lat_deg, lon_deg, alt_m)) {
        is_initialized_ = false;
        return false;
    }

    // Store origin coordinates
    origin_lat_deg_ = lat_deg;
    origin_lon_deg_ = lon_deg;
    origin_alt_m_ = alt_m;

    // Cache trigonometric values
    cache_trig_values_();

    // Compute ECEF coordinates of origin
    origin_ecef_ = gps_to_ecef(origin_lat_deg_, origin_lon_deg_, origin_alt_m_);

    // Validate ECEF result
    if (!is_valid_ecef(origin_ecef_)) {
        is_initialized_ = false;
        return false;
    }

    is_initialized_ = true;
    return true;
}

bool CoordinateFrame::initializeOnFirstFix(const GPS_Data& gps_data) {
    return initialize(gps_data.latitude_deg, gps_data.longitude_deg, gps_data.altitude_m);
}

bool CoordinateFrame::isInitialized() const {
    return is_initialized_;
}

LocalFrame CoordinateFrame::gpsToLocalNED(double lat_deg, double lon_deg, double alt_m) const {

    assert_initialized_();

    // Validate input coordinates
    if (!is_valid_gps(lat_deg, lon_deg, alt_m)) {
        return LocalFrame(NAN,
                         NAN,
                         NAN);
    }

    // Convert GPS to ECEF
    ECEF point_ecef = gps_to_ecef(lat_deg, lon_deg, alt_m);

    // Validate ECEF result
    if (!is_valid_ecef(point_ecef)) {
        return LocalFrame(NAN,
                         NAN,
                         NAN);
    }

    // Convert ECEF to NED
    LocalFrame ned = ecef_to_ned(point_ecef, origin_ecef_, origin_lat_rad_);

    return ned;
}

GPS_Data CoordinateFrame::localNEDToGPS(const LocalFrame& ned_point) const {

    assert_initialized_();

    // Convert NED to ECEF
    ECEF point_ecef = ned_to_ecef(ned_point, origin_ecef_, origin_lat_rad_);

    // Validate ECEF result
    if (!is_valid_ecef(point_ecef)) {
        return GPS_Data(NAN,
                       NAN,
                       NAN);
    }

    // Convert ECEF to GPS
    GPS_Data gps = ecef_to_gps(point_ecef.x, point_ecef.y, point_ecef.z);

    return gps;
}

GPS_Data CoordinateFrame::getOrigin() const {

    assert_initialized_();

    return GPS_Data(origin_lat_deg_, origin_lon_deg_, origin_alt_m_);
}

double CoordinateFrame::getOriginAltitude() const {

    assert_initialized_();

    return origin_alt_m_;
}

bool CoordinateFrame::reinitialize(double lat_deg, double lon_deg, double alt_m) {
    // reinitialize just calls initialize, which handles locking
    return initialize(lat_deg, lon_deg, alt_m);
}

void CoordinateFrame::cache_trig_values_() {
    // Convert latitude to radians and cache it
    origin_lat_rad_ = origin_lat_deg_ * M_PI / 180.0;

    // Pre-compute and cache sine and cosine of origin latitude
    sin_lat_ = sin(origin_lat_rad_);
    cos_lat_ = cos(origin_lat_rad_);
}

bool CoordinateFrame::assert_initialized_() const {
    return is_initialized_;
}
