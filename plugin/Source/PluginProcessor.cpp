#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <cmath>
#include <algorithm>

#include <juce_audio_formats/juce_audio_formats.h>

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

    // Capture up to 2 minutes of mono audio by default
    const int captureMaxSeconds = 120;
    const int captureMaxSamples = (int) (sampleRate * (double) captureMaxSeconds);
    captureBuffer.assign((size_t) captureMaxSamples, 0.0f);
    captureWritePos = 0;
    captureSamplesRecorded = 0;
    isCapturing.store(false);

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

    // Capture input (mono) if armed
    if (isCapturing.load() && !captureBuffer.empty())
    {
        const int capSize = (int) captureBuffer.size();
        int remaining = capSize - captureWritePos;
        int toCopy = std::min(numSamples, remaining);

        if (toCopy > 0)
        {
            std::copy(channelData, channelData + toCopy, captureBuffer.begin() + captureWritePos);
            captureWritePos += toCopy;
            captureSamplesRecorded = std::min(capSize, captureSamplesRecorded + toCopy);
        }

        if (captureWritePos >= capSize)
            isCapturing.store(false);
    }

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
    
    // AI voice conversion is now offline-only (use OfflineVoiceProcessor)
    // Real-time AI processing would require too much latency
    // For now, skip AI processing in real-time mode
    
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

void VocalSuiteAudioProcessor::startCapture()
{
    if (captureBuffer.empty())
        return;

    captureWritePos = 0;
    captureSamplesRecorded = 0;
    lastCapturedFile = juce::File();
    isCapturing.store(true);
}

void VocalSuiteAudioProcessor::stopCapture()
{
    isCapturing.store(false);

    if (captureSamplesRecorded <= 0 || captureBuffer.empty())
        return;

    juce::File rendersDir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
        .getChildFile("VocalSuitePro")
        .getChildFile("Renders");

    rendersDir.createDirectory();

    const auto timestamp = juce::Time::getCurrentTime().formatted("%Y%m%d_%H%M%S");
    lastCapturedFile = rendersDir.getChildFile("capture_" + timestamp + ".wav");

    const int samplesToWrite = captureSamplesRecorded;
    const double sr = currentSampleRate;
    const std::vector<float> bufferCopy(captureBuffer.begin(), captureBuffer.begin() + samplesToWrite);
    const juce::File outFileCopy = lastCapturedFile;

    captureWriteInProgress.store(true);
    captureWriteFinished.reset();

    std::thread([this, bufferCopy, outFileCopy, sr]() {
        juce::WavAudioFormat wav;

        std::unique_ptr<juce::FileOutputStream> outStream(outFileCopy.createOutputStream());
        if (outStream == nullptr)
        {
            captureWriteInProgress.store(false);
            captureWriteFinished.signal();
            return;
        }

        std::unique_ptr<juce::AudioFormatWriter> writer(
            wav.createWriterFor(outStream.get(), sr, 1, 16, {}, 0));

        if (writer == nullptr)
        {
            captureWriteInProgress.store(false);
            captureWriteFinished.signal();
            return;
        }

        outStream.release();

        juce::AudioBuffer<float> tmp(1, (int) bufferCopy.size());
        tmp.copyFrom(0, 0, bufferCopy.data(), (int) bufferCopy.size());
        writer->writeFromAudioSampleBuffer(tmp, 0, tmp.getNumSamples());

        captureWriteInProgress.store(false);
        captureWriteFinished.signal();
    }).detach();
}

void VocalSuiteAudioProcessor::convertCapturedAudio(const std::string& modelId, int pitchShift, float formantShift)
{
    if (isCapturing.load())
        stopCapture();

    if (captureWriteInProgress.load())
        captureWriteFinished.wait(5000);

    if (!lastCapturedFile.existsAsFile())
    {
        DBG("[SwindleVX] convertCapturedAudio: no capture file available");
        return;
    }

    juce::File modelsDir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
        .getChildFile("VocalSuitePro")
        .getChildFile("Models");

    juce::File modelFile;
    {
        const juce::File candidate(modelId);
        if (candidate.existsAsFile())
            modelFile = candidate;
        else
        {
            modelFile = modelsDir.getChildFile(modelId);
            if (!modelFile.existsAsFile())
                modelFile = modelsDir.getChildFile(modelId + ".pth");
        }
    }

    if (!modelFile.existsAsFile())
    {
        DBG("[SwindleVX] convertCapturedAudio: model not found: " + modelFile.getFullPathName());
        return;
    }

    juce::File rendersDir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
        .getChildFile("VocalSuitePro")
        .getChildFile("Renders");
    rendersDir.createDirectory();

    const auto timestamp = juce::Time::getCurrentTime().formatted("%Y%m%d_%H%M%S");
    const auto safeModel = juce::File::createLegalFileName(modelFile.getFileNameWithoutExtension());
    const juce::File outFile = rendersDir.getChildFile("converted_" + safeModel + "_" + timestamp + ".wav");

    const juce::File inputFile = lastCapturedFile;
    const juce::File modelFileCopy = modelFile;
    const int pitchShiftCopy = pitchShift;
    const float formantShiftCopy = formantShift;

    // Use the lightweight WORLD-based converter by default.
    const juce::File scriptFile = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
        .getChildFile("VocalSuitePro")
        .getChildFile("RVC")
        .getChildFile("voice_convert.py");

    if (!scriptFile.existsAsFile())
    {
        DBG("[SwindleVX] convertCapturedAudio: backend script not found: " + scriptFile.getFullPathName());
        return;
    }

    std::thread([inputFile, outFile, modelFileCopy, scriptFile, pitchShiftCopy, formantShiftCopy]() {
        DBG("[SwindleVX] Converting captured audio via Python backend...");

        const juce::String args = " run -n rvc310 python "
            + scriptFile.getFullPathName().quoted()
            + " " + inputFile.getFullPathName().quoted()
            + " " + outFile.getFullPathName().quoted()
            + " --model " + modelFileCopy.getFullPathName().quoted()
            + " --pitch " + juce::String(pitchShiftCopy)
            + " --formant " + juce::String(formantShiftCopy);

        const juce::File home = juce::File::getSpecialLocation(juce::File::userHomeDirectory);
        const std::vector<juce::String> condaCandidates = {
            "conda",
            (home.getChildFile("miniconda3/bin/conda").getFullPathName()),
            (home.getChildFile("mambaforge/bin/conda").getFullPathName()),
            "/opt/homebrew/Caskroom/miniconda/base/bin/conda",
            "/usr/local/Caskroom/miniconda/base/bin/conda",
            "/opt/homebrew/bin/conda",
            "/usr/local/bin/conda"
        };

        juce::ChildProcess proc;
        bool started = false;
        for (const auto& condaPath : condaCandidates)
        {
            const juce::String cmd = condaPath.quoted() + args;
            if (proc.start(cmd))
            {
                started = true;
                break;
            }
        }

        if (!started)
        {
            DBG("[SwindleVX] Failed to start backend process (conda not found on PATH?)");
            return;
        }

        proc.waitForProcessToFinish(-1);
        const auto outText = proc.readAllProcessOutput();
        if (outText.isNotEmpty())
            DBG("[SwindleVX] Backend output: " + outText);

        if (outFile.existsAsFile())
            DBG("[SwindleVX] Converted file saved: " + outFile.getFullPathName());
        else
            DBG("[SwindleVX] Backend finished but output file missing: " + outFile.getFullPathName());
    }).detach();
}

// JUCE Entry point
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new VocalSuiteAudioProcessor();
}
