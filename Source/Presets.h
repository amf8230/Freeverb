#pragma once

#include "ReverbEngine.h"

namespace ev
{

struct Preset
{
    const char* name;
    int   type;
    float predelayMs, decaySec, size, damping, diffusion, early, modDepth;
    float lowCutHz, highCutHz, width, warmth, mix;
};

// Factory presets. Mix values for the "Master" presets are deliberately low:
// on a full mix a little reverb goes a long way.
inline const Preset kPresets[] = {
    //  name                type      pre   decay size  damp  diff  early mod   lowcut highcut width warm  mix
    { "Vault Chamber",     Chamber, 15.0f, 1.8f, 0.50f, 0.45f, 0.70f, 0.50f, 0.25f, 120.0f,  9000.0f, 1.00f, 0.25f, 0.25f },
    { "Vocal Chamber",     Chamber, 60.0f, 2.2f, 0.55f, 0.50f, 0.75f, 0.35f, 0.30f, 180.0f,  8500.0f, 1.10f, 0.30f, 0.20f },
    { "Drum Chamber",      Chamber,  5.0f, 1.1f, 0.40f, 0.55f, 0.65f, 0.70f, 0.15f, 150.0f,  7000.0f, 1.20f, 0.45f, 0.25f },
    { "Dark Chamber",      Chamber, 25.0f, 2.8f, 0.70f, 0.80f, 0.75f, 0.40f, 0.30f, 100.0f,  5500.0f, 1.00f, 0.35f, 0.25f },
    { "Classic Plate",     Plate,   20.0f, 2.0f, 0.50f, 0.30f, 0.80f, 0.00f, 0.40f, 200.0f, 14000.0f, 1.20f, 0.10f, 0.20f },
    { "Vocal Plate",       Plate,   80.0f, 1.6f, 0.45f, 0.35f, 0.80f, 0.00f, 0.45f, 250.0f, 12000.0f, 1.10f, 0.15f, 0.18f },
    { "Snare Plate",       Plate,    0.0f, 1.2f, 0.35f, 0.25f, 0.75f, 0.00f, 0.30f, 220.0f, 15000.0f, 1.30f, 0.20f, 0.25f },
    { "Live Room",         Room,     3.0f, 0.8f, 0.50f, 0.45f, 0.60f, 0.80f, 0.15f,  60.0f, 11000.0f, 1.20f, 0.30f, 0.22f },
    { "Tight Drum Room",   Room,     0.0f, 0.45f,0.30f, 0.55f, 0.55f, 0.90f, 0.10f,  80.0f,  9000.0f, 1.30f, 0.50f, 0.25f },
    { "Wood Room",         Room,     8.0f, 1.2f, 0.70f, 0.60f, 0.65f, 0.70f, 0.20f,  70.0f,  8500.0f, 1.10f, 0.30f, 0.20f },
    { "Concert Hall",      Hall,    30.0f, 3.2f, 0.75f, 0.50f, 0.70f, 0.40f, 0.50f,  60.0f, 10000.0f, 1.30f, 0.10f, 0.20f },
    { "Strings Hall",      Hall,    45.0f, 4.5f, 0.85f, 0.55f, 0.75f, 0.30f, 0.60f,  90.0f,  9000.0f, 1.40f, 0.10f, 0.22f },
    { "Master Glue",       Room,     0.0f, 0.6f, 0.45f, 0.50f, 0.70f, 0.50f, 0.10f, 300.0f,  8000.0f, 1.00f, 0.10f, 0.06f },
    { "Master Air",        Plate,   15.0f, 1.2f, 0.40f, 0.30f, 0.80f, 0.00f, 0.30f, 600.0f, 16000.0f, 1.20f, 0.00f, 0.05f },
    { "Master Depth",      Chamber, 10.0f, 1.4f, 0.50f, 0.50f, 0.70f, 0.40f, 0.20f, 250.0f,  9000.0f, 1.10f, 0.15f, 0.07f },
};

constexpr int kNumPresets = (int) (sizeof (kPresets) / sizeof (kPresets[0]));

} // namespace ev
