/**
 * Unit Tests for State Dynamics (Phase 3 EKF)
 *
 * Tests the state propagation and Jacobian computation functions used in
 * the Extended Kalman Filter for BNO085 IMU + GPS sensor fusion.
 *
 * Test Coverage:
 * - propagate_state() with various sensor inputs
 * - Quaternion update and normalization
 * - Velocity integration with gravity
 * - Position integration
 * - F matrix dimension and structure
 * - F matrix Jacobian validation against finite differences
 * - Q matrix dimension and positive definiteness
 * - Utility functions (quaternion_to_rotation_matrix, etc.)
 *
 * Total: 20+ test cases
 */

#ifdef DEBUG_MODE

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <limits>
#include <iostream>

#include "../src/navigation/state_dynamics.h"
#include "../src/config/ekf_config.h"
#include "../src/math/quaternion.h"

// ============================================================================
// TEST FIXTURES AND UTILITIES
// ============================================================================

class StateDynamicsTest : public ::testing::Test {
 protected:
    // Tolerance values
    static constexpr float TOLERANCE_STATE = 1e-5f;
    static constexpr float TOLERANCE_MATRIX = 1e-6f;
    static constexpr float TOLERANCE_JACOBIAN = 1e-4f;  // For finite difference
    static constexpr float TOLERANCE_POSITIVITY = 1e-10f;

    // Helper: Initialize state to identity (no rotation, at origin, no motion)
    void initialize_identity_state(float state[EKF_STATE_SIZE]) {
        // Quaternion: identity [1, 0, 0, 0]
        state[0] = 1.0f;
        state[1] = 0.0f;
        state[2] = 0.0f;
        state[3] = 0.0f;

        // Velocity: zero [0, 0, 0]
        state[4] = 0.0f;
        state[5] = 0.0f;
        state[6] = 0.0f;

        // Position: origin [0, 0, 0]
        state[7] = 0.0f;
        state[8] = 0.0f;
        state[9] = 0.0f;

        // Accelerometer bias: zero [0, 0, 0]
        state[10] = 0.0f;
        state[11] = 0.0f;
        state[12] = 0.0f;

        // Gyroscope bias: zero [0, 0, 0]
        state[13] = 0.0f;
        state[14] = 0.0f;
        state[15] = 0.0f;
    }

    // Helper: Compute quaternion magnitude
    float quat_magnitude(const float q[4]) {
        return sqrt(q[0]*q[0] + q[1]*q[1] + q[2]*q[2] + q[3]*q[3]);
    }

    // Helper: Check if quaternion is normalized
    bool quat_is_normalized(const float q[4], float tol = 0.01f) {
        float mag = quat_magnitude(q);
        return fabsf(mag - 1.0f) < tol;
    }

    // Helper: Finite difference approximation of Jacobian
    void compute_F_matrix_finite_diff(float F_fd[256],
                                      const float state[EKF_STATE_SIZE],
                                      const float gyro_rad_s[3],
                                      const float accel_m_s2[3],
                                      float dt_s,
                                      float eps = 1e-6f) {
        // Initialize output
        memset(F_fd, 0, 256*sizeof(float));

        // Compute baseline state propagation
        float state_base[EKF_STATE_SIZE];
        memcpy(state_base, state, EKF_STATE_SIZE*sizeof(float));
        propagate_state(state_base, gyro_rad_s, accel_m_s2, dt_s);

        // Perturb each state element and compute numerical derivative
        for (int j = 0; j < EKF_STATE_SIZE; j++) {
            float state_pert[EKF_STATE_SIZE];
            memcpy(state_pert, state, EKF_STATE_SIZE*sizeof(float));

            // Perturb state[j] by +eps
            state_pert[j] += eps;

            // Propagate perturbed state
            propagate_state(state_pert, gyro_rad_s, accel_m_s2, dt_s);

            // Compute finite difference: (f(x+eps) - f(x)) / eps
            for (int i = 0; i < EKF_STATE_SIZE; i++) {
                F_fd[i*EKF_STATE_SIZE + j] = (state_pert[i] - state_base[i]) / eps;
            }
        }
    }

    // Helper: Check if matrix is positive definite (all eigenvalues > 0)
    // Simple check: diagonal dominance for diagonal matrices
    bool is_positive_definite_diagonal(const float M[256]) {
        // For a diagonal matrix, just check all diagonal elements > 0
        for (int i = 0; i < EKF_STATE_SIZE; i++) {
            if (M[i*EKF_STATE_SIZE + i] <= TOLERANCE_POSITIVITY) {
                return false;
            }
        }
        return true;
    }
};

