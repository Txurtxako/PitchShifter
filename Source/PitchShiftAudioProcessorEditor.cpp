#include "PitchShiftAudioProcessorEditor.h"

static juce::String getGuitarTuning (int semi)
{
    switch (semi)
    {
        case 0:   return "E  A  D  G  B  E";
        case -1:  return "Eb  Ab  Db  Gb  Bb  Eb";
        case -2:  return "D  G  C  F  A  D";
        case -3:  return "Db  Gb  B  E  Ab  Db";
        case -4:  return "C  F  Bb  Eb  G  C";
        case -5:  return "B  E  A  D  F#  B";
        case -6:  return "Bb  Eb  Ab  Db  F  Bb";
        case -7:  return "A  D  G  C  E  A";
        case -12: return "E  A  D  G  B  E  (-1 OCT)";
        case 1:   return "F  Bb  Eb  Ab  C  F";
        case 2:   return "F#  B  E  A  C#  F#";
        case 12:  return "E  A  D  G  B  E  (+1 OCT)";
        default:  return (semi > 0 ? "+" : "") + juce::String (semi) + " ST";
    }
}

static juce::String getBassTuning (int semi)
{
    switch (semi)
    {
        case 0:   return "E  A  D  G";
        case -1:  return "Eb  Ab  Db  Gb";
        case -2:  return "D  G  C  F";
        case -3:  return "Db  Gb  B  E";
        case -4:  return "C  F  Bb  Eb";
        case -5:  return "B  E  A  D";
        case -7:  return "A  D  G  C";
        case -12: return "E  A  D  G  (-1 OCT)";
        case 12:  return "E  A  D  G  (+1 OCT)";
        default:  return (semi > 0 ? "+" : "") + juce::String (semi) + " ST";
    }
}

CustomPedalLookAndFeel::CustomPedalLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, juce::Colour (0xffe2e8f0));
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
}

void CustomPedalLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                               float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                                               juce::Slider& slider)
{
    auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (4.0f);
    auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) / 2.0f;
    auto toAngle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
    auto centre = bounds.getCentre();

    // Bisel exterior oscuro
    g.setColour (juce::Colour (0xff1c202c));
    g.fillEllipse (centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f);
    g.setColour (juce::Colour (0xff333a4d));
    g.drawEllipse (centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f, 1.5f);

    // Arco de fondo (inactivo)
    auto arcRadius = radius - 5.0f;
    juce::Path bgArc;
    bgArc.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
    g.setColour (juce::Colour (0xff0d1017));
    g.strokePath (bgArc, juce::PathStrokeType (4.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Color temático según el control
    juce::Colour arcCol = juce::Colour (0xfff59e0b); // Ámbar
    if (slider.getName() == "cents" || slider.getName() == "tight" || slider.getName() == "lowCut")
        arcCol = juce::Colour (0xff06b6d4); // Cian
    else if (slider.getName() == "mix" || slider.getName() == "outputGain")
        arcCol = juce::Colour (0xff10b981); // Verde esmeralda
    else if (slider.getName() == "tone" || slider.getName() == "highCut" || slider.getName() == "inputGain")
        arcCol = juce::Colour (0xfff59e0b); // Ámbar / Oro
    else if (slider.getName() == "gate")
        arcCol = juce::Colour (0xffef4444); // Rojo

    // Arco de valor activo
    juce::Path valArc;
    valArc.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, rotaryStartAngle, toAngle, true);
    g.setColour (arcCol);
    g.strokePath (valArc, juce::PathStrokeType (4.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Cuerpo interior metálico del potenciómetro
    auto knobRadius = radius - 11.0f;
    juce::ColourGradient grad (juce::Colour (0xff2e3344), centre.x - knobRadius, centre.y - knobRadius,
                               juce::Colour (0xff141720), centre.x + knobRadius, centre.y + knobRadius, false);
    g.setGradientFill (grad);
    g.fillEllipse (centre.x - knobRadius, centre.y - knobRadius, knobRadius * 2.0f, knobRadius * 2.0f);

    g.setColour (juce::Colour (0xff4b5568));
    g.drawEllipse (centre.x - knobRadius, centre.y - knobRadius, knobRadius * 2.0f, knobRadius * 2.0f, 1.2f);

    // Muesca / Indicador de posición
    juce::Path p;
    auto pointerLength = knobRadius * 0.75f;
    auto pointerThickness = 2.5f;
    p.addRoundedRectangle (-pointerThickness * 0.5f, -knobRadius, pointerThickness, pointerLength, 1.0f);
    p.applyTransform (juce::AffineTransform::rotation (toAngle).translated (centre.x, centre.y));
    g.setColour (arcCol);
    g.fillPath (p);
}

void CustomPedalLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                                               float sliderPos, float minSliderPos, float maxSliderPos,
                                               const juce::Slider::SliderStyle, juce::Slider& slider)
{
    juce::ignoreUnused (minSliderPos, maxSliderPos, slider);
    auto trackY = (float) y + (float) height * 0.5f;
    auto trackH = 6.0f;
    auto trackBounds = juce::Rectangle<float> ((float) x, trackY - trackH * 0.5f, (float) width, trackH);

    // Canal o ranura oscura con bisel interno
    g.setColour (juce::Colour (0xff1c1f28));
    g.fillRoundedRectangle (trackBounds, 3.0f);
    g.setColour (juce::Colour (0xff2e3342));
    g.drawRoundedRectangle (trackBounds, 3.0f, 1.0f);

    // Disco metálico ámbar del fader
    auto thumbRadius = 10.0f;
    auto thumbX = juce::jlimit ((float) x + thumbRadius, (float) (x + width) - thumbRadius, sliderPos);
    auto thumbY = trackY;

    // Resplandor cálido ámbar
    g.setColour (juce::Colour (0x55f59e0b));
    g.fillEllipse (thumbX - thumbRadius - 2.0f, thumbY - thumbRadius - 2.0f, (thumbRadius + 2.0f) * 2.0f, (thumbRadius + 2.0f) * 2.0f);

    // Gradiente metálico dorado / ámbar
    juce::ColourGradient thumbGrad (juce::Colour (0xfffbbf24), thumbX - thumbRadius * 0.5f, thumbY - thumbRadius * 0.5f,
                                    juce::Colour (0xffb45309), thumbX + thumbRadius, thumbY + thumbRadius, false);
    g.setGradientFill (thumbGrad);
    g.fillEllipse (thumbX - thumbRadius, thumbY - thumbRadius, thumbRadius * 2.0f, thumbRadius * 2.0f);

    // Anillo exterior brillante
    g.setColour (juce::Colour (0xfffde68a));
    g.drawEllipse (thumbX - thumbRadius, thumbY - thumbRadius, thumbRadius * 2.0f, thumbRadius * 2.0f, 1.5f);
}

void CustomPedalLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button,
                                                   const juce::Colour& backgroundColour,
                                                   bool shouldDrawButtonAsHighlighted,
                                                   bool shouldDrawButtonAsDown)
{
    juce::ignoreUnused (backgroundColour);
    auto bounds = button.getLocalBounds().toFloat().reduced (1.0f);

    // Si es el Footswitch (Pulsador metálico 3PDT cromado)
    if (button.getName() == "footswitch")
    {
        auto centre = bounds.getCentre();
        auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.45f;

        // Sombra
        g.setColour (juce::Colour (0x66000000));
        g.fillEllipse (centre.x - radius + 2.0f, centre.y - radius + 3.0f, radius * 2.0f, radius * 2.0f);

        // Tuerca exterior metálica
        juce::ColourGradient nutGrad (juce::Colour (0xff475569), centre.x - radius, centre.y - radius,
                                      juce::Colour (0xff1e293b), centre.x + radius, centre.y + radius, false);
        g.setGradientFill (nutGrad);
        g.fillEllipse (centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f);
        g.setColour (juce::Colour (0xff64748b));
        g.drawEllipse (centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f, 1.5f);

        // Botón cromado central
        auto innerRadius = radius * 0.70f;
        juce::ColourGradient chromeGrad (shouldDrawButtonAsDown ? juce::Colour (0xff94a3b8) : juce::Colour (0xfff1f5f9),
                                         centre.x - innerRadius, centre.y - innerRadius,
                                         shouldDrawButtonAsDown ? juce::Colour (0xff334155) : juce::Colour (0xff64748b),
                                         centre.x + innerRadius, centre.y + innerRadius, false);
        g.setGradientFill (chromeGrad);
        g.fillEllipse (centre.x - innerRadius, centre.y - innerRadius, innerRadius * 2.0f, innerRadius * 2.0f);
        g.setColour (juce::Colour (0xffcbd5e1));
        g.drawEllipse (centre.x - innerRadius, centre.y - innerRadius, innerRadius * 2.0f, innerRadius * 2.0f, 1.0f);

        // Icono de standby/power en el centro
        const bool active = !button.getToggleState();
        g.setColour (active ? juce::Colour (0xff059669) : juce::Colour (0xff64748b));
        juce::Path powerArc;
        powerArc.addCentredArc (centre.x, centre.y, innerRadius * 0.45f, innerRadius * 0.45f, 0.0f, juce::MathConstants<float>::pi * 0.25f, juce::MathConstants<float>::pi * 1.75f, true);
        g.strokePath (powerArc, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.drawLine (centre.x, centre.y - innerRadius * 0.5f, centre.x, centre.y - 1.0f, 2.0f);
        return;
    }

    // Botones estándar
    juce::Colour bg = button.getToggleState()
                        ? juce::Colour (0xfff59e0b)
                        : (shouldDrawButtonAsHighlighted ? juce::Colour (0xff293042) : juce::Colour (0xff181d28));

    if (shouldDrawButtonAsDown)
        bg = bg.darker (0.2f);

    g.setColour (bg);
    g.fillRoundedRectangle (bounds, 5.0f);

    juce::Colour borderCol = button.getToggleState() ? juce::Colour (0xfffbbf24) : juce::Colour (0xff333d52);
    g.setColour (borderCol);
    g.drawRoundedRectangle (bounds, 5.0f, 1.0f);
}

void CustomPedalLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& button,
                                             bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown)
{
    juce::ignoreUnused (shouldDrawButtonAsHighlighted, shouldDrawButtonAsDown);

    if (button.getName() == "footswitch")
        return;

    juce::Colour textCol = button.getToggleState()
                             ? juce::Colour (0xff000000)
                             : juce::Colour (0xffe2e8f0);

    g.setColour (textCol);
    g.setFont (juce::Font (11.0f, juce::Font::bold));
    g.drawText (button.getButtonText(), button.getLocalBounds(), juce::Justification::centred, true);
}

