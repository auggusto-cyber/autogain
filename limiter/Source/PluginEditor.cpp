#include "PluginEditor.h"
#include "BinaryData.h"

using namespace juce;

// textos com acento: o arquivo e UTF-8 (no Windows compilado com /utf-8)
#define TXT(s) juce::String::fromUTF8 (s)

namespace
{
    constexpr int W = 1100, M = 24, CT = 178;   // largura, margem, topo do conteudo

    String fmt (float db, int decimals = 1, bool sign = false)
    {
        if (db <= -99.0f) return "-inf";
        if (std::abs (db) < 0.05f && decimals > 0) db = 0.0f;
        return (sign && db >= 0.05f ? "+" : "") + String (db, decimals);
    }

    String grText (float gr) { return gr > -0.05f ? String ("0.0 dB") : String (gr, 1) + " dB"; }

    float textWidth (const Font& f, const String& t)
    {
        GlyphArrangement ga;
        ga.addLineOfText (f, t, 0.0f, 0.0f);
        return ga.getBoundingBox (0, -1, true).getWidth();
    }

    void text (Graphics& g, const String& t, Rectangle<int> r, Colour c, const Font& f, Justification j = Justification::centredLeft)
    {
        g.setFont (f);
        g.setColour (c);
        g.drawText (t, r, j, false);
    }

    // rotulo pequeno em maiusculas
    void caps (Graphics& g, const String& t, Rectangle<int> r, Colour c = Pal::grey, Justification j = Justification::centredLeft, float h = 11.0f)
    {
        auto f = AGLLookAndFeel::sans (h, true).withExtraKerningFactor (0.06f);
        text (g, t, r, c, f, j);
    }

    void panel (Graphics& g, Rectangle<int> r, Colour fill = Pal::panel, float radius = 12.0f)
    {
        g.setColour (fill);
        g.fillRoundedRectangle (r.toFloat(), radius);
        g.setColour (Pal::line);
        g.drawRoundedRectangle (r.toFloat().reduced (0.5f), radius, 1.0f);
    }

    void dot (Graphics& g, float cx, float cy, Colour c, float d = 8.0f)
    {
        g.setColour (c.withAlpha (0.25f));
        g.fillEllipse (cx - d, cy - d, d * 2.0f, d * 2.0f);
        g.setColour (c);
        g.fillEllipse (cx - d * 0.5f, cy - d * 0.5f, d, d);
    }

    // valor grande + unidade pequena, centralizados juntos
    void valueUnit (Graphics& g, const String& v, const String& unit, Rectangle<int> r, const Font& vf, Colour vc,
                    Justification j = Justification::centred)
    {
        const auto uf = AGLLookAndFeel::sans (13.0f);
        const float vw = textWidth (vf, v), uw = textWidth (uf, unit), total = vw + 6.0f + uw;
        float x = (float) r.getX();
        if (j == Justification::centred)      x = (float) r.getCentreX() - total * 0.5f;
        if (j == Justification::centredRight) x = (float) r.getRight() - total;
        text (g, v, { (int) x, r.getY(), (int) vw + 4, r.getHeight() }, vc, vf);
        text (g, unit, { (int) (x + vw + 6.0f), r.getY() + 3, (int) uw + 4, r.getHeight() }, Pal::grey, uf);
    }

    Colour accentOf (const Component& c)
    {
        const auto a = c.getProperties().getWithDefault ("accent", "").toString();
        if (a == "cyan")  return Pal::cyan;
        if (a == "pink")  return Pal::pink;
        if (a == "amber") return Pal::amber;
        if (a == "green") return Pal::green;
        return Pal::white;
    }

    String kindOf (const Component& c) { return c.getProperties().getWithDefault ("kind", "").toString(); }
}

// ============================================================ LookAndFeel
AGLLookAndFeel::AGLLookAndFeel()
{
    interRegular = Typeface::createSystemTypefaceFor (BinaryData::InterRegular_otf,         (size_t) BinaryData::InterRegular_otfSize);
    interSemi    = Typeface::createSystemTypefaceFor (BinaryData::InterSemiBold_otf,        (size_t) BinaryData::InterSemiBold_otfSize);
    interBold    = Typeface::createSystemTypefaceFor (BinaryData::InterBold_otf,            (size_t) BinaryData::InterBold_otfSize);
    monoRegular  = Typeface::createSystemTypefaceFor (BinaryData::JetBrainsMonoRegular_ttf, (size_t) BinaryData::JetBrainsMonoRegular_ttfSize);
    monoBold     = Typeface::createSystemTypefaceFor (BinaryData::JetBrainsMonoBold_ttf,    (size_t) BinaryData::JetBrainsMonoBold_ttfSize);

    setColour (PopupMenu::backgroundColourId,            Pal::field);
    setColour (PopupMenu::textColourId,                  Pal::white);
    setColour (PopupMenu::highlightedBackgroundColourId, Pal::tabOn);
    setColour (PopupMenu::highlightedTextColourId,       Pal::white);
    setColour (ComboBox::textColourId,                   Pal::white);
    setColour (ComboBox::backgroundColourId,             Pal::field);
    setColour (ComboBox::outlineColourId,                Pal::line2);
    setColour (ComboBox::arrowColourId,                  Pal::white);
}

Font AGLLookAndFeel::sans (float h, bool isBold)  { return Font (FontOptions (h, isBold ? Font::bold : Font::plain)); }
Font AGLLookAndFeel::heavy (float h)              { return Font (FontOptions ("AGL Heavy", h, Font::bold)); }
Font AGLLookAndFeel::mono (float h, bool isBold)  { return Font (FontOptions ("AGL Mono", h, isBold ? Font::bold : Font::plain)); }

Typeface::Ptr AGLLookAndFeel::getTypefaceForFont (const Font& f)
{
    const auto name = f.getTypefaceName();
    if (name == "AGL Mono")  return f.isBold() ? monoBold : monoRegular;
    if (name == "AGL Heavy") return interBold;
    if (name == Font::getDefaultSansSerifFontName())
        return f.isBold() ? interSemi : interRegular;
    return LookAndFeel_V4::getTypefaceForFont (f);
}

void AGLLookAndFeel::drawButtonBackground (Graphics& g, Button& b, const Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const auto kind = kindOf (b);
    const auto acc = accentOf (b);
    const bool on = b.getToggleState();

    if (kind == "tab")
    {
        if (on) { g.setColour (Pal::tabOn); g.fillRoundedRectangle (r, 8.0f); }
        else if (over) { g.setColour (Pal::tabOn.withAlpha (0.4f)); g.fillRoundedRectangle (r, 8.0f); }
        return;
    }
    if (kind == "primary")
    {
        g.setColour (b.isEnabled() ? Pal::greenBt.brighter (over ? 0.08f : 0.0f) : Pal::greenBt.withAlpha (0.30f));
        g.fillRoundedRectangle (r, 8.0f);
        return;
    }

    const float radius = kind == "pill" ? r.getHeight() * 0.5f : 8.0f;
    if (on && (kind == "pill" || kind == "toggle"))
    {
        g.setColour (Pal::field);
        g.fillRoundedRectangle (r, radius);
        g.setColour (acc.withAlpha (0.12f));
        g.fillRoundedRectangle (r, radius);
        g.setColour (acc.withAlpha (0.55f));
        g.drawRoundedRectangle (r, radius, 1.0f);
        return;
    }
    g.setColour (Colour (0xff1a1e24).brighter (over ? 0.08f : 0.0f).darker (down ? 0.2f : 0.0f));
    g.fillRoundedRectangle (r, radius);
    g.setColour (Pal::line2);
    g.drawRoundedRectangle (r, radius, 1.0f);
}

