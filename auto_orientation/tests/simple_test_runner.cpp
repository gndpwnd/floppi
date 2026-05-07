/**
 * Simple Test Runner - No Dependencies
 *
 * Runs basic tests for BNO085 extensions without requiring gtest.
 * This allows validation of the math pipeline on systems without gtest installed.
 */

#include <iostream>
#include <iomanip>
#include <cmath>
#include <vector>
#include <string>
#include "../src/math/quaternion.h"
#include "../src/math/quaternion_conversions.h"

// ============================================================================
// Test Framework
// ============================================================================

struct TestResult {
  std::string name;
  bool passed;
  std::string message;
};

std::vector<TestResult> test_results;
int test_count = 0;
int pass_count = 0;

void test_assert(bool condition, const std::string& test_name, const std::string& message = "") {
  test_count++;
  if (condition) {
    pass_count++;
    test_results.push_back({test_name, true, ""});
    std::cout << "✓ " << test_name << std::endl;
  } else {
    test_results.push_back({test_name, false, message});
    std::cout << "✗ " << test_name << std::endl;
    if (!message.empty()) {
      std::cout << "  Reason: " << message << std::endl;
    }
  }
}

void print_summary() {
  std::cout << "\n" << std::string(70, '=') << std::endl;
  std::cout << "TEST SUMMARY" << std::endl;
  std::cout << std::string(70, '=') << std::endl;
  std::cout << "Passed: " << pass_count << "/" << test_count << std::endl;
  if (pass_count == test_count) {
    std::cout << "✓ All tests passed!" << std::endl;
  } else {
    std::cout << "✗ " << (test_count - pass_count) << " test(s) failed" << std::endl;
  }
  std::cout << std::string(70, '=') << std::endl << std::endl;
}

// ============================================================================
// Test Cases
// ============================================================================

void test_identity_quaternion() {
  std::cout << "\n>>> Testing Identity Quaternion <<<" << std::endl;

  Quaternion q(1.0f, 0.0f, 0.0f, 0.0f);
  RotationMatrix R = quaternion_to_rotation_matrix(q);

  // Check identity matrix
  bool is_identity = true;
  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 3; j++) {
      float expected = (i == j) ? 1.0f : 0.0f;
      if (std::fabs(R.m[i][j] - expected) > 0.0001f) {
        is_identity = false;
      }
    }
  }

  test_assert(is_identity, "Identity quaternion → identity matrix");
}

void test_orthonormality() {
  std::cout << "\n>>> Testing Orthonormality <<<" << std::endl;

  Quaternion rotations[] = {
      Quaternion(0.7071f, 0.7071f, 0.0f, 0.0f),
      Quaternion(0.7071f, 0.0f, 0.7071f, 0.0f),
      Quaternion(0.7071f, 0.0f, 0.0f, 0.7071f),
      Quaternion(0.5f, 0.5f, 0.5f, 0.5f),
  };

  for (int idx = 0; idx < 4; idx++) {
    Quaternion q = rotations[idx];
    RotationMatrix R = quaternion_to_rotation_matrix(q);

    // Check R * R^T = I
    RotationMatrix RT = R.transpose();

    bool is_orthonormal = true;
    for (int i = 0; i < 3; i++) {
      for (int j = 0; j < 3; j++) {
        float sum = 0.0f;
        for (int k = 0; k < 3; k++) {
          sum += R.m[i][k] * RT.m[k][j];
        }
        float expected = (i == j) ? 1.0f : 0.0f;
        if (std::fabs(sum - expected) > 0.001f) {
          is_orthonormal = false;
        }
      }
    }

    test_assert(is_orthonormal, "Orthonormality for quaternion " + std::to_string(idx));
  }
}

void test_euler_angles_valid_range() {
  std::cout << "\n>>> Testing Euler Angles Valid Range <<<" << std::endl;

  Quaternion rotations[] = {
      Quaternion(1.0f, 0.0f, 0.0f, 0.0f),
      Quaternion(0.7071f, 0.7071f, 0.0f, 0.0f),
      Quaternion(0.5f, 0.5f, 0.5f, 0.5f),
      Quaternion(0.707f, 0.0f, 0.707f, 0.0f),
  };

  for (int idx = 0; idx < 4; idx++) {
    EulerAngles euler = quaternion_to_euler_degrees(rotations[idx]);

    bool in_range = (euler.roll_deg >= -180.0f && euler.roll_deg <= 180.0f) &&
                    (euler.pitch_deg >= -90.0f && euler.pitch_deg <= 90.0f) &&
                    (euler.yaw_deg >= -180.0f && euler.yaw_deg <= 180.0f);

    test_assert(in_range, "Euler angles in valid range for quaternion " + std::to_string(idx));
  }
}

