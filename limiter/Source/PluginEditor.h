#pragma once

#include "PluginProcessor.h"
#include <array>

namespace Neon
{
    const juce::Colour black   { 0xff000000 };
    const juce::Colour panel   { 0xff0e1013 };
    const juce::Colour well    { 0xff050607 };
    const juce::Colour border  { 0xff262b33 };
    const juce::Colour control { 0xff16191e };
    const juce::Colour edge    { 0xff333a44 };
    const juce::Colour track   { 0xff14171c };
    const juce::Colour white   { 0xfff2f4f6 };
    const juce::Colour grey    { 0xffa8b0bc };
    const juce::Colour light   { 0xffc9d0d9 };
    const juce::Colour cyan    { 0xff00f0ff };
    const juce::Colour lime    { 0xff39ff14 };
    const juce::Colour yellow  { 0xffffe600 };
    const juce::Colour magenta { 0xffff2bd6 };
    const juce::Colour red     { 0xffff2b4e };
}

class NeonLookAndFeel : public juce::LookAndFeel_V4
{
public:
    NeonLookAndFeel();
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float minPos, float maxPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    juce::Typeface::Ptr getTypefaceForFont (const juce::Font&) override;

private:
    juce::Typeface::Ptr interRegular, interBold, interBoldItalic;
};

class AGLEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit AGLEditor (AGLProcessor&);
    ~AGLEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    // usados pela pre-visualizacao / testes
    void showTab (int tab)    { setTab (tab); }
    void refreshForSnapshot() { timerCallback(); }

private:
    void timerCallback() override;
    void setTab (int tab);
    void setParam (const juce::String& id, float value);
    void nudge (const juce::String& id, float delta, float lo, float hi);

    void paintHeader (juce::Graphics&);
    void paintChain (juce::Graphics&);
    void paintMinimal (juce::Graphics&);
    void paintAdvanced (juce::Graphics&);
    void paintAnalysis (juce::Graphics&);
    void paintLevelMeter (juce::Graphics&, juce::Rectangle<int>, float levelDb, float peakDb, float markDb, juce::Colour mark);
    void paintGrMeter (juce::Graphics&, juce::Rectangle<int>, float grDb);

    struct Suggestion
    {
        float agGain = 0, gate = -50, threshold = -1, predictedGr = 0, peakAfterAg = -100, lufsOut = -100;
        bool  truePeak = false, raiseLookahead = false;
    };
    Suggestion makeSuggestion() const;

    AGLProcessor& proc;
    juce::SharedResourcePointer<NeonLookAndFeel> lnf;
    static inline std::atomic<int> openEditors { 0 };

    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAtt  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;

    // geral
    juce::TextButton tabButtons[3];
    juce::TextButton powerButton { "ON" }, holdButton { "HOLD" }, agButton, limButton;

    // Minimalista
    juce::TextButton targetDown { "-" }, targetUp { "+" }, threshDown { "-" }, threshUp { "+" }, ceilDown { "-" }, ceilUp { "+" };

    // Avancado - Auto Gain
    juce::Slider targetSlider, gateSlider, boostSlider, cutSlider;
    juce::ComboBox detectionBox, responseBox;
    // Avancado - Limiter
    juce::Slider thresholdSlider, ceilingSlider, releaseSlider;
    juce::TextButton linkButton { "LINK" }, autoRelButton { "AUTO" }, truePeakButton { "TRUE PEAK" };
    juce::ComboBox lookaheadBox;

    // Analise
    juce::ComboBox destinationBox, durationBox;
    juce::TextButton startButton { "Iniciar leitura" }, applyButton { "Aplicar nos dois" }, discardButton { "Descartar" };

    std::unique_ptr<ButtonAtt> powerAtt, holdAtt, agAtt, limAtt, linkAtt, autoRelAtt, truePeakAtt;
    std::unique_ptr<SliderAtt> targetAtt, gateAtt, boostAtt, cutAtt, thresholdAtt, ceilingAtt, releaseAtt;
    std::unique_ptr<ComboAtt>  detectionAtt, responseAtt, lookaheadAtt;

    int currentTab = 0;
    float lastThreshold = 0.0f;
    std::array<float, 150> grHistory {};   // 5 s a 30 Hz
    int grHead = 0;
    Analyzer::Result result;
    bool applied = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AGLEditor)
};
