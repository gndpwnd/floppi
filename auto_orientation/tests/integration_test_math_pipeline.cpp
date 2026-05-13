/**
 * Integration Test: Full Math Pipeline
 *
 * End-to-end test of the math pipeline:
 * - Read/simulate BNO085 quaternion
 * - Compute rotation matrix via quaternion_to_matrix()
 * - Compute Euler angles via quaternion_to_euler()
 * - Verify all three representations consistent
 * - Test 20+ different orientations
 * - Validate accuracy to ±0.01°
 *
 * Based on PHASE_1_MASTER_IMPLEMENTATION_PLAN.md Task 6.1
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include "../src/math/quaternion.h"
#include "../src/math/quaternion_conversions.h"

// ============================================================================
// Test Fixture
// ============================================================================

class MathPipelineIntegrationTest : public ::testing::Test {
 protected:
  static constexpr float TOLERANCE = 0.0001f;      // 0.01% for matrix/vector comparisons
  static constexpr float ANGLE_TOLERANCE = 0.01f;  // 0.01° for angle comparisons
  static constexpr int NUM_TEST_VECTORS = 6;       // Number of test vectors for each rotation

  /**
   * Verify that matrix and Euler angles represent the same rotation
   * by rotating test vectors and comparing results.
   */
  void VerifyConsistency(const RotationMatrix& R, const EulerAngles& euler,
                         const Quaternion& q) {
    // Create rotation matrices from all three representations
    RotationMatrix R_from_euler = quaternion_to_rotation_matrix(
        euler_to_quaternion_degrees(euler.roll, euler.pitch, euler.yaw)
    );

    // Test with unit vectors along each axis
    Vector3D test_vectors[] = {
        Vector3D(1.0f, 0.0f, 0.0f),  // X-axis
        Vector3D(0.0f, 1.0f, 0.0f),  // Y-axis
        Vector3D(0.0f, 0.0f, 1.0f),  // Z-axis
        Vector3D(1.0f, 1.0f, 0.0f),  // XY diagonal
        Vector3D(1.0f, 0.0f, 1.0f),  // XZ diagonal
        Vector3D(0.0f, 1.0f, 1.0f),  // YZ diagonal
    };

    for (const auto& v : test_vectors) {
      // Rotate using matrix
      Vector3D v_rotated_matrix = R.rotate_vector(v);

      // Rotate using quaternion
      Vector3D v_rotated_q = rotate_vector_by_quaternion(q, v);

      // Rotate using Euler-derived matrix
      Vector3D v_rotated_euler = R_from_euler.rotate_vector(v);

      // All three should give the same result (within tolerance)
      EXPECT_LT(std::fabs(v_rotated_matrix.x - v_rotated_q.x), TOLERANCE);
      EXPECT_LT(std::fabs(v_rotated_matrix.y - v_rotated_q.y), TOLERANCE);
      EXPECT_LT(std::fabs(v_rotated_matrix.z - v_rotated_q.z), TOLERANCE);

      EXPECT_LT(std::fabs(v_rotated_matrix.x - v_rotated_euler.x), TOLERANCE);
      EXPECT_LT(std::fabs(v_rotated_matrix.y - v_rotated_euler.y), TOLERANCE);
      EXPECT_LT(std::fabs(v_rotated_matrix.z - v_rotated_euler.z), TOLERANCE);
    }
  }

  /**
   * Check if quaternion magnitude is valid.
   */
  bool IsValidQuaternion(const Quaternion& q) {
    float mag = q.magnitude();
    return mag > 0.95f && mag < 1.05f;
  }

  /**
   * Check if Euler angles are in valid ranges.
   */
  bool IsValidEulerAngles(const EulerAngles& e) {
    return e.roll >= -180.0f && e.roll <= 180.0f &&
           e.pitch >= -90.0f && e.pitch <= 90.0f &&
           e.yaw >= -180.0f && e.yaw <= 180.0f;
  }
};

// ============================================================================
// Main Integration Tests
// ============================================================================

/**
 * Test 1: Basic rotations (0°, 90°, 180°, 270°) around each axis
 */
