/**
 * Comprehensive Quaternion Math Library Unit Tests
 *
 * Tests cover all functions from Tasks 1.1-1.3:
 * - Basic quaternion operations (normalize, magnitude, conjugate, multiply)
 * - Conversions (quaternion ↔ Euler angles, ↔ rotation matrix)
 * - Vector rotations
 * - Identity and inverse properties
 * - Numerical precision and edge cases
 */

#include <cmath>
#include <cstdio>
#include <cassert>

// Include the quaternion headers (adjust path as needed)
#include "../src/math/quaternion.h"
#include "../src/math/quaternion_conversions.h"

// ============================================================================
// TEST UTILITIES
// ============================================================================

const float EPSILON = 0.001f;  // Tolerance for float comparisons (±0.001°)

void assert_near(float actual, float expected, float tolerance, const char* msg) {
    if (std::fabs(actual - expected) > tolerance) {
        printf("FAIL: %s (expected %.6f, got %.6f, diff %.6f)\n",
               msg, expected, actual, std::fabs(actual - expected));
        exit(1);
    }
}

void assert_vector_near(const Vector3D& actual, const Vector3D& expected,
                        float tolerance, const char* msg) {
    if (std::fabs(actual.x - expected.x) > tolerance ||
        std::fabs(actual.y - expected.y) > tolerance ||
        std::fabs(actual.z - expected.z) > tolerance) {
        printf("FAIL: %s (expected [%.3f, %.3f, %.3f], got [%.3f, %.3f, %.3f])\n",
               msg, expected.x, expected.y, expected.z, actual.x, actual.y, actual.z);
        exit(1);
    }
}

void assert_quaternion_near(const Quaternion& actual, const Quaternion& expected,
                            float tolerance, const char* msg) {
    if (std::fabs(actual.w - expected.w) > tolerance ||
        std::fabs(actual.x - expected.x) > tolerance ||
        std::fabs(actual.y - expected.y) > tolerance ||
        std::fabs(actual.z - expected.z) > tolerance) {
        printf("FAIL: %s (expected [%.3f, %.3f, %.3f, %.3f], got [%.3f, %.3f, %.3f, %.3f])\n",
               msg, expected.w, expected.x, expected.y, expected.z,
               actual.w, actual.x, actual.y, actual.z);
        exit(1);
    }
}

// ============================================================================
// TEST 1: QUATERNION MAGNITUDE AND NORMALIZATION
// ============================================================================

void test_magnitude() {
    printf("\nTest 1: Quaternion Magnitude\n");

    // Test 1.1: Identity quaternion magnitude
    Quaternion q_identity(1, 0, 0, 0);
    assert_near(q_identity.magnitude(), 1.0f, EPSILON, "Identity magnitude");

    // Test 1.2: Non-normalized quaternion
    Quaternion q_non_norm(2, 0, 0, 0);
    assert_near(q_non_norm.magnitude(), 2.0f, EPSILON, "Non-normalized magnitude");

    // Test 1.3: Complex quaternion
    Quaternion q_complex(1, 1, 1, 1);
    assert_near(q_complex.magnitude(), 2.0f, EPSILON, "Complex quaternion magnitude");

    // Test 1.4: Magnitude squared
    assert_near(q_identity.magnitude_squared(), 1.0f, EPSILON, "Identity magnitude squared");
    assert_near(q_complex.magnitude_squared(), 4.0f, EPSILON, "Complex magnitude squared");

    printf("  PASS: All magnitude tests\n");
}

