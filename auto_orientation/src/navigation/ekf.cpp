#include <Arduino.h>
#include "ekf.h"
#include "../config/ekf_config.h"
#include <string.h>
#include <math.h>

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * Multiply two 3x3 matrices: C = A * B
 */
static void matrix_mult_3x3(const float A[3][3], const float B[3][3], float C[3][3]) {
  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 3; j++) {
      C[i][j] = 0.0f;
      for (int k = 0; k < 3; k++) {
        C[i][j] += A[i][k] * B[k][j];
      }
    }
  }
}

/**
 * Multiply 16x16 matrices: C = A * B
 */
static void matrix_mult_16x16(const Matrix16x16& A, const Matrix16x16& B, Matrix16x16& C) {
  float temp[16][16] = {};
  for (int i = 0; i < 16; i++) {
    for (int j = 0; j < 16; j++) {
      for (int k = 0; k < 16; k++) {
        temp[i][j] += A[i][k] * B[k][j];
      }
    }
  }
  memcpy(C, temp, sizeof(Matrix16x16));
}

/**
 * Multiply 16x16 * 16x1: result = A * b
 */
static void matrix_mult_16x16_vec(const Matrix16x16& A, const float b[16], float result[16]) {
  for (int i = 0; i < 16; i++) {
    result[i] = 0.0f;
    for (int j = 0; j < 16; j++) {
      result[i] += A[i][j] * b[j];
    }
  }
}

/**
 * Transpose a 16x16 matrix: B = A^T
 */
static void matrix_transpose_16x16(const Matrix16x16& A, Matrix16x16& B) {
  for (int i = 0; i < 16; i++) {
    for (int j = 0; j < 16; j++) {
      B[i][j] = A[j][i];
    }
  }
}

/**
 * Multiply quaternion: q_result = q1 * q2 (Hamilton product)
 * q = [w, x, y, z]
 */
static void quaternion_multiply(const float q1[4], const float q2[4], float result[4]) {
  float w1 = q1[0], x1 = q1[1], y1 = q1[2], z1 = q1[3];
  float w2 = q2[0], x2 = q2[1], y2 = q2[2], z2 = q2[3];

  result[0] = w1*w2 - x1*x2 - y1*y2 - z1*z2;
  result[1] = w1*x2 + x1*w2 + y1*z2 - z1*y2;
  result[2] = w1*y2 - x1*z2 + y1*w2 + z1*x2;
  result[3] = w1*z2 + x1*y2 - y1*x2 + z1*w2;
}

/**
 * Rotate a vector using quaternion: v_rotated = q * v * q^*
 * Simplified version for Earth gravity and acceleration vectors
 */
static void quaternion_rotate_vector(const float q[4], const float v[3], float result[3]) {
  // Convert vector to pure quaternion: [0, vx, vy, vz]
  float v_quat[4] = {0.0f, v[0], v[1], v[2]};

  // Compute conjugate: q* = [w, -x, -y, -z]
  float q_conj[4] = {q[0], -q[1], -q[2], -q[3]};

  // Compute q * v
  float temp[4];
  quaternion_multiply(q, v_quat, temp);

  // Compute (q * v) * q*
  float rotated[4];
  quaternion_multiply(temp, q_conj, rotated);

  // Extract vector part
  result[0] = rotated[1];
  result[1] = rotated[2];
  result[2] = rotated[3];
}

/**
 * Normalize a quaternion
 */
static void quaternion_normalize(float q[4]) {
  float mag_sq = q[0]*q[0] + q[1]*q[1] + q[2]*q[2] + q[3]*q[3];
  if (mag_sq < 1e-10f) return;

  float mag = sqrt(mag_sq);
  q[0] /= mag;
  q[1] /= mag;
  q[2] /= mag;
  q[3] /= mag;
}

/**
 * Convert axis-angle to quaternion: q = [cos(theta/2), sin(theta/2)*axis]
 */
static void axis_angle_to_quaternion(float angle_rad, const float axis[3], float q[4]) {
  float half_angle = angle_rad * 0.5f;
  float sin_half = sin(half_angle);

  q[0] = cos(half_angle);
  q[1] = axis[0] * sin_half;
  q[2] = axis[1] * sin_half;
  q[3] = axis[2] * sin_half;

  quaternion_normalize(q);
}

/**
 * Clamp a value between min and max
 */
static float clamp(float value, float min_val, float max_val) {
  return fmax(min_val, fmin(max_val, value));
}