void test_known_rotations() {
  std::cout << "\n>>> Testing Known Rotations <<<" << std::endl;

  // 90° around X axis: roll ≈ 90°
  {
    Quaternion q = euler_to_quaternion_degrees(90.0f, 0.0f, 0.0f);
    EulerAngles euler = quaternion_to_euler_degrees(q);
    bool correct = std::fabs(euler.roll_deg - 90.0f) < 0.5f;
    test_assert(correct, "90° rotation around X axis (roll ≈ 90°)");
  }

  // 90° around Y axis: pitch ≈ 90°
  {
    Quaternion q = euler_to_quaternion_degrees(0.0f, 90.0f, 0.0f);
    EulerAngles euler = quaternion_to_euler_degrees(q);
    bool correct = std::fabs(euler.pitch_deg - 90.0f) < 0.5f;
    test_assert(correct, "90° rotation around Y axis (pitch ≈ 90°)");
  }

  // 90° around Z axis: yaw ≈ 90°
  {
    Quaternion q = euler_to_quaternion_degrees(0.0f, 0.0f, 90.0f);
    EulerAngles euler = quaternion_to_euler_degrees(q);
    bool correct = std::fabs(euler.yaw_deg - 90.0f) < 0.5f;
    test_assert(correct, "90° rotation around Z axis (yaw ≈ 90°)");
  }
}

void test_round_trip_conversion() {
  std::cout << "\n>>> Testing Round-Trip Conversion <<<" << std::endl;

  float test_angles[] = {0.0f, 30.0f, 45.0f, 60.0f, -30.0f, -45.0f};

  for (int idx = 0; idx < 6; idx++) {
    float original_roll = test_angles[idx];
    float original_pitch = test_angles[(idx + 1) % 6];
    float original_yaw = test_angles[(idx + 2) % 6];

    // Skip near gimbal lock
    if (std::fabs(original_pitch) > 85.0f) {
      continue;
    }

    // euler -> q -> euler
    Quaternion q = euler_to_quaternion_degrees(original_roll, original_pitch, original_yaw);
    EulerAngles result = quaternion_to_euler_degrees(q);

    bool correct = (std::fabs(result.roll_deg - original_roll) < 1.0f) &&
                   (std::fabs(result.pitch_deg - original_pitch) < 1.0f) &&
                   (std::fabs(result.yaw_deg - original_yaw) < 1.0f);

    std::string msg = "Roll=" + std::to_string((int)original_roll) +
                      ", Pitch=" + std::to_string((int)original_pitch) +
                      ", Yaw=" + std::to_string((int)original_yaw);
    test_assert(correct, "Round-trip conversion: " + msg);
  }
}

void test_quaternion_magnitude() {
  std::cout << "\n>>> Testing Quaternion Magnitude <<<" << std::endl;

  Quaternion test_quats[] = {
      Quaternion(1.0f, 0.0f, 0.0f, 0.0f),
      Quaternion(0.7071f, 0.7071f, 0.0f, 0.0f),
      Quaternion(0.5f, 0.5f, 0.5f, 0.5f),
  };

  for (int idx = 0; idx < 3; idx++) {
    float mag = test_quats[idx].magnitude();
    bool valid = mag > 0.99f && mag < 1.01f;
    test_assert(valid, "Quaternion magnitude valid for quat " + std::to_string(idx) +
                       " (magnitude: " + std::to_string(mag) + ")");
  }
}

void test_matrix_and_euler_consistent() {
  std::cout << "\n>>> Testing Matrix-Euler Consistency <<<" << std::endl;

  Quaternion q(0.6f, 0.4f, 0.5f, 0.35f);
  q.normalize();

  RotationMatrix R = quaternion_to_rotation_matrix(q);
  EulerAngles euler = quaternion_to_euler_degrees(q);

  // Create rotation matrix from Euler angles
  Quaternion q_from_euler = euler_to_quaternion_degrees(
      euler.roll_deg, euler.pitch_deg, euler.yaw_deg);
  RotationMatrix R_from_euler = quaternion_to_rotation_matrix(q_from_euler);

  // Check similarity
  bool matrices_similar = true;
  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 3; j++) {
      if (std::fabs(R.m[i][j] - R_from_euler.m[i][j]) > 0.001f) {
        matrices_similar = false;
      }
    }
  }

  test_assert(matrices_similar, "Matrix from quaternion equals matrix from Euler angles");
}

