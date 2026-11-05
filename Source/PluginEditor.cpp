#include "PluginEditor.h"
#include "BinaryData.h"

using namespace juce;

namespace
{
    constexpr int W = 680, H = 760, M = 24;

    Font bold (float h)   { return Font (FontOptions (h, Font::bold)); }

    String fmt (float db, bool sign = false)
    {
        if (db <= -99.0f) return "-inf";
        return (sign && db >= 0.05f ? "+" : "") + String (db, 1);
    }

    // texto com brilho neon (sombra colorida desfocada atras do texto)
    void glow (Graphics& g, const String& t, Rectangle<int> r, Colour c, const Font& f, Justification j)
    {
        GlyphArrangement ga;
        ga.addFittedText (f, t, (float) r.getX(), (float) r.getY(), (float) r.getWidth(), (float) r.getHeight(), j, 1);
        Path p;
        ga.createPath (p);
        const int radius = jlimit (4, 14, (int) (f.getHeight() * 0.22f));
        DropShadow (c.withAlpha (0.55f), radius, {}).drawForPath (g, p);
        g.setColour (c);
        g.fillPath (p);
    }

    void label (Graphics& g, const String& t, Rectangle<int> r, Colour c = Neon::grey, float h = 13.0f,
                Justification j = Justification::centredLeft)
    {
        g.setFont (bold (h));
        g.setColour (c);
        g.drawText (t, r, j, false);
    }

    void wellBox (Graphics& g, Rectangle<int> r)
    {
        g.setColour (Neon::well);
        g.fillRoundedRectangle (r.toFloat(), 12.0f);
        g.setColour (Neon::border);
        g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 12.0f, 1.0f);
    }
}

// ============================================================ LookAndFeel
NeonLookAndFeel::NeonLookAndFeel()
{
    interRegular    = Typeface::createSystemTypefaceFor (BinaryData::InterRegular_otf,    (size_t) BinaryData::InterRegular_otfSize);
    interBold       = Typeface::createSystemTypefaceFor (BinaryData::InterBold_otf,       (size_t) BinaryData::InterBold_otfSize);
    interBoldItalic = Typeface::createSystemTypefaceFor (BinaryData::InterBoldItalic_otf, (size_t) BinaryData::InterBoldItalic_otfSize);

    setColour (TextButton::buttonColourId,   Neon::control);
    setColour (TextButton::buttonOnColourId, Colour (0xff002a2e));
    setColour (TextButton::textColourOffId,  Neon::light);
    setColour (TextButton::textColourOnId,   Neon::cyan);

    setColour (ComboBox::backgroundColourId, Neon::control);
    setColour (ComboBox::textColourId,       Neon::cyan);
    setColour (ComboBox::outlineColourId,    Colour (0xff333a44));
    setColour (ComboBox::arrowColourId,      Neon::cyan);
    setColour (PopupMenu::backgroundColourId,            Neon::control);
    setColour (PopupMenu::textColourId,                  Neon::white);
    setColour (PopupMenu::highlightedBackgroundColourId, Colour (0xff002a2e));
    setColour (PopupMenu::highlightedTextColourId,       Neon::cyan);

    setColour (Slider::backgroundColourId,     Colour (0xff1a1e24));
    setColour (Slider::trackColourId,          Neon::cyan.withAlpha (0.35f));
    setColour (Slider::thumbColourId,          Neon::cyan);
    setColour (Slider::textBoxTextColourId,    Neon::white);
    setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
    setColour (Slider::textBoxBackgroundColourId, Colours::transparentBlack);
}

void NeonLookAndFeel::drawButtonBackground (Graphics& g, Button& b, const Colour&, bool over, bool down)
{
    auto r  = b.getLocalBounds().toFloat().reduced (1.0f);
    auto on = b.getToggleState();
    auto accent = b.getProperties().getWithDefault ("accent", "cyan").toString() == "lime" ? Neon::lime : Neon::cyan;
    // botoes de acao (ex.: "Aplicar ajustes") ficam acesos quando estao disponiveis
    const bool action = (bool) b.getProperties().getWithDefault ("action", false);
    const bool lit = on || (action && b.isEnabled());

    g.setColour (lit ? accent.withAlpha (0.14f) : Neon::control.brighter (over ? 0.15f : 0.0f).darker (down ? 0.2f : 0.0f));
    g.fillRoundedRectangle (r, 10.0f);
    g.setColour (lit ? accent : Colour (0xff333a44));
    g.drawRoundedRectangle (r, 10.0f, lit ? 2.0f : 1.0f);
}