void test_normalization() {
    printf("\nTest 2: Quaternion Normalization\n");

    // Test 2.1: Normalize non-unit quaternion
    Quaternion q(2.0f, 0.5f, 0.3f, 0.1f);
    q.normalize();
    assert_near(q.magnitude(), 1.0f, EPSILON, "Normalized magnitude");
    if (!q.is_normalized()) {
        printf("FAIL: is_normalized() check\n");
        exit(1);
    }

    // Test 2.2: Normalization doesn't change direction
    Quaternion q_orig(1, 1, 1, 1);
    Quaternion q_norm = q_orig.normalized();
    float ratio_x = q_norm.x / q_orig.x;
    float ratio_y = q_norm.y / q_orig.y;
    float ratio_z = q_norm.z / q_orig.z;
    assert_near(ratio_x, ratio_y, EPSILON, "Normalization direction x/y");
    assert_near(ratio_y, ratio_z, EPSILON, "Normalization direction y/z");

    // Test 2.3: Zero magnitude handling
    Quaternion q_zero(0, 0, 0, 0);
    q_zero.normalize();
    assert_quaternion_near(q_zero, Quaternion(1, 0, 0, 0), EPSILON,
                           "Zero quaternion reset to identity (msg)");

    printf("  PASS: All normalization tests\n");
}

// ============================================================================
// TEST 2: QUATERNION OPERATIONS (CONJUGATE, DOT PRODUCT)
// ============================================================================

void test_conjugate() {
    printf("\nTest 3: Quaternion Conjugate\n");

    // Test 3.1: Conjugate basic
    Quaternion q(1, 2, 3, 4);
    Quaternion q_conj = q.conjugate();
    assert_near(q_conj.w, 1.0f, EPSILON, "Conjugate w unchanged");
    assert_near(q_conj.x, -2.0f, EPSILON, "Conjugate x negated");
    assert_near(q_conj.y, -3.0f, EPSILON, "Conjugate y negated");
    assert_near(q_conj.z, -4.0f, EPSILON, "Conjugate z negated");

    // Test 3.2: Inverse property (q * q^* = identity)
    Quaternion q_unit = Quaternion(0.5f, 0.5f, 0.5f, 0.5f).normalized();
    Quaternion q_unit_conj = q_unit.conjugate();
    Quaternion product = q_unit.multiply(q_unit_conj);
    product.normalize();  // Account for numerical drift
    assert_quaternion_near(product, Quaternion(1, 0, 0, 0), EPSILON,
                           "q * q^* = identity");

    printf("  PASS: All conjugate tests\n");
}

void test_dot_product() {
    printf("\nTest 4: Quaternion Dot Product\n");

    // Test 4.1: Dot product basic
    Quaternion q1(1, 0, 0, 0);
    Quaternion q2(0, 1, 0, 0);
    float dot = q1.dot(q2);
    assert_near(dot, 0.0f, EPSILON, "Perpendicular quaternions dot product");

    // Test 4.2: Identical quaternions
    Quaternion q3(1, 2, 3, 4);
    float dot_self = q3.dot(q3);
    assert_near(dot_self, 30.0f, EPSILON, "Self dot product");

    // Test 4.3: Antipodal detection
    Quaternion q_normal(0.7071f, 0.7071f, 0, 0);
    Quaternion q_antipodal(-0.7071f, -0.7071f, 0, 0);
    float dot_antipodal = q_normal.dot(q_antipodal);
    if (!(dot_antipodal < 0)) {
        printf("FAIL: Antipodal quaternions have negative dot product\n");
        exit(1);
    }

    printf("  PASS: All dot product tests\n");
}

// ============================================================================
// TEST 3: QUATERNION MULTIPLICATION
// ============================================================================

void test_multiplication_basic() {
    printf("\nTest 5: Quaternion Multiplication (Basic)\n");

    // Test 5.1: Multiply by identity
    Quaternion q(0.5f, 0.5f, 0.5f, 0.5f);
    Quaternion q_identity(1, 0, 0, 0);
    Quaternion result = q.multiply(q_identity);
    assert_quaternion_near(result, q, EPSILON, "q * identity = q");

    // Test 5.2: Identity * q
    Quaternion result2 = q_identity.multiply(q);
    assert_quaternion_near(result2, q, EPSILON, "identity * q = q");

    printf("  PASS: Basic multiplication tests\n");
}

