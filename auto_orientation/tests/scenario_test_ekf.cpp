/**
 * Comprehensive EKF Scenario Tests - Phase 3 Validation
 *
 * Real-world scenario tests for Extended Kalman Filter GPS-IMU fusion.
 * Tests validate correct state propagation, measurement handling, GPS dropout recovery,
 * numerical stability, and sensor fusion quality.
 *
 * Test Count: 15+ scenario-based tests
 * Target: 100% pass rate, no NaN/Inf, positive-definite covariance throughout
 *
 * Framework: Simple assert()-based testing with printf() output.
 * All test data is deterministic (no random values).
 * Known-good reference values used for validation.
 */

#include <iostream>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <vector>

// Include EKF and related libraries
#include "../src/navigation/ekf.h"
#include "../src/math/quaternion.h"
#include "../src/navigation/coordinate_frame.h"

// ============================================================================
// TEST FRAMEWORK
// ============================================================================

int test_count = 0;
int pass_count = 0;

void test_assert(bool condition, const std::string& test_name, const std::string& message = "") {
    test_count++;
    if (condition) {
        pass_count++;
        printf("  ✓ %s\n", test_name.c_str());
    } else {
        printf("  ✗ %s\n", test_name.c_str());
        if (!message.empty()) {
            printf("    Reason: %s\n", message.c_str());
        }
    }
}

void print_summary() {
    printf("\n%s\n", std::string(70, '=').c_str());
    printf("SCENARIO TEST SUMMARY\n");
    printf("%s\n", std::string(70, '=').c_str());
    printf("Passed: %d/%d (%.1f%%)\n", pass_count, test_count, 100.0f * pass_count / test_count);
    if (pass_count == test_count) {
        printf("✓ All scenario tests passed!\n");
    } else {
        printf("✗ %d scenario test(s) failed\n", (test_count - pass_count));
    }
    printf("%s\n\n", std::string(70, '=').c_str());
}

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

namespace {

constexpr float EPSILON = 1e-6f;
constexpr float GRAVITY = 9.81f;

bool is_finite(float value) {
    return !std::isnan(value) && !std::isinf(value);
}

bool is_valid_quaternion(const Quaternion& q) {
    return is_finite(q.w) && is_finite(q.x) && is_finite(q.y) && is_finite(q.z) &&
           std::fabs(q.magnitude() - 1.0f) < 0.1f;  // Allow for some denormalization
}

bool is_valid_state(float state[16]) {
    for (int i = 0; i < 16; i++) {
        if (!is_finite(state[i])) return false;
    }
    return true;
}

bool is_valid_covariance(float P[256]) {
    // Check diagonal is positive
    for (int i = 0; i < 16; i++) {
        if (!is_finite(P[i * 16 + i]) || P[i * 16 + i] < 0) return false;
    }
    // Check symmetry (spot check)
    for (int i = 0; i < 16; i++) {
        for (int j = i + 1; j < 16; j++) {
            if (!is_finite(P[i * 16 + j]) || !is_finite(P[j * 16 + i])) return false;
            float diff = std::fabs(P[i * 16 + j] - P[j * 16 + i]);
            if (diff > 1e-4f) return false;  // Allow small numerical error
        }
    }
    return true;
}

char error_buffer[256];

std::string format_error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(error_buffer, sizeof(error_buffer), fmt, args);
    va_end(args);
    return std::string(error_buffer);
}

}  // namespace

// ============================================================================
// SCENARIO 1: Static (No Motion)
// ============================================================================