Typeface::Ptr NeonLookAndFeel::getTypefaceForFont (const Font& f)
{
    if (f.getTypefaceName() == Font::getDefaultSansSerifFontName())
    {
        if (f.isBold() && f.isItalic()) return interBoldItalic;
        if (f.isBold())                 return interBold;
        return interRegular;
    }
    return LookAndFeel_V4::getTypefaceForFont (f);
}

Font NeonLookAndFeel::getTextButtonFont (TextButton&, int h)
{
    return bold (jmin (18.0f, (float) h * 0.42f));
}

// ============================================================ Editor
AutoGainEditor::AutoGainEditor (AutoGainProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    // padrao global para que todo texto (inclusive menus) use a fonte Inter
    ++openEditors;
    LookAndFeel::setDefaultLookAndFeel (&lnf.get());
    setLookAndFeel (&lnf.get());

    const char* tabNames[] = { "Minimalista", "Avancado", "Analise" };
    for (int i = 0; i < 3; ++i)
    {
        auto& b = tabButtons[i];
        b.setButtonText (String::fromUTF8 (i == 1 ? "Avan\xc3\xa7" "ado" : i == 2 ? "An\xc3\xa1lise" : tabNames[i]));
        b.setRadioGroupId (1);
        b.setClickingTogglesState (true);
        b.onClick = [this, i] { if (tabButtons[i].getToggleState()) setTab (i); };
        addAndMakeVisible (b);
    }

    powerButton.setClickingTogglesState (true);
    powerButton.getProperties().set ("accent", "lime");
    powerButton.setColour (TextButton::textColourOnId, Neon::lime);
    addAndMakeVisible (powerButton);
    powerAtt = std::make_unique<ButtonAtt> (proc.apvts, "active", powerButton);

    holdButton.setClickingTogglesState (true);
    holdButton.setColour (TextButton::textColourOnId, Neon::yellow);
    addAndMakeVisible (holdButton);
    holdAtt = std::make_unique<ButtonAtt> (proc.apvts, "hold", holdButton);

    minusButton.onClick = [this] { nudgeTarget (-0.5f); };
    plusButton.onClick  = [this] { nudgeTarget (+0.5f); };
    addChildComponent (minusButton);
    addChildComponent (plusButton);

    targetSlider.setSliderStyle (Slider::LinearHorizontal);
    targetSlider.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
    targetSlider.setColour (Slider::trackColourId, Neon::cyan);
    addChildComponent (targetSlider);
    targetAtt = std::make_unique<SliderAtt> (proc.apvts, "target", targetSlider);

    auto setupBar = [this] (Slider& s, const String& id, std::unique_ptr<SliderAtt>& att)
    {
        s.setSliderStyle (Slider::LinearBar);
        s.setTextValueSuffix (" dB");
        addChildComponent (s);
        att = std::make_unique<SliderAtt> (proc.apvts, id, s);
    };
    setupBar (boostSlider,   "maxBoost", boostAtt);
    setupBar (cutSlider,     "maxCut",   cutAtt);
    setupBar (gateSlider,    "gate",     gateAtt);
    setupBar (ceilingSlider, "ceiling",  ceilingAtt);
    gateSlider.setTextValueSuffix (" dBFS");
    ceilingSlider.setTextValueSuffix (" dBFS");

    detectionBox.addItemList ({ "Pico", "RMS", "Pico + RMS" }, 1);
    responseBox.addItemList ({ "Lenta (ao vivo)", String::fromUTF8 ("M\xc3\xa9" "dia"), String::fromUTF8 ("R\xc3\xa1" "pida") }, 1);
    addChildComponent (detectionBox);
    addChildComponent (responseBox);
    detectionAtt = std::make_unique<ComboAtt> (proc.apvts, "detection", detectionBox);
    responseAtt  = std::make_unique<ComboAtt> (proc.apvts, "response",  responseBox);

    durationBox.addItem ("10 s", 10);
    durationBox.addItem ("30 s", 30);
    durationBox.addItem ("60 s", 60);
    durationBox.setSelectedId (30, dontSendNotification);
    addChildComponent (durationBox);

    startButton.onClick = [this]
    {
        if (proc.engine.analysisState.load() == AutoGainEngine::running)
            proc.engine.cancelAnalysis();
        else
        {
            applied = false;
            result = {};
            proc.engine.startAnalysis ((float) durationBox.getSelectedId());
        }
    };
    applyButton.getProperties().set ("accent", "lime");
    applyButton.getProperties().set ("action", true);
    applyButton.setColour (TextButton::textColourOffId, Neon::lime);
    applyButton.onClick = [this]
    {
        if (! result.valid) return;
        const auto s = makeSuggestion();
        setParam ("gate", s.gateDb);
        proc.engine.jumpGainTo (s.gainDb);
        applied = true;
    };
    discardButton.onClick = [this]
    {
        proc.engine.cancelAnalysis();
        result = {};
        applied = false;
    };
    addChildComponent (startButton);
    addChildComponent (applyButton);
    addChildComponent (discardButton);

    setSize (W, H);
    setTab (jlimit (0, 2, proc.uiTab.load()));
    startTimerHz (30);
}

