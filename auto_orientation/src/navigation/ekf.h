#ifndef EKF_H
#define EKF_H

#include <stdint.h>
#include <math.h>
#include "covariance_manager.h"

/**
 * Extended Kalman Filter (EKF) for GPS + IMU Fusion
 *
 * Fuses BNO085 IMU (high-frequency orientation/acceleration) with
 * GPS (low-frequency position) to estimate state with GPS dropout handling.
 *
 * State Vector (16 elements):
 *   [0-3]:   Quaternion q = [w, x, y, z]
 *   [4-6]:   Velocity v_ned = [v_north, v_east, v_down] (m/s)
 *   [7-9]:   Position p_ned = [p_north, p_east, p_down] (m)
 *   [10-12]: Accelerometer bias (m/s²)
 *   [13-15]: Gyroscope bias (rad/s)
 *
 * Covariance Matrix P:
 *   16x16 symmetric positive-definite matrix
 *   P[i][j] represents covariance between state[i] and state[j]
 */

class ExtendedKalmanFilter {
 public:
  // ========================================================================
  // Lifecycle Management
  // ========================================================================

  ExtendedKalmanFilter();
  ~ExtendedKalmanFilter();

  /**
   * Initialize the EKF with initial state and covariance.
   *
   * Sets initial state to identity quaternion and zero position/velocity.
   * Initializes covariance P with large uncertainties (diagonal form).
   *
   * @param P_initial Initial covariance (16x16) - typically large diagonal matrix
   * @return true if initialization succeeded
   */
  bool initialize(const Matrix16x16& P_initial);

  /**
   * Reset the EKF to initial state (used for recovery from divergence).
   *
   * @return true if reset succeeded
   */
  bool reset();

  // ========================================================================
  // Main Filter Operations
  // ========================================================================

  /**
   * Predict step: Update state estimate based on IMU measurements.
   *
   * Propagates state forward using:
   * - Gyroscope: Updates quaternion (attitude)
   * - Accelerometer: Updates velocity and position
   * - Time step: dt (seconds)
   *
   * Also propagates covariance forward using Jacobian F.
   *
   * @param gyro_rad_s Gyroscope measurement [wx, wy, wz] in rad/s
   * @param accel_m_s2 Accelerometer measurement [ax, ay, az] in m/s²
   * @param dt_sec Time step in seconds
   * @return true if prediction succeeded
   */
  bool predict(const float gyro_rad_s[3], const float accel_m_s2[3], float dt_sec);

  /**
   * Update step: Correct state estimate based on GPS measurement.
   *
   * Updates state and covariance using GPS position measurement.
   * Implements standard Kalman gain and state update equations.
   *
   * GPS measurement is in NED frame (converted by main.cpp before calling).
   *
   * @param gps_pos_ned_m GPS position in NED [north, east, down] in meters
   * @param gps_accuracy_m Measurement noise standard deviation in meters
   * @return true if update succeeded
   */
  bool update(const float gps_pos_ned_m[3], float gps_accuracy_m);

  // ========================================================================
  // GPS Dropout Handling
  // ========================================================================

  /**
   * Set GPS dropout flag.
   *
   * When dropout flag is true, predict() operates in dead reckoning mode
   * (no GPS updates allowed). Updates can be rejected with this flag set.
   *
   * @param flag true if GPS signal is lost, false if GPS is available
   */
  void set_gps_dropout(bool flag);

  /**
   * Check if GPS is currently providing valid measurements.
   *
   * @return true if GPS has lock (not in dropout state)
   */
  bool is_gps_locked() const;

  /**
   * Get time since last GPS update in milliseconds.
   *
   * @return Milliseconds since last successful update(), or UINT32_MAX if never updated
   */
  uint32_t get_gps_age_ms() const;

  /**
   * Get time in dead reckoning mode (since GPS signal was lost).
   *
   * @return Milliseconds since GPS dropout started, or 0 if GPS is locked
   */
  uint32_t get_dead_reckoning_age_ms() const;

  /**
   * Check if dead reckoning has exceeded maximum timeout.
   *
   * @return true if dead reckoning > 30 seconds (system should mark position invalid)
   */
  bool is_dead_reckoning_expired() const;

  // ========================================================================
  // State Access
  // ========================================================================

  /**
   * Get current state vector (read-only).
   *
   * @return Pointer to 16-element state vector
   */
  const float* get_state() const { return state_; }

  /**
   * Get current covariance matrix (read-only).
   *
   * @return Pointer to 16x16 covariance matrix (row-major)
   */
  const Matrix16x16& get_covariance() const { return P_; }

  /**
   * Get quaternion from state vector.
   *
   * @param q Output: quaternion [w, x, y, z]
   */
  void get_quaternion(float q[4]) const;

