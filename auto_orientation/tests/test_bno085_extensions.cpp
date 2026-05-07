/**
 * Unit Tests for BNO085 Extensions
 *
 * Tests the rotation matrix and Euler angle extraction methods:
 * - get_rotation_matrix(): Tests orthonormality, determinant, unit column vectors
 * - get_euler_angles(): Tests valid ranges, round-trip conversion, known rotations
 * - validate_quaternion_magnitude(): Tests quaternion validation
 *
 * Based on PHASE_1_MASTER_IMPLEMENTATION_PLAN.md Tasks 4.1-4.3
 */

#include <gtest/gtest.h>
#include <cmath>
#include "../src/math/quaternion.h"
#include "../src/math/quaternion_conversions.h"

// ============================================================================
// Test Fixtures
// ============================================================================

/**
 * Base test fixture for rotation matrix properties.
 */
class RotationMatrixTest : public ::testing::Test {
 protected:
  // Tolerance for floating-point comparisons (0.01%)
  static constexpr float TOLERANCE = 0.0001f;
  static constexpr float ANGLE_TOLERANCE_DEG = 0.01f;

  /**
   * Check if two matrices are approximately equal.
   */
  bool MatricesEqual(const RotationMatrix& a, const RotationMatrix& b) {
    for (int i = 0; i < 3; i++) {
      for (int j = 0; j < 3; j++) {
        if (std::fabs(a.m[i][j] - b.m[i][j]) > TOLERANCE) {
          return false;
        }
      }
    }
    return true;
  }

  /**
   * Check if matrix is orthonormal: R * R^T = I
   */
  void AssertOrthonormal(const RotationMatrix& R) {
    RotationMatrix RT = R.transpose();

    // Compute R * R^T
    for (int i = 0; i < 3; i++) {
      for (int j = 0; j < 3; j++) {
        float sum = 0.0f;
        for (int k = 0; k < 3; k++) {
          sum += R.m[i][k] * RT.m[k][j];
        }
        float expected = (i == j) ? 1.0f : 0.0f;
        ASSERT_LT(std::fabs(sum - expected), TOLERANCE)
            << "Orthonormality failed at (" << i << "," << j << "): "
            << sum << " != " << expected;
      }
    }
  }

  /**
   * Check if matrix columns have unit length.
   */
  void AssertUnitColumnVectors(const RotationMatrix& R) {
    for (int col = 0; col < 3; col++) {
      float mag_sq = 0.0f;
      for (int row = 0; row < 3; row++) {
        mag_sq += R.m[row][col] * R.m[row][col];
      }
      float magnitude = std::sqrt(mag_sq);
      ASSERT_LT(std::fabs(magnitude - 1.0f), TOLERANCE)
          << "Column " << col << " has magnitude " << magnitude;
    }
  }

  /**
   * Check determinant is +1 (proper rotation, not reflection).
   */
  float ComputeDeterminant(const RotationMatrix& R) {
    // det(R) = R[0,0]*(R[1,1]*R[2,2] - R[1,2]*R[2,1])
    //        - R[0,1]*(R[1,0]*R[2,2] - R[1,2]*R[2,0])
    //        + R[0,2]*(R[1,0]*R[2,1] - R[1,1]*R[2,0])
    float det = R.m[0][0] * (R.m[1][1] * R.m[2][2] - R.m[1][2] * R.m[2][1])
              - R.m[0][1] * (R.m[1][0] * R.m[2][2] - R.m[1][2] * R.m[2][0])
              + R.m[0][2] * (R.m[1][0] * R.m[2][1] - R.m[1][1] * R.m[2][0]);
    return det;
  }
};

/**
 * Test fixture for Euler angles.
 */
class EulerAnglesTest : public ::testing::Test {
 protected:
  static constexpr float TOLERANCE = 0.01f;  // 0.01 degrees

