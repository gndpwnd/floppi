#include "quaternion_conversions.h"

// ============================================================================
// EULER ANGLES METHODS
// ============================================================================

EulerAngles EulerAngles::to_degrees() const {
    const float RAD_TO_DEG = 57.29577951f;  // 180/π
    return EulerAngles(roll * RAD_TO_DEG, pitch * RAD_TO_DEG, yaw * RAD_TO_DEG);
}

EulerAngles EulerAngles::to_radians() const {
    const float DEG_TO_RAD = 0.0174532925f;  // π/180
    return EulerAngles(roll * DEG_TO_RAD, pitch * DEG_TO_RAD, yaw * DEG_TO_RAD);
}

// ============================================================================
// VECTOR3D METHODS
// ============================================================================

float Vector3D::magnitude() const {
    return sqrt(x * x + y * y + z * z);
}

float Vector3D::magnitude_squared() const {
    return x * x + y * y + z * z;
}

Vector3D Vector3D::normalized() const {
    float mag = magnitude();
    if (mag < 1e-6f) {
        return Vector3D(1.0f, 0.0f, 0.0f);  // Degenerate case
    }
    float inv_mag = 1.0f / mag;
    return Vector3D(x * inv_mag, y * inv_mag, z * inv_mag);
}

float Vector3D::dot(const Vector3D& other) const {
    return x * other.x + y * other.y + z * other.z;
}

Vector3D Vector3D::cross(const Vector3D& other) const {
    return Vector3D(
        y * other.z - z * other.y,
        z * other.x - x * other.z,
        x * other.y - y * other.x
    );
}

// ============================================================================
// ROTATION MATRIX METHODS
// ============================================================================

RotationMatrix::RotationMatrix() {
    set_identity();
}

void RotationMatrix::set_identity() {
    m[0][0] = 1.0f; m[0][1] = 0.0f; m[0][2] = 0.0f;
    m[1][0] = 0.0f; m[1][1] = 1.0f; m[1][2] = 0.0f;
    m[2][0] = 0.0f; m[2][1] = 0.0f; m[2][2] = 1.0f;
}

Vector3D RotationMatrix::rotate_vector(const Vector3D& v) const {
    return Vector3D(
        m[0][0] * v.x + m[0][1] * v.y + m[0][2] * v.z,
        m[1][0] * v.x + m[1][1] * v.y + m[1][2] * v.z,
        m[2][0] * v.x + m[2][1] * v.y + m[2][2] * v.z
    );
}

bool RotationMatrix::is_orthonormal(float tolerance) const {
    // Check R * R^T = I
    // Compute R * R^T
    RotationMatrix rt = transpose();
    RotationMatrix product;

    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            float sum = 0.0f;
            for (int k = 0; k < 3; k++) {
                sum += m[i][k] * rt.m[k][j];
            }
            product.m[i][j] = sum;
        }
    }

    // Check that product is approximately identity
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            float expected = (i == j) ? 1.0f : 0.0f;
            if (fabs(product.m[i][j] - expected) > tolerance) {
                return false;
            }
        }
    }

    return true;
}

RotationMatrix RotationMatrix::transpose() const {
    RotationMatrix result;
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            result.m[i][j] = m[j][i];
        }
    }
    return result;
}

// ============================================================================
// QUATERNION TO EULER ANGLES
// ============================================================================

EulerAngles quaternion_to_euler(const Quaternion& q) {
    EulerAngles euler;

    // Normalize before extraction. The ZYX (Tait-Bryan) formulas below assume
    // a unit quaternion: pitch = asin(2*(w*y - z*x)) is only valid when
    // |q| == 1. Sensor / EKF / round-trip quaternions accumulate float32
    // round-off and arrive slightly off-norm. Near the pitch = ±90° gimbal-lock
    // singularity that tiny norm error is catastrophic: asin's derivative
    // 1/sqrt(1-x^2) diverges as x -> 1, so a ~1e-7 error in the asin argument
    // is amplified into a ~0.03° pitch error (enough to break a 0.01° tolerance
    // at exactly 90°). Renormalizing pins |q|=1 so the singular case lands on
    // |sinp| >= 1 and is handled exactly by the guard below.
    Quaternion qn = q;
    qn.normalize();

    // Roll (x-axis rotation)
    float sinr_cosp = 2.0f * (qn.w * qn.x + qn.y * qn.z);
    float cosr_cosp = 1.0f - 2.0f * (qn.x * qn.x + qn.y * qn.y);
    euler.roll = atan2(sinr_cosp, cosr_cosp);

    // Pitch (y-axis rotation)
    float sinp = 2.0f * (qn.w * qn.y - qn.z * qn.x);
    // Clamp into [-1, 1]: even after normalization, float round-off can leave
    // |sinp| a hair above 1 at the singularity. The clamp keeps asin in-domain
    // and snaps the singular case to exactly ±90°.
    if (sinp >= 1.0f) {
        euler.pitch = M_PI / 2.0f;
    } else if (sinp <= -1.0f) {
        euler.pitch = -M_PI / 2.0f;
    } else {
        euler.pitch = asin(sinp);
    }

    // Yaw (z-axis rotation)
    float siny_cosp = 2.0f * (qn.w * qn.z + qn.x * qn.y);
    float cosy_cosp = 1.0f - 2.0f * (qn.y * qn.y + qn.z * qn.z);
    euler.yaw = atan2(siny_cosp, cosy_cosp);

    return euler;
}