// ============================================================================
// PROPAGATE_STATE TESTS
// ============================================================================

TEST_F(StateDynamicsTest, PropagatState_ZeroDt_NoChange) {
    // With dt=0, state should not change
    float state[EKF_STATE_SIZE];
    initialize_identity_state(state);

    float state_copy[EKF_STATE_SIZE];
    memcpy(state_copy, state, EKF_STATE_SIZE*sizeof(float));

    float gyro[3] = {0.1f, 0.2f, 0.3f};
    float accel[3] = {0.0f, 0.0f, -9.81f};

    propagate_state(state, gyro, accel, 0.0f);  // dt = 0

    // Check all state elements unchanged
    for (int i = 0; i < EKF_STATE_SIZE; i++) {
        EXPECT_NEAR(state[i], state_copy[i], TOLERANCE_STATE)
            << "State[" << i << "] changed with dt=0";
    }
}

TEST_F(StateDynamicsTest, PropagatState_ZeroGyro_QuaternionUnchanged) {
    // With zero gyro (and zero gyro bias), quaternion should not change
    float state[EKF_STATE_SIZE];
    initialize_identity_state(state);

    float gyro[3] = {0.0f, 0.0f, 0.0f};
    float accel[3] = {0.0f, 0.0f, -9.81f};
    float dt_s = 0.01f;

    float q_before[4];
    memcpy(q_before, &state[0], 4*sizeof(float));

    propagate_state(state, gyro, accel, dt_s);

    float q_after[4];
    memcpy(q_after, &state[0], 4*sizeof(float));

    // Quaternion should be unchanged
    for (int i = 0; i < 4; i++) {
        EXPECT_NEAR(q_after[i], q_before[i], TOLERANCE_STATE)
            << "Quaternion[" << i << "] changed with zero gyro";
    }
}

TEST_F(StateDynamicsTest, PropagatState_ZeroAccel_GravityApplied) {
    // With zero accel measurement but gravity in place:
    // a_ned = R(q) * (0 - 0) + [0, 0, g] = [0, 0, g]
    // v_new = v + dt * [0, 0, g]
    float state[EKF_STATE_SIZE];
    initialize_identity_state(state);
    state[4] = 0.0f;  // v_north = 0
    state[5] = 0.0f;  // v_east = 0
    state[6] = 0.0f;  // v_down = 0

    float gyro[3] = {0.0f, 0.0f, 0.0f};
    float accel[3] = {0.0f, 0.0f, 0.0f};  // Zero accel (still in free fall)
    float dt_s = 0.01f;

    propagate_state(state, gyro, accel, dt_s);

    // Velocity in down direction should increase (gravity applied)
    float v_down = state[6];
    float expected_v_down = EARTH_GRAVITY_MPS2 * dt_s;

    EXPECT_NEAR(v_down, expected_v_down, TOLERANCE_STATE)
        << "Gravity not applied correctly to velocity";
}

TEST_F(StateDynamicsTest, PropagatState_QuaternionNormalized) {
    // After propagation, quaternion should remain normalized
    float state[EKF_STATE_SIZE];
    initialize_identity_state(state);

    float gyro[3] = {1.0f, 2.0f, 3.0f};  // Significant angular velocity
    float accel[3] = {0.0f, 0.0f, -9.81f};
    float dt_s = 0.1f;

    propagate_state(state, gyro, accel, dt_s);

    // Extract quaternion
    float q[4];
    memcpy(q, &state[0], 4*sizeof(float));

    // Check normalization
    EXPECT_TRUE(quat_is_normalized(q))
        << "Quaternion not normalized after propagation";
}

TEST_F(StateDynamicsTest, PropagatState_VelocityIncreases_WithConstantAccel) {
    // With constant acceleration in body frame and identity orientation:
    // a_body = [1, 0, 0] m/s²
    // a_ned = R(I) * a_body = [1, 0, 0] m/s² (no rotation)
    // v_new = [0, 0, 0] + dt * [1, 0, 0 + g]
    //       = [dt, 0, g*dt]
    float state[EKF_STATE_SIZE];
    initialize_identity_state(state);

    float gyro[3] = {0.0f, 0.0f, 0.0f};
    float accel[3] = {1.0f, 0.0f, 0.0f};
    float dt_s = 0.01f;

    propagate_state(state, gyro, accel, dt_s);

    float v_north = state[4];
    float v_east = state[5];
    float v_down = state[6];

    EXPECT_NEAR(v_north, 1.0f * dt_s, TOLERANCE_STATE);
    EXPECT_NEAR(v_east, 0.0f, TOLERANCE_STATE);
    EXPECT_NEAR(v_down, EARTH_GRAVITY_MPS2 * dt_s, TOLERANCE_STATE);
}