  /**
   * Check if angle is in valid range.
   */
  void AssertAngleInRange(float angle_deg, float min_deg, float max_deg) {
    ASSERT_GE(angle_deg, min_deg - TOLERANCE)
        << "Angle " << angle_deg << " less than " << min_deg;
    ASSERT_LE(angle_deg, max_deg + TOLERANCE)
        << "Angle " << angle_deg << " greater than " << max_deg;
  }

  /**
   * Check if two angles are approximately equal (handling wrap-around).
   */
  bool AnglesEqual(float a, float b) {
    float diff = std::fabs(a - b);
    // Handle wrap-around for angles (e.g., -180 and +180 are equivalent)
    if (diff > 180.0f) {
      diff = 360.0f - diff;
    }
    return diff < TOLERANCE;
  }
};

// ============================================================================
// Rotation Matrix Tests
// ============================================================================

/**
 * Task 4.1: Test rotation matrix output properties
 */

TEST_F(RotationMatrixTest, IdentityQuaternionGivesIdentityMatrix) {
  // Identity quaternion [1, 0, 0, 0] should give identity matrix
  Quaternion q(1.0f, 0.0f, 0.0f, 0.0f);
  RotationMatrix R = quaternion_to_rotation_matrix(q);

  RotationMatrix I;
  I.set_identity();

  EXPECT_TRUE(MatricesEqual(R, I));
}

TEST_F(RotationMatrixTest, MatrixIsOrthonormal) {
  // Test with various quaternions that all represent valid rotations
  Quaternion rotations[] = {
      Quaternion(1.0f, 0.0f, 0.0f, 0.0f),           // Identity
      Quaternion(0.7071f, 0.7071f, 0.0f, 0.0f),     // 90° around X
      Quaternion(0.7071f, 0.0f, 0.7071f, 0.0f),     // 90° around Y
      Quaternion(0.7071f, 0.0f, 0.0f, 0.7071f),     // 90° around Z
  };

  for (const auto& q : rotations) {
    RotationMatrix R = quaternion_to_rotation_matrix(q);
    AssertOrthonormal(R);
  }
}

TEST_F(RotationMatrixTest, DeterminantIsPositiveOne) {
  // All rotation matrices should have det(R) = +1 (proper rotation)
  Quaternion rotations[] = {
      Quaternion(0.7071f, 0.7071f, 0.0f, 0.0f),
      Quaternion(0.7071f, 0.0f, 0.7071f, 0.0f),
      Quaternion(0.7071f, 0.0f, 0.0f, 0.7071f),
      Quaternion(0.5f, 0.5f, 0.5f, 0.5f),
  };

  for (const auto& q : rotations) {
    RotationMatrix R = quaternion_to_rotation_matrix(q);
    float det = ComputeDeterminant(R);
    EXPECT_GT(det, 0.99f) << "Determinant should be +1 for proper rotation";
    EXPECT_LT(det, 1.01f) << "Determinant should be +1 for proper rotation";
  }
}

TEST_F(RotationMatrixTest, ColumnVectorsHaveUnitLength) {
  // Each column of a rotation matrix should be a unit vector
  Quaternion q(0.5f, 0.5f, 0.5f, 0.5f);
  RotationMatrix R = quaternion_to_rotation_matrix(q);

  AssertUnitColumnVectors(R);
}

TEST_F(RotationMatrixTest, RowVectorsHaveUnitLength) {
  // Each row of a rotation matrix should also be a unit vector
  Quaternion q(0.5f, 0.5f, 0.5f, 0.5f);
  RotationMatrix R = quaternion_to_rotation_matrix(q);

  for (int row = 0; row < 3; row++) {
    float mag_sq = 0.0f;
    for (int col = 0; col < 3; col++) {
      mag_sq += R.m[row][col] * R.m[row][col];
    }
    float magnitude = std::sqrt(mag_sq);
    EXPECT_LT(std::fabs(magnitude - 1.0f), TOLERANCE);
  }
}

