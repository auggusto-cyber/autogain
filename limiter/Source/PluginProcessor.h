#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "AutoGainEngine.h"
#include "LimiterEngine.h"
#include "Analyzer.h"

// Auto Gain + Limiter by Augusto Souza
// Cadeia: Entrada -> Auto Gain (0 ms) -> Limiter (0 a 5 ms) -> Saida
class AGLProcessor : public juce::AudioProcessor, private juce::Timer
{
public:
    AGLProcessor();
    ~AGLProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Auto Gain + Limiter"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    juce::AudioProcessorValueTreeState apvts;

    AutoGainEngine ag;
    LimiterEngine  lim;
    Analyzer       analyzer;
    std::atomic<int> uiTab { 0 };

    float param (const char* id) const { return apvts.getRawParameterValue (id)->load(); }
    AutoGainParams agParams() const;
    LimiterParams  limParams() const;

    // latencia que o plugin informa ao REAPER (amostras)
    int wantedLatency() const;

private:
    void timerCallback() override;   // atualiza a latencia quando o lookahead muda

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AGLProcessor)
};
