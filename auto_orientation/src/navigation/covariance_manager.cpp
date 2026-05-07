#include <Arduino.h>
#include "covariance_manager.h"
#include <math.h>

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * Simple Cholesky decomposition attempt (LLT).
 * Returns true if matrix is positive-definite (Cholesky succeeds).
 * Uses only first few iterations to avoid overflow.
 */
static bool try_cholesky_decomposition(const Matrix16x16& P) {
  // Allocate temporary matrix for Cholesky
  float L[16][16];
  memcpy(L, P, sizeof(Matrix16x16));

  // Try to perform Cholesky decomposition
  for (int i = 0; i < 16; i++) {
    // Diagonal element: L[i][i] = sqrt(A[i][i] - sum(L[i][j]^2 for j < i))
    float diag_sum = 0.0f;
    for (int j = 0; j < i; j++) {
      diag_sum += L[i][j] * L[i][j];
    }

    float diag_val = L[i][i] - diag_sum;
    if (diag_val <= 1e-12f) {
      // Not positive-definite
      return false;
    }

    L[i][i] = sqrt(diag_val);

    // Lower triangular elements
    for (int k = i + 1; k < 16; k++) {
      float sum = 0.0f;
      for (int j = 0; j < i; j++) {
        sum += L[k][j] * L[i][j];
      }
      L[k][i] = (L[k][i] - sum) / L[i][i];
    }
  }

  return true;
}

/**
 * Copy a 16x16 matrix
 */
static void matrix_copy(const Matrix16x16& src, Matrix16x16& dst) {
  memcpy(dst, src, sizeof(Matrix16x16));
}

/**
 * Zero a 16x16 matrix
 */
static void matrix_zero(Matrix16x16& M) {
  memset(M, 0, sizeof(Matrix16x16));
}

// ============================================================================
// Covariance Symmetry
// ============================================================================

void enforce_covariance_symmetry(const Matrix16x16& P_in, Matrix16x16& P_out) {
  // P_new = 0.5 * (P + P^T)
  for (int i = 0; i < 16; i++) {
    for (int j = 0; j < 16; j++) {
      P_out[i][j] = 0.5f * (P_in[i][j] + P_in[j][i]);
    }
  }
}

// ============================================================================
// Positive Definiteness Check
// ============================================================================

bool is_positive_definite(const Matrix16x16& P, float epsilon) {
  // Check 1: All diagonal elements must be positive
  for (int i = 0; i < 16; i++) {
    if (P[i][i] <= epsilon) {
      return false;
    }
  }

  // Check 2: Try Cholesky decomposition
  return try_cholesky_decomposition(P);
}

// ============================================================================
// Eigenvalue Clamping
// ============================================================================

bool clamp_negative_eigenvalues(const Matrix16x16& P_in, Matrix16x16& P_out, float epsilon) {
  // Check if input is already positive-definite
  if (is_positive_definite(P_in, epsilon)) {
    matrix_copy(P_in, P_out);
    return true;
  }

  // Start with a copy
  matrix_copy(P_in, P_out);

  // Step 1: Clamp all diagonal elements to minimum epsilon
  for (int i = 0; i < 16; i++) {
    if (P_out[i][i] < epsilon) {
      P_out[i][i] = epsilon;
    }
  }

  // Step 2: Try to reconstruct a positive-definite matrix
  // by enforcing diagonal dominance and symmetry
  for (int i = 0; i < 16; i++) {
    for (int j = 0; j < 16; j++) {
      if (i == j) {
        // Diagonal is already clamped
        continue;
      }

      // Limit off-diagonal elements to prevent violation of
      // diagonal dominance and positive-definiteness
      float diag_avg = 0.5f * (P_out[i][i] + P_out[j][j]);
      float max_off_diag = 0.99f * diag_avg;

      if (abs(P_out[i][j]) > max_off_diag) {
        P_out[i][j] = (P_out[i][j] > 0) ? max_off_diag : -max_off_diag;
      }
    }
  }

  // Step 3: Enforce symmetry
  for (int i = 0; i < 16; i++) {
    for (int j = i + 1; j < 16; j++) {
      float avg = 0.5f * (P_out[i][j] + P_out[j][i]);
      P_out[i][j] = avg;
      P_out[j][i] = avg;
    }
  }

  // Step 4: Verify result
  return is_positive_definite(P_out, epsilon);
}

// ============================================================================
// Diagnostic Functions
// ============================================================================

float compute_innovation_magnitude(const Vector3& innovation) {
  float mag_sq = innovation[0] * innovation[0] +
                 innovation[1] * innovation[1] +
                 innovation[2] * innovation[2];

  if (!isfinite(mag_sq) || mag_sq < 0.0f) {
    return NAN;
  }

  return sqrt(mag_sq);
}

float compute_covariance_trace(const Matrix16x16& P) {
  float trace = 0.0f;
  for (int i = 0; i < 16; i++) {
    if (isfinite(P[i][i])) {
      trace += P[i][i];
    }
  }
  return trace;
}

float get_max_covariance_diagonal(const Matrix16x16& P) {
  float max_val = 0.0f;
  for (int i = 0; i < 16; i++) {
    if (P[i][i] > max_val && isfinite(P[i][i])) {
      max_val = P[i][i];
    }
  }
  return max_val;
}

float estimate_condition_number(const Matrix16x16& P) {
  // Find maximum and minimum diagonal elements
  float max_diag = 1e-20f;
  float min_diag = 1e20f;

  for (int i = 0; i < 16; i++) {
    if (isfinite(P[i][i]) && P[i][i] > 0.0f) {
      if (P[i][i] > max_diag) {
        max_diag = P[i][i];
      }
      if (P[i][i] < min_diag) {
        min_diag = P[i][i];
      }
    }
  }

  if (min_diag < 1e-20f || !isfinite(max_diag / min_diag)) {
    return -1.0f;  // Computation failed
  }

  return max_diag / min_diag;
}