/**
 * Get current time in milliseconds (for Arduino compatibility)
 */
static uint32_t millis_() {
  // In tests, this will be mocked; in Arduino, it calls ::millis()
  #ifdef ARDUINO
  return millis();
  #else
  // For unit tests: return a mock value
  static uint32_t mock_time = 0;
  return mock_time;
  #endif
}

// ============================================================================
// Constructor / Destructor
// ============================================================================

ExtendedKalmanFilter::ExtendedKalmanFilter()
    : gps_dropout_flag_(true),
      last_gps_update_ms_(0),
      dropout_start_time_ms_(0),
      consecutive_dropout_updates_(0),
      last_innovation_magnitude_(0.0f) {
  memset(state_, 0, sizeof(state_));
  memset(P_, 0, sizeof(Matrix16x16));
  memset(F_, 0, sizeof(Matrix16x16));
  memset(Q_, 0, sizeof(Matrix16x16));

  // Initialize state: identity quaternion
  state_[0] = 1.0f;  // w = 1
}

ExtendedKalmanFilter::~ExtendedKalmanFilter() {
}

// ============================================================================
// Initialization
// ============================================================================

bool ExtendedKalmanFilter::initialize(const Matrix16x16& P_initial) {
  // Initialize state to identity quaternion, zero velocity/position
  state_[0] = 1.0f;  // quaternion w
  for (int i = 1; i < 16; i++) {
    state_[i] = 0.0f;
  }

  // Copy initial covariance
  memcpy(P_, P_initial, sizeof(Matrix16x16));

  // Initialize Q matrix
  initialize_q_matrix_();

  // Initialize GPS tracking
  gps_dropout_flag_ = true;
  last_gps_update_ms_ = 0;
  dropout_start_time_ms_ = millis_();
  consecutive_dropout_updates_ = 0;
  last_innovation_magnitude_ = 0.0f;

  // Verify covariance is positive-definite
  if (!is_positive_definite(P_)) {
    return false;
  }

  return true;
}

bool ExtendedKalmanFilter::reset() {
  return initialize(P_);  // Re-initialize with current covariance
}

// ============================================================================
// Q Matrix Initialization
// ============================================================================

void ExtendedKalmanFilter::initialize_q_matrix_() {
  // Q is process noise covariance (16x16 diagonal)
  // Structure: [q (4) | v (3) | p (3) | a_bias (3) | w_bias (3)]

  memset(Q_, 0, sizeof(Matrix16x16));

  // Quaternion noise: propagate from angular velocity noise
  float q_noise = Q_GYRO_NOISE * Q_GYRO_NOISE;
  Q_[0][0] = q_noise;
  Q_[1][1] = q_noise;
  Q_[2][2] = q_noise;
  Q_[3][3] = q_noise;

  // Velocity noise: from acceleration noise
  float v_noise = Q_ACCEL_NOISE * Q_ACCEL_NOISE;
  Q_[4][4] = v_noise;
  Q_[5][5] = v_noise;
  Q_[6][6] = v_noise;

  // Position noise: from velocity changes
  float p_noise = v_noise * 0.01f;  // Small since integrated from velocity
  Q_[7][7] = p_noise;
  Q_[8][8] = p_noise;
  Q_[9][9] = p_noise;

  // Accelerometer bias noise
  float a_bias_noise = Q_ACCEL_BIAS_NOISE;
  Q_[10][10] = a_bias_noise;
  Q_[11][11] = a_bias_noise;
  Q_[12][12] = a_bias_noise;

  // Gyroscope bias noise
  float w_bias_noise = Q_GYRO_BIAS_NOISE;
  Q_[13][13] = w_bias_noise;
  Q_[14][14] = w_bias_noise;
  Q_[15][15] = w_bias_noise;
}

// ============================================================================
// Predict Step (IMU-driven)
// ============================================================================