void test_vector_rotation_consistency() {
  std::cout << "\n>>> Testing Vector Rotation Consistency <<<" << std::endl;

  Quaternion q = euler_to_quaternion_degrees(30.0f, 45.0f, 60.0f);
  RotationMatrix R = quaternion_to_rotation_matrix(q);
  Vector3D test_vector(1.0f, 2.0f, 3.0f);

  Vector3D rotated_by_matrix = R.rotate_vector(test_vector);
  Vector3D rotated_by_quaternion = rotate_vector_by_quaternion(q, test_vector);

  bool consistent = (std::fabs(rotated_by_matrix.x - rotated_by_quaternion.x) < 0.0001f) &&
                    (std::fabs(rotated_by_matrix.y - rotated_by_quaternion.y) < 0.0001f) &&
                    (std::fabs(rotated_by_matrix.z - rotated_by_quaternion.z) < 0.0001f);

  test_assert(consistent, "Vector rotation by matrix vs quaternion");
}

void test_gimbal_lock_handling() {
  std::cout << "\n>>> Testing Gimbal Lock Handling <<<" << std::endl;

  float near_lock_angles[] = {85.0f, 89.0f, -85.0f, -89.0f};

  for (float pitch : near_lock_angles) {
    Quaternion q = euler_to_quaternion_degrees(30.0f, pitch, 45.0f);
    EulerAngles euler = quaternion_to_euler_degrees(q);

    // Should produce valid numbers, no NaN or inf
    bool valid = !std::isnan(euler.roll_deg) && !std::isnan(euler.pitch_deg) &&
                 !std::isnan(euler.yaw_deg) && !std::isinf(euler.roll_deg) &&
                 !std::isinf(euler.pitch_deg) && !std::isinf(euler.yaw_deg);

    test_assert(valid, "Gimbal lock handling at pitch " + std::to_string((int)pitch) + "°");
  }
}

void test_determinant_positive_one() {
  std::cout << "\n>>> Testing Determinant = +1 <<<" << std::endl;

  Quaternion rotations[] = {
      Quaternion(0.7071f, 0.7071f, 0.0f, 0.0f),
      Quaternion(0.7071f, 0.0f, 0.7071f, 0.0f),
      Quaternion(0.7071f, 0.0f, 0.0f, 0.7071f),
  };

  for (int idx = 0; idx < 3; idx++) {
    RotationMatrix R = quaternion_to_rotation_matrix(rotations[idx]);

    // Compute determinant
    float det = R.m[0][0] * (R.m[1][1] * R.m[2][2] - R.m[1][2] * R.m[2][1]) -
                R.m[0][1] * (R.m[1][0] * R.m[2][2] - R.m[1][2] * R.m[2][0]) +
                R.m[0][2] * (R.m[1][0] * R.m[2][1] - R.m[1][1] * R.m[2][0]);

    bool valid = det > 0.99f && det < 1.01f;
    test_assert(valid, "Determinant ≈ +1 for quaternion " + std::to_string(idx) +
                       " (det: " + std::to_string(det) + ")");
  }
}

void test_small_angles() {
  std::cout << "\n>>> Testing Small Angle Accuracy <<<" << std::endl;

  float small_angles[] = {0.1f, 0.5f, 1.0f, 2.0f, 5.0f};

  for (float angle : small_angles) {
    Quaternion q = euler_to_quaternion_degrees(angle, angle, angle);
    EulerAngles euler = quaternion_to_euler_degrees(q);

    bool correct = (std::fabs(euler.roll_deg - angle) < 0.1f) &&
                   (std::fabs(euler.pitch_deg - angle) < 0.1f) &&
                   (std::fabs(euler.yaw_deg - angle) < 0.1f);

    test_assert(correct, "Small angle accuracy at " + std::to_string(angle) + "°");
  }
}

// ============================================================================
// Main
// ============================================================================

int main() {
  std::cout << "=" << std::string(68, '=') << "=" << std::endl;
  std::cout << "BNO085 EXTENSION TESTS (No Dependencies)" << std::endl;
  std::cout << "=" << std::string(68, '=') << "=" << std::endl;

  test_identity_quaternion();
  test_orthonormality();
  test_euler_angles_valid_range();
  test_known_rotations();
  test_round_trip_conversion();
  test_quaternion_magnitude();
  test_matrix_and_euler_consistent();
  test_vector_rotation_consistency();
  test_gimbal_lock_handling();
  test_determinant_positive_one();
  test_small_angles();

  print_summary();

  return (pass_count == test_count) ? 0 : 1;
}
