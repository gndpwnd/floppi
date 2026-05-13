/**
 * Scenario regression test: balance-robot pitch replay
 *
 * Phase 4.5a host-only regression test. Replays the synthetic pitch
 * trajectory captured in tests/data/balancing_reference_trajectory.csv
 * through the new PIDController and asserts that the per-sample output
 * matches the CSV's pid_output column within tolerance.
 *
 * The CSV fixture is produced by tools/generate_balance_trajectory.py
 * which simulates the legacy SelfBallancingRobot3.ino PID loop on a
 * 2nd-order inverted-pendulum plant with friction. Both implementations
 * use:
 *   Kp = 65, Ki = 12, Kd = 38
 *   sample = 5 ms, output limits = +/- 255
 *   PITCH_OFFSET_DEG = -8.6
 *   D-on-error (PID_v1 DIRECT mode)
 *   stiction deadband = 15 PWM
 *
 * The comparison is done on pid_output (the value BEFORE motor-level
 * sign inversion). The left_pwm / right_pwm columns in the CSV are
 * intentionally NOT asserted -- the generator writes them with opposite
 * signs (left = +pid, right = -pid) while the .ino actually drives both
 * motors with the same sign; that's a generator quirk, not a controller
 * bug, and it doesn't affect pid_output.
 *
 * Build / run (host only -- never compile for AVR):
 *   g++ -O2 -std=c++11 -Isrc \
 *       tests/scenario_test_balancing.cpp src/control/pid_controller.cpp \
 *       -o /tmp/scenario_test_balancing
 *   /tmp/scenario_test_balancing tests/data/balancing_reference_trajectory.csv
 *
 * Exit code: 0 on pass, 1 on fail (or fixture missing / unparseable).
 */

#include "../src/control/pid_controller.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {

// Generator constants (must match tools/generate_balance_trajectory.py
// and docs/archive/balancing_robot_reference/SelfBallancingRobot3.ino).
const float KP                = 65.0f;
const float KI                = 12.0f;
const float KD                = 38.0f;
const float PITCH_OFFSET_DEG  = -8.6f;
const uint16_t SAMPLE_DT_MS   = 5;
const float OUTPUT_MIN        = -255.0f;
const float OUTPUT_MAX        = +255.0f;
const float STICTION_DEADBAND = 15.0f;

struct Row {
    uint32_t timestamp_ms;
    float    raw_pitch_deg;
    float    corrected_pitch_deg;
    float    pid_output;       // expected, post-stiction, post-clamp
    float    left_pwm;         // unused in assertions
    float    right_pwm;        // unused in assertions
};

/**
 * Mirror of the generator's apply_stiction_clamp():
 *   - magnitude below STICTION_DEADBAND -> 0
 *   - magnitude above PWM_MAX           -> +/- PWM_MAX (clamped)
 *
 * The PIDController already clamps to +/- 255 internally, so all this
 * function adds on top is the deadband.
 */
float apply_stiction_clamp(float pid_output) {
    if (std::fabs(pid_output) < STICTION_DEADBAND) {
        return 0.0f;
    }
    if (pid_output > OUTPUT_MAX)  return OUTPUT_MAX;
    if (pid_output < OUTPUT_MIN)  return OUTPUT_MIN;
    return pid_output;
}

/**
 * Parse a single CSV line into a Row. Returns false on malformed input.
 * Format: timestamp_ms,raw_pitch_deg,corrected_pitch_deg,pid_output,left_pwm,right_pwm
 */
bool parse_row(const std::string& line, Row& out) {
    std::stringstream ss(line);
    std::string field;
    float vals[6];
    int idx = 0;
    while (std::getline(ss, field, ',')) {
        if (idx >= 6) return false;
        // strtof handles trailing whitespace / negative numbers fine
        char* endp = nullptr;
        vals[idx] = std::strtof(field.c_str(), &endp);
        if (endp == field.c_str()) {
            return false;  // no digits parsed
        }
        ++idx;
    }
    if (idx != 6) return false;

    out.timestamp_ms        = static_cast<uint32_t>(vals[0]);
    out.raw_pitch_deg       = vals[1];
    out.corrected_pitch_deg = vals[2];
    out.pid_output          = vals[3];
    out.left_pwm            = vals[4];
    out.right_pwm           = vals[5];
    return true;
}

