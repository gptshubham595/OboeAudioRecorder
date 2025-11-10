#ifndef OBOESAMPLE_BIQUADFILTER_H
#define OBOESAMPLE_BIQUADFILTER_H

#include <cmath>
#include <algorithm>

class BiquadFilter {
public:
    // Coefficient setters based on filter type
    void setBandpass(float sampleRate, float centerFreq, float Q);
    void setHighShelf(float sampleRate, float centerFreq, float Q, float gainDb);
    void setPeaking(float sampleRate, float centerFreq, float Q, float gainDb);

    // Processes a single sample
    float process(float in);

    // Clears the filter history
    void reset();

private:
    // Biquad coefficients
    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f;
    float a1 = 0.0f, a2 = 0.0f;

    // State variables (Direct Form I)
    float x1 = 0.0f, x2 = 0.0f; // input history
    float y1 = 0.0f, y2 = 0.0f; // output history
};

#endif //OBOESAMPLE_BIQUADFILTER_H