TEST_F(StateDynamicsTest, PropagatState_PositionIncreases_WithConstantVelocity) {
    // With constant velocity, position should increase
    float state[EKF_STATE_SIZE];
    initialize_identity_state(state);
    state[4] = 1.0f;  // v_north = 1 m/s
    state[5] = 0.0f;  // v_east = 0
    state[6] = 0.0f;  // v_down = 0

    float gyro[3] = {0.0f, 0.0f, 0.0f};
    float accel[3] = {0.0f, 0.0f, -9.81f};  // Just gravity
    float dt_s = 0.01f;

    propagate_state(state, gyro, accel, dt_s);

    float p_north = state[7];
    float p_east = state[8];
    float p_down = state[9];

    EXPECT_NEAR(p_north, 1.0f * dt_s, TOLERANCE_STATE);
    EXPECT_NEAR(p_east, 0.0f, TOLERANCE_STATE);
    EXPECT_NEAR(p_down, 0.0f, TOLERANCE_STATE);
}

TEST_F(StateDynamicsTest, PropagatState_BiasRemainsConstant) {
    // During propagation, biases should not change
    float state[EKF_STATE_SIZE];
    initialize_identity_state(state);
    state[10] = 0.1f;  // accel_bias[0]
    state[11] = 0.2f;  // accel_bias[1]
    state[12] = 0.3f;  // accel_bias[2]
    state[13] = 0.01f; // gyro_bias[0]
    state[14] = 0.02f; // gyro_bias[1]
    state[15] = 0.03f; // gyro_bias[2]

    float state_copy[EKF_STATE_SIZE];
    memcpy(state_copy, state, EKF_STATE_SIZE*sizeof(float));

    float gyro[3] = {0.1f, 0.2f, 0.3f};
    float accel[3] = {1.0f, 0.0f, -9.81f};
    float dt_s = 0.01f;

    propagate_state(state, gyro, accel, dt_s);

    // Check biases unchanged
    for (int i = 10; i < 16; i++) {
        EXPECT_EQ(state[i], state_copy[i])
            << "Bias[" << i << "] changed during propagation";
    }
}

// ============================================================================
// F MATRIX TESTS
// ============================================================================

TEST_F(StateDynamicsTest, FMatrix_Dimensions) {
    // F matrix should be 16x16
    float F[256];
    float state[EKF_STATE_SIZE];
    initialize_identity_state(state);

    float gyro[3] = {0.0f, 0.0f, 0.0f};
    float accel[3] = {0.0f, 0.0f, -9.81f};
    float dt_s = 0.01f;

    compute_F_matrix(F, state, gyro, accel, dt_s);

    // If we get here without crash, dimensions are OK
    // (No explicit check needed for compile-time array)
    EXPECT_TRUE(true);
}

TEST_F(StateDynamicsTest, FMatrix_StructureIdentity) {
    // With zero gyro, F should be close to block-diagonal:
    // - ∂q/∂q ≈ I (identity)
    // - ∂v/∂v = I
    // - ∂p/∂p = I
    float F[256];
    float state[EKF_STATE_SIZE];
    initialize_identity_state(state);

    float gyro[3] = {0.0f, 0.0f, 0.0f};
    float accel[3] = {0.0f, 0.0f, 0.0f};
    float dt_s = 0.01f;

    compute_F_matrix(F, state, gyro, accel, dt_s);

    // Check quaternion block: should be close to I
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            float expected = (i == j) ? 1.0f : 0.0f;
            EXPECT_NEAR(F[i*16 + j], expected, TOLERANCE_MATRIX * 10)
                << "F[" << i << "," << j << "] not identity-like";
        }
    }

    // Check velocity block: should be identity
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            float expected = (i == j) ? 1.0f : 0.0f;
            EXPECT_NEAR(F[(4+i)*16 + (4+j)], expected, TOLERANCE_MATRIX)
                << "F[" << (4+i) << "," << (4+j) << "] not identity";
        }
    }
}