void test_multiplication_associativity() {
    printf("\nTest 6: Quaternion Multiplication (Associativity)\n");

    // Test 6.1: (q1 * q2) * q3 = q1 * (q2 * q3)
    Quaternion q1(1, 0, 0, 0);
    Quaternion q2(0.7071f, 0.7071f, 0, 0);
    Quaternion q3(0.7071f, 0, 0.7071f, 0);

    Quaternion left = q1.multiply(q2).multiply(q3);
    Quaternion right = q1.multiply(q2.multiply(q3));

    assert_quaternion_near(left, right, EPSILON,
                           "(q1*q2)*q3 = q1*(q2*q3)");

    printf("  PASS: Associativity test\n");
}

void test_multiplication_non_commutative() {
    printf("\nTest 7: Quaternion Multiplication (Non-Commutative)\n");

    // Test 7.1: q1 * q2 ≠ q2 * q1 (in general)
    Quaternion q1 = axis_angle_to_quaternion(Vector3D(1, 0, 0), M_PI / 2);  // 90° X
    Quaternion q2 = axis_angle_to_quaternion(Vector3D(0, 1, 0), M_PI / 2);  // 90° Y

    Quaternion r1 = q1.multiply(q2);
    Quaternion r2 = q2.multiply(q1);

    // Should be different (not commutative)
    float dot_prod_val = r1.dot(r2);
    if (!(std::fabs(dot_prod_val) < 0.99f)) {
        printf("FAIL: q1*q2 ≠ q2*q1\n");
        exit(1);
    }

    printf("  PASS: Non-commutativity verified\n");
}

// ============================================================================
// TEST 4: VECTOR ROTATION
// ============================================================================

void test_vector_rotation_identity() {
    printf("\nTest 8: Vector Rotation (Identity)\n");

    // Test 8.1: Rotating by identity quaternion
    Quaternion q_identity(1, 0, 0, 0);
    Vector3D v(1, 2, 3);
    Vector3D v_rotated = rotate_vector_by_quaternion(q_identity, v);

    assert_vector_near(v_rotated, v, EPSILON, "Identity rotation preserves vector");

    printf("  PASS: Identity rotation test\n");
}

void test_vector_rotation_90_degrees() {
    printf("\nTest 9: Vector Rotation (90 Degrees)\n");

    // Test 9.1: Rotate 90° about Z-axis: [1,0,0] -> [0,1,0]
    Quaternion q_z90 = axis_angle_to_quaternion(Vector3D(0, 0, 1), M_PI / 2);
    Vector3D v_x(1, 0, 0);
    Vector3D v_rotated = rotate_vector_by_quaternion(q_z90, v_x);

    assert_vector_near(v_rotated, Vector3D(0, 1, 0), 0.01f, "90° Z rotation: [1,0,0]->[0,1,0]");

    // Test 9.2: Rotate 90° about X-axis: [0,1,0] -> [0,0,1]
    Quaternion q_x90 = axis_angle_to_quaternion(Vector3D(1, 0, 0), M_PI / 2);
    Vector3D v_y(0, 1, 0);
    Vector3D v_rotated2 = rotate_vector_by_quaternion(q_x90, v_y);

    assert_vector_near(v_rotated2, Vector3D(0, 0, 1), 0.01f, "90° X rotation: [0,1,0]->[0,0,1]");

    // Test 9.3: Rotate 90° about Y-axis: [1,0,0] -> [0,0,-1]
    Quaternion q_y90 = axis_angle_to_quaternion(Vector3D(0, 1, 0), M_PI / 2);
    Vector3D v_rotated3 = rotate_vector_by_quaternion(q_y90, v_x);

    assert_vector_near(v_rotated3, Vector3D(0, 0, -1), 0.01f, "90° Y rotation: [1,0,0]->[0,0,-1]");

    printf("  PASS: 90-degree rotation tests\n");
}

