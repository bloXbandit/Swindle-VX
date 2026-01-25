#include "ONNXInference.h"
#include <iostream>
#include <fstream>

// NOTE: To compile this, you need to:
// 1. Download ONNX Runtime from https://github.com/microsoft/onnxruntime/releases
// 2. Add to CMakeLists.txt:
//    include_directories(/path/to/onnxruntime/include)
//    target_link_libraries(VocalSuitePro PRIVATE /path/to/onnxruntime/lib/libonnxruntime.dylib)
// 3. Uncomment the #include below:

// #include <onnxruntime/core/session/onnxruntime_cxx_api.h>

namespace blink {

ONNXInference::ONNXInference() : session(nullptr), env(nullptr), isModelLoaded(false) {
    #ifdef ONNX_RUNTIME_AVAILABLE
    env = new Ort::Env(ORT_LOGGING_LEVEL_WARNING, "VocalSuiteAI");
    #endif
}

ONNXInference::~ONNXInference() {
    #ifdef ONNX_RUNTIME_AVAILABLE
    delete session;
    delete env;
    #endif
}

bool ONNXInference::loadModel(const std::string& modelPath) {
    #ifdef ONNX_RUNTIME_AVAILABLE
    try {
        // Check if file exists
        std::ifstream file(modelPath);
        if (!file.good()) {
            std::cerr << "Model file not found: " << modelPath << std::endl;
            return false;
        }
        
        // Configure session options
        Ort::SessionOptions sessionOptions;
        sessionOptions.SetIntraOpNumThreads(2); // Use 2 threads for inference
        sessionOptions.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
        
        // Enable CPU optimizations
        sessionOptions.SetExecutionMode(ExecutionMode::ORT_SEQUENTIAL);
        
        // Create session
        session = new Ort::Session(*env, modelPath.c_str(), sessionOptions);
        
        // Get input/output names and shapes
        Ort::AllocatorWithDefaultOptions allocator;
        
        // Input info
        size_t numInputNodes = session->GetInputCount();
        inputNames.clear();
        for (size_t i = 0; i < numInputNodes; i++) {
            auto inputName = session->GetInputNameAllocated(i, allocator);
            inputNames.push_back(inputName.get());
        }
        
        // Output info
        size_t numOutputNodes = session->GetOutputCount();
        outputNames.clear();
        for (size_t i = 0; i < numOutputNodes; i++) {
            auto outputName = session->GetOutputNameAllocated(i, allocator);
            outputNames.push_back(outputName.get());
        }
        
        isModelLoaded = true;
        std::cout << "AI Model loaded successfully: " << modelPath << std::endl;
        std::cout << "Inputs: " << numInputNodes << ", Outputs: " << numOutputNodes << std::endl;
        
        return true;
        
    } catch (const Ort::Exception& e) {
        std::cerr << "ONNX Runtime error: " << e.what() << std::endl;
        isModelLoaded = false;
        return false;
    }
    #else
    std::cerr << "ONNX Runtime not available. Compile with ONNX_RUNTIME_AVAILABLE defined." << std::endl;
    return false;
    #endif
}

void ONNXInference::process(const float* input, float* output, int numSamples) {
    #ifdef ONNX_RUNTIME_AVAILABLE
    if (!session || !isModelLoaded) {
        // Fallback: Copy input to output if no model loaded
        for (int i = 0; i < numSamples; i++) {
            output[i] = input[i];
        }
        return;
    }

    try {
        // 1. FEATURE EXTRACTION
        // For RVC models, typical input is:
        // - F0 (fundamental frequency) curve
        // - Spectral features (mel-spectrogram or similar)
        // - Speaker embedding (if multi-speaker model)
        
        // Simplified: We'll assume the model expects raw audio
        // Real RVC models need proper feature extraction
        
        // 2. PREPARE INPUT TENSOR
        auto memoryInfo = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
        
        std::vector<int64_t> inputShape = {1, numSamples}; // Batch size 1, numSamples length
        std::vector<float> inputData(input, input + numSamples);
        
        Ort::Value inputTensor = Ort::Value::CreateTensor<float>(
            memoryInfo, 
            inputData.data(), 
            inputData.size(),
            inputShape.data(), 
            inputShape.size()
        );
        
        // 3. RUN INFERENCE
        std::vector<const char*> inputNamesPtr;
        for (const auto& name : inputNames) {
            inputNamesPtr.push_back(name.c_str());
        }
        
        std::vector<const char*> outputNamesPtr;
        for (const auto& name : outputNames) {
            outputNamesPtr.push_back(name.c_str());
        }
        
        auto outputTensors = session->Run(
            Ort::RunOptions{nullptr},
            inputNamesPtr.data(),
            &inputTensor,
            1,
            outputNamesPtr.data(),
            outputNames.size()
        );
        
        // 4. EXTRACT OUTPUT
        float* outputData = outputTensors[0].GetTensorMutableData<float>();
        auto outputShape = outputTensors[0].GetTensorTypeAndShapeInfo().GetShape();
        
        int outputSize = 1;
        for (auto dim : outputShape) {
            outputSize *= dim;
        }
        
        // Copy to output buffer
        int copySize = std::min(numSamples, outputSize);
        for (int i = 0; i < copySize; i++) {
            output[i] = outputData[i];
        }
        
        // Pad with zeros if output is shorter
        for (int i = copySize; i < numSamples; i++) {
            output[i] = 0.0f;
        }
        
    } catch (const Ort::Exception& e) {
        std::cerr << "ONNX inference error: " << e.what() << std::endl;
        // Fallback to passthrough
        for (int i = 0; i < numSamples; i++) {
            output[i] = input[i];
        }
    }
    #else
    // No ONNX Runtime - passthrough
    for (int i = 0; i < numSamples; i++) {
        output[i] = input[i];
    }
    #endif
}

bool ONNXInference::isLoaded() const {
    return isModelLoaded;
}

} // namespace blink
