#include "PitchShifter.h"
#include <cmath>
#include <algorithm>
#include <cstring>

namespace blink {

PitchShifter::PitchShifter(int size, int hop) : fftSize(size), hopSize(hop), osamp(size/hop) {
    // Initialize buffers
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
    
    // Circular buffers for overlap-add
    inFIFO.resize(fftSize, 0.0f);
    outFIFO.resize(fftSize, 0.0f);
    outputAccum.resize(fftSize * 2, 0.0f);
    
    inFIFOIndex = 0;
    outFIFOIndex = 0;

    // Hann Window with proper normalization
    for (int i = 0; i < fftSize; i++) {
        window[i] = 0.5f * (1.0f - cosf(2.0f * M_PI * i / (fftSize - 1)));
    }
    
    // Calculate window normalization factor for overlap-add
    windowNorm = 0.0f;
    for (int i = 0; i < fftSize; i += hopSize) {
        windowNorm += window[i % fftSize] * window[i % fftSize];
    }
    windowNorm = 1.0f / (windowNorm + 0.0001f);
}

void PitchShifter::process(const float* input, float* output, float pitchRatio, float formantRatio) {
    // Process each sample through the pitch shifter with proper overlap-add
    for (int i = 0; i < hopSize; i++) {
        // Fill input FIFO
        inFIFO[inFIFOIndex] = input[i];
        
        // Output from FIFO
        output[i] = outFIFO[outFIFOIndex];
        outFIFO[outFIFOIndex] = 0.0f;
        
        inFIFOIndex++;
        outFIFOIndex++;
        
        // Process when we have enough samples
        if (inFIFOIndex >= fftSize) {
            inFIFOIndex = 0;
            
            // Process one frame
            processFrame(inFIFO.data(), pitchRatio, formantRatio);
            
            // Overlap-add to output accumulator
            for (int k = 0; k < fftSize; k++) {
                outputAccum[k] += fftBuffer[k];
            }
            
            // Copy to output FIFO
            std::copy(outputAccum.begin(), outputAccum.begin() + hopSize, outFIFO.begin());
            
            // Shift accumulator
            std::copy(outputAccum.begin() + hopSize, outputAccum.end(), outputAccum.begin());
            std::fill(outputAccum.end() - hopSize, outputAccum.end(), 0.0f);
            
            outFIFOIndex = 0;
        }
    }
}

void PitchShifter::processFrame(const float* frame, float pitchRatio, float formantRatio) {
    std::fill(newMagnitude.begin(), newMagnitude.end(), 0.0f);
    std::fill(newPhase.begin(), newPhase.end(), 0.0f);
    
    // 1. ANALYSIS: Apply window and prepare for FFT
    for (int i = 0; i < fftSize; i++) {
        fftReal[i] = frame[i] * window[i];
        fftImag[i] = 0.0f;
    }
    
    // 2. Forward FFT
    performFFT(fftReal.data(), fftImag.data(), fftSize, true);
    
    // 3. Convert to magnitude and phase
    for (int k = 0; k <= fftSize / 2; k++) {
        magnitude[k] = sqrtf(fftReal[k] * fftReal[k] + fftImag[k] * fftImag[k]);
        phase[k] = atan2f(fftImag[k], fftReal[k]);
    }
    
    // 4. PHASE VOCODER: Calculate instantaneous frequency (SMB algorithm)
    float freqPerBin = 44100.0f / fftSize; // Assuming 44.1kHz sample rate
    float expectedPhaseAdvance = 2.0f * M_PI * hopSize / fftSize;
    
    for (int k = 0; k <= fftSize / 2; k++) {
        // Phase difference
        float phaseDiff = phase[k] - lastPhase[k];
        lastPhase[k] = phase[k];
        
        // Wrap phase difference to [-π, π]
        while (phaseDiff > M_PI) phaseDiff -= 2.0f * M_PI;
        while (phaseDiff < -M_PI) phaseDiff += 2.0f * M_PI;
        
        // Calculate true frequency (instantaneous frequency)
        float trueFreq = k * freqPerBin + (phaseDiff / expectedPhaseAdvance) * freqPerBin * hopSize / (2.0f * M_PI);
        instFreq[k] = trueFreq;
    }
    
    // 5. PITCH SHIFTING: Remap frequencies with interpolation
    for (int k = 0; k <= fftSize / 2; k++) {
        float newFreq = instFreq[k] * pitchRatio;
        int newBin = (int)(newFreq / freqPerBin);
        
        if (newBin >= 0 && newBin <= fftSize / 2) {
            // Linear interpolation for smoother results
            float frac = (newFreq / freqPerBin) - newBin;
            
            newMagnitude[newBin] += magnitude[k] * (1.0f - frac);
            if (newBin + 1 <= fftSize / 2) {
                newMagnitude[newBin + 1] += magnitude[k] * frac;
            }
            
            // Phase accumulation
            sumPhase[newBin] += (phase[k] - lastPhase[k]) * pitchRatio;
            newPhase[newBin] = sumPhase[newBin];
        }
    }
    
    // 6. FORMANT PRESERVATION
    if (std::abs(formantRatio - 1.0f) > 0.01f) {
        // Extract spectral envelope using moving average
        int smoothWindow = std::max(5, fftSize / 100);
        
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
            float sourceK = k / formantRatio;
            int k1 = (int)sourceK;
            int k2 = k1 + 1;
            float frac = sourceK - k1;
            
            if (k1 >= 0 && k1 <= fftSize / 2) {
                warpedEnvelope[k] = envelope[k1] * (1.0f - frac);
            }
            if (k2 >= 0 && k2 <= fftSize / 2) {
                warpedEnvelope[k] += envelope[k2] * frac;
            }
        }
        
        // Apply warped envelope to magnitude
        for (int k = 0; k <= fftSize / 2; k++) {
            float origEnv = envelope[(int)(k * pitchRatio)];
            if (origEnv > 0.0001f && warpedEnvelope[k] > 0.0001f) {
                newMagnitude[k] *= warpedEnvelope[k] / origEnv;
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
    
    // 9. Apply window and normalize
    for (int i = 0; i < fftSize; i++) {
        fftBuffer[i] = fftReal[i] * window[i] * windowNorm / fftSize;
    }
}

void PitchShifter::shiftFormants(std::vector<std::complex<float>>& spectrum, float ratio) {
    // Integrated into processFrame - kept for compatibility
}

// Cooley-Tukey FFT implementation
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