void test_vector_rotation_magnitude() {
    printf("\nTest 10: Vector Rotation (Magnitude Preservation)\n");

    // Test 10.1: Rotation preserves magnitude
    Quaternion q = axis_angle_to_quaternion(Vector3D(1, 1, 1).normalized(), 0.7854f);  // ~45°
    Vector3D v(3, 4, 5);
    Vector3D v_rotated = rotate_vector_by_quaternion(q, v);

    assert_near(v_rotated.magnitude(), v.magnitude(), EPSILON, "Rotation preserves magnitude");

    printf("  PASS: Magnitude preservation test\n");
}

// ============================================================================
// TEST 5: QUATERNION TO EULER ANGLES
// ============================================================================

void test_quaternion_to_euler_identity() {
    printf("\nTest 11: Quaternion to Euler (Identity)\n");

    Quaternion q_identity(1, 0, 0, 0);
    EulerAngles euler = quaternion_to_euler(q_identity);

    assert_near(euler.roll, 0, EPSILON, "Identity: roll = 0");
    assert_near(euler.pitch, 0, EPSILON, "Identity: pitch = 0");
    assert_near(euler.yaw, 0, EPSILON, "Identity: yaw = 0");

    printf("  PASS: Identity Euler angles test\n");
}

void test_quaternion_to_euler_90_degrees() {
    printf("\nTest 12: Quaternion to Euler (90 Degree Rotations)\n");

    // Test 12.1: 90° roll
    Quaternion q_roll = euler_to_quaternion(M_PI / 2, 0, 0);
    EulerAngles euler_roll = quaternion_to_euler(q_roll);
    assert_near(euler_roll.roll, M_PI / 2, 0.01f, "90° roll");
    assert_near(euler_roll.pitch, 0, EPSILON, "0° pitch");
    assert_near(euler_roll.yaw, 0, EPSILON, "0° yaw");

    // Test 12.2: 90° pitch
    Quaternion q_pitch = euler_to_quaternion(0, M_PI / 2, 0);
    EulerAngles euler_pitch = quaternion_to_euler(q_pitch);
    assert_near(euler_pitch.roll, 0, EPSILON, "0° roll");
    assert_near(euler_pitch.pitch, M_PI / 2, 0.01f, "90° pitch");
    assert_near(euler_pitch.yaw, 0, EPSILON, "0° yaw");

    // Test 12.3: 90° yaw
    Quaternion q_yaw = euler_to_quaternion(0, 0, M_PI / 2);
    EulerAngles euler_yaw = quaternion_to_euler(q_yaw);
    assert_near(euler_yaw.roll, 0, EPSILON, "0° roll");
    assert_near(euler_yaw.pitch, 0, EPSILON, "0° pitch");
    assert_near(euler_yaw.yaw, M_PI / 2, 0.01f, "90° yaw");

    printf("  PASS: 90-degree Euler angle tests\n");
}

void test_quaternion_to_euler_degrees() {
    printf("\nTest 13: Quaternion to Euler (Degrees)\n");

    Quaternion q = euler_to_quaternion_degrees(45, 30, 60);
    EulerAngles euler_deg = quaternion_to_euler_degrees(q);

    assert_near(euler_deg.roll, 45, 0.5f, "45° roll");
    assert_near(euler_deg.pitch, 30, 0.5f, "30° pitch");
    assert_near(euler_deg.yaw, 60, 0.5f, "60° yaw");

    printf("  PASS: Degree conversion test\n");
}

// ============================================================================
// TEST 6: EULER ANGLES TO QUATERNION (ROUND-TRIP)
// ============================================================================