AutoGainEditor::~AutoGainEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
    if (--openEditors == 0)                        // ultima janela do plugin fechando
        LookAndFeel::setDefaultLookAndFeel (nullptr);
}

float AutoGainEditor::param (const String& id) const
{
    return proc.apvts.getRawParameterValue (id)->load();
}

void AutoGainEditor::setParam (const String& id, float value)
{
    if (auto* p = proc.apvts.getParameter (id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (value));
        p->endChangeGesture();
    }
}

void AutoGainEditor::nudgeTarget (float deltaDb)
{
    setParam ("target", jlimit (-48.0f, 0.0f, param ("target") + deltaDb));
}

void AutoGainEditor::setTab (int tab)
{
    currentTab = tab;
    proc.uiTab.store (tab);
    tabButtons[tab].setToggleState (true, dontSendNotification);

    const bool mini = tab == 0, adv = tab == 1, ana = tab == 2;
    minusButton.setVisible (mini);
    plusButton.setVisible (mini);
    for (auto* c : std::initializer_list<Component*> { &targetSlider, &boostSlider, &cutSlider, &gateSlider,
                                                       &ceilingSlider, &detectionBox, &responseBox })
        c->setVisible (adv);
    for (auto* c : std::initializer_list<Component*> { &durationBox, &startButton, &applyButton, &discardButton })
        c->setVisible (ana);

    resized();
    repaint();
}

AutoGainEditor::Suggestion AutoGainEditor::makeSuggestion() const
{
    Suggestion s;
    if (! result.valid) return s;

    const float target = param ("target");
    const float level  = (int) param ("detection") == 0 ? result.peakDb : result.rmsDb;
    s.gainDb = jlimit (-param ("maxCut"), param ("maxBoost"), target - level);
    // gate 6 dB acima do ruido, mas sempre pelo menos 20 dB abaixo da media do sinal
    // (se a leitura nao teve pausas, o "ruido" medido e a propria voz)
    s.gateDb = jlimit (-80.0f, -30.0f, std::round (jmin (result.noiseDb + 6.0f, result.rmsDb - 20.0f)));
    s.overDb = result.peakDb + s.gainDb - param ("ceiling");
    return s;
}

void AutoGainEditor::timerCallback()
{
    const int state = proc.engine.analysisState.load();
    if (state == AutoGainEngine::done && ! result.valid)
        result = proc.engine.getAnalysisResult();

    startButton.setButtonText (state == AutoGainEngine::running ? "Cancelar"
                               : state == AutoGainEngine::done  ? "Ler novamente" : "Iniciar leitura");
    applyButton.setEnabled (result.valid && ! applied);
    powerButton.setButtonText (powerButton.getToggleState() ? "ON" : "OFF");
    repaint();
}