EulerAngles quaternion_to_euler_degrees(const Quaternion& q) {
    EulerAngles euler = quaternion_to_euler(q);
    return euler.to_degrees();
}

// ============================================================================
// EULER ANGLES TO QUATERNION
// ============================================================================

Quaternion euler_to_quaternion(float roll, float pitch, float yaw) {
    // Compute half angles
    float roll_half = roll / 2.0f;
    float pitch_half = pitch / 2.0f;
    float yaw_half = yaw / 2.0f;

    float cr = cos(roll_half);
    float sr = sin(roll_half);
    float cp = cos(pitch_half);
    float sp = sin(pitch_half);
    float cy = cos(yaw_half);
    float sy = sin(yaw_half);

    Quaternion q;
    q.w = cr * cp * cy + sr * sp * sy;
    q.x = sr * cp * cy - cr * sp * sy;
    q.y = cr * sp * cy + sr * cp * sy;
    q.z = cr * cp * sy - sr * sp * cy;

    return q;
}

Quaternion euler_to_quaternion_degrees(float roll_deg, float pitch_deg, float yaw_deg) {
    const float DEG_TO_RAD = 0.0174532925f;  // π/180

    return euler_to_quaternion(
        roll_deg * DEG_TO_RAD,
        pitch_deg * DEG_TO_RAD,
        yaw_deg * DEG_TO_RAD
    );
}

// ============================================================================
// QUATERNION TO ROTATION MATRIX
// ============================================================================

RotationMatrix quaternion_to_rotation_matrix(const Quaternion& q) {
    RotationMatrix R;

    // Pre-compute squared values for efficiency
    float x2 = q.x * q.x;
    float y2 = q.y * q.y;
    float z2 = q.z * q.z;

    float xy2 = 2.0f * q.x * q.y;
    float xz2 = 2.0f * q.x * q.z;
    float yz2 = 2.0f * q.y * q.z;
    float wx2 = 2.0f * q.w * q.x;
    float wy2 = 2.0f * q.w * q.y;
    float wz2 = 2.0f * q.w * q.z;

    // Build rotation matrix (formula from QUATERNION_REFERENCE.md)
    R.m[0][0] = 1.0f - 2.0f * (y2 + z2);
    R.m[0][1] = xy2 - wz2;
    R.m[0][2] = xz2 + wy2;

    R.m[1][0] = xy2 + wz2;
    R.m[1][1] = 1.0f - 2.0f * (x2 + z2);
    R.m[1][2] = yz2 - wx2;

    R.m[2][0] = xz2 - wy2;
    R.m[2][1] = yz2 + wx2;
    R.m[2][2] = 1.0f - 2.0f * (x2 + y2);

    return R;
}

// ============================================================================
// ROTATION MATRIX TO QUATERNION (Shepperd's Method)
// ============================================================================

