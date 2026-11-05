#pragma once

#include "PluginProcessor.h"

// Visual Black Neon
namespace Neon
{
    const juce::Colour black   { 0xff000000 };
    const juce::Colour panel   { 0xff0e1013 };
    const juce::Colour well    { 0xff050607 };
    const juce::Colour border  { 0xff262b33 };
    const juce::Colour control { 0xff16191e };
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

    // fonte Inter embutida: o plugin fica igual em qualquer Windows
    juce::Typeface::Ptr getTypefaceForFont (const juce::Font&) override;

private:
    juce::Typeface::Ptr interRegular, interBold, interBoldItalic;
};

class AutoGainEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit AutoGainEditor (AutoGainProcessor&);
    ~AutoGainEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    // usados pela pre-visualizacao / testes
    void showTab (int tab)    { setTab (tab); }
    void refreshForSnapshot() { timerCallback(); }

private:
    void timerCallback() override;
    void setTab (int tab);
    void nudgeTarget (float deltaDb);
    void setParam (const juce::String& id, float value);
    float param (const juce::String& id) const;

    void paintHeader (juce::Graphics&);
    void paintMinimal (juce::Graphics&);
    void paintAdvanced (juce::Graphics&);
    void paintAnalysis (juce::Graphics&);
    void paintStatus (juce::Graphics&, juce::Rectangle<int> area);
    void paintMeter (juce::Graphics&, juce::Rectangle<int> bar, float rmsDb, float peakDb, float markerDb, juce::Colour markerColour);

    struct Suggestion { float gainDb = 0, gateDb = -50, overDb = 0; };
    Suggestion makeSuggestion() const;

    AutoGainProcessor& proc;
    // um unico visual compartilhado por todas as janelas abertas do plugin
    juce::SharedResourcePointer<NeonLookAndFeel> lnf;
    static inline std::atomic<int> openEditors { 0 };   // janelas abertas (todas na thread da interface)

    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAtt  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;

    juce::TextButton tabButtons[3];
    juce::TextButton powerButton { "ON" }, holdButton { "HOLD" };

    // Minimalista
    juce::TextButton minusButton { "-" }, plusButton { "+" };

    // Avancado
    juce::Slider targetSlider, boostSlider, cutSlider, gateSlider, ceilingSlider;
    juce::ComboBox detectionBox, responseBox;

    // Analise
    juce::ComboBox durationBox;
    juce::TextButton startButton { "Iniciar leitura" }, applyButton { "Aplicar ajustes" }, discardButton { "Descartar" };

    std::unique_ptr<ButtonAtt> powerAtt, holdAtt;
    std::unique_ptr<SliderAtt> targetAtt, boostAtt, cutAtt, gateAtt, ceilingAtt;
    std::unique_ptr<ComboAtt>  detectionAtt, responseAtt;

    int currentTab = 0;
    AutoGainEngine::AnalysisResult result;
    bool applied = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AutoGainEditor)
};