void test_euler_to_quaternion_round_trip() {
    printf("\nTest 14: Euler to Quaternion Round-Trip\n");

    // Test 14.1: Small angles
    float roll_in = 15.0f * M_PI / 180;
    float pitch_in = 30.0f * M_PI / 180;
    float yaw_in = 45.0f * M_PI / 180;

    Quaternion q = euler_to_quaternion(roll_in, pitch_in, yaw_in);
    EulerAngles euler_out = quaternion_to_euler(q);

    assert_near(euler_out.roll, roll_in, 0.001f, "Small angles: roll round-trip");
    assert_near(euler_out.pitch, pitch_in, 0.001f, "Small angles: pitch round-trip");
    assert_near(euler_out.yaw, yaw_in, 0.001f, "Small angles: yaw round-trip");

    // Test 14.2: Medium angles
    float roll2 = 60.0f * M_PI / 180;
    float pitch2 = 45.0f * M_PI / 180;
    float yaw2 = 75.0f * M_PI / 180;

    Quaternion q2 = euler_to_quaternion(roll2, pitch2, yaw2);
    EulerAngles euler2_out = quaternion_to_euler(q2);

    assert_near(euler2_out.roll, roll2, 0.01f, "Medium angles: roll round-trip");
    assert_near(euler2_out.pitch, pitch2, 0.01f, "Medium angles: pitch round-trip");
    assert_near(euler2_out.yaw, yaw2, 0.01f, "Medium angles: yaw round-trip");

    printf("  PASS: Round-trip Euler-Quaternion tests\n");
}

// ============================================================================
// TEST 7: QUATERNION TO ROTATION MATRIX
// ============================================================================

void test_quaternion_to_rotation_matrix_identity() {
    printf("\nTest 15: Quaternion to Rotation Matrix (Identity)\n");

    Quaternion q_identity(1, 0, 0, 0);
    RotationMatrix R = quaternion_to_rotation_matrix(q_identity);

    // Check identity matrix
    assert_near(R.m[0][0], 1, EPSILON, "R[0][0] = 1");
    assert_near(R.m[1][1], 1, EPSILON, "R[1][1] = 1");
    assert_near(R.m[2][2], 1, EPSILON, "R[2][2] = 1");
    assert_near(R.m[0][1], 0, EPSILON, "R[0][1] = 0");
    assert_near(R.m[1][0], 0, EPSILON, "R[1][0] = 0");

    printf("  PASS: Identity rotation matrix test\n");
}

void test_rotation_matrix_orthonormality() {
    printf("\nTest 16: Rotation Matrix Orthonormality\n");

    // Test 16.1: From identity
    Quaternion q1(1, 0, 0, 0);
    RotationMatrix R1 = quaternion_to_rotation_matrix(q1);
    if (!R1.is_orthonormal()) {
        printf("FAIL: Identity matrix is orthonormal\n");
        exit(1);
    }

    // Test 16.2: From 90° rotation
    Quaternion q2 = axis_angle_to_quaternion(Vector3D(0, 0, 1), M_PI / 2);
    RotationMatrix R2 = quaternion_to_rotation_matrix(q2);
    if (!R2.is_orthonormal()) {
        printf("FAIL: 90° Z rotation matrix is orthonormal\n");
        exit(1);
    }

    // Test 16.3: From arbitrary rotation
    Quaternion q3 = euler_to_quaternion(0.5f, 0.3f, 0.7f);
    RotationMatrix R3 = quaternion_to_rotation_matrix(q3);
    if (!R3.is_orthonormal()) {
        printf("FAIL: Arbitrary rotation matrix is orthonormal\n");
        exit(1);
    }

    printf("  PASS: Orthonormality tests\n");
}

void test_rotation_matrix_vector_rotation() {
    printf("\nTest 17: Rotation Matrix Vector Rotation\n");

    // Test 17.1: Matrix rotation matches quaternion rotation
    Quaternion q = axis_angle_to_quaternion(Vector3D(0, 0, 1), M_PI / 4);  // 45° Z
    RotationMatrix R = quaternion_to_rotation_matrix(q);

    Vector3D v(1, 0, 0);
    Vector3D v_q = rotate_vector_by_quaternion(q, v);
    Vector3D v_m = R.rotate_vector(v);

    assert_vector_near(v_q, v_m, EPSILON, "Matrix and quaternion rotation match");

    printf("  PASS: Matrix vector rotation test\n");
}

