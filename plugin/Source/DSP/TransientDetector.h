#pragma once

#include <vector>
#include <cmath>

namespace blink {

/**
 * Transient detector for preserving consonants and attacks in vocal processing.
 * Uses energy-based detection to identify sharp transients (s, t, k sounds).
 */
class TransientDetector {
public:
    TransientDetector(int windowSize = 512);
    ~TransientDetector() = default;

    /**
     * Detect if the current frame contains a transient.
     * @param buffer Audio buffer to analyze
     * @param numSamples Number of samples in buffer
     * @return true if transient detected, false otherwise
     */
    bool detectTransient(const float* buffer, int numSamples);

    /**
     * Get the transient strength (0.0 = no transient, 1.0 = strong transient)
     */
    float getTransientStrength() const { return transientStrength; }

    /**
     * Set sensitivity threshold (lower = more sensitive)
     * Default: 2.5
     */
    void setThreshold(float threshold) { this->threshold = threshold; }

private:
    int windowSize;
    float threshold;
    float transientStrength;
    
    // Energy tracking
    float prevEnergy;
    float prevPrevEnergy;
    
    // Calculate RMS energy of a buffer
    float calculateEnergy(const float* buffer, int numSamples);
};

} // namespace blink