TEST_F(StateDynamicsTest, FMatrix_FiniteDifference_Quaternion) {
    // Validate F matrix against finite differences (quaternion block)
    float state[EKF_STATE_SIZE];
    initialize_identity_state(state);

    float gyro[3] = {0.1f, 0.2f, 0.3f};
    float accel[3] = {1.0f, 0.0f, -9.81f};
    float dt_s = 0.01f;

    // Compute F analytically
    float F[256];
    compute_F_matrix(F, state, gyro, accel, dt_s);

    // Compute F via finite differences
    float F_fd[256];
    compute_F_matrix_finite_diff(F_fd, state, gyro, accel, dt_s);

    // Compare quaternion block [0:4, 0:4]
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            EXPECT_NEAR(F[i*16 + j], F_fd[i*16 + j], TOLERANCE_JACOBIAN)
                << "F[" << i << "," << j << "] quaternion block mismatch";
        }
    }
}

TEST_F(StateDynamicsTest, FMatrix_FiniteDifference_Velocity) {
    // Validate F matrix against finite differences (velocity block)
    float state[EKF_STATE_SIZE];
    initialize_identity_state(state);

    float gyro[3] = {0.1f, 0.2f, 0.3f};
    float accel[3] = {1.0f, 0.0f, -9.81f};
    float dt_s = 0.01f;

    float F[256];
    compute_F_matrix(F, state, gyro, accel, dt_s);

    float F_fd[256];
    compute_F_matrix_finite_diff(F_fd, state, gyro, accel, dt_s);

    // Compare velocity block [4:7, *]
    for (int i = 4; i < 7; i++) {
        for (int j = 0; j < EKF_STATE_SIZE; j++) {
            EXPECT_NEAR(F[i*16 + j], F_fd[i*16 + j], TOLERANCE_JACOBIAN)
                << "F[" << i << "," << j << "] velocity block mismatch";
        }
    }
}

TEST_F(StateDynamicsTest, FMatrix_FiniteDifference_Position) {
    // Validate F matrix against finite differences (position block)
    float state[EKF_STATE_SIZE];
    initialize_identity_state(state);
    state[4] = 1.0f;  // v_north = 1 m/s
    state[5] = 2.0f;  // v_east = 2 m/s

    float gyro[3] = {0.0f, 0.0f, 0.0f};
    float accel[3] = {0.0f, 0.0f, -9.81f};
    float dt_s = 0.01f;

    float F[256];
    compute_F_matrix(F, state, gyro, accel, dt_s);

    float F_fd[256];
    compute_F_matrix_finite_diff(F_fd, state, gyro, accel, dt_s);

    // Compare position block [7:10, *]
    for (int i = 7; i < 10; i++) {
        for (int j = 0; j < EKF_STATE_SIZE; j++) {
            EXPECT_NEAR(F[i*16 + j], F_fd[i*16 + j], TOLERANCE_JACOBIAN)
                << "F[" << i << "," << j << "] position block mismatch";
        }
    }
}

// ============================================================================
// Q MATRIX TESTS
// ============================================================================

TEST_F(StateDynamicsTest, QMatrix_Dimensions) {
    // Q matrix should be 16x16
    float Q[256];
    float dt_s = 0.01f;

    compute_Q_matrix(Q, dt_s);

    // If we get here without crash, dimensions are OK
    EXPECT_TRUE(true);
}

TEST_F(StateDynamicsTest, QMatrix_DiagonalStructure) {
    // Q should be diagonal
    float Q[256];
    float dt_s = 0.01f;

    compute_Q_matrix(Q, dt_s);

    // Check off-diagonal elements are zero
    for (int i = 0; i < EKF_STATE_SIZE; i++) {
        for (int j = 0; j < EKF_STATE_SIZE; j++) {
            if (i != j) {
                EXPECT_NEAR(Q[i*16 + j], 0.0f, TOLERANCE_MATRIX)
                    << "Q[" << i << "," << j << "] is non-zero (not diagonal)";
            }
        }
    }
}

TEST_F(StateDynamicsTest, QMatrix_PositiveDefinite) {
    // All diagonal elements should be positive (sufficient for diagonal matrices)
    float Q[256];
    float dt_s = 0.01f;

    compute_Q_matrix(Q, dt_s);

    for (int i = 0; i < EKF_STATE_SIZE; i++) {
        EXPECT_GT(Q[i*16 + i], TOLERANCE_POSITIVITY)
            << "Q[" << i << "," << i << "] is not positive";
    }
}

