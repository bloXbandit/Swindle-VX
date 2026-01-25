#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <cmath>
#include <algorithm>

VocalSuiteAudioProcessor::VocalSuiteAudioProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::mono(), true)
                                     .withOutput("Output", juce::AudioChannelSet::mono(), true)),
      parameters(*this, nullptr, "PARAMETERS", createParameterLayout()),
      pitchDetector(44100.0, 2048),
      pitchShifter(2048, 512),
      pitchCorrector(),
      voiceCharacter(),
      aiProcessor()
{
    // Get parameter pointers
    correctionAmount = parameters.getRawParameterValue("correction");
    correctionSpeed = parameters.getRawParameterValue("speed");
    pitchShift = parameters.getRawParameterValue("pitch");
    formantShift = parameters.getRawParameterValue("formant");
    breathAmount = parameters.getRawParameterValue("breath");
    resonanceAmount = parameters.getRawParameterValue("resonance");
    aiBlend = parameters.getRawParameterValue("blend");
    keyParam = parameters.getRawParameterValue("key");
    scaleParam = parameters.getRawParameterValue("scale");

    resetPitchShiftState();
}

VocalSuiteAudioProcessor::~VocalSuiteAudioProcessor() {}

juce::AudioProcessorValueTreeState::ParameterLayout VocalSuiteAudioProcessor::createParameterLayout() {
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    
    // Pitch Correction
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "correction", "Correction Amount", 0.0f, 1.0f, 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "speed", "Correction Speed", 0.0f, 1.0f, 0.2f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "pitch", "Pitch Shift", -24.0f, 24.0f, 0.0f));
    
    // Key and Scale
    params.push_back(std::make_unique<juce::AudioParameterInt>(
        "key", "Key", 0, 11, 0)); // 0=C, 1=C#, ..., 11=B
    params.push_back(std::make_unique<juce::AudioParameterInt>(
        "scale", "Scale", 0, 8, 0)); // 0=Major, 1=Minor, etc.
    
    // Voice Character
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "formant", "Formant Shift", -12.0f, 12.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "breath", "Breath Amount", 0.0f, 1.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "resonance", "Resonance", 0.0f, 1.0f, 0.5f));
    
    // AI Voice Model
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "blend", "AI Blend", 0.0f, 1.0f, 0.0f));
    
    return { params.begin(), params.end() };
}

void VocalSuiteAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    currentSampleRate = sampleRate;
    maxBlockSize = samplesPerBlock;
    
    // Prepare all modules
    pitchDetector.setSampleRate(sampleRate);
    pitchDetector.setBufferSize(pitchShiftFrameSize);
    
    voiceCharacter.prepare(sampleRate, samplesPerBlock);

    resetPitchShiftState();

    pitchInRing.assign(pitchShiftFrameSize, 0.0f);
    pitchOlaRing.assign(pitchShiftFrameSize, 0.0f);
    pitchOlaGainRing.assign(pitchShiftFrameSize, 0.0f);
    pitchFrame.assign(pitchShiftFrameSize, 0.0f);
    pitchFrameOut.assign(pitchShiftFrameSize, 0.0f);

    pitchOlaWindow.resize(pitchShiftFrameSize);
    for (int n = 0; n < pitchShiftFrameSize; n++) {
        pitchOlaWindow[n] = 0.5f * (1.0f - std::cos(2.0f * (float)M_PI * (float)n / (float)(pitchShiftFrameSize - 1)));
    }

    pitchDetectFrame.assign(pitchShiftFrameSize, 0.0f);

    // Allocate working buffers
    workingBuffer.resize(samplesPerBlock * 4); // Extra space for overlap-add
    aiOutputBuffer.resize(samplesPerBlock);

    setLatencySamples(pitchShiftFrameSize - pitchShiftHopSize);
}

void VocalSuiteAudioProcessor::releaseResources() {
    workingBuffer.clear();
    aiOutputBuffer.clear();

    pitchInRing.clear();
    pitchOlaRing.clear();
    pitchOlaGainRing.clear();
    pitchFrame.clear();
    pitchFrameOut.clear();
    pitchOlaWindow.clear();
    pitchDetectFrame.clear();
}

