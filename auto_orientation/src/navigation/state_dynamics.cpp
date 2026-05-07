#include "state_dynamics.h"
#include "../config/ekf_config.h"
#include <math.h>
#include <string.h>

// ============================================================================
// INTERNAL HELPER: QUATERNION MULTIPLICATION (FOR PROPAGATION)
// ============================================================================

/**
 * Multiply two quaternions: q_result = q1 * q2
 * Represents: first apply q2, then apply q1
 *
 * Formula:
 * w = w1*w2 - x1*x2 - y1*y2 - z1*z2
 * x = w1*x2 + x1*w2 + y1*z2 - z1*y2
 * y = w1*y2 - x1*z2 + y1*w2 + z1*x2
 * z = w1*z2 + x1*y2 - y1*x2 + z1*w2
 */
static void quat_multiply(float q_result[4],
                         const float q1[4], const float q2[4]) {
    float w1 = q1[0], x1 = q1[1], y1 = q1[2], z1 = q1[3];
    float w2 = q2[0], x2 = q2[1], y2 = q2[2], z2 = q2[3];

    q_result[0] = w1*w2 - x1*x2 - y1*y2 - z1*z2;
    q_result[1] = w1*x2 + x1*w2 + y1*z2 - z1*y2;
    q_result[2] = w1*y2 - x1*z2 + y1*w2 + z1*x2;
    q_result[3] = w1*z2 + x1*y2 - y1*x2 + z1*w2;
}

/**
 * Normalize quaternion to unit magnitude
 */
static void quat_normalize(float q[4]) {
    float mag_sq = q[0]*q[0] + q[1]*q[1] + q[2]*q[2] + q[3]*q[3];

    if (mag_sq < 1e-12f) {
        // Degenerate case: reset to identity
        q[0] = 1.0f;
        q[1] = 0.0f;
        q[2] = 0.0f;
        q[3] = 0.0f;
        return;
    }

    float inv_mag = 1.0f / sqrtf(mag_sq);
    q[0] *= inv_mag;
    q[1] *= inv_mag;
    q[2] *= inv_mag;
    q[3] *= inv_mag;
}

// ============================================================================
// STATE PROPAGATION
// ============================================================================

