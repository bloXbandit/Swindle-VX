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
 * 1. Download ONNX Runtime SDK
 * 2. Add include path to CMakeLists.txt
 * 3. Link libonnxruntime library
 * 4. Define ONNX_RUNTIME_AVAILABLE in build
 */
class ONNXInference {
public:
    ONNXInference();
    ~ONNXInference();
    
    /**
     * Load an ONNX model from file
     * @param modelPath Path to .onnx model file
     * @return true if loaded successfully
     */
    bool loadModel(const std::string& modelPath);
    
    /**
     * Process audio through the AI model
     * @param input Input audio buffer
     * @param output Output audio buffer
     * @param numSamples Number of samples to process
     */
    void process(const float* input, float* output, int numSamples);
    
    /**
     * Check if a model is loaded
     */
    bool isLoaded() const;

private:
    Ort::Session* session;
    Ort::Env* env;
    bool isModelLoaded;
    
    std::vector<std::string> inputNames;
    std::vector<std::string> outputNames;
};

} // namespace blink