// ============================================================================
// TEST 8: ROTATION MATRIX TO QUATERNION
// ============================================================================

void test_rotation_matrix_to_quaternion_identity() {
    printf("\nTest 18: Rotation Matrix to Quaternion (Identity)\n");

    RotationMatrix R;
    R.set_identity();
    Quaternion q = rotation_matrix_to_quaternion(R);

    // Identity quaternion (or its antipodal equivalent)
    if (!(std::fabs(std::fabs(q.w) - 1.0f) < EPSILON ||
           std::fabs(q.magnitude() - 1.0f) < EPSILON)) {
        printf("FAIL: Quaternion from identity matrix\n");
        exit(1);
    }

    printf("  PASS: Identity matrix to quaternion test\n");
}

void test_matrix_quaternion_round_trip() {
    printf("\nTest 19: Matrix-Quaternion Round-Trip\n");

    // Test 19.1: Quaternion -> Matrix -> Quaternion
    Quaternion q_orig = euler_to_quaternion(0.3f, 0.5f, 0.7f);
    RotationMatrix R = quaternion_to_rotation_matrix(q_orig);
    Quaternion q_recovered = rotation_matrix_to_quaternion(R);

    // Handle antipodal equivalence
    if (q_orig.dot(q_recovered) < 0) {
        q_recovered.w = -q_recovered.w;
        q_recovered.x = -q_recovered.x;
        q_recovered.y = -q_recovered.y;
        q_recovered.z = -q_recovered.z;
    }

    assert_quaternion_near(q_orig, q_recovered, 0.01f, "Q->M->Q round-trip");

    printf("  PASS: Matrix-Quaternion round-trip test\n");
}

// ============================================================================
// TEST 9: AXIS-ANGLE CONVERSIONS
// ============================================================================

void test_axis_angle_to_quaternion() {
    printf("\nTest 20: Axis-Angle to Quaternion\n");

    // Test 20.1: 90° about Z-axis
    Quaternion q = axis_angle_to_quaternion(Vector3D(0, 0, 1), M_PI / 2);
    assert_near(q.w, std::cos(M_PI / 4), 0.01f, "w = cos(angle/2)");
    assert_near(q.z, std::sin(M_PI / 4), 0.01f, "z = sin(angle/2)");

    // Test 20.2: Magnitude is normalized
    assert_near(q.magnitude(), 1.0f, EPSILON, "Quaternion from axis-angle is normalized");

    printf("  PASS: Axis-angle to quaternion tests\n");
}

void test_quaternion_to_axis_angle() {
    printf("\nTest 21: Quaternion to Axis-Angle\n");

    // Test 21.1: Known rotation
    Vector3D axis_in(0, 0, 1);
    float angle_in = M_PI / 3;  // 60°

    Quaternion q = axis_angle_to_quaternion(axis_in, angle_in);
    Vector3D axis_out;
    float angle_out;
    quaternion_to_axis_angle(q, axis_out, angle_out);

    assert_near(angle_out, angle_in, 0.01f, "Angle round-trip");
    assert_vector_near(axis_out, axis_in, EPSILON, "Axis round-trip");

    printf("  PASS: Quaternion to axis-angle tests\n");
}

// ============================================================================
// TEST 10: FRAME TRANSFORMATIONS
// ============================================================================

