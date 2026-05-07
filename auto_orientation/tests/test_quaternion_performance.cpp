/**
 * Quaternion Math Library Performance Benchmarks
 *
 * Measures execution time for key operations to verify they meet
 * the <1ms requirement for Arduino Mega
 */

#include <cmath>
#include <cstdio>
#include <chrono>

#include "../src/math/quaternion.h"
#include "../src/math/quaternion_conversions.h"

using namespace std::chrono;

// ============================================================================
// PERFORMANCE BENCHMARKING
// ============================================================================

typedef high_resolution_clock Clock;

void benchmark_quaternion_multiply(int iterations = 10000) {
    printf("\n--- Quaternion Multiplication Benchmark ---\n");

    Quaternion q1 = euler_to_quaternion(0.3f, 0.5f, 0.7f);
    Quaternion q2 = euler_to_quaternion(0.2f, 0.1f, 0.4f);

    auto start = Clock::now();
    for (int i = 0; i < iterations; i++) {
        Quaternion result = q1.multiply(q2);
        (void)result;  // Prevent optimization
    }
    auto end = Clock::now();

    double elapsed_ms = duration_cast<microseconds>(end - start).count() / 1000.0;
    double time_per_op_us = duration_cast<nanoseconds>(end - start).count() / iterations / 1000.0;

    printf("  Iterations: %d\n", iterations);
    printf("  Total time: %.3f ms\n", elapsed_ms);
    printf("  Time per operation: %.3f microseconds\n", time_per_op_us);
    printf("  Operations per millisecond: %.1f\n", iterations / elapsed_ms);

    if (time_per_op_us < 1000.0) {
        printf("  PASS: < 1 millisecond per operation\n");
    } else {
        printf("  WARNING: > 1 millisecond per operation\n");
    }
}

void benchmark_vector_rotation(int iterations = 10000) {
    printf("\n--- Vector Rotation Benchmark ---\n");

    Quaternion q = euler_to_quaternion(0.3f, 0.5f, 0.7f);
    Vector3D v(1, 2, 3);

    auto start = Clock::now();
    for (int i = 0; i < iterations; i++) {
        Vector3D result = rotate_vector_by_quaternion(q, v);
        (void)result;
    }
    auto end = Clock::now();

    double elapsed_ms = duration_cast<microseconds>(end - start).count() / 1000.0;
    double time_per_op_us = duration_cast<nanoseconds>(end - start).count() / iterations / 1000.0;

    printf("  Iterations: %d\n", iterations);
    printf("  Total time: %.3f ms\n", elapsed_ms);
    printf("  Time per operation: %.3f microseconds\n", time_per_op_us);
    printf("  Operations per millisecond: %.1f\n", iterations / elapsed_ms);

    if (time_per_op_us < 1000.0) {
        printf("  PASS: < 1 millisecond per operation\n");
    } else {
        printf("  WARNING: > 1 millisecond per operation\n");
    }
}

void benchmark_quaternion_to_matrix(int iterations = 1000) {
    printf("\n--- Quaternion to Rotation Matrix Benchmark ---\n");

    Quaternion q = euler_to_quaternion(0.3f, 0.5f, 0.7f);

    auto start = Clock::now();
    for (int i = 0; i < iterations; i++) {
        RotationMatrix result = quaternion_to_rotation_matrix(q);
        (void)result;
    }
    auto end = Clock::now();

    double elapsed_ms = duration_cast<microseconds>(end - start).count() / 1000.0;
    double time_per_op_us = duration_cast<nanoseconds>(end - start).count() / iterations / 1000.0;

    printf("  Iterations: %d\n", iterations);
    printf("  Total time: %.3f ms\n", elapsed_ms);
    printf("  Time per operation: %.3f microseconds\n", time_per_op_us);
    printf("  Operations per millisecond: %.1f\n", iterations / elapsed_ms);

    if (time_per_op_us < 1000.0) {
        printf("  PASS: < 1 millisecond per operation\n");
    } else {
        printf("  WARNING: > 1 millisecond per operation\n");
    }
}

