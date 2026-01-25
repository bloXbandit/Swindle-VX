#pragma once

#include <string>
#include <vector>

// Forward declarations to avoid including ONNX headers in header file
// Actual types will be used in the .cpp file
namespace Ort {
    class Env;
    class Session;
}

namespace blink {

/**
 * ONNX Runtime inference engine for AI voice conversion.
 * Supports RVC (Retrieval-based Voice Conversion) models.
 * 
 * To enable ONNX Runtime:
 * 1. Download ONNX Runtime SDK from https://github.com/microsoft/onnxruntime/releases
 * 2. Extract to ~/onnxruntime/
 * 3. Update CMakeLists.txt to link the library
 * 4. Define ONNX_RUNTIME_AVAILABLE in build
 */
class ONNXInference {
public:
    ONNXInference();
    ~ONNXInference();
    
    /**
     * Load an ONNX model from file.
     * @param modelPath Path to .onnx model file
     * @return true if loaded successfully
     */
    bool loadModel(const std::string& modelPath);
    
    /**
     * Process audio through the AI model (offline mode).
     * @param f0 F0 (pitch) curve in Hz
     * @param melSpec Mel-spectrogram (numFrames x numMelBands)
     * @param numFrames Number of frames
     * @param numMelBands Number of mel bands (typically 80)
     * @param output Output audio buffer
     * @param outputSize Size of output buffer
     * @return Number of samples generated
     */
    int processOffline(const float* f0, const float* melSpec, 
                      int numFrames, int numMelBands,
                      float* output, int outputSize);
    
    /**
     * Check if a model is loaded.
     */
    bool isLoaded() const;
    
    /**
     * Get model information.
     */
    std::string getModelInfo() const;

private:
    Ort::Session* session;
    Ort::Env* env;
    bool isModelLoaded;
    
    std::vector<std::string> inputNames;
    std::vector<std::string> outputNames;
    
    // Model metadata
    std::string modelPath;
    int expectedMelBands;
};

} // namespace blink
