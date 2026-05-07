#include "quaternion.h"

float Quaternion::magnitude() const {
    return std::sqrt(w * w + x * x + y * y + z * z);
}

float Quaternion::magnitude_squared() const {
    return w * w + x * x + y * y + z * z;
}

bool Quaternion::is_normalized(float tolerance) const {
    float mag = magnitude();
    return std::fabs(mag - 1.0f) < tolerance;
}

bool Quaternion::is_valid() const {
    // Check that magnitude is not zero or NaN
    float mag_sq = magnitude_squared();
    return mag_sq > 1e-12f && mag_sq == mag_sq;  // mag_sq == mag_sq checks for NaN
}

Quaternion& Quaternion::normalize() {
    float mag = magnitude();

    if (mag < 1e-6f) {
        // Degenerate case: magnitude is zero or very small
        // Reset to identity
        w = 1.0f;
        x = 0.0f;
        y = 0.0f;
        z = 0.0f;
        return *this;
    }

    float inv_mag = 1.0f / mag;
    w *= inv_mag;
    x *= inv_mag;
    y *= inv_mag;
    z *= inv_mag;

    return *this;
}

Quaternion Quaternion::normalized() const {
    Quaternion result = *this;
    result.normalize();
    return result;
}

Quaternion Quaternion::conjugate() const {
    return Quaternion(w, -x, -y, -z);
}

Quaternion& Quaternion::conjugate_in_place() {
    x = -x;
    y = -y;
    z = -z;
    // w is unchanged
    return *this;
}

float Quaternion::dot(const Quaternion& other) const {
    return w * other.w + x * other.x + y * other.y + z * other.z;
}

Quaternion Quaternion::multiply(const Quaternion& other) const {
    // q_result = this * other
    // Represents: first apply 'other', then apply 'this'
    //
    // Formula (from QUATERNION_REFERENCE.md):
    // w_out = w1*w2 - x1*x2 - y1*y2 - z1*z2
    // x_out = w1*x2 + x1*w2 + y1*z2 - z1*y2
    // y_out = w1*y2 - x1*z2 + y1*w2 + z1*x2
    // z_out = w1*z2 + x1*y2 - y1*x2 + z1*w2

    return Quaternion(
        w * other.w - x * other.x - y * other.y - z * other.z,
        w * other.x + x * other.w + y * other.z - z * other.y,
        w * other.y - x * other.z + y * other.w + z * other.x,
        w * other.z + x * other.y - y * other.x + z * other.w
    );
}

Quaternion& Quaternion::multiply_in_place(const Quaternion& other) {
    Quaternion result = multiply(other);
    *this = result;
    return *this;
}

void Quaternion::to_array(float out[4]) const {
    out[0] = w;
    out[1] = x;
    out[2] = y;
    out[3] = z;
}

Quaternion& Quaternion::from_array(const float components[4]) {
    w = components[0];
    x = components[1];
    y = components[2];
    z = components[3];
    return *this;
}
