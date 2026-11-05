#include "PluginProcessor.h"
#include "PluginEditor.h"

using APVTS = juce::AudioProcessorValueTreeState;

AutoGainProcessor::AutoGainProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "AutoGain", createLayout())
{
}

APVTS::ParameterLayout AutoGainProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    auto db = [] (int decimals) { return AudioParameterFloatAttributes().withLabel ("dB")
                                         .withStringFromValueFunction ([decimals] (float v, int) { return String (v, decimals); }); };

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "target", 1 },   "Target Level",
                                                        NormalisableRange<float> (-48.0f, 0.0f, 0.5f), -18.0f, db (1)));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "detection", 1 }, "Deteccao",
                                                         StringArray { "Pico", "RMS", "Pico + RMS" }, 2));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "response", 1 },  "Resposta",
                                                         StringArray { "Lenta (ao vivo)", "Media", "Rapida" }, 0));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "maxBoost", 1 }, "Aumento maximo",
                                                        NormalisableRange<float> (0.0f, 24.0f, 1.0f), 12.0f, db (0)));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "maxCut", 1 },   "Corte maximo",
                                                        NormalisableRange<float> (0.0f, 36.0f, 1.0f), 24.0f, db (0)));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "gate", 1 },     "Gate (silencio)",
                                                        NormalisableRange<float> (-80.0f, -20.0f, 1.0f), -50.0f, db (0)));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "ceiling", 1 },  "Protecao de pico",
                                                        NormalisableRange<float> (-12.0f, 0.0f, 0.1f), -1.0f, db (1)));
    p.push_back (std::make_unique<AudioParameterBool>  (ParameterID { "active", 1 },   "Ativo", true));
    p.push_back (std::make_unique<AudioParameterBool>  (ParameterID { "hold", 1 },     "Hold", false));

    return { p.begin(), p.end() };
}

bool AutoGainProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return out == layouts.getMainInputChannelSet();
}

void AutoGainProcessor::prepareToPlay (double sampleRate, int)
{
    engine.prepare (sampleRate);
}

AutoGainParams AutoGainProcessor::readParams() const
{
    auto v = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    AutoGainParams p;
    p.target    = v ("target");
    p.detection = (int) v ("detection");
    p.response  = (int) v ("response");
    p.maxBoost  = v ("maxBoost");
    p.maxCut    = v ("maxCut");
    p.gate      = v ("gate");
    p.ceiling   = v ("ceiling");
    p.active    = v ("active") > 0.5f;
    p.hold      = v ("hold") > 0.5f;
    return p;
}

void AutoGainProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numIn = getTotalNumInputChannels();
    const int n     = buffer.getNumSamples();
    for (int ch = numIn; ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, n);

    engine.process (buffer.getArrayOfWritePointers(), numIn, n, readParams());
}

void AutoGainProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("uiTab", uiTab.load(), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void AutoGainProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto state = juce::ValueTree::fromXml (*xml);
            uiTab.store ((int) state.getProperty ("uiTab", 0));
            apvts.replaceState (state);
        }
}

juce::AudioProcessorEditor* AutoGainProcessor::createEditor()
{
    return new AutoGainEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AutoGainProcessor();
}
