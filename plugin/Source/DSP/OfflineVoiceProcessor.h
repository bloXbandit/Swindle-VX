#pragma once

#include "F0Extractor.h"
#include "MelSpectrogram.h"
#include "../AI/ONNXInference.h"
#include <vector>

namespace blink {

/**
 * Offline voice processor for AI voice transformation.
 * Processes entire audio buffers with F0 extraction, mel-spectrogram, and ONNX inference.
 */
class OfflineVoiceProcessor {
public:
    OfflineVoiceProcessor();
    ~OfflineVoiceProcessor() = default;

    /**
     * Set sample rate.
     */
    void setSampleRate(double sampleRate);

    /**
     * Load an ONNX voice model.
     * @param modelPath Path to .onnx model file
     * @return true if loaded successfully
     */
    bool loadVoiceModel(const std::string& modelPath);

    /**
     * Process audio buffer offline (for pre-recorded vocals).
     * @param input Input audio buffer
     * @param output Output audio buffer
     * @param numSamples Number of samples
     * @return true if processing successful
     */
    bool processOffline(const float* input, float* output, int numSamples);

    /**
     * Check if a voice model is loaded.
     */
    bool isModelLoaded() const;

    /**
     * Get processing status message.
     */
    std::string getStatusMessage() const;

private:
    F0Extractor f0Extractor;
    MelSpectrogram melSpec;
    ONNXInference onnxInference;
    
    double sampleRate;
    int hopSize;
    int fftSize;
    
    std::string statusMessage;
    
    // Extract features from audio
    bool extractFeatures(const float* input, int numSamples,
                        std::vector<float>& f0Curve,
                        std::vector<float>& melSpecData);
};

} // namespace blink
