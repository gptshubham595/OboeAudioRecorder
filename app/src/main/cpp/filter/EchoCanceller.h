#ifndef OBOESAMPLE_ECHOCANCELLER_H
#define OBOESAMPLE_ECHOCANCELLER_H

#include <vector>
#include <cmath>

// Simple echo suppression using adaptive filtering
class EchoCanceller {
public:
    EchoCanceller(int sampleRate);

    // Set echo delay in milliseconds
    void setEchoDelay(float delayMs);

    // Set echo suppression amount (0.0 to 1.0)
    void setSuppressionAmount(float amount);

    // Process a single sample
    float process(float input);

    // Reset state
    void reset();

private:
    std::vector<float> mDelayBuffer;
    int mSampleRate;
    int mDelayInSamples;
    int mWriteIndex;
    float mSuppressionAmount;

    // Adaptive filter coefficient
    float mAdaptiveCoeff;
};

#endif //OBOESAMPLE_ECHOCANCELLER_H