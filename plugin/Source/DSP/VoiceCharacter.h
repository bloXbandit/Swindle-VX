#pragma once

#include <vector>
#include <random>

namespace blink {

/**
 * Voice character module for breath, resonance, and additional formant control.
 * Adds natural vocal characteristics and timbral modifications.
 */
class VoiceCharacter {
public:
    VoiceCharacter();
    
    void prepare(double sampleRate, int maxBlockSize);
    
    /**
     * Process audio with voice character effects
     * @param buffer Audio buffer to process in-place
     * @param numSamples Number of samples to process
     * @param breathAmount Breath/air amount (0.0 to 1.0)
     * @param resonanceAmount Resonance/presence (0.0 to 1.0)
     * @param resonanceFreq Center frequency for resonance (Hz)
     */
    void process(float* buffer, int numSamples, 
                 float breathAmount, float resonanceAmount, float resonanceFreq);

private:
    double sampleRate;

    float lastResonanceFreq = 0.0f;
    
    // Breath simulation (filtered noise)
    std::mt19937 noiseGen;
    std::uniform_real_distribution<float> noiseDist;
    std::vector<float> breathBuffer;
    
    // High-pass filter for breath (removes low rumble)
    float breathHPF_x1, breathHPF_x2, breathHPF_y1, breathHPF_y2;
    float breathHPF_b0, breathHPF_b1, breathHPF_b2, breathHPF_a1, breathHPF_a2;
    
    // Resonance filter (peaking EQ)
    float resonance_x1, resonance_x2, resonance_y1, resonance_y2;
    float resonance_b0, resonance_b1, resonance_b2, resonance_a1, resonance_a2;
    
    // Filter design helpers
    void designHighPass(float cutoffHz, float Q);
    void designPeaking(float centerHz, float Q, float gainDB);
    
    // Apply biquad filter
    float processBiquad(float input, 
                       float& x1, float& x2, float& y1, float& y2,
                       float b0, float b1, float b2, float a1, float a2);
};

} // namespace blink
