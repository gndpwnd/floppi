#ifndef QUATERNION_H
#define QUATERNION_H

#include <cmath>
#include <cstdint>

/**
 * Quaternion Class for 3D Rotation Representation
 *
 * A quaternion represents a 3D rotation without gimbal lock.
 * Form: q = [w, x, y, z] where w is the scalar part and (x,y,z) is the vector part.
 *
 * For valid rotations, the quaternion must be normalized (unit magnitude):
 * w² + x² + y² + z² = 1.0
 *
 * Based on the QUATERNION_REFERENCE.md and QUATERNION_IMPLEMENTATION_GUIDE.md.
 */

struct Quaternion {
    float w;  // Scalar (real) part
    float x;  // Vector part X
    float y;  // Vector part Y
    float z;  // Vector part Z

    // Default constructor: identity rotation (no rotation)
    Quaternion() : w(1.0f), x(0.0f), y(0.0f), z(0.0f) {}

    /**
     * Constructor from components.
     * @param w_ Scalar part
     * @param x_ Vector part X
     * @param y_ Vector part Y
     * @param z_ Vector part Z
     */
    Quaternion(float w_, float x_, float y_, float z_)
        : w(w_), x(x_), y(y_), z(z_) {}

    /**
     * Constructor from array [w, x, y, z].
     * @param components Array of 4 floats [w, x, y, z]
     */
    Quaternion(const float components[4])
        : w(components[0]), x(components[1]), y(components[2]), z(components[3]) {}

    /**
     * Get the magnitude (length) of the quaternion.
     * For a valid rotation quaternion, this should be approximately 1.0.
     * @return Magnitude: sqrt(w² + x² + y² + z²)
     */
    float magnitude() const;

    /**
     * Get the squared magnitude of the quaternion.
     * More efficient when you only need to compare magnitudes.
     * @return Squared magnitude: w² + x² + y² + z²
     */
    float magnitude_squared() const;

    /**
     * Check if this quaternion is normalized (unit magnitude).
     * @param tolerance Maximum acceptable deviation from 1.0 (default 0.01)
     * @return True if |magnitude - 1.0| < tolerance
     */
    bool is_normalized(float tolerance = 0.01f) const;

    /**
     * Check if this quaternion is valid (not zero, not NaN).
     * @return True if magnitude > 1e-6
     */
    bool is_valid() const;

    /**
     * Normalize this quaternion in-place (convert to unit quaternion).
     * If magnitude is zero, resets to identity [1, 0, 0, 0].
     * @return Reference to this quaternion (for chaining)
     */
    Quaternion& normalize();

    /**
     * Get a normalized copy of this quaternion.
     * Does not modify the original.
     * @return New normalized quaternion
     */
    Quaternion normalized() const;

    /**
     * Get the conjugate (inverse for unit quaternions).
     * Conjugate: q* = [w, -x, -y, -z]
     * Property: q * q* = [1, 0, 0, 0] (identity)
     * @return Conjugate quaternion
     */
    Quaternion conjugate() const;

    /**
     * Compute the conjugate in-place.
     * @return Reference to this quaternion
     */
    Quaternion& conjugate_in_place();

    /**
     * Compute dot product with another quaternion.
     * Used for measuring similarity and detecting antipodal equivalence.
     * @param other The other quaternion
     * @return w1*w2 + x1*x2 + y1*y2 + z1*z2
     */
    float dot(const Quaternion& other) const;

    /**
     * Multiply this quaternion by another (composition of rotations).
     * Result = this * other
     * Represents: first apply 'other' rotation, then apply 'this' rotation.
     * Note: quaternion multiplication is NOT commutative (order matters).
     * @param other The quaternion to multiply by
     * @return New quaternion representing composed rotation
     */
    Quaternion multiply(const Quaternion& other) const;

    /**
     * Multiply in-place: this = this * other.
     * @param other The quaternion to multiply by
     * @return Reference to this quaternion
     */
    Quaternion& multiply_in_place(const Quaternion& other);

    /**
     * Serialize to array [w, x, y, z].
     * @param out Array to receive 4 floats (must have length >= 4)
     */
    void to_array(float out[4]) const;

    /**
     * Load from array [w, x, y, z].
     * @param components Array of 4 floats
     * @return Reference to this quaternion
     */
    Quaternion& from_array(const float components[4]);
};

#endif  // QUATERNION_H
