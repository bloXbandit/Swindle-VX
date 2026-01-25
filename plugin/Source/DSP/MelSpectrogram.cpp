#include "MelSpectrogram.h"
#include <cmath>
#include <algorithm>

namespace blink {

MelSpectrogram::MelSpectrogram(int fftSize, int hopSize, int numMelBands)
    : fftSize(fftSize), hopSize(hopSize), numMelBands(numMelBands), sampleRate(44100.0) {
    
    // Calculate FFT order and create JUCE FFT
    fftOrder = calculateFFTOrder(fftSize);
    fft = std::make_unique<juce::dsp::FFT>(fftOrder);
    
    // Initialize buffers
    window.resize(fftSize);
    fftData.resize(fftSize * 2, 0.0f);
    powerSpectrum.resize(fftSize / 2 + 1);
    
    // Hann window
    for (int i = 0; i < fftSize; i++) {
        window[i] = 0.5f * (1.0f - cosf(2.0f * juce::MathConstants<float>::pi * i / (fftSize - 1)));
    }
    
    // Initialize mel filterbank
    initMelFilterbank();
}

void MelSpectrogram::setSampleRate(double sampleRate) {
    this->sampleRate = sampleRate;
    initMelFilterbank();  // Rebuild filterbank for new sample rate
}

int MelSpectrogram::calculateFFTOrder(int size) {
    int order = 0;
    int powerOfTwo = 1;
    while (powerOfTwo < size) {
        powerOfTwo *= 2;
        order++;
    }
    return order;
}

float MelSpectrogram::hzToMel(float hz) {
    // Convert Hz to mel scale
    return 2595.0f * log10f(1.0f + hz / 700.0f);
}

float MelSpectrogram::melToHz(float mel) {
    // Convert mel scale to Hz
    return 700.0f * (powf(10.0f, mel / 2595.0f) - 1.0f);
}

void MelSpectrogram::initMelFilterbank() {
    // Create mel filterbank (triangular filters)
    melFilterbank.clear();
    melFilterbank.resize(numMelBands);
    
    // Frequency range: 0 Hz to Nyquist
    float minFreq = 0.0f;
    float maxFreq = (float)sampleRate / 2.0f;
    
    // Convert to mel scale
    float minMel = hzToMel(minFreq);
    float maxMel = hzToMel(maxFreq);
    
    // Create mel-spaced frequency points
    std::vector<float> melPoints(numMelBands + 2);
    for (int i = 0; i < numMelBands + 2; i++) {
        melPoints[i] = minMel + (maxMel - minMel) * i / (numMelBands + 1);
    }
    
    // Convert back to Hz
    std::vector<float> hzPoints(numMelBands + 2);
    for (int i = 0; i < numMelBands + 2; i++) {
        hzPoints[i] = melToHz(melPoints[i]);
    }
    
    // Convert Hz to FFT bins
    std::vector<int> binPoints(numMelBands + 2);
    for (int i = 0; i < numMelBands + 2; i++) {
        binPoints[i] = (int)floorf((fftSize + 1) * hzPoints[i] / sampleRate);
    }
    
    // Create triangular filters
    for (int i = 0; i < numMelBands; i++) {
        melFilterbank[i].resize(fftSize / 2 + 1, 0.0f);
        
        int leftBin = binPoints[i];
        int centerBin = binPoints[i + 1];
        int rightBin = binPoints[i + 2];
        
        // Left slope (rising)
        for (int k = leftBin; k < centerBin; k++) {
            if (centerBin > leftBin) {
                melFilterbank[i][k] = (float)(k - leftBin) / (centerBin - leftBin);
            }
        }
        
        // Right slope (falling)
        for (int k = centerBin; k < rightBin; k++) {
            if (rightBin > centerBin) {
                melFilterbank[i][k] = (float)(rightBin - k) / (rightBin - centerBin);
            }
        }
    }
}

void MelSpectrogram::processFrame(const float* buffer, int numSamples, float* output) {
    // Ensure we have enough samples
    int samplesToUse = std::min(numSamples, fftSize);
    
    // Apply window and prepare for FFT
    for (int i = 0; i < samplesToUse; i++) {
        fftData[i * 2] = buffer[i] * window[i];  // Real part
        fftData[i * 2 + 1] = 0.0f;               // Imaginary part
    }
    
    // Zero-pad if needed
    for (int i = samplesToUse; i < fftSize; i++) {
        fftData[i * 2] = 0.0f;
        fftData[i * 2 + 1] = 0.0f;
    }
    
    // Forward FFT
    fft->performFrequencyOnlyForwardTransform(fftData.data());
    
    // Calculate power spectrum
    for (int k = 0; k <= fftSize / 2; k++) {
        float real = fftData[k * 2];
        float imag = fftData[k * 2 + 1];
        powerSpectrum[k] = real * real + imag * imag;
    }
    
    // Apply mel filterbank
    for (int i = 0; i < numMelBands; i++) {
        float melEnergy = 0.0f;
        for (int k = 0; k <= fftSize / 2; k++) {
            melEnergy += powerSpectrum[k] * melFilterbank[i][k];
        }
        
        // Log scale (standard for mel-spectrograms)
        output[i] = log10f(melEnergy + 1e-10f);  // Add small value to avoid log(0)
    }
}

} // namespace blink