void VocalSuiteAudioProcessor::resetPitchShiftState() {
    pitchRingPos = 0;
    pitchSamplesFilled = 0;
    pitchSamplesSinceProcess = 0;
}

void VocalSuiteAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) {
    juce::ScopedNoDenormals noDenormals;
    
    auto* channelData = buffer.getWritePointer(0);
    int numSamples = buffer.getNumSamples();

    const int aiProcessSamples = std::min(numSamples, (int) aiOutputBuffer.size());
    
    float correction = (correctionAmount != nullptr) ? correctionAmount->load() : 0.5f;
    float speed = (correctionSpeed != nullptr) ? correctionSpeed->load() : 0.2f;
    float pitchSemitones = (pitchShift != nullptr) ? pitchShift->load() : 0.0f;
    float formantSemitones = (formantShift != nullptr) ? formantShift->load() : 0.0f;
    float breath = (breathAmount != nullptr) ? breathAmount->load() : 0.0f;
    float resonance = (resonanceAmount != nullptr) ? resonanceAmount->load() : 0.5f;
    float blend = (aiBlend != nullptr) ? aiBlend->load() : 0.0f;
    int key = (keyParam != nullptr) ? (int)keyParam->load() : 0;
    int scale = (scaleParam != nullptr) ? (int)scaleParam->load() : 0;
    
    // ===== PROCESSING CHAIN =====
    
    float detectedPitch = 0.0f;
    if (numSamples >= pitchShiftFrameSize) {
        detectedPitch = pitchDetector.getPitch(channelData, pitchShiftFrameSize);
    } else if (pitchSamplesFilled >= pitchShiftFrameSize && !pitchDetectFrame.empty()) {
        const int start = pitchRingPos;
        const int len1 = pitchShiftFrameSize - start;
        if (len1 > 0)
            std::copy(pitchInRing.begin() + start, pitchInRing.end(), pitchDetectFrame.begin());
        if (start > 0)
            std::copy(pitchInRing.begin(), pitchInRing.begin() + start, pitchDetectFrame.begin() + len1);
        detectedPitch = pitchDetector.getPitch(pitchDetectFrame.data(), pitchShiftFrameSize);
    }
    currentPitch = detectedPitch; // Store for UI visualization
    
    // 2. PITCH CORRECTION
    float totalPitchRatio = 1.0f;
    float formantRatio = 1.0f;
    bool pitchShiftEnabled = false;

    if (correction > 0.01f && detectedPitch > 0.0f) {
        pitchCorrector.setKey(key);
        pitchCorrector.setScale(static_cast<blink::PitchCorrector::ScaleType>(scale));

        float correctedPitch = pitchCorrector.correctPitch(detectedPitch, correction, speed);
        targetPitch = correctedPitch;

        float correctionRatio = correctedPitch / detectedPitch;
        totalPitchRatio = correctionRatio * std::pow(2.0f, pitchSemitones / 12.0f);
        formantRatio = std::pow(2.0f, formantSemitones / 12.0f);
        pitchShiftEnabled = true;
    } else if (std::abs(pitchSemitones) > 0.1f || std::abs(formantSemitones) > 0.1f) {
        totalPitchRatio = std::pow(2.0f, pitchSemitones / 12.0f);
        formantRatio = std::pow(2.0f, formantSemitones / 12.0f);
        targetPitch = detectedPitch;
        pitchShiftEnabled = true;
    } else {
        targetPitch = detectedPitch;
    }

    totalPitchRatio = juce::jlimit(0.25f, 4.0f, totalPitchRatio);
    formantRatio = juce::jlimit(0.25f, 4.0f, formantRatio);

    if (pitchShiftEnabled && !pitchInRing.empty() && !pitchOlaRing.empty()) {
        for (int i = 0; i < numSamples; i++) {
            const float inSample = channelData[i];

            pitchInRing[pitchRingPos] = inSample;
            const float outAccum = pitchOlaRing[pitchRingPos];
            const float gainAccum = pitchOlaGainRing.empty() ? 0.0f : pitchOlaGainRing[pitchRingPos];
            pitchOlaRing[pitchRingPos] = 0.0f;
            if (!pitchOlaGainRing.empty())
                pitchOlaGainRing[pitchRingPos] = 0.0f;

            const float outSample = (gainAccum > 1.0e-6f) ? (outAccum / gainAccum) : outAccum;
            channelData[i] = (pitchSamplesFilled < pitchShiftFrameSize) ? inSample : outSample;

            pitchRingPos++;
            if (pitchRingPos >= pitchShiftFrameSize)
                pitchRingPos = 0;

            pitchSamplesFilled = std::min(pitchShiftFrameSize, pitchSamplesFilled + 1);
            pitchSamplesSinceProcess++;

            if (pitchSamplesFilled == pitchShiftFrameSize && pitchSamplesSinceProcess >= pitchShiftHopSize) {
                pitchSamplesSinceProcess = 0;

                const int start = pitchRingPos;
                const int len1 = pitchShiftFrameSize - start;
                if (len1 > 0)
                    std::copy(pitchInRing.begin() + start, pitchInRing.end(), pitchFrame.begin());
                if (start > 0)
                    std::copy(pitchInRing.begin(), pitchInRing.begin() + start, pitchFrame.begin() + len1);

                pitchShifter.process(pitchFrame.data(), pitchFrameOut.data(), totalPitchRatio, formantRatio);

                for (int n = 0; n < pitchShiftFrameSize; n++) {
                    const int idx = (start + n) % pitchShiftFrameSize;
                    const float w = pitchOlaWindow.empty() ? 1.0f : pitchOlaWindow[n];
                    pitchOlaRing[idx] += pitchFrameOut[n] * w;
                    if (!pitchOlaGainRing.empty())
                        pitchOlaGainRing[idx] += w;
                }
            }
        }
    }
    
    // 3. VOICE CHARACTER (Breath, Resonance)
    float resonanceFreq = 2500.0f; // Default resonance frequency (can be made adjustable)
    voiceCharacter.process(channelData, numSamples, breath, resonance, resonanceFreq);
    
    if (blend > 0.01f && aiProcessor.isLoaded() && aiProcessSamples == numSamples) {
        aiProcessor.process(channelData, aiOutputBuffer.data(), aiProcessSamples);
        
        for (int i = 0; i < aiProcessSamples; i++) {
            channelData[i] = channelData[i] * (1.0f - blend) + aiOutputBuffer[i] * blend;
        }
    }
    
    // 5. SOFT CLIPPING (prevent harsh clipping)
    for (int i = 0; i < numSamples; i++) {
        channelData[i] = std::tanh(channelData[i]);
    }
}