TEST_F(MathPipelineIntegrationTest, BasicRotationsAroundXAxis) {
  float roll_angles[] = {0.0f, 45.0f, 90.0f, 135.0f, 180.0f, -45.0f, -90.0f, -135.0f};

  for (float roll : roll_angles) {
    // Simulate BNO085 output: create quaternion from Euler angles
    Quaternion q = euler_to_quaternion_degrees(roll, 0.0f, 0.0f);
    EXPECT_TRUE(IsValidQuaternion(q)) << "Invalid quaternion at roll=" << roll;

    // Extract rotation matrix
    RotationMatrix R = quaternion_to_rotation_matrix(q);
    EXPECT_TRUE(R.is_orthonormal()) << "Matrix not orthonormal at roll=" << roll;

    // Extract Euler angles
    EulerAngles euler = quaternion_to_euler_degrees(q);
    EXPECT_TRUE(IsValidEulerAngles(euler)) << "Invalid Euler angles at roll=" << roll;

    // Verify roll is approximately correct
    EXPECT_NEAR(euler.roll, roll, ANGLE_TOLERANCE)
        << "Roll mismatch at " << roll << "°";

    // Verify consistency
    VerifyConsistency(R, euler, q);
  }
}

TEST_F(MathPipelineIntegrationTest, BasicRotationsAroundYAxis) {
  float pitch_angles[] = {0.0f, 30.0f, 45.0f, 60.0f, -30.0f, -45.0f, -60.0f};

  for (float pitch : pitch_angles) {
    Quaternion q = euler_to_quaternion_degrees(0.0f, pitch, 0.0f);
    EXPECT_TRUE(IsValidQuaternion(q)) << "Invalid quaternion at pitch=" << pitch;

    RotationMatrix R = quaternion_to_rotation_matrix(q);
    EXPECT_TRUE(R.is_orthonormal()) << "Matrix not orthonormal at pitch=" << pitch;

    EulerAngles euler = quaternion_to_euler_degrees(q);
    EXPECT_TRUE(IsValidEulerAngles(euler)) << "Invalid Euler angles at pitch=" << pitch;

    // Verify pitch is approximately correct
    EXPECT_NEAR(euler.pitch, pitch, ANGLE_TOLERANCE)
        << "Pitch mismatch at " << pitch << "°";

    VerifyConsistency(R, euler, q);
  }
}

TEST_F(MathPipelineIntegrationTest, BasicRotationsAroundZAxis) {
  float yaw_angles[] = {0.0f, 45.0f, 90.0f, 135.0f, 180.0f, -45.0f, -90.0f, -135.0f};

  for (float yaw : yaw_angles) {
    Quaternion q = euler_to_quaternion_degrees(0.0f, 0.0f, yaw);
    EXPECT_TRUE(IsValidQuaternion(q)) << "Invalid quaternion at yaw=" << yaw;

    RotationMatrix R = quaternion_to_rotation_matrix(q);
    EXPECT_TRUE(R.is_orthonormal()) << "Matrix not orthonormal at yaw=" << yaw;

    EulerAngles euler = quaternion_to_euler_degrees(q);
    EXPECT_TRUE(IsValidEulerAngles(euler)) << "Invalid Euler angles at yaw=" << yaw;

    // Verify yaw is approximately correct (handling wrap-around)
    float yaw_diff = std::fabs(euler.yaw - yaw);
    if (yaw_diff > 180.0f) {
      yaw_diff = 360.0f - yaw_diff;
    }
    EXPECT_LT(yaw_diff, ANGLE_TOLERANCE)
        << "Yaw mismatch at " << yaw << "°, got " << euler.yaw;

    VerifyConsistency(R, euler, q);
  }
}

/**
 * Test 2: Combined rotations (all three axes)
 */