void scenario_1_static_no_motion() {
    printf("\n--- Scenario 1: Static (No Motion) ---\n");

    ExtendedKalmanFilter ekf;
    ekf.reset();

    // Initialize at origin with identity orientation
    Quaternion q_identity(1.0f, 0.0f, 0.0f, 0.0f);
    ekf.initialize(q_identity, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);

    // Setup coordinate frame
    CoordinateFrame frame;
    frame.set_reference(47.3667, 11.1833, 520.0);  // Munich reference

    float initial_uncertainty = ekf.getPositionUncertainty();
    test_assert(initial_uncertainty > 0, "Scenario 1: Initial uncertainty > 0",
                format_error("Initial uncertainty: %.2f", initial_uncertainty));

    // Run 10 GPS update cycles without motion
    float dt = 1.0f;  // 1 Hz GPS
    for (int i = 0; i < 10; i++) {
        // No IMU input (stationary)
        ekf.predict(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, GRAVITY, dt);

        // GPS measurement at same location
        ekf.update(47.3667, 11.1833, 520.0);

        // Verify state validity
        float state[16];
        ekf.getState(state);
        test_assert(is_valid_state(state),
                    format_error("Scenario 1: State valid after cycle %d", i),
                    "State contains NaN/Inf");
    }

    float final_uncertainty = ekf.getPositionUncertainty();
    test_assert(final_uncertainty < 5.0f,
                format_error("Scenario 1: Position uncertainty < 5m (got %.2f m)", final_uncertainty),
                "Uncertainty failed to converge");

    test_assert(final_uncertainty < initial_uncertainty,
                format_error("Scenario 1: Uncertainty decreased (%.2f -> %.2f)", initial_uncertainty, final_uncertainty),
                "Uncertainty should decrease with measurements");

    // Verify covariance still positive definite
    float P[256];
    ekf.getCovariance(P);
    test_assert(ekf.isCovariancePositiveDefinite(),
                "Scenario 1: Covariance positive definite");
}

// ============================================================================
// SCENARIO 2: Constant Velocity
// ============================================================================

void scenario_2_constant_velocity() {
    printf("\n--- Scenario 2: Constant Velocity ---\n");

    ExtendedKalmanFilter ekf;
    ekf.reset();

    Quaternion q_identity(1.0f, 0.0f, 0.0f, 0.0f);
    ekf.initialize(q_identity, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);

    CoordinateFrame frame;
    frame.set_reference(47.3667, 11.1833, 520.0);

    float dt = 0.1f;  // 10 Hz IMU
    float target_velocity = 1.0f;  // 1 m/s north
    float acceleration = 0.2f;  // 0.2 m/s² for 5 steps to reach 1 m/s

    // Phase 1: Acceleration for 1 second
    for (int i = 0; i < 10; i++) {
        ekf.predict(0.0f, 0.0f, 0.0f, acceleration, 0.0f, GRAVITY, dt);
    }

    // Phase 2: Constant velocity for 10 seconds (100 steps)
    for (int i = 0; i < 100; i++) {
        ekf.predict(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, GRAVITY, dt);

        // GPS update every 10 steps (1 Hz)
        if (i % 10 == 0) {
            float pn, pe, pd;
            ekf.getPosition(pn, pe, pd);

            // Convert NED position to GPS coordinates
            // Approximate: 1 meter north ≈ 0.000009 degrees
            double lat_offset = pn / 111111.0;
            ekf.update(47.3667 + lat_offset, 11.1833, 520.0);
        }
    }

    // Verify final state
    float v_n, v_e, v_d;
    ekf.getVelocity(v_n, v_e, v_d);
    test_assert(std::fabs(v_n - target_velocity) < 0.3f,
                format_error("Scenario 2: Velocity ≈ 1 m/s (got %.2f)", v_n),
                "Velocity should be approximately 1 m/s north");

    float pn, pe, pd;
    ekf.getPosition(pn, pe, pd);
    test_assert(pn > 5.0f,  // Should have moved north
                format_error("Scenario 2: Position moved north (got %.2f m)", pn),
                "Position should increase");

    float state[16];
    ekf.getState(state);
    test_assert(is_valid_state(state),
                "Scenario 2: State valid");
}

// ============================================================================
// SCENARIO 3: GPS Dropout & Recovery
// ============================================================================

