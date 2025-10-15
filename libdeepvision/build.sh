#!/bin/bash

# Create build directory
mkdir -p build
cd build

# Configure with CMake
cmake ..

# Build the project
make -j$(nproc)

echo "Build completed! Run with:"
echo "./detector <model_path.onnx> <image_path>"
