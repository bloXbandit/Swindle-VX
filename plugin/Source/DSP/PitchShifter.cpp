#include "PitchShifter.h"
#include <cmath>
#include <algorithm>
#include <cstring>

namespace blink {

PitchShifter::PitchShifter(int size, int hop) 
    : fftSize(size), hopSize(hop), osamp(size/hop), sampleRate(44100.0),
      transientDetector(size), bypassPitchShiftOnTransient(true),
      lpcAnalyzer(12), useLPCFormants(true) {
    
    lpcEnvelope.resize(fftSize / 2 + 1, 0.0f);
    warpedLPCEnvelope.resize(fftSize / 2 + 1, 0.0f);
    
    // Calculate FFT order and create JUCE FFT
    fftOrder = calculateFFTOrder(fftSize);
    fft = std::make_unique<juce::dsp::FFT>(fftOrder);
    
    // Initialize buffers
    window.resize(fftSize);
    fftBuffer.resize(fftSize);
    lastPhase.resize(fftSize / 2 + 1, 0.0f);
    sumPhase.resize(fftSize / 2 + 1, 0.0f);
    
    // JUCE FFT uses interleaved real/imag format
    fftData.resize(fftSize * 2, 0.0f);
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
        window[i] = 0.5f * (1.0f - cosf(2.0f * juce::MathConstants<float>::pi * i / (fftSize - 1)));
    }
    
    // Calculate window normalization factor for overlap-add
    windowNorm = 0.0f;
    for (int i = 0; i < fftSize; i += hopSize) {
        windowNorm += window[i % fftSize] * window[i % fftSize];
    }
    windowNorm = 1.0f / (windowNorm + 0.0001f);
    
    // Initialize frequency per bin
    freqPerBin = (float)sampleRate / fftSize;
}

void PitchShifter::setSampleRate(double newSampleRate) {
    sampleRate = newSampleRate;
    freqPerBin = (float)sampleRate / fftSize;
}

