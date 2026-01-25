#include "PitchShifter.h"
#include <cmath>
#include <algorithm>

namespace blink {

PitchShifter::PitchShifter(int size, int hop) : fftSize(size), hopSize(hop) {
    window.resize(fftSize);
    fftBuffer.resize(fftSize);
    lastPhase.resize(fftSize / 2 + 1, 0.0f);
    sumPhase.resize(fftSize / 2 + 1, 0.0f);

    fftReal.resize(fftSize);
    fftImag.resize(fftSize);
    magnitude.resize(fftSize / 2 + 1);
    phase.resize(fftSize / 2 + 1);
    instFreq.resize(fftSize / 2 + 1);
    newMagnitude.resize(fftSize / 2 + 1);
    newPhase.resize(fftSize / 2 + 1);
    envelope.resize(fftSize / 2 + 1);
    warpedEnvelope.resize(fftSize / 2 + 1);

    // Hann Window
    for (int i = 0; i < fftSize; i++) {
        window[i] = 0.5f * (1.0f - cosf(2.0f * M_PI * i / (fftSize - 1)));
    }
}

void PitchShifter::process(const float* input, float* output, float pitchRatio, float formantRatio) {
    std::fill(newMagnitude.begin(), newMagnitude.end(), 0.0f);
    std::fill(newPhase.begin(), newPhase.end(), 0.0f);
    
    // 1. ANALYSIS: Apply window and prepare for FFT
    for (int i = 0; i < fftSize; i++) {
        fftReal[i] = input[i] * window[i];
        fftImag[i] = 0.0f;
    }
    
    // 2. Forward FFT (using simple DFT for demonstration - replace with juce::dsp::FFT in production)
    performFFT(fftReal.data(), fftImag.data(), fftSize, true);
    
    // 3. Convert to magnitude and phase
    for (int k = 0; k <= fftSize / 2; k++) {
        magnitude[k] = sqrtf(fftReal[k] * fftReal[k] + fftImag[k] * fftImag[k]);
        phase[k] = atan2f(fftImag[k], fftReal[k]);
    }
    
    // 4. PHASE VOCODER: Calculate instantaneous frequency
    float expectedPhaseAdvance = 2.0f * M_PI * hopSize / fftSize;
    
    for (int k = 0; k <= fftSize / 2; k++) {
        // Phase difference
        float phaseDiff = phase[k] - lastPhase[k];
        lastPhase[k] = phase[k];
        
        // Wrap phase difference to [-π, π]
        while (phaseDiff > M_PI) phaseDiff -= 2.0f * M_PI;
        while (phaseDiff < -M_PI) phaseDiff += 2.0f * M_PI;
        
        // Calculate instantaneous frequency
        float binFreq = k * expectedPhaseAdvance;
        instFreq[k] = binFreq + phaseDiff;
    }
    
    // 5. PITCH SHIFTING: Remap frequencies
    for (int k = 0; k <= fftSize / 2; k++) {
        int newBin = (int)(k * pitchRatio);
        if (newBin <= fftSize / 2) {
            newMagnitude[newBin] += magnitude[k];
            
            // Accumulate phase
            sumPhase[newBin] += instFreq[k] * pitchRatio;
            newPhase[newBin] = sumPhase[newBin];
        }
    }
    
    // 6. FORMANT PRESERVATION (if needed)
    if (std::abs(formantRatio - 1.0f) > 0.01f) {
        // Extract spectral envelope (simplified - use LPC or cepstral analysis for better results)
        int smoothWindow = 20; // Smoothing window for envelope
        
        for (int k = 0; k <= fftSize / 2; k++) {
            float sum = 0.0f;
            int count = 0;
            for (int j = std::max(0, k - smoothWindow); j <= std::min(fftSize / 2, k + smoothWindow); j++) {
                sum += magnitude[j];
                count++;
            }
            envelope[k] = sum / count;
        }
        
        // Warp the envelope
        std::fill(warpedEnvelope.begin(), warpedEnvelope.end(), 0.0f);
        for (int k = 0; k <= fftSize / 2; k++) {
            int sourceK = (int)(k / formantRatio);
            if (sourceK <= fftSize / 2) {
                warpedEnvelope[k] = envelope[sourceK];
            }
        }
        
        // Apply warped envelope to magnitude
        for (int k = 0; k <= fftSize / 2; k++) {
            if (envelope[k] > 0.0001f) {
                newMagnitude[k] *= warpedEnvelope[k] / (envelope[k] + 0.0001f);
            }
        }
    }
    
    // 7. Convert back to real/imaginary
    for (int k = 0; k <= fftSize / 2; k++) {
        fftReal[k] = newMagnitude[k] * cosf(newPhase[k]);
        fftImag[k] = newMagnitude[k] * sinf(newPhase[k]);
    }
    
    // Mirror for negative frequencies
    for (int k = fftSize / 2 + 1; k < fftSize; k++) {
        fftReal[k] = fftReal[fftSize - k];
        fftImag[k] = -fftImag[fftSize - k];
    }
    
    // 8. SYNTHESIS: Inverse FFT
    performFFT(fftReal.data(), fftImag.data(), fftSize, false);
    
    // 9. Apply window and output
    for (int i = 0; i < fftSize; i++) {
        output[i] = fftReal[i] * window[i] / fftSize;
    }
}

void PitchShifter::shiftFormants(std::vector<std::complex<float>>& spectrum, float ratio) {
    // This is now integrated into the main process() function above
    // Kept for compatibility
}

// Simple DFT implementation (replace with juce::dsp::FFT for production)
void PitchShifter::performFFT(float* real, float* imag, int n, bool forward) {
    // Bit-reversal permutation
    int j = 0;
    for (int i = 0; i < n - 1; i++) {
        if (i < j) {
            std::swap(real[i], real[j]);
            std::swap(imag[i], imag[j]);
        }
        int k = n / 2;
        while (k <= j) {
            j -= k;
            k /= 2;
        }
        j += k;
    }
    
    // Cooley-Tukey FFT
    float direction = forward ? -1.0f : 1.0f;
    for (int len = 2; len <= n; len *= 2) {
        float angle = direction * 2.0f * M_PI / len;
        float wlenReal = cosf(angle);
        float wlenImag = sinf(angle);
        
        for (int i = 0; i < n; i += len) {
            float wReal = 1.0f;
            float wImag = 0.0f;
            
            for (int j = 0; j < len / 2; j++) {
                float uReal = real[i + j];
                float uImag = imag[i + j];
                float vReal = real[i + j + len / 2] * wReal - imag[i + j + len / 2] * wImag;
                float vImag = real[i + j + len / 2] * wImag + imag[i + j + len / 2] * wReal;
                
                real[i + j] = uReal + vReal;
                imag[i + j] = uImag + vImag;
                real[i + j + len / 2] = uReal - vReal;
                imag[i + j + len / 2] = uImag - vImag;
                
                float wTempReal = wReal * wlenReal - wImag * wlenImag;
                wImag = wReal * wlenImag + wImag * wlenReal;
                wReal = wTempReal;
            }
        }
    }
}

} // namespace blink
