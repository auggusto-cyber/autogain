#include "PluginEditor.h"
#include "BinaryData.h"

using namespace juce;

// textos com acento: o arquivo e UTF-8 (no Windows compilado com /utf-8)
#define TXT(s) juce::String::fromUTF8 (s)

namespace
{
    constexpr int W = 1100, H = 820, M = 24;
    constexpr int CONTENT_Y = 232;

    Font bold (float h)   { return Font (FontOptions (h, Font::bold)); }
    Font plainF (float h) { return Font (FontOptions (h, Font::plain)); }

    String fmt (float db, int decimals = 1, bool sign = false)
    {
        if (db <= -99.0f) return "-inf";
        return (sign && db >= 0.05f ? "+" : "") + String (db, decimals);
    }

    void glow (Graphics& g, const String& t, Rectangle<int> r, Colour c, const Font& f, Justification j)
    {
        GlyphArrangement ga;
        ga.addFittedText (f, t, (float) r.getX(), (float) r.getY(), (float) r.getWidth(), (float) r.getHeight(), j, 1);
        Path p;
        ga.createPath (p);
        DropShadow (c.withAlpha (0.55f), jlimit (4, 14, (int) (f.getHeight() * 0.22f)), {}).drawForPath (g, p);
        g.setColour (c);
        g.fillPath (p);
    }

    void label (Graphics& g, const String& t, Rectangle<int> r, Colour c = Neon::grey, float h = 13.0f,
                Justification j = Justification::centredLeft, bool isBold = true)
    {
        g.setFont (isBold ? bold (h) : plainF (h));
        g.setColour (c);
        g.drawText (t, r, j, false);
    }

    void box (Graphics& g, Rectangle<int> r, Colour fill = Neon::well, Colour line = Neon::border, float radius = 12.0f, float lw = 1.0f)
    {
        g.setColour (fill);
        g.fillRoundedRectangle (r.toFloat(), radius);
        g.setColour (line);
        g.drawRoundedRectangle (r.toFloat().reduced (lw * 0.5f), radius, lw);
    }

    void arrow (Graphics& g, float x, float y)
    {
        Path p;
        p.startNewSubPath (x, y); p.lineTo (x + 18, y);
        p.startNewSubPath (x + 12, y - 5); p.lineTo (x + 18, y); p.lineTo (x + 12, y + 5);
        g.setColour (Neon::grey);
        g.strokePath (p, PathStrokeType (2.0f, PathStrokeType::curved, PathStrokeType::rounded));
    }

    String grText (float gr) { return gr > -0.05f ? String ("0.0 dB") : String (gr, 1) + " dB"; }

    float textWidth (const Font& f, const String& t)
    {
        GlyphArrangement ga;
        ga.addLineOfText (f, t, 0.0f, 0.0f);
        return ga.getBoundingBox (0, -1, true).getWidth();
    }

    Colour accentOf (const Component& c)
    {
        const auto a = c.getProperties().getWithDefault ("accent", "cyan").toString();
        if (a == "lime")    return Neon::lime;
        if (a == "magenta") return Neon::magenta;
        if (a == "yellow")  return Neon::yellow;
        return Neon::cyan;
    }
}

// ============================================================ LookAndFeel
NeonLookAndFeel::NeonLookAndFeel()
{
    interRegular    = Typeface::createSystemTypefaceFor (BinaryData::InterRegular_otf,    (size_t) BinaryData::InterRegular_otfSize);
    interBold       = Typeface::createSystemTypefaceFor (BinaryData::InterBold_otf,       (size_t) BinaryData::InterBold_otfSize);
    interBoldItalic = Typeface::createSystemTypefaceFor (BinaryData::InterBoldItalic_otf, (size_t) BinaryData::InterBoldItalic_otfSize);

    setColour (TextButton::buttonColourId,   Neon::control);
    setColour (TextButton::buttonOnColourId, Neon::control);
    setColour (TextButton::textColourOffId,  Neon::light);
    setColour (TextButton::textColourOnId,   Neon::cyan);

    setColour (ComboBox::backgroundColourId, Neon::control);
    setColour (ComboBox::textColourId,       Neon::cyan);
    setColour (ComboBox::outlineColourId,    Neon::edge);
    setColour (ComboBox::arrowColourId,      Neon::cyan);
    setColour (PopupMenu::backgroundColourId,            Neon::control);
    setColour (PopupMenu::textColourId,                  Neon::white);
    setColour (PopupMenu::highlightedBackgroundColourId, Colour (0xff002a2e));
    setColour (PopupMenu::highlightedTextColourId,       Neon::cyan);

    setColour (Slider::backgroundColourId,        Colour (0xff1a1e24));
    setColour (Slider::trackColourId,             Neon::cyan.withAlpha (0.35f));
    setColour (Slider::thumbColourId,             Neon::cyan);
    setColour (Slider::textBoxTextColourId,       Neon::white);
    setColour (Slider::textBoxOutlineColourId,    Colours::transparentBlack);
    setColour (Slider::textBoxBackgroundColourId, Colours::transparentBlack);
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

void NeonLookAndFeel::drawButtonBackground (Graphics& g, Button& b, const Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const auto accent = accentOf (b);
    const bool action = (bool) b.getProperties().getWithDefault ("action", false);
    const bool lit = b.getToggleState() || (action && b.isEnabled());

    g.setColour (lit ? accent.withAlpha (0.14f) : Neon::control.brighter (over ? 0.15f : 0.0f).darker (down ? 0.2f : 0.0f));
    g.fillRoundedRectangle (r, 10.0f);
    g.setColour (lit ? accent : Neon::edge);
    g.drawRoundedRectangle (r, 10.0f, lit ? 2.0f : 1.0f);
}

Font NeonLookAndFeel::getTextButtonFont (TextButton&, int h)
{
    return bold (jmin (17.0f, (float) h * 0.40f));
}

void NeonLookAndFeel::drawLinearSlider (Graphics& g, int x, int y, int w, int h, float pos, float minPos, float maxPos,
                                        Slider::SliderStyle style, Slider& s)
{
    const auto accent = accentOf (s);

    if (style == Slider::LinearVertical)
    {
        const auto tr = Rectangle<float> ((float) x + (float) w * 0.5f - 9.0f, (float) y, 18.0f, (float) h);
        g.setColour (Neon::track);
        g.fillRoundedRectangle (tr, 5.0f);
        const bool fromTop = s.getProperties().getWithDefault ("fill", "bottom").toString() == "top";
        g.setColour (accent.withAlpha (0.35f));
        if (fromTop) g.fillRoundedRectangle (tr.withBottom (pos), 5.0f);
        else         g.fillRoundedRectangle (tr.withTop (pos), 5.0f);

        auto thumb = Rectangle<float> ((float) x + 2.0f, pos - 11.0f, (float) w - 4.0f, 22.0f);
        g.setColour (Neon::panel);
        g.fillRoundedRectangle (thumb, 5.0f);
        g.setColour (accent);
        g.drawRoundedRectangle (thumb, 5.0f, 2.0f);
        g.setFont (bold (12.0f));
        g.drawText (String (s.getValue(), 1), thumb, Justification::centred, false);
        return;
    }

    if (style == Slider::LinearHorizontal)
    {
        const float cy = (float) y + (float) h * 0.5f;
        g.setColour (Neon::edge);
        g.fillRoundedRectangle ((float) x, cy - 3.0f, (float) w, 6.0f, 3.0f);
        g.setColour (accent);
        g.fillRoundedRectangle ((float) x, cy - 3.0f, pos - (float) x, 6.0f, 3.0f);
        g.setColour (Neon::panel);
        g.fillEllipse (pos - 11.0f, cy - 11.0f, 22.0f, 22.0f);
        g.setColour (accent);
        g.drawEllipse (pos - 10.0f, cy - 10.0f, 20.0f, 20.0f, 2.0f);
        return;
    }

    LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, pos, minPos, maxPos, style, s);
}

