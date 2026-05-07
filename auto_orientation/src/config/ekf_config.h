#ifndef EKF_CONFIG_H
#define EKF_CONFIG_H

#include <stdint.h>

/**
 * EKF Configuration Parameters
 *
 * Contains all tuning constants for the Extended Kalman Filter used in
 * Phase 3 sensor fusion (BNO085 IMU + GPS).
 *
 * Notes:
 * - Process noise (Q) values are for continuous time; scaled by dt in discrete time
 * - Measurement noise (R) values are for discrete measurements
 * - All noise values are variance (σ²), not standard deviation (σ)
 */

// ============================================================================
// PROCESS NOISE (Q Matrix Diagonal) - Continuous Time
// ============================================================================

/**
 * Quaternion (orientation) white noise
 * BNO085 IMU specification: ~0.005 rad/s heading drift
 * This represents uncertainty in angular velocity measurement
 */
static constexpr float Q_GYRO_NOISE = 0.005f;  // rad/s

/**
 * Linear acceleration white noise
 * Typical accelerometer noise: ~0.05 m/s² (50 mGal)
 */
static constexpr float Q_ACCEL_NOISE = 0.05f;  // m/s²

/**
 * Accelerometer bias drift rate
 * Very small to represent slow bias evolution
 * Scaled by dt² in state transition
 */
static constexpr float Q_ACCEL_BIAS_NOISE = 1e-8f;  // m²/s⁶

/**
 * Gyroscope bias drift rate (if needed)
 * Typically small for high-end IMUs
 */
static constexpr float Q_GYRO_BIAS_NOISE = 1e-10f;  // rad²/s⁴

// ============================================================================
// MEASUREMENT NOISE (R Matrix Diagonal)
// ============================================================================

/**
 * GPS position measurement noise scaling
 * Multiply HDOP (Horizontal Dilution of Precision) by this factor
 * to get measurement covariance for position
 *
 * Example: HDOP=2.0 -> R_position = 2.0 * 1.5 = 3.0 m²
 */
static constexpr float R_POSITION_HDOP_FACTOR = 1.5f;

/**
 * Minimum GPS position measurement noise
 * Even with excellent HDOP, use at least this much uncertainty
 * Prevents over-trust in potentially faulty fixes
 */
static constexpr float R_POSITION_MIN = 0.5f;  // m²

/**
 * Maximum GPS position measurement noise
 * For very poor signal, cap the uncertainty at this value
 * Prevents filter divergence from extreme HDOP values
 */
static constexpr float R_POSITION_MAX = 100.0f;  // m²

/**
 * GPS altitude measurement noise
 * Typically 1.5-2x horizontal accuracy due to fewer satellites in vertical plane
 */
static constexpr float R_ALTITUDE_HDOP_FACTOR = 2.0f;

/**
 * Minimum altitude measurement noise
 */
static constexpr float R_ALTITUDE_MIN = 1.0f;  // m²

/**
 * Maximum altitude measurement noise
 */
static constexpr float R_ALTITUDE_MAX = 200.0f;  // m²

// ============================================================================
// NUMERICAL STABILITY PARAMETERS
// ============================================================================

/**
 * Minimum allowed eigenvalue for covariance matrix P
 * Prevents singular or near-singular matrices
 * Applied during covariance update
 */
static constexpr float COVARIANCE_CLAMP_MIN = 1e-12f;

/**
 * SVD singular value threshold
 * Values below this are treated as zero in SVD decomposition
 * Used for pseudo-inverse calculations
 */
static constexpr float SVD_SINGULAR_THRESHOLD = 1e-10f;

/**
 * Maximum condition number for numerical stability check
 * cond(P) = λ_max / λ_min
 * Larger values indicate potential numerical issues
 */
static constexpr float COVARIANCE_MAX_CONDITION = 1e10f;

// ============================================================================
// PHYSICAL CONSTANTS
// ============================================================================

/**
 * Gravitational acceleration (m/s²)
 * Used in velocity update: v_new = v + dt * (R(q) * accel + [0, 0, g])
 * Local value for mid-latitudes (Germany/USA)
 *
 * Variations with latitude:
 * - Equator: ~9.78 m/s²
 * - 45°N: ~9.806 m/s²
 * - Poles: ~9.83 m/s²
 */
static constexpr float EARTH_GRAVITY_MPS2 = 9.81f;

// ============================================================================
// KALMAN FILTER GAIN CONSTRAINTS
// ============================================================================

/**
 * Maximum innovation (measurement residual) before reset
 * If |innovation| exceeds this, filter treats it as outlier
 * Prevents divergence from bad GPS fixes
 *
 * ~10σ confidence bound for Gaussian innovation
 */
static constexpr float MAX_INNOVATION_POSITION = 50.0f;  // meters

/**
 * Maximum allowed Kalman gain component
 * Prevents excessive correction from any single measurement
 */