void AGLLookAndFeel::drawButtonText (Graphics& g, TextButton& b, bool, bool)
{
    const auto kind = kindOf (b);
    const bool on = b.getToggleState();
    const auto acc = accentOf (b);

    Colour c = Pal::white;
    if (kind == "tab")     c = on ? Pal::white : Pal::grey;
    if (kind == "pill")    c = on ? acc : Pal::grey;
    if (kind == "toggle")  c = on ? acc : Pal::text;
    if (kind == "primary") c = Colour (0xff052e1f);
    if (! b.isEnabled())   c = c.withAlpha (0.45f);

    const auto f = sans (kind == "tab" ? 13.5f : 12.5f, true);
    const auto t = b.getButtonText();
    const bool hasDot = (bool) b.getProperties().getWithDefault ("dot", false);
    const float tw = textWidth (f, t);
    const float total = tw + (hasDot ? 14.0f : 0.0f);
    float x = (float) b.getWidth() * 0.5f - total * 0.5f;
    if (hasDot)
    {
        g.setColour (on ? acc : Pal::dim);
        g.fillEllipse (x, (float) b.getHeight() * 0.5f - 3.5f, 7.0f, 7.0f);
        x += 14.0f;
    }
    g.setFont (f);
    g.setColour (c);
    g.drawText (t, (int) x, 0, (int) tw + 4, b.getHeight(), Justification::centredLeft, false);
}

void AGLLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos, float start, float end, Slider& s)
{
    const auto acc = accentOf (s);

    if (kindOf (s) == "field")   // campo do Release: caixa com valor + barra embaixo
    {
        auto r = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (0.5f);
        g.setColour (Pal::field); g.fillRoundedRectangle (r, 7.0f);
        g.setColour (Pal::line2); g.drawRoundedRectangle (r, 7.0f, 1.0f);
        text (g, String ((int) std::round (s.getValue())), r.toNearestInt().withTrimmedLeft (12).withTrimmedBottom (4), Pal::white, mono (15.0f, true));
        text (g, "ms", r.toNearestInt().withTrimmedRight (12).withTrimmedBottom (4), Pal::grey, mono (11.0f), Justification::centredRight);
        auto bar = Rectangle<float> (r.getX() + 10.0f, r.getBottom() - 7.0f, r.getWidth() - 20.0f, 3.0f);
        g.setColour (Pal::track); g.fillRoundedRectangle (bar, 1.5f);
        g.setColour (acc); g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * pos), 1.5f);
        return;
    }

    // knob
    const float size = (float) jmin (w, h);
    const auto c = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).getCentre();
    const float r = size * 0.5f - 6.0f;
    const float ang = start + pos * (end - start);

    Path track;
    track.addCentredArc (c.x, c.y, r, r, 0.0f, start, end, true);
    g.setColour (Pal::track);
    g.strokePath (track, PathStrokeType (6.0f, PathStrokeType::curved, PathStrokeType::rounded));

    Path val;
    val.addCentredArc (c.x, c.y, r, r, 0.0f, start, ang, true);
    g.setColour (acc.withAlpha (0.22f));
    g.strokePath (val, PathStrokeType (12.0f, PathStrokeType::curved, PathStrokeType::rounded));
    g.setColour (acc);
    g.strokePath (val, PathStrokeType (6.0f, PathStrokeType::curved, PathStrokeType::rounded));

    const float br = r - 13.0f;
    Path body;
    body.addEllipse (c.x - br, c.y - br, br * 2, br * 2);
    DropShadow (Colours::black.withAlpha (0.7f), 10, { 0, 3 }).drawForPath (g, body);
    ColourGradient grad (Colour (0xff2c313a), c.x, c.y - br, Colour (0xff15181d), c.x, c.y + br, false);
    g.setGradientFill (grad);
    g.fillPath (body);
    g.setColour (Colour (0xff3a404a));
    g.strokePath (body, PathStrokeType (1.0f));

    const float ia = ang - MathConstants<float>::halfPi;
    Line<float> ind (c.x + std::cos (ia) * br * 0.45f, c.y + std::sin (ia) * br * 0.45f,
                     c.x + std::cos (ia) * br * 0.80f, c.y + std::sin (ia) * br * 0.80f);
    g.setColour (acc);
    g.drawLine (ind, 3.0f);
}

void AGLLookAndFeel::drawLinearSlider (Graphics& g, int x, int y, int w, int h, float pos, float minPos, float maxPos,
                                       Slider::SliderStyle style, Slider& s)
{
    const auto acc = accentOf (s);
    const auto kind = kindOf (s);

    if (style == Slider::LinearVertical)
    {
        const auto tr = Rectangle<float> ((float) x + (float) w * 0.5f - 3.0f, (float) y, 6.0f, (float) h);
        g.setColour (Pal::track); g.fillRoundedRectangle (tr, 3.0f);
        g.setColour (acc.withAlpha (0.45f));
        if (kind == "fillTop") g.fillRoundedRectangle (tr.withBottom (pos), 3.0f);
        else                   g.fillRoundedRectangle (tr.withTop (pos), 3.0f);
        auto tag = Rectangle<float> ((float) x + 2.0f, pos - 11.0f, (float) w - 4.0f, 22.0f);
        g.setColour (Pal::field); g.fillRoundedRectangle (tag, 6.0f);
        g.setColour (acc); g.drawRoundedRectangle (tag, 6.0f, 1.2f);
        text (g, String (s.getValue(), 1), tag.toNearestInt(), Pal::white, mono (12.0f, true), Justification::centred);
        return;
    }
    if (kind == "line")   // Target: linha fina e bolinha branca
    {
        const float cy = (float) y + (float) h * 0.5f;
        g.setColour (Pal::line2); g.fillRect ((float) x, cy - 1.0f, (float) w, 2.0f);
        g.setColour (Pal::white); g.fillEllipse (pos - 8.0f, cy - 8.0f, 16.0f, 16.0f);
        g.setColour (acc); g.drawEllipse (pos - 8.0f, cy - 8.0f, 16.0f, 16.0f, 2.0f);
        return;
    }
    if (kind == "thin")   // barras de Gate / Aumento / Corte
    {
        const float cy = (float) y + (float) h * 0.5f;
        auto tr = Rectangle<float> ((float) x, cy - 2.0f, (float) w, 4.0f);
        g.setColour (Pal::track); g.fillRoundedRectangle (tr, 2.0f);
        g.setColour (acc); g.fillRoundedRectangle (tr.withRight (pos), 2.0f);
        return;
    }
    LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, pos, minPos, maxPos, style, s);
}

void AGLLookAndFeel::drawComboBox (Graphics& g, int w, int h, bool, int, int, int, int, ComboBox&)
{
    auto r = Rectangle<float> (0.0f, 0.0f, (float) w, (float) h).reduced (0.5f);
    g.setColour (Pal::field); g.fillRoundedRectangle (r, 7.0f);
    g.setColour (Pal::line2); g.drawRoundedRectangle (r, 7.0f, 1.0f);
    Path ch;
    const float cx = (float) w - 16.0f, cy = (float) h * 0.5f;
    ch.startNewSubPath (cx - 4.5f, cy - 2.0f); ch.lineTo (cx, cy + 2.5f); ch.lineTo (cx + 4.5f, cy - 2.0f);
    g.setColour (Pal::white);
    g.strokePath (ch, PathStrokeType (1.6f, PathStrokeType::curved, PathStrokeType::rounded));
}

Font AGLLookAndFeel::getComboBoxFont (ComboBox&)  { return sans (13.0f); }
Font AGLLookAndFeel::getPopupMenuFont()           { return sans (13.0f); }
void AGLLookAndFeel::positionComboBoxText (ComboBox& b, Label& l)
{
    l.setBounds (8, 1, b.getWidth() - 30, b.getHeight() - 2);
    l.setFont (getComboBoxFont (b));
}