// ============================================================ Layout
void AutoGainEditor::resized()
{
    holdButton.setBounds (W - M - 64 - 10 - 76, 30, 76, 48);
    powerButton.setBounds (W - M - 64, 30, 64, 48);

    auto tabs = Rectangle<int> (M, 100, W - 2 * M, 48);
    const int tw = tabs.getWidth() / 3;
    for (int i = 0; i < 3; ++i)
        tabButtons[i].setBounds (tabs.getX() + i * tw, tabs.getY(), tw - (i < 2 ? 4 : 0), tabs.getHeight());

    // Minimalista
    minusButton.setBounds (W / 2 - 150, 318, 52, 52);
    plusButton.setBounds  (W / 2 + 98,  318, 52, 52);

    // Avancado
    targetSlider.setBounds (M + 12, 250, 452, 28);
    const int cw = (W - 2 * M - 20) / 3;
    auto cell = [&] (int col, int row) { return Rectangle<int> (M + col * (cw + 10), 540 + row * 78, cw, 68); };
    auto ctrl = [] (Rectangle<int> c) { return c.withTrimmedTop (26).reduced (8, 4); };
    detectionBox.setBounds  (ctrl (cell (0, 0)));
    responseBox.setBounds   (ctrl (cell (1, 0)));
    gateSlider.setBounds    (ctrl (cell (2, 0)));
    boostSlider.setBounds   (ctrl (cell (0, 1)));
    cutSlider.setBounds     (ctrl (cell (1, 1)));
    ceilingSlider.setBounds (ctrl (cell (2, 1)));

    // Analise
    durationBox.setBounds (W - M - 16 - 160 - 10 - 90, 186, 90, 40);
    startButton.setBounds (W - M - 16 - 160, 184, 160, 44);
    applyButton.setBounds (M, 668, 400, 52);
    discardButton.setBounds (M + 412, 668, W - 2 * M - 412, 52);
}

// ============================================================ Desenho
void AutoGainEditor::paint (Graphics& g)
{
    g.fillAll (Neon::black);
    auto panelR = getLocalBounds().reduced (8).toFloat();
    g.setColour (Neon::panel);
    g.fillRoundedRectangle (panelR, 18.0f);
    g.setColour (Neon::border);
    g.drawRoundedRectangle (panelR, 18.0f, 1.0f);

    paintHeader (g);
    if (currentTab == 0)      paintMinimal (g);
    else if (currentTab == 1) paintAdvanced (g);
    else                      paintAnalysis (g);
}

void AutoGainEditor::paintHeader (Graphics& g)
{
    Path logo;
    logo.startNewSubPath (M + 2.0f, 54.0f);
    logo.lineTo (M + 10.0f, 54.0f); logo.lineTo (M + 14.0f, 42.0f); logo.lineTo (M + 20.0f, 64.0f);
    logo.lineTo (M + 25.0f, 48.0f); logo.lineTo (M + 29.0f, 56.0f); logo.lineTo (M + 42.0f, 56.0f);
    g.setColour (Neon::cyan);
    g.strokePath (logo, PathStrokeType (3.0f, PathStrokeType::curved, PathStrokeType::rounded));

    g.setColour (Colours::white);
    g.setFont (bold (28.0f));
    g.drawText ("AUTO GAIN", M + 56, 26, 300, 30, Justification::centredLeft);
    label (g, "Intelligent Level Control", { M + 56, 56, 300, 18 }, Neon::grey, 14.0f);
    glow (g, "by Augusto Souza", { M + 56, 74, 300, 16 }, Neon::magenta, Font (FontOptions (12.0f, Font::bold | Font::italic)),
          Justification::centredLeft);
}

void AutoGainEditor::paintStatus (Graphics& g, Rectangle<int> area)
{
    wellBox (g, area);
    const bool active = param ("active") > 0.5f;
    const bool hold   = param ("hold") > 0.5f;

    Colour c = Neon::lime;
    String t = String::fromUTF8 ("Autom\xc3\xa1tico ativo");
    if (! active)                       { c = Neon::grey;   t = "Desligado (bypass)"; }
    else if (hold)                      { c = Neon::yellow; t = "HOLD - ganho congelado"; }
    else if (proc.engine.gateClosed.load())    { c = Neon::cyan;   t = String::fromUTF8 ("Congelado (sil\xc3\xaancio)"); }

    auto dot = Rectangle<float> ((float) area.getX() + 16.0f, (float) area.getCentreY() - 7.0f, 14.0f, 14.0f);
    g.setColour (c.withAlpha (0.3f));
    g.fillEllipse (dot.expanded (3.0f));
    g.setColour (c);
    g.fillEllipse (dot);
    label (g, t, area.withTrimmedLeft (42), c, 17.0f);
    label (g, String::fromUTF8 ("Lat\xc3\xaancia 0 ms"), area.withTrimmedRight (16), Neon::grey, 13.0f, Justification::centredRight);
}