// ============================================================ Editor
AGLEditor::AGLEditor (AGLProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    ++openEditors;
    LookAndFeel::setDefaultLookAndFeel (&lnf.get());
    setLookAndFeel (&lnf.get());

    auto& st = proc.apvts;

    // ---- abas ----
    const char* names[] = { "Minimalista", "Avançado", "Análise" };
    for (int i = 0; i < 3; ++i)
    {
        auto& b = tabButtons[i];
        b.setButtonText (TXT (names[i]));
        b.setRadioGroupId (1);
        b.setClickingTogglesState (true);
        b.onClick = [this, i] { if (tabButtons[i].getToggleState()) setTab (i); };
        addAndMakeVisible (b);
    }

    // ---- geral ----
    auto toggle = [this] (TextButton& b, const char* accent, Colour textOn)
    {
        b.setClickingTogglesState (true);
        b.getProperties().set ("accent", accent);
        b.setColour (TextButton::textColourOnId, textOn);
        addAndMakeVisible (b);
    };
    toggle (powerButton, "lime", Neon::lime);
    toggle (holdButton, "yellow", Neon::yellow);
    toggle (agButton, "cyan", Neon::cyan);
    toggle (limButton, "magenta", Neon::magenta);
    powerAtt = std::make_unique<ButtonAtt> (st, "active", powerButton);
    holdAtt  = std::make_unique<ButtonAtt> (st, "hold", holdButton);
    agAtt    = std::make_unique<ButtonAtt> (st, "agOn", agButton);
    limAtt   = std::make_unique<ButtonAtt> (st, "limOn", limButton);

    // ---- Minimalista ----
    auto stepper = [this] (TextButton& b, const char* accent, std::function<void()> fn)
    {
        b.getProperties().set ("accent", accent);
        b.setColour (TextButton::textColourOffId, accentOf (b));
        b.onClick = std::move (fn);
        addChildComponent (b);
    };
    stepper (targetDown, "cyan",   [this] { nudge ("target", -0.5f, -48.0f, 0.0f); });
    stepper (targetUp,   "cyan",   [this] { nudge ("target", +0.5f, -48.0f, 0.0f); });
    stepper (threshDown, "cyan",   [this] { nudge ("threshold", -0.5f, -30.0f, 0.0f); });
    stepper (threshUp,   "cyan",   [this] { nudge ("threshold", +0.5f, -30.0f, 0.0f); });
    stepper (ceilDown,   "yellow", [this] { nudge ("ceiling", -0.1f, -12.0f, 0.0f); });
    stepper (ceilUp,     "yellow", [this] { nudge ("ceiling", +0.1f, -12.0f, 0.0f); });

    // ---- Avancado: Auto Gain ----
    targetSlider.setSliderStyle (Slider::LinearHorizontal);
    targetSlider.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
    addChildComponent (targetSlider);
    targetAtt = std::make_unique<SliderAtt> (st, "target", targetSlider);

    auto bar = [this, &st] (Slider& s, const char* id, const char* suffix, std::unique_ptr<SliderAtt>& att)
    {
        s.setSliderStyle (Slider::LinearBar);
        s.setTextValueSuffix (suffix);
        addChildComponent (s);
        att = std::make_unique<SliderAtt> (st, id, s);
    };
    bar (gateSlider,  "gate",     " dBFS", gateAtt);
    bar (boostSlider, "maxBoost", " dB",   boostAtt);
    bar (cutSlider,   "maxCut",   " dB",   cutAtt);

    detectionBox.addItemList ({ "Pico", "RMS", "Pico + RMS" }, 1);
    responseBox.addItemList ({ "Lenta (ao vivo)", TXT ("Média"), TXT ("Rápida") }, 1);
    addChildComponent (detectionBox);
    addChildComponent (responseBox);
    detectionAtt = std::make_unique<ComboAtt> (st, "detection", detectionBox);
    responseAtt  = std::make_unique<ComboAtt> (st, "response",  responseBox);

    // ---- Avancado: Limiter ----
    auto vert = [this, &st] (Slider& s, const char* id, const char* accent, const char* fill, std::unique_ptr<SliderAtt>& att)
    {
        s.setSliderStyle (Slider::LinearVertical);
        s.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
        s.getProperties().set ("accent", accent);
        s.getProperties().set ("fill", fill);
        addChildComponent (s);
        att = std::make_unique<SliderAtt> (st, id, s);
    };
    vert (thresholdSlider, "threshold", "cyan",   "top",    thresholdAtt);
    vert (ceilingSlider,   "ceiling",   "yellow", "bottom", ceilingAtt);

    lastThreshold = proc.param ("threshold");
    thresholdSlider.onValueChange = [this]
    {
        const float now = (float) thresholdSlider.getValue();
        if (proc.param ("link") > 0.5f)
            setParam ("ceiling", jlimit (-12.0f, 0.0f, proc.param ("ceiling") + (now - lastThreshold)));
        lastThreshold = now;
    };

    releaseSlider.setSliderStyle (Slider::LinearBar);
    releaseSlider.setTextValueSuffix (" ms");
    addChildComponent (releaseSlider);
    releaseAtt = std::make_unique<SliderAtt> (st, "release", releaseSlider);

    auto smallToggle = [this] (TextButton& b, const char* accent, Colour on)
    {
        b.setClickingTogglesState (true);
        b.getProperties().set ("accent", accent);
        b.setColour (TextButton::textColourOnId, on);
        addChildComponent (b);
    };
    smallToggle (linkButton, "cyan", Neon::cyan);
    smallToggle (autoRelButton, "lime", Neon::lime);
    smallToggle (truePeakButton, "yellow", Neon::yellow);
    linkAtt     = std::make_unique<ButtonAtt> (st, "link", linkButton);
    autoRelAtt  = std::make_unique<ButtonAtt> (st, "autoRelease", autoRelButton);
    truePeakAtt = std::make_unique<ButtonAtt> (st, "truePeak", truePeakButton);

    lookaheadBox.addItemList ({ "0 ms (ao vivo)", "1 ms", "3 ms", "5 ms" }, 1);
    addChildComponent (lookaheadBox);
    lookaheadAtt = std::make_unique<ComboAtt> (st, "lookahead", lookaheadBox);

    // ---- Analise ----
    destinationBox.addItem ("Ao vivo", 1);
    destinationBox.addItem ("Streaming (-14 LUFS)", 2);
    destinationBox.setSelectedId (1, dontSendNotification);
    destinationBox.onChange = [this] { applied = false; };
    durationBox.addItem ("10 s", 10);
    durationBox.addItem ("30 s", 30);
    durationBox.addItem ("60 s", 60);
    durationBox.setSelectedId (30, dontSendNotification);
    addChildComponent (destinationBox);
    addChildComponent (durationBox);

    startButton.onClick = [this]
    {
        if (proc.analyzer.state.load() == Analyzer::running)
            proc.analyzer.cancel();
        else
        {
            applied = false;
            result = {};
            proc.analyzer.start ((float) durationBox.getSelectedId());
        }
    };
    applyButton.getProperties().set ("accent", "lime");
    applyButton.getProperties().set ("action", true);
    applyButton.setColour (TextButton::textColourOffId, Neon::lime);
    applyButton.onClick = [this]
    {
        if (! result.valid) return;
        const auto s = makeSuggestion();
        setParam ("gate", s.gate);
        setParam ("threshold", s.threshold);
        setParam ("autoRelease", 1.0f);
        if (s.truePeak)       setParam ("truePeak", 1.0f);
        if (s.raiseLookahead) setParam ("lookahead", 1.0f);
        if (proc.param ("agOn") > 0.5f) proc.ag.jumpGainTo (s.agGain);
        lastThreshold = s.threshold;
        applied = true;
    };
    discardButton.onClick = [this] { proc.analyzer.cancel(); result = {}; applied = false; };
    addChildComponent (startButton);
    addChildComponent (applyButton);
    addChildComponent (discardButton);

    setSize (W, H);
    setTab (jlimit (0, 2, proc.uiTab.load()));
    startTimerHz (30);
}