bool ExtendedKalmanFilter::predict(const float gyro_rad_s[3], const float accel_m_s2[3], float dt_sec) {
  if (dt_sec <= 0.0f || dt_sec > 0.5f) {
    return false;  // Invalid dt (>500ms is unreasonable)
  }

  // Get current state components
  float q[4], v[3], p[3];
  memcpy(q, state_ + 0, 4 * sizeof(float));  // Quaternion
  memcpy(v, state_ + 4, 3 * sizeof(float));  // Velocity
  memcpy(p, state_ + 7, 3 * sizeof(float));  // Position
  float a_bias[3], w_bias[3];
  memcpy(a_bias, state_ + 10, 3 * sizeof(float));  // Accel bias
  memcpy(w_bias, state_ + 13, 3 * sizeof(float));  // Gyro bias

  // ========================================================================
  // Update Quaternion (from gyroscope)
  // ========================================================================

  // Apply gyroscope bias correction
  float gyro_corrected[3];
  gyro_corrected[0] = gyro_rad_s[0] - w_bias[0];
  gyro_corrected[1] = gyro_rad_s[1] - w_bias[1];
  gyro_corrected[2] = gyro_rad_s[2] - w_bias[2];

  // Convert to quaternion: dq/dt = 0.5 * q * omega_quat
  float omega_norm = sqrt(gyro_corrected[0]*gyro_corrected[0] +
                                gyro_corrected[1]*gyro_corrected[1] +
                                gyro_corrected[2]*gyro_corrected[2]);

  float q_new[4];
  if (omega_norm > 1e-6f) {
    // Normalize rotation axis
    float axis[3] = {gyro_corrected[0] / omega_norm,
                      gyro_corrected[1] / omega_norm,
                      gyro_corrected[2] / omega_norm};

    // Create rotation quaternion for this dt
    float angle = omega_norm * dt_sec;
    float dq[4];
    axis_angle_to_quaternion(angle, axis, dq);

    // Apply rotation: q_new = dq * q
    quaternion_multiply(dq, q, q_new);
  } else {
    // No rotation
    memcpy(q_new, q, 4 * sizeof(float));
  }

  normalize_quaternion_();

  // ========================================================================
  // Update Velocity (from accelerometer + gravity)
  // ========================================================================

  // Apply accelerometer bias correction
  float accel_corrected[3];
  accel_corrected[0] = accel_m_s2[0] - a_bias[0];
  accel_corrected[1] = accel_m_s2[1] - a_bias[1];
  accel_corrected[2] = accel_m_s2[2] - a_bias[2];

  // Rotate acceleration from body frame to NED frame
  float accel_ned[3];
  quaternion_rotate_vector(q_new, accel_corrected, accel_ned);

  // Add gravity (gravity acts downward in NED: [0, 0, g])
  float a_ned[3];
  a_ned[0] = accel_ned[0];
  a_ned[1] = accel_ned[1];
  a_ned[2] = accel_ned[2] + EARTH_GRAVITY_MPS2;

  // Integrate velocity: v_new = v + a * dt
  float v_new[3];
  v_new[0] = v[0] + a_ned[0] * dt_sec;
  v_new[1] = v[1] + a_ned[1] * dt_sec;
  v_new[2] = v[2] + a_ned[2] * dt_sec;

  // ========================================================================
  // Update Position (from velocity)
  // ========================================================================

  float p_new[3];
  p_new[0] = p[0] + v[0] * dt_sec + 0.5f * a_ned[0] * dt_sec * dt_sec;
  p_new[1] = p[1] + v[1] * dt_sec + 0.5f * a_ned[1] * dt_sec * dt_sec;
  p_new[2] = p[2] + v[2] * dt_sec + 0.5f * a_ned[2] * dt_sec * dt_sec;

  // ========================================================================
  // Update State Vector
  // ========================================================================

  memcpy(state_ + 0, q_new, 4 * sizeof(float));
  memcpy(state_ + 4, v_new, 3 * sizeof(float));
  memcpy(state_ + 7, p_new, 3 * sizeof(float));
  // a_bias and w_bias remain unchanged (constant model)

  // ========================================================================
  // Update Covariance: P = F * P * F^T + Q
  // ========================================================================

  compute_f_jacobian_(gyro_rad_s, accel_m_s2, dt_sec);

  // FP = F * P (stack local replaces former P_temp_ member; ~1 KB stack peak)
  Matrix16x16 FP;
  matrix_mult_16x16(F_, P_, FP);

  // F_T = F^T
  Matrix16x16 F_T;
  matrix_transpose_16x16(F_, F_T);

  // P = FP * F_T + Q
  Matrix16x16 FPF_T;
  matrix_mult_16x16(FP, F_T, FPF_T);

  // Add Q
  for (int i = 0; i < 16; i++) {
    for (int j = 0; j < 16; j++) {
      P_[i][j] = FPF_T[i][j] + Q_[i][j];
    }
  }

  // Enforce covariance symmetry and positive-definiteness
  Matrix16x16 P_symmetric;
  enforce_covariance_symmetry(P_, P_symmetric);
  memcpy(P_, P_symmetric, sizeof(Matrix16x16));

  if (!is_positive_definite(P_)) {
    clamp_negative_eigenvalues(P_, P_);
  }

  // Track dropout
  if (gps_dropout_flag_) {
    consecutive_dropout_updates_++;
  }

  return true;
}

