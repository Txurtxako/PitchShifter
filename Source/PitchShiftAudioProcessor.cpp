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
    readPos = 0.0;
    isSplicing = false;
    spliceFadeReadPos = 0.0;
    spliceProgress = 0;
    initializedReadPos = false;

    lowCutZ1L = lowCutZ2L = lowCutZ1R = lowCutZ2R = 0.0f;
    highCutZ1L = highCutZ2L = highCutZ1R = highCutZ2R = 0.0f;
    gateEnvelope = 0.0f;
    gateHoldCounter = 0;
    gateOpen = false;

    // Reportar latencia nominal al DAW (Compensación de retardo PDC en Cubase, Reaper, etc.)
    const int nominalLatency = (int) std::round (currentSampleRate * 0.016);
    setLatencySamples (nominalLatency);
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

    // Ajuste de límites operativos para flujo único sin chorus
    const bool isBass = (mode == 1);
    const int nominalDelay = (int) std::round (currentSampleRate * (isBass ? 0.035 : 0.016));
    const int windowSamples = (int) std::round (currentSampleRate * (isBass ? 0.030 : 0.014));
    const int minDelay = (int) std::round (currentSampleRate * (isBass ? 0.006 : 0.003));
    const int maxDelay = nominalDelay + windowSamples;
    const int searchRadius = isBass ? 128 : 64;

    const int delayBufLen = (int) delayBufferL.size();
    auto* channelL = buffer.getWritePointer (0);
    auto* channelR = (numChannels > 1) ? buffer.getWritePointer (1) : channelL;

    // Dinámica de puerta de ruido musical de estudio: 2ms ataque, 90ms caída exponencial natural, 25ms sostenimiento (hold), 5dB histéresis
    const float attackCoeff = 1.0f - std::exp (-1.0f / (float)(currentSampleRate * 0.002));
    const float releaseCoeff = 1.0f - std::exp (-1.0f / (float)(currentSampleRate * 0.090));
    const int holdSamples = (int)(currentSampleRate * 0.025);
    const float hysteresisDb = 5.0f;

    // Interpolador cúbico Hermite de 4 puntos (mantiene agudos nítidos sin aliasing ni filtrado paso bajo)
    auto hermiteInterpolate = [] (const std::vector<float>& buf, double pos, int len) noexcept -> float
    {
        const int i1 = (int) std::floor (pos);
        const float frac = (float)(pos - (double) i1);
        const int i0 = (i1 - 1 + len) % len;
        const int i2 = (i1 + 1) % len;
        const int i3 = (i1 + 2) % len;

        const float y0 = buf[i0];
        const float y1 = buf[((i1 % len) + len) % len];
        const float y2 = buf[i2];
        const float y3 = buf[i3];

        const float c0 = y1;
        const float c1 = 0.5f * (y2 - y0);
        const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);

        return ((c3 * frac + c2) * frac + c1) * frac + c0;
    };

    // Búsqueda de coincidencia de fase y paso por cero en micro-empalme
    auto findPhaseMatch = [&] (double currPos, double targetPos, int radius) noexcept -> double
    {
        const int currIdx = (int) std::floor (currPos);
        const int currPrev = (currIdx - 1 + delayBufLen) % delayBufLen;
        const float currVal = delayBufferL[((currIdx % delayBufLen) + delayBufLen) % delayBufLen];
        const float currSlope = currVal - delayBufferL[currPrev];

        const int targetIdx = (int) std::floor (targetPos);
        int bestIdx = targetIdx;
        float minScore = 1.0e12f;

        for (int offset = -radius; offset <= radius; ++offset)
        {
            const int candIdx = (targetIdx + offset + delayBufLen * 16) % delayBufLen;
            const int candPrev = (candIdx - 1 + delayBufLen) % delayBufLen;
            const float candVal = delayBufferL[candIdx];
            const float candSlope = candVal - delayBufferL[candPrev];

            const float slopeMismatch = (currSlope * candSlope < 0.0f) ? 2.0f : 0.0f;
            const float valDiff = std::abs (candVal - currVal);
            const float slopeDiff = std::abs (candSlope - currSlope);

            const float score = valDiff + slopeDiff * 2.0f + slopeMismatch;
            if (score < minScore)
            {
                minScore = score;
                bestIdx = candIdx;
            }
        }

        const double frac = currPos - (double) currIdx;
        return (double)((bestIdx + delayBufLen * 16) % delayBufLen) + frac;
    };

    for (int i = 0; i < numSamples; ++i)
    {
        // 1. Ganancia de entrada
        const float inL = channelL[i] * inGainLin;
        const float inR = channelR[i] * inGainLin;

        // 2. Puerta de ruido musical con histéresis y tiempo de sostenimiento (Hold Time)
        if (gateOn)
        {
            const float peak = std::max (std::abs (inL), std::abs (inR));
            const float peakDb = (peak > 0.00001f) ? 20.0f * std::log10 (peak) : -100.0f;

            if (peakDb >= gateThreshDb)
            {
                gateHoldCounter = holdSamples;
                gateOpen = true;
            }
            else if (peakDb < (gateThreshDb - hysteresisDb) && gateHoldCounter <= 0)
            {
                gateOpen = false;
            }

            if (gateHoldCounter > 0)
                gateHoldCounter--;

            const float targetGain = gateOpen ? 1.0f : 0.0f;
            const float coeff = (targetGain > gateEnvelope) ? attackCoeff : releaseCoeff;
            gateEnvelope += (targetGain - gateEnvelope) * coeff;
        }
        else
        {
            gateEnvelope = 1.0f;
            gateOpen = true;
            gateHoldCounter = 0;
        }

        const float sigL = inL * gateEnvelope;
        const float sigR = inR * gateEnvelope;

        // 3. Escritura al buffer circular (señal directa post-puerta)
        delayBufferL[writePointer] = sigL;
        delayBufferR[writePointer] = sigR;

        // 4. Inicializar puntero de lectura en primera ejecución
        if (!initializedReadPos)
        {
            readPos = (double)((writePointer - nominalDelay + delayBufLen * 16) % delayBufLen);
            initializedReadPos = true;
        }

        // 5. Control de flujo único y micro-empalme en paso por cero
        double currentDelay = std::fmod ((double) writePointer - readPos + (double)(delayBufLen * 16), (double) delayBufLen);

        if (!isSplicing)
        {
            if (pitchRatio < 0.9999 && currentDelay >= (double) maxDelay)
            {
                const double targetPos = std::fmod ((double) writePointer - (double) nominalDelay + (double)(delayBufLen * 16), (double) delayBufLen);
                const double matchedPos = findPhaseMatch (readPos, targetPos, searchRadius);
                isSplicing = true;
                spliceFadeReadPos = readPos;
                readPos = matchedPos;
                spliceProgress = 0;
            }
            else if (pitchRatio > 1.0001 && currentDelay <= (double) minDelay)
            {
                const double targetPos = std::fmod ((double) writePointer - (double) nominalDelay + (double)(delayBufLen * 16), (double) delayBufLen);
                const double matchedPos = findPhaseMatch (readPos, targetPos, searchRadius);
                isSplicing = true;
                spliceFadeReadPos = readPos;
                readPos = matchedPos;
                spliceProgress = 0;
            }
        }

        // 6. Lectura de señal procesada (Wet)
        float wetL = 0.0f;
        float wetR = 0.0f;

        if (isSplicing)
        {
            // Micro-cruce suave de 3ms en fase idéntica
            const float progress = (float) spliceProgress / (float) spliceLengthSamples;
            const float wOut = 0.5f * (1.0f + std::cos (juce::MathConstants<float>::pi * progress));
            const float wIn  = 0.5f * (1.0f - std::cos (juce::MathConstants<float>::pi * progress));

            const float oldL = hermiteInterpolate (delayBufferL, spliceFadeReadPos, delayBufLen);
            const float oldR = hermiteInterpolate (delayBufferR, spliceFadeReadPos, delayBufLen);
            const float newL = hermiteInterpolate (delayBufferL, readPos, delayBufLen);
            const float newR = hermiteInterpolate (delayBufferR, readPos, delayBufLen);

            wetL = oldL * wOut + newL * wIn;
            wetR = oldR * wOut + newR * wIn;

            spliceFadeReadPos = std::fmod (spliceFadeReadPos + pitchRatio + (double)(delayBufLen * 16), (double) delayBufLen);
            spliceProgress++;
            if (spliceProgress >= spliceLengthSamples)
                isSplicing = false;
        }
        else
        {
            // 98% DEL TIEMPO: UN SOLO CANAL ACTIVO. CERO CHORUS, CERO DETUNER, TONO 100% PURO
            wetL = hermiteInterpolate (delayBufferL, readPos, delayBufLen);
            wetR = hermiteInterpolate (delayBufferR, readPos, delayBufLen);
        }

        // Avance continuo del puntero principal a velocidad de pitch
        readPos = std::fmod (readPos + pitchRatio + (double)(delayBufLen * 16), (double) delayBufLen);

        // 7. Filtros Corte de Graves y Corte de Agudos (APLICADOS EXCLUSIVAMENTE A LA SEÑAL WET, EL DRY QUEDA INTACTO)
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

        // 8. COMPENSACIÓN DE LATENCIA DRY / WET
        const double dryReadPos = std::fmod ((double) writePointer - (double) nominalDelay + (double)(delayBufLen * 16), (double) delayBufLen);
        const float dryL = hermiteInterpolate (delayBufferL, dryReadPos, delayBufLen);
        const float dryR = hermiteInterpolate (delayBufferR, dryReadPos, delayBufLen);

        // 9. Mezcla Dry/Wet con ley de potencia constante
        const float dryWeight = std::cos (mix * 0.5f * juce::MathConstants<float>::pi);
        const float wetWeight = std::sin (mix * 0.5f * juce::MathConstants<float>::pi);

        float mixedL = dryL * dryWeight + wetL * wetWeight;
        float mixedR = dryR * dryWeight + wetR * wetWeight;

        // 10. Ganancia de salida y asignación al buffer
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