PitchShiftAudioProcessorEditor::PitchShiftAudioProcessorEditor (PitchShiftAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    setLookAndFeel (&customLookAndFeel);

    auto setupRotary = [this] (juce::Slider& s, juce::Label& l, const juce::String& name, const juce::String& text, const juce::String& suffix) {
        s.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 65, 18);
        s.setTextValueSuffix (suffix);
        s.setName (name);
        addAndMakeVisible (s);

        l.setText (text, juce::dontSendNotification);
        l.setJustificationType (juce::Justification::centred);
        l.setFont (juce::Font (11.0f, juce::Font::bold));
        l.setColour (juce::Label::textColourId, juce::Colour (0xff94a3b8));
        addAndMakeVisible (l);
    };

    auto setupLinearFader = [this] (juce::Slider& s, juce::Label& l, const juce::String& name, const juce::String& text) {
        s.setSliderStyle (juce::Slider::LinearHorizontal);
        s.setTextBoxStyle (juce::Slider::TextBoxRight, false, 60, 18);
        s.setTextValueSuffix (" dB");
        s.setName (name);
        addAndMakeVisible (s);

        l.setText (text, juce::dontSendNotification);
        l.setJustificationType (juce::Justification::centredLeft);
        l.setFont (juce::Font (11.0f, juce::Font::bold));
        l.setColour (juce::Label::textColourId, juce::Colour (0xffcbd5e1));
        addAndMakeVisible (l);
    };

    // 6 Potenciómetros superiores (Modelado tímbrico y transposición)
    setupRotary (semitonesSlider,  semitonesLabel,  "semitones",  "SEMITONOS",      " ST");
    setupRotary (centsSlider,      centsLabel,      "cents",      "FINO (CENTS)",   " ct");
    setupRotary (mixSlider,        mixLabel,        "mix",        "MEZCLA (MIX)",   " %");
    setupRotary (lowCutSlider,     lowCutLabel,     "lowCut",     "CORTE GRAVES",   " Hz");
    setupRotary (highCutSlider,    highCutLabel,    "highCut",    "CORTE AGUDOS",   " Hz");
    setupRotary (gateSlider,       gateLabel,       "gate",       "PUERTA RUIDO",   " dB");

    // Faders lineales inferiores (Ganancia de Entrada y Volumen de Salida)
    setupLinearFader (inputGainSlider,  inputGainLabel,  "inputGain",  "GANANCIA ENTRADA");
    setupLinearFader (outputGainSlider, outputGainLabel, "outputGain", "VOLUMEN SALIDA");

    // Conexión con APVTS para soporte completo de automatización DAW
    inputGainAttachment  = std::make_unique<SliderAttachment> (audioProcessor.apvts, "inputGain", inputGainSlider);
    semitonesAttachment  = std::make_unique<SliderAttachment> (audioProcessor.apvts, "semitones", semitonesSlider);
    centsAttachment      = std::make_unique<SliderAttachment> (audioProcessor.apvts, "cents", centsSlider);
    mixAttachment        = std::make_unique<SliderAttachment> (audioProcessor.apvts, "mix", mixSlider);
    lowCutAttachment     = std::make_unique<SliderAttachment> (audioProcessor.apvts, "lowCut", lowCutSlider);
    highCutAttachment    = std::make_unique<SliderAttachment> (audioProcessor.apvts, "highCut", highCutSlider);
    gateAttachment       = std::make_unique<SliderAttachment> (audioProcessor.apvts, "gateThreshold", gateSlider);
    outputGainAttachment = std::make_unique<SliderAttachment> (audioProcessor.apvts, "outputGain", outputGainSlider);

    // Botones de ajuste paso a paso (-1, 0, +1)
    semitoneDownBtn.onClick = [this] { semitonesSlider.setValue (semitonesSlider.getValue() - 1.0, juce::sendNotificationSync); };
    semitoneZeroBtn.onClick = [this] { semitonesSlider.setValue (0.0, juce::sendNotificationSync); centsSlider.setValue (0.0, juce::sendNotificationSync); };
    semitoneUpBtn.onClick   = [this] { semitonesSlider.setValue (semitonesSlider.getValue() + 1.0, juce::sendNotificationSync); };
    addAndMakeVisible (semitoneDownBtn);
    addAndMakeVisible (semitoneZeroBtn);
    addAndMakeVisible (semitoneUpBtn);

    // Accesos rápidos (-12 a +12)
    const std::vector<std::pair<int, juce::String>> jumps = {
        { -12, "-12" }, { -7, "-7" }, { -5, "-5" }, { -4, "-4" },
        { -3, "-3" }, { -2, "-2" }, { -1, "-1" }, { 0, "0" },
        { 1, "+1" }, { 2, "+2" }, { 5, "+5" }, { 7, "+7" }, { 12, "+12" }
    };
    for (auto& item : jumps)
    {
        auto btn = std::make_unique<juce::TextButton> (item.second);
        int targetVal = item.first;
        btn->onClick = [this, targetVal] {
            semitonesSlider.setValue ((double) targetVal, juce::sendNotificationSync);
        };
        addAndMakeVisible (*btn);
        quickJumpButtons.push_back (std::move (btn));
    }

    // Selector de Modo de Instrumento
    guitarModeBtn.onClick = [this] {
        if (auto* p = audioProcessor.apvts.getParameter ("mode"))
            p->setValueNotifyingHost (0.0f);
    };
    bassModeBtn.onClick = [this] {
        if (auto* p = audioProcessor.apvts.getParameter ("mode"))
            p->setValueNotifyingHost (1.0f);
    };
    addAndMakeVisible (guitarModeBtn);
    addAndMakeVisible (bassModeBtn);

    // Puerta de Ruido Toggle
    gateToggleBtn.setClickingTogglesState (true);
    gateEnableAttachment = std::make_unique<ButtonAttachment> (audioProcessor.apvts, "gateEnable", gateToggleBtn);
    addAndMakeVisible (gateToggleBtn);

    // Footswitch Bypass
    bypassFootswitch.setName ("footswitch");
    bypassFootswitch.setClickingTogglesState (true);
    bypassAttachment = std::make_unique<ButtonAttachment> (audioProcessor.apvts, "bypass", bypassFootswitch);
    addAndMakeVisible (bypassFootswitch);

    startTimerHz (30);
    setSize (840, 580);
}

