#pragma once

#include <vector>
#include <complex>

namespace blink {

/**
 * High-quality Phase Vocoder for pitch and formant shifting.
 * Supports independent control of pitch and spectral envelope (formants).
 */
class PitchShifter {
public:
    PitchShifter(int fftSize, int hopSize);
    ~PitchShifter() = default;

    /**
     * Processes a frame of audio.
     * @param input Frame of FFT_SIZE samples
     * @param output Destination buffer
     * @param pitchRatio Frequency scaling factor (e.g., 2.0 is an octave up)
     * @param formantRatio Formant scaling factor
     */
    void process(const float* input, float* output, float pitchRatio, float formantRatio);

private:
    int fftSize;
    int hopSize;
    
    std::vector<float> window;
    std::vector<std::complex<float>> fftBuffer;
    std::vector<float> lastPhase;
    std::vector<float> sumPhase;

    std::vector<float> fftReal;
    std::vector<float> fftImag;
    std::vector<float> magnitude;
    std::vector<float> phase;
    std::vector<float> instFreq;
    std::vector<float> newMagnitude;
    std::vector<float> newPhase;
    std::vector<float> envelope;
    std::vector<float> warpedEnvelope;
    
    // Spectral envelope for formant preservation
    void shiftFormants(std::vector<std::complex<float>>& spectrum, float ratio);
    
    // FFT helper (replace with juce::dsp::FFT in production)
    void performFFT(float* real, float* imag, int n, bool forward);
};

} // namespace blink