TEST_F(MathPipelineIntegrationTest, CombinedRotations) {
  struct TestCase {
    float roll;
    float pitch;
    float yaw;
  };

  TestCase test_cases[] = {
      {0.0f, 0.0f, 0.0f},        // Identity
      {30.0f, 30.0f, 30.0f},     // Symmetric
      {45.0f, -30.0f, 60.0f},    // Asymmetric
      {-20.0f, 35.0f, -50.0f},   // Various signs
      {60.0f, -45.0f, 75.0f},    // Larger angles
      {15.0f, 25.0f, 35.0f},     // Sequential increases
      {-15.0f, -25.0f, -35.0f},  // Sequential decreases
  };

  for (const auto& tc : test_cases) {
    Quaternion q = euler_to_quaternion_degrees(tc.roll, tc.pitch, tc.yaw);
    EXPECT_TRUE(IsValidQuaternion(q))
        << "Invalid quaternion at roll=" << tc.roll << ", pitch=" << tc.pitch
        << ", yaw=" << tc.yaw;

    RotationMatrix R = quaternion_to_rotation_matrix(q);
    EXPECT_TRUE(R.is_orthonormal())
        << "Matrix not orthonormal at roll=" << tc.roll << ", pitch=" << tc.pitch
        << ", yaw=" << tc.yaw;

    EulerAngles euler = quaternion_to_euler_degrees(q);
    EXPECT_TRUE(IsValidEulerAngles(euler))
        << "Invalid Euler angles at roll=" << tc.roll << ", pitch=" << tc.pitch
        << ", yaw=" << tc.yaw;

    // Verify roundtrip accuracy
    bool near_gimbal_lock = std::fabs(tc.pitch) > 85.0f;
    if (!near_gimbal_lock) {
      EXPECT_NEAR(euler.roll, tc.roll, ANGLE_TOLERANCE);
      EXPECT_NEAR(euler.yaw, tc.yaw, ANGLE_TOLERANCE);
    }
    EXPECT_NEAR(euler.pitch, tc.pitch, ANGLE_TOLERANCE);

    VerifyConsistency(R, euler, q);
  }
}

/**
 * Test 3: Random orientations (20+ cases)
 */
TEST_F(MathPipelineIntegrationTest, RandomOrientations) {
  srand(12345);  // Fixed seed for reproducibility

  for (int i = 0; i < 25; i++) {
    // Generate random angles
    float roll = (rand() % 360) - 180.0f;
    float pitch = (rand() % 180) - 90.0f;
    float yaw = (rand() % 360) - 180.0f;

    Quaternion q = euler_to_quaternion_degrees(roll, pitch, yaw);
    EXPECT_TRUE(IsValidQuaternion(q));

    RotationMatrix R = quaternion_to_rotation_matrix(q);
    EXPECT_TRUE(R.is_orthonormal());

    EulerAngles euler = quaternion_to_euler_degrees(q);
    EXPECT_TRUE(IsValidEulerAngles(euler));

    bool near_gimbal_lock = std::fabs(pitch) > 85.0f;
    if (!near_gimbal_lock) {
      // Allow slightly more tolerance for random orientations
      EXPECT_NEAR(euler.roll, roll, ANGLE_TOLERANCE * 10.0f);
      EXPECT_NEAR(euler.yaw, yaw, ANGLE_TOLERANCE * 10.0f);
    }

    VerifyConsistency(R, euler, q);
  }
}

/**
 * Test 4: Edge cases near gimbal lock
 */
TEST_F(MathPipelineIntegrationTest, NearGimbalLock) {
  float pitch_near_lock[] = {
      85.0f, 87.0f, 89.0f, 89.9f,   // Near +90°
      -85.0f, -87.0f, -89.0f, -89.9f  // Near -90°
  };

  for (float pitch : pitch_near_lock) {
    Quaternion q = euler_to_quaternion_degrees(30.0f, pitch, 45.0f);
    EXPECT_TRUE(IsValidQuaternion(q));

    RotationMatrix R = quaternion_to_rotation_matrix(q);
    EXPECT_TRUE(R.is_orthonormal());

    EulerAngles euler = quaternion_to_euler_degrees(q);
    EXPECT_TRUE(IsValidEulerAngles(euler));

    // Pitch should still be accurate
    EXPECT_NEAR(euler.pitch, pitch, ANGLE_TOLERANCE);

    // Consistency should still hold (even though roll/yaw are indeterminate)
    VerifyConsistency(R, euler, q);
  }
}

/**
 * Test 5: Very small angles (accuracy at small scales)
 */
TEST_F(MathPipelineIntegrationTest, SmallAngles) {
  float small_angles[] = {0.1f, 0.5f, 1.0f, 2.0f, 5.0f};

  for (float angle : small_angles) {
    Quaternion q = euler_to_quaternion_degrees(angle, angle, angle);
    EXPECT_TRUE(IsValidQuaternion(q));

    EulerAngles euler = quaternion_to_euler_degrees(q);
    EXPECT_TRUE(IsValidEulerAngles(euler));

    EXPECT_NEAR(euler.roll, angle, ANGLE_TOLERANCE);
    EXPECT_NEAR(euler.pitch, angle, ANGLE_TOLERANCE);
    EXPECT_NEAR(euler.yaw, angle, ANGLE_TOLERANCE);
  }
}