PitchShiftAudioProcessorEditor::~PitchShiftAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void PitchShiftAudioProcessorEditor::timerCallback()
{
    const bool isBass = audioProcessor.apvts.getRawParameterValue ("mode")->load() > 0.5f;
    guitarModeBtn.setToggleState (!isBass, juce::dontSendNotification);
    bassModeBtn.setToggleState (isBass, juce::dontSendNotification);

    const bool isGateOn = audioProcessor.apvts.getRawParameterValue ("gateEnable")->load() > 0.5f;
    gateToggleBtn.setButtonText (isGateOn ? "GATE ON" : "GATE OFF");

    const int currentSemi = (int) std::round (audioProcessor.apvts.getRawParameterValue ("semitones")->load());
    const int jumpVals[] = { -12, -7, -5, -4, -3, -2, -1, 0, 1, 2, 5, 7, 12 };
    for (size_t i = 0; i < quickJumpButtons.size() && i < 13; ++i)
    {
        quickJumpButtons[i]->setToggleState (currentSemi == jumpVals[i], juce::dontSendNotification);
    }

    repaint();
}

void PitchShiftAudioProcessorEditor::paint (juce::Graphics& g)
{
    // 1. Chasis oscuro tipo pedal boutique
    juce::ColourGradient bgGrad (juce::Colour (0xff151822), 0.0f, 0.0f,
                                 juce::Colour (0xff0c0e14), 0.0f, (float) getHeight(), false);
    g.setGradientFill (bgGrad);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 12.0f);

    g.setColour (juce::Colour (0xff2b3245));
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 12.0f, 1.5f);

    // Tornillos de chasis
    auto drawScrew = [&g] (float cx, float cy) {
        g.setColour (juce::Colour (0xff1f2430));
        g.fillEllipse (cx - 7.0f, cy - 7.0f, 14.0f, 14.0f);
        g.setColour (juce::Colour (0xff4b5563));
        g.drawEllipse (cx - 7.0f, cy - 7.0f, 14.0f, 14.0f, 1.0f);
        g.setColour (juce::Colour (0xff6b7280));
        g.drawLine (cx - 4.0f, cy - 4.0f, cx + 4.0f, cy + 4.0f, 1.5f);
    };
    drawScrew (16.0f, 16.0f);
    drawScrew ((float) getWidth() - 16.0f, 16.0f);
    drawScrew (16.0f, (float) getHeight() - 16.0f);
    drawScrew ((float) getWidth() - 16.0f, (float) getHeight() - 16.0f);

    // 2. Encabezado
    g.setColour (juce::Colour (0xfff59e0b));
    g.setFont (juce::Font (20.0f, juce::Font::bold));
    g.drawText ("PITCH SHIFT", 35, 14, 200, 24, juce::Justification::centredLeft);

    g.setColour (juce::Colour (0xff94a3b8));
    g.setFont (juce::Font (11.0f, juce::Font::bold));
    g.drawText ("GUITAR & BASS TRANSPOSER", 165, 17, 350, 20, juce::Justification::centredLeft);

    // 3. Pantalla OLED Central (x: 24, y: 50, w: 792, h: 105)
    juce::Rectangle<float> oledRect (24.0f, 50.0f, 792.0f, 105.0f);
    g.setColour (juce::Colour (0xff080b12));
    g.fillRoundedRectangle (oledRect, 8.0f);
    g.setColour (juce::Colour (0xff1f293d));
    g.drawRoundedRectangle (oledRect, 8.0f, 1.5f);

    g.setColour (juce::Colour (0xff475569));
    g.setFont (juce::Font (10.0f, juce::Font::bold));
    g.drawText ("POLYPHONIC TRANSPOSITION ENGINE", 36, 56, 300, 16, juce::Justification::centredLeft);

    const bool isBass = audioProcessor.apvts.getRawParameterValue ("mode")->load() > 0.5f;
    g.setColour (isBass ? juce::Colour (0xff06b6d4) : juce::Colour (0xfff59e0b));
    g.drawText (isBass ? "MODO: BAJO ELECTRICO" : "MODO: GUITARRA", 560, 56, 240, 16, juce::Justification::centredRight);

    const int semi = (int) std::round (audioProcessor.apvts.getRawParameterValue ("semitones")->load());
    const int cents = (int) std::round (audioProcessor.apvts.getRawParameterValue ("cents")->load());

    juce::String pitchTitle;
    if (semi > 0) pitchTitle = "+" + juce::String (semi) + " SEMITONOS";
    else if (semi < 0) pitchTitle = juce::String (semi) + " SEMITONOS";
    else pitchTitle = "0 SEMITONOS (ESTANDAR)";

    juce::String tuningName;
    if (semi == 0 && cents == 0) tuningName = "ESTANDAR E (440 Hz)";
    else if (semi == -1) tuningName = "Eb / D# STANDARD";
    else if (semi == -2) tuningName = "D STANDARD / DROP D";
    else if (semi == -3) tuningName = "C# / Db STANDARD";
    else if (semi == -4) tuningName = "C STANDARD / DROP C";
    else if (semi == -5) tuningName = "B STANDARD (7 Cuerdas / Baritono)";
    else if (semi == -7) tuningName = "A STANDARD / DROP A";
    else if (semi == -12) tuningName = "OCTAVA ABAJO (-1 OCT)";
    else if (semi == 12) tuningName = "OCTAVA ARRIBA (+1 OCT)";
    else if (semi < 0) tuningName = "DROP TUNING (" + juce::String (std::abs(semi)) + " TONOS ABAJO)";
    else tuningName = "PITCH UP / CAPO (+" + juce::String (semi) + " TONOS ARRIBA)";

    g.setColour (semi == 0 ? juce::Colour (0xff10b981) : (semi < 0 ? juce::Colour (0xfff59e0b) : juce::Colour (0xff06b6d4)));
    g.setFont (juce::Font (24.0f, juce::Font::bold));
    g.drawText (pitchTitle + "   •   " + tuningName, 36, 75, 760, 35, juce::Justification::centredLeft);

    juce::String stringsText = isBass 
        ? "CUERDAS RESULTANTES (BAJO): " + getBassTuning (semi)
        : "CUERDAS RESULTANTES (GUITARRA): " + getGuitarTuning (semi);

    g.setColour (juce::Colour (0xffcbd5e1));
    g.setFont (juce::Font (11.0f, juce::Font::bold));
    g.drawText (stringsText, 36, 110, 500, 18, juce::Justification::centredLeft);

    const int mixVal = (int) std::round (audioProcessor.apvts.getRawParameterValue ("mix")->load());
    const float inVal = audioProcessor.apvts.getRawParameterValue ("inputGain")->load();
    const float outVal = audioProcessor.apvts.getRawParameterValue ("outputGain")->load();
    float lowCutVal = 0.0f;
    if (auto* p = audioProcessor.apvts.getRawParameterValue ("lowCut")) lowCutVal = p->load();
    else if (auto* p = audioProcessor.apvts.getRawParameterValue ("tightCut")) lowCutVal = p->load();
    float highCutVal = 20000.0f;
    if (auto* p = audioProcessor.apvts.getRawParameterValue ("highCut")) highCutVal = p->load();
    else if (auto* p = audioProcessor.apvts.getRawParameterValue ("toneCut")) highCutVal = p->load();

    juce::String techInfo = "IN: " + juce::String (inVal > 0 ? "+" : "") + juce::String (inVal, 1) + " dB"
                          + "  |  MIX: " + juce::String (mixVal) + "%"
                          + "  |  CORTE GRV: " + (lowCutVal <= 10.0f ? "OFF" : juce::String ((int)lowCutVal) + " Hz (12dB/oct)")
                          + "  |  CORTE AGD: " + (highCutVal >= 19990.0f ? "OFF" : juce::String ((int)highCutVal) + " Hz (12dB/oct)")
                          + "  |  OUT: " + juce::String (outVal > 0 ? "+" : "") + juce::String (outVal, 1) + " dB";

    g.setColour (juce::Colour (0xff64748b));
    g.setFont (juce::Font (10.0f, juce::Font::plain));
    g.drawText (techInfo, 36, 130, 750, 16, juce::Justification::centredLeft);

    // 4. Barra de accesos directos
    juce::Rectangle<float> jumpBarRect (24.0f, 165.0f, 792.0f, 36.0f);
    g.setColour (juce::Colour (0xff10131b));
    g.fillRoundedRectangle (jumpBarRect, 6.0f);
    g.setColour (juce::Colour (0xff1e2433));
    g.drawRoundedRectangle (jumpBarRect, 6.0f, 1.0f);

    // 5. Panel de potenciómetros (6 potenciómetros hardware de modelado tímbrico)
    juce::Rectangle<float> knobsRect (24.0f, 208.0f, 792.0f, 198.0f);
    g.setColour (juce::Colour (0xff10131b));
    g.fillRoundedRectangle (knobsRect, 8.0f);
    g.setColour (juce::Colour (0xff1f2536));
    g.drawRoundedRectangle (knobsRect, 8.0f, 1.0f);

    // 6. Panel inferior y Footswitch (Faders In/Out Gain + Pulsador central Stomp)
    juce::Rectangle<float> bottomRect (24.0f, 415.0f, 792.0f, 145.0f);
    g.setColour (juce::Colour (0xff141721));
    g.fillRoundedRectangle (bottomRect, 8.0f);
    g.setColour (juce::Colour (0xff222838));
    g.drawRoundedRectangle (bottomRect, 8.0f, 1.0f);

    // Texto de estado de efecto (Sin luz LED, solo texto informativo)
    const bool isBypassed = audioProcessor.apvts.getRawParameterValue ("bypass")->load() > 0.5f;

    if (!isBypassed)
    {
        g.setColour (juce::Colour (0xff34d399));
        g.setFont (juce::Font (11.0f, juce::Font::bold));
        g.drawText ("ACTIVO", 220, 428, 400, 16, juce::Justification::centred);
    }
    else
    {
        g.setColour (juce::Colour (0xff94a3b8));
        g.setFont (juce::Font (11.0f, juce::Font::bold));
        g.drawText ("BYPASS", 220, 428, 400, 16, juce::Justification::centred);
    }

    g.setColour (juce::Colour (0xff64748b));
    g.setFont (juce::Font (9.0f, juce::Font::bold));
    g.drawText ("PISAR PARA ACTIVAR / BYPASS", 270, 538, 300, 14, juce::Justification::centred);

    g.setColour (juce::Colour (0xff475569));
    g.setFont (juce::Font (10.0f, juce::Font::plain));
    g.drawText ("XAINA DSP", 540, 538, 260, 16, juce::Justification::centredRight);
}