int PitchShifter::calculateFFTOrder(int size) {
    int order = 0;
    int powerOfTwo = 1;
    while (powerOfTwo < size) {
        powerOfTwo *= 2;
        order++;
    }
    return order;
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
    // Detect transients (consonants, attacks)
    bool isTransient = transientDetector.detectTransient(frame, fftSize);
    
    // Bypass pitch shifting on transients to preserve clarity
    if (bypassPitchShiftOnTransient && isTransient && std::abs(pitchRatio - 1.0f) > 0.01f) {
        // Copy input to output with window applied
        for (int i = 0; i < fftSize; i++) {
            fftBuffer[i] = frame[i] * window[i] * windowNorm;
        }
        return;
    }
    
    std::fill(newMagnitude.begin(), newMagnitude.end(), 0.0f);
    std::fill(newPhase.begin(), newPhase.end(), 0.0f);
    
    // 1. ANALYSIS: Apply window and prepare for FFT
    for (int i = 0; i < fftSize; i++) {
        fftData[i * 2] = frame[i] * window[i];  // Real part
        fftData[i * 2 + 1] = 0.0f;              // Imaginary part
    }
    
    // 2. Forward FFT using JUCE
    fft->performFrequencyOnlyForwardTransform(fftData.data());
    
    // 3. Convert to magnitude and phase
    for (int k = 0; k <= fftSize / 2; k++) {
        float real = fftData[k * 2];
        float imag = fftData[k * 2 + 1];
        magnitude[k] = sqrtf(real * real + imag * imag);
        phase[k] = atan2f(imag, real);
    }
    
    // 4. INSTANTANEOUS FREQUENCY: Calculate true frequency of each bin
    float expectedPhaseDiff = 2.0f * juce::MathConstants<float>::pi * hopSize / fftSize;
    
    for (int k = 0; k <= fftSize / 2; k++) {
        // Phase difference
        float phaseDiff = phase[k] - lastPhase[k];
        lastPhase[k] = phase[k];
        
        // Unwrap phase (bring into -π to π range)
        phaseDiff -= k * expectedPhaseDiff;
        int qpd = (int)(phaseDiff / juce::MathConstants<float>::pi);
        if (qpd >= 0) qpd += qpd & 1;
        else qpd -= qpd & 1;
        phaseDiff -= juce::MathConstants<float>::pi * qpd;
        
        // Calculate instantaneous frequency
        float deviation = phaseDiff * osamp / (2.0f * juce::MathConstants<float>::pi);
        instFreq[k] = (k + deviation) * freqPerBin;
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
        if (useLPCFormants) {
            // LPC-BASED FORMANT SHIFTING (Professional Quality)
            
            // Step 1: Analyze vocal tract with LPC
            lpcAnalyzer.analyze(frame, fftSize);
            
            // Step 2: Get spectral envelope from LPC
            std::vector<float> frequencies(fftSize / 2 + 1);
            for (int k = 0; k <= fftSize / 2; k++) {
                frequencies[k] = k * freqPerBin;
            }
            lpcAnalyzer.getSpectralEnvelope(frequencies.data(), fftSize / 2 + 1,
                                           lpcEnvelope.data(), (float)sampleRate);
            
            // Step 3: Warp the LPC envelope
            std::fill(warpedLPCEnvelope.begin(), warpedLPCEnvelope.end(), 0.0f);
            for (int k = 0; k <= fftSize / 2; k++) {
                float targetFreq = k * freqPerBin;
                float sourceFreq = targetFreq / formantRatio;
                float sourceBin = sourceFreq / freqPerBin;
                
                int k1 = (int)sourceBin;
                int k2 = k1 + 1;
                float frac = sourceBin - k1;
                
                if (k1 >= 0 && k1 <= fftSize / 2) {
                    warpedLPCEnvelope[k] = lpcEnvelope[k1] * (1.0f - frac);
                }
                if (k2 >= 0 && k2 <= fftSize / 2) {
                    warpedLPCEnvelope[k] += lpcEnvelope[k2] * frac;
                }
            }
            
            // Step 4: Apply warped envelope to magnitude
            for (int k = 0; k <= fftSize / 2; k++) {
                // Remove original envelope, apply warped envelope
                if (lpcEnvelope[k] > 0.0001f) {
                    float correction = warpedLPCEnvelope[k] / lpcEnvelope[k];
                    newMagnitude[k] *= correction;
                }
            }
        } else {
            // FALLBACK: Simple moving average (original method)
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
                int origBin = (int)(k * pitchRatio);
                if (origBin >= 0 && origBin <= fftSize / 2) {
                    float origEnv = envelope[origBin];
                    if (origEnv > 0.0001f && warpedEnvelope[k] > 0.0001f) {
                        newMagnitude[k] *= warpedEnvelope[k] / origEnv;
                    }
                }
            }
        }
    }
    
    // 7. Convert back to real/imaginary for inverse FFT
    for (int k = 0; k <= fftSize / 2; k++) {
        fftData[k * 2] = newMagnitude[k] * cosf(newPhase[k]);
        fftData[k * 2 + 1] = newMagnitude[k] * sinf(newPhase[k]);
    }
    
    // Mirror for negative frequencies (JUCE FFT requirement)
    for (int k = fftSize / 2 + 1; k < fftSize; k++) {
        fftData[k * 2] = fftData[(fftSize - k) * 2];
        fftData[k * 2 + 1] = -fftData[(fftSize - k) * 2 + 1];
    }
    
    // 8. Inverse FFT using JUCE
    fft->performRealOnlyInverseTransform(fftData.data());
    
    // 9. Apply window and normalize
    for (int i = 0; i < fftSize; i++) {
        fftBuffer[i] = fftData[i * 2] * window[i] * windowNorm;
    }
}

void PitchShifter::shiftFormants(std::vector<std::complex<float>>& spectrum, float ratio) {
    // Integrated into processFrame - kept for compatibility
}

} // namespace blink