bool load_csv(const char* path, std::vector<Row>& out) {
    std::ifstream f(path);
    if (!f.is_open()) {
        std::fprintf(stderr,
                     "ERROR: could not open CSV fixture: %s\n", path);
        return false;
    }
    std::string line;
    // Skip header
    if (!std::getline(f, line)) {
        std::fprintf(stderr, "ERROR: empty CSV file: %s\n", path);
        return false;
    }
    int line_no = 1;
    while (std::getline(f, line)) {
        ++line_no;
        if (line.empty()) continue;
        Row r;
        if (!parse_row(line, r)) {
            std::fprintf(stderr,
                         "ERROR: malformed CSV line %d: %s\n",
                         line_no, line.c_str());
            return false;
        }
        out.push_back(r);
    }
    return !out.empty();
}

}  // namespace

int main(int argc, char** argv) {
    const char* csv_path =
        (argc > 1) ? argv[1]
                   : "tests/data/balancing_reference_trajectory.csv";

    std::vector<Row> rows;
    if (!load_csv(csv_path, rows)) {
        return 1;
    }

    // Configure the controller to match the .ino / generator EXACTLY.
    //
    // The legacy PID_v1 library in DIRECT mode uses derivative-on-error,
    // so we must turn off our default derivative-on-measurement setting.
    // The first-compute D skip in PIDController matches the generator's
    // `prev_error is None` branch (both produce derivative = 0 on sample 0).
    PIDController pid(KP, KI, KD, OUTPUT_MIN, OUTPUT_MAX);
    pid.set_sample_time(SAMPLE_DT_MS);
    pid.set_setpoint(0.0f);
    pid.set_d_on_measurement(false);

    // Tolerance: 5 PWM units. This is generous on purpose -- the Python
    // generator uses double precision while the controller is float, and
    // anti-windup conventions differ very slightly in edge cases. We
    // care about behavioural agreement, not bit-exact reproduction.
    const float TOLERANCE = 5.0f;

    float max_abs_err = 0.0f;
    double sum_abs_err = 0.0;  // double accumulator across 600 samples
    int    max_err_index = -1;

    for (size_t i = 0; i < rows.size(); ++i) {
        // The .ino feeds `corrected = rawPitch - PITCH_OFFSET` to the PID.
        // Re-derive it here from raw_pitch_deg so the test is robust to
        // any future change in how the CSV reports the corrected value.
        const float corrected = rows[i].raw_pitch_deg - PITCH_OFFSET_DEG;

        // PID step.
        const float pid_raw = pid.compute(corrected, SAMPLE_DT_MS);

        // Generator applies stiction deadband AFTER the PID output is
        // produced; do the same here so we're comparing apples-to-apples.
        const float computed = apply_stiction_clamp(pid_raw);

        const float expected = rows[i].pid_output;
        const float err = std::fabs(computed - expected);

        if (err > max_abs_err) {
            max_abs_err   = err;
            max_err_index = static_cast<int>(i);
        }
        sum_abs_err += err;
    }

    const float mean_abs_err =
        static_cast<float>(sum_abs_err / static_cast<double>(rows.size()));
    const bool pass = (max_abs_err <= TOLERANCE);

    uint32_t max_err_t_ms = 0;
    if (max_err_index >= 0 &&
        static_cast<size_t>(max_err_index) < rows.size()) {
        max_err_t_ms = rows[max_err_index].timestamp_ms;
    }

    std::printf("Scenario test: balance robot pitch replay\n");
    std::printf("  samples processed: %zu\n", rows.size());
    std::printf("  max absolute error: %.3f PWM at sample %d (t=%ums)\n",
                max_abs_err, max_err_index, max_err_t_ms);
    std::printf("  mean absolute error: %.3f PWM\n", mean_abs_err);
    std::printf("  %s (tolerance +/- %.1f PWM)\n",
                pass ? "PASSED" : "FAILED", TOLERANCE);

    return pass ? 0 : 1;
}