AGLEditor::~AGLEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
    if (--openEditors == 0)
        LookAndFeel::setDefaultLookAndFeel (nullptr);
}

void AGLEditor::setParam (const String& id, float value)
{
    if (auto* p = proc.apvts.getParameter (id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (value));
        p->endChangeGesture();
    }
}

void AGLEditor::nudge (const String& id, float delta, float lo, float hi)
{
    const float now = proc.param (id.toRawUTF8());
    setParam (id, jlimit (lo, hi, now + delta));
    if (id == "threshold" && proc.param ("link") > 0.5f)
        setParam ("ceiling", jlimit (-12.0f, 0.0f, proc.param ("ceiling") + delta));
    if (id == "threshold")
        lastThreshold = proc.param ("threshold");
}

void AGLEditor::setTab (int tab)
{
    currentTab = tab;
    proc.uiTab.store (tab);
    tabButtons[tab].setToggleState (true, dontSendNotification);

    const bool mini = tab == 0, adv = tab == 1, ana = tab == 2;
    for (auto* c : std::initializer_list<Component*> { &targetDown, &targetUp, &threshDown, &threshUp, &ceilDown, &ceilUp })
        c->setVisible (mini);
    for (auto* c : std::initializer_list<Component*> { &targetSlider, &gateSlider, &boostSlider, &cutSlider, &detectionBox, &responseBox,
                                                       &thresholdSlider, &ceilingSlider, &releaseSlider, &linkButton, &autoRelButton,
                                                       &truePeakButton, &lookaheadBox })
        c->setVisible (adv);
    for (auto* c : std::initializer_list<Component*> { &destinationBox, &durationBox, &startButton, &applyButton, &discardButton })
        c->setVisible (ana);

    resized();
    repaint();
}

AGLEditor::Suggestion AGLEditor::makeSuggestion() const
{
    Suggestion s;
    if (! result.valid) return s;

    const bool agOn = proc.param ("agOn") > 0.5f;
    // mesma regra do motor: Pico, RMS ou Pico + RMS (o menor dos dois)
    const int det = (int) proc.param ("detection");
    const float target = proc.param ("target");
    float want = det == 0 ? target - result.peakDb : target - result.rmsDb;
    if (det == 2) want = jmin (want, proc.param ("ceiling") - result.peakDb);
    s.agGain = agOn ? jlimit (-proc.param ("maxCut"), proc.param ("maxBoost"), want) : 0.0f;
    s.gate   = jlimit (-80.0f, -30.0f, std::round (jmin (result.noiseDb + 6.0f, result.rmsDb - 20.0f)));

    const float ceiling = proc.param ("ceiling");
    s.peakAfterAg = result.truePeakDb + s.agGain;
    const float lufsAfterAg = result.lufs + s.agGain;

    float limGain = 0.0f;
    if (destinationBox.getSelectedId() == 2)                 // streaming: -14 LUFS
    {
        limGain = -14.0f - lufsAfterAg;
        s.threshold = jlimit (-30.0f, 0.0f, std::round ((ceiling - limGain) * 2.0f) / 2.0f);
        limGain = ceiling - s.threshold;
        s.truePeak = true;
        s.raiseLookahead = (int) proc.param ("lookahead") == 0;
    }
    else                                                      // ao vivo: so protecao
    {
        s.threshold = ceiling;
    }
    s.predictedGr = jmax (0.0f, s.peakAfterAg + limGain - ceiling);
    s.lufsOut = lufsAfterAg + limGain;
    return s;
}

void AGLEditor::timerCallback()
{
    // historico da atenuacao (grafico)
    grHistory[(size_t) grHead] = proc.lim.grPeakSinceRead.exchange (0.0f);
    grHead = (grHead + 1) % (int) grHistory.size();

    const int state = proc.analyzer.state.load();
    if (state == Analyzer::done && ! result.valid)
        result = proc.analyzer.result();

    startButton.setButtonText (state == Analyzer::running ? "Cancelar"
                               : state == Analyzer::done  ? "Ler novamente" : "Iniciar leitura");
    applyButton.setEnabled (result.valid && ! applied);

    powerButton.setButtonText (powerButton.getToggleState() ? "ON" : "OFF");
    agButton.setButtonText  (String ("AUTO GAIN  ") + (agButton.getToggleState()  ? "ON" : "OFF"));
    limButton.setButtonText (String ("LIMITER  ")   + (limButton.getToggleState() ? "ON" : "OFF"));
    autoRelButton.setButtonText (autoRelButton.getToggleState() ? "AUTO" : "MANUAL");
    truePeakButton.setButtonText (String ("TRUE PEAK  ") + (truePeakButton.getToggleState() ? "ON" : "OFF"));
    repaint();
}

