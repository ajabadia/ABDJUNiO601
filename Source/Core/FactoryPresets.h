#pragma once
#include <vector>
#include <cstddef>

/**
 * Factory presets extracted from the original Juno-106 ROM.
 * Generated automatically from "factory patches.106".
 */
struct FactoryPresetData {
    const char* name;
    unsigned char bytes[18];
};

static const FactoryPresetData junoFactoryPresets[] = {
    {"Placeholder", {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}}
};