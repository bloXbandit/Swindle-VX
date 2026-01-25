#pragma once

#include <JuceHeader.h>
#include <vector>

namespace blink {

/**
 * Mel-Spectrogram generator for AI voice models.
 * Converts audio to mel-scale frequency representation.
 */
class MelSpectrogram {
public:
    MelSpectrogram(int fftSize = 2048, int hopSize = 512, int numMelBands = 80);
    ~MelSpectrogram() = default;

    /**
     * Set sample rate for frequency calculations.
     */
    void setSampleRate(double sampleRate);

    /**
     * Process audio frame and generate mel-spectrogram.
     * @param buffer Input audio buffer
     * @param numSamples Number of samples
     * @param output Output mel-spectrogram (numMelBands values)
     */
    void processFrame(const float* buffer, int numSamples, float* output);

    /**
     * Get number of mel bands.
     */
    int getNumMelBands() const { return numMelBands; }

    /**
     * Get hop size.
     */
    int getHopSize() const { return hopSize; }

private:
    int fftSize;
    int hopSize;
    int numMelBands;
    double sampleRate;
    
    // JUCE FFT
    std::unique_ptr<juce::dsp::FFT> fft;
    int fftOrder;
    
    // Buffers
    std::vector<float> window;
    std::vector<float> fftData;
    std::vector<float> powerSpectrum;
    
    // Mel filterbank
    std::vector<std::vector<float>> melFilterbank;
    
    // Initialize mel filterbank
    void initMelFilterbank();
    
    // Helper functions
    float hzToMel(float hz);
    float melToHz(float mel);
    int calculateFFTOrder(int size);
};

} // namespace blink
