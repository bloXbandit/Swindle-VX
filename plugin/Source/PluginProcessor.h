#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "DSP/PitchDetector.h"
#include "DSP/PitchShifter.h"
#include "DSP/PitchCorrector.h"
#include "DSP/VoiceCharacter.h"
#include "AI/ONNXInference.h"

class VocalSuiteAudioProcessor : public juce::AudioProcessor {
public:
    VocalSuiteAudioProcessor();
    ~VocalSuiteAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // Public for UI access
    juce::AudioProcessorValueTreeState parameters;
    
    // Getters for visualization
    float getCurrentPitch() const { return currentPitch; }
    float getTargetPitch() const { return targetPitch; }
    
    void loadVoiceModel(const std::string& modelId, const std::string& modelType);

    void startCapture();
    void stopCapture();
    void convertCapturedAudio(const std::string& modelId, int pitchShift, float formantShift);

private:
    // DSP Modules
    blink::PitchDetector pitchDetector;
    blink::PitchShifter pitchShifter;
    blink::PitchCorrector pitchCorrector;
    blink::VoiceCharacter voiceCharacter;
    blink::ONNXInference aiProcessor;

    void resetPitchShiftState();
    
    // Parameters
    std::atomic<float>* correctionAmount = nullptr;
    std::atomic<float>* correctionSpeed = nullptr;
    std::atomic<float>* pitchShift = nullptr;
    std::atomic<float>* formantShift = nullptr;
    std::atomic<float>* breathAmount = nullptr;
    std::atomic<float>* resonanceAmount = nullptr;
    std::atomic<float>* aiBlend = nullptr;
    std::atomic<float>* keyParam = nullptr;
    std::atomic<float>* scaleParam = nullptr;
    
    // State
    double currentSampleRate = 44100.0;
    int maxBlockSize = 0;
    float currentPitch = 0.0f;
    float targetPitch = 0.0f;
    
    // Working buffers
    std::vector<float> workingBuffer;
    std::vector<float> aiOutputBuffer;

    static constexpr int pitchShiftFrameSize = 2048;
    static constexpr int pitchShiftHopSize = 512;

    std::vector<float> pitchInRing;
    std::vector<float> pitchOlaRing;
    std::vector<float> pitchOlaGainRing;
    std::vector<float> pitchFrame;
    std::vector<float> pitchFrameOut;
    std::vector<float> pitchOlaWindow;
    std::vector<float> pitchDetectFrame;
    int pitchRingPos = 0;
    int pitchSamplesFilled = 0;
    int pitchSamplesSinceProcess = 0;

    std::vector<float> captureBuffer;
    std::atomic<bool> isCapturing { false };
    int captureWritePos = 0;
    int captureSamplesRecorded = 0;
    juce::File lastCapturedFile;
    std::atomic<bool> captureWriteInProgress { false };
    juce::WaitableEvent captureWriteFinished;

    // Parameter layout
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VocalSuiteAudioProcessor)
};