float* propagate_state(float state[EKF_STATE_SIZE],
                       const float gyro_rad_s[3],
                       const float accel_m_s2[3],
                       float dt_s) {
    // Extract state components
    float q[4];
    memcpy(q, &state[0], 4*sizeof(float));

    float v_ned[3];
    memcpy(v_ned, &state[4], 3*sizeof(float));

    float p_ned[3];
    memcpy(p_ned, &state[7], 3*sizeof(float));

    float accel_bias[3];
    memcpy(accel_bias, &state[10], 3*sizeof(float));

    float gyro_bias[3];
    memcpy(gyro_bias, &state[13], 3*sizeof(float));

    // ========================================================================
    // STEP 1: Update Quaternion
    // ========================================================================
    // Subtract bias from gyro measurement
    float omega_body[3];
    omega_body[0] = gyro_rad_s[0] - gyro_bias[0];
    omega_body[1] = gyro_rad_s[1] - gyro_bias[1];
    omega_body[2] = gyro_rad_s[2] - gyro_bias[2];

    // Create quaternion from angular velocity: q_omega = [0, ωx, ωy, ωz]
    float q_omega[4];
    q_omega[0] = 0.0f;
    q_omega[1] = omega_body[0];
    q_omega[2] = omega_body[1];
    q_omega[3] = omega_body[2];

    // Quaternion derivative: q_dot = 0.5 * q * q_omega
    float q_dot[4];
    quat_multiply(q_dot, q, q_omega);
    q_dot[0] *= 0.5f;
    q_dot[1] *= 0.5f;
    q_dot[2] *= 0.5f;
    q_dot[3] *= 0.5f;

    // Update quaternion: q_new = q + dt * q_dot
    float q_new[4];
    q_new[0] = q[0] + dt_s * q_dot[0];
    q_new[1] = q[1] + dt_s * q_dot[1];
    q_new[2] = q[2] + dt_s * q_dot[2];
    q_new[3] = q[3] + dt_s * q_dot[3];

    // Normalize quaternion
    quat_normalize(q_new);

    // ========================================================================
    // STEP 2: Compute Rotation Matrix from Quaternion
    // ========================================================================
    // We need R(q) for the velocity update: a_ned = R(q) * a_body
    //
    // R(q) transforms from body frame to NED frame.
    // For q = [w, x, y, z], the rotation matrix is:
    //
    // R = [ 1-2(y²+z²)   2(xy-wz)      2(xz+wy)    ]
    //     [ 2(xy+wz)     1-2(x²+z²)    2(yz-wx)    ]
    //     [ 2(xz-wy)     2(yz+wx)      1-2(x²+y²)  ]

    float w = q_new[0], x = q_new[1], y = q_new[2], z = q_new[3];

    float x2 = x*x, y2 = y*y, z2 = z*z;
    float xy = x*y, xz = x*z, yz = y*z;
    float wx = w*x, wy = w*y, wz = w*z;

    float R[9];
    R[0] = 1.0f - 2.0f*(y2 + z2);  // R[0,0]
    R[1] = 2.0f*(xy - wz);          // R[0,1]
    R[2] = 2.0f*(xz + wy);          // R[0,2]

    R[3] = 2.0f*(xy + wz);          // R[1,0]
    R[4] = 1.0f - 2.0f*(x2 + z2);  // R[1,1]
    R[5] = 2.0f*(yz - wx);          // R[1,2]

    R[6] = 2.0f*(xz - wy);          // R[2,0]
    R[7] = 2.0f*(yz + wx);          // R[2,1]
    R[8] = 1.0f - 2.0f*(x2 + y2);  // R[2,2]

    // ========================================================================
    // STEP 3: Update Velocity
    // ========================================================================
    // v_new = v + dt * (R(q) * (a_body - a_bias) + [0, 0, g])
    //
    // First compute: a_accel = a_body - a_bias
    float a_accel[3];
    a_accel[0] = accel_m_s2[0] - accel_bias[0];
    a_accel[1] = accel_m_s2[1] - accel_bias[1];
    a_accel[2] = accel_m_s2[2] - accel_bias[2];

    // Transform to NED: a_ned = R(q) * a_accel
    float a_ned[3];
    a_ned[0] = R[0]*a_accel[0] + R[1]*a_accel[1] + R[2]*a_accel[2];
    a_ned[1] = R[3]*a_accel[0] + R[4]*a_accel[1] + R[5]*a_accel[2];
    a_ned[2] = R[6]*a_accel[0] + R[7]*a_accel[1] + R[8]*a_accel[2];

    // Add gravity: a_ned[2] += g (down direction)
    a_ned[2] += EARTH_GRAVITY_MPS2;

    // Update velocity: v_new = v + dt * a_ned
    float v_new[3];
    v_new[0] = v_ned[0] + dt_s * a_ned[0];
    v_new[1] = v_ned[1] + dt_s * a_ned[1];
    v_new[2] = v_ned[2] + dt_s * a_ned[2];

    // ========================================================================
    // STEP 4: Update Position
    // ========================================================================
    // p_new = p + dt * v
    float p_new[3];
    p_new[0] = p_ned[0] + dt_s * v_new[0];
    p_new[1] = p_ned[1] + dt_s * v_new[1];
    p_new[2] = p_ned[2] + dt_s * v_new[2];

    // ========================================================================
    // STEP 5: Bias Evolution (constant, no change)
    // ========================================================================
    // Biases are not updated in propagation step
    // (They would be updated during measurement update)

    // ========================================================================
    // WRITE BACK TO STATE VECTOR
    // ========================================================================
    memcpy(&state[0], q_new, 4*sizeof(float));
    memcpy(&state[4], v_new, 3*sizeof(float));
    memcpy(&state[7], p_new, 3*sizeof(float));
    // Biases remain unchanged

    return state;
}

// ============================================================================
// QUATERNION TO ROTATION MATRIX
// ============================================================================

