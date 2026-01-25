#pragma once

#include <vector>
#include <complex>

namespace blink {

/**
 * Professional Phase Vocoder with proper overlap-add synthesis.
 * Implements SMB-style pitch shifting with formant preservation.
 * Supports independent control of pitch and spectral envelope (formants).
 */
class PitchShifter {
public:
    PitchShifter(int fftSize, int hopSize);
    ~PitchShifter() = default;

    /**
     * Processes audio with overlap-add.
     * @param input Input buffer (hopSize samples)
     * @param output Output buffer (hopSize samples)
     * @param pitchRatio Frequency scaling factor (e.g., 2.0 is an octave up)
     * @param formantRatio Formant scaling factor (1.0 = no change)
     */
    void process(const float* input, float* output, float pitchRatio, float formantRatio);

private:
    int fftSize;
    int hopSize;
    int osamp; // Oversampling factor
    
    // Window and FFT buffers
    std::vector<float> window;
    std::vector<float> fftBuffer;
    std::vector<float> lastPhase;
    std::vector<float> sumPhase;

    // FFT processing buffers
    std::vector<float> fftReal;
    std::vector<float> fftImag;
    std::vector<float> magnitude;
    std::vector<float> phase;
    std::vector<float> instFreq;
    std::vector<float> newMagnitude;
    std::vector<float> newPhase;
    std::vector<float> envelope;
    std::vector<float> warpedEnvelope;
    
    // Overlap-add buffers
    std::vector<float> inFIFO;
    std::vector<float> outFIFO;
    std::vector<float> outputAccum;
    int inFIFOIndex;
    int outFIFOIndex;
    float windowNorm;
    
    // Process one frame
    void processFrame(const float* frame, float pitchRatio, float formantRatio);
    
    // Spectral envelope for formant preservation
    void shiftFormants(std::vector<std::complex<float>>& spectrum, float ratio);
    
    // FFT helper (Cooley-Tukey implementation)
    void performFFT(float* real, float* imag, int n, bool forward);
};

} // namespace blink
