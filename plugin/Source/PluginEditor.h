#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

/**
 * The Editor uses a juce::WebBrowserComponent to host the React UI.
 * This bridges the C++ Audio Processor parameters to the Web Frontend.
 */
class VocalSuiteAudioProcessorEditor : public juce::AudioProcessorEditor,
                                        public juce::WebBrowserComponent::ResourceProvider
{
public:
    VocalSuiteAudioProcessorEditor(VocalSuiteAudioProcessor&);
    ~VocalSuiteAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    void handleMessage(const juce::var& message);

    // ResourceProvider interface (serves the React build files)
    std::optional<juce::WebBrowserComponent::Resource> getResource(const juce::String& url);

private:
    VocalSuiteAudioProcessor& audioProcessor;
    juce::WebBrowserComponent webView;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VocalSuiteAudioProcessorEditor)
};