TEST_F(RotationMatrixTest, TransposeIsInverse) {
  // For rotation matrices: R^T = R^-1, so R * R^T = I
  Quaternion q(0.7071f, 0.7071f, 0.0f, 0.0f);
  RotationMatrix R = quaternion_to_rotation_matrix(q);
  RotationMatrix RT = R.transpose();

  // Compute R * R^T (should be identity)
  RotationMatrix product;
  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 3; j++) {
      float sum = 0.0f;
      for (int k = 0; k < 3; k++) {
        sum += R.m[i][k] * RT.m[k][j];
      }
      product.m[i][j] = sum;
    }
  }

  RotationMatrix I;
  I.set_identity();
  EXPECT_TRUE(MatricesEqual(product, I));
}

// ============================================================================
// Euler Angles Tests
// ============================================================================

/**
 * Task 4.2: Test Euler angles from quaternion
 */

TEST_F(EulerAnglesTest, AnglesInValidRange) {
  // Test that Euler angles are in valid ranges
  Quaternion rotations[] = {
      Quaternion(1.0f, 0.0f, 0.0f, 0.0f),
      Quaternion(0.7071f, 0.7071f, 0.0f, 0.0f),
      Quaternion(0.5f, 0.5f, 0.5f, 0.5f),
      Quaternion(0.707f, 0.0f, 0.707f, 0.0f),
  };

  for (const auto& q : rotations) {
    EulerAngles euler = quaternion_to_euler_degrees(q);

    // Valid ranges:
    // Roll: -180 to +180
    // Pitch: -90 to +90
    // Yaw: -180 to +180
    AssertAngleInRange(euler.roll_deg, -180.0f, 180.0f);
    AssertAngleInRange(euler.pitch_deg, -90.0f, 90.0f);
    AssertAngleInRange(euler.yaw_deg, -180.0f, 180.0f);
  }
}

TEST_F(EulerAnglesTest, RoundTripConversion) {
  // Test: euler -> quaternion -> euler gives same result
  EulerAngles original_angles[] = {
      EulerAngles(0.0f, 0.0f, 0.0f),
      EulerAngles(30.0f, 20.0f, 45.0f),
      EulerAngles(-30.0f, 30.0f, -45.0f),
      EulerAngles(90.0f, 0.0f, 0.0f),
      EulerAngles(0.0f, 45.0f, 0.0f),
      EulerAngles(0.0f, 0.0f, 90.0f),
  };

  for (const auto& original : original_angles) {
    // Convert degrees to radians
    Quaternion q = euler_to_quaternion_degrees(
        original.roll_deg, original.pitch_deg, original.yaw_deg);

    // Convert back
    EulerAngles result = quaternion_to_euler_degrees(q);

    // Check if they match (allowing for gimbal lock near ±90° pitch)
    bool near_gimbal_lock = std::fabs(original.pitch_deg) > 85.0f;

    if (!near_gimbal_lock) {
      EXPECT_TRUE(AnglesEqual(result.roll_deg, original.roll_deg))
          << "Roll mismatch: " << result.roll_deg << " vs " << original.roll_deg;
      EXPECT_TRUE(AnglesEqual(result.yaw_deg, original.yaw_deg))
          << "Yaw mismatch: " << result.yaw_deg << " vs " << original.yaw_deg;
    }

    EXPECT_TRUE(AnglesEqual(result.pitch_deg, original.pitch_deg))
        << "Pitch mismatch: " << result.pitch_deg << " vs " << original.pitch_deg;
  }
}

/**
 * Task 4.2: Test known rotations
 */

TEST_F(EulerAnglesTest, KnownRotations) {
  // 90° around X axis: roll ≈ 90°
  {
    Quaternion q = euler_to_quaternion_degrees(90.0f, 0.0f, 0.0f);
    EulerAngles euler = quaternion_to_euler_degrees(q);
    EXPECT_TRUE(AnglesEqual(euler.roll_deg, 90.0f));
  }

  // 90° around Y axis: pitch ≈ 90°
  {
    Quaternion q = euler_to_quaternion_degrees(0.0f, 90.0f, 0.0f);
    EulerAngles euler = quaternion_to_euler_degrees(q);
    // At pitch = 90°, gimbal lock occurs, so roll/yaw become indeterminate
    EXPECT_TRUE(AnglesEqual(euler.pitch_deg, 90.0f));
  }

  // 90° around Z axis: yaw ≈ 90°
  {
    Quaternion q = euler_to_quaternion_degrees(0.0f, 0.0f, 90.0f);
    EulerAngles euler = quaternion_to_euler_degrees(q);
    EXPECT_TRUE(AnglesEqual(euler.yaw_deg, 90.0f));
  }
}

