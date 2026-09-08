#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PitchShiftAudioProcessor.h"

class CustomPedalLookAndFeel : public juce::LookAndFeel_V4
{
public:
    CustomPedalLookAndFeel();
    ~CustomPedalLookAndFeel() override = default;

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                           juce::Slider& slider) override;

    void drawButtonBackground (juce::Graphics& g, juce::Button& button,
                               const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted,
                               bool shouldDrawButtonAsDown) override;

    void drawButtonText (juce::Graphics& g, juce::TextButton& button,
                         bool shouldDrawButtonAsHighlighted,
                         bool shouldDrawButtonAsDown) override;
};

class PitchShiftAudioProcessorEditor : public juce::AudioProcessorEditor,
                                       private juce::Timer
{
public:
    explicit PitchShiftAudioProcessorEditor (PitchShiftAudioProcessor&);
    ~PitchShiftAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    PitchShiftAudioProcessor& audioProcessor;
    CustomPedalLookAndFeel customLookAndFeel;

    // Sliders principales (8 potenciómetros hardware)
    juce::Slider inputGainSlider;
    juce::Slider semitonesSlider;
    juce::Slider centsSlider;
    juce::Slider mixSlider;
    juce::Slider lowCutSlider;
    juce::Slider highCutSlider;
    juce::Slider gateSlider;
    juce::Slider outputGainSlider;

    // Etiquetas de los potenciómetros
    juce::Label inputGainLabel;
    juce::Label semitonesLabel;
    juce::Label centsLabel;
    juce::Label mixLabel;
    juce::Label lowCutLabel;
    juce::Label highCutLabel;
    juce::Label gateLabel;
    juce::Label outputGainLabel;

    // Botones de ajuste fino semitono (-1, 0, +1)
    juce::TextButton semitoneDownBtn { "-1" };
    juce::TextButton semitoneZeroBtn { "0" };
    juce::TextButton semitoneUpBtn   { "+1" };

    // Botones de acceso directo rápido (-12 a +12)
    std::vector<std::unique_ptr<juce::TextButton>> quickJumpButtons;

    // Selector de modo de instrumento (Guitarra / Bajo)
    juce::TextButton guitarModeBtn { "GUITARRA" };
    juce::TextButton bassModeBtn   { "BAJO" };

    // Toggle de Puerta de Ruido
    juce::TextButton gateToggleBtn { "GATE ON" };

    // Footswitch Bypass Stomp
    juce::TextButton bypassFootswitch;

    // Conexiones sincronizadas bidireccionalmente con el DAW (APVTS)
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    std::unique_ptr<SliderAttachment> inputGainAttachment;
    std::unique_ptr<SliderAttachment> semitonesAttachment;
    std::unique_ptr<SliderAttachment> centsAttachment;
    std::unique_ptr<SliderAttachment> mixAttachment;
    std::unique_ptr<SliderAttachment> lowCutAttachment;
    std::unique_ptr<SliderAttachment> highCutAttachment;
    std::unique_ptr<SliderAttachment> gateAttachment;
    std::unique_ptr<SliderAttachment> outputGainAttachment;
    std::unique_ptr<ButtonAttachment> bypassAttachment;
    std::unique_ptr<ButtonAttachment> gateEnableAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PitchShiftAudioProcessorEditor)
};