// ============================================================ Layout
void AGLEditor::resized()
{
    powerButton.setBounds (W - M - 64, 30, 64, 48);
    holdButton.setBounds (W - M - 64 - 10 - 86, 30, 86, 48);

    agButton.setBounds (M + 184, 112, 176, 36);
    limButton.setBounds (M + 404, 112, 160, 36);

    const int tw = (W - 2 * M - 8) / 3;
    for (int i = 0; i < 3; ++i)
        tabButtons[i].setBounds (M + i * (tw + 4), 168, tw, 48);

    // Minimalista
    const int agX = M, limX = M + 538, pw = 514, y0 = CONTENT_Y;
    targetDown.setBounds (agX + pw / 2 - 64, y0 + 178, 52, 52);
    targetUp.setBounds   (agX + pw / 2 + 12, y0 + 178, 52, 52);
    const int colW = (pw - 48) / 2;
    const int c1 = limX + 16, c2 = limX + 16 + colW + 16;
    threshDown.setBounds (c1 + colW / 2 - 52, y0 + 162, 46, 46);
    threshUp.setBounds   (c1 + colW / 2 + 6,  y0 + 162, 46, 46);
    ceilDown.setBounds   (c2 + colW / 2 - 52, y0 + 162, 46, 46);
    ceilUp.setBounds     (c2 + colW / 2 + 6,  y0 + 162, 46, 46);

    // Avancado - Auto Gain (coluna x = M, largura 420)
    targetSlider.setBounds (M + 16, y0 + 84, 388, 28);
    const int gw = (420 - 32 - 10) / 2;
    auto cell = [&] (int col, int row) { return Rectangle<int> (M + 16 + col * (gw + 10), y0 + 320 + row * 74, gw, 64); };
    auto ctrl = [] (Rectangle<int> c) { return c.withTrimmedTop (24).reduced (8, 3); };
    detectionBox.setBounds (ctrl (cell (0, 0)));
    responseBox.setBounds  (ctrl (cell (1, 0)));
    gateSlider.setBounds   (ctrl (cell (0, 1)));
    boostSlider.setBounds  (ctrl (cell (1, 1)));
    cutSlider.setBounds    (ctrl (cell (0, 2)));

    // Avancado - Limiter (coluna x = M + 584, largura 468)
    const int lx = M + 584;
    thresholdSlider.setBounds (lx + 14, y0 + 68, 72, 300);
    linkButton.setBounds      (lx + 94, y0 + 196, 56, 44);
    ceilingSlider.setBounds   (lx + 158, y0 + 68, 72, 300);
    const int by = y0 + 412;
    releaseSlider.setBounds  (lx + 16, by + 26, 200, 30);
    autoRelButton.setBounds  (lx + 224, by + 22, 84, 36);
    lookaheadBox.setBounds   (lx + 16, by + 96, 200, 34);
    truePeakButton.setBounds (lx + 224, by + 92, 228, 40);

    // Analise
    destinationBox.setBounds (W - M - 16 - 160 - 10 - 90 - 10 - 200, y0 + 22, 200, 40);
    durationBox.setBounds    (W - M - 16 - 160 - 10 - 90, y0 + 22, 90, 40);
    startButton.setBounds    (W - M - 16 - 160, y0 + 20, 160, 44);
    applyButton.setBounds    (M, H - M - 52, 640, 52);
    discardButton.setBounds  (M + 652, H - M - 52, W - 2 * M - 652, 52);
}

// ============================================================ Desenho
void AGLEditor::paint (Graphics& g)
{
    g.fillAll (Neon::black);
    auto panelR = getLocalBounds().reduced (8).toFloat();
    g.setColour (Neon::panel);
    g.fillRoundedRectangle (panelR, 18.0f);
    g.setColour (Neon::border);
    g.drawRoundedRectangle (panelR, 18.0f, 1.0f);

    paintHeader (g);
    paintChain (g);
    if (currentTab == 0)      paintMinimal (g);
    else if (currentTab == 1) paintAdvanced (g);
    else                      paintAnalysis (g);
}

void AGLEditor::paintHeader (Graphics& g)
{
    Path logo;
    logo.startNewSubPath (M + 2.0f, 58.0f);
    logo.lineTo (M + 10.0f, 58.0f); logo.lineTo (M + 14.0f, 46.0f); logo.lineTo (M + 20.0f, 68.0f);
    logo.lineTo (M + 25.0f, 52.0f); logo.lineTo (M + 29.0f, 60.0f); logo.lineTo (M + 42.0f, 60.0f);
    g.setColour (Neon::cyan);
    g.strokePath (logo, PathStrokeType (3.0f, PathStrokeType::curved, PathStrokeType::rounded));
    g.setColour (Neon::magenta);
    for (float x = M + 2.0f; x < M + 42.0f; x += 6.0f)
        g.fillRect (x, 40.0f, 3.0f, 2.0f);

    const auto tf = bold (28.0f);
    const String a = "AUTO GAIN ", plus = "+ ", b = "LIMITER";
    float x = M + 56.0f;
    g.setFont (tf);
    g.setColour (Colours::white); g.drawText (a, (int) x, 26, 400, 32, Justification::centredLeft, false); x += textWidth (tf, a) + 8.0f;
    g.setColour (Neon::magenta);  g.drawText (plus, (int) x, 26, 60, 32, Justification::centredLeft, false); x += textWidth (tf, plus) + 8.0f;
    g.setColour (Colours::white); g.drawText (b, (int) x, 26, 300, 32, Justification::centredLeft, false);
    label (g, "Intelligent Level Control", { M + 56, 58, 300, 18 }, Neon::grey, 14.0f, Justification::centredLeft, false);
    glow (g, "by Augusto Souza", { M + 56, 76, 300, 16 }, Neon::magenta, Font (FontOptions (12.0f, Font::bold | Font::italic)),
          Justification::centredLeft);
}

void AGLEditor::paintChain (Graphics& g)
{
    auto r = Rectangle<int> (M, 104, W - 2 * M, 52);
    box (g, r);
    label (g, "CADEIA", { M + 14, 104, 70, 52 });
    label (g, "Entrada", { M + 84, 104, 70, 52 }, Neon::light, 14.0f, Justification::centredLeft, false);
    arrow (g, M + 156.0f, 130.0f);
    arrow (g, M + 374.0f, 130.0f);
    arrow (g, M + 574.0f, 130.0f);
    label (g, TXT ("Saída"), { M + 602, 104, 70, 52 }, Neon::light, 14.0f, Justification::centredLeft, false);

    const int lat = proc.getLatencySamples();
    const double sr = proc.getSampleRate() > 0.0 ? proc.getSampleRate() : 48000.0;
    const String latTxt = TXT ("Latência ") + String (lat * 1000.0 / sr, 2) + " ms  (" + String (lat) + " amostras)";
    label (g, latTxt, r.withTrimmedRight (16), lat == 0 ? Neon::lime : Neon::cyan, 14.0f, Justification::centredRight);
}

void AGLEditor::paintLevelMeter (Graphics& g, Rectangle<int> bar, float levelDb, float peakDb, float markDb, Colour mark)
{
    g.setColour (Neon::track);
    g.fillRoundedRectangle (bar.toFloat(), 4.0f);
    auto in = bar.reduced (3).toFloat();
    auto yOf = [&] (float db) { return in.getY() + in.getHeight() * jlimit (0.0f, 1.0f, -db / 60.0f); };

    const float top = yOf (levelDb);
    auto zone = [&] (float fromDb, float toDb, Colour c)
    {
        const float yA = jmax (top, yOf (toDb)), yB = yOf (fromDb);
        if (yB > yA) { g.setColour (c); g.fillRect (in.getX(), yA, in.getWidth(), yB - yA); }
    };
    zone (-60.0f, -12.0f, Neon::lime);
    zone (-12.0f, -6.0f, Neon::yellow);
    zone (-6.0f, 0.0f, Neon::red);
    g.setColour (peakDb > -6.0f ? Neon::red : Neon::yellow);
    g.fillRect (in.getX(), yOf (peakDb) - 1.0f, in.getWidth(), 2.0f);
    if (markDb > -99.0f)
    {
        g.setColour (mark);
        g.fillRect ((float) bar.getX(), yOf (markDb) - 1.0f, (float) bar.getWidth(), 2.0f);
    }
}

