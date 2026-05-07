#ifndef STATE_DYNAMICS_H
#define STATE_DYNAMICS_H

#include "../math/quaternion.h"
#include <cstring>

/**
 * State Dynamics for Phase 3 Extended Kalman Filter
 *
 * Implements state transition (propagation) and Jacobian computation for
 * the EKF used in BNO085 IMU + GPS sensor fusion.
 *
 * State Vector (16 elements):
 * [0-3]:   Quaternion q = [w, x, y, z]
 * [4-6]:   Velocity v_ned = [v_north, v_east, v_down] (m/s)
 * [7-9]:   Position p_ned = [p_north, p_east, p_down] (meters)
 * [10-12]: Accelerometer bias (m/s²)
 * [13-15]: Gyroscope bias (rad/s)
 *
 * All matrices are pre-allocated for fixed size (no dynamic allocation).
 */

// State vector size constant
static constexpr int EKF_STATE_SIZE = 16;

// Matrix and vector types (using flat arrays)
using EKFState = float[EKF_STATE_SIZE];

// ============================================================================
// STATE PROPAGATION
// ============================================================================

/**
 * Propagate EKF state one time step forward
 *
 * Implements discrete-time state transition:
 *
 * 1. Quaternion update (orientation):
 *    q_new = q + 0.5 * dt * (q ⊗ ω)
 *    where ω = gyro_rad_s - gyro_bias
 *
 * 2. Velocity update:
 *    v_new = v + dt * (R(q) * (accel - accel_bias) + gravity_vector)
 *    where R(q) is rotation matrix from body to NED frame
 *    gravity_vector = [0, 0, g] (9.81 m/s²)
 *
 * 3. Position update:
 *    p_new = p + dt * v
 *
 * 4. Bias evolution (constant):
 *    bias_new = bias (biases don't change in propagation step)
 *
 * @param state Input state vector (will be modified in-place)
 * @param gyro_rad_s Raw gyroscope measurements [roll, pitch, yaw] in rad/s
 * @param accel_m_s2 Raw accelerometer measurements [ax, ay, az] in m/s²
 * @param dt_s Time step in seconds
 * @return Pointer to modified state (same as input state)
 *
 * @note
 * - Quaternion is automatically normalized after update
 * - Uses calibration stored in state vector (elements [10-15])
 * - Gravity is always applied downward in NED frame
 */
float* propagate_state(float state[EKF_STATE_SIZE],
                       const float gyro_rad_s[3],
                       const float accel_m_s2[3],
                       float dt_s);

// ============================================================================
// JACOBIAN COMPUTATION
// ============================================================================

/**
 * Compute the state transition Jacobian matrix F
 *
 * F is the 16×16 Jacobian of propagate_state with respect to state:
 * F[i][j] = ∂(state_new[i]) / ∂(state[j])
 *
 * Block structure (exploits sparsity):
 * - [0-3, 0-3]: ∂q/∂q (quaternion kinematics - nonlinear)
 * - [4-6, 0-3]: ∂v/∂q (velocity depends on orientation)
 * - [4-6, 4-6]: ∂v/∂v (velocity update - identity-like)
 * - [4-6, 10-12]: ∂v/∂accel_bias (coupling with bias)
 * - [7-9, 4-6]: ∂p/∂v (position integrates velocity - identity)
 * - [7-9, 7-9]: ∂p/∂p (position - identity)
 * - Other blocks: mostly zero
 *
 * @param F Output 16×16 Jacobian matrix (row-major, 256 elements)
 * @param state Current state vector
 * @param gyro_rad_s Gyroscope measurements [roll, pitch, yaw] in rad/s
 * @param accel_m_s2 Accelerometer measurements [ax, ay, az] in m/s²
 * @param dt_s Time step in seconds
 *
 * @note Memory layout: F[i*16 + j] = F[i][j] for row-major order
 * @note Numerical stability: handles small quaternions carefully
 */
void compute_F_matrix(float F[256],  // 16×16 = 256 elements
                      const float state[EKF_STATE_SIZE],
                      const float gyro_rad_s[3],
                      const float accel_m_s2[3],
                      float dt_s);

/**
 * Compute the process noise covariance matrix Q
 *
 * Q is the 16×16 discrete-time process noise covariance:
 * Q[i][j] = E[w[i] * w[j]] where w is process noise
 *
 * Diagonal structure (mostly):
 * - Q[0-3, 0-3]: Quaternion noise (scaled by dt²)
 * - Q[4-6, 4-6]: Velocity noise (scaled by dt²)
 * - Q[7-9, 7-9]: Position noise (usually zero - only indirect through velocity)
 * - Q[10-12, 10-12]: Accel bias noise (scaled by dt²)
 * - Q[13-15, 13-15]: Gyro bias noise (scaled by dt²)
 * - Off-diagonal: Zero (white noise assumption)
 *
 * @param Q Output 16×16 process noise matrix (row-major, 256 elements)
 * @param dt_s Time step in seconds
 *
 * @note Positive definite by construction (diagonal with positive values)
 * @note Uses noise parameters from ekf_config.h
 * @note Scales continuous-time noise by dt appropriately
 */
void compute_Q_matrix(float Q[256],  // 16×16 = 256 elements
                      float dt_s);

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

/**
 * Compute rotation matrix from quaternion
 *
 * Converts quaternion representation to 3×3 rotation matrix R(q) that
 * transforms vectors from body frame to NED frame.
 *
 * Formula: R(q) with components based on quaternion q = [w, x, y, z]
 *
 * @param R Output 3×3 rotation matrix (row-major, 9 elements)
 * @param q Quaternion [w, x, y, z]
 *
 * @note Memory layout: R[i*3 + j] = R[i][j]
 */
void quaternion_to_rotation_matrix(float R[9],  // 3×3 = 9 elements
                                   const float q[4]);

/**
 * Compute derivative of rotation matrix with respect to quaternion
 *
 * Used in Jacobian computation: ∂R[i,j]/∂q[k]
 *
 * @param dR_dq Output array of partial derivatives (3×3×4 = 36 elements)
 * @param q Quaternion [w, x, y, z]
 *
 * @note This is a helper for F matrix computation
 * @note dR_dq[i*12 + j*4 + k] = ∂R[i][j]/∂q[k]
 */
void rotation_matrix_quaternion_jacobian(float dR_dq[36],  // 3×3×4 = 36 elements
                                         const float q[4]);

#endif  // STATE_DYNAMICS_H
