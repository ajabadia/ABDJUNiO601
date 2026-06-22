#!/bin/bash
# build_all_platforms_and_formats.sh
# Build script for macOS / Linux to compile all models and all configured plugin formats.

echo "======================================================================="
echo "JUNiO 601 - BUILD ALL MODELS AND ALL FORMATS (macOS / Linux)"
echo "======================================================================="

# Detect OS
OS_NAME=$(uname -s)
echo "Running on: $OS_NAME"

# Check if cmake is installed
if ! command -v cmake &> /dev/null; then
    echo "[ERROR] cmake could not be found. Please install CMake and try again."
    exit 1
fi

# Increment build number
VERSION_FILE="build_no.txt"
if [ ! -f "$VERSION_FILE" ]; then
    echo "0" > "$VERSION_FILE"
fi
build_no=$(cat "$VERSION_FILE")
build_no=$((build_no + 1))
echo "$build_no" > "$VERSION_FILE"

# Generate BuildVersion.h
echo "#define JUNO_BUILD_VERSION \"$build_no\"" > "Source/Core/BuildVersion.h"
echo "#define JUNO_BUILD_TIMESTAMP \"$(date)\"" >> "Source/Core/BuildVersion.h"

# Determine Generator (use Xcode on macOS if available, otherwise Makefiles)
GENERATOR=""
if [ "$OS_NAME" = "Darwin" ]; then
    if xcode-select -p &> /dev/null; then
        GENERATOR="-G Xcode"
        echo "Using Xcode generator for macOS."
    else
        echo "Xcode Command Line Tools not fully detected, falling back to Unix Makefiles."
    fi
fi

EXIT_CODE=0

build_model() {
    MODEL=$1
    case $MODEL in
        0) NAME="Super Six"; DIR="build_supersix" ;;
        1) NAME="ABD JUNiO 601 (J-106)"; DIR="build_j106" ;;
        2) NAME="ABD JUNiO 06 (J-60)"; DIR="build_j60" ;;
        3) NAME="ABD JUNiO SIX (J-6)"; DIR="build_j6" ;;
    esac

    echo ""
    echo "======================================================================="
    echo "Building all formats for: $NAME (JUNO_TARGET_MODEL=$MODEL)"
    echo "======================================================================="

    mkdir -p "$DIR"

    # Configure CMake
    if [ "$OS_NAME" = "Darwin" ] && [ -n "$GENERATOR" ]; then
        cmake -S . -B "$DIR" $GENERATOR -DJUNO_TARGET_MODEL=$MODEL
    else
        cmake -S . -B "$DIR" -DCMAKE_BUILD_TYPE=Release -DJUNO_TARGET_MODEL=$MODEL
    fi

    if [ $? -ne 0 ]; then
        echo "[ERROR] CMake configuration failed for $NAME"
        EXIT_CODE=1
        return
    fi

    # Build formats
    echo "[INFO] Compiling Standalone, VST3, AU and other configured formats..."
    cmake --build "$DIR" --config Release

    if [ $? -ne 0 ]; then
        echo "[ERROR] Build failed for $NAME"
        EXIT_CODE=1
        return
    fi

    echo "[SUCCESS] $NAME built successfully."
}

# Loop through all 4 models
for m in 0 1 2 3; do
    build_model $m
done

if [ $EXIT_CODE -ne 0 ]; then
    echo ""
    echo "======================================================================="
    echo "[ERROR] SOME MODELS OR FORMATS FAILED TO BUILD - Check output above"
    echo "======================================================================="
    exit 1
fi

echo ""
echo "======================================================================="
echo "[SUCCESS] ALL MODELS AND FORMATS BUILT SUCCESSFULLY FOR $OS_NAME"
echo "======================================================================="
exit 0