Quaternion rotation_matrix_to_quaternion(const RotationMatrix& R) {
    Quaternion q;

    // Compute trace: sum of diagonal elements
    float trace = R.m[0][0] + R.m[1][1] + R.m[2][2];

    if (trace > 0.0f) {
        // Case 1: trace > 0 (w is the largest component)
        float S = 2.0f * sqrt(trace + 1.0f);
        q.w = 0.25f * S;
        q.x = (R.m[2][1] - R.m[1][2]) / S;
        q.y = (R.m[0][2] - R.m[2][0]) / S;
        q.z = (R.m[1][0] - R.m[0][1]) / S;
    } else if ((R.m[0][0] > R.m[1][1]) && (R.m[0][0] > R.m[2][2])) {
        // Case 2: R[0][0] is the largest diagonal element (x is largest)
        float S = 2.0f * sqrt(1.0f + R.m[0][0] - R.m[1][1] - R.m[2][2]);
        q.w = (R.m[2][1] - R.m[1][2]) / S;
        q.x = 0.25f * S;
        q.y = (R.m[0][1] + R.m[1][0]) / S;
        q.z = (R.m[0][2] + R.m[2][0]) / S;
    } else if (R.m[1][1] > R.m[2][2]) {
        // Case 3: R[1][1] is the largest diagonal element (y is largest)
        float S = 2.0f * sqrt(1.0f + R.m[1][1] - R.m[0][0] - R.m[2][2]);
        q.w = (R.m[0][2] - R.m[2][0]) / S;
        q.x = (R.m[0][1] + R.m[1][0]) / S;
        q.y = 0.25f * S;
        q.z = (R.m[1][2] + R.m[2][1]) / S;
    } else {
        // Case 4: R[2][2] is the largest diagonal element (z is largest)
        float S = 2.0f * sqrt(1.0f + R.m[2][2] - R.m[0][0] - R.m[1][1]);
        q.w = (R.m[1][0] - R.m[0][1]) / S;
        q.x = (R.m[0][2] + R.m[2][0]) / S;
        q.y = (R.m[1][2] + R.m[2][1]) / S;
        q.z = 0.25f * S;
    }

    // Ensure normalization
    q.normalize();
    return q;
}

// ============================================================================
// VECTOR ROTATION
// ============================================================================

Vector3D rotate_vector_by_quaternion(const Quaternion& q, const Vector3D& v) {
    // Use efficient matrix form instead of quaternion multiplication
    // This avoids computing the full rotation matrix

    float w2 = q.w * q.w;
    float x2 = q.x * q.x;
    float y2 = q.y * q.y;
    float z2 = q.z * q.z;

    float xy2 = 2.0f * q.x * q.y;
    float xz2 = 2.0f * q.x * q.z;
    float yz2 = 2.0f * q.y * q.z;
    float wx2 = 2.0f * q.w * q.x;
    float wy2 = 2.0f * q.w * q.y;
    float wz2 = 2.0f * q.w * q.z;

    // Apply rotation
    Vector3D rotated;
    rotated.x = (w2 + x2 - y2 - z2) * v.x + (xy2 - wz2) * v.y + (xz2 + wy2) * v.z;
    rotated.y = (xy2 + wz2) * v.x + (w2 - x2 + y2 - z2) * v.y + (yz2 - wx2) * v.z;
    rotated.z = (xz2 - wy2) * v.x + (yz2 + wx2) * v.y + (w2 - x2 - y2 + z2) * v.z;

    return rotated;
}

// ============================================================================
// AXIS-ANGLE CONVERSIONS
// ============================================================================

Quaternion axis_angle_to_quaternion(const Vector3D& axis, float angle) {
    Vector3D normalized_axis = axis.normalized();

    float half_angle = angle / 2.0f;
    float sin_half = sin(half_angle);

    Quaternion q;
    q.w = cos(half_angle);
    q.x = normalized_axis.x * sin_half;
    q.y = normalized_axis.y * sin_half;
    q.z = normalized_axis.z * sin_half;

    return q;
}

void quaternion_to_axis_angle(const Quaternion& q, Vector3D& out_axis, float& out_angle) {
    // Clamp w to [-1, 1] to avoid acos domain errors
    float w_clamped = fmax(-1.0f, fmin(1.0f, q.w));
    out_angle = 2.0f * acos(w_clamped);

    float sin_half = sin(out_angle / 2.0f);

    if (fabs(sin_half) < 1e-6f) {
        // Zero rotation or very small angle
        out_axis = Vector3D(1.0f, 0.0f, 0.0f);  // Arbitrary axis
        out_angle = 0.0f;
    } else {
        float inv_sin = 1.0f / sin_half;
        out_axis = Vector3D(q.x * inv_sin, q.y * inv_sin, q.z * inv_sin);
    }
}

// ============================================================================
// FRAME TRANSFORMATIONS
// ============================================================================

Vector3D transform_body_to_world(const Quaternion& attitude_world_to_body,
                                  const Vector3D& body_vector) {
    // attitude_world_to_body represents rotation from world to body
    // To transform body -> world, use inverse (conjugate)
    Quaternion q_body_to_world = attitude_world_to_body.conjugate();
    return rotate_vector_by_quaternion(q_body_to_world, body_vector);
}

Vector3D transform_world_to_body(const Quaternion& attitude_world_to_body,
                                  const Vector3D& world_vector) {
    // attitude_world_to_body already rotates world -> body
    return rotate_vector_by_quaternion(attitude_world_to_body, world_vector);
}
