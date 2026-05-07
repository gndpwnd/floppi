#ifndef COVARIANCE_MANAGER_H
#define COVARIANCE_MANAGER_H

#include <math.h>
#include <string.h>

/**
 * Covariance Manager: Numerical Stability Utilities for EKF
 *
 * Provides functions to ensure the EKF covariance matrix P remains
 * symmetric and positive-definite despite numerical errors from
 * matrix operations.
 *
 * All matrices are assumed to be 16x16 (4-dimensional state vector):
 * - Quaternion [0-3]
 * - Velocity [4-6]
 * - Position [7-9]
 * - Accel bias [10-12]
 * - Gyro bias [13-15]
 */

// ============================================================================
// Data Structures
// ============================================================================

/**
 * 16x16 matrix stored in row-major order (256 float elements)
 */
using Matrix16x16 = float[16][16];

/**
 * 3D vector (e.g., innovation or position)
 */
using Vector3 = float[3];

// ============================================================================
// Covariance Symmetry and Positive Definiteness
// ============================================================================

/**
 * Enforce covariance matrix symmetry.
 *
 * The EKF update equations can introduce small asymmetries due to
 * floating-point rounding errors. This function corrects them:
 * P_new = 0.5 * (P + P^T)
 *
 * Should be called after every update() step.
 *
 * @param P_in Input 16x16 covariance matrix (row-major)
 * @param P_out Output 16x16 symmetric covariance matrix
 */
void enforce_covariance_symmetry(const Matrix16x16& P_in, Matrix16x16& P_out);

/**
 * Check if a matrix is positive-definite (all eigenvalues > 0).
 *
 * Uses simplified checks for numerical stability:
 * 1. All diagonal elements must be positive
 * 2. Attempts Cholesky decomposition (if succeeds, matrix is PD)
 *
 * @param P 16x16 covariance matrix (row-major)
 * @param epsilon Minimum allowed eigenvalue (default 1e-12)
 * @return true if matrix appears positive-definite
 */
bool is_positive_definite(const Matrix16x16& P, float epsilon = 1e-12f);

/**
 * Clamp negative eigenvalues to a minimum value.
 *
 * If the covariance matrix becomes ill-conditioned (eigenvalues too small),
 * this function reconstructs it with clamped eigenvalues to maintain
 * numerical stability.
 *
 * Simplified approach using diagonal dominance check:
 * 1. Check diagonal elements
 * 2. If any diagonal element < epsilon, clamp to epsilon
 * 3. Use Cholesky decomposition fallback if available
 *
 * @param P_in Input 16x16 covariance matrix
 * @param P_out Output 16x16 positive-definite covariance matrix
 * @param epsilon Minimum allowed eigenvalue (default 1e-12)
 * @return true if clamping succeeded, false if matrix was already invalid
 */
bool clamp_negative_eigenvalues(const Matrix16x16& P_in, Matrix16x16& P_out, float epsilon = 1e-12f);

// ============================================================================
// Diagnostic Functions
// ============================================================================

/**
 * Compute innovation magnitude for EKF health monitoring.
 *
 * Innovation is the difference between measured and predicted states:
 * y = z - H * x_predicted
 *
 * Magnitude = sqrt(innovation^T * innovation)
 *
 * @param innovation 3D innovation vector (position residual)
 * @return Magnitude in meters (or NaN if input is invalid)
 */
float compute_innovation_magnitude(const Vector3& innovation);

/**
 * Compute trace of covariance matrix (total uncertainty).
 *
 * Trace = sum of diagonal elements = sum of variances
 *
 * Lower trace generally indicates lower total uncertainty.
 * Can be used to detect divergence or filter health.
 *
 * @param P 16x16 covariance matrix
 * @return Sum of diagonal elements (trace)
 */
float compute_covariance_trace(const Matrix16x16& P);

/**
 * Get maximum diagonal element of covariance (max variance).
 *
 * Useful for detecting which state is most uncertain.
 *
 * @param P 16x16 covariance matrix
 * @return Maximum diagonal element (variance)
 */
float get_max_covariance_diagonal(const Matrix16x16& P);

/**
 * Get the condition number of the covariance matrix.
 *
 * Condition number = max_eigenvalue / min_eigenvalue
 * High condition numbers indicate numerical instability.
 *
 * Simplified estimate using diagonal ratios.
 *
 * @param P 16x16 covariance matrix
 * @return Estimated condition number (or -1.0f if computation fails)
 */
float estimate_condition_number(const Matrix16x16& P);

#endif  // COVARIANCE_MANAGER_H