void AutoGainEditor::paintMinimal (Graphics& g)
{
    label (g, "TARGET LEVEL", { 0, 170, W, 22 }, Neon::grey, 16.0f, Justification::centred);
    glow (g, fmt (param ("target")) + " dBFS", { 0, 196, W, 110 }, Neon::cyan, bold (84.0f), Justification::centred);
    label (g, String::fromUTF8 ("ajuste o alvo (passo 0,5 dB)"), { W / 2 - 92, 318, 184, 52 }, Neon::grey, 13.0f, Justification::centred);

    const int y = 392, h = 140, bw = (W - 2 * M - 14) / 2;
    auto a = Rectangle<int> (M, y, bw, h), b = Rectangle<int> (M + bw + 14, y, bw, h);
    wellBox (g, a);
    wellBox (g, b);

    label (g, "ENTRADA", a.withHeight (40).translated (0, 8), Neon::grey, 13.0f, Justification::centred);
    g.setColour (Neon::white);
    g.setFont (bold (36.0f));
    g.drawText (fmt (proc.engine.inRmsDb.load()) + " RMS", a.withY (y + 40).withHeight (46), Justification::centred);
    glow (g, "Pico " + fmt (proc.engine.inPeakDb.load()) + " dBFS", a.withY (y + 92).withHeight (24), Neon::yellow, bold (16.0f), Justification::centred);

    label (g, "AUTO GAIN", b.withHeight (40).translated (0, 8), Neon::grey, 13.0f, Justification::centred);
    glow (g, fmt (proc.engine.gainDb.load(), true) + " dB", b.withY (y + 40).withHeight (46), Neon::lime, bold (36.0f), Justification::centred);
    label (g, String::fromUTF8 ("Detec\xc3\xa7\xc3\xa3o: ") + detectionBox.getText(), b.withY (y + 92).withHeight (24),
           Neon::grey, 15.0f, Justification::centred);

    paintStatus (g, { M, 552, W - 2 * M, 60 });

    // barra de ganho bipolar
    auto bar = Rectangle<int> (M, 640, W - 2 * M, 14);
    const float lo = -param ("maxCut"), hi = param ("maxBoost");
    auto xOf = [&] (float db) { return (float) bar.getX() + (float) bar.getWidth() * (jlimit (lo, hi, db) - lo) / jmax (1.0f, hi - lo); };
    g.setColour (Colour (0xff1a1e24));
    g.fillRoundedRectangle (bar.toFloat(), 7.0f);
    const float x0 = xOf (0.0f), x1 = xOf (proc.engine.gainDb.load());
    g.setColour (Neon::lime);
    g.fillRect (Rectangle<float>::leftTopRightBottom (jmin (x0, x1), (float) bar.getY(), jmax (x0, x1), (float) bar.getBottom()));
    g.setColour (Colours::white);
    g.fillRect (x0 - 1.0f, (float) bar.getY() - 3.0f, 2.0f, (float) bar.getHeight() + 6.0f);
    label (g, String (lo, 0) + " dB", { M, 660, 100, 18 }, Neon::grey, 12.0f);
    label (g, "+" + String (hi, 0) + " dB", { W - M - 100, 660, 100, 18 }, Neon::grey, 12.0f, Justification::centredRight);
    label (g, "0", { (int) x0 - 10, 660, 20, 18 }, Colours::white, 12.0f, Justification::centred);
}

void AutoGainEditor::paintMeter (Graphics& g, Rectangle<int> bar, float rmsDb, float peakDb, float markerDb, Colour markerColour)
{
    g.setColour (Colour (0xff14171c));
    g.fillRoundedRectangle (bar.toFloat(), 4.0f);
    auto inner = bar.reduced (3).toFloat();
    auto yOf = [&] (float db) { return inner.getY() + inner.getHeight() * jlimit (0.0f, 1.0f, -db / 60.0f); };

    // RMS preenchido, cor por zona (verde ate -12, amarelo ate -6, vermelho acima)
    const float top = yOf (rmsDb);
    auto zone = [&] (float fromDb, float toDb, Colour c)
    {
        const float yA = jmax (top, yOf (toDb)), yB = yOf (fromDb);
        if (yB > yA) { g.setColour (c); g.fillRect (inner.getX(), yA, inner.getWidth(), yB - yA); }
    };
    zone (-60.0f, -12.0f, Neon::lime);
    zone (-12.0f, -6.0f, Neon::yellow);
    zone (-6.0f, 0.0f, Neon::red);

    // pico como linha
    g.setColour (peakDb > -6.0f ? Neon::red : Neon::yellow);
    g.fillRect (inner.getX(), yOf (peakDb) - 1.0f, inner.getWidth(), 2.0f);

    g.setColour (markerColour);
    g.fillRect ((float) bar.getX(), yOf (markerDb) - 1.0f, (float) bar.getWidth(), 2.0f);
}

