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
    void setIdentity();

    // Double-precision coefficients and state reduce error near DC and Nyquist.
    // Biquad coefficients
    double b0 = 1.0f, b1 = 0.0f, b2 = 0.0f;
    double a1 = 0.0f, a2 = 0.0f;

    // State variables (Direct Form I)
    double x1 = 0.0f, x2 = 0.0f; // input history
    double y1 = 0.0f, y2 = 0.0f; // output history
};

#endif //OBOESAMPLE_BIQUADFILTER_H