void scenario_3_gps_dropout_recovery() {
    printf("\n--- Scenario 3: GPS Dropout & Recovery ---\n");

    ExtendedKalmanFilter ekf;
    ekf.reset();

    Quaternion q_identity(1.0f, 0.0f, 0.0f, 0.0f);
    ekf.initialize(q_identity, 0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);  // Initial velocity

    CoordinateFrame frame;
    frame.set_reference(47.3667, 11.1833, 520.0);

    float dt = 0.1f;

    // Phase 1: GPS locked - 5 update cycles
    printf("  Phase 1: GPS locked\n");
    for (int i = 0; i < 50; i++) {
        ekf.predict(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, GRAVITY, dt);

        if (i % 10 == 0) {
            float pn, pe, pd;
            ekf.getPosition(pn, pe, pd);
            double lat_offset = pn / 111111.0;
            ekf.update(47.3667 + lat_offset, 11.1833, 520.0);
        }
    }

    float unc_before_dropout = ekf.getPositionUncertainty();

    // Phase 2: GPS dropout - 10 seconds of dead reckoning
    printf("  Phase 2: GPS dropout (10 seconds)\n");
    ekf.setGpsDropout(true);
    test_assert(!ekf.isGpsValid(), "Scenario 3: GPS dropout set");

    for (int i = 0; i < 100; i++) {
        ekf.predict(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, GRAVITY, dt);
        // No update calls during dropout
    }

    float unc_during_dropout = ekf.getPositionUncertainty();
    test_assert(unc_during_dropout > unc_before_dropout,
                format_error("Scenario 3: Uncertainty grew during dropout (%.2f -> %.2f)",
                            unc_before_dropout, unc_during_dropout),
                "Uncertainty should increase without measurements");

    // Phase 3: GPS recovery - 5 update cycles
    printf("  Phase 3: GPS recovery\n");
    ekf.setGpsDropout(false);
    test_assert(ekf.isGpsValid(), "Scenario 3: GPS re-locked");

    for (int i = 0; i < 50; i++) {
        ekf.predict(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, GRAVITY, dt);

        if (i % 10 == 0) {
            float pn, pe, pd;
            ekf.getPosition(pn, pe, pd);
            double lat_offset = pn / 111111.0;
            ekf.update(47.3667 + lat_offset, 11.1833, 520.0);
        }
    }

    float unc_after_recovery = ekf.getPositionUncertainty();
    test_assert(unc_after_recovery < unc_during_dropout,
                format_error("Scenario 3: Uncertainty recovered (%.2f -> %.2f)",
                            unc_during_dropout, unc_after_recovery),
                "Uncertainty should decrease after re-lock");

    float state[16];
    ekf.getState(state);
    test_assert(is_valid_state(state) && ekf.isCovarianceValid(),
                "Scenario 3: State and covariance valid");
}

// ============================================================================
// SCENARIO 4: Filter Smoothing
// ============================================================================

