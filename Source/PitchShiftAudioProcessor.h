#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

class PitchShiftAudioProcessor : public juce::AudioProcessor
{
public:
    PitchShiftAudioProcessor();
    ~PitchShiftAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Parámetros accesibles desde el DAW
    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    // DSP Pitch Shifting Circular Buffer
    std::vector<float> delayBufferL;
    std::vector<float> delayBufferR;
    int writePointer = 0;
    double phase0 = 0.0;
    double phase1 = 0.5;

    // Filtros de modelado Tight y Tone sin dependencias complejas
    float tightPrevInL = 0.0f, tightPrevInR = 0.0f;
    float tightPrevOutL = 0.0f, tightPrevOutR = 0.0f;
    float tonePrevOutL = 0.0f, tonePrevOutR = 0.0f;

    // Puerta de ruido
    float gateEnvelope = 0.0f;

    double currentSampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PitchShiftAudioProcessor)
};