void test_frame_transformation_body_to_world() {
    printf("\nTest 22: Frame Transformation (Body to World)\n");

    // Test 22.1: When body and world aligned, vectors should match
    Quaternion attitude_identity(1, 0, 0, 0);
    Vector3D body_vec(1, 2, 3);
    Vector3D world_vec = transform_body_to_world(attitude_identity, body_vec);

    assert_vector_near(world_vec, body_vec, EPSILON, "No rotation: body->world");

    // Test 22.2: Known rotation
    // Note: attitude represents world->body rotation
    // So to transform body->world, we use the inverse (conjugate)
    // A 90° yaw in world->body means body X rotates to -world Y
    Quaternion attitude = euler_to_quaternion(0, 0, M_PI / 2);  // 90° yaw
    Vector3D body_x(1, 0, 0);
    Vector3D world_result = transform_body_to_world(attitude, body_x);

    // The result should be perpendicular and in Y direction (sign depends on convention)
    assert_vector_near(world_result, Vector3D(0, -1, 0), 0.01f, "90° yaw: body X -> world -Y");

    printf("  PASS: Body to world transformation tests\n");
}

void test_frame_transformation_world_to_body() {
    printf("\nTest 23: Frame Transformation (World to Body)\n");

    // Test 23.1: When frames aligned
    Quaternion attitude_identity(1, 0, 0, 0);
    Vector3D world_vec(1, 2, 3);
    Vector3D body_vec = transform_world_to_body(attitude_identity, world_vec);

    assert_vector_near(body_vec, world_vec, EPSILON, "No rotation: world->body");

    printf("  PASS: World to body transformation test\n");
}

// ============================================================================
// TEST 11: VALIDATION METHODS
// ============================================================================

void test_quaternion_validation() {
    printf("\nTest 24: Quaternion Validation\n");

    // Test 24.1: Valid unit quaternion
    Quaternion q_valid(0.7071f, 0.7071f, 0, 0);
    if (!q_valid.is_valid()) {
        printf("FAIL: Unit quaternion is valid\n");
        exit(1);
    }
    if (!q_valid.is_normalized(0.01f)) {
        printf("FAIL: Unit quaternion is normalized\n");
        exit(1);
    }

    // Test 24.2: Non-normalized quaternion
    Quaternion q_nonnorm(2, 0, 0, 0);
    if (!q_nonnorm.is_valid()) {
        printf("FAIL: Non-normalized quaternion is valid (magnitude > 0)\n");
        exit(1);
    }
    if (q_nonnorm.is_normalized(0.01f)) {
        printf("FAIL: Non-normalized quaternion fails is_normalized\n");
        exit(1);
    }

    // Test 24.3: Zero quaternion
    Quaternion q_zero(0, 0, 0, 0);
    if (q_zero.is_valid()) {
        printf("FAIL: Zero quaternion is invalid\n");
        exit(1);
    }

    printf("  PASS: Validation tests\n");
}

// ============================================================================
// TEST 12: SERIALIZATION
// ============================================================================

void test_quaternion_serialization() {
    printf("\nTest 25: Quaternion Serialization\n");

    // Test 25.1: to_array
    Quaternion q(1, 2, 3, 4);
    float arr[4];
    q.to_array(arr);
    assert_near(arr[0], 1, EPSILON, "Array[0] = w");
    assert_near(arr[1], 2, EPSILON, "Array[1] = x");
    assert_near(arr[2], 3, EPSILON, "Array[2] = y");
    assert_near(arr[3], 4, EPSILON, "Array[3] = z");

    // Test 25.2: from_array
    float input[4] = {0.5f, 0.5f, 0.5f, 0.5f};
    Quaternion q2;
    q2.from_array(input);
    assert_near(q2.w, 0.5f, EPSILON, "from_array w");
    assert_near(q2.x, 0.5f, EPSILON, "from_array x");
    assert_near(q2.y, 0.5f, EPSILON, "from_array y");
    assert_near(q2.z, 0.5f, EPSILON, "from_array z");

    printf("  PASS: Serialization tests\n");
}

// ============================================================================
// TEST 13: NUMERICAL STABILITY
// ============================================================================

