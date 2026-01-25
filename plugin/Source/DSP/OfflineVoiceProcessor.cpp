#include "OfflineVoiceProcessor.h"
#include <iostream>
#include <cmath>

namespace blink {

OfflineVoiceProcessor::OfflineVoiceProcessor()
    : f0Extractor(512), melSpec(2048, 512, 80),
      sampleRate(44100.0), hopSize(512), fftSize(2048),
      statusMessage("Ready") {
}

void OfflineVoiceProcessor::setSampleRate(double sampleRate) {
    this->sampleRate = sampleRate;
    f0Extractor.setSampleRate(sampleRate);
    melSpec.setSampleRate(sampleRate);
}

bool OfflineVoiceProcessor::loadVoiceModel(const std::string& modelPath) {
    statusMessage = "Loading model: " + modelPath;
    bool success = onnxInference.loadModel(modelPath);
    
    if (success) {
        statusMessage = "Model loaded successfully";
    } else {
        statusMessage = "Failed to load model";
    }
    
    return success;
}

bool OfflineVoiceProcessor::extractFeatures(const float* input, int numSamples,
                                           std::vector<float>& f0Curve,
                                           std::vector<float>& melSpecData) {
    // Calculate number of frames
    int numFrames = (numSamples - fftSize) / hopSize + 1;
    
    if (numFrames <= 0) {
        statusMessage = "Audio too short for processing";
        return false;
    }
    
    // Clear previous data
    f0Curve.clear();
    melSpecData.clear();
    
    // Reset F0 extractor
    f0Extractor.reset();
    
    // Process each frame
    int numMelBands = melSpec.getNumMelBands();
    std::vector<float> melFrame(numMelBands);
    
    for (int frame = 0; frame < numFrames; frame++) {
        int offset = frame * hopSize;
        
        // Extract F0
        float f0 = f0Extractor.processSample(input + offset, fftSize);
        f0Curve.push_back(f0);
        
        // Extract mel-spectrogram
        melSpec.processFrame(input + offset, fftSize, melFrame.data());
        
        // Append to mel-spec data (transpose: [numFrames x numMelBands])
        for (int i = 0; i < numMelBands; i++) {
            melSpecData.push_back(melFrame[i]);
        }
    }
    
    statusMessage = "Features extracted: " + std::to_string(numFrames) + " frames";
    return true;
}

bool OfflineVoiceProcessor::processOffline(const float* input, float* output, int numSamples) {
    if (!onnxInference.isLoaded()) {
        statusMessage = "No voice model loaded";
        // Pass through unchanged
        std::copy(input, input + numSamples, output);
        return false;
    }
    
    statusMessage = "Processing audio...";
    
    // Step 1: Extract features
    std::vector<float> f0Curve;
    std::vector<float> melSpecData;
    
    if (!extractFeatures(input, numSamples, f0Curve, melSpecData)) {
        // Pass through unchanged
        std::copy(input, input + numSamples, output);
        return false;
    }
    
    int numFrames = (int)f0Curve.size();
    int numMelBands = melSpec.getNumMelBands();
    
    // Step 2: Run ONNX inference
    std::vector<float> aiOutput(numSamples);
    
    int samplesGenerated = onnxInference.processOffline(
        f0Curve.data(), melSpecData.data(),
        numFrames, numMelBands,
        aiOutput.data(), numSamples
    );
    
    if (samplesGenerated > 0) {
        // Copy AI output
        std::copy(aiOutput.begin(), aiOutput.begin() + std::min(samplesGenerated, numSamples), output);
        
        // Zero-pad if needed
        if (samplesGenerated < numSamples) {
            std::fill(output + samplesGenerated, output + numSamples, 0.0f);
        }
        
        statusMessage = "Processing complete: " + std::to_string(samplesGenerated) + " samples";
        return true;
    } else {
        // ONNX failed - pass through unchanged
        statusMessage = "ONNX inference failed - passthrough";
        std::copy(input, input + numSamples, output);
        return false;
    }
}

bool OfflineVoiceProcessor::isModelLoaded() const {
    return onnxInference.isLoaded();
}

std::string OfflineVoiceProcessor::getStatusMessage() const {
    return statusMessage;
}

} // namespace blink