void scenario_4_filter_smoothing() {
    printf("\n--- Scenario 4: Filter Smoothing ---\n");

    ExtendedKalmanFilter ekf;
    ekf.reset();

    Quaternion q_identity(1.0f, 0.0f, 0.0f, 0.0f);
    ekf.initialize(q_identity, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);

    CoordinateFrame frame;
    frame.set_reference(47.3667, 11.1833, 520.0);

    // Simulate noisy GPS (±5 meters)
    std::vector<float> gps_noise = {
        2.5f, -3.2f, 4.1f, -2.8f, 3.5f, -4.2f, 2.1f, -3.8f,
        3.3f, -2.5f, 4.0f, -3.1f, 2.9f, -4.3f, 3.7f, -2.9f
    };

    std::vector<float> ekf_positions;
    std::vector<float> gps_positions;

    float dt = 1.0f;
    for (size_t i = 0; i < gps_noise.size(); i++) {
        ekf.predict(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, GRAVITY, dt);

        float pn, pe, pd;
        ekf.getPosition(pn, pe, pd);
        ekf_positions.push_back(pn);

        // GPS with noise
        float gps_pn = gps_noise[i];
        gps_positions.push_back(gps_pn);

        double lat_offset = gps_pn / 111111.0;
        ekf.update(47.3667 + lat_offset, 11.1833, 520.0);
    }

    // Compute variance of raw GPS and filtered positions
    float gps_mean = 0.0f, ekf_mean = 0.0f;
    for (size_t i = 0; i < gps_positions.size(); i++) {
        gps_mean += gps_positions[i];
        ekf_mean += ekf_positions[i];
    }
    gps_mean /= gps_positions.size();
    ekf_mean /= ekf_positions.size();

    float gps_var = 0.0f, ekf_var = 0.0f;
    for (size_t i = 0; i < gps_positions.size(); i++) {
        gps_var += (gps_positions[i] - gps_mean) * (gps_positions[i] - gps_mean);
        ekf_var += (ekf_positions[i] - ekf_mean) * (ekf_positions[i] - ekf_mean);
    }
    gps_var /= gps_positions.size();
    ekf_var /= ekf_positions.size();

    float gps_std = std::sqrt(gps_var);
    float ekf_std = std::sqrt(ekf_var);
    float variance_reduction = (gps_std - ekf_std) / gps_std;

    test_assert(ekf_std < gps_std,
                format_error("Scenario 4: EKF smoother (GPS std=%.2f, EKF std=%.2f)", gps_std, ekf_std),
                "Filtered signal should have lower variance than raw GPS");

    test_assert(variance_reduction > 0.3f,
                format_error("Scenario 4: Variance reduced >30%% (got %.1f%%)", variance_reduction * 100),
                "Should achieve >30% variance reduction");
}

// ============================================================================
// SCENARIO 5: High Acceleration
// ============================================================================

void scenario_5_high_acceleration() {
    printf("\n--- Scenario 5: High Acceleration ---\n");

    ExtendedKalmanFilter ekf;
    ekf.reset();

    Quaternion q_identity(1.0f, 0.0f, 0.0f, 0.0f);
    ekf.initialize(q_identity, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);

    float dt = 0.1f;
    float acceleration = 2.0f;  // 2 m/s²
    float expected_velocity = acceleration * 5.0f;  // 10 m/s after 5 seconds
    float expected_position = 0.5f * acceleration * 5.0f * 5.0f;  // 25 meters

    // 50 steps = 5 seconds
    for (int i = 0; i < 50; i++) {
        ekf.predict(0.0f, 0.0f, 0.0f, acceleration, 0.0f, GRAVITY, dt);
    }

    float v_n, v_e, v_d;
    ekf.getVelocity(v_n, v_e, v_d);

    test_assert(std::fabs(v_n - expected_velocity) < 1.0f,
                format_error("Scenario 5: Final velocity ≈ 10 m/s (got %.2f)", v_n),
                "Velocity should match physics");

    float pn, pe, pd;
    ekf.getPosition(pn, pe, pd);

    test_assert(std::fabs(pn - expected_position) < 2.0f,
                format_error("Scenario 5: Final position ≈ 25m (got %.2f)", pn),
                "Position should match physics");

    float state[16];
    ekf.getState(state);
    test_assert(is_valid_state(state),
                "Scenario 5: State valid");
}

// ============================================================================
// SCENARIO 6: Direction Change (90° Turn)
// ============================================================================

void scenario_6_direction_change() {
    printf("\n--- Scenario 6: Direction Change (90° Turn) ---\n");

    ExtendedKalmanFilter ekf;
    ekf.reset();

    Quaternion q_identity(1.0f, 0.0f, 0.0f, 0.0f);
    ekf.initialize(q_identity, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);  // Moving north

    float dt = 0.1f;

    // Move north for 5 seconds
    for (int i = 0; i < 50; i++) {
        ekf.predict(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, GRAVITY, dt);
    }

    float pn1, pe1, pd1;
    ekf.getPosition(pn1, pe1, pd1);
    test_assert(pn1 > 4.0f, "Scenario 6: Moved north");

    // Apply 90-degree yaw rotation (rotate to move east)
    // Quaternion for 90-degree rotation around Z (yaw)
    float angle = M_PI / 2.0f;
    Quaternion q_turn(std::cos(angle / 2.0f), 0.0f, 0.0f, std::sin(angle / 2.0f));
    ekf.setAttitude(q_turn);

    // Change velocity to east
    ekf.setVelocity(0.0f, 1.0f, 0.0f);

    // Move east for 5 seconds
    for (int i = 0; i < 50; i++) {
        ekf.predict(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, GRAVITY, dt);
    }

    float pn2, pe2, pd2;
    ekf.getPosition(pn2, pe2, pd2);

    test_assert(std::fabs(pn2 - pn1) < 1.0f,
                format_error("Scenario 6: North position stable (was %.2f, now %.2f)", pn1, pn2),
                "North position should stay roughly constant");

    test_assert(pe2 > 4.0f,
                format_error("Scenario 6: Moved east (%.2f m)", pe2),
                "Should move east after turn");

    float state[16];
    ekf.getState(state);
    test_assert(is_valid_state(state) && is_valid_quaternion(ekf.getAttitude()),
                "Scenario 6: State and quaternion valid");
}

