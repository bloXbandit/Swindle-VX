cmake_minimum_required(VERSION 3.15)

project(VocalSuitePro VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find JUCE
# Option 1: JUCE as subdirectory (recommended)
add_subdirectory(JUCE)

# Option 2: JUCE installed system-wide
# find_package(JUCE CONFIG REQUIRED)

juce_add_plugin(VocalSuitePro
    COMPANY_NAME "BlinkLabs"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER_CODE Bink
    PLUGIN_CODE VcSp
    FORMATS VST3 AU
    PRODUCT_NAME "Vocal Suite Pro")

target_sources(VocalSuitePro
    PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    
    # DSP Modules
    Source/DSP/PitchDetector.cpp
    Source/DSP/PitchDetector.h
    Source/DSP/PitchShifter.cpp
    Source/DSP/TransientDetector.cpp
    Source/DSP/TransientDetector.h
    Source/DSP/LPCAnalyzer.cpp
    Source/DSP/LPCAnalyzer.h
    Source/DSP/F0Extractor.cpp
    Source/DSP/F0Extractor.h
    Source/DSP/MelSpectrogram.cpp
    Source/DSP/MelSpectrogram.h
    Source/DSP/OfflineVoiceProcessor.cpp
    Source/DSP/OfflineVoiceProcessor.h
    Source/DSP/PitchShifter.h
    Source/DSP/PitchCorrector.cpp
    Source/DSP/PitchCorrector.h
    Source/DSP/VoiceCharacter.cpp
    Source/DSP/VoiceCharacter.h
    
    # AI Module
    Source/AI/ONNXInference.cpp
    Source/AI/ONNXInference.h
)

target_compile_definitions(VocalSuitePro
    PUBLIC
    JUCE_WEB_BROWSER=1
    JUCE_USE_CURL=1
    JUCE_VST3_CAN_REPLACE_VST2=0
    
    # Uncomment when ONNX Runtime is available
    # ONNX_RUNTIME_AVAILABLE=1
)

target_link_libraries(VocalSuitePro
    PRIVATE
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_gui_extra
    juce::juce_dsp
    
    # Uncomment and set path when ONNX Runtime is available
    # /path/to/onnxruntime/lib/libonnxruntime.dylib
)

set(WEBUI_DIST_DIR "${CMAKE_CURRENT_LIST_DIR}/../dist")

if (EXISTS "${WEBUI_DIST_DIR}/index.html")
    file(GLOB_RECURSE WEBUI_DIST_SOURCES
        CONFIGURE_DEPENDS
        "${WEBUI_DIST_DIR}/*")

    juce_add_binary_data(VocalSuiteProBinaryData
        SOURCES ${WEBUI_DIST_SOURCES})

    target_link_libraries(VocalSuitePro PRIVATE VocalSuiteProBinaryData)
    target_compile_definitions(VocalSuitePro PRIVATE VOCALSUITE_EMBED_UI=1)
else()
    message(WARNING "VocalSuitePro: Web UI not embedded because dist/ was not found. Run the Vite build to generate ${WEBUI_DIST_DIR}.")
    target_compile_definitions(VocalSuitePro PRIVATE VOCALSUITE_EMBED_UI=0)
endif()

# Uncomment when ONNX Runtime is available
# target_include_directories(VocalSuitePro
#     PRIVATE
#     /path/to/onnxruntime/include
# )

juce_generate_juce_header(VocalSuitePro)
