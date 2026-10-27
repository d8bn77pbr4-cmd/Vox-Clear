#pragma once

#include <cmath>

// Running quantile estimate (e.g. "the level that 90 % of the time is not exceeded").
// Each new value nudges the estimate up by p*step or down by (1-p)*step, so it settles on the
// p-quantile without storing any history - cheap and allocation-free for the audio thread.
struct QuantileTracker
{
    float value = 0.0f;
    bool valid = false;

    void add (float x, float p, float step)
    {
        if (! valid) { value = x; valid = true; return; }
        value += x > value ? p * step : -(1.0f - p) * step;
    }
    void seed (float v) { value = v; valid = true; }
};

inline float gainToDb (float g) { return 20.0f * std::log10 (g + 1.0e-9f); }