// ============================================================================
// SCENARIO 7: Extended GPS Outage (>30 seconds)
// ============================================================================

void scenario_7_extended_gps_outage() {
    printf("\n--- Scenario 7: Extended GPS Outage (>30 seconds) ---\n");

    ExtendedKalmanFilter ekf;
    ekf.reset();

    Quaternion q_identity(1.0f, 0.0f, 0.0f, 0.0f);
    ekf.initialize(q_identity, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);

    float dt = 0.1f;

    // Initial GPS lock - 5 seconds
    for (int i = 0; i < 50; i++) {
        ekf.predict(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, GRAVITY, dt);
        if (i % 10 == 0) {
            ekf.update(47.3667, 11.1833, 520.0);
        }
    }

    // Extended dropout - 35 seconds (350 steps at 10 Hz)
    ekf.setGpsDropout(true);
    for (int i = 0; i < 350; i++) {
        ekf.predict(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, GRAVITY, dt);
    }

    float dropout_duration = ekf.getGpsDropoutDuration();
    test_assert(dropout_duration > 30.0f,
                format_error("Scenario 7: Dropout duration > 30s (got %.1f)", dropout_duration),
                "Dropout duration should be tracked");

    float unc_during_outage = ekf.getPositionUncertainty();

    // Recovery - 10 seconds of GPS
    ekf.setGpsDropout(false);
    for (int i = 0; i < 100; i++) {
        ekf.predict(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, GRAVITY, dt);
        if (i % 10 == 0) {
            ekf.update(47.3667, 11.1833, 520.0);
        }
    }

    float unc_after_recovery = ekf.getPositionUncertainty();
    test_assert(unc_after_recovery < unc_during_outage,
                format_error("Scenario 7: Uncertainty recovered (%.2f -> %.2f)",
                            unc_during_outage, unc_after_recovery),
                "Should recover after extended outage");

    float state[16];
    ekf.getState(state);
    test_assert(is_valid_state(state) && ekf.isCovarianceValid(),
                "Scenario 7: State valid after extended outage");
}

// ============================================================================
// SCENARIO 8: Noisy IMU + GPS
// ============================================================================

void scenario_8_noisy_imu_gps() {
    printf("\n--- Scenario 8: Noisy IMU + GPS ---\n");

    ExtendedKalmanFilter ekf;
    ekf.reset();

    Quaternion q_identity(1.0f, 0.0f, 0.0f, 0.0f);
    ekf.initialize(q_identity, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);

    // Simulate noisy measurements
    float accel_noise[] = {0.05f, -0.08f, 0.03f, -0.06f, 0.07f, -0.04f};
    float gps_noise[] = {2.1f, -2.8f, 3.2f, -1.9f, 2.5f, -3.1f};

    std::vector<float> filtered_positions;

    float dt = 1.0f;
    for (int i = 0; i < 6; i++) {
        // Noisy IMU
        ekf.predict(0.0f, 0.0f, 0.0f, accel_noise[i], 0.0f, GRAVITY, dt);

        // Noisy GPS
        double lat_offset = gps_noise[i] / 111111.0;
        ekf.update(47.3667 + lat_offset, 11.1833, 520.0);

        float pn, pe, pd;
        ekf.getPosition(pn, pe, pd);
        filtered_positions.push_back(pn);
    }

    // Verify filter is still valid and positions are reasonable
    float state[16];
    ekf.getState(state);
    test_assert(is_valid_state(state) && ekf.isCovarianceValid(),
                "Scenario 8: State valid with noisy measurements");

    // Final position should be reasonable (not too far from noise)
    float final_pos = filtered_positions.back();
    test_assert(std::fabs(final_pos) < 10.0f,
                format_error("Scenario 8: Position reasonable (%.2f m)", final_pos),
                "Position should be bounded");
}

