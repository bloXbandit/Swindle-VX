#pragma once

#include "PitchDetector.h"
#include <vector>

namespace blink {

/**
 * F0 (Fundamental Frequency) extractor for AI voice models.
 * Tracks pitch over time and outputs continuous F0 curve.
 */
class F0Extractor {
public:
    F0Extractor(int hopSize = 512);
    ~F0Extractor() = default;

    /**
     * Set sample rate for frequency calculations.
     */
    void setSampleRate(double sampleRate);

    /**
     * Process audio and extract F0 values.
     * @param buffer Input audio buffer
     * @param numSamples Number of samples
     * @return Detected F0 in Hz (0.0 if unvoiced)
     */
    float processSample(const float* buffer, int numSamples);

    /**
     * Get the complete F0 curve accumulated so far.
     */
    const std::vector<float>& getF0Curve() const { return f0Curve; }

    /**
     * Clear the accumulated F0 curve.
     */
    void reset();

    /**
     * Get F0 curve in MIDI note format (for RVC models).
     * @param output Output buffer for MIDI notes
     * @param numFrames Number of frames to convert
     */
    void getF0AsMIDI(float* output, int numFrames);

private:
    PitchDetector pitchDetector;
    std::vector<float> f0Curve;
    int hopSize;
    double sampleRate;
    
    // Smoothing for continuous F0
    float prevF0;
    float smoothingFactor;
    
    // Convert Hz to MIDI note number
    float hzToMIDI(float hz);
};

} // namespace blink
