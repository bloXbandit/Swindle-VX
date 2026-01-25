#pragma once

#include <vector>
#include <cmath>

namespace blink {

/**
 * Linear Predictive Coding (LPC) analyzer for formant extraction.
 * Uses Levinson-Durbin recursion to model the vocal tract.
 */
class LPCAnalyzer {
public:
    LPCAnalyzer(int order = 12);
    ~LPCAnalyzer() = default;

    /**
     * Analyze a frame of audio and extract LPC coefficients.
     * @param buffer Input audio buffer
     * @param numSamples Number of samples
     * @return true if analysis successful
     */
    bool analyze(const float* buffer, int numSamples);

    /**
     * Get the LPC coefficients (vocal tract model).
     * @return Vector of LPC coefficients (size = order)
     */
    const std::vector<float>& getCoefficients() const { return lpcCoeffs; }

    /**
     * Get the prediction error (residual energy).
     */
    float getError() const { return predictionError; }

    /**
     * Apply LPC filter to get spectral envelope.
     * @param frequencies Array of frequencies to evaluate
     * @param numFreqs Number of frequencies
     * @param envelope Output envelope values
     * @param sampleRate Sample rate for frequency conversion
     */
    void getSpectralEnvelope(const float* frequencies, int numFreqs, 
                            float* envelope, float sampleRate);

private:
    int order;  // LPC order (typically 10-16 for speech)
    std::vector<float> lpcCoeffs;
    std::vector<float> autocorr;
    float predictionError;
    
    // Levinson-Durbin recursion
    void levinsonDurbin(const std::vector<float>& r);
    
    // Calculate autocorrelation
    void calculateAutocorrelation(const float* buffer, int numSamples);
};

} // namespace blink