/**
 * Test 6: Large angles (full rotations)
 */
TEST_F(MathPipelineIntegrationTest, LargeAngles) {
  Quaternion q = euler_to_quaternion_degrees(180.0f, 0.0f, 0.0f);
  EXPECT_TRUE(IsValidQuaternion(q));

  RotationMatrix R = quaternion_to_rotation_matrix(q);
  EXPECT_TRUE(R.is_orthonormal());

  EulerAngles euler = quaternion_to_euler_degrees(q);
  EXPECT_TRUE(IsValidEulerAngles(euler));

  // 180° roll should be preserved or wrapped
  EXPECT_TRUE(std::fabs(std::fabs(euler.roll) - 180.0f) < ANGLE_TOLERANCE);
}

// ============================================================================
// Matrix Properties Validation
// ============================================================================

TEST_F(MathPipelineIntegrationTest, MatrixOrthonormality) {
  // Every quaternion should produce an orthonormal matrix
  for (int i = 0; i < 20; i++) {
    float r = (i * 18.0f);
    float p = (i * 9.0f);
    float y = (i * 18.0f);

    Quaternion q = euler_to_quaternion_degrees(r, p, y);
    RotationMatrix R = quaternion_to_rotation_matrix(q);

    EXPECT_TRUE(R.is_orthonormal())
        << "Matrix not orthonormal at iteration " << i;
  }
}

TEST_F(MathPipelineIntegrationTest, MatrixDeterminant) {
  // Every rotation matrix should have determinant ≈ +1
  for (int i = 0; i < 20; i++) {
    float r = (i * 18.0f);
    float p = (i * 9.0f);
    float y = (i * 18.0f);

    Quaternion q = euler_to_quaternion_degrees(r, p, y);
    RotationMatrix R = quaternion_to_rotation_matrix(q);

    // Compute determinant
    float det = R.m[0][0] * (R.m[1][1] * R.m[2][2] - R.m[1][2] * R.m[2][1])
              - R.m[0][1] * (R.m[1][0] * R.m[2][2] - R.m[1][2] * R.m[2][0])
              + R.m[0][2] * (R.m[1][0] * R.m[2][1] - R.m[1][1] * R.m[2][0]);

    EXPECT_GT(det, 0.99f) << "Determinant too low at iteration " << i;
    EXPECT_LT(det, 1.01f) << "Determinant too high at iteration " << i;
  }
}

// ============================================================================
// Vector Rotation Consistency
// ============================================================================

TEST_F(MathPipelineIntegrationTest, VectorRotationConsistency) {
  // Verify that rotating vectors via matrix and quaternion give same results
  Quaternion q = euler_to_quaternion_degrees(30.0f, 45.0f, 60.0f);
  RotationMatrix R = quaternion_to_rotation_matrix(q);

  Vector3D test_vector(1.0f, 2.0f, 3.0f);

  Vector3D rotated_by_matrix = R.rotate_vector(test_vector);
  Vector3D rotated_by_quaternion = rotate_vector_by_quaternion(q, test_vector);

  EXPECT_NEAR(rotated_by_matrix.x, rotated_by_quaternion.x, TOLERANCE);
  EXPECT_NEAR(rotated_by_matrix.y, rotated_by_quaternion.y, TOLERANCE);
  EXPECT_NEAR(rotated_by_matrix.z, rotated_by_quaternion.z, TOLERANCE);
}

// ============================================================================
// Quaternion Magnitude Validation Throughout Pipeline
// ============================================================================

TEST_F(MathPipelineIntegrationTest, QuaternionMagnitudePreserved) {
  // Starting with normalized quaternion, all conversions should preserve magnitude
  for (int i = 0; i < 15; i++) {
    float r = (i * 24.0f);
    float p = (i * 12.0f);
    float y = (i * 24.0f);

    Quaternion q = euler_to_quaternion_degrees(r, p, y);

    float mag = q.magnitude();
    EXPECT_GT(mag, 0.99f) << "Quaternion magnitude too low at iteration " << i;
    EXPECT_LT(mag, 1.01f) << "Quaternion magnitude too high at iteration " << i;
  }
}

#endif  // INTEGRATION_TEST_MATH_PIPELINE_H