void benchmark_quaternion_normalize(int iterations = 10000) {
    printf("\n--- Quaternion Normalization Benchmark ---\n");

    Quaternion q(2.0f, 0.5f, 0.3f, 0.1f);

    auto start = Clock::now();
    for (int i = 0; i < iterations; i++) {
        Quaternion q_copy = q;
        q_copy.normalize();
        (void)q_copy;
    }
    auto end = Clock::now();

    double elapsed_ms = duration_cast<microseconds>(end - start).count() / 1000.0;
    double time_per_op_us = duration_cast<nanoseconds>(end - start).count() / iterations / 1000.0;

    printf("  Iterations: %d\n", iterations);
    printf("  Total time: %.3f ms\n", elapsed_ms);
    printf("  Time per operation: %.3f microseconds\n", time_per_op_us);
    printf("  Operations per millisecond: %.1f\n", iterations / elapsed_ms);

    if (time_per_op_us < 1000.0) {
        printf("  PASS: < 1 millisecond per operation\n");
    } else {
        printf("  WARNING: > 1 millisecond per operation\n");
    }
}

void benchmark_euler_to_quaternion(int iterations = 5000) {
    printf("\n--- Euler to Quaternion Benchmark ---\n");

    auto start = Clock::now();
    for (int i = 0; i < iterations; i++) {
        Quaternion result = euler_to_quaternion(0.3f, 0.5f, 0.7f);
        (void)result;
    }
    auto end = Clock::now();

    double elapsed_ms = duration_cast<microseconds>(end - start).count() / 1000.0;
    double time_per_op_us = duration_cast<nanoseconds>(end - start).count() / iterations / 1000.0;

    printf("  Iterations: %d\n", iterations);
    printf("  Total time: %.3f ms\n", elapsed_ms);
    printf("  Time per operation: %.3f microseconds\n", time_per_op_us);
    printf("  Operations per millisecond: %.1f\n", iterations / elapsed_ms);

    if (time_per_op_us < 1000.0) {
        printf("  PASS: < 1 millisecond per operation\n");
    } else {
        printf("  WARNING: > 1 millisecond per operation\n");
    }
}

void benchmark_quaternion_to_euler(int iterations = 5000) {
    printf("\n--- Quaternion to Euler Benchmark ---\n");

    Quaternion q = euler_to_quaternion(0.3f, 0.5f, 0.7f);

    auto start = Clock::now();
    for (int i = 0; i < iterations; i++) {
        EulerAngles result = quaternion_to_euler(q);
        (void)result;
    }
    auto end = Clock::now();

    double elapsed_ms = duration_cast<microseconds>(end - start).count() / 1000.0;
    double time_per_op_us = duration_cast<nanoseconds>(end - start).count() / iterations / 1000.0;

    printf("  Iterations: %d\n", iterations);
    printf("  Total time: %.3f ms\n", elapsed_ms);
    printf("  Time per operation: %.3f microseconds\n", time_per_op_us);
    printf("  Operations per millisecond: %.1f\n", iterations / elapsed_ms);

    if (time_per_op_us < 1000.0) {
        printf("  PASS: < 1 millisecond per operation\n");
    } else {
        printf("  WARNING: > 1 millisecond per operation\n");
    }
}

int main() {
    printf("========================================\n");
    printf("QUATERNION LIBRARY PERFORMANCE BENCHMARKS\n");
    printf("========================================\n");

    benchmark_quaternion_multiply();
    benchmark_vector_rotation();
    benchmark_quaternion_normalize();
    benchmark_euler_to_quaternion();
    benchmark_quaternion_to_euler();
    benchmark_quaternion_to_matrix();

    printf("\n========================================\n");
    printf("PERFORMANCE BENCHMARKS COMPLETE\n");
    printf("========================================\n");

    return 0;
}