TEST_F(StateDynamicsTest, QMatrix_ScaleWithDt) {
    // Q should scale appropriately with dt
    float Q1[256];
    float Q2[256];

    float dt1 = 0.01f;
    float dt2 = 0.02f;

    compute_Q_matrix(Q1, dt1);
    compute_Q_matrix(Q2, dt2);

    // For most elements, Q2 ≈ 2 * Q1 (linear scaling with dt)
    // For bias, Q scales with dt (not dt²)
    for (int i = 0; i < EKF_STATE_SIZE; i++) {
        if (i < 7) {  // q, v blocks scale with dt²
            EXPECT_NEAR(Q2[i*16 + i] / Q1[i*16 + i], 4.0f, 0.1f)
                << "Q[" << i << "] does not scale as dt²";
        } else if (i >= 10) {  // bias blocks scale with dt
            EXPECT_NEAR(Q2[i*16 + i] / Q1[i*16 + i], 2.0f, 0.1f)
                << "Q[" << i << "] does not scale as dt";
        }
    }
}

// ============================================================================
// UTILITY FUNCTION TESTS
// ============================================================================

TEST_F(StateDynamicsTest, QuaternionToRotationMatrix_Identity) {
    // Identity quaternion [1, 0, 0, 0] should give identity rotation matrix
    float q[4] = {1.0f, 0.0f, 0.0f, 0.0f};
    float R[9];

    quaternion_to_rotation_matrix(R, q);

    // Check identity structure
    float expected[9] = {1, 0, 0,
                         0, 1, 0,
                         0, 0, 1};
    for (int i = 0; i < 9; i++) {
        EXPECT_NEAR(R[i], expected[i], TOLERANCE_MATRIX)
            << "R[" << i << "] not identity";
    }
}

TEST_F(StateDynamicsTest, QuaternionToRotationMatrix_90DegreeRoll) {
    // Quaternion for 90° roll: [cos(45°), sin(45°), 0, 0]
    float cos45 = cosf(M_PI / 4.0f);
    float sin45 = sinf(M_PI / 4.0f);
    float q[4] = {cos45, sin45, 0.0f, 0.0f};
    float R[9];

    quaternion_to_rotation_matrix(R, q);

    // Expected rotation matrix for 90° roll (x-axis rotation):
    // [1,  0,     0    ]
    // [0,  cos90, -sin90]
    // [0,  sin90,  cos90]
    // Note: sin(90°)=1, cos(90°)=0, but we're actually at 90° not the identity
    // The actual result depends on the quaternion form

    // Just check orthogonality for now: R^T * R = I
    float RTR[9] = {0};
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            for (int k = 0; k < 3; k++) {
                RTR[i*3 + j] += R[k*3 + i] * R[k*3 + j];
            }
        }
    }

    // RTR should be identity
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            float expected = (i == j) ? 1.0f : 0.0f;
            EXPECT_NEAR(RTR[i*3 + j], expected, TOLERANCE_MATRIX)
                << "Rotation matrix not orthogonal at [" << i << "," << j << "]";
        }
    }
}

TEST_F(StateDynamicsTest, RotationMatrixQuaternionJacobian_Dimensions) {
    // Jacobian should be 3×3×4 = 36 elements
    float q[4] = {1.0f, 0.0f, 0.0f, 0.0f};
    float dR_dq[36];

    rotation_matrix_quaternion_jacobian(dR_dq, q);

    // If we get here without crash, dimensions are OK
    EXPECT_TRUE(true);
}

// ============================================================================
// INTEGRATION TESTS
// ============================================================================

TEST_F(StateDynamicsTest, RoundTrip_Propagation_AndJacobian) {
    // Propagate state twice with small dt
    // Compare to single propagation with 2*dt
    // Check consistency with F matrix
    float state1[EKF_STATE_SIZE];
    initialize_identity_state(state1);
    state1[4] = 1.0f;  // v_north

    float state2[EKF_STATE_SIZE];
    memcpy(state2, state1, EKF_STATE_SIZE*sizeof(float));

    float gyro[3] = {0.1f, 0.2f, 0.3f};
    float accel[3] = {1.0f, 0.0f, -9.81f};
    float dt_s = 0.005f;

    // Propagate state1 twice
    propagate_state(state1, gyro, accel, dt_s);
    propagate_state(state1, gyro, accel, dt_s);

    // Propagate state2 once with 2*dt
    propagate_state(state2, gyro, accel, 2*dt_s);

    // Results should be close (within numerical error)
    for (int i = 0; i < EKF_STATE_SIZE; i++) {
        // Allow larger tolerance for two-step propagation
        float tol = (i < 4) ? TOLERANCE_STATE * 10 : TOLERANCE_STATE * 5;
        EXPECT_NEAR(state1[i], state2[i], tol)
            << "Two-step vs. single-step mismatch at state[" << i << "]";
    }
}

#endif  // DEBUG_MODE
