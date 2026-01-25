#pragma once

#include <vector>
#include <complex>

namespace blink {

/**
 * Professional implementation of the YIN algorithm for pitch detection.
 * Optimized for low-latency real-time vocal processing.
 */
class PitchDetector {
public:
    PitchDetector(double sampleRate, int bufferSize);
    ~PitchDetector() = default;

    /**
     * Processes a buffer of audio and returns the detected frequency in Hz.
     * Returns 0.0 if no pitch is detected (unvoiced/silence).
     */
    float getPitch(const float* buffer, int numSamples);

    void setSampleRate(double newRate) { sampleRate = newRate; }
    void setBufferSize(int newSize) { bufferSize = newSize; yinBuffer.resize(newSize / 2); }

private:
    double sampleRate;
    int bufferSize;
    std::vector<float> yinBuffer;
    float threshold = 0.10f; // Typical YIN threshold for vocals

    // Internal YIN steps
    void difference(const float* buffer, int analysisSize);
    void cumulativeMeanNormalizedDifference();
    int absoluteThreshold();
    float parabolicInterpolation(int tauEstimate);
};

} // namespace blink