void AutoGainEditor::paintAdvanced (Graphics& g)
{
    // Target
    auto t = Rectangle<int> (M, 164, 476, 136);
    wellBox (g, t);
    label (g, "TARGET LEVEL", { M + 16, 172, 200, 20 });
    glow (g, fmt (param ("target")) + " dBFS", { M + 16, 192, 400, 52 }, Neon::cyan, bold (46.0f), Justification::centredLeft);
    const char* marks[] = { "-48", "-36", "-24", "-12", "0" };
    for (int i = 0; i < 5; ++i)
        label (g, marks[i], { M + 12 + i * 113 - (i == 4 ? 30 : 0), 278, 40, 16 }, Neon::grey, 12.0f,
               i == 4 ? Justification::centredRight : Justification::centredLeft);

    // Entrada / saida
    const int rw = (476 - 12) / 2;
    auto in = Rectangle<int> (M, 312, rw, 88), out = Rectangle<int> (M + rw + 12, 312, rw, 88);
    wellBox (g, in);
    wellBox (g, out);
    label (g, "ENTRADA RMS", in.withHeight (24).translated (0, 6), Neon::grey, 12.0f, Justification::centred);
    label (g, String::fromUTF8 ("SA\xc3\x8d" "DA RMS"), out.withHeight (24).translated (0, 6), Neon::grey, 12.0f, Justification::centred);
    g.setColour (Neon::white);
    g.setFont (bold (28.0f));
    g.drawText (fmt (proc.engine.inRmsDb.load()),  in.withY (336).withHeight (32),  Justification::centred);
    g.drawText (fmt (proc.engine.outRmsDb.load()), out.withY (336).withHeight (32), Justification::centred);
    label (g, "pico " + fmt (proc.engine.inPeakDb.load()) + " dBFS",  in.withY (370).withHeight (20),  Neon::yellow, 12.0f, Justification::centred);
    label (g, "pico " + fmt (proc.engine.outPeakDb.load()) + " dBFS", out.withY (370).withHeight (20), Neon::yellow, 12.0f, Justification::centred);

    // Ganho aplicado (bipolar)
    auto ga = Rectangle<int> (M, 412, 476, 112);
    wellBox (g, ga);
    label (g, "GANHO APLICADO", { M + 16, 420, 200, 30 });
    glow (g, fmt (proc.engine.gainDb.load(), true) + " dB", { M + 200, 418, 260, 36 }, Neon::lime, bold (30.0f), Justification::centredRight);
    auto bar = Rectangle<int> (M + 16, 464, 444, 14);
    const float lo = -param ("maxCut"), hi = param ("maxBoost");
    auto xOf = [&] (float db) { return (float) bar.getX() + (float) bar.getWidth() * (jlimit (lo, hi, db) - lo) / jmax (1.0f, hi - lo); };
    g.setColour (Colour (0xff1a1e24));
    g.fillRoundedRectangle (bar.toFloat(), 7.0f);
    const float x0 = xOf (0.0f), x1 = xOf (proc.engine.gainDb.load());
    g.setColour (Neon::lime);
    g.fillRect (Rectangle<float>::leftTopRightBottom (jmin (x0, x1), (float) bar.getY(), jmax (x0, x1), (float) bar.getBottom()));
    g.setColour (Colours::white);
    g.fillRect (x0 - 1.0f, (float) bar.getY() - 3.0f, 2.0f, (float) bar.getHeight() + 6.0f);
    label (g, String (lo, 0), { bar.getX(), 484, 60, 16 }, Neon::grey, 12.0f);
    label (g, "0", { (int) x0 - 10, 484, 20, 16 }, Colours::white, 12.0f, Justification::centred);
    label (g, "+" + String (hi, 0), { bar.getRight() - 60, 484, 60, 16 }, Neon::grey, 12.0f, Justification::centredRight);
    const float gr = proc.engine.limiterGrDb.load();
    label (g, String::fromUTF8 ("Prote\xc3\xa7\xc3\xa3o de pico cortando: ") + String (jmax (0.0f, -gr), 1) + " dB", { bar.getX(), 502, 444, 16 },
           gr < -0.1f ? Neon::yellow : Neon::grey, 12.0f);

    // Medidores
    auto mw = Rectangle<int> (M + 488, 164, W - 2 * M - 488, 360);
    wellBox (g, mw);
    label (g, "IN",  { mw.getX() + 14, 172, 26, 18 }, Neon::grey, 13.0f, Justification::centred);
    label (g, "OUT", { mw.getX() + 50, 172, 30, 18 }, Neon::grey, 13.0f, Justification::centred);
    auto m1 = Rectangle<int> (mw.getX() + 16, 194, 24, 300), m2 = Rectangle<int> (mw.getX() + 54, 194, 24, 300);
    paintMeter (g, m1, proc.engine.inRmsDb.load(),  proc.engine.inPeakDb.load(),  param ("gate"),   Colours::white.withAlpha (0.8f));
    paintMeter (g, m2, proc.engine.outRmsDb.load(), proc.engine.outPeakDb.load(), param ("target"), Neon::cyan);
    for (int db : { 0, -6, -12, -18, -24, -36, -48, -60 })
    {
        const int y = 197 + (int) (294.0f * (float) -db / 60.0f);
        label (g, String (db), { mw.getX() + 86, y - 8, 40, 16 }, Neon::light, 11.0f);
    }
    label (g, "ciano: alvo / branco: gate", { mw.getX() + 6, 500, mw.getWidth() - 12, 16 }, Neon::grey, 10.0f, Justification::centred);

    // Parametros
    const int cw = (W - 2 * M - 20) / 3;
    const char* names[] = { "DETEC\xc3\x87\xc3\x83O", "RESPOSTA", "GATE (SIL\xc3\x8ANCIO)",
                            "AUMENTO M\xc3\x81XIMO", "CORTE M\xc3\x81XIMO", "PROTE\xc3\x87\xc3\x83O DE PICO" };
    for (int i = 0; i < 6; ++i)
    {
        auto c = Rectangle<int> (M + (i % 3) * (cw + 10), 540 + (i / 3) * 78, cw, 68);
        g.setColour (Neon::control);
        g.fillRoundedRectangle (c.toFloat(), 10.0f);
        g.setColour (Colour (0xff333a44));
        g.drawRoundedRectangle (c.toFloat().reduced (0.5f), 10.0f, 1.0f);
        label (g, String::fromUTF8 (names[i]), c.withHeight (26).withTrimmedLeft (12), Neon::grey, 11.0f);
    }

    paintStatus (g, { M, 700, W - 2 * M, 44 });
}

