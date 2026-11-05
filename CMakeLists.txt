cmake_minimum_required(VERSION 3.22)
project(AutoGain VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(AUTOGAIN_BUILD_TESTS "Compila o teste automatico do motor de audio" ON)

# Baixa o JUCE automaticamente na primeira configuracao
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(AutoGain
    VERSION                   1.0.0
    COMPANY_NAME              "Augusto Souza"
    PLUGIN_MANUFACTURER_CODE  Augs
    PLUGIN_CODE               AtGn
    FORMATS                   VST3
    PRODUCT_NAME              "Auto Gain"
    VST3_CATEGORIES           Fx Dynamics
    IS_SYNTH                  FALSE
    NEEDS_MIDI_INPUT          FALSE
    NEEDS_MIDI_OUTPUT         FALSE
    COPY_PLUGIN_AFTER_BUILD   FALSE)

# Fonte Inter embutida (licenca OFL, ver Fonts/LICENSE-Inter.txt)
juce_add_binary_data(AutoGainFonts SOURCES
    Fonts/Inter-Regular.otf
    Fonts/Inter-Bold.otf
    Fonts/Inter-BoldItalic.otf)

target_sources(AutoGain PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(AutoGain PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(AutoGain
    PRIVATE
        AutoGainFonts
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

if(AUTOGAIN_BUILD_TESTS)
    add_executable(EngineTest Tests/EngineTest.cpp)
    target_include_directories(EngineTest PRIVATE Source)
endif()