// ============================================================================
// SCENARIO 9: Initial Convergence
// ============================================================================

void scenario_9_initial_convergence() {
    printf("\n--- Scenario 9: Initial Convergence ---\n");

    ExtendedKalmanFilter ekf;
    ekf.reset();

    // Start with large initial uncertainty
    Quaternion q_identity(1.0f, 0.0f, 0.0f, 0.0f);
    ekf.initialize(q_identity, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    ekf.setInitialCovariance(0.5f, 5.0f, 400.0f, 1.0f);  // Large position uncertainty

    float initial_uncertainty = ekf.getPositionUncertainty();
    test_assert(initial_uncertainty > 10.0f,
                format_error("Scenario 9: Initial uncertainty large (%.2f m)", initial_uncertainty),
                "Should start with large uncertainty");

    float dt = 1.0f;
    float convergence_time = 0.0f;

    // Apply GPS measurements
    for (int i = 0; i < 30; i++) {
        ekf.predict(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, GRAVITY, dt);
        ekf.update(47.3667, 11.1833, 520.0);

        float uncertainty = ekf.getPositionUncertainty();
        if (uncertainty < 10.0f && convergence_time == 0.0f) {
            convergence_time = (i + 1) * dt;
        }
    }

    float final_uncertainty = ekf.getPositionUncertainty();
    test_assert(final_uncertainty < 10.0f,
                format_error("Scenario 9: Converged < 10m (%.2f m)", final_uncertainty),
                "Should converge within 30 seconds");

    test_assert(convergence_time > 0.0f && convergence_time <= 30.0f,
                format_error("Scenario 9: Convergence time = %.1f seconds", convergence_time),
                "Should measure reasonable convergence time");

    float state[16];
    ekf.getState(state);
    test_assert(is_valid_state(state),
                "Scenario 9: State valid");
}

// ============================================================================
// SCENARIO 10: Cold Start (No Prior Information)
// ============================================================================

void scenario_10_cold_start() {
    printf("\n--- Scenario 10: Cold Start (No Prior Information) ---\n");

    ExtendedKalmanFilter ekf;
    ekf.reset();

    // Initialize with large uncertainty
    Quaternion q_identity(1.0f, 0.0f, 0.0f, 0.0f);
    ekf.initialize(q_identity, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    ekf.setInitialCovariance(0.5f, 5.0f, 1000.0f, 1.0f);  // Very large position uncertainty

    float dt = 1.0f;

    // Only GPS measurements, no IMU motion
    for (int i = 0; i < 10; i++) {
        ekf.predict(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, GRAVITY, dt);
        ekf.update(47.3667, 11.1833, 520.0);  // Same GPS location
    }

    // Position should converge to GPS value
    float pn, pe, pd;
    ekf.getPosition(pn, pe, pd);

    // Expected: position converges to near zero (GPS reference is origin)
    test_assert(std::fabs(pn) < 5.0f && std::fabs(pe) < 5.0f,
                format_error("Scenario 10: Position converged (pn=%.2f, pe=%.2f)", pn, pe),
                "Position should converge to GPS from cold start");

    float state[16];
    ekf.getState(state);
    test_assert(is_valid_state(state),
                "Scenario 10: State valid");
}

// ============================================================================
// SCENARIO 11: Stability Over Time (1000 cycles)
// ============================================================================

void scenario_11_stability_over_time() {
    printf("\n--- Scenario 11: Stability Over Time (1000 cycles) ---\n");

    ExtendedKalmanFilter ekf;
    ekf.reset();

    Quaternion q_identity(1.0f, 0.0f, 0.0f, 0.0f);
    ekf.initialize(q_identity, 0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);

    float dt = 0.1f;
    int invalid_state_count = 0;
    int invalid_cov_count = 0;

    for (int i = 0; i < 1000; i++) {
        ekf.predict(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, GRAVITY, dt);

        if (i % 100 == 0) {
            ekf.update(47.3667, 11.1833, 520.0);
        }

        float state[16];
        ekf.getState(state);
        if (!is_valid_state(state)) invalid_state_count++;
        if (!ekf.isCovarianceValid()) invalid_cov_count++;
    }

    test_assert(invalid_state_count == 0,
                format_error("Scenario 11: No invalid states (found %d)", invalid_state_count),
                "State should remain valid throughout");

    test_assert(invalid_cov_count == 0,
                format_error("Scenario 11: No invalid covariances (found %d)", invalid_cov_count),
                "Covariance should remain valid throughout");

    test_assert(ekf.isCovariancePositiveDefinite(),
                "Scenario 11: Covariance positive definite at end");
}

// ============================================================================
// SCENARIO 12: Numerical Precision
// ============================================================================

void scenario_12_numerical_precision() {
    printf("\n--- Scenario 12: Numerical Precision ---\n");

    ExtendedKalmanFilter ekf;
    ekf.reset();

    Quaternion q_identity(1.0f, 0.0f, 0.0f, 0.0f);
    ekf.initialize(q_identity, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);

    float dt = 0.1f;

    // Very fast motion (10 m/s)
    float fast_velocity = 10.0f;
    for (int i = 0; i < 50; i++) {
        ekf.setVelocity(fast_velocity, 0.0f, 0.0f);
        ekf.predict(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, GRAVITY, dt);
    }

    float pn, pe, pd;
    ekf.getPosition(pn, pe, pd);
    float expected_pos = fast_velocity * 50 * dt;

    test_assert(std::fabs(pn - expected_pos) < 1.0f,
                format_error("Scenario 12: Fast motion (expected %.2f, got %.2f)", expected_pos, pn),
                "Fast motion should be accurate");

    // Reset and test very slow motion
    ekf.reset();
    ekf.initialize(q_identity, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);

    float slow_velocity = 0.01f;
    for (int i = 0; i < 50; i++) {
        ekf.setVelocity(slow_velocity, 0.0f, 0.0f);
        ekf.predict(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, GRAVITY, dt);
    }

    ekf.getPosition(pn, pe, pd);
    expected_pos = slow_velocity * 50 * dt;

    test_assert(pn > 0.0f,  // Just verify it moved
                "Scenario 12: Slow motion detected");

    float state[16];
    ekf.getState(state);
    test_assert(is_valid_state(state),
                "Scenario 12: State valid with extreme velocities");
}

// ============================================================================
// SCENARIO 13: Gravity Handling
// ============================================================================

void scenario_13_gravity_handling() {
    printf("\n--- Scenario 13: Gravity Handling ---\n");

    ExtendedKalmanFilter ekf;
    ekf.reset();

    // Level orientation (0 roll, 0 pitch, 0 yaw)
    Quaternion q_level(1.0f, 0.0f, 0.0f, 0.0f);
    ekf.initialize(q_level, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);

    float dt = 0.1f;

    // With level orientation and no acceleration,
    // the predicted vertical velocity should account for gravity
    for (int i = 0; i < 10; i++) {
        ekf.predict(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, dt);  // No measured accel
    }

    float v_n, v_e, v_d;
    ekf.getVelocity(v_n, v_e, v_d);

    // Vertical velocity should be non-zero due to gravity
    test_assert(std::fabs(v_d) > 0.5f,
                format_error("Scenario 13: Gravity effect detected (v_d=%.2f)", v_d),
                "Gravity should cause vertical acceleration");

    float state[16];
    ekf.getState(state);
    test_assert(is_valid_state(state),
                "Scenario 13: State valid");
}

// ============================================================================
// SCENARIO 14: Bias Estimation
// ============================================================================

void scenario_14_bias_estimation() {
    printf("\n--- Scenario 14: Bias Estimation ---\n");

    ExtendedKalmanFilter ekf;
    ekf.reset();

    Quaternion q_identity(1.0f, 0.0f, 0.0f, 0.0f);
    ekf.initialize(q_identity, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);

    // Constant bias in accelerometer
    float bias_true = 0.1f;  // 0.1 m/s² bias in X
    float dt = 0.1f;

    for (int i = 0; i < 100; i++) {
        // Apply measured acceleration with bias
        ekf.predict(0.0f, 0.0f, 0.0f, bias_true, 0.0f, GRAVITY, dt);

        // GPS updates at 1 Hz
        if (i % 10 == 0) {
            ekf.update(47.3667, 11.1833, 520.0);
        }
    }

    float bias_x, bias_y, bias_z;
    ekf.getAccelerometerBias(bias_x, bias_y, bias_z);

    // Bias estimation should track the true bias
    test_assert(std::fabs(bias_x - bias_true) < 0.05f,
                format_error("Scenario 14: Bias estimated (true=%.3f, est=%.3f)", bias_true, bias_x),
                "Should estimate accelerometer bias");

    float state[16];
    ekf.getState(state);
    test_assert(is_valid_state(state),
                "Scenario 14: State valid");
}

// ============================================================================
// SCENARIO 15: Multi-Cycle Consistency (Determinism)
// ============================================================================

void scenario_15_multi_cycle_consistency() {
    printf("\n--- Scenario 15: Multi-Cycle Consistency (Determinism) ---\n");

    // Run scenario 3 times and verify identical results
    std::vector<float> results[3];

    for (int run = 0; run < 3; run++) {
        ExtendedKalmanFilter ekf;
        ekf.reset();

        Quaternion q_identity(1.0f, 0.0f, 0.0f, 0.0f);
        ekf.initialize(q_identity, 0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);

        float dt = 0.1f;
        for (int i = 0; i < 100; i++) {
            ekf.predict(0.0f, 0.0f, 0.0f, 0.1f, 0.0f, GRAVITY, dt);

            if (i % 20 == 0) {
                ekf.update(47.3667, 11.1833, 520.0);
            }
        }

        float pn, pe, pd;
        ekf.getPosition(pn, pe, pd);
        results[run].push_back(pn);
        results[run].push_back(pe);
        results[run].push_back(pd);
    }

    // Compare results (should be identical)
    bool all_identical = true;
    for (int i = 0; i < 3; i++) {
        if (std::fabs(results[0][i] - results[1][i]) > EPSILON ||
            std::fabs(results[0][i] - results[2][i]) > EPSILON) {
            all_identical = false;
            break;
        }
    }

    test_assert(all_identical,
                "Scenario 15: Deterministic results across runs");
}

// ============================================================================
// MAIN TEST RUNNER
// ============================================================================

int main() {
    printf("\n");
    printf("%s\n", std::string(70, '=').c_str());
    printf("PHASE 3 EKF SCENARIO TESTS\n");
    printf("Comprehensive real-world scenario validation\n");
    printf("%s\n\n", std::string(70, '=').c_str());

    scenario_1_static_no_motion();
    scenario_2_constant_velocity();
    scenario_3_gps_dropout_recovery();
    scenario_4_filter_smoothing();
    scenario_5_high_acceleration();
    scenario_6_direction_change();
    scenario_7_extended_gps_outage();
    scenario_8_noisy_imu_gps();
    scenario_9_initial_convergence();
    scenario_10_cold_start();
    scenario_11_stability_over_time();
    scenario_12_numerical_precision();
    scenario_13_gravity_handling();
    scenario_14_bias_estimation();
    scenario_15_multi_cycle_consistency();

    print_summary();

    return (pass_count == test_count) ? 0 : 1;
}