void AGLEditor::paintGrMeter (Graphics& g, Rectangle<int> bar, float grDb)
{
    g.setColour (Neon::track);
    g.fillRoundedRectangle (bar.toFloat(), 4.0f);
    auto in = bar.reduced (3).toFloat();
    const float h = in.getHeight() * jlimit (0.0f, 1.0f, -grDb / 12.0f);
    g.setColour (Neon::magenta);
    g.fillRect (in.getX(), in.getY(), in.getWidth(), h);
}

void AGLEditor::paintMinimal (Graphics& g)
{
    const int y0 = CONTENT_Y, pw = 514, ph = 476;
    const bool master = proc.param ("active") > 0.5f;

    // ---------- 1. AUTO GAIN ----------
    {
        const auto r = Rectangle<int> (M, y0, pw, ph);
        const bool on = master && proc.param ("agOn") > 0.5f;
        box (g, r, Neon::well, on ? Neon::cyan : Neon::edge, 14.0f, 1.5f);
        label (g, TXT ("1 · AUTO GAIN"), { M + 18, y0 + 12, 300, 22 }, on ? Neon::cyan : Neon::grey, 14.0f);
        label (g, "TARGET LEVEL", { M, y0 + 46, pw, 20 }, Neon::grey, 13.0f, Justification::centred);
        glow (g, fmt (proc.param ("target")), { M, y0 + 66, pw, 80 }, Neon::cyan, bold (72.0f), Justification::centred);
        label (g, "dBFS", { M, y0 + 146, pw, 20 }, Neon::grey, 14.0f, Justification::centred, false);

        const int bw = (pw - 32 - 12) / 2;
        auto a = Rectangle<int> (M + 16, y0 + 250, bw, 120), b = Rectangle<int> (M + 16 + bw + 12, y0 + 250, bw, 120);
        box (g, a, Neon::panel); box (g, b, Neon::panel);
        label (g, "ENTRADA", a.withHeight (30).translated (0, 8), Neon::grey, 12.0f, Justification::centred);
        g.setColour (Neon::white); g.setFont (bold (30.0f));
        g.drawText (fmt (proc.ag.inRmsDb.load()) + " RMS", a.withY (y0 + 286).withHeight (40), Justification::centred);
        label (g, "pico " + fmt (proc.ag.inPeakDb.load()) + " dBFS", a.withY (y0 + 330).withHeight (24), Neon::yellow, 14.0f, Justification::centred);
        label (g, "GANHO", b.withHeight (30).translated (0, 8), Neon::grey, 12.0f, Justification::centred);
        glow (g, fmt (proc.ag.gainDb.load(), 1, true) + " dB", b.withY (y0 + 286).withHeight (40), on ? Neon::lime : Neon::grey, bold (30.0f), Justification::centred);
        label (g, detectionBox.getText(), b.withY (y0 + 330).withHeight (24), Neon::grey, 14.0f, Justification::centred, false);

        Colour c = Neon::lime; String t = TXT ("Automático ativo");
        if (! on)                                   { c = Neon::grey;   t = "Desligado"; }
        else if (proc.param ("hold") > 0.5f)        { c = Neon::yellow; t = "HOLD: ganho congelado"; }
        else if (proc.ag.gateClosed.load())         { c = Neon::cyan;   t = TXT ("Congelado (silêncio)"); }
        auto st = Rectangle<int> (M + 16, y0 + 396, pw - 32, 56);
        box (g, st, Neon::panel);
        g.setColour (c); g.fillEllipse ((float) st.getX() + 16.0f, (float) st.getCentreY() - 7.0f, 14.0f, 14.0f);
        label (g, t, st.withTrimmedLeft (42), c, 16.0f);
        label (g, TXT ("0 ms"), st.withTrimmedRight (16), Neon::grey, 13.0f, Justification::centredRight);
    }

    // ---------- 2. LIMITER ----------
    {
        const int x = M + 538;
        const auto r = Rectangle<int> (x, y0, pw, ph);
        const bool on = master && proc.param ("limOn") > 0.5f;
        box (g, r, Neon::well, on ? Neon::magenta : Neon::edge, 14.0f, 1.5f);
        label (g, TXT ("2 · LIMITER"), { x + 18, y0 + 12, 300, 22 }, on ? Neon::magenta : Neon::grey, 14.0f);

        const int colW = (pw - 48) / 2;
        const int c1 = x + 16, c2 = x + 16 + colW + 16;
        label (g, "THRESHOLD", { c1, y0 + 46, colW, 20 }, Neon::grey, 13.0f, Justification::centred);
        label (g, "CEILING", { c2, y0 + 46, colW, 20 }, Neon::grey, 13.0f, Justification::centred);
        glow (g, fmt (proc.param ("threshold")), { c1, y0 + 66, colW, 64 }, Neon::cyan, bold (54.0f), Justification::centred);
        glow (g, fmt (proc.param ("ceiling")),   { c2, y0 + 66, colW, 64 }, Neon::yellow, bold (54.0f), Justification::centred);
        label (g, "dB", { c1, y0 + 132, colW, 20 }, Neon::grey, 13.0f, Justification::centred, false);
        label (g, "dBFS", { c2, y0 + 132, colW, 20 }, Neon::grey, 13.0f, Justification::centred, false);

        const float gr = on ? proc.lim.grDb.load() : 0.0f;
        auto at = Rectangle<int> (x + 16, y0 + 236, pw - 32, 100);
        box (g, at, Neon::panel);
        label (g, TXT ("ATENUAÇÃO"), { at.getX() + 14, at.getY() + 8, 200, 30 }, Neon::grey, 12.0f);
        glow (g, grText (gr), { at.getRight() - 214, at.getY() + 6, 200, 34 }, Neon::magenta, bold (28.0f), Justification::centredRight);
        auto bar = Rectangle<float> ((float) at.getX() + 14.0f, (float) at.getY() + 52.0f, (float) at.getWidth() - 28.0f, 14.0f);
        g.setColour (Colour (0xff1a1e24)); g.fillRoundedRectangle (bar, 7.0f);
        const float frac = jlimit (0.0f, 1.0f, -gr / 12.0f);
        g.setColour (Neon::magenta); g.fillRoundedRectangle (bar.withLeft (bar.getRight() - bar.getWidth() * frac), 7.0f);
        const char* ticks[] = { "-12", "-9", "-6", "-3", "0" };
        for (int i = 0; i < 5; ++i)
            label (g, ticks[i], { (int) bar.getX() + (int) (bar.getWidth() * (float) i / 4.0f) - 16, (int) bar.getBottom() + 4, 32, 16 },
                   Neon::grey, 11.0f, Justification::centred, false);

        auto out = Rectangle<int> (x + 16, y0 + 348, pw - 32, 36);
        label (g, TXT ("SAÍDA: pico ") + fmt (proc.lim.outPeakDb.load()) + " dBFS", out, Neon::yellow, 15.0f, Justification::centred);

        Colour c = Neon::lime; String t = "Livre (sem atenuar)";
        if (! on)               { c = Neon::grey;    t = "Desligado"; }
        else if (gr < -0.05f)   { c = Neon::magenta; t = "Limitando picos"; }
        auto st = Rectangle<int> (x + 16, y0 + 396, pw - 32, 56);
        box (g, st, Neon::panel);
        g.setColour (c); g.fillEllipse ((float) st.getX() + 16.0f, (float) st.getCentreY() - 7.0f, 14.0f, 14.0f);
        label (g, t, st.withTrimmedLeft (42), c, 16.0f);
        label (g, "Lookahead " + lookaheadBox.getText().upToFirstOccurrenceOf (" (", false, false), st.withTrimmedRight (16),
               Neon::grey, 13.0f, Justification::centredRight);
    }

    // ---------- saida final (barra horizontal) ----------
    {
        auto r = Rectangle<int> (M, y0 + ph + 16, W - 2 * M, 60);
        box (g, r);
        label (g, TXT ("SAÍDA FINAL"), { r.getX() + 16, r.getY(), 120, r.getHeight() });
        auto bar = Rectangle<float> ((float) r.getX() + 140.0f, (float) r.getCentreY() - 8.0f, (float) r.getWidth() - 260.0f, 16.0f);
        g.setColour (Neon::track); g.fillRoundedRectangle (bar, 4.0f);
        auto xOf = [&] (float db) { return bar.getX() + bar.getWidth() * jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f); };
        const float outRms = proc.ag.outRmsDb.load() + (proc.param ("limOn") > 0.5f ? proc.param ("ceiling") - proc.param ("threshold") + proc.lim.grDb.load() : 0.0f);
        g.setColour (Neon::lime); g.fillRect (Rectangle<float>::leftTopRightBottom (bar.getX(), bar.getY() + 3, xOf (outRms), bar.getBottom() - 3));
        g.setColour (Neon::yellow); g.fillRect (xOf (proc.lim.outPeakDb.load()) - 1.0f, bar.getY(), 2.0f, bar.getHeight());
        g.setColour (Neon::red); g.fillRect (xOf (proc.param ("ceiling")) - 1.0f, bar.getY() - 4, 2.0f, bar.getHeight() + 8);
        label (g, "teto " + fmt (proc.param ("ceiling")), { (int) bar.getRight() + 12, r.getY(), 100, r.getHeight() }, Neon::red, 13.0f);
    }
}