// ============================================================================
// Update Step (GPS-driven)
// ============================================================================

bool ExtendedKalmanFilter::update(const float gps_pos_ned_m[3], float gps_accuracy_m) {
  // Check GPS dropout flag
  if (gps_dropout_flag_) {
    return false;  // Reject update during GPS dropout
  }

  // Clamp measurement noise
  float measurement_noise = gps_accuracy_m * gps_accuracy_m;
  measurement_noise = clamp(measurement_noise, R_POSITION_MIN, R_POSITION_MAX);

  // ========================================================================
  // Compute Innovation (measurement residual)
  // ========================================================================

  float innovation[3];
  innovation[0] = gps_pos_ned_m[0] - state_[7];  // Position measurement - predicted
  innovation[1] = gps_pos_ned_m[1] - state_[8];
  innovation[2] = gps_pos_ned_m[2] - state_[9];

  // Check innovation magnitude (outlier rejection)
  last_innovation_magnitude_ = compute_innovation_magnitude(innovation);
  if (last_innovation_magnitude_ > MAX_INNOVATION_POSITION) {
    return false;  // Reject outlier
  }

  // ========================================================================
  // Compute Measurement Jacobian H (3x16)
  // ========================================================================

  float H[3][16];
  memset(H, 0, sizeof(H));
  H[0][7] = 1.0f;  // dz1/dp_north
  H[1][8] = 1.0f;  // dz2/dp_east
  H[2][9] = 1.0f;  // dz3/dp_down

  // ========================================================================
  // Compute Innovation Covariance: S = H * P * H^T + R
  // ========================================================================

  // H * P (3x16)
  float HP[3][16] = {};
  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 16; j++) {
      for (int k = 0; k < 16; k++) {
        HP[i][j] += H[i][k] * P_[k][j];
      }
    }
  }

  // S = H * P * H^T + R (3x3)
  float S[3][3] = {};
  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 3; j++) {
      for (int k = 0; k < 16; k++) {
        S[i][j] += HP[i][k] * H[j][k];
      }
      if (i == j) {
        S[i][j] += measurement_noise;  // Add measurement noise
      }
    }
  }

  // ========================================================================
  // Compute Kalman Gain: K = P * H^T * S^-1
  // ========================================================================

  // Invert S using simple 3x3 matrix inversion
  float det = S[0][0] * (S[1][1]*S[2][2] - S[1][2]*S[2][1])
            - S[0][1] * (S[1][0]*S[2][2] - S[1][2]*S[2][0])
            + S[0][2] * (S[1][0]*S[2][1] - S[1][1]*S[2][0]);

  if (abs(det) < 1e-10f) {
    return false;  // Singular matrix
  }

  float S_inv[3][3];
  S_inv[0][0] = (S[1][1]*S[2][2] - S[1][2]*S[2][1]) / det;
  S_inv[0][1] = (S[0][2]*S[2][1] - S[0][1]*S[2][2]) / det;
  S_inv[0][2] = (S[0][1]*S[1][2] - S[0][2]*S[1][1]) / det;
  S_inv[1][0] = (S[1][2]*S[2][0] - S[1][0]*S[2][2]) / det;
  S_inv[1][1] = (S[0][0]*S[2][2] - S[0][2]*S[2][0]) / det;
  S_inv[1][2] = (S[0][2]*S[1][0] - S[0][0]*S[1][2]) / det;
  S_inv[2][0] = (S[1][0]*S[2][1] - S[1][1]*S[2][0]) / det;
  S_inv[2][1] = (S[0][1]*S[2][0] - S[0][0]*S[2][1]) / det;
  S_inv[2][2] = (S[0][0]*S[1][1] - S[0][1]*S[1][0]) / det;

  // K = P * H^T * S_inv (16x3)
  float K[16][3] = {};
  for (int i = 0; i < 16; i++) {
    for (int j = 0; j < 3; j++) {
      for (int k = 0; k < 3; k++) {
        K[i][j] += P_[i][k + 7] * S_inv[k][j];  // H^T[k][j] = H[j][k]
      }
    }
  }

  // Clamp Kalman gain
  for (int i = 0; i < 16; i++) {
    for (int j = 0; j < 3; j++) {
      K[i][j] = clamp(K[i][j], -MAX_KALMAN_GAIN, MAX_KALMAN_GAIN);
    }
  }

  // ========================================================================
  // Update State: x = x + K * innovation
  // ========================================================================

  for (int i = 0; i < 16; i++) {
    for (int j = 0; j < 3; j++) {
      state_[i] += K[i][j] * innovation[j];
    }
  }

  // Normalize quaternion after update
  normalize_quaternion_();

  // ========================================================================
  // Update Covariance: P = (I - K*H) * P
  // ========================================================================

  // K * H (16x16)
  float KH[16][16] = {};
  for (int i = 0; i < 16; i++) {
    for (int j = 0; j < 16; j++) {
      for (int k = 0; k < 3; k++) {
        KH[i][j] += K[i][k] * H[k][j];
      }
    }
  }

  // (I - K*H)
  float I_KH[16][16];
  for (int i = 0; i < 16; i++) {
    for (int j = 0; j < 16; j++) {
      I_KH[i][j] = (i == j ? 1.0f : 0.0f) - KH[i][j];
    }
  }

  // P = (I - K*H) * P  (stack local replaces former P_temp_ member)
  Matrix16x16 P_new;
  matrix_mult_16x16(I_KH, P_, P_new);
  memcpy(P_, P_new, sizeof(Matrix16x16));

  // Enforce symmetry and positive-definiteness
  Matrix16x16 P_symmetric;
  enforce_covariance_symmetry(P_, P_symmetric);
  memcpy(P_, P_symmetric, sizeof(Matrix16x16));

  if (!is_positive_definite(P_)) {
    clamp_negative_eigenvalues(P_, P_);
  }

  // ========================================================================
  // Update GPS tracking
  // ========================================================================

  last_gps_update_ms_ = millis_();
  consecutive_dropout_updates_ = 0;

  return true;
}

