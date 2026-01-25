#include "VoiceCharacter.h"
#include <cmath>
#include <algorithm>

namespace blink {

VoiceCharacter::VoiceCharacter() 
    : sampleRate(44100.0), 
      noiseDist(-1.0f, 1.0f),
      breathHPF_x1(0), breathHPF_x2(0), breathHPF_y1(0), breathHPF_y2(0),
      resonance_x1(0), resonance_x2(0), resonance_y1(0), resonance_y2(0) {
    noiseGen.seed(12345);
}

void VoiceCharacter::prepare(double sr, int maxBlockSize) {
    sampleRate = sr;
    breathBuffer.resize(maxBlockSize);

    lastResonanceFreq = 0.0f;
    
    // Design default filters
    designHighPass(2000.0f, 0.707f); // High-pass for breath at 2kHz
    designPeaking(2500.0f, 2.0f, 6.0f); // Default resonance
}

void VoiceCharacter::process(float* buffer, int numSamples, 
                             float breathAmount, float resonanceAmount, float resonanceFreq) {
    // 1. BREATH SIMULATION
    if (breathAmount > 0.001f) {
        const int breathSamples = (int) std::min<size_t>((size_t) numSamples, breathBuffer.size());
        // Generate filtered white noise
        for (int i = 0; i < breathSamples; i++) {
            float noise = noiseDist(noiseGen);
            // High-pass filter the noise to sound like breath
            breathBuffer[i] = processBiquad(noise, 
                breathHPF_x1, breathHPF_x2, breathHPF_y1, breathHPF_y2,
                breathHPF_b0, breathHPF_b1, breathHPF_b2, breathHPF_a1, breathHPF_a2);
        }
        
        // Mix breath with original signal
        for (int i = 0; i < breathSamples; i++) {
            buffer[i] += breathBuffer[i] * breathAmount * 0.15f; // Scale down the breath
        }
    }
    
    // 2. RESONANCE (Vocal Formant Enhancement)
    if (resonanceAmount > 0.001f) {
        // Update resonance filter if frequency changed significantly
        if (std::abs(resonanceFreq - lastResonanceFreq) > 10.0f) {
            float gainDB = 3.0f + resonanceAmount * 9.0f; // 3-12 dB boost
            float Q = 1.0f + resonanceAmount * 3.0f; // Q: 1.0 to 4.0
            designPeaking(resonanceFreq, Q, gainDB);
            lastResonanceFreq = resonanceFreq;
        }
        
        // Apply resonance filter
        for (int i = 0; i < numSamples; i++) {
            buffer[i] = processBiquad(buffer[i],
                resonance_x1, resonance_x2, resonance_y1, resonance_y2,
                resonance_b0, resonance_b1, resonance_b2, resonance_a1, resonance_a2);
        }
    }
}

void VoiceCharacter::designHighPass(float cutoffHz, float Q) {
    float w0 = 2.0f * M_PI * cutoffHz / sampleRate;
    float cosw0 = cosf(w0);
    float alpha = sinf(w0) / (2.0f * Q);
    
    float a0 = 1.0f + alpha;
    breathHPF_b0 = (1.0f + cosw0) / 2.0f / a0;
    breathHPF_b1 = -(1.0f + cosw0) / a0;
    breathHPF_b2 = (1.0f + cosw0) / 2.0f / a0;
    breathHPF_a1 = -2.0f * cosw0 / a0;
    breathHPF_a2 = (1.0f - alpha) / a0;
}

void VoiceCharacter::designPeaking(float centerHz, float Q, float gainDB) {
    float w0 = 2.0f * M_PI * centerHz / sampleRate;
    float cosw0 = cosf(w0);
    float sinw0 = sinf(w0);
    float A = powf(10.0f, gainDB / 40.0f);
    float alpha = sinw0 / (2.0f * Q);
    
    float a0 = 1.0f + alpha / A;
    resonance_b0 = (1.0f + alpha * A) / a0;
    resonance_b1 = (-2.0f * cosw0) / a0;
    resonance_b2 = (1.0f - alpha * A) / a0;
    resonance_a1 = (-2.0f * cosw0) / a0;
    resonance_a2 = (1.0f - alpha / A) / a0;
}

float VoiceCharacter::processBiquad(float input, 
                                    float& x1, float& x2, float& y1, float& y2,
                                    float b0, float b1, float b2, float a1, float a2) {
    float output = b0 * input + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
    
    // Update state
    x2 = x1;
    x1 = input;
    y2 = y1;
    y1 = output;
    
    return output;
}

} // namespace blink