// ============================================================ Editor
AGLEditor::AGLEditor (AGLProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    ++openEditors;
    LookAndFeel::setDefaultLookAndFeel (&lnf.get());
    setLookAndFeel (&lnf.get());
    auto& st = proc.apvts;

    auto setup = [this] (Component& c, const char* kind, const char* accent, bool visible = true)
    {
        c.getProperties().set ("kind", kind);
        c.getProperties().set ("accent", accent);
        if (visible) addAndMakeVisible (c); else addChildComponent (c);
    };

    // ---- cabecalho ----
    for (auto* b : { &agPill, &limPill, &simButton, &powerButton })
        b->getProperties().set ("dot", true);
    setup (agPill, "pill", "cyan");
    setup (limPill, "pill", "pink");
    setup (simButton, "toggle", "amber");
    setup (holdButton, "toggle", "amber");
    setup (powerButton, "toggle", "green");
    for (auto* b : { &agPill, &limPill, &holdButton, &powerButton, &simButton })
        b->setClickingTogglesState (true);
    simButton.setToggleState (proc.simulate.load(), dontSendNotification);
    simButton.onClick = [this] { proc.simulate.store (simButton.getToggleState()); };
    agAtt    = std::make_unique<ButtonAtt> (st, "agOn", agPill);
    limAtt   = std::make_unique<ButtonAtt> (st, "limOn", limPill);
    holdAtt  = std::make_unique<ButtonAtt> (st, "hold", holdButton);
    powerAtt = std::make_unique<ButtonAtt> (st, "active", powerButton);

    const char* names[] = { "Minimalista", "Avancado", "Analise" };
    for (int i = 0; i < 3; ++i)
    {
        auto& b = tabButtons[i];
        b.setButtonText (TXT (names[i]));
        b.setRadioGroupId (1);
        b.setClickingTogglesState (true);
        b.onClick = [this, i] { if (tabButtons[i].getToggleState()) setTab (i); };
        setup (b, "tab", "");
    }

    // ---- Minimalista: knobs ----
    auto knob = [&setup, &st] (Slider& s, const char* id, const char* accent, double def, std::unique_ptr<SliderAtt>& att)
    {
        s.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
        s.setRotaryParameters (MathConstants<float>::pi * 1.25f, MathConstants<float>::pi * 2.75f, true);
        setup (s, "knob", accent, false);
        att = std::make_unique<SliderAtt> (st, id, s);
        s.setDoubleClickReturnValue (true, def);
    };
    knob (targetKnob,    "target",    "cyan",  -18.0, targetKnobAtt);
    knob (thresholdKnob, "threshold", "pink",  -1.0,  thresholdKnobAtt);
    knob (ceilingKnob,   "ceiling",   "amber", -1.0,  ceilingKnobAtt);

    // ---- Avancado: Auto Gain ----
    targetSlider.setSliderStyle (Slider::LinearHorizontal);
    targetSlider.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
    setup (targetSlider, "line", "cyan", false);
    targetAtt = std::make_unique<SliderAtt> (st, "target", targetSlider);
    targetSlider.setDoubleClickReturnValue (true, -18.0);

    auto thin = [&setup, &st] (Slider& s, const char* id, const char* accent, double def, std::unique_ptr<SliderAtt>& att)
    {
        s.setSliderStyle (Slider::LinearHorizontal);
        s.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
        setup (s, "thin", accent, false);
        att = std::make_unique<SliderAtt> (st, id, s);
        s.setDoubleClickReturnValue (true, def);
    };
    thin (gateSlider,  "gate",     "cyan",  -50.0, gateAtt);
    thin (boostSlider, "maxBoost", "green",  12.0, boostAtt);
    thin (cutSlider,   "maxCut",   "cyan",   24.0, cutAtt);

    detectionBox.addItemList ({ "Pico", "RMS", "Pico + RMS" }, 1);
    responseBox.addItemList ({ "Lenta (ao vivo)", TXT ("Media"), TXT ("Rapida") }, 1);
    addChildComponent (detectionBox);
    addChildComponent (responseBox);
    detectionAtt = std::make_unique<ComboAtt> (st, "detection", detectionBox);
    responseAtt  = std::make_unique<ComboAtt> (st, "response",  responseBox);

    // ---- Avancado: Limiter ----
    auto vert = [&setup, &st] (Slider& s, const char* id, const char* kind, const char* accent, double def, std::unique_ptr<SliderAtt>& att)
    {
        s.setSliderStyle (Slider::LinearVertical);
        s.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
        setup (s, kind, accent, false);
        att = std::make_unique<SliderAtt> (st, id, s);
        s.setDoubleClickReturnValue (true, def);
    };
    vert (thresholdSlider, "threshold", "fillTop",    "pink",  -1.0, thresholdAtt);
    vert (ceilingSlider,   "ceiling",   "fillBottom", "amber", -1.0, ceilingAtt);

    // LINK: mexer no Threshold move o Ceiling junto (vale para o slider e para o knob)
    lastThreshold = proc.param ("threshold");
    auto onThreshold = [this] (Slider& s)
    {
        const float now = (float) s.getValue();
        if (proc.param ("link") > 0.5f && std::abs (now - lastThreshold) > 1.0e-4f)
            setParam ("ceiling", jlimit (-12.0f, 0.0f, proc.param ("ceiling") + (now - lastThreshold)));
        lastThreshold = now;
    };
    thresholdSlider.onValueChange = [this, onThreshold] { onThreshold (thresholdSlider); };
    thresholdKnob.onValueChange   = [this, onThreshold] { onThreshold (thresholdKnob); };

    releaseField.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    releaseField.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
    setup (releaseField, "field", "pink", false);
    releaseAtt = std::make_unique<SliderAtt> (st, "release", releaseField);
    releaseField.setDoubleClickReturnValue (true, 100.0);

    setup (linkButton, "toggle", "cyan", false);
    setup (autoRelButton, "toggle", "green", false);
    setup (truePeakButton, "toggle", "amber", false);
    for (auto* b : { &linkButton, &autoRelButton, &truePeakButton })
        b->setClickingTogglesState (true);
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

    setup (startButton, "", "", false);
    setup (applyButton, "primary", "", false);
    setup (discardButton, "", "", false);
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

int AGLEditor::heightForTab (int tab) const { return tab == 2 ? 826 : 734; }

void AGLEditor::setTab (int tab)
{
    currentTab = tab;
    proc.uiTab.store (tab);
    tabButtons[tab].setToggleState (true, dontSendNotification);

    const bool mini = tab == 0, adv = tab == 1, ana = tab == 2;
    for (auto* c : std::initializer_list<Component*> { &targetKnob, &thresholdKnob, &ceilingKnob })
        c->setVisible (mini);
    for (auto* c : std::initializer_list<Component*> { &targetSlider, &gateSlider, &boostSlider, &cutSlider, &detectionBox, &responseBox,
                                                       &thresholdSlider, &ceilingSlider, &releaseField, &linkButton, &autoRelButton,
                                                       &truePeakButton, &lookaheadBox })
        c->setVisible (adv);
    for (auto* c : std::initializer_list<Component*> { &destinationBox, &durationBox, &startButton, &applyButton, &discardButton })
        c->setVisible (ana);

    if (getHeight() != heightForTab (tab) || getWidth() != W)
        setSize (W, heightForTab (tab));
    else
        resized();
    repaint();
}

AGLEditor::Suggestion AGLEditor::makeSuggestion() const
{
    Suggestion s;
    if (! result.valid) return s;

    // mesma regra do motor: Pico, RMS ou Pico + RMS (o menor dos dois)
    const bool agOn = proc.param ("agOn") > 0.5f;
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
        s.threshold = ceiling;

    s.predictedGr = jmax (0.0f, s.peakAfterAg + limGain - ceiling);
    s.lufsOut = lufsAfterAg + limGain;
    return s;
}

void AGLEditor::timerCallback()
{
    grHistory[(size_t) grHead] = proc.lim.grPeakSinceRead.exchange (0.0f);
    grHead = (grHead + 1) % (int) grHistory.size();

    const int state = proc.analyzer.state.load();
    if (state == Analyzer::done && ! result.valid)
        result = proc.analyzer.result();

    startButton.setButtonText (state == Analyzer::running ? "Cancelar"
                               : state == Analyzer::done  ? "Ler novamente" : "Iniciar leitura");
    applyButton.setEnabled (result.valid && ! applied);

    agPill.setButtonText  (String ("AUTO GAIN ") + (agPill.getToggleState()  ? "ON" : "OFF"));
    limPill.setButtonText (String ("LIMITER ")   + (limPill.getToggleState() ? "ON" : "OFF"));
    powerButton.setButtonText (powerButton.getToggleState() ? "ON" : "OFF");
    simButton.setToggleState (proc.simulate.load(), dontSendNotification);
    simButton.setButtonText (simButton.getToggleState() ? TXT ("Simulando...") : TXT ("Simular Audio"));
    autoRelButton.setButtonText (autoRelButton.getToggleState() ? "AUTO" : "MANUAL");
    truePeakButton.setButtonText (String ("TRUE PEAK ") + (truePeakButton.getToggleState() ? "ON" : "OFF"));
    repaint();
}

// ============================================================ Layout
void AGLEditor::resized()
{
    const int capX = W - M - 400;
    agPill.setBounds  (capX + 90, 28, 116, 28);
    limPill.setBounds (capX + 232, 28, 104, 28);

    simButton.setBounds   (186, 76, 124, 30);
    holdButton.setBounds  (318, 76, 60, 30);
    powerButton.setBounds (386, 76, 62, 30);

    const int tw = (W - 2 * M - 8) / 3;
    for (int i = 0; i < 3; ++i)
        tabButtons[i].setBounds (M + 4 + i * tw, 124, tw, 36);

    // Minimalista
    const int y0 = CT;
    targetKnob.setBounds    (M + 259 - 66, y0 + 84, 132, 132);
    const int lx = 558;
    thresholdKnob.setBounds (lx + 130 - 60, y0 + 84, 120, 120);
    ceilingKnob.setBounds   (lx + 388 - 60, y0 + 84, 120, 120);

    // Avancado - Auto Gain (x 24, largura 436)
    targetSlider.setBounds (M + 20, y0 + 72, 396, 24);
    detectionBox.setBounds (M + 20, y0 + 278, 192, 32);
    responseBox.setBounds  (M + 224, y0 + 278, 192, 32);
    gateSlider.setBounds   (M + 32, y0 + 352, 168, 14);
    boostSlider.setBounds  (M + 236, y0 + 352, 168, 14);
    cutSlider.setBounds    (M + 32, y0 + 418, 168, 14);

    // Avancado - Limiter (x 644, largura 432)
    const int L = 644;
    thresholdSlider.setBounds (L + 24, y0 + 74, 72, 186);
    linkButton.setBounds      (L + 108, y0 + 140, 52, 30);
    ceilingSlider.setBounds   (L + 172, y0 + 74, 72, 186);
    releaseField.setBounds    (L + 20, y0 + 330, 180, 40);
    autoRelButton.setBounds   (L + 214, y0 + 312, 198, 38);
    lookaheadBox.setBounds    (L + 20, y0 + 406, 180, 34);
    truePeakButton.setBounds  (L + 214, y0 + 386, 198, 38);

    // Analise
    destinationBox.setBounds (W - M - 18 - 118 - 8 - 70 - 8 - 150, y0 + 18, 150, 30);
    durationBox.setBounds    (W - M - 18 - 118 - 8 - 70, y0 + 18, 70, 30);
    startButton.setBounds    (W - M - 18 - 118, y0 + 18, 118, 30);
    applyButton.setBounds    (M, 702, 532, 40);
    discardButton.setBounds  (M + 544, 702, W - 2 * M - 544, 40);
}

// ============================================================ Desenho
void AGLEditor::paint (Graphics& g)
{
    g.fillAll (Pal::bg);
    paintHeader (g);
    if (currentTab == 0)      paintMinimal (g);
    else if (currentTab == 1) paintAdvanced (g);
    else                      paintAnalysis (g);
    paintOutputBar (g);
}

void AGLEditor::paintHeader (Graphics& g)
{
    // logo
    auto lg = Rectangle<float> ((float) M, 22.0f, 38.0f, 38.0f);
    g.setColour (Colour (0xff181c22)); g.fillRoundedRectangle (lg, 8.0f);
    g.setColour (Pal::line2); g.drawRoundedRectangle (lg, 8.0f, 1.0f);
    Path w;
    const float bx = lg.getX() + 7.0f, by = lg.getCentreY() + 2.0f;
    w.startNewSubPath (bx, by); w.lineTo (bx + 6, by); w.lineTo (bx + 9, by - 7); w.lineTo (bx + 13, by + 6);
    w.lineTo (bx + 16, by - 3); w.lineTo (bx + 18, by); w.lineTo (bx + 24, by);
    g.setColour (Pal::cyan);
    g.strokePath (w, PathStrokeType (2.0f, PathStrokeType::curved, PathStrokeType::rounded));
    g.setColour (Pal::pink);
    for (float x = bx; x < bx + 24.0f; x += 5.0f) g.fillRect (x, lg.getY() + 9.0f, 2.5f, 1.5f);

    // titulo
    const auto tf = AGLLookAndFeel::heavy (19.0f);
    float x = (float) M + 52.0f;
    const String a = "AUTO GAIN", b = "LIMITER";
    text (g, a, { (int) x, 20, 200, 26 }, Pal::white, tf); x += textWidth (tf, a) + 7.0f;
    text (g, "+", { (int) x, 20, 20, 26 }, Pal::pink, tf); x += textWidth (tf, "+") + 7.0f;
    text (g, b, { (int) x, 20, 200, 26 }, Pal::white, tf);
    text (g, TXT ("Intelligent Level Control  •  by Augusto Souza"), { M + 52, 46, 400, 16 }, Pal::grey, AGLLookAndFeel::sans (12.0f));

    // cadeia (capsula)
    const int capX = W - M - 400;
    auto cap = Rectangle<float> ((float) capX, 22.0f, 400.0f, 40.0f);
    g.setColour (Pal::panel); g.fillRoundedRectangle (cap, 20.0f);
    g.setColour (Pal::line); g.drawRoundedRectangle (cap, 20.0f, 1.0f);
    const auto sf = AGLLookAndFeel::sans (12.0f);
    text (g, "Entrada", { capX + 16, 22, 60, 40 }, Pal::grey, sf);
    text (g, TXT ("→"), { capX + 68, 22, 20, 40 }, Pal::dim, sf, Justification::centred);
    text (g, TXT ("→"), { capX + 209, 22, 20, 40 }, Pal::dim, sf, Justification::centred);
    text (g, TXT ("→"), { capX + 339, 22, 20, 40 }, Pal::dim, sf, Justification::centred);
    text (g, TXT ("Saida"), { capX + 360, 22, 40, 40 }, Pal::grey, sf);

    // latencia
    const int lat = proc.getLatencySamples();
    const double sr = proc.getSampleRate() > 0.0 ? proc.getSampleRate() : 48000.0;
    const auto lf = AGLLookAndFeel::sans (12.5f), lb = AGLLookAndFeel::sans (12.5f, true);
    const String l1 = TXT ("Latencia"), l2 = String (lat * 1000.0 / sr, 2) + " ms", l3 = "(" + String (lat) + " smp)";
    float lx = (float) M;
    text (g, l1, { (int) lx, 76, 80, 30 }, Pal::grey, lf); lx += textWidth (lf, l1) + 6.0f;
    text (g, l2, { (int) lx, 76, 80, 30 }, Pal::white, lb); lx += textWidth (lb, l2) + 6.0f;
    text (g, l3, { (int) lx, 76, 80, 30 }, Pal::dim, lf);

    // abas (fundo)
    auto tabs = Rectangle<float> ((float) M, 120.0f, (float) (W - 2 * M), 44.0f);
    g.setColour (Colour (0xff101317)); g.fillRoundedRectangle (tabs, 10.0f);
    g.setColour (Pal::line); g.drawRoundedRectangle (tabs, 10.0f, 1.0f);
}

void AGLEditor::paintStatus (Graphics& g, Rectangle<int> r, Colour c, const String& t, const String& right)
{
    g.setColour (Pal::inner); g.fillRoundedRectangle (r.toFloat(), 8.0f);
    dot (g, (float) r.getX() + 20.0f, (float) r.getCentreY(), c, 7.0f);
    text (g, t, r.withTrimmedLeft (34), Pal::white, AGLLookAndFeel::sans (13.0f, true));
    text (g, right, r.withTrimmedRight (14), Pal::grey, AGLLookAndFeel::mono (11.5f), Justification::centredRight);
}

void AGLEditor::paintMinimal (Graphics& g)
{
    const int y0 = CT, ph = 470;
    const bool master = proc.param ("active") > 0.5f;

    // ---------- 1. AUTO GAIN ----------
    {
        const int x0 = M, pw = 518;
        const bool on = master && proc.param ("agOn") > 0.5f;
        panel (g, { x0, y0, pw, ph });
        dot (g, (float) x0 + 26.0f, (float) y0 + 30.0f, on ? Pal::cyan : Pal::dim, 7.0f);
        text (g, TXT ("1 · AUTO GAIN"), { x0 + 38, y0 + 18, 200, 24 }, on ? Pal::cyan : Pal::grey, AGLLookAndFeel::sans (13.5f, true));
        text (g, "Arraste o knob ou use o scroll", { x0, y0 + 18, pw - 20, 24 }, Pal::dim, AGLLookAndFeel::sans (11.5f), Justification::centredRight);
        caps (g, "TARGET LEVEL", { x0, y0 + 56, pw, 18 }, Pal::text, Justification::centred);
        valueUnit (g, fmt (proc.param ("target")), "dBFS", { x0, y0 + 226, pw, 40 }, AGLLookAndFeel::heavy (34.0f), Pal::white);

        const int bw = (pw - 40 - 14) / 2;
        auto a = Rectangle<int> (x0 + 20, y0 + 290, bw, 100), b = Rectangle<int> (x0 + 20 + bw + 14, y0 + 290, bw, 100);
        for (auto r : { a, b }) { g.setColour (Pal::inner); g.fillRoundedRectangle (r.toFloat(), 10.0f); }
        caps (g, "ENTRADA", a.withHeight (20).translated (0, 14), Pal::text, Justification::centred);
        text (g, fmt (proc.ag.inRmsDb.load()) + " RMS", a.withY (a.getY() + 36).withHeight (32), Pal::white, AGLLookAndFeel::heavy (24.0f), Justification::centred);
        text (g, "pico " + fmt (proc.ag.inPeakDb.load()) + " dBFS", a.withY (a.getY() + 70).withHeight (18), Pal::grey, AGLLookAndFeel::sans (12.0f), Justification::centred);
        caps (g, "GANHO", b.withHeight (20).translated (0, 14), Pal::text, Justification::centred);
        text (g, fmt (proc.ag.gainDb.load(), 1, true) + " dB", b.withY (b.getY() + 36).withHeight (32), on ? Pal::green : Pal::grey, AGLLookAndFeel::heavy (24.0f), Justification::centred);
        text (g, detectionBox.getText(), b.withY (b.getY() + 70).withHeight (18), Pal::grey, AGLLookAndFeel::sans (12.0f), Justification::centred);

        Colour c = Pal::green; String t = TXT ("Automatico ativo");
        if (! on)                               { c = Pal::dim;   t = "Desligado"; }
        else if (proc.param ("hold") > 0.5f)    { c = Pal::amber; t = "HOLD: ganho congelado"; }
        else if (proc.ag.gateClosed.load())     { c = Pal::cyan;  t = TXT ("Congelado (silencio)"); }
        paintStatus (g, { x0 + 20, y0 + 410, pw - 40, 42 }, c, t, "0 ms");
    }

    // ---------- 2. LIMITER ----------
    {
        const int x0 = 558, pw = 518;
        const bool on = master && proc.param ("limOn") > 0.5f;
        panel (g, { x0, y0, pw, ph });
        dot (g, (float) x0 + 26.0f, (float) y0 + 30.0f, on ? Pal::pink : Pal::dim, 7.0f);
        text (g, TXT ("2 · LIMITER"), { x0 + 38, y0 + 18, 200, 24 }, on ? Pal::pink : Pal::grey, AGLLookAndFeel::sans (13.5f, true));
        text (g, "Duplo clique para resetar", { x0, y0 + 18, pw - 20, 24 }, Pal::dim, AGLLookAndFeel::sans (11.5f), Justification::centredRight);
        caps (g, "THRESHOLD", { x0 + 130 - 80, y0 + 56, 160, 18 }, Pal::text, Justification::centred);
        caps (g, "CEILING",   { x0 + 388 - 80, y0 + 56, 160, 18 }, Pal::text, Justification::centred);
        valueUnit (g, fmt (proc.param ("threshold")), "dB",  { x0 + 130 - 100, y0 + 212, 200, 36 }, AGLLookAndFeel::heavy (26.0f), Pal::white);
        valueUnit (g, fmt (proc.param ("ceiling")),  "dBFS", { x0 + 388 - 100, y0 + 212, 200, 36 }, AGLLookAndFeel::heavy (26.0f), Pal::white);

        const float gr = on ? proc.lim.grDb.load() : 0.0f;
        auto at = Rectangle<int> (x0 + 20, y0 + 268, pw - 40, 126);
        g.setColour (Pal::inner); g.fillRoundedRectangle (at.toFloat(), 10.0f);
        caps (g, TXT ("ATENUACAO"), { at.getX() + 16, at.getY() + 14, 200, 18 }, Pal::text);
        text (g, grText (gr), { at.getRight() - 216, at.getY() + 12, 200, 22 }, Pal::amber, AGLLookAndFeel::mono (15.0f, true), Justification::centredRight);
        auto bar = Rectangle<float> ((float) at.getX() + 16.0f, (float) at.getY() + 46.0f, (float) at.getWidth() - 32.0f, 8.0f);
        g.setColour (Pal::track); g.fillRoundedRectangle (bar, 4.0f);
        const float frac = jlimit (0.0f, 1.0f, -gr / 12.0f);
        g.setColour (Pal::amber); g.fillRoundedRectangle (bar.withLeft (bar.getRight() - bar.getWidth() * frac), 4.0f);
        const char* ticks[] = { "-12", "-9", "-6", "-3", "0" };
        for (int i = 0; i < 5; ++i)
        {
            const int tx = (int) (bar.getX() + bar.getWidth() * (float) i / 4.0f);
            const auto j = i == 0 ? Justification::centredLeft : i == 4 ? Justification::centredRight : Justification::centred;
            text (g, ticks[i], { i == 0 ? tx : i == 4 ? tx - 30 : tx - 15, (int) bar.getBottom() + 6, 30, 14 }, Pal::grey, AGLLookAndFeel::mono (10.0f), j);
        }
        const auto sf = AGLLookAndFeel::sans (12.5f), sb = AGLLookAndFeel::sans (12.5f, true);
        const String s1 = TXT ("SAIDA: "), s2 = "pico " + fmt (proc.lim.outPeakDb.load()) + " dBFS";
        const float tot = textWidth (sf, s1) + textWidth (sb, s2);
        const int sx = at.getCentreX() - (int) (tot * 0.5f);
        text (g, s1, { sx, at.getY() + 92, 80, 20 }, Pal::grey, sf);
        text (g, s2, { sx + (int) textWidth (sf, s1), at.getY() + 92, 200, 20 }, Pal::white, sb);

        Colour c = Pal::green; String t = "Livre (sem atenuar)";
        if (! on)              { c = Pal::dim;  t = "Desligado"; }
        else if (gr < -0.05f)  { c = Pal::pink; t = "Limitando picos"; }
        paintStatus (g, { x0 + 20, y0 + 410, pw - 40, 42 }, c, t,
                     "Lookahead " + lookaheadBox.getText().upToFirstOccurrenceOf (" (", false, false));
    }
}

void AGLEditor::paintVMeter (Graphics& g, Rectangle<int> bar, float levelDb, float peakDb, float markDb, Colour mark)
{
    g.setColour (Pal::track.darker (0.3f));
    g.fillRoundedRectangle (bar.toFloat(), 3.0f);
    auto in = bar.toFloat().reduced (2.0f);
    auto yOf = [&] (float db) { return in.getY() + in.getHeight() * jlimit (0.0f, 1.0f, -db / 60.0f); };
    const float top = yOf (levelDb);
    auto zone = [&] (float fromDb, float toDb, Colour c)
    {
        const float yA = jmax (top, yOf (toDb)), yB = yOf (fromDb);
        if (yB > yA) { g.setColour (c); g.fillRect (in.getX(), yA, in.getWidth(), yB - yA); }
    };
    zone (-60.0f, -12.0f, Pal::green);
    zone (-12.0f, -6.0f, Pal::amber);
    zone (-6.0f, 0.0f, Pal::red);
    if (peakDb > -59.0f) { g.setColour (Pal::white.withAlpha (0.8f)); g.fillRect (in.getX(), yOf (peakDb) - 1.0f, in.getWidth(), 2.0f); }
    if (markDb > -99.0f)
    {
        g.setColour (mark);
        g.fillRect ((float) bar.getX() - 1.0f, yOf (markDb) - 1.0f, (float) bar.getWidth() + 2.0f, 2.0f);
    }
}

void AGLEditor::paintAdvanced (Graphics& g)
{
    const int y0 = CT, ph = 470;
    const bool master = proc.param ("active") > 0.5f;

    // ---------- 1. AUTO GAIN ----------
    {
        const int x0 = M, pw = 436;
        const bool on = master && proc.param ("agOn") > 0.5f;
        panel (g, { x0, y0, pw, ph });
        dot (g, (float) x0 + 26.0f, (float) y0 + 30.0f, on ? Pal::cyan : Pal::dim, 7.0f);
        text (g, TXT ("1 · AUTO GAIN"), { x0 + 38, y0 + 18, 200, 24 }, on ? Pal::cyan : Pal::grey, AGLLookAndFeel::sans (13.5f, true));
        text (g, fmt (proc.param ("target")) + " dBFS", { x0, y0 + 14, pw - 20, 32 }, Pal::white, AGLLookAndFeel::mono (19.0f, true), Justification::centredRight);
        caps (g, "TARGET LEVEL", { x0 + 20, y0 + 52, 200, 18 }, Pal::text);
        const auto mf = AGLLookAndFeel::mono (10.5f);
        text (g, "-48", { x0 + 20, y0 + 98, 40, 14 }, Pal::grey, mf);
        text (g, "-24", { x0 + 20 + 198 - 20, y0 + 98, 40, 14 }, Pal::grey, mf, Justification::centred);
        text (g, "0", { x0 + 416 - 40, y0 + 98, 40, 14 }, Pal::grey, mf, Justification::centredRight);

        const int bw = 192;
        auto a = Rectangle<int> (x0 + 20, y0 + 122, bw, 82), b = Rectangle<int> (x0 + 224, y0 + 122, bw, 82);
        for (auto r : { a, b }) { g.setColour (Pal::inner); g.fillRoundedRectangle (r.toFloat(), 10.0f); }
        caps (g, "ENTRADA RMS", a.withHeight (18).translated (0, 12), Pal::text, Justification::centred);
        text (g, fmt (proc.ag.inRmsDb.load()), a.withY (a.getY() + 32).withHeight (28), Pal::white, AGLLookAndFeel::mono (20.0f, true), Justification::centred);
        text (g, "pico " + fmt (proc.ag.inPeakDb.load()), a.withY (a.getY() + 60).withHeight (14), Pal::grey, AGLLookAndFeel::mono (10.5f), Justification::centred);
        caps (g, "GANHO APLICADO", b.withHeight (18).translated (0, 12), Pal::text, Justification::centred);
        text (g, fmt (proc.ag.gainDb.load(), 1, true) + " dB", b.withY (b.getY() + 32).withHeight (28), on ? Pal::green : Pal::grey, AGLLookAndFeel::mono (20.0f, true), Justification::centred);
        text (g, "faixa -" + String (proc.param ("maxCut"), 0) + " / +" + String (proc.param ("maxBoost"), 0),
              b.withY (b.getY() + 60).withHeight (14), Pal::grey, AGLLookAndFeel::mono (10.5f), Justification::centred);

        // barra bipolar
        auto bar = Rectangle<float> ((float) x0 + 20.0f, (float) y0 + 220.0f, 396.0f, 8.0f);
        const float lo = -proc.param ("maxCut"), hi = proc.param ("maxBoost");
        auto xOf = [&] (float db) { return bar.getX() + bar.getWidth() * (jlimit (lo, hi, db) - lo) / jmax (1.0f, hi - lo); };
        g.setColour (Pal::track); g.fillRoundedRectangle (bar, 4.0f);
        const float xz = xOf (0.0f), xg = xOf (proc.ag.gainDb.load());
        g.setColour (on ? Pal::green : Pal::dim);
        g.fillRoundedRectangle (Rectangle<float>::leftTopRightBottom (jmin (xz, xg), bar.getY(), jmax (xz, xg) + 0.5f, bar.getBottom()), 2.0f);
        text (g, String (lo, 0), { x0 + 20, y0 + 234, 40, 14 }, Pal::grey, mf);
        text (g, "0", { (int) xz - 20, y0 + 234, 40, 14 }, Pal::grey, mf, Justification::centred);
        text (g, "+" + String (hi, 0), { x0 + 416 - 40, y0 + 234, 40, 14 }, Pal::grey, mf, Justification::centredRight);

        caps (g, TXT ("DETECCAO"), { x0 + 20, y0 + 258, 190, 18 }, Pal::text);
        caps (g, "RESPOSTA", { x0 + 224, y0 + 258, 190, 18 }, Pal::text);

        struct Cell { Rectangle<int> r; const char* name; String value; const char* unit; };
        const Cell cells[] = {
            { { x0 + 20,  y0 + 324, 192, 54 }, "GATE (SILENCIO)", String (proc.param ("gate"), 0),     "dBFS" },
            { { x0 + 224, y0 + 324, 192, 54 }, "AUMENTO MAXIMO",  String (proc.param ("maxBoost"), 0), "dB" },
            { { x0 + 20,  y0 + 390, 192, 54 }, "CORTE MAXIMO",    String (proc.param ("maxCut"), 0),   "dB" } };
        for (auto& c : cells)
        {
            g.setColour (Pal::inner); g.fillRoundedRectangle (c.r.toFloat(), 9.0f);
            caps (g, TXT (c.name), { c.r.getX() + 12, c.r.getY() + 8, 140, 18 }, Pal::text, Justification::centredLeft, 10.0f);
            const auto vf = AGLLookAndFeel::mono (13.0f, true), uf = AGLLookAndFeel::mono (11.0f);
            const float uw = textWidth (uf, c.unit);
            text (g, c.unit, { c.r.getRight() - 12 - (int) uw - 2, c.r.getY() + 8, (int) uw + 4, 18 }, Pal::grey, uf);
            text (g, c.value, { c.r.getRight() - 12 - (int) uw - 80, c.r.getY() + 8, 74, 18 }, Pal::white, vf, Justification::centredRight);
        }
        text (g, TXT ("O teto agora e do Limiter →"), { x0 + 224, y0 + 390, 192, 54 }, Pal::grey, AGLLookAndFeel::sans (12.0f), Justification::centred);
    }

    // ---------- medidores ----------
    {
        const int x0 = 472, pw = 160;
        panel (g, { x0, y0, pw, ph });
        const int bx[3] = { x0 + 34, x0 + 62, x0 + 90 };
        caps (g, "IN",  { bx[0] - 14, y0 + 20, 40, 16 }, Pal::text, Justification::centred);
        caps (g, "GR",  { bx[1] - 14, y0 + 20, 40, 16 }, Pal::amber, Justification::centred);
        caps (g, "OUT", { bx[2] - 14, y0 + 20, 40, 16 }, Pal::text, Justification::centred);
        const int my = y0 + 88, mh = 258;
        paintVMeter (g, { bx[0], my, 12, mh }, proc.ag.inRmsDb.load(), proc.ag.inPeakDb.load(), proc.param ("gate"), Pal::white);
        {   // GR: de cima para baixo, 0 a -12
            auto r = Rectangle<float> ((float) bx[1], (float) my, 12.0f, (float) mh);
            g.setColour (Pal::track.darker (0.3f)); g.fillRoundedRectangle (r, 3.0f);
            const float gr = master && proc.param ("limOn") > 0.5f ? proc.lim.grDb.load() : 0.0f;
            g.setColour (Pal::amber);
            g.fillRoundedRectangle (r.reduced (2.0f).withHeight ((r.getHeight() - 4.0f) * jlimit (0.0f, 1.0f, -gr / 12.0f)), 2.0f);
        }
        const float outRms = proc.ag.outRmsDb.load() + (proc.param ("limOn") > 0.5f ? proc.param ("ceiling") - proc.param ("threshold") + proc.lim.grDb.load() : 0.0f);
        paintVMeter (g, { bx[2], my, 12, mh }, outRms, proc.lim.outPeakDb.load(), proc.param ("ceiling"), Pal::amber);
        for (int db : { 0, -12, -24, -36, -48, -60 })
        {
            const int yy = my + 2 + (int) ((float) (mh - 4) * (float) -db / 60.0f);
            text (g, String (db), { x0 + 112, yy - 7, 40, 14 }, Pal::grey, AGLLookAndFeel::mono (10.0f));
        }
        text (g, "GR: 0 a -12", { x0, y0 + 414, pw, 16 }, Pal::amber, AGLLookAndFeel::mono (10.5f), Justification::centred);
        text (g, TXT ("branco: gate • ambar: teto"), { x0, y0 + 434, pw, 16 }, Pal::grey, AGLLookAndFeel::sans (10.5f), Justification::centred);
    }

    // ---------- 2. LIMITER ----------
    {
        const int x0 = 644, pw = 432;
        const bool on = master && proc.param ("limOn") > 0.5f;
        panel (g, { x0, y0, pw, ph });
        dot (g, (float) x0 + 26.0f, (float) y0 + 30.0f, on ? Pal::pink : Pal::dim, 7.0f);
        text (g, TXT ("2 · LIMITER"), { x0 + 38, y0 + 18, 200, 24 }, on ? Pal::pink : Pal::grey, AGLLookAndFeel::sans (13.5f, true));
        const String grs = grText (on ? proc.lim.grDb.load() : 0.0f);
        const auto gf = AGLLookAndFeel::mono (14.0f, true);
        const float gw = textWidth (gf, grs);
        text (g, grs, { x0 + pw - 20 - (int) gw - 2, y0 + 18, (int) gw + 4, 24 }, Pal::amber, gf);
        text (g, TXT ("Atenuacao · 5 s"), { x0 + pw - 20 - (int) gw - 130, y0 + 18, 120, 24 }, Pal::grey, AGLLookAndFeel::sans (12.5f), Justification::centredRight);

        caps (g, "THRESHOLD", { x0 + 10, y0 + 54, 100, 16 }, Pal::text, Justification::centred, 10.0f);
        caps (g, "CEILING",   { x0 + 158, y0 + 54, 100, 16 }, Pal::text, Justification::centred, 10.0f);
        text (g, "move os dois", { x0 + 96, y0 + 174, 76, 14 }, Pal::grey, AGLLookAndFeel::sans (10.5f), Justification::centred);
        const String gs = fmt (proc.param ("ceiling") - proc.param ("threshold"), 1, true) + " dB";
        const auto gl = AGLLookAndFeel::sans (12.5f), gv = AGLLookAndFeel::mono (12.5f, true);
        const float tot = textWidth (gl, "ganho ") + textWidth (gv, gs);
        const int gx = x0 + 134 - (int) (tot * 0.5f);
        text (g, "ganho ", { gx, y0 + 270, 60, 18 }, Pal::grey, gl);
        text (g, gs, { gx + (int) textWidth (gl, "ganho "), y0 + 270, 120, 18 }, Pal::white, gv);

        // grafico da atenuacao (5 s)
        auto gr = Rectangle<int> (x0 + 256, y0 + 62, pw - 256 - 18, 196);
        g.setColour (Pal::field); g.fillRoundedRectangle (gr.toFloat(), 8.0f);
        g.setColour (Pal::line); g.drawRoundedRectangle (gr.toFloat(), 8.0f, 1.0f);
        for (int k = 1; k < 4; ++k)
            g.drawHorizontalLine (gr.getY() + gr.getHeight() * k / 4, (float) gr.getX() + 1.0f, (float) gr.getRight() - 1.0f);
        Path ln, fl;
        const int n = (int) grHistory.size();
        for (int i = 0; i < n; ++i)
        {
            const float v = grHistory[(size_t) ((grHead + i) % n)];
            const float px = (float) gr.getX() + 2.0f + ((float) gr.getWidth() - 4.0f) * (float) i / (float) (n - 1);
            const float py = (float) gr.getY() + 2.0f + ((float) gr.getHeight() - 4.0f) * jlimit (0.0f, 1.0f, -v / 12.0f);
            if (i == 0) { ln.startNewSubPath (px, py); fl.startNewSubPath (px, (float) gr.getY() + 2.0f); }
            ln.lineTo (px, py); fl.lineTo (px, py);
        }
        fl.lineTo ((float) gr.getRight() - 2.0f, (float) gr.getY() + 2.0f);
        fl.closeSubPath();
        g.setColour (Pal::amber.withAlpha (0.15f)); g.fillPath (fl);
        g.setColour (Pal::amber); g.strokePath (ln, PathStrokeType (1.6f));
        text (g, "-3 / -6 / -9 dB", { gr.getX(), gr.getBottom() + 4, gr.getWidth(), 14 }, Pal::grey, AGLLookAndFeel::mono (10.0f), Justification::centred);

        caps (g, "RELEASE",   { x0 + 20, y0 + 310, 160, 16 }, Pal::text, Justification::centredLeft, 10.0f);
        caps (g, "LOOKAHEAD", { x0 + 20, y0 + 386, 160, 16 }, Pal::text, Justification::centredLeft, 10.0f);
        const String autoTxt = proc.param ("autoRelease") > 0.5f ? TXT ("automatico: 30 a 400 ms") : TXT ("manual: use o campo ao lado");
        text (g, autoTxt, { x0 + 214, y0 + 352, 198, 16 }, Pal::grey, AGLLookAndFeel::sans (10.5f), Justification::centred);
        const bool tpOn = proc.param ("truePeak") > 0.5f, la0 = (int) proc.param ("lookahead") == 0;
        text (g, tpOn && la0 ? TXT ("True Peak precisa de lookahead") : TXT ("pico entre amostras (exportacao)"),
              { x0 + 214, y0 + 426, 198, 16 }, tpOn && la0 ? Pal::amber : Pal::grey, AGLLookAndFeel::sans (10.5f), Justification::centred);
    }
}

void AGLEditor::paintAnalysis (Graphics& g)
{
    const int y0 = CT;
    const int state = proc.analyzer.state.load();

    // leitura
    auto top = Rectangle<int> (M, y0, W - 2 * M, 104);
    panel (g, top);
    text (g, TXT ("LEITURA DO DIA · UMA LEITURA AJUSTA OS DOIS"), { M + 18, y0 + 14, 520, 20 }, Pal::white, AGLLookAndFeel::sans (13.0f, true));
    text (g, TXT ("Fale e toque normalmente, com pausas e com o trecho mais forte."), { M + 18, y0 + 34, 520, 18 }, Pal::grey, AGLLookAndFeel::sans (12.5f));
    auto bar = Rectangle<float> ((float) M + 18.0f, (float) y0 + 64.0f, (float) (W - 2 * M - 36), 6.0f);
    g.setColour (Pal::track); g.fillRoundedRectangle (bar, 3.0f);
    const float prog = state == Analyzer::done ? 1.0f : state == Analyzer::running ? proc.analyzer.progress.load() : 0.0f;
    g.setColour (Pal::green); g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * jlimit (0.0f, 1.0f, prog)), 3.0f);
    const String stTxt = state == Analyzer::running ? "Lendo... " + String ((int) (prog * 100.0f)) + "%"
                       : state == Analyzer::done    ? TXT ("Leitura concluida") : TXT ("Pronto para ler");
    text (g, stTxt, { M + 18, y0 + 76, 300, 16 }, state == Analyzer::done ? Pal::green : Pal::grey, AGLLookAndFeel::sans (11.5f, true));

    // resultados
    const bool v = result.valid;
    struct Item { const char* name; String value; const char* unit; };
    const Item items[] = {
        { "PICO",      v ? fmt (result.peakDb) : "--",        "dBFS" },
        { "TRUE PEAK", v ? fmt (result.truePeakDb) : "--",    "dBTP" },
        { "RMS",       v ? fmt (result.rmsDb) : "--",         "dBFS" },
        { "LOUDNESS",  v ? fmt (result.lufs) : "--",          "LUFS" },
        { "DINAMICA",  v ? fmt (result.dynamicsDb()) : "--",  "dB" },
        { "RUIDO",     v ? fmt (result.noiseDb) : "--",       "dBFS" } };
    const int iw = (W - 2 * M - 5 * 12) / 6;
    for (int i = 0; i < 6; ++i)
    {
        auto r = Rectangle<int> (M + i * (iw + 12), y0 + 118, iw, 86);
        panel (g, r);
        caps (g, TXT (items[i].name), r.withHeight (18).translated (0, 14), Pal::text, Justification::centred);
        text (g, items[i].value, r.withY (r.getY() + 34).withHeight (30), Pal::white, AGLLookAndFeel::heavy (23.0f), Justification::centred);
        text (g, items[i].unit, r.withY (r.getY() + 64).withHeight (14), Pal::grey, AGLLookAndFeel::sans (11.0f), Justification::centred);
    }

    // sugestoes
    const auto s = makeSuggestion();
    auto sg = Rectangle<int> (M, y0 + 218, W - 2 * M, 292);
    panel (g, sg);
    const bool stream = destinationBox.getSelectedId() == 2;
    text (g, TXT ("AJUSTES SUGERIDOS · DESTINO ") + (stream ? "STREAMING (-14 LUFS)" : "AO VIVO"),
          { M + 18, y0 + 234, 600, 20 }, Pal::white, AGLLookAndFeel::sans (12.5f, true));
    const int c0 = M + 18, c1 = M + 264, c2 = M + 586, cR = W - M - 18;
    caps (g, TXT ("MODULO"), { c0, y0 + 266, 150, 16 }, Pal::grey, Justification::centredLeft, 10.0f);
    caps (g, TXT ("PARAMETRO"), { c1, y0 + 266, 250, 16 }, Pal::grey, Justification::centredLeft, 10.0f);
    caps (g, "ATUAL", { c2, y0 + 266, 200, 16 }, Pal::grey, Justification::centredLeft, 10.0f);
    caps (g, "SUGERIDO", { cR - 200, y0 + 266, 200, 16 }, Pal::grey, Justification::centredRight, 10.0f);

    struct Row { bool ag; const char* name; String now, sug; bool change; };
    const String keep = TXT ("mantem");
    const bool tpNow = proc.param ("truePeak") > 0.5f;
    const Row rows[] = {
        { true,  "Ganho Inicial",   fmt (proc.ag.gainDb.load(), 1, true) + " dB",      v ? fmt (s.agGain, 1, true) + " dB" : "--", true },
        { true,  "Gate (silencio)", String (proc.param ("gate"), 0) + " dBFS",        v ? String (s.gate, 0) + " dBFS" : "--",   true },
        { false, "Threshold",       fmt (proc.param ("threshold")) + " dB",           v ? fmt (s.threshold) + " dB" : "--",      true },
        { false, "Ceiling",         fmt (proc.param ("ceiling")) + " dBFS",           keep,                                      false },
        { false, "Release",         proc.param ("autoRelease") > 0.5f ? String ("Auto") : String (proc.param ("release"), 0) + " ms",
                                    v ? String ("Auto") : String ("--"),              proc.param ("autoRelease") < 0.5f },
        { false, "True Peak",       tpNow ? "ON" : "OFF",                             v ? (s.truePeak ? "ON" : (tpNow ? "ON" : "OFF")) : "--", s.truePeak && ! tpNow } };
    for (int i = 0; i < 6; ++i)
    {
        const int yy = y0 + 286 + i * 29;
        g.setColour (Pal::line); g.drawHorizontalLine (yy, (float) c0, (float) cR);
        text (g, rows[i].ag ? "Auto Gain" : "Limiter", { c0, yy, 200, 29 }, rows[i].ag ? Pal::cyan : Pal::pink, AGLLookAndFeel::sans (13.0f, true));
        text (g, TXT (rows[i].name), { c1, yy, 300, 29 }, Pal::text, AGLLookAndFeel::sans (13.0f));
        text (g, rows[i].now, { c2, yy, 200, 29 }, Pal::grey, AGLLookAndFeel::mono (12.5f));
        const bool hi = v && rows[i].change;
        text (g, rows[i].sug, { cR - 260, yy, 260, 29 }, hi ? Pal::green : Pal::text,
              hi ? AGLLookAndFeel::mono (12.5f, true) : AGLLookAndFeel::sans (13.0f), Justification::centredRight);
    }
    if (v)
        text (g, TXT ("Previsao: depois do Auto Gain o pico chega a ") + fmt (s.peakAfterAg, 1, true)
                 + TXT (" dBTP; o Limiter vai atenuar cerca de ") + String (s.predictedGr, 1)
                 + TXT (" dB nos trechos mais fortes; loudness final ≈ ") + String (s.lufsOut, 1) + " LUFS.",
              { M + 18, y0 + 466, W - 2 * M - 36, 18 }, Pal::grey, AGLLookAndFeel::sans (11.5f));

    // aviso (entre a tabela e os botoes)
    String warn;
    if (v && s.predictedGr > 6.0f)
        warn = TXT ("Atenuacao alta (mais de 6 dB): o som pode ficar esmagado. Abaixe o Target ou suba o Threshold.");
    else if (v && ! stream && ! tpNow && result.truePeakDb - result.peakDb > 0.3f)
        warn = TXT ("O true peak esta ") + String (result.truePeakDb - result.peakDb, 1)
             + TXT (" dB acima do pico normal. Para gravar ou exportar, use o destino Streaming (liga o True Peak).");
    if (warn.isNotEmpty())
        text (g, warn, { M + 18, y0 + 486, W - 2 * M - 36, 18 }, Pal::amber, AGLLookAndFeel::sans (11.5f, true));
    else if (applied)
        text (g, TXT ("Ajustes aplicados nos dois modulos."), { M + 18, y0 + 486, W - 2 * M - 36, 18 }, Pal::green, AGLLookAndFeel::sans (11.5f, true));
}