bool VocalSuiteAudioProcessor::hasEditor() const {
    return true;
}

juce::AudioProcessorEditor* VocalSuiteAudioProcessor::createEditor() {
    return new VocalSuiteAudioProcessorEditor(*this);
}

void VocalSuiteAudioProcessor::getStateInformation(juce::MemoryBlock& destData) {
    auto state = parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void VocalSuiteAudioProcessor::setStateInformation(const void* data, int sizeInBytes) {
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState.get() != nullptr) {
        if (xmlState->hasTagName(parameters.state.getType())) {
            parameters.replaceState(juce::ValueTree::fromXml(*xmlState));
        }
    }
}

void VocalSuiteAudioProcessor::loadVoiceModel(const std::string& modelId, const std::string& modelType) {
    if (modelType == "onnx") {
        if (modelId == "none") {
            return;
        }
        
        juce::File modelsDir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
            .getChildFile("VocalSuitePro")
            .getChildFile("Models");
        
        juce::File modelFile = modelsDir.getChildFile(modelId + ".onnx");
        
        if (modelFile.existsAsFile()) {
            aiProcessor.loadModel(modelFile.getFullPathName().toStdString());
        }
    }
    else if (modelType == "fish") {
    }
}

// JUCE Entry point
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new VocalSuiteAudioProcessor();
}