void AGLEditor::paintAdvanced (Graphics& g)
{
    const int y0 = CONTENT_Y, ch = H - M - y0;
    const bool master = proc.param ("active") > 0.5f;

    // ---------- 1. AUTO GAIN ----------
    {
        const auto r = Rectangle<int> (M, y0, 420, ch);
        const bool on = master && proc.param ("agOn") > 0.5f;
        box (g, r, Neon::well, on ? Neon::cyan : Neon::edge, 14.0f, 1.5f);
        label (g, TXT ("1 · AUTO GAIN"), { M + 16, y0 + 12, 200, 22 }, on ? Neon::cyan : Neon::grey, 14.0f);
        label (g, "TARGET LEVEL", { M + 16, y0 + 48, 200, 22 });
        glow (g, fmt (proc.param ("target")) + " dBFS", { M + 160, y0 + 40, 244, 40 }, Neon::cyan, bold (30.0f), Justification::centredRight);
        label (g, "-48", { M + 16, y0 + 112, 40, 16 }, Neon::grey, 11.0f, Justification::centredLeft, false);
        label (g, "-24", { M + 190, y0 + 112, 40, 16 }, Neon::grey, 11.0f, Justification::centred, false);
        label (g, "0", { M + 364, y0 + 112, 40, 16 }, Neon::grey, 11.0f, Justification::centredRight, false);

        const int bw = (420 - 32 - 10) / 2;
        auto a = Rectangle<int> (M + 16, y0 + 138, bw, 84), b = Rectangle<int> (M + 16 + bw + 10, y0 + 138, bw, 84);
        box (g, a, Neon::panel); box (g, b, Neon::panel);
        label (g, "ENTRADA RMS", a.withHeight (24).translated (0, 6), Neon::grey, 11.0f, Justification::centred);
        g.setColour (Neon::white); g.setFont (bold (24.0f));
        g.drawText (fmt (proc.ag.inRmsDb.load()), a.withY (y0 + 166).withHeight (28), Justification::centred);
        label (g, "pico " + fmt (proc.ag.inPeakDb.load()), a.withY (y0 + 194).withHeight (20), Neon::yellow, 12.0f, Justification::centred);
        label (g, "GANHO APLICADO", b.withHeight (24).translated (0, 6), Neon::grey, 11.0f, Justification::centred);
        glow (g, fmt (proc.ag.gainDb.load(), 1, true) + " dB", b.withY (y0 + 166).withHeight (28), on ? Neon::lime : Neon::grey, bold (24.0f), Justification::centred);
        label (g, TXT ("faixa −") + String (proc.param ("maxCut"), 0) + " / +" + String (proc.param ("maxBoost"), 0),
               b.withY (y0 + 194).withHeight (20), Neon::grey, 12.0f, Justification::centred, false);

        // barra bipolar do ganho
        auto bar = Rectangle<int> (M + 16, y0 + 246, 388, 14);
        const float lo = -proc.param ("maxCut"), hi = proc.param ("maxBoost");
        auto xOf = [&] (float db) { return (float) bar.getX() + (float) bar.getWidth() * (jlimit (lo, hi, db) - lo) / jmax (1.0f, hi - lo); };
        g.setColour (Colour (0xff1a1e24)); g.fillRoundedRectangle (bar.toFloat(), 7.0f);
        const float x0 = xOf (0.0f), x1 = xOf (proc.ag.gainDb.load());
        g.setColour (Neon::lime);
        g.fillRect (Rectangle<float>::leftTopRightBottom (jmin (x0, x1), (float) bar.getY(), jmax (x0, x1), (float) bar.getBottom()));
        g.setColour (Colours::white); g.fillRect (x0 - 1.0f, (float) bar.getY() - 3.0f, 2.0f, (float) bar.getHeight() + 6.0f);
        label (g, String (lo, 0), { bar.getX(), y0 + 264, 40, 16 }, Neon::grey, 11.0f, Justification::centredLeft, false);
        label (g, "0", { (int) x0 - 10, y0 + 264, 20, 16 }, Colours::white, 11.0f, Justification::centred);
        label (g, "+" + String (hi, 0), { bar.getRight() - 40, y0 + 264, 40, 16 }, Neon::grey, 11.0f, Justification::centredRight, false);

        const int gw = (420 - 32 - 10) / 2;
        const char* names[] = { "DETECÇÃO", "RESPOSTA", "GATE (SILÊNCIO)", "AUMENTO MÁXIMO", "CORTE MÁXIMO" };
        for (int i = 0; i < 5; ++i)
        {
            auto c = Rectangle<int> (M + 16 + (i % 2) * (gw + 10), y0 + 320 + (i / 2) * 74, gw, 64);
            box (g, c, Neon::control, Neon::edge, 10.0f);
            label (g, TXT (names[i]), c.withHeight (24).withTrimmedLeft (10), Neon::grey, 10.5f);
        }
        auto info = Rectangle<int> (M + 16 + gw + 10, y0 + 468, gw, 64);
        label (g, TXT ("O teto agora é do Limiter →"), info, Neon::grey, 12.0f, Justification::centred, false);
    }

    // ---------- medidores ----------
    {
        const auto r = Rectangle<int> (M + 432, y0, 140, ch);
        box (g, r);
        const int mx = r.getX() + 14;
        label (g, "IN",  { mx, y0 + 12, 24, 18 }, Neon::grey, 12.0f, Justification::centred);
        label (g, "GR",  { mx + 34, y0 + 12, 24, 18 }, Neon::grey, 12.0f, Justification::centred);
        label (g, "OUT", { mx + 66, y0 + 12, 30, 18 }, Neon::grey, 12.0f, Justification::centred);
        const int mh = 420;
        paintLevelMeter (g, { mx, y0 + 36, 24, mh }, proc.ag.inRmsDb.load(), proc.ag.inPeakDb.load(), proc.param ("gate"), Colours::white.withAlpha (0.8f));
        paintGrMeter (g, { mx + 34, y0 + 36, 24, mh }, master && proc.param ("limOn") > 0.5f ? proc.lim.grDb.load() : 0.0f);
        const float outRms = proc.ag.outRmsDb.load() + (proc.param ("limOn") > 0.5f ? proc.param ("ceiling") - proc.param ("threshold") + proc.lim.grDb.load() : 0.0f);
        paintLevelMeter (g, { mx + 68, y0 + 36, 24, mh }, outRms, proc.lim.outPeakDb.load(), proc.param ("ceiling"), Neon::yellow);
        for (int db : { 0, -12, -24, -36, -48, -60 })
        {
            const int yy = y0 + 39 + (int) ((float) (mh - 6) * (float) -db / 60.0f);
            label (g, String (db), { mx + 98, yy - 8, 30, 16 }, Neon::light, 10.0f, Justification::centredLeft, false);
        }
        label (g, "GR: 0 a -12", { r.getX() + 6, y0 + 464, r.getWidth() - 12, 16 }, Neon::magenta, 10.0f, Justification::centred, false);
        label (g, "branco: gate", { r.getX() + 6, y0 + 482, r.getWidth() - 12, 16 }, Neon::grey, 10.0f, Justification::centred, false);
        label (g, "amarelo: teto", { r.getX() + 6, y0 + 500, r.getWidth() - 12, 16 }, Neon::grey, 10.0f, Justification::centred, false);
    }

    // ---------- 2. LIMITER ----------
    {
        const int lx = M + 584, lw = W - M - lx;
        const auto r = Rectangle<int> (lx, y0, lw, ch);
        const bool on = master && proc.param ("limOn") > 0.5f;
        box (g, r, Neon::well, on ? Neon::magenta : Neon::edge, 14.0f, 1.5f);
        label (g, TXT ("2 · LIMITER"), { lx + 16, y0 + 12, 200, 22 }, on ? Neon::magenta : Neon::grey, 14.0f);
        label (g, "THRESHOLD", { lx + 6, y0 + 44, 88, 18 }, Neon::grey, 11.0f, Justification::centred);
        label (g, "CEILING", { lx + 150, y0 + 44, 88, 18 }, Neon::grey, 11.0f, Justification::centred);
        label (g, "move os dois", { lx + 84, y0 + 246, 76, 14 }, Neon::grey, 9.5f, Justification::centred, false);
        label (g, TXT ("ganho ") + fmt (proc.param ("ceiling") - proc.param ("threshold"), 1, true) + " dB",
               { lx + 6, y0 + 374, 230, 20 }, Neon::light, 12.0f, Justification::centred);

        // grafico da atenuacao
        auto gr = Rectangle<int> (lx + 248, y0 + 44, lw - 264, 330);
        label (g, TXT ("ATENUAÇÃO · 5 s"), { gr.getX(), y0 + 12, 160, 22 });
        glow (g, grText (on ? proc.lim.grDb.load() : 0.0f), { gr.getRight() - 120, y0 + 8, 120, 30 }, Neon::magenta, bold (22.0f), Justification::centredRight);
        g.setColour (Colour (0xff0a0c0f)); g.fillRoundedRectangle (gr.toFloat(), 6.0f);
        g.setColour (Neon::border);
        for (int k = 1; k < 4; ++k)
            g.drawHorizontalLine (gr.getY() + gr.getHeight() * k / 4, (float) gr.getX(), (float) gr.getRight());
        Path line, fill;
        const int n = (int) grHistory.size();
        for (int i = 0; i < n; ++i)
        {
            const float v = grHistory[(size_t) ((grHead + i) % n)];
            const float px = (float) gr.getX() + (float) gr.getWidth() * (float) i / (float) (n - 1);
            const float py = (float) gr.getY() + (float) gr.getHeight() * jlimit (0.0f, 1.0f, -v / 12.0f);
            if (i == 0) { line.startNewSubPath (px, py); fill.startNewSubPath (px, (float) gr.getY()); }
            line.lineTo (px, py);
            fill.lineTo (px, py);
        }
        fill.lineTo ((float) gr.getRight(), (float) gr.getY());
        fill.closeSubPath();
        g.setColour (Neon::magenta.withAlpha (0.18f)); g.fillPath (fill);
        g.setColour (Neon::magenta); g.strokePath (line, PathStrokeType (2.0f));
        label (g, "-3 / -6 / -9 dB", { gr.getX(), gr.getBottom() + 2, gr.getWidth(), 16 }, Neon::grey, 10.0f, Justification::centred, false);

        // controles de baixo
        const int by = y0 + 412;
        label (g, "RELEASE", { lx + 16, by, 200, 20 }, Neon::grey, 10.5f);
        label (g, "LOOKAHEAD", { lx + 16, by + 72, 200, 20 }, Neon::grey, 10.5f);
        const bool tpOn = proc.param ("truePeak") > 0.5f, la0 = (int) proc.param ("lookahead") == 0;
        label (g, tpOn && la0 ? TXT ("True Peak precisa de lookahead") : TXT ("pico entre amostras (exportação)"),
               { lx + 224, by + 134, 228, 16 }, tpOn && la0 ? Neon::yellow : Neon::grey, 10.5f, Justification::centred, false);
        const String autoTxt = proc.param ("autoRelease") > 0.5f ? TXT ("automático: 30 a 400 ms") : TXT ("manual");
        label (g, autoTxt, { lx + 224, by + 58, 228, 16 }, Neon::grey, 10.5f, Justification::centredLeft, false);
    }
}

