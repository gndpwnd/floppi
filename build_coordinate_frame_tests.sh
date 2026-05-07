#!/bin/bash

# Build coordinate frame tests
set -e

echo "Building CoordinateFrame tests..."
g++ -std=c++17 -Wall -Wextra -I. -DDEBUG_MODE \
    src/math/coordinates.cpp \
    src/navigation/coordinate_frame.cpp \
    tests/test_coordinate_frame.cpp \
    -o tests/test_coordinate_frame \
    $(pkg-config --cflags --libs gtest_main gtest) 2>&1

echo "Build complete!"
ls -lh tests/test_coordinate_frame

echo ""
echo "Running tests..."
./tests/test_coordinate_frame --gtest_color=yes

