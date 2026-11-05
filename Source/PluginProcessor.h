#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "AutoGainEngine.h"

// Auto Gain by Augusto Souza
// Ganho automatico para uso ao vivo (REAPER Input FX), latencia zero.
class AutoGainProcessor : public juce::AudioProcessor
{
public:
    AutoGainProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Auto Gain"; }
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

    AutoGainEngine engine;               // medidores, leitura do dia, ganho
    std::atomic<int> uiTab { 0 };        // aba da interface (nao altera o processamento)

private:
    AutoGainParams readParams() const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AutoGainProcessor)
};
