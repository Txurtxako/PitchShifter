#include "PitchShiftAudioProcessor.h"
#include "PitchShiftAudioProcessorEditor.h"

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

    // Ganancia de entrada: -24 dB a +24 dB
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "inputGain", 1 }, "Input Gain",
        juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f));

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

    // Corte de Graves (Low Cut): 0 Hz a 20 kHz (12 dB/octava Butterworth)
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "lowCut", 1 }, "Corte Graves (Low Cut)",
        juce::NormalisableRange<float> (0.0f, 20000.0f, 1.0f, 0.35f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "tightCut", 1 }, "Tight Low Cut (Legacy)",
        juce::NormalisableRange<float> (0.0f, 20000.0f, 1.0f, 0.35f), 0.0f));

    // Corte de Agudos (High Cut): 20 kHz a 0 Hz (12 dB/octava Butterworth)
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "highCut", 1 }, "Corte Agudos (High Cut)",
        juce::NormalisableRange<float> (0.0f, 20000.0f, 1.0f, 0.35f), 20000.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "toneCut", 1 }, "Tone High Cut (Legacy)",
        juce::NormalisableRange<float> (0.0f, 20000.0f, 1.0f, 0.35f), 20000.0f));

    // Puerta de Ruido
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "gateEnable", 1 }, "Noise Gate", true));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "gateThreshold", 1 }, "Gate Threshold",
        juce::NormalisableRange<float> (-80.0f, -20.0f, 0.5f), -55.0f));

    // Ganancia de salida: -24 dB a +24 dB
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "outputGain", 1 }, "Output Gain",
        juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f));

    // Bypass
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "bypass", 1 }, "Bypass", false));

    return { params.begin(), params.end() };
}

void PitchShiftAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused (samplesPerBlock);
    currentSampleRate = sampleRate;

    // Buffer circular de 65536 muestras (~1.4 seg a 48kHz)
    const int bufferSize = 65536;
    delayBufferL.assign (bufferSize, 0.0f);
    delayBufferR.assign (bufferSize, 0.0f);
    writePointer = 0;
    phase0 = 0.0;
    phase1 = 0.5;
    delay0 = 400.0;
    delay1 = 1200.0;

    lowCutZ1L = lowCutZ2L = lowCutZ1R = lowCutZ2R = 0.0f;
    highCutZ1L = highCutZ2L = highCutZ1R = highCutZ2R = 0.0f;
    gateEnvelope = 0.0f;
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
    const int semitones = (int) apvts.getRawParameterValue ("semitones")->load();
    const int cents = (int) apvts.getRawParameterValue ("cents")->load();
    const int mode = (int) apvts.getRawParameterValue ("mode")->load();
    const float mix = apvts.getRawParameterValue ("mix")->load() / 100.0f;
    const bool bypass = apvts.getRawParameterValue ("bypass")->load() > 0.5f;

    float inGainDb = 0.0f;
    if (auto* p = apvts.getRawParameterValue ("inputGain")) inGainDb = p->load();
    const float inGainLin = std::pow (10.0f, inGainDb / 20.0f);

    float outGainDb = 0.0f;
    if (auto* p = apvts.getRawParameterValue ("outputGain")) outGainDb = p->load();
    const float outGainLin = std::pow (10.0f, outGainDb / 20.0f);

    float lowCutHz = 0.0f;
    if (auto* p = apvts.getRawParameterValue ("lowCut")) lowCutHz = p->load();
    else if (auto* p = apvts.getRawParameterValue ("tightCut")) lowCutHz = p->load();

    float highCutHz = 20000.0f;
    if (auto* p = apvts.getRawParameterValue ("highCut")) highCutHz = p->load();
    else if (auto* p = apvts.getRawParameterValue ("toneCut")) highCutHz = p->load();

    const bool gateOn = apvts.getRawParameterValue ("gateEnable")->load() > 0.5f;
    const float gateThreshDb = apvts.getRawParameterValue ("gateThreshold")->load();

    if (bypass)
        return;

    // Coeficientes filtro Corte Graves (12 dB/octava Butterworth High-Pass)
    const bool hasLowCut = (lowCutHz > 10.0f);
    float lowB0 = 1.0f, lowB1 = 0.0f, lowB2 = 0.0f, lowA1 = 0.0f, lowA2 = 0.0f;
    if (hasLowCut)
    {
        const float fc = juce::jlimit (10.0f, (float)(currentSampleRate * 0.495), lowCutHz);
        const float w0 = 2.0f * juce::MathConstants<float>::pi * fc / (float) currentSampleRate;
        const float cosw0 = std::cos (w0);
        const float sinw0 = std::sin (w0);
        const float alpha = sinw0 / (2.0f * 0.70710678f);
        const float a0 = 1.0f + alpha;

        lowB0 = ((1.0f + cosw0) * 0.5f) / a0;
        lowB1 = (-(1.0f + cosw0)) / a0;
        lowB2 = ((1.0f + cosw0) * 0.5f) / a0;
        lowA1 = (-2.0f * cosw0) / a0;
        lowA2 = (1.0f - alpha) / a0;
    }

    // Coeficientes filtro Corte Agudos (12 dB/octava Butterworth Low-Pass)
    const bool hasHighCut = (highCutHz < 19990.0f);
    float highB0 = 1.0f, highB1 = 0.0f, highB2 = 0.0f, highA1 = 0.0f, highA2 = 0.0f;
    if (hasHighCut)
    {
        const float fc = juce::jlimit (10.0f, (float)(currentSampleRate * 0.495), highCutHz);
        const float w0 = 2.0f * juce::MathConstants<float>::pi * fc / (float) currentSampleRate;
        const float cosw0 = std::cos (w0);
        const float sinw0 = std::sin (w0);
        const float alpha = sinw0 / (2.0f * 0.70710678f);
        const float a0 = 1.0f + alpha;

        highB0 = ((1.0f - cosw0) * 0.5f) / a0;
        highB1 = (1.0f - cosw0) / a0;
        highB2 = ((1.0f - cosw0) * 0.5f) / a0;
        highA1 = (-2.0f * cosw0) / a0;
        highA2 = (1.0f - alpha) / a0;
    }

    const double totalSemitones = (double) semitones + ((double) cents / 100.0);
    const double pitchRatio = std::pow (2.0, totalSemitones / 12.0);

    // Ajuste de tamaño de ventana: Modo Bajo (95ms) vs Modo Guitarra (42ms)
    const bool isBass = (mode == 1);
    const double grainSeconds = isBass ? 0.095 : 0.042;
    const int grainSize = (int)(currentSampleRate * grainSeconds);

    const double phaseRate = 1.0 / (double) grainSize;
    const double delayDelta = 1.0 - pitchRatio;

    const int delayBufLen = (int) delayBufferL.size();
    auto* channelL = buffer.getWritePointer (0);
    auto* channelR = (numChannels > 1) ? buffer.getWritePointer (1) : channelL;

    // Función de ventana Blackman-Harris de 4 términos (-92 dB de rechazo de lóbulos para eliminar ondulación y batido)
    auto blackmanHarris = [] (double phase) noexcept -> float
    {
        if (phase <= 0.0 || phase >= 1.0) return 0.0f;
        const double theta = 2.0 * juce::MathConstants<double>::pi * phase;
        const double w = 0.35875 - 0.48829 * std::cos (theta)
                                 + 0.14128 * std::cos (2.0 * theta)
                                 - 0.01168 * std::cos (3.0 * theta);
        return (float) std::max (0.0, w);
    };

    // Interpolador cúbico Hermite de 4 puntos (mantiene agudos nítidos sin aliasing ni filtrado paso bajo)
    auto hermiteInterpolate = [] (const std::vector<float>& buf, double pos, int len) noexcept -> float
    {
        const int i1 = (int) pos;
        const float frac = (float)(pos - (double) i1);
        const int i0 = (i1 - 1 + len) % len;
        const int i2 = (i1 + 1) % len;
        const int i3 = (i1 + 2) % len;

        const float y0 = buf[i0];
        const float y1 = buf[i1 % len];
        const float y2 = buf[i2];
        const float y3 = buf[i3];

        const float c0 = y1;
        const float c1 = 0.5f * (y2 - y0);
        const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);

        return ((c3 * frac + c2) * frac + c1) * frac + c0;
    };

    // Cálculo de retardo nominal para el grano que reinicia
    auto calcNominalDelay = [] (double otherDelay, int gSize, double pRatio, int bLen) noexcept -> double
    {
        const double minDelay = 64.0;
        const double maxDelay = (double)(bLen - gSize - 256);

        double target = otherDelay - ((double) gSize * 0.5 * (1.0 - pRatio));
        if (target < minDelay)
            target = minDelay + std::abs ((double) gSize * 0.25);
        else if (target > maxDelay)
            target = maxDelay - std::abs ((double) gSize * 0.25);
        return target;
    };

    // Búsqueda de alineación de fase por correlación WSOLA (Waveform Similarity Overlap-Add)
    // Encuentra el desfase exacto donde la forma de onda del grano entrante coincide en fase con el grano activo
    auto findWsolaAlignment = [&] (double nominalTarget, double activeDelay, int bLen, bool bassMode) noexcept -> double
    {
        const int searchRadius = bassMode ? 160 : 96;
        const int templateLen = 32;

        const int refPos = ((writePointer - (int) activeDelay) % bLen + bLen * 4) % bLen;
        int bestDelta = 0;
        float minDifference = 1.0e12f;

        const int minOffset = std::max (64, (int) std::floor (nominalTarget - (double) searchRadius));
        const int maxOffset = std::min (bLen - templateLen - 64, (int) std::floor (nominalTarget + (double) searchRadius));

        for (int candOffset = minOffset; candOffset <= maxOffset; candOffset += 2)
        {
            const int candPos = ((writePointer - candOffset) % bLen + bLen * 4) % bLen;
            float diff = 0.0f;

            for (int m = 0; m < templateLen; m += 2)
            {
                const float sRef = delayBufferL[(refPos - m + bLen) % bLen];
                const float sCand = delayBufferL[(candPos - m + bLen) % bLen];
                const float d = sRef - sCand;
                diff += d * d;
            }

            if (diff < minDifference)
            {
                minDifference = diff;
                bestDelta = candOffset - (int) nominalTarget;
            }
        }

        return nominalTarget + (double) bestDelta;
    };

    for (int i = 0; i < numSamples; ++i)
    {
        // 1. Ganancia de entrada
        const float inL = channelL[i] * inGainLin;
        const float inR = channelR[i] * inGainLin;

        // 2. Puerta de ruido
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

        float sigL = inL * gateEnvelope;
        float sigR = inR * gateEnvelope;

        // 3. Escritura al buffer circular (señal directa post-puerta)
        delayBufferL[writePointer] = sigL;
        delayBufferR[writePointer] = sigR;

        // 4. Avance de retardos de grano y fases WSOLA
        delay0 += delayDelta;
        delay1 += delayDelta;

        phase0 += phaseRate;
        phase1 += phaseRate;

        // Reset y alineación de fase por correlación para Grano 0
        if (phase0 >= 1.0)
        {
            phase0 -= 1.0;
            const double nominalTarget = calcNominalDelay (delay1, grainSize, pitchRatio, delayBufLen);
            delay0 = findWsolaAlignment (nominalTarget, delay1, delayBufLen, isBass);
        }

        // Reset y alineación de fase por correlación para Grano 1
        if (phase1 >= 1.0)
        {
            phase1 -= 1.0;
            const double nominalTarget = calcNominalDelay (delay0, grainSize, pitchRatio, delayBufLen);
            delay1 = findWsolaAlignment (nominalTarget, delay0, delayBufLen, isBass);
        }

        // 5. Ponderación con ventana Blackman-Harris de 4 términos (-92 dB sidelobes)
        const float rawW0 = blackmanHarris (phase0);
        const float rawW1 = blackmanHarris (phase1);
        const float sumW = rawW0 + rawW1 + 1.0e-9f;
        const float w0 = rawW0 / sumW;
        const float w1 = rawW1 / sumW;

        // 6. Lectura de muestras con interpolación cúbica Hermite de 4 puntos
        const double readPos0 = std::fmod ((double) writePointer - delay0 + (double)(delayBufLen * 4), (double) delayBufLen);
        const double readPos1 = std::fmod ((double) writePointer - delay1 + (double)(delayBufLen * 4), (double) delayBufLen);

        const float val0L = hermiteInterpolate (delayBufferL, readPos0, delayBufLen);
        const float val0R = hermiteInterpolate (delayBufferR, readPos0, delayBufLen);
        const float val1L = hermiteInterpolate (delayBufferL, readPos1, delayBufLen);
        const float val1R = hermiteInterpolate (delayBufferR, readPos1, delayBufLen);

        // 7. Suma solapada en fase perfecta: sin filtro de peine ni modulación chorus
        float wetL = val0L * w0 + val1L * w1;
        float wetR = val0R * w0 + val1R * w1;

        // 4. Filtros Corte de Graves y Corte de Agudos (APLICADOS EXCLUSIVAMENTE A LA SEÑAL WET, EL DRY QUEDA INTACTO)
        if (hasLowCut)
        {
            float yL = lowB0 * wetL + lowCutZ1L;
            lowCutZ1L = lowB1 * wetL - lowA1 * yL + lowCutZ2L;
            lowCutZ2L = lowB2 * wetL - lowA2 * yL;
            if (std::abs (lowCutZ1L) < 1.0e-15f) lowCutZ1L = 0.0f;
            if (std::abs (lowCutZ2L) < 1.0e-15f) lowCutZ2L = 0.0f;
            wetL = yL;

            float yR = lowB0 * wetR + lowCutZ1R;
            lowCutZ1R = lowB1 * wetR - lowA1 * yR + lowCutZ2R;
            lowCutZ2R = lowB2 * wetR - lowA2 * yR;
            if (std::abs (lowCutZ1R) < 1.0e-15f) lowCutZ1R = 0.0f;
            if (std::abs (lowCutZ2R) < 1.0e-15f) lowCutZ2R = 0.0f;
            wetR = yR;
        }

        if (hasHighCut)
        {
            float yL = highB0 * wetL + highCutZ1L;
            highCutZ1L = highB1 * wetL - highA1 * yL + highCutZ2L;
            highCutZ2L = highB2 * wetL - highA2 * yL;
            if (std::abs (highCutZ1L) < 1.0e-15f) highCutZ1L = 0.0f;
            if (std::abs (highCutZ2L) < 1.0e-15f) highCutZ2L = 0.0f;
            wetL = yL;

            float yR = highB0 * wetR + highCutZ1R;
            highCutZ1R = highB1 * wetR - highA1 * yR + highCutZ2R;
            highCutZ2R = highB2 * wetR - highA2 * yR;
            if (std::abs (highCutZ1R) < 1.0e-15f) highCutZ1R = 0.0f;
            if (std::abs (highCutZ2R) < 1.0e-15f) highCutZ2R = 0.0f;
            wetR = yR;
        }

        // 5. Mezcla Dry/Wet con ley de potencia constante (Dry sin alteración espectral)
        const float dryWeight = std::cos (mix * 0.5f * juce::MathConstants<float>::pi);
        const float wetWeight = std::sin (mix * 0.5f * juce::MathConstants<float>::pi);

        float mixedL = sigL * dryWeight + wetL * wetWeight;
        float mixedR = sigR * dryWeight + wetR * wetWeight;

        // 6. Ganancia de salida y asignación al buffer
        channelL[i] = mixedL * outGainLin;
        channelR[i] = mixedR * outGainLin;

        writePointer = (writePointer + 1) % delayBufLen;
    }
}

juce::AudioProcessorEditor* PitchShiftAudioProcessor::createEditor()
{
    return new PitchShiftAudioProcessorEditor (*this);
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
