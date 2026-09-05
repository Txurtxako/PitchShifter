#include "PitchShiftAudioProcessor.h"
#include "PluginEditor.h"

PitchShiftAudioProcessor::PitchShiftAudioProcessor()
    : AudioProcessor (BusesProperties().withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Parameters", createParameterLayout())
{
}

PitchShiftAudioProcessor::~PitchShiftAudioProcessor()
{
}

juce::AudioProcessorValueTreeState::ParameterLayout PitchShiftAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Semitonos: -24 a +24 con paso de 1 semitono
    params.push_back (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { "semitones", 1 }, "Semitones", -24, 24, 0));

    // Centésimas (cents): -50 a +50
    params.push_back (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { "cents", 1 }, "Cents", -50, 50, 0));

    // Modo de instrumento: 0 = Guitarra, 1 = Bajo
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "mode", 1 }, "Instrument Mode",
        juce::StringArray { "Guitar", "Bass" }, 0));

    // Mezcla Dry/Wet: 0% a 100%
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "mix", 1 }, "Mix",
        juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f), 100.0f));

    // Tight Low-Cut: 20 Hz a 140 Hz
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "tightCut", 1 }, "Tight Low Cut",
        juce::NormalisableRange<float> (20.0f, 140.0f, 1.0f), 20.0f));

    // Tone High-Cut: 2000 Hz a 20000 Hz
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "toneCut", 1 }, "Tone High Cut",
        juce::NormalisableRange<float> (2000.0f, 20000.0f, 10.0f), 18000.0f));

    // Bypass
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "bypass", 1 }, "Bypass", false));

    // Noise Gate
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "gateEnable", 1 }, "Noise Gate", true));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "gateThreshold", 1 }, "Gate Threshold",
        juce::NormalisableRange<float> (-80.0f, -20.0f, 0.5f), -55.0f));

    return { params.begin(), params.end() };
}

void PitchShiftAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    // Buffer circular de 65536 muestras (~1.4 seg a 48kHz)
    const int bufferSize = 65536;
    delayBufferL.assign (bufferSize, 0.0f);
    delayBufferR.assign (bufferSize, 0.0f);
    writePointer = 0;
    phase0 = 0.0;
    phase1 = 0.5;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = (juce::uint32) samplesPerBlock;
    spec.numChannels = 2;

    tightLowCutFilter.reset();
    toneHighCutFilter.reset();
}

void PitchShiftAudioProcessor::releaseResources()
{
}

bool PitchShiftAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo()
        && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono())
        return false;

    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;

    return true;
}

void PitchShiftAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);

    juce::ScopedNoDenormals noDenormals;
    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    // Obtener valores de parámetros
    const int semitones = apvts.getRawParameterValue ("semitones")->load();
    const int cents = apvts.getRawParameterValue ("cents")->load();
    const int mode = (int) apvts.getRawParameterValue ("mode")->load();
    const float mix = apvts.getRawParameterValue ("mix")->load() / 100.0f;
    const bool bypass = apvts.getRawParameterValue ("bypass")->load() > 0.5f;
    const float tightHz = apvts.getRawParameterValue ("tightCut")->load();
    const float toneHz = apvts.getRawParameterValue ("toneCut")->load();
    const bool gateOn = apvts.getRawParameterValue ("gateEnable")->load() > 0.5f;
    const float gateThreshDb = apvts.getRawParameterValue ("gateThreshold")->load();

    if (bypass)
        return;

    // Si la transposición es cero y mix es 100%, señal transparente
    const double totalSemitones = semitones + (cents / 100.0);
    const double pitchRatio = std::pow (2.0, totalSemitones / 12.0);

    // Ajuste de tamaño de ventana: Modo Bajo (95ms) vs Modo Guitarra (42ms)
    const double grainSeconds = (mode == 1) ? 0.095 : 0.042;
    const int grainSize = (int)(currentSampleRate * grainSeconds);
    const double phaseRate = (1.0 - pitchRatio) / (double) grainSize;

    const int delayBufLen = (int) delayBufferL.size();
    auto* channelL = buffer.getWritePointer (0);
    auto* channelR = (numChannels > 1) ? buffer.getWritePointer (1) : channelL;

    for (int i = 0; i < numSamples; ++i)
    {
        const float inL = channelL[i];
        const float inR = channelR[i];

        // Puerta de ruido
        if (gateOn)
        {
            const float peak = std::max (std::abs (inL), std::abs (inR));
            const float peakDb = (peak > 0.00001f) ? 20.0f * std::log10 (peak) : -100.0f;
            const float targetGain = (peakDb >= gateThreshDb) ? 1.0f : 0.0f;
            gateEnvelope += (targetGain - gateEnvelope) * (targetGain > gateEnvelope ? 0.4f : 0.02f);
        }
        else
        {
            gateEnvelope = 1.0f;
        }

        const float gatedL = inL * gateEnvelope;
        const float gatedR = inR * gateEnvelope;

        // Escritura al buffer circular
        delayBufferL[writePointer] = gatedL;
        delayBufferR[writePointer] = gatedR;

        // Progreso de fases (0.0 a 1.0)
        phase0 += phaseRate;
        while (phase0 >= 1.0) phase0 -= 1.0;
        while (phase0 < 0.0)  phase0 += 1.0;

        phase1 += phaseRate;
        while (phase1 >= 1.0) phase1 -= 1.0;
        while (phase1 < 0.0)  phase1 += 1.0;

        const double delay0 = phase0 * grainSize;
        const double delay1 = phase1 * grainSize;

        // Ventana Hann suave
        const float w0 = 0.5f * (1.0f - std::cos (2.0f * juce::MathConstants<float>::pi * (float) phase0));
        const float w1 = 0.5f * (1.0f - std::cos (2.0f * juce::MathConstants<float>::pi * (float) phase1));

        // Lectura con interpolación lineal
        const double readPos0 = fmod (writePointer - delay0 + delayBufLen * 2, delayBufLen);
        const double readPos1 = fmod (writePointer - delay1 + delayBufLen * 2, delayBufLen);

        const int idx0A = (int) readPos0;
        const int idx0B = (idx0A + 1) % delayBufLen;
        const float frac0 = (float)(readPos0 - idx0A);

        const int idx1A = (int) readPos1;
        const int idx1B = (idx1A + 1) % delayBufLen;
        const float frac1 = (float)(readPos1 - idx1A);

        const float val0L = delayBufferL[idx0A] * (1.0f - frac0) + delayBufferL[idx0B] * frac0;
        const float val0R = delayBufferR[idx0A] * (1.0f - frac0) + delayBufferR[idx0B] * frac0;

        const float val1L = delayBufferL[idx1A] * (1.0f - frac1) + delayBufferL[idx1B] * frac1;
        const float val1R = delayBufferR[idx1A] * (1.0f - frac1) + delayBufferR[idx1B] * frac1;

        const float wetL = val0L * w0 + val1L * w1;
        const float wetR = val0R * w0 + val1R * w1;

        // Mezcla Dry/Wet con ley de potencia constante
        const float dryWeight = std::cos (mix * 0.5f * juce::MathConstants<float>::pi);
        const float wetWeight = std::sin (mix * 0.5f * juce::MathConstants<float>::pi);

        channelL[i] = inL * dryWeight + wetL * wetWeight;
        channelR[i] = inR * dryWeight + wetR * wetWeight;

        writePointer = (writePointer + 1) % delayBufLen;
    }
}

juce::AudioProcessorEditor* PitchShiftAudioProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor (*this);
}

bool PitchShiftAudioProcessor::hasEditor() const { return true; }
const juce::String PitchShiftAudioProcessor::getName() const { return "PitchShift VST3"; }
bool PitchShiftAudioProcessor::acceptsMidi() const { return false; }
bool PitchShiftAudioProcessor::producesMidi() const { return false; }
bool PitchShiftAudioProcessor::isMidiEffect() const { return false; }
double PitchShiftAudioProcessor::getTailLengthSeconds() const { return 0.0; }
int PitchShiftAudioProcessor::getNumPrograms() { return 1; }
int PitchShiftAudioProcessor::getCurrentProgram() { return 0; }
void PitchShiftAudioProcessor::setCurrentProgram (int) {}
const juce::String PitchShiftAudioProcessor::getProgramName (int) { return {}; }
void PitchShiftAudioProcessor::changeProgramName (int, const juce::String&) {}

void PitchShiftAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void PitchShiftAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState != nullptr)
        if (xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PitchShiftAudioProcessor();
}
