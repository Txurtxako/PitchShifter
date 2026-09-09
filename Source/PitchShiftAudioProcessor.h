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
    // DSP Pitch Shifting: Single-Stream Zero-Crossing Phase-Matched Splicing (sin chorus ni detuner)
    std::vector<float> delayBufferL;
    std::vector<float> delayBufferR;
    int writePointer = 0;
    double readPos = 0.0;
    bool isSplicing = false;
    double spliceFadeReadPos = 0.0;
    int spliceProgress = 0;
    int spliceLengthSamples = 144;
    bool initializedReadPos = false;

    // Filtros de corte 12 dB/octava (Butterworth Q = 0.7071) Direct Form II Transposed
    float lowCutZ1L = 0.0f, lowCutZ2L = 0.0f;
    float lowCutZ1R = 0.0f, lowCutZ2R = 0.0f;
    float highCutZ1L = 0.0f, highCutZ2L = 0.0f;
    float highCutZ1R = 0.0f, highCutZ2R = 0.0f;

    // Puerta de ruido con histéresis y tiempo de sostenimiento (Hold Time)
    float gateEnvelope = 0.0f;
    int gateHoldCounter = 0;
    bool gateOpen = false;

    double currentSampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PitchShiftAudioProcessor)
};
