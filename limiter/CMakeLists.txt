cmake_minimum_required(VERSION 3.22)
project(AutoGainLimiter VERSION 1.0.0)

# Auto Gain + Limiter by Augusto Souza
# Pasta autogain/limiter: usa o motor do Auto Gain (../Source) e a fonte Inter (../Fonts)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(AGL_BUILD_TESTS "Compila o teste dos motores" ON)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(JUCE)

set(AG_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)

juce_add_plugin(AutoGainLimiter
    VERSION                   1.0.0
    COMPANY_NAME              "Augusto Souza"
    PLUGIN_MANUFACTURER_CODE  Augs
    PLUGIN_CODE               AgLm
    FORMATS                   VST3
    PRODUCT_NAME              "Auto Gain + Limiter"
    VST3_CATEGORIES           Fx Dynamics
    IS_SYNTH                  FALSE
    NEEDS_MIDI_INPUT          FALSE
    NEEDS_MIDI_OUTPUT         FALSE
    COPY_PLUGIN_AFTER_BUILD   FALSE)

juce_add_binary_data(AGLFonts SOURCES
    ${AG_ROOT}/Fonts/Inter-Regular.otf
    ${AG_ROOT}/Fonts/Inter-Bold.otf
    ${AG_ROOT}/Fonts/Inter-BoldItalic.otf)

target_sources(AutoGainLimiter PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_include_directories(AutoGainLimiter PRIVATE Source ${AG_ROOT}/Source)

target_compile_definitions(AutoGainLimiter PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

# textos com acento no codigo-fonte (UTF-8)
if(MSVC)
    target_compile_options(AutoGainLimiter PRIVATE /utf-8)
endif()

target_link_libraries(AutoGainLimiter
    PRIVATE
        AGLFonts
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

if(AGL_BUILD_TESTS)
    add_executable(LimiterTest Tests/LimiterTest.cpp)
    target_include_directories(LimiterTest PRIVATE Source ${AG_ROOT}/Source)
    add_executable(EngineTest ${AG_ROOT}/Tests/EngineTest.cpp)
    target_include_directories(EngineTest PRIVATE ${AG_ROOT}/Source)
    if(MSVC)
        target_compile_options(LimiterTest PRIVATE /utf-8)
    endif()
endif()
