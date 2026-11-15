#pragma once

#include "PluginProcessor.h"
#include <array>

// Paleta do layout do Augusto (v1.1)
namespace Pal
{
    const juce::Colour bg      { 0xff0b0d10 };   // fundo da janela
    const juce::Colour panel   { 0xff13161b };   // paineis
    const juce::Colour inner   { 0xff0d1014 };   // caixas internas
    const juce::Colour field   { 0xff090b0e };   // campos / combos
    const juce::Colour line    { 0xff242830 };   // bordas
    const juce::Colour line2   { 0xff2f343d };   // bordas de botoes
    const juce::Colour tabOn   { 0xff252a32 };
    const juce::Colour track   { 0xff262b33 };
    const juce::Colour white   { 0xfff3f5f8 };
    const juce::Colour text    { 0xffd5d9e0 };
    const juce::Colour grey    { 0xff8d95a3 };
    const juce::Colour dim     { 0xff5f6774 };
    const juce::Colour cyan    { 0xff22c7ee };
    const juce::Colour pink    { 0xffff3d8b };
    const juce::Colour amber   { 0xfff5a524 };
    const juce::Colour green   { 0xff34d399 };
    const juce::Colour greenBt { 0xff10b981 };
    const juce::Colour red     { 0xffef4444 };
}

class AGLLookAndFeel : public juce::LookAndFeel_V4
{
public:
    AGLLookAndFeel();

    juce::Typeface::Ptr getTypefaceForFont (const juce::Font&) override;

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool over, bool down) override;

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float minPos, float maxPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;

    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Font getPopupMenuFont() override;

    static juce::Font sans (float h, bool isBold = false);
    static juce::Font heavy (float h);
    static juce::Font mono (float h, bool isBold = false);

private:
    juce::Typeface::Ptr interRegular, interSemi, interBold, monoRegular, monoBold;
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

    void paintHeader (juce::Graphics&);
    void paintMinimal (juce::Graphics&);
    void paintAdvanced (juce::Graphics&);
    void paintAnalysis (juce::Graphics&);
    void paintOutputBar (juce::Graphics&);
    void paintVMeter (juce::Graphics&, juce::Rectangle<int>, float levelDb, float peakDb, float markDb, juce::Colour mark);
    void paintStatus (juce::Graphics&, juce::Rectangle<int>, juce::Colour, const juce::String& text, const juce::String& right);

    struct Suggestion
    {
        float agGain = 0, gate = -50, threshold = -1, predictedGr = 0, peakAfterAg = -100, lufsOut = -100;
        bool  truePeak = false, raiseLookahead = false;
    };
    Suggestion makeSuggestion() const;
    int heightForTab (int tab) const;

    AGLProcessor& proc;
    juce::SharedResourcePointer<AGLLookAndFeel> lnf;
    static inline std::atomic<int> openEditors { 0 };

    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAtt  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;

    // cabecalho
    juce::TextButton agPill, limPill, simButton, holdButton { "HOLD" }, powerButton { "ON" };
    juce::TextButton tabButtons[3];

    // Minimalista: knobs
    juce::Slider targetKnob, thresholdKnob, ceilingKnob;

    // Avancado - Auto Gain
    juce::Slider targetSlider, gateSlider, boostSlider, cutSlider;
    juce::ComboBox detectionBox, responseBox;
    // Avancado - Limiter
    juce::Slider thresholdSlider, ceilingSlider, releaseField;
    juce::TextButton linkButton { "LINK" }, autoRelButton { "AUTO" }, truePeakButton { "TRUE PEAK" };
    juce::ComboBox lookaheadBox;

    // Analise
    juce::ComboBox destinationBox, durationBox;
    juce::TextButton startButton { "Iniciar leitura" }, applyButton { "Aplicar nos dois" }, discardButton { "Descartar" };

    std::unique_ptr<ButtonAtt> powerAtt, holdAtt, agAtt, limAtt, linkAtt, autoRelAtt, truePeakAtt;
    std::unique_ptr<SliderAtt> targetKnobAtt, thresholdKnobAtt, ceilingKnobAtt;
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
