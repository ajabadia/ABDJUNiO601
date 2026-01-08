#!/bin/bash
mkdir -p build_rpi
cd build_rpi
# Headless build for Raspberry Pi by default
cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_HEADLESS=ON
cmake --build . --config Release
