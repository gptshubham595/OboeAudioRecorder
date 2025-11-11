#ifndef OBOESAMPLE_NOISEREDUCTION_H
#define OBOESAMPLE_NOISEREDUCTION_H

#include <vector>
#include <cmath>

// Simple noise reduction using moving average smoothing
class NoiseReduction {
public:
    NoiseReduction(int smoothingWindow = 5);

    // Set noise reduction strength (0.0 to 1.0)
    void setReductionAmount(float amount);

    // Process a single sample
    float process(float input);

    // Reset state
    void reset();

private:
    std::vector<float> mBuffer;
    int mWindowSize;
    int mIndex;
    float mReductionAmount;
    float mSum;
};

#endif //OBOESAMPLE_NOISEREDUCTION_H