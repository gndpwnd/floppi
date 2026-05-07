#ifndef QUATERNION_CONVERSIONS_H
#define QUATERNION_CONVERSIONS_H

#include "quaternion.h"

/**
 * Euler Angles Structure (radians and degrees)
 */
struct EulerAngles {
    float roll;   // Radians
    float pitch;  // Radians
    float yaw;    // Radians

    EulerAngles() : roll(0.0f), pitch(0.0f), yaw(0.0f) {}

    EulerAngles(float r, float p, float y) : roll(r), pitch(p), yaw(y) {}

    /**
     * Convert to degrees.
     * @return New EulerAngles with values in degrees
     */
    EulerAngles to_degrees() const;

    /**
     * Convert to radians (in-place conversion if needed).
     * @return New EulerAngles with values in radians
     */
    EulerAngles to_radians() const;
};

/**
 * 3D Vector structure
 */
struct Vector3D {
    float x, y, z;

    Vector3D() : x(0.0f), y(0.0f), z(0.0f) {}

    Vector3D(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}

    float magnitude() const;

    float magnitude_squared() const;

    Vector3D normalized() const;

    float dot(const Vector3D& other) const;

    Vector3D cross(const Vector3D& other) const;
};

/**
 * 3x3 Rotation Matrix
 * Stored in row-major order: m[row][col]
 * Used for rotating vectors: v_out = R * v_in
 */
struct RotationMatrix {
    float m[3][3];

    RotationMatrix();

    /**
     * Initialize to identity matrix.
     */
    void set_identity();

    /**
     * Rotate a vector: result = this * vector
     * @param v Input vector
     * @return Rotated vector
     */
    Vector3D rotate_vector(const Vector3D& v) const;

    /**
     * Check if matrix is orthonormal (valid rotation matrix).
     * Verifies: R * R^T = I (within tolerance)
     * @param tolerance Maximum acceptable deviation (default 0.001)
     * @return True if matrix is approximately orthonormal
     */
    bool is_orthonormal(float tolerance = 0.001f) const;

    /**
     * Get the transpose of this matrix.
     * @return Transposed matrix (equivalent to inverse for rotation matrices)
     */
    RotationMatrix transpose() const;
};

// ============================================================================
// QUATERNION TO/FROM CONVERSIONS
// ============================================================================

/**
 * Convert quaternion to Euler angles (ZYX convention).
 *
 * ZYX convention means: intrinsic rotations Z (yaw), then Y (pitch), then X (roll).
 * This is also known as Tait-Bryan angles.
 *
 * Formulas (from QUATERNION_REFERENCE.md):
 *   roll  = atan2(2*(w*x + y*z), 1 - 2*(x² + y²))
 *   pitch = asin(constrain(2*(w*y - z*x), -1, 1))
 *   yaw   = atan2(2*(w*z + x*y), 1 - 2*(y² + z²))
 *
 * @param q Input quaternion (should be normalized)
 * @return EulerAngles in radians
 */
EulerAngles quaternion_to_euler(const Quaternion& q);

/**
 * Convert quaternion to Euler angles in degrees.
 * @param q Input quaternion
 * @return EulerAngles in degrees
 */
EulerAngles quaternion_to_euler_degrees(const Quaternion& q);

/**
 * Convert Euler angles to quaternion (ZYX convention).
 *
 * Formulas (from QUATERNION_REFERENCE.md):
 *   cr = cos(roll/2),   sr = sin(roll/2)
 *   cp = cos(pitch/2),  sp = sin(pitch/2)
 *   cy = cos(yaw/2),    sy = sin(yaw/2)
 *
 *   w = cr*cp*cy + sr*sp*sy
 *   x = sr*cp*cy - cr*sp*sy
 *   y = cr*sp*cy + sr*cp*sy
 *   z = cr*cp*sy - sr*sp*cy
 *
 * @param roll Roll in radians
 * @param pitch Pitch in radians
 * @param yaw Yaw in radians
 * @return Normalized quaternion
 */
Quaternion euler_to_quaternion(float roll, float pitch, float yaw);