TEST_F(EulerAnglesTest, SmallAngleAccuracy) {
  // Test accuracy for small angles
  EulerAngles small_angles[] = {
      EulerAngles(0.1f, 0.0f, 0.0f),
      EulerAngles(0.0f, 0.1f, 0.0f),
      EulerAngles(0.0f, 0.0f, 0.1f),
      EulerAngles(1.0f, 1.0f, 1.0f),
  };

  for (const auto& original : small_angles) {
    Quaternion q = euler_to_quaternion_degrees(
        original.roll_deg, original.pitch_deg, original.yaw_deg);
    EulerAngles result = quaternion_to_euler_degrees(q);

    EXPECT_TRUE(AnglesEqual(result.roll_deg, original.roll_deg));
    EXPECT_TRUE(AnglesEqual(result.pitch_deg, original.pitch_deg));
    EXPECT_TRUE(AnglesEqual(result.yaw_deg, original.yaw_deg));
  }
}

// ============================================================================
// Round-Trip Conversion Tests
// ============================================================================

/**
 * Test: quaternion -> matrix -> quaternion
 */
TEST_F(RotationMatrixTest, QuaternionMatrixRoundTrip) {
  Quaternion original[] = {
      Quaternion(1.0f, 0.0f, 0.0f, 0.0f),
      Quaternion(0.7071f, 0.7071f, 0.0f, 0.0f),
      Quaternion(0.5f, 0.5f, 0.5f, 0.5f),
  };

  for (const auto& q : original) {
    // q -> matrix
    RotationMatrix R = quaternion_to_rotation_matrix(q);

    // matrix -> q
    Quaternion recovered = rotation_matrix_to_quaternion(R);

    // Check if quaternions represent the same rotation
    // (accounting for antipodal equivalence: q and -q are equivalent)
    float dot = q.dot(recovered);
    float magnitude_product = q.magnitude() * recovered.magnitude();
    float cos_half_angle = std::fabs(dot) / magnitude_product;

    // Should be very close to 1.0 if they represent the same rotation
    EXPECT_GT(cos_half_angle, 0.99f)
        << "Quaternion round-trip conversion failed. Cos(half_angle) = "
        << cos_half_angle;
  }
}

/**
 * Test: quaternion -> euler -> quaternion -> euler
 */
TEST_F(EulerAnglesTest, QuaternionEulerRoundTrip) {
  EulerAngles angles_deg[] = {
      EulerAngles(0.0f, 0.0f, 0.0f),
      EulerAngles(45.0f, 30.0f, 60.0f),
      EulerAngles(-45.0f, -30.0f, -60.0f),
      EulerAngles(90.0f, 0.0f, 0.0f),
      EulerAngles(0.0f, 0.0f, 90.0f),
  };

  for (const auto& original : angles_deg) {
    // euler -> q
    Quaternion q = euler_to_quaternion_degrees(
        original.roll_deg, original.pitch_deg, original.yaw_deg);

    // q -> euler
    EulerAngles result = quaternion_to_euler_degrees(q);

    // Check accuracy (except near gimbal lock)
    bool near_gimbal_lock = std::fabs(original.pitch_deg) > 85.0f;

    if (!near_gimbal_lock) {
      EXPECT_TRUE(AnglesEqual(result.roll_deg, original.roll_deg));
      EXPECT_TRUE(AnglesEqual(result.yaw_deg, original.yaw_deg));
    }
    EXPECT_TRUE(AnglesEqual(result.pitch_deg, original.pitch_deg));
  }
}

// ============================================================================
// Edge Cases and Gimbal Lock
// ============================================================================

