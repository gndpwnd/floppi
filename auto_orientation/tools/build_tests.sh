#!/bin/bash

# Build math tests for desktop (gtest)
set -e

echo "Building BNO085 extension tests..."
g++ -std=c++17 -Wall -Wextra -I. \
    src/math/quaternion.cpp \
    src/math/quaternion_conversions.cpp \
    tests/test_bno085_extensions.cpp \
    -o tests/test_bno085_extensions \
    $(pkg-config --cflags --libs gtest_main gtest) 2>&1 | head -20

echo "Building math pipeline integration tests..."
g++ -std=c++17 -Wall -Wextra -I. \
    src/math/quaternion.cpp \
    src/math/quaternion_conversions.cpp \
    tests/integration_test_math_pipeline.cpp \
    -o tests/integration_test_math_pipeline \
    $(pkg-config --cflags --libs gtest_main gtest) 2>&1 | head -20

echo "Building benchmark tests..."
g++ -std=c++17 -Wall -Wextra -O3 -I. \
    src/math/quaternion.cpp \
    src/math/quaternion_conversions.cpp \
    tests/benchmark_math.cpp \
    -o tests/benchmark_math \
    $(pkg-config --cflags --libs gtest_main gtest) 2>&1 | head -20

echo "Build complete!"
ls -lh tests/test_bno085_extensions tests/integration_test_math_pipeline tests/benchmark_math
