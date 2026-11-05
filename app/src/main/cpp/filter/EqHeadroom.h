#pragma once
#include <algorithm>
#include <cmath>

inline float eqHeadroomGain(bool peaking, float peakDb, bool shelf, float shelfDb) {
    auto positiveGain = [](bool enabled, float db) {
        return enabled && std::isfinite(db) ? std::clamp(db, 0.0f, 24.0f) : 0.0f;
    };
    return std::pow(10.0f, -(positiveGain(peaking, peakDb) +
                            positiveGain(shelf, shelfDb)) / 20.0f);
}