TEST_F(EulerAnglesTest, GimbalLockAtPitch90) {
  // At pitch = ±90°, roll and yaw become indeterminate
  // But we should still get valid output (no NaN, no inf)
  Quaternion q = euler_to_quaternion_degrees(0.0f, 90.0f, 0.0f);
  EulerAngles euler = quaternion_to_euler_degrees(q);

  EXPECT_FALSE(std::isnan(euler.roll_deg));
  EXPECT_FALSE(std::isnan(euler.pitch_deg));
  EXPECT_FALSE(std::isnan(euler.yaw_deg));

  EXPECT_FALSE(std::isinf(euler.roll_deg));
  EXPECT_FALSE(std::isinf(euler.pitch_deg));
  EXPECT_FALSE(std::isinf(euler.yaw_deg));
}

TEST_F(EulerAnglesTest, GimbalLockAtPitchMinus90) {
  // At pitch = -90°, roll and yaw also become indeterminate
  Quaternion q = euler_to_quaternion_degrees(0.0f, -90.0f, 0.0f);
  EulerAngles euler = quaternion_to_euler_degrees(q);

  EXPECT_FALSE(std::isnan(euler.roll_deg));
  EXPECT_FALSE(std::isnan(euler.pitch_deg));
  EXPECT_FALSE(std::isnan(euler.yaw_deg));

  EXPECT_FALSE(std::isinf(euler.roll_deg));
  EXPECT_FALSE(std::isinf(euler.pitch_deg));
  EXPECT_FALSE(std::isinf(euler.yaw_deg));
}

// ============================================================================
// Quaternion Magnitude Validation
// ============================================================================

TEST_F(RotationMatrixTest, ValidQuaternionMagnitude) {
  // Valid rotation quaternions should have magnitude ≈ 1.0
  Quaternion q[] = {
      Quaternion(1.0f, 0.0f, 0.0f, 0.0f),
      Quaternion(0.7071f, 0.7071f, 0.0f, 0.0f),
      Quaternion(0.5f, 0.5f, 0.5f, 0.5f),
  };

  for (const auto& quat : q) {
    float mag = quat.magnitude();
    EXPECT_GT(mag, 0.99f) << "Quaternion magnitude too low: " << mag;
    EXPECT_LT(mag, 1.01f) << "Quaternion magnitude too high: " << mag;
  }
}

TEST_F(RotationMatrixTest, NormalizedQuaternion) {
  // Unnormalized quaternion should have different magnitude
  Quaternion unnormalized(2.0f, 0.0f, 0.0f, 0.0f);
  EXPECT_GT(unnormalized.magnitude(), 1.5f);

  // After normalization, should be ≈ 1.0
  Quaternion normalized = unnormalized.normalized();
  EXPECT_LT(std::fabs(normalized.magnitude() - 1.0f), TOLERANCE);
}

// ============================================================================
// Integration: Matrix and Euler from Same Quaternion
// ============================================================================

TEST_F(RotationMatrixTest, MatrixAndEulerConsistent) {
  // Matrix and Euler angles from same quaternion should be consistent
  Quaternion q(0.6f, 0.4f, 0.5f, 0.35f);
  q.normalize();

  RotationMatrix R = quaternion_to_rotation_matrix(q);
  EulerAngles euler = quaternion_to_euler_degrees(q);

  // Verify the matrix represents the same rotation as the Euler angles
  // by checking that rotating unit vectors gives expected results

  // Create a rotation matrix from Euler angles separately
  // And verify they give the same result
  Quaternion q_from_euler = euler_to_quaternion_degrees(
      euler.roll_deg, euler.pitch_deg, euler.yaw_deg);
  RotationMatrix R_from_euler = quaternion_to_rotation_matrix(q_from_euler);

  // The two matrices should be very similar
  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 3; j++) {
      EXPECT_LT(std::fabs(R.m[i][j] - R_from_euler.m[i][j]), 0.001f);
    }
  }
}

#endif  // TEST_BNO085_EXTENSIONS_H