void PitchShiftAudioProcessorEditor::resized()
{
    guitarModeBtn.setBounds (615, 14, 100, 26);
    bassModeBtn.setBounds (720, 14, 95, 26);

    int startX = 26;
    int btnW = 57;
    for (auto& btn : quickJumpButtons)
    {
        btn->setBounds (startX, 170, btnW, 26);
        startX += btnW + 3;
    }

    // 6 Potenciómetros superiores en el panel principal
    int knobStartX = 28;
    int colWidth = 130;
    juce::Slider* sliders[] = { &semitonesSlider, &centsSlider, &mixSlider, &lowCutSlider, &highCutSlider, &gateSlider };
    juce::Label* labels[]   = { &semitonesLabel, &centsLabel, &mixLabel, &lowCutLabel, &highCutLabel, &gateLabel };

    for (int i = 0; i < 6; ++i)
    {
        int x = knobStartX + i * colWidth;
        labels[i]->setBounds (x, 216, colWidth, 18);
        sliders[i]->setBounds (x + (colWidth - 85) / 2, 236, 85, 95);
    }

    // Botones de ajuste fino bajo Semitonos (Columna 0)
    semitoneDownBtn.setBounds (knobStartX + 18, 342, 28, 22);
    semitoneZeroBtn.setBounds (knobStartX + 51, 342, 28, 22);
    semitoneUpBtn.setBounds   (knobStartX + 84, 342, 28, 22);

    // Botón Puerta de Ruido bajo Gate (Columna 5)
    int gateColX = knobStartX + 5 * colWidth;
    gateToggleBtn.setBounds (gateColX + 16, 342, 98, 22);

    // Panel Inferior: Fader Entrada (izq), Footswitch Bypass (centro), Fader Salida (der)
    inputGainLabel.setBounds (42, 450, 190, 18);
    inputGainSlider.setBounds (40, 470, 285, 34);

    bypassFootswitch.setBounds (420 - 32, 454, 64, 62);

    outputGainLabel.setBounds (515, 450, 190, 18);
    outputGainSlider.setBounds (513, 470, 285, 34);
}