void quaternion_to_rotation_matrix(float R[9], const float q[4]) {
    float w = q[0], x = q[1], y = q[2], z = q[3];

    float x2 = x*x, y2 = y*y, z2 = z*z;
    float xy = x*y, xz = x*z, yz = y*z;
    float wx = w*x, wy = w*y, wz = w*z;

    R[0] = 1.0f - 2.0f*(y2 + z2);
    R[1] = 2.0f*(xy - wz);
    R[2] = 2.0f*(xz + wy);

    R[3] = 2.0f*(xy + wz);
    R[4] = 1.0f - 2.0f*(x2 + z2);
    R[5] = 2.0f*(yz - wx);

    R[6] = 2.0f*(xz - wy);
    R[7] = 2.0f*(yz + wx);
    R[8] = 1.0f - 2.0f*(x2 + y2);
}

// ============================================================================
// QUATERNION JACOBIAN (HELPER FOR F MATRIX)
// ============================================================================

void rotation_matrix_quaternion_jacobian(float dR_dq[36], const float q[4]) {
    float w = q[0], x = q[1], y = q[2], z = q[3];

    // Partial derivatives of each rotation matrix element
    // w.r.t. each quaternion component

    // R[0,0] = 1 - 2(y² + z²)
    dR_dq[0*4 + 0] = 0.0f;      // ∂R[0,0]/∂w
    dR_dq[0*4 + 1] = 0.0f;      // ∂R[0,0]/∂x
    dR_dq[0*4 + 2] = -4.0f*y;   // ∂R[0,0]/∂y
    dR_dq[0*4 + 3] = -4.0f*z;   // ∂R[0,0]/∂z

    // R[0,1] = 2(xy - wz)
    dR_dq[1*4 + 0] = -2.0f*z;   // ∂R[0,1]/∂w
    dR_dq[1*4 + 1] = 2.0f*y;    // ∂R[0,1]/∂x
    dR_dq[1*4 + 2] = 2.0f*x;    // ∂R[0,1]/∂y
    dR_dq[1*4 + 3] = -2.0f*w;   // ∂R[0,1]/∂z

    // R[0,2] = 2(xz + wy)
    dR_dq[2*4 + 0] = 2.0f*y;    // ∂R[0,2]/∂w
    dR_dq[2*4 + 1] = 2.0f*z;    // ∂R[0,2]/∂x
    dR_dq[2*4 + 2] = 2.0f*w;    // ∂R[0,2]/∂y
    dR_dq[2*4 + 3] = 2.0f*x;    // ∂R[0,2]/∂z

    // R[1,0] = 2(xy + wz)
    dR_dq[3*4 + 0] = 2.0f*z;    // ∂R[1,0]/∂w
    dR_dq[3*4 + 1] = 2.0f*y;    // ∂R[1,0]/∂x
    dR_dq[3*4 + 2] = 2.0f*x;    // ∂R[1,0]/∂y
    dR_dq[3*4 + 3] = 2.0f*w;    // ∂R[1,0]/∂z

    // R[1,1] = 1 - 2(x² + z²)
    dR_dq[4*4 + 0] = 0.0f;      // ∂R[1,1]/∂w
    dR_dq[4*4 + 1] = -4.0f*x;   // ∂R[1,1]/∂x
    dR_dq[4*4 + 2] = 0.0f;      // ∂R[1,1]/∂y
    dR_dq[4*4 + 3] = -4.0f*z;   // ∂R[1,1]/∂z

    // R[1,2] = 2(yz - wx)
    dR_dq[5*4 + 0] = -2.0f*x;   // ∂R[1,2]/∂w
    dR_dq[5*4 + 1] = -2.0f*w;   // ∂R[1,2]/∂x
    dR_dq[5*4 + 2] = 2.0f*z;    // ∂R[1,2]/∂y
    dR_dq[5*4 + 3] = 2.0f*y;    // ∂R[1,2]/∂z

    // R[2,0] = 2(xz - wy)
    dR_dq[6*4 + 0] = -2.0f*y;   // ∂R[2,0]/∂w
    dR_dq[6*4 + 1] = 2.0f*z;    // ∂R[2,0]/∂x
    dR_dq[6*4 + 2] = -2.0f*w;   // ∂R[2,0]/∂y
    dR_dq[6*4 + 3] = 2.0f*x;    // ∂R[2,0]/∂z

    // R[2,1] = 2(yz + wx)
    dR_dq[7*4 + 0] = 2.0f*x;    // ∂R[2,1]/∂w
    dR_dq[7*4 + 1] = 2.0f*w;    // ∂R[2,1]/∂x
    dR_dq[7*4 + 2] = 2.0f*z;    // ∂R[2,1]/∂y
    dR_dq[7*4 + 3] = 2.0f*y;    // ∂R[2,1]/∂z

    // R[2,2] = 1 - 2(x² + y²)
    dR_dq[8*4 + 0] = 0.0f;      // ∂R[2,2]/∂w
    dR_dq[8*4 + 1] = -4.0f*x;   // ∂R[2,2]/∂x
    dR_dq[8*4 + 2] = -4.0f*y;   // ∂R[2,2]/∂y
    dR_dq[8*4 + 3] = 0.0f;      // ∂R[2,2]/∂z
}

