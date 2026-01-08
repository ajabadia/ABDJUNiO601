#!/bin/bash
mkdir -p build_mac
cd build_mac
cmake .. -DCMAKE_BUILD_TYPE=Release -G Xcode
cmake --build . --config Release