void AutoGainEditor::paintAnalysis (Graphics& g)
{
    const int state = proc.engine.analysisState.load();

    // Leitura
    auto top = Rectangle<int> (M, 164, W - 2 * M, 120);
    wellBox (g, top);
    label (g, "LEITURA DO DIA (PASSAGEM DE SOM)", { M + 16, 176, 360, 20 });
    label (g, "Toque ou fale normalmente durante a leitura.", { M + 16, 198, 340, 20 }, Neon::light, 14.0f);
    auto bar = Rectangle<float> ((float) M + 16.0f, 240.0f, (float) (W - 2 * M - 32), 10.0f);
    g.setColour (Colour (0xff1a1e24));
    g.fillRoundedRectangle (bar, 5.0f);
    const float prog = state == AutoGainEngine::done ? 1.0f : state == AutoGainEngine::running ? proc.engine.analysisProgress.load() : 0.0f;
    g.setColour (Neon::lime);
    g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * jlimit (0.0f, 1.0f, prog)), 5.0f);
    const String stTxt = state == AutoGainEngine::running ? "Lendo... " + String ((int) (prog * 100.0f)) + "%"
                       : state == AutoGainEngine::done    ? String::fromUTF8 ("Leitura conclu\xc3\xad" "da")
                                                             : "Pronto para ler";
    label (g, stTxt, { M + 16, 256, 300, 20 }, state == AutoGainEngine::done ? Neon::lime : Neon::grey, 13.0f);

    // Resultados
    const int bw = (W - 2 * M - 30) / 4;
    struct Item { const char* name; float value; Colour c; const char* unit; };
    const Item items[] = {
        { "PICO M\xc3\x81XIMO",   result.peakDb,                  Neon::yellow,  "dBFS" },
        { "RMS M\xc3\x89" "DIO",  result.rmsDb,                   Neon::cyan,    "dBFS" },
        { "DIN\xc3\x82MICA",      result.peakDb - result.rmsDb,   Neon::white,   "dB (pico - RMS)" },
        { "RU\xc3\x8D" "DO DE FUNDO", result.noiseDb,             Neon::magenta, "dBFS" } };
    for (int i = 0; i < 4; ++i)
    {
        auto r = Rectangle<int> (M + i * (bw + 10), 296, bw, 100);
        wellBox (g, r);
        label (g, String::fromUTF8 (items[i].name), r.withHeight (22).translated (0, 8), Neon::grey, 12.0f, Justification::centred);
        glow (g, result.valid ? String (items[i].value, 1) : "--", r.withY (330).withHeight (32), items[i].c, bold (26.0f), Justification::centred);
        label (g, items[i].unit, r.withY (364).withHeight (20), Neon::grey, 12.0f, Justification::centred);
    }

    // Sugestoes
    auto sg = Rectangle<int> (M, 408, W - 2 * M, 176);
    wellBox (g, sg);
    label (g, "AJUSTES SUGERIDOS PARA HOJE", { M + 16, 416, 400, 20 });
    const int c0 = M + 16, c1 = M + 260, c2 = M + 430;
    label (g, String::fromUTF8 ("PAR\xc3\x82METRO"), { c0, 440, 200, 18 }, Neon::grey, 12.0f);
    label (g, "ATUAL",    { c1, 440, 150, 18 }, Neon::grey, 12.0f);
    label (g, "SUGERIDO", { c2, 440, 150, 18 }, Neon::grey, 12.0f);

    const auto s = makeSuggestion();
    struct Row { const char* name; String now, sug; bool change; };
    const Row rows[] = {
        { "Ganho inicial",            fmt (proc.engine.gainDb.load(), true) + " dB", result.valid ? fmt (s.gainDb, true) + " dB" : "--", true },
        { "Gate (sil\xc3\xaancio)",   String (param ("gate"), 0) + " dBFS",   result.valid ? String (s.gateDb, 0) + " dBFS" : "--", true },
        { "Prote\xc3\xa7\xc3\xa3o de pico", String (param ("ceiling"), 1) + " dBFS", String::fromUTF8 ("mant\xc3\xa9m"), false },
        { "Detec\xc3\xa7\xc3\xa3o",   detectionBox.getText(),                 String::fromUTF8 ("mant\xc3\xa9m"), false } };
    for (int i = 0; i < 4; ++i)
    {
        const int y = 462 + i * 28;
        label (g, String::fromUTF8 (rows[i].name), { c0, y, 240, 24 }, Neon::white, 15.0f);
        label (g, rows[i].now, { c1, y, 160, 24 }, Neon::white, 15.0f);
        if (rows[i].change && result.valid) glow (g, rows[i].sug, { c2, y, 180, 24 }, Neon::lime, bold (15.0f), Justification::centredLeft);
        else                                label (g, rows[i].sug, { c2, y, 180, 24 }, Neon::light, 15.0f);
    }

    // Alerta
    auto al = Rectangle<int> (M, 596, W - 2 * M, 60);
    if (result.valid && s.overDb > 0.0f)
    {
        g.setColour (Colour (0xff1a1700));
        g.fillRoundedRectangle (al.toFloat(), 12.0f);
        g.setColour (Neon::yellow);
        g.drawRoundedRectangle (al.toFloat().reduced (0.5f), 12.0f, 1.0f);
        label (g, String::fromUTF8 ("Aten\xc3\xa7\xc3\xa3o: com ") + fmt (s.gainDb, true) + " dB o pico chegaria a "
                      + fmt (result.peakDb + s.gainDb, true) + String::fromUTF8 (" dBFS. A prote\xc3\xa7\xc3\xa3o vai cortar cerca de ")
                      + String (s.overDb, 1) + " dB nos trechos mais fortes.",
               al.reduced (16, 0), Neon::yellow, 13.0f);
    }
    else if (applied)
    {
        label (g, String::fromUTF8 ("Ajustes aplicados. O ganho autom\xc3\xa1tico continua a partir daqui."), al.reduced (16, 0), Neon::lime, 14.0f);
    }
}