void AGLEditor::paintAnalysis (Graphics& g)
{
    const int y0 = CONTENT_Y;
    const int state = proc.analyzer.state.load();

    // leitura
    auto top = Rectangle<int> (M, y0, W - 2 * M, 112);
    box (g, top);
    label (g, TXT ("LEITURA DO DIA · UMA LEITURA AJUSTA OS DOIS"), { M + 16, y0 + 14, 460, 18 });
    label (g, TXT ("Fale e toque normalmente, com pausas e com o trecho mais forte."), { M + 16, y0 + 36, 460, 18 }, Neon::light, 13.0f,
           Justification::centredLeft, false);
    auto bar = Rectangle<float> ((float) M + 16.0f, (float) y0 + 76.0f, (float) (W - 2 * M - 32), 10.0f);
    g.setColour (Colour (0xff1a1e24)); g.fillRoundedRectangle (bar, 5.0f);
    const float prog = state == Analyzer::done ? 1.0f : state == Analyzer::running ? proc.analyzer.progress.load() : 0.0f;
    g.setColour (Neon::lime); g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * jlimit (0.0f, 1.0f, prog)), 5.0f);
    const String stTxt = state == Analyzer::running ? "Lendo... " + String ((int) (prog * 100.0f)) + "%"
                       : state == Analyzer::done    ? TXT ("Leitura concluída") : TXT ("Pronto para ler");
    label (g, stTxt, { M + 16, y0 + 88, 300, 20 }, state == Analyzer::done ? Neon::lime : Neon::grey, 12.0f);

    // resultados
    struct Item { const char* name; String value; Colour c; const char* unit; };
    const bool v = result.valid;
    const Item items[] = {
        { "PICO",       v ? String (result.peakDb, 1) : "--",       Neon::white,   "dBFS" },
        { "TRUE PEAK",  v ? String (result.truePeakDb, 1) : "--",   Neon::yellow,  "dBTP" },
        { "RMS",        v ? String (result.rmsDb, 1) : "--",        Neon::cyan,    "dBFS" },
        { "LOUDNESS",   v ? String (result.lufs, 1) : "--",         Neon::lime,    "LUFS" },
        { "DINÂMICA",   v ? String (result.dynamicsDb(), 1) : "--", Neon::white,   "dB" },
        { "RUÍDO",      v ? String (result.noiseDb, 1) : "--",      Neon::magenta, "dBFS" } };
    const int iw = (W - 2 * M - 50) / 6;
    for (int i = 0; i < 6; ++i)
    {
        auto r = Rectangle<int> (M + i * (iw + 10), y0 + 124, iw, 92);
        box (g, r);
        label (g, TXT (items[i].name), r.withHeight (22).translated (0, 8), Neon::grey, 11.0f, Justification::centred);
        glow (g, items[i].value, r.withY (y0 + 156).withHeight (30), items[i].c, bold (24.0f), Justification::centred);
        label (g, items[i].unit, r.withY (y0 + 188).withHeight (18), Neon::grey, 11.0f, Justification::centred, false);
    }

    // sugestoes
    const auto s = makeSuggestion();
    auto sg = Rectangle<int> (M, y0 + 228, W - 2 * M, 226);
    box (g, sg);
    const bool stream = destinationBox.getSelectedId() == 2;
    label (g, TXT ("AJUSTES SUGERIDOS · DESTINO ") + (stream ? "STREAMING (-14 LUFS)" : "AO VIVO"), { M + 16, y0 + 240, 600, 18 });
    const int c0 = M + 16, c1 = M + 180, c2 = M + 440, c3 = M + 700;
    label (g, TXT ("MÓDULO"), { c0, y0 + 264, 150, 18 }, Neon::grey, 11.0f);
    label (g, TXT ("PARÂMETRO"), { c1, y0 + 264, 250, 18 }, Neon::grey, 11.0f);
    label (g, "ATUAL", { c2, y0 + 264, 250, 18 }, Neon::grey, 11.0f);
    label (g, "SUGERIDO", { c3, y0 + 264, 300, 18 }, Neon::grey, 11.0f);

    struct Row { bool ag; const char* name; String now, sug; bool change; };
    const String keep = TXT ("mantém");
    const bool tpNow = proc.param ("truePeak") > 0.5f;
    const Row rows[] = {
        { true,  "Ganho inicial",   fmt (proc.ag.gainDb.load(), 1, true) + " dB",        v ? fmt (s.agGain, 1, true) + " dB" : "--", true },
        { true,  "Gate (silêncio)", String (proc.param ("gate"), 0) + " dBFS",          v ? String (s.gate, 0) + " dBFS" : "--",  true },
        { false, "Threshold",       fmt (proc.param ("threshold")) + " dB",             v ? fmt (s.threshold) + " dB" : "--",     true },
        { false, "Ceiling",         fmt (proc.param ("ceiling")) + " dBFS",             keep,                                     false },
        { false, "Release",         proc.param ("autoRelease") > 0.5f ? String ("Auto") : String (proc.param ("release"), 0) + " ms",
                                    v ? String ("Auto") : String ("--"),                proc.param ("autoRelease") < 0.5f },
        { false, "True Peak",       tpNow ? "ON" : "OFF",                               v ? (s.truePeak ? "ON" : (tpNow ? "ON" : "OFF")) : "--", s.truePeak && ! tpNow } };
    for (int i = 0; i < 6; ++i)
    {
        const int yy = y0 + 284 + i * 24;
        label (g, rows[i].ag ? "Auto Gain" : "Limiter", { c0, yy, 150, 24 }, rows[i].ag ? Neon::cyan : Neon::magenta, 14.0f);
        label (g, TXT (rows[i].name), { c1, yy, 250, 24 }, Neon::white, 14.0f);
        label (g, rows[i].now, { c2, yy, 250, 24 }, Neon::white, 14.0f, Justification::centredLeft, false);
        if (v && rows[i].change) glow (g, rows[i].sug, { c3, yy, 300, 24 }, Neon::lime, bold (14.0f), Justification::centredLeft);
        else                     label (g, rows[i].sug, { c3, yy, 300, 24 }, Neon::light, 14.0f, Justification::centredLeft, false);
    }
    if (v)
        label (g, TXT ("Previsão: depois do Auto Gain o pico chega a ") + fmt (s.peakAfterAg, 1, true)
                  + TXT (" dBTP; o Limiter vai atenuar cerca de ") + String (s.predictedGr, 1)
                  + TXT (" dB nos trechos mais fortes; loudness final ≈ ") + String (s.lufsOut, 1) + " LUFS.",
               { M + 16, y0 + 432, W - 2 * M - 32, 18 }, Neon::grey, 12.0f, Justification::centredLeft, false);

    // alerta
    auto al = Rectangle<int> (M, y0 + 462, W - 2 * M, 40);
    String warn;
    if (v && s.predictedGr > 6.0f)
        warn = TXT ("Atenuação alta (mais de 6 dB): o som pode ficar esmagado. Abaixe o Target ou suba o Threshold.");
    else if (v && ! stream && ! tpNow && result.truePeakDb - result.peakDb > 0.3f)
        warn = TXT ("O true peak está ") + String (result.truePeakDb - result.peakDb, 1)
             + TXT (" dB acima do pico normal. Para gravar ou exportar, use Destino Streaming (liga o True Peak).");
    if (warn.isNotEmpty())
    {
        box (g, al, Colour (0xff1a1700), Neon::yellow, 12.0f);
        label (g, warn, al.reduced (16, 0), Neon::yellow, 13.0f, Justification::centredLeft, false);
    }
    else if (applied)
        label (g, TXT ("Ajustes aplicados nos dois módulos."), al.reduced (16, 0), Neon::lime, 14.0f);
}
