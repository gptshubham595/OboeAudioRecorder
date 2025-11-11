#ifndef OBOESAMPLE_NOISEGATE_H
#define OBOESAMPLE_NOISEGATE_H

#include <cmath>
#include <algorithm>

class NoiseGate {
public:
    NoiseGate();

    // Configure noise gate parameters
    void setThreshold(float thresholdDb);  // dB level below which signal is attenuated
    void setRatio(float ratio);             // Expansion ratio (e.g., 2:1, 4:1)
    void setAttack(float attackMs, float sampleRate);   // How fast gate closes
    void setRelease(float releaseMs, float sampleRate); // How fast gate opens

    // Process a single sample
    float process(float input);

    // Reset gate state
    void reset();

private:
    float mThreshold;      // Linear threshold
    float mRatio;          // Expansion ratio
    float mAttackCoeff;    // Attack time coefficient
    float mReleaseCoeff;   // Release time coefficient
    float mEnvelope;       // Current envelope level

    // Helper to calculate time coefficients
    float calculateCoeff(float timeMs, float sampleRate);
};

#endif //OBOESAMPLE_NOISEGATE_H