// ============================================================================
// JACOBIAN COMPUTATION: F MATRIX
// ============================================================================

void compute_F_matrix(float F[256],
                      const float state[EKF_STATE_SIZE],
                      const float gyro_rad_s[3],
                      const float accel_m_s2[3],
                      float dt_s) {
    // Initialize F to zero
    memset(F, 0, 256*sizeof(float));

    // Extract state components
    float q[4];
    memcpy(q, &state[0], 4*sizeof(float));

    float accel_bias[3];
    memcpy(accel_bias, &state[10], 3*sizeof(float));

    float gyro_bias[3];
    memcpy(gyro_bias, &state[13], 3*sizeof(float));

    // Subtract biases from measurements
    float omega_body[3];
    omega_body[0] = gyro_rad_s[0] - gyro_bias[0];
    omega_body[1] = gyro_rad_s[1] - gyro_bias[1];
    omega_body[2] = gyro_rad_s[2] - gyro_bias[2];

    float a_accel[3];
    a_accel[0] = accel_m_s2[0] - accel_bias[0];
    a_accel[1] = accel_m_s2[1] - accel_bias[1];
    a_accel[2] = accel_m_s2[2] - accel_bias[2];

    // ========================================================================
    // BLOCK: ∂q/∂q (Quaternion kinematics)
    // ========================================================================
    // q_dot = 0.5 * q * [0, ω]
    // ∂q_dot/∂q involves quaternion multiplication structure
    //
    // For q = [w, x, y, z], the skew-symmetric structure is:
    // ∂q_dot/∂q = 0.5 * [[0, -ωx, -ωy, -ωz],
    //                      [ωx, 0, ωz, -ωy],
    //                      [ωy, -ωz, 0, ωx],
    //                      [ωz, ωy, -ωx, 0]]
    //
    // Then F[0:4, 0:4] = I + dt * (∂q_dot/∂q)

    float wx = omega_body[0];
    float wy = omega_body[1];
    float wz = omega_body[2];

    // Identity + dt * (0.5 * Omega matrix)
    F[0*16 + 0] = 1.0f;                  // ∂q_new[0]/∂q[0]
    F[0*16 + 1] = -0.5f * dt_s * wx;     // ∂q_new[0]/∂q[1]
    F[0*16 + 2] = -0.5f * dt_s * wy;     // ∂q_new[0]/∂q[2]
    F[0*16 + 3] = -0.5f * dt_s * wz;     // ∂q_new[0]/∂q[3]

    F[1*16 + 0] = 0.5f * dt_s * wx;      // ∂q_new[1]/∂q[0]
    F[1*16 + 1] = 1.0f;                  // ∂q_new[1]/∂q[1]
    F[1*16 + 2] = 0.5f * dt_s * wz;      // ∂q_new[1]/∂q[2]
    F[1*16 + 3] = -0.5f * dt_s * wy;     // ∂q_new[1]/∂q[3]

    F[2*16 + 0] = 0.5f * dt_s * wy;      // ∂q_new[2]/∂q[0]
    F[2*16 + 1] = -0.5f * dt_s * wz;     // ∂q_new[2]/∂q[1]
    F[2*16 + 2] = 1.0f;                  // ∂q_new[2]/∂q[2]
    F[2*16 + 3] = 0.5f * dt_s * wx;      // ∂q_new[2]/∂q[3]

    F[3*16 + 0] = 0.5f * dt_s * wz;      // ∂q_new[3]/∂q[0]
    F[3*16 + 1] = 0.5f * dt_s * wy;      // ∂q_new[3]/∂q[1]
    F[3*16 + 2] = -0.5f * dt_s * wx;     // ∂q_new[3]/∂q[2]
    F[3*16 + 3] = 1.0f;                  // ∂q_new[3]/∂q[3]

    // ========================================================================
    // BLOCK: ∂v/∂q (Velocity depends on orientation)
    // ========================================================================
    // v_new = v + dt * (R(q) * a_accel + [0, 0, g])
    // ∂v/∂q involves derivatives of R w.r.t. q
    //
    // Get rotation matrix and its Jacobian
    float R[9];
    quaternion_to_rotation_matrix(R, q);

    float dR_dq[36];
    rotation_matrix_quaternion_jacobian(dR_dq, q);

    // For each velocity component i and quaternion component k:
    // ∂v[i]/∂q[k] = dt * sum_j (∂R[i,j]/∂q[k] * a_accel[j])

    for (int i = 0; i < 3; i++) {  // velocity rows (4-6)
        for (int k = 0; k < 4; k++) {  // quaternion cols (0-3)
            float sum = 0.0f;
            for (int j = 0; j < 3; j++) {
                sum += dR_dq[i*12 + j*4 + k] * a_accel[j];
            }
            F[(4+i)*16 + k] = dt_s * sum;
        }
    }

    // ========================================================================
    // BLOCK: ∂v/∂v (Velocity integration)
    // ========================================================================
    // v_new = v + dt * (...)
    // ∂v_new[i]/∂v[j] = δ_ij (identity)

    F[4*16 + 4] = 1.0f;
    F[5*16 + 5] = 1.0f;
    F[6*16 + 6] = 1.0f;

    // ========================================================================
    // BLOCK: ∂v/∂accel_bias (Velocity depends on bias)
    // ========================================================================
    // v_new = v + dt * (R(q) * (a_body - a_bias) + [0, 0, g])
    // ∂v/∂a_bias = -dt * R(q)

    for (int i = 0; i < 3; i++) {  // velocity rows
        for (int j = 0; j < 3; j++) {  // accel_bias cols
            F[(4+i)*16 + (10+j)] = -dt_s * R[i*3 + j];
        }
    }

    // ========================================================================
    // BLOCK: ∂p/∂v (Position integration)
    // ========================================================================
    // p_new = p + dt * v_new ≈ p + dt * (v + dt * (...))
    //       = p + dt * v + O(dt²)
    // ∂p_new[i]/∂v[j] = dt * δ_ij

    F[7*16 + 4] = dt_s;
    F[8*16 + 5] = dt_s;
    F[9*16 + 6] = dt_s;

    // ========================================================================
    // BLOCK: ∂p/∂p (Position propagation)
    // ========================================================================
    // p_new = p + dt * v
    // ∂p_new[i]/∂p[j] = δ_ij (identity)

    F[7*16 + 7] = 1.0f;
    F[8*16 + 8] = 1.0f;
    F[9*16 + 9] = 1.0f;

    // ========================================================================
    // BLOCK: ∂q/∂gyro_bias (Quaternion affected by gyro bias)
    // ========================================================================
    // ω_actual = ω_measured - gyro_bias
    // q_dot = 0.5 * q * [0, -ω_actual]
    // ∂q_dot/∂gyro_bias = -0.5 * (∂q_dot/∂ω) = -0.5 * (0.5 * Omega_matrix)
    //                   = -0.25 * Omega_matrix
    //
    // Then F[0:4, 13:16] involves this structure

    // This would be additional terms, but typically gyro bias contribution
    // is small and can be simplified. For full accuracy:
    // F[i, 13+j] = ∂q_new[i] / ∂gyro_bias[j]
    // Since ω affects q through Omega matrix structure

    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 3; j++) {
            // ∂q_dot/∂gyro_bias[j] = -0.5 * (∂q_dot/∂ω[j])
            // The sign comes from ω_actual = ω_measured - gyro_bias
            F[i*16 + (13+j)] = -0.5f * dt_s * F[i*16 + j];  // Use the Omega pattern
        }
    }

    // ========================================================================
    // BLOCK: ∂v/∂gyro_bias (Velocity indirectly affected through orientation)
    // ========================================================================
    // This is a second-order effect (ω_bias → q → R → v)
    // For simplicity, set to zero (can be added if higher accuracy needed)
    // In practice, gyro bias changes q, which changes R
    // But this coupling is weak and often neglected in EKF

    // ========================================================================
    // All other blocks are zero (already initialized by memset)
    // ========================================================================
}

