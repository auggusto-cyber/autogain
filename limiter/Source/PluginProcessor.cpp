#include "PluginProcessor.h"
#include "PluginEditor.h"

using APVTS = juce::AudioProcessorValueTreeState;

AGLProcessor::AGLProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "AutoGainLimiter", createLayout())
{
    startTimerHz (10);
}

AGLProcessor::~AGLProcessor()
{
    stopTimer();
}

APVTS::ParameterLayout AGLProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    auto num = [] (const char* unit, int decimals)
    {
        return AudioParameterFloatAttributes().withLabel (unit)
                   .withStringFromValueFunction ([decimals] (float v, int) { return String (v, decimals); });
    };

    // geral
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "active", 1 }, "Ligado", true));

    // 1. Auto Gain
    p.push_back (std::make_unique<AudioParameterBool>  (ParameterID { "agOn", 1 },     "Auto Gain ligado", true));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "target", 1 },   "Target Level",
                                                        NormalisableRange<float> (-48.0f, 0.0f, 0.5f), -18.0f, num ("dB", 1)));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "detection", 1 }, "Deteccao",
                                                         StringArray { "Pico", "RMS", "Pico + RMS" }, 2));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "response", 1 },  "Resposta",
                                                         StringArray { "Lenta (ao vivo)", "Media", "Rapida" }, 0));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "maxBoost", 1 }, "Aumento maximo",
                                                        NormalisableRange<float> (0.0f, 24.0f, 1.0f), 12.0f, num ("dB", 0)));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "maxCut", 1 },   "Corte maximo",
                                                        NormalisableRange<float> (0.0f, 36.0f, 1.0f), 24.0f, num ("dB", 0)));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "gate", 1 },     "Gate (silencio)",
                                                        NormalisableRange<float> (-80.0f, -20.0f, 1.0f), -50.0f, num ("dB", 0)));
    p.push_back (std::make_unique<AudioParameterBool>  (ParameterID { "hold", 1 },     "Hold", false));

    // 2. Limiter
    p.push_back (std::make_unique<AudioParameterBool>  (ParameterID { "limOn", 1 },     "Limiter ligado", true));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "threshold", 1 }, "Threshold",
                                                        NormalisableRange<float> (-30.0f, 0.0f, 0.1f), -1.0f, num ("dB", 1)));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "ceiling", 1 },   "Ceiling",
                                                        NormalisableRange<float> (-12.0f, 0.0f, 0.1f), -1.0f, num ("dB", 1)));
    NormalisableRange<float> rel (1.0f, 1000.0f, 1.0f);
    rel.setSkewForCentre (100.0f);
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "release", 1 },   "Release", rel, 100.0f, num ("ms", 0)));
    p.push_back (std::make_unique<AudioParameterBool>  (ParameterID { "autoRelease", 1 }, "Auto Release", true));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "lookahead", 1 }, "Lookahead",
                                                         StringArray { "0 ms (ao vivo)", "1 ms", "3 ms", "5 ms" }, 1));
    p.push_back (std::make_unique<AudioParameterBool>  (ParameterID { "truePeak", 1 },  "True Peak", false));
    p.push_back (std::make_unique<AudioParameterBool>  (ParameterID { "link", 1 },      "Link", false));

    return { p.begin(), p.end() };
}

bool AGLProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return out == layouts.getMainInputChannelSet();
}

AutoGainParams AGLProcessor::agParams() const
{
    AutoGainParams p;
    p.target    = param ("target");
    p.detection = (int) param ("detection");
    p.response  = (int) param ("response");
    p.maxBoost  = param ("maxBoost");
    p.maxCut    = param ("maxCut");
    p.gate      = param ("gate");
    p.ceiling   = param ("ceiling");          // Pico + RMS usa o teto do Limiter
    p.active    = param ("active") > 0.5f && param ("agOn") > 0.5f;
    p.hold      = param ("hold") > 0.5f;
    p.peakProtection = false;                 // quem cuida do teto e o Limiter
    return p;
}

LimiterParams AGLProcessor::limParams() const
{
    LimiterParams p;
    p.threshold   = param ("threshold");
    p.ceiling     = param ("ceiling");
    p.releaseMs   = param ("release");
    p.autoRelease = param ("autoRelease") > 0.5f;
    p.lookahead   = (int) param ("lookahead");
    p.truePeak    = param ("truePeak") > 0.5f;
    p.active      = param ("active") > 0.5f && param ("limOn") > 0.5f;
    return p;
}

int AGLProcessor::wantedLatency() const
{
    const double sr = getSampleRate() > 0.0 ? getSampleRate() : 48000.0;
    return LimiterEngine::latencyFor (sr, (int) param ("lookahead"), param ("truePeak") > 0.5f);
}

void AGLProcessor::prepareToPlay (double sampleRate, int)
{
    ag.prepare (sampleRate);
    lim.prepare (sampleRate);
    analyzer.prepare (sampleRate);
    setLatencySamples (wantedLatency());
}

void AGLProcessor::timerCallback()
{
    const int wanted = wantedLatency();
    if (wanted != getLatencySamples())
        setLatencySamples (wanted);
}

void AGLProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numIn = getTotalNumInputChannels();
    const int n     = buffer.getNumSamples();
    for (int ch = numIn; ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, n);
    if (numIn <= 0 || n <= 0)
        return;

    if (simulate.load())
        fillSimulation (buffer, numIn, n);

    float* const* data = buffer.getArrayOfWritePointers();
    analyzer.process (buffer.getArrayOfReadPointers(), numIn, n);   // leitura na entrada
    ag.process (data, numIn, n, agParams());                        // 1. Auto Gain
    lim.process (data, numIn, n, limParams());                      // 2. Limiter
}

void AGLProcessor::fillSimulation (juce::AudioBuffer<float>& buffer, int numCh, int n)
{
    // voz simulada a ~-26 dBFS RMS: 2 s falando, 1 s de pausa com chiado (-58 dBFS), golpes fortes de vez em quando
    const double sr = getSampleRate() > 0.0 ? getSampleRate() : 48000.0;
    const double twoPi = juce::MathConstants<double>::twoPi;
    for (int i = 0; i < n; ++i)
    {
        const bool pause = std::fmod (simTime, 3.0) >= 2.0;
        float s;
        if (pause)
            s = 0.0022f * (2.0f * simRandom.nextFloat() - 1.0f);
        else
        {
            const double syll = 0.55 + 0.45 * std::sin (twoPi * 4.3 * simTime);
            s = (float) (0.07 * syll * (std::sin (simPhase) + 0.35 * std::sin (2.0 * simPhase) + 0.15 * std::sin (3.0 * simPhase)));
            if (std::fmod (simTime, 5.3) < 0.025)
                s += 0.45f * (float) std::sin (twoPi * 1200.0 * simTime);
        }
        simPhase += twoPi * (180.0 + 25.0 * std::sin (twoPi * 0.7 * simTime)) / sr;
        simTime += 1.0 / sr;
        for (int c = 0; c < numCh; ++c)
            buffer.setSample (c, i, s);
    }
}

void AGLProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("uiTab", uiTab.load(), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void AGLProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto state = juce::ValueTree::fromXml (*xml);
            uiTab.store ((int) state.getProperty ("uiTab", 0));
            apvts.replaceState (state);
        }
}

juce::AudioProcessorEditor* AGLProcessor::createEditor()
{
    return new AGLEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AGLProcessor();
}
