#include "ONNXInference.h"
#include <iostream>
#include <fstream>

// NOTE: To compile this, you need to:
// 1. Download ONNX Runtime from https://github.com/microsoft/onnxruntime/releases
//    Recommended: onnxruntime-osx-arm64-1.17.0.tgz (for Apple Silicon)
//    or: onnxruntime-osx-x86_64-1.17.0.tgz (for Intel Mac)
// 2. Extract to ~/onnxruntime/
// 3. Update CMakeLists.txt:
//    include_directories(~/onnxruntime/include)
//    target_link_libraries(VocalSuitePro PRIVATE ~/onnxruntime/lib/libonnxruntime.dylib)
//    target_compile_definitions(VocalSuitePro PRIVATE ONNX_RUNTIME_AVAILABLE=1)
// 4. Uncomment the #include below:

#ifdef ONNX_RUNTIME_AVAILABLE
#include <onnxruntime/core/session/onnxruntime_cxx_api.h>
#endif

namespace blink {

ONNXInference::ONNXInference() 
    : session(nullptr), env(nullptr), isModelLoaded(false), expectedMelBands(80) {
    #ifdef ONNX_RUNTIME_AVAILABLE
    env = new Ort::Env(ORT_LOGGING_LEVEL_WARNING, "VocalSuiteAI");
    std::cout << "ONNX Runtime initialized" << std::endl;
    #else
    std::cout << "ONNX Runtime not available - AI voice cloning disabled" << std::endl;
    std::cout << "To enable: Download ONNX Runtime and update CMakeLists.txt" << std::endl;
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
        file.close();
        
        // Configure session options
        Ort::SessionOptions sessionOptions;
        sessionOptions.SetIntraOpNumThreads(4); // Use 4 threads for inference
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
            std::cout << "Input " << i << ": " << inputName.get() << std::endl;
        }
        
        // Output info
        size_t numOutputNodes = session->GetOutputCount();
        outputNames.clear();
        for (size_t i = 0; i < numOutputNodes; i++) {
            auto outputName = session->GetOutputNameAllocated(i, allocator);
            outputNames.push_back(outputName.get());
            std::cout << "Output " << i << ": " << outputName.get() << std::endl;
        }
        
        this->modelPath = modelPath;
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
    std::cerr << "ONNX Runtime not available - cannot load model" << std::endl;
    return false;
    #endif
}

int ONNXInference::processOffline(const float* f0, const float* melSpec,
                                  int numFrames, int numMelBands,
                                  float* output, int outputSize) {
    #ifdef ONNX_RUNTIME_AVAILABLE
    if (!isModelLoaded) {
        std::cerr << "No model loaded" << std::endl;
        // Pass through unchanged
        return 0;
    }
    
    try {
        // Create ONNX tensors
        Ort::MemoryInfo memoryInfo = Ort::MemoryInfo::CreateCpu(
            OrtAllocatorType::OrtArenaAllocator, OrtMemType::OrtMemTypeDefault);
        
        // Input 1: F0 curve (shape: [1, numFrames])
        std::vector<int64_t> f0Shape = {1, numFrames};
        Ort::Value f0Tensor = Ort::Value::CreateTensor<float>(
            memoryInfo, const_cast<float*>(f0), numFrames, f0Shape.data(), f0Shape.size());
        
        // Input 2: Mel-spectrogram (shape: [1, numMelBands, numFrames])
        std::vector<int64_t> melShape = {1, numMelBands, numFrames};
        Ort::Value melTensor = Ort::Value::CreateTensor<float>(
            memoryInfo, const_cast<float*>(melSpec), numFrames * numMelBands,
            melShape.data(), melShape.size());
        
        // Prepare inputs
        std::vector<Ort::Value> inputTensors;
        inputTensors.push_back(std::move(f0Tensor));
        inputTensors.push_back(std::move(melTensor));
        
        // Prepare input/output names
        std::vector<const char*> inputNamesCStr;
        for (const auto& name : inputNames) {
            inputNamesCStr.push_back(name.c_str());
        }
        
        std::vector<const char*> outputNamesCStr;
        for (const auto& name : outputNames) {
            outputNamesCStr.push_back(name.c_str());
        }
        
        // Run inference
        auto outputTensors = session->Run(
            Ort::RunOptions{nullptr},
            inputNamesCStr.data(), inputTensors.data(), inputTensors.size(),
            outputNamesCStr.data(), outputNamesCStr.size()
        );
        
        // Extract output audio
        float* outputData = outputTensors[0].GetTensorMutableData<float>();
        auto outputShape = outputTensors[0].GetTensorTypeAndShapeInfo().GetShape();
        
        int numOutputSamples = 1;
        for (auto dim : outputShape) {
            numOutputSamples *= (int)dim;
        }
        
        // Copy to output buffer
        int samplesToCopy = std::min(numOutputSamples, outputSize);
        std::copy(outputData, outputData + samplesToCopy, output);
        
        std::cout << "AI inference complete: " << samplesToCopy << " samples generated" << std::endl;
        
        return samplesToCopy;
        
    } catch (const Ort::Exception& e) {
        std::cerr << "ONNX inference error: " << e.what() << std::endl;
        return 0;
    }
    #else
    // ONNX not available - pass through unchanged
    std::cerr << "ONNX Runtime not available" << std::endl;
    return 0;
    #endif
}

bool ONNXInference::isLoaded() const {
    return isModelLoaded;
}

std::string ONNXInference::getModelInfo() const {
    if (!isModelLoaded) {
        return "No model loaded";
    }
    
    std::string info = "Model: " + modelPath + "\n";
    info += "Inputs: " + std::to_string(inputNames.size()) + "\n";
    info += "Outputs: " + std::to_string(outputNames.size()) + "\n";
    
    return info;
}

} // namespace blink