void AGLEditor::paintOutputBar (Graphics& g)
{
    const int H = getHeight();
    auto r = Rectangle<int> (M, H - M - 46, W - 2 * M, 46);
    panel (g, r);
    caps (g, TXT ("SAIDA FINAL"), { r.getX() + 18, r.getY(), 120, r.getHeight() }, Pal::text);

    const String teto = "teto " + fmt (proc.param ("ceiling")) + " dBFS";
    const auto tf = AGLLookAndFeel::mono (12.0f, true);
    const float tw = textWidth (tf, teto);
    text (g, teto, { r.getRight() - 18 - (int) tw - 2, r.getY(), (int) tw + 4, r.getHeight() }, Pal::amber, tf);

    const float bx = (float) r.getX() + 120.0f, bw = (float) r.getRight() - 18.0f - tw - 24.0f - bx;
    auto xOf = [&] (float db) { return bx + bw * jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f); };
    for (int c = 0; c < 2; ++c)
    {
        auto lane = Rectangle<float> (bx, (float) r.getCentreY() - 6.0f + (float) c * 8.0f, bw, 4.0f);
        g.setColour (Pal::track); g.fillRoundedRectangle (lane, 2.0f);
        const float pk = proc.lim.outPeakChDb[c].load();
        if (pk > -60.0f)
        {
            ColourGradient grad (Pal::green, bx, 0.0f, Pal::amber, xOf (-3.0f), 0.0f, false);
            g.setGradientFill (grad);
            g.fillRoundedRectangle (lane.withRight (xOf (pk)), 2.0f);
        }
    }
    g.setColour (Pal::amber);
    g.fillRect (xOf (proc.param ("ceiling")) - 1.0f, (float) r.getCentreY() - 11.0f, 2.0f, 22.0f);
}
