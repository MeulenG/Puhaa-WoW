#!/bin/bash

set -e  # Exit on error

cd "$(dirname "$0")"

echo "Building puhaa-wow..."

mkdir -p build
cd build

echo "Configuring with CMake..."
cmake .. -DCMAKE_BUILD_TYPE=Release

# Build with all cores
echo "Building with $(nproc) cores..."
cmake --build . --parallel $(nproc)

# Ensure Data symlink exists in bin directory
cd bin
if [ ! -e Data ]; then
    ln -s ../../Data Data
fi
cd ..

cd extern/unicorn-2.1.4
mkdir -p build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --parallel $(nproc)
cd ../../../..

echo ""
echo "Build complete! Binary: build/bin/puhaa-wow"
echo "Run with: cd build/bin && ./puhaa-wow"