/**
 * Convert Euler angles (in degrees) to quaternion.
 * @param roll Roll in degrees
 * @param pitch Pitch in degrees
 * @param yaw Yaw in degrees
 * @return Normalized quaternion
 */
Quaternion euler_to_quaternion_degrees(float roll, float pitch, float yaw);

/**
 * Convert quaternion to rotation matrix.
 *
 * Formula (from QUATERNION_REFERENCE.md):
 *   R(q) = [
 *     1-2(y²+z²)    2(xy-wz)      2(xz+wy)
 *     2(xy+wz)      1-2(x²+z²)    2(yz-wx)
 *     2(xz-wy)      2(yz+wx)      1-2(x²+y²)
 *   ]
 *
 * The resulting matrix is orthonormal: R * R^T = I
 *
 * @param q Input quaternion (should be normalized)
 * @return 3x3 rotation matrix
 */
RotationMatrix quaternion_to_rotation_matrix(const Quaternion& q);

/**
 * Convert rotation matrix to quaternion using Shepperd's method.
 *
 * Shepperd's method is numerically stable and avoids division by very small numbers
 * by choosing the largest denominator.
 *
 * References:
 *   Shepperd, S. W. (1978). "Quaternion from rotation matrix"
 *   Used in Eigen and other robust quaternion libraries
 *
 * @param R Input rotation matrix (should be orthonormal)
 * @return Normalized quaternion
 */
Quaternion rotation_matrix_to_quaternion(const RotationMatrix& R);

/**
 * Rotate a vector using a quaternion.
 *
 * Applies the rotation: v' = q * v * q^*
 *
 * Internally uses the efficient matrix form:
 *   v'_x = (1-2(y²+z²))*v_x + 2(xy-wz)*v_y + 2(xz+wy)*v_z
 *   v'_y = 2(xy+wz)*v_x + (1-2(x²+z²))*v_y + 2(yz-wx)*v_z
 *   v'_z = 2(xz-wy)*v_x + 2(yz+wx)*v_y + (1-2(x²+y²))*v_z
 *
 * @param q Rotation quaternion (should be normalized)
 * @param v Input vector
 * @return Rotated vector
 */
Vector3D rotate_vector_by_quaternion(const Quaternion& q, const Vector3D& v);

// ============================================================================
// AXIS-ANGLE CONVERSIONS (helper functions)
// ============================================================================

/**
 * Convert axis-angle to quaternion.
 *
 * Formula:
 *   w = cos(angle/2)
 *   x = axis.x * sin(angle/2)
 *   y = axis.y * sin(angle/2)
 *   z = axis.z * sin(angle/2)
 *
 * @param axis Unit rotation axis (will be normalized if not)
 * @param angle Rotation angle in radians
 * @return Normalized quaternion
 */
Quaternion axis_angle_to_quaternion(const Vector3D& axis, float angle);

/**
 * Convert quaternion to axis-angle.
 *
 * @param q Input quaternion (should be normalized)
 * @param out_axis Output axis (normalized vector)
 * @param out_angle Output angle in radians
 */
void quaternion_to_axis_angle(const Quaternion& q, Vector3D& out_axis, float& out_angle);

// ============================================================================
// FRAME TRANSFORMATIONS
// ============================================================================

/**
 * Transform a body-frame vector to world-frame using attitude quaternion.
 *
 * The attitude quaternion represents rotation from world to body frame.
 * To transform body -> world, we use the inverse (conjugate).
 *
 * @param attitude_world_to_body Quaternion representing world -> body rotation
 * @param body_vector Vector in body frame
 * @return Vector in world frame
 */
Vector3D transform_body_to_world(const Quaternion& attitude_world_to_body,
                                  const Vector3D& body_vector);

/**
 * Transform a world-frame vector to body-frame using attitude quaternion.
 *
 * @param attitude_world_to_body Quaternion representing world -> body rotation
 * @param world_vector Vector in world frame
 * @return Vector in body frame
 */
Vector3D transform_world_to_body(const Quaternion& attitude_world_to_body,
                                  const Vector3D& world_vector);

#endif  // QUATERNION_CONVERSIONS_H