// ============================================================================
// GPS Dropout Management
// ============================================================================

void ExtendedKalmanFilter::set_gps_dropout(bool flag) {
  if (flag && !gps_dropout_flag_) {
    // Transitioning to dropout
    dropout_start_time_ms_ = millis_();
  }
  gps_dropout_flag_ = flag;
}

bool ExtendedKalmanFilter::is_gps_locked() const {
  return !gps_dropout_flag_;
}

uint32_t ExtendedKalmanFilter::get_gps_age_ms() const {
  if (last_gps_update_ms_ == 0) {
    return UINT32_MAX;
  }
  uint32_t now = millis_();
  if (now >= last_gps_update_ms_) {
    return now - last_gps_update_ms_;
  }
  return 0;  // Clock wrapped
}

uint32_t ExtendedKalmanFilter::get_dead_reckoning_age_ms() const {
  if (!gps_dropout_flag_) {
    return 0;  // Not in dropout
  }
  uint32_t now = millis_();
  if (now >= dropout_start_time_ms_) {
    return now - dropout_start_time_ms_;
  }
  return 0;  // Clock wrapped
}

bool ExtendedKalmanFilter::is_dead_reckoning_expired() const {
  return get_dead_reckoning_age_ms() > 30000;  // 30 seconds
}

// ============================================================================
// State Access
// ============================================================================

void ExtendedKalmanFilter::get_quaternion(float q[4]) const {
  memcpy(q, state_, 4 * sizeof(float));
}

void ExtendedKalmanFilter::get_velocity(float v[3]) const {
  memcpy(v, state_ + 4, 3 * sizeof(float));
}

void ExtendedKalmanFilter::get_position(float p[3]) const {
  memcpy(p, state_ + 7, 3 * sizeof(float));
}

void ExtendedKalmanFilter::get_standard_deviations(float std_devs[16]) const {
  for (int i = 0; i < 16; i++) {
    if (P_[i][i] >= 0.0f) {
      std_devs[i] = sqrt(P_[i][i]);
    } else {
      std_devs[i] = 0.0f;
    }
  }
}

// ============================================================================
// Diagnostics
// ============================================================================

float ExtendedKalmanFilter::get_covariance_trace() const {
  return compute_covariance_trace(P_);
}

float ExtendedKalmanFilter::get_covariance_condition_number() const {
  return estimate_condition_number(P_);
}

bool ExtendedKalmanFilter::is_covariance_valid() const {
  return is_positive_definite(P_);
}