static constexpr float MAX_KALMAN_GAIN = 10.0f;

// ============================================================================
// JSON OUTPUT CONFIGURATION (Phase 3)
// ============================================================================

/**
 * Enable detailed JSON output with full fused state
 * When enabled, output includes quaternion, Euler angles, velocity, position,
 * uncertainty, and GPS status
 */
static constexpr bool ENABLE_DETAILED_JSON = true;

/**
 * Enable raw GPS vs fused position comparison JSON
 * Useful for tuning and validation
 * Shows difference between raw GPS measurement and fused estimate
 */
static constexpr bool ENABLE_COMPARISON_JSON = true;

/**
 * Enable full uncertainty/covariance output
 * Includes standard deviation for each state dimension
 */
static constexpr bool ENABLE_UNCERTAINTY_JSON = true;

// ============================================================================
// INITIAL STATE UNCERTAINTY (P matrix diagonal at t=0)
// ============================================================================

/**
 * Initial attitude uncertainty
 * Represents initial confidence in quaternion estimate
 * Higher value = less confident
 */
static constexpr float INITIAL_UNCERTAINTY_ATTITUDE = 1.0f;  // rad²

/**
 * Initial velocity uncertainty
 * Higher value = less confident in initial velocity estimate
 */
static constexpr float INITIAL_UNCERTAINTY_VELOCITY = 100.0f;  // m²/s²

/**
 * Initial position uncertainty
 * High value since coordinate frame depends on first GPS fix
 */
static constexpr float INITIAL_UNCERTAINTY_POSITION = 10000.0f;  // m²

/**
 * Initial accelerometer bias uncertainty
 * Bias can drift, so starts with moderate confidence
 */
static constexpr float INITIAL_UNCERTAINTY_ACCEL_BIAS = 0.25f;  // m²/s⁴

/**
 * Initial gyroscope bias uncertainty
 */
static constexpr float INITIAL_UNCERTAINTY_GYRO_BIAS = 0.01f;  // rad²/s⁴

// ============================================================================
// TUNING PRESETS
// ============================================================================

/**
 * Tuning preset selector (1-3):
 * 1 = Balanced (default, good for most scenarios)
 * 2 = Conservative (higher process noise, trusts measurements more)
 * 3 = Aggressive (lower process noise, trusts model more)
 *
 * Can be set to 0 for custom tuning with manual Q and R matrices
 */
static constexpr uint8_t TUNING_PRESET = 1;

// ============================================================================
// PRESET-DEPENDENT TUNING MULTIPLIERS
// ============================================================================

/**
 * Process noise multiplier based on TUNING_PRESET
 * Conservative (2): 2.0x - More measurement trust
 * Balanced (1): 1.0x - Default
 * Aggressive (3): 0.5x - More model trust
 */
static constexpr float Q_MULTIPLIER = (TUNING_PRESET == 2) ? 2.0f :
                                       (TUNING_PRESET == 3) ? 0.5f : 1.0f;

/**
 * Measurement noise multiplier
 * Conservative (2): 0.8x - Higher measurement covariance (trust less)
 * Balanced (1): 1.0x - Default
 * Aggressive (3): 1.5x - Lower measurement covariance (trust more)
 */
static constexpr float R_MULTIPLIER = (TUNING_PRESET == 2) ? 0.8f :
                                       (TUNING_PRESET == 3) ? 1.5f : 1.0f;

// ============================================================================
// GPS-DEPENDENT MEASUREMENT NOISE
// ============================================================================

/**
 * Enable dynamic R matrix scaling with HDOP
 * When true: R_actual = R_base * HDOP
 * When false: R_actual = R_base (constant)
 */
static constexpr bool R_SCALE_WITH_HDOP = true;

/**
 * Base position measurement noise (when HDOP=1.0)
 * Represents ideal GPS conditions
 */
static constexpr float R_BASE_POSITION_M = 1.0f;  // meters (σ)

/**
 * Base altitude measurement noise (when HDOP=1.0)
 */
static constexpr float R_BASE_ALTITUDE_M = 2.0f;  // meters (σ)

// ============================================================================
// STATE VECTOR ORGANIZATION (Reference)
// ============================================================================

/**
 * EKF State Vector (16 elements):
 *
 * [0-3]:   Quaternion q = [w, x, y, z]
 * [4-6]:   Velocity v_ned = [v_north, v_east, v_down] (m/s)
 * [7-9]:   Position p_ned = [p_north, p_east, p_down] (meters)
 * [10-12]: Accelerometer bias (m/s²)
 * [13-15]: Gyroscope bias (rad/s)
 *
 * Matrices:
 * - F: 16x16 state transition Jacobian
 * - Q: 16x16 process noise covariance
 * - H: 3x16 measurement Jacobian (GPS position measurement)
 * - R: 3x3 measurement noise covariance (diagonal)
 * - P: 16x16 state covariance
 */

#endif  // EKF_CONFIG_H
