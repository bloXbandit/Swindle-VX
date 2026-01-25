#include "TransientDetector.h"
#include <algorithm>
#include <cmath>

namespace blink {

TransientDetector::TransientDetector(int windowSize)
    : windowSize(windowSize), threshold(2.5f), transientStrength(0.0f),
      prevEnergy(0.0f), prevPrevEnergy(0.0f) {
}

float TransientDetector::calculateEnergy(const float* buffer, int numSamples) {
    float sum = 0.0f;
    for (int i = 0; i < numSamples; i++) {
        sum += buffer[i] * buffer[i];
    }
    return sqrtf(sum / numSamples);
}

bool TransientDetector::detectTransient(const float* buffer, int numSamples) {
    // Calculate current energy
    float currentEnergy = calculateEnergy(buffer, numSamples);
    
    // Avoid division by zero
    if (prevEnergy < 0.0001f) {
        prevPrevEnergy = prevEnergy;
        prevEnergy = currentEnergy;
        transientStrength = 0.0f;
        return false;
    }
    
    // Calculate energy ratio (current vs previous)
    float energyRatio = currentEnergy / prevEnergy;
    
    // Calculate second derivative (acceleration of energy change)
    float prevRatio = prevEnergy / (prevPrevEnergy + 0.0001f);
    float acceleration = energyRatio - prevRatio;
    
    // Transient detected if:
    // 1. Energy increased significantly (energyRatio > threshold)
    // 2. Acceleration is positive (energy is increasing faster)
    bool isTransient = (energyRatio > threshold) && (acceleration > 0.5f);
    
    // Calculate transient strength (0.0 to 1.0)
    if (isTransient) {
        transientStrength = std::min(1.0f, (energyRatio - threshold) / threshold);
    } else {
        transientStrength = 0.0f;
    }
    
    // Update history
    prevPrevEnergy = prevEnergy;
    prevEnergy = currentEnergy;
    
    return isTransient;
}

} // namespace blink
