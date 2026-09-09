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
    // DSP Pitch Shifting Circular Buffer con WSOLA (Waveform Similarity Overlap-Add)
    std::vector<float> delayBufferL;
    std::vector<float> delayBufferR;
    int writePointer = 0;
    double phase0 = 0.0;
    double phase1 = 0.5;
    double delay0 = 400.0;
    double delay1 = 1200.0;

    // Filtros de corte 12 dB/octava (Butterworth Q = 0.7071) Direct Form II Transposed
    float lowCutZ1L = 0.0f, lowCutZ2L = 0.0f;
    float lowCutZ1R = 0.0f, lowCutZ2R = 0.0f;
    float highCutZ1L = 0.0f, highCutZ2L = 0.0f;
    float highCutZ1R = 0.0f, highCutZ2R = 0.0f;

    // Puerta de ruido
    float gateEnvelope = 0.0f;

    double currentSampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PitchShiftAudioProcessor)
};
