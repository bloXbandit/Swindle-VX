#include "F0Extractor.h"
#include <cmath>
#include <algorithm>

namespace blink {

F0Extractor::F0Extractor(int hopSize)
    : pitchDetector(2048), hopSize(hopSize), sampleRate(44100.0),
      prevF0(0.0f), smoothingFactor(0.3f) {
}

void F0Extractor::setSampleRate(double sampleRate) {
    this->sampleRate = sampleRate;
}

float F0Extractor::processSample(const float* buffer, int numSamples) {
    // Detect pitch using YIN
    float frequency = pitchDetector.detectPitch(buffer, numSamples, (float)sampleRate);
    
    // Smooth F0 to avoid jitter
    if (frequency > 0.0f) {
        if (prevF0 > 0.0f) {
            // Exponential smoothing
            frequency = prevF0 * smoothingFactor + frequency * (1.0f - smoothingFactor);
        }
        prevF0 = frequency;
    } else {
        // Unvoiced frame (silence or noise)
        frequency = 0.0f;
        prevF0 = 0.0f;
    }
    
    // Store in F0 curve
    f0Curve.push_back(frequency);
    
    return frequency;
}

void F0Extractor::reset() {
    f0Curve.clear();
    prevF0 = 0.0f;
}

float F0Extractor::hzToMIDI(float hz) {
    if (hz <= 0.0f) return 0.0f;
    // MIDI note = 69 + 12 * log2(f / 440)
    return 69.0f + 12.0f * log2f(hz / 440.0f);
}

void F0Extractor::getF0AsMIDI(float* output, int numFrames) {
    int availableFrames = std::min(numFrames, (int)f0Curve.size());
    
    for (int i = 0; i < availableFrames; i++) {
        output[i] = hzToMIDI(f0Curve[i]);
    }
    
    // Fill remaining with zeros if needed
    for (int i = availableFrames; i < numFrames; i++) {
        output[i] = 0.0f;
    }
}

} // namespace blink