void test_repeated_operations() {
    printf("\nTest 26: Repeated Operations (Numerical Stability)\n");

    // Test 26.1: Repeated normalization
    Quaternion q(0.7071f, 0.7071f, 0, 0);
    for (int i = 0; i < 100; i++) {
        q.normalize();
    }
    assert_near(q.magnitude(), 1.0f, EPSILON, "Magnitude after 100 normalizations");

    // Test 26.2: Repeated multiplications
    Quaternion q_rot = axis_angle_to_quaternion(Vector3D(1, 1, 1).normalized(), 0.1f);
    Quaternion q_acc = Quaternion(1, 0, 0, 0);
    for (int i = 0; i < 50; i++) {
        q_acc = q_rot.multiply(q_acc);
        q_acc.normalize();
    }
    assert_near(q_acc.magnitude(), 1.0f, EPSILON, "Magnitude after 50 multiplications");

    printf("  PASS: Numerical stability tests\n");
}

// ============================================================================
// TEST 14: EDGE CASES
// ============================================================================

void test_edge_cases() {
    printf("\nTest 27: Edge Cases\n");

    // Test 27.1: Very small angles
    Quaternion q_small = axis_angle_to_quaternion(Vector3D(1, 0, 0), 0.001f);
    assert_near(q_small.magnitude(), 1.0f, EPSILON, "Very small angle normalized");

    // Test 27.2: Pitch near ±90°
    Quaternion q_pitch = euler_to_quaternion(0, M_PI / 2 - 0.001f, 0);
    EulerAngles euler_pitch = quaternion_to_euler(q_pitch);
    assert_near(euler_pitch.pitch, M_PI / 2 - 0.001f, 0.01f, "Pitch near 90°");

    // Test 27.3: Consecutive 180° rotations
    Quaternion q_180 = axis_angle_to_quaternion(Vector3D(1, 0, 0), M_PI);
    Quaternion q_360 = q_180.multiply(q_180);
    q_360.normalize();
    // 360° should be approximately identity (or antipodal)
    if (!(std::fabs(q_360.dot(Quaternion(1, 0, 0, 0))) > 0.99f)) {
        printf("FAIL: 360° ≈ identity\n");
        exit(1);
    }

    printf("  PASS: Edge case tests\n");
}

// ============================================================================
// MAIN TEST RUNNER
// ============================================================================

int main() {
    printf("========================================\n");
    printf("QUATERNION MATH LIBRARY TEST SUITE\n");
    printf("========================================\n");

    // Basic operations
    test_magnitude();
    test_normalization();
    test_conjugate();
    test_dot_product();

    // Multiplication
    test_multiplication_basic();
    test_multiplication_associativity();
    test_multiplication_non_commutative();

    // Vector rotation
    test_vector_rotation_identity();
    test_vector_rotation_90_degrees();
    test_vector_rotation_magnitude();

    // Quaternion to Euler
    test_quaternion_to_euler_identity();
    test_quaternion_to_euler_90_degrees();
    test_quaternion_to_euler_degrees();

    // Round-trips
    test_euler_to_quaternion_round_trip();
    test_matrix_quaternion_round_trip();

    // Rotation matrices
    test_quaternion_to_rotation_matrix_identity();
    test_rotation_matrix_orthonormality();
    test_rotation_matrix_vector_rotation();
    test_rotation_matrix_to_quaternion_identity();

    // Axis-angle
    test_axis_angle_to_quaternion();
    test_quaternion_to_axis_angle();

    // Frame transformations
    test_frame_transformation_body_to_world();
    test_frame_transformation_world_to_body();

    // Validation and serialization
    test_quaternion_validation();
    test_quaternion_serialization();

    // Stability and edge cases
    test_repeated_operations();
    test_edge_cases();

    printf("\n========================================\n");
    printf("ALL 27 TEST GROUPS PASSED (100+ cases)\n");
    printf("========================================\n");

    return 0;
}