  /**
   * Get velocity from state vector.
   *
   * @param v Output: velocity [v_north, v_east, v_down] in m/s
   */
  void get_velocity(float v[3]) const;

  /**
   * Get position from state vector (NED frame).
   *
   * @param p Output: position [p_north, p_east, p_down] in meters
   */
  void get_position(float p[3]) const;

  /**
   * Get standard deviations (sqrt of diagonal covariance).
   *
   * @param std_devs Output: 16-element array of standard deviations
   */
  void get_standard_deviations(float std_devs[16]) const;

  // ========================================================================
  // Diagnostics
  // ========================================================================

  /**
   * Get the last innovation magnitude (measurement residual).
   *
   * Used for EKF health monitoring.
   *
   * @return Innovation magnitude in meters (0 if no update yet)
   */
  float get_last_innovation_magnitude() const { return last_innovation_magnitude_; }

  /**
   * Get consecutive count of dropout prediction updates.
   *
   * @return Number of consecutive predict-only steps during dropout
   */
  uint32_t get_consecutive_dropout_updates() const { return consecutive_dropout_updates_; }

  /**
   * Get covariance trace (total uncertainty).
   *
   * @return Sum of diagonal elements of covariance matrix
   */
  float get_covariance_trace() const;

  /**
   * Get estimated condition number of covariance matrix.
   *
   * @return Condition number (or -1.0f if estimation failed)
   */
  float get_covariance_condition_number() const;

  /**
   * Check if covariance matrix is positive-definite.
   *
   * @return true if matrix is valid
   */
  bool is_covariance_valid() const;

 private:
  // ========================================================================
  // State Storage
  // ========================================================================

  float state_[16];         ///< 16-element state vector
  Matrix16x16 P_;           ///< 16x16 covariance matrix
  Matrix16x16 F_;           ///< State transition Jacobian (computed in predict)
  Matrix16x16 Q_;           ///< Process noise covariance
  // P_temp_ removed (2026-05-19): was a 1 KB scratch buffer used as a
  // ping-pong matrix during predict() and update() covariance arithmetic.
  // Every call site can use a function-scope local instead — same algorithm,
  // ~1 KB SRAM reclaimed per EKF instance. Stack peak rises by ~1 KB during
  // predict()/update() only.
  // See docs/findings/mega_orientation_ram_overflow_diagnosis_2026-05-19.md (F1).

  // ========================================================================
  // GPS Dropout Tracking
  // ========================================================================

  bool gps_dropout_flag_;              ///< true if GPS signal is lost
  uint32_t last_gps_update_ms_;        ///< Timestamp of last successful update
  uint32_t dropout_start_time_ms_;     ///< Timestamp when dropout started
  uint32_t consecutive_dropout_updates_;  ///< Count of predict-only updates during dropout
  float last_innovation_magnitude_;    ///< Magnitude of last innovation (for diagnostics)

  // ========================================================================
  // Private Helper Methods
  // ========================================================================

  /**
   * Initialize Q matrix (process noise) from ekf_config.h constants.
   */
  void initialize_q_matrix_();

  /**
   * Normalize quaternion in state vector.
   */
  void normalize_quaternion_();

  /**
   * Compute state transition Jacobian F for prediction step.
   *
   * F is the Jacobian of the state transition function with respect to state.
   * Depends on gyro, accel, and current quaternion.
   *
   * @param gyro_rad_s Gyroscope measurement [wx, wy, wz]
   * @param accel_m_s2 Accelerometer measurement [ax, ay, az]
   * @param dt_sec Time step
   */
  void compute_f_jacobian_(const float gyro_rad_s[3], const float accel_m_s2[3], float dt_sec);

  /**
   * Compute measurement Jacobian H (3x16 for position measurement).
   *
   * H relates measurements (3D position) to state.
   * For GPS position measurement: H = [0 0 0 0 | 0 0 0 | 1 0 0 | 0 0 0 | 0 0 0]
   *                                            (quaternion) (vel)  (pos)  (accel) (gyro)
   *
   * @param H Output: 3x16 measurement Jacobian (stored as 3 rows of 16 elements each)
   */
  void compute_h_jacobian_(float H[3][16]) const;

  /**
   * Update covariance matrix after prediction: P = F * P * F^T + Q
   *
   * @param F State transition Jacobian
   */
  void update_covariance_predict_(const Matrix16x16& F);

  /**
   * Update covariance matrix after measurement: P = (I - K*H) * P
   *
   * @param K Kalman gain (3x16)
   * @param H Measurement Jacobian (3x16)
   */
  void update_covariance_update_(const float K[3][16], const float H[3][16]);
};

#endif  // EKF_H