// ============================================================================
// PROCESS NOISE COVARIANCE: Q MATRIX
// ============================================================================

void compute_Q_matrix(float Q[256], float dt_s) {
    // Initialize Q to zero
    memset(Q, 0, 256*sizeof(float));

    // Scale factors for discrete-time process noise
    // Continuous-time: x_dot = f(x) + w, where w ~ N(0, σ²)
    // Discrete-time: x_k+1 = x_k + ∫f(x)dt + w_discrete
    // For white noise: E[w_discrete * w_discrete^T] ≈ σ² * dt

    float dt2 = dt_s * dt_s;

    // ========================================================================
    // Quaternion (orientation) noise: [0-3, 0-3]
    // ========================================================================
    // Gyroscope white noise contributes to quaternion uncertainty
    // σ_q² ≈ (0.5 * dt * σ_ω)² ≈ 0.25 * dt² * σ_ω²
    // Use σ_ω² = Q_GYRO_NOISE

    float q_noise = 0.25f * dt2 * Q_GYRO_NOISE;

    Q[0*16 + 0] = q_noise;
    Q[1*16 + 1] = q_noise;
    Q[2*16 + 2] = q_noise;
    Q[3*16 + 3] = q_noise;

    // ========================================================================
    // Velocity noise: [4-6, 4-6]
    // ========================================================================
    // Accelerometer white noise contributes to velocity uncertainty
    // σ_v² ≈ (dt * σ_a)² = dt² * σ_a²
    // Use σ_a² = Q_ACCEL_NOISE

    float v_noise = dt2 * Q_ACCEL_NOISE;

    Q[4*16 + 4] = v_noise;
    Q[5*16 + 5] = v_noise;
    Q[6*16 + 6] = v_noise;

    // ========================================================================
    // Position noise: [7-9, 7-9]
    // ========================================================================
    // Position uncertainty primarily comes through velocity
    // For typical EKF, we don't add process noise directly to position
    // (Position is constrained by velocity integration)
    // However, we add a small amount (dt²/3 of velocity noise) to account for
    // model mismatch and ensure positive definiteness
    // This prevents covariance collapse while keeping it small enough that
    // velocity integration dominates the position uncertainty

    float p_noise = (dt2 / 3.0f) * v_noise;

    Q[7*16 + 7] = p_noise;
    Q[8*16 + 8] = p_noise;
    Q[9*16 + 9] = p_noise;

    // ========================================================================
    // Accelerometer bias noise: [10-12, 10-12]
    // ========================================================================
    // Bias evolves as a random walk: bias_new = bias + w_bias
    // where w_bias ~ N(0, σ_bias²)
    // Use σ_bias² = Q_ACCEL_BIAS_NOISE

    float accel_bias_noise = dt_s * Q_ACCEL_BIAS_NOISE;

    Q[10*16 + 10] = accel_bias_noise;
    Q[11*16 + 11] = accel_bias_noise;
    Q[12*16 + 12] = accel_bias_noise;

    // ========================================================================
    // Gyroscope bias noise: [13-15, 13-15]
    // ========================================================================
    // Similarly for gyro bias
    // Use σ_gyro_bias² = Q_GYRO_BIAS_NOISE

    float gyro_bias_noise = dt_s * Q_GYRO_BIAS_NOISE;

    Q[13*16 + 13] = gyro_bias_noise;
    Q[14*16 + 14] = gyro_bias_noise;
    Q[15*16 + 15] = gyro_bias_noise;
}