// ============================================================================
// Private Helper Methods
// ============================================================================

void ExtendedKalmanFilter::normalize_quaternion_() {
  float mag_sq = state_[0]*state_[0] + state_[1]*state_[1] +
                 state_[2]*state_[2] + state_[3]*state_[3];
  if (mag_sq < 1e-10f) {
    state_[0] = 1.0f;
    state_[1] = state_[2] = state_[3] = 0.0f;
    return;
  }

  float mag = sqrt(mag_sq);
  state_[0] /= mag;
  state_[1] /= mag;
  state_[2] /= mag;
  state_[3] /= mag;
}

void ExtendedKalmanFilter::compute_f_jacobian_(const float gyro_rad_s[3],
                                                const float accel_m_s2[3],
                                                float dt_sec) {
  // Initialize F to identity
  for (int i = 0; i < 16; i++) {
    for (int j = 0; j < 16; j++) {
      F_[i][j] = (i == j) ? 1.0f : 0.0f;
    }
  }

  // Quaternion partial derivatives (simplified, diagonal-dominant)
  float q_noise = sqrt(Q_[0][0]);
  float w = sqrt(gyro_rad_s[0]*gyro_rad_s[0] +
                      gyro_rad_s[1]*gyro_rad_s[1] +
                      gyro_rad_s[2]*gyro_rad_s[2]);

  if (w > 1e-6f) {
    // Scale quaternion update sensitivity
    float scale = 0.5f * dt_sec;
    F_[0][0] -= scale * scale;
  }

  // Velocity partial derivatives: v_new = v + a*dt
  // dv/dp = 0 (position doesn't affect velocity in this model)
  // dv/da = dt (acceleration affects velocity)
  float accel_mag = sqrt(accel_m_s2[0]*accel_m_s2[0] +
                               accel_m_s2[1]*accel_m_s2[1] +
                               accel_m_s2[2]*accel_m_s2[2]);
  if (accel_mag > 0.01f) {
    // Simplified: velocity uncertainty grows with acceleration
    F_[4][4] = 1.0f + 0.1f * dt_sec * accel_mag;
    F_[5][5] = F_[4][4];
    F_[6][6] = F_[4][4];
  }

  // Position partial derivatives: p_new = p + v*dt
  // dp/dv = dt (velocity affects position)
  F_[7][4] = dt_sec;
  F_[8][5] = dt_sec;
  F_[9][6] = dt_sec;

  // Bias derivatives: bias_new = bias (constant model)
  // F[10-15][10-15] = 1 (already set above)
}

void ExtendedKalmanFilter::compute_h_jacobian_(float H[3][16]) const {
  // H is the measurement Jacobian (3 measurements, 16 states)
  // GPS measures position: z = [p_north, p_east, p_down]
  // H relates measurements to state

  memset(H, 0, sizeof(float) * 3 * 16);

  // dz1/dp_north = 1
  H[0][7] = 1.0f;
  // dz2/dp_east = 1
  H[1][8] = 1.0f;
  // dz3/dp_down = 1
  H[2][9] = 1.0f;
}

void ExtendedKalmanFilter::update_covariance_predict_(const Matrix16x16& F) {
  // P = F * P * F^T + Q
  Matrix16x16 FP, FPF_T;

  matrix_mult_16x16(F, P_, FP);
  Matrix16x16 F_T;
  matrix_transpose_16x16(F, F_T);
  matrix_mult_16x16(FP, F_T, FPF_T);

  for (int i = 0; i < 16; i++) {
    for (int j = 0; j < 16; j++) {
      P_[i][j] = FPF_T[i][j] + Q_[i][j];
    }
  }
}

void ExtendedKalmanFilter::update_covariance_update_(const float K[3][16],
                                                      const float H[3][16]) {
  // P = (I - K*H) * P
  float KH[16][16] = {};
  for (int i = 0; i < 16; i++) {
    for (int j = 0; j < 16; j++) {
      for (int k = 0; k < 3; k++) {
        KH[i][j] += K[i][k] * H[k][j];
      }
    }
  }

  // Build (I - K*H) into a stack local (replaces former P_temp_ member).
  Matrix16x16 I_KH;
  for (int i = 0; i < 16; i++) {
    for (int j = 0; j < 16; j++) {
      I_KH[i][j] = (i == j ? 1.0f : 0.0f) - KH[i][j];
    }
  }

  matrix_mult_16x16(I_KH, P_, P_);
}
