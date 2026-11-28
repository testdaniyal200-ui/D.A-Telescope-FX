#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <algorithm>
#include <cmath>

namespace
{
constexpr double twoPi = juce::MathConstants<double>::twoPi;
float percent (const juce::AudioProcessorValueTreeState& state, const char* id) noexcept
{
    if (auto* value = state.getRawParameterValue (id))
        return juce::jlimit (0.0f, 1.0f, value->load() * 0.01f);
    return 0.0f;
}
int choice (const juce::AudioProcessorValueTreeState& state, const char* id) noexcept
{
    if (auto* value = state.getRawParameterValue (id))
        return juce::jlimit (0, 4, static_cast<int> (std::lround (value->load())));
    return 0;
}
// One-pole low-pass, stateless helper used by the tone filter.
float onePolerImpl (float& state, float input, float coefficient) noexcept
{
    state += coefficient * (input - state);
    return state;
}
}

juce::AudioProcessorValueTreeState::ParameterLayout DATelescopeAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    const auto f = [&layout] (const char* id, const char* name, float maximum, float defaultValue)
    {
        layout.add (std::make_unique<juce::AudioParameterFloat>
            (juce::ParameterID { id, 1 }, name,
             juce::NormalisableRange<float> (0.0f, maximum, 0.1f), defaultValue));
    };
    f ("zoom", "Zoom", 100.0f, 72.0f);
    f ("spread", "Spread", 100.0f, 58.0f);
    f ("distance", "Distance", 100.0f, 63.0f);
    f ("motion", "Motion", 100.0f, 41.0f);
    f ("mix", "Wet Mix", 100.0f, 45.0f);
    f ("feedback", "Feedback", 85.0f, 32.0f);
    f ("tone", "Tone", 100.0f, 65.0f);
    const auto addChoice = [&layout] (const char* id, const char* name,
                                      juce::StringArray labels, int initial)
    {
        layout.add (std::make_unique<juce::AudioParameterChoice>
            (juce::ParameterID { id, 1 }, name, std::move (labels), initial));
    };
    addChoice ("mode", "Engine", { "Telescope", "Orbit", "Warp", "Dimension", "Atmosphere" }, 0);
    addChoice ("zoomProfile", "Zoom Profile", { "Close", "Balanced", "Far" }, 2);
    addChoice ("spreadProfile", "Spread Profile", { "Mono", "Wide", "Galactic" }, 2);
    addChoice ("distanceProfile", "Distance Profile", { "Near", "Mid", "Deep" }, 2);
    addChoice ("motionProfile", "Motion Profile", { "Static", "Orbit", "Warp" }, 2);
    return layout;
}

DATelescopeAudioProcessor::DATelescopeAudioProcessor()
    : juce::AudioProcessor (BusesProperties()
        .withInput ("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      parameters (*this, nullptr, "DA_Telescope_State", createParameterLayout())
{
}

bool DATelescopeAudioProcessor::isBusesLayoutSupported (const BusesLayout& layout) const
{
    const auto& in = layout.getMainInputChannelSet();
    const auto& out = layout.getMainOutputChannelSet();
    return in == out && (in == juce::AudioChannelSet::mono()
                         || in == juce::AudioChannelSet::stereo());
}

void DATelescopeAudioProcessor::resetEffects()
{
    for (auto& buffer : delayBuffers)
        std::fill (buffer.begin(), buffer.end(), 0.0f);
    toneState = { 0.0f, 0.0f };
    hpState = { 0.0f, 0.0f };
    for (auto& s : smooth)
    {
        s.reset (activeSampleRate, 0.02);
    }
    // Snap to the current parameter values so the very first block does not ramp from zero.
    smooth[0].setCurrentAndTargetValue (percent (parameters, "zoom"));
    smooth[1].setCurrentAndTargetValue (percent (parameters, "spread"));
    smooth[2].setCurrentAndTargetValue (percent (parameters, "distance"));
    smooth[3].setCurrentAndTargetValue (percent (parameters, "motion"));
    smooth[4].setCurrentAndTargetValue (percent (parameters, "mix"));
    smooth[5].setCurrentAndTargetValue (juce::jlimit (0.0f, 0.85f, percent (parameters, "feedback")));
    smooth[6].setCurrentAndTargetValue (percent (parameters, "tone"));
    writePosition = 0;
    lfoPhase = 0.0;
    lfoPhase2 = 0.0;
    reverb.reset();
}

void DATelescopeAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    activeSampleRate = std::max (8000.0, sampleRate);
    delayBufferLength = std::max (4096, static_cast<int> (std::ceil (activeSampleRate * 4.0)));
    for (auto& buffer : delayBuffers)
        buffer.resize (static_cast<size_t> (delayBufferLength));
    wetScratch.setSize (2, std::max (1, samplesPerBlock), false, true, true);
    reverb.setSampleRate (activeSampleRate);
    resetEffects();
}

void DATelescopeAudioProcessor::releaseResources()
{
    wetScratch.setSize (0, 0);
}

float DATelescopeAudioProcessor::readDelay (int channel, float delaySamples) const noexcept
{
    // Interpolated ring-buffer read. One delay line per channel, preallocated in prepareToPlay.
    const auto& ring = delayBuffers[static_cast<size_t> (channel)];
    if (ring.empty()) return 0.0f;
    float at = static_cast<float> (writePosition) - delaySamples;
    while (at < 0.0f) at += static_cast<float> (delayBufferLength);
    while (at >= static_cast<float> (delayBufferLength)) at -= static_cast<float> (delayBufferLength);
    const int first = static_cast<int> (at);
    const int second = (first + 1) % delayBufferLength;
    const float frac = at - static_cast<float> (first);
    return ring[static_cast<size_t> (first)] * (1.0f - frac)
           + ring[static_cast<size_t> (second)] * frac;
}

float DATelescopeAudioProcessor::onePoler (int channel, float input, float coefficient) noexcept
{
    return onePolerImpl (hpState[static_cast<size_t> (channel)], input, coefficient);
}

void DATelescopeAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const auto numChannels = buffer.getNumChannels();
    const int sampleCount = buffer.getNumSamples();
    if (numChannels == 0 || sampleCount == 0) return;
    if (delayBuffers[0].empty() || wetScratch.getNumSamples() == 0) return;

    // ---- Smooth the control parameters so preset changes and automation never click. ----
    smooth[0].setTargetValue (percent (parameters, "zoom"));
    smooth[1].setTargetValue (percent (parameters, "spread"));
    smooth[2].setTargetValue (percent (parameters, "distance"));
    smooth[3].setTargetValue (percent (parameters, "motion"));
    smooth[4].setTargetValue (percent (parameters, "mix"));
    smooth[5].setTargetValue (juce::jlimit (0.0f, 0.85f, percent (parameters, "feedback")));
    smooth[6].setTargetValue (percent (parameters, "tone"));

    const float zoom = smooth[0].getCurrentValue();
    const float spread = smooth[1].getCurrentValue();
    const float distance = smooth[2].getCurrentValue();
    const float motion = smooth[3].getCurrentValue();
    const float mix = smooth[4].getCurrentValue();
    const float feedback = smooth[5].getCurrentValue();
    const float tone = smooth[6].getCurrentValue();
    const int mode = choice (parameters, "mode");
    const int zoomProfile = choice (parameters, "zoomProfile");
    const int spreadProfile = choice (parameters, "spreadProfile");
    const int distanceProfile = choice (parameters, "distanceProfile");
    const int motionProfile = choice (parameters, "motionProfile");

    // ---- Time / filter / spatial coefficients derived from the parameters. ----
    const float distanceFactor = distance * (distanceProfile == 2 ? 1.25f : distanceProfile == 0 ? 0.65f : 1.0f);
    const float zoomFactor = zoom * (zoomProfile == 2 ? 1.3f : zoomProfile == 0 ? 0.7f : 1.0f);
    float delaySec = 0.045f + 0.22f * zoomFactor + 0.33f * distanceFactor;
    if (mode == 2) delaySec *= 0.62f;
    if (mode == 3) delaySec *= 0.30f;
    if (mode == 4) delaySec *= 1.12f;

    // Two independent LFOs drive the ping-pong taps for a rich, moving echo field.
    const float modulationDepth = motion * (motionProfile == 0 ? 0.0f : motionProfile == 2 ? 1.45f : 0.80f);
    const double lfoHz = (mode == 1 ? 0.15 : 0.07) + 2.8 * motion * motion;
    const double lfo2Hz = lfoHz * 1.7 + 0.05;

    // Tone: one-pole low-pass on the delay readback (0 = dark, 1 = bright).
    const float filterCoefficient = 0.006f + 0.55f * tone * tone;
    // A gentle high-pass on the wet signal keeps low-end clean for wide presets.
    const float hpCoefficient = 0.10f + 0.55f * tone;

    float stereoWidth = 0.30f + 1.8f * spread;
    if (spreadProfile == 0) stereoWidth = 0.0f;
    if (spreadProfile == 1) stereoWidth = std::min (1.2f, stereoWidth);
    if (spreadProfile == 2) stereoWidth = std::min (2.25f, stereoWidth * 1.30f);

    juce::Reverb::Parameters reverbParams;
    reverbParams.roomSize = juce::jlimit (0.0f, 0.98f, 0.15f + distanceFactor * 0.67f + (mode == 4 ? 0.18f : 0.0f));
    reverbParams.damping = 0.12f + (1.0f - tone) * 0.68f;
    reverbParams.wetLevel = mode == 4 ? 0.92f : mode == 3 ? 0.55f : 0.47f;
    reverbParams.dryLevel = 1.0f - reverbParams.wetLevel;
    reverbParams.width = stereoWidth > 0.1f ? 1.0f : 0.0f;
    reverbParams.freezeMode = 0.0f;
    reverb.setParameters (reverbParams);

    float maxInput = 0.0f, maxOutput = 0.0f;
    const int scratchSize = wetScratch.getNumSamples();
    for (int start = 0; start < sampleCount; start += scratchSize)
    {
        const int count = std::min (scratchSize, sampleCount - start);
        float* wetL = wetScratch.getWritePointer (0);
        float* wetR = wetScratch.getWritePointer (1);
        const float* inputL = buffer.getReadPointer (0, start);
        const float* inputR = buffer.getReadPointer (numChannels > 1 ? 1 : 0, start);

        for (int i = 0; i < count; ++i)
        {
            const float inL = inputL[i];
            const float inR = inputR[i];
            maxInput = std::max (maxInput, std::max (std::abs (inL), std::abs (inR)));

            const float sine = static_cast<float> (std::sin (lfoPhase));
            const float cosine = static_cast<float> (std::cos (lfoPhase));
            const float sine2 = static_cast<float> (std::sin (lfoPhase2));
            const float cosine2 = static_cast<float> (std::cos (lfoPhase2));

            // Modulated, slightly different delay times for each tap -> spatial smear.
            const float wobble = (mode == 2 ? 0.023f : mode == 3 ? 0.009f : 0.006f) * modulationDepth;
            const float base = delaySec * static_cast<float> (activeSampleRate);
            const float offsetL = juce::jlimit (2.0f, static_cast<float> (delayBufferLength - 2),
                                               base + wobble * static_cast<float> (activeSampleRate) * sine);
            const float offsetR = juce::jlimit (2.0f, static_cast<float> (delayBufferLength - 2),
                                               base * 1.071f + wobble * static_cast<float> (activeSampleRate) * cosine);
            const float offsetL2 = juce::jlimit (2.0f, static_cast<float> (delayBufferLength - 2),
                                                base * 0.53f + wobble * static_cast<float> (activeSampleRate) * sine2);
            const float offsetR2 = juce::jlimit (2.0f, static_cast<float> (delayBufferLength - 2),
                                                base * 0.58f + wobble * static_cast<float> (activeSampleRate) * cosine2);

            // Ping-pong: each tap reads the *other* channel's delay line, then
            // feedback is written back cross-coupled. This yields a true bouncing
            // stereo echo instead of two independent mono delays.
            const float echoL  = readDelay (0, offsetL)  * 0.62f + readDelay (1, offsetR2) * 0.38f;
            const float echoR  = readDelay (1, offsetR)  * 0.62f + readDelay (0, offsetL2) * 0.38f;
            toneState[0] += filterCoefficient * (echoL - toneState[0]);
            toneState[1] += filterCoefficient * (echoR - toneState[1]);

            // Write cross-coupled: left tap fed by right echo, right tap by left echo.
            delayBuffers[0][static_cast<size_t> (writePosition)] = inL + (echoR * feedback);
            delayBuffers[1][static_cast<size_t> (writePosition)] = inR + (echoL * feedback);
            if (++writePosition >= delayBufferLength) writePosition = 0;

            // Filtered, mode-shaped wet signal.
            float left  = onePoler (0, 0.72f * toneState[0] + 0.28f * inL, hpCoefficient);
            float right = onePoler (1, 0.72f * toneState[1] + 0.28f * inR, hpCoefficient);
            if (mode == 1) // Orbit: rotating wet-field pan.
            {
                const float leftGain = 0.65f + 0.35f * sine;
                const float rightGain = 0.65f - 0.35f * sine;
                left *= leftGain; right *= rightGain;
            }
            else if (mode == 2) // Warp: cross-coupled moving delays.
            {
                left = 0.75f * toneState[0] + 0.25f * toneState[1];
                right = 0.75f * toneState[1] + 0.25f * toneState[0];
            }
            else if (mode == 3) // Dimension: short stereo modulated taps.
            {
                left = 0.65f * toneState[0] + 0.35f * inL;
                right = 0.65f * toneState[1] + 0.35f * inR;
            }
            wetL[i] = left;
            wetR[i] = right;
            lfoPhase  += twoPi * lfoHz  / activeSampleRate;
            lfoPhase2 += twoPi * lfo2Hz / activeSampleRate;
            if (lfoPhase  >= twoPi) lfoPhase  -= twoPi;
            if (lfoPhase2 >= twoPi) lfoPhase2 -= twoPi;
        }

        reverb.processStereo (wetL, wetR, count);
        for (int i = 0; i < count; ++i)
        {
            // Mid/side stereo widening on the reverb return.
            float mid  = 0.5f * (wetL[i] + wetR[i]);
            float side = 0.5f * (wetL[i] - wetR[i]) * stereoWidth;
            const float wetLeft  = mid + side;
            const float wetRight = mid - side;
            const float dryLeft  = buffer.getSample (0, start + i);
            const float dryRight = buffer.getSample (numChannels > 1 ? 1 : 0, start + i);
            const float summedL = dryLeft * (1.0f - mix) + wetLeft * mix;
            const float summedR = dryRight * (1.0f - mix) + wetRight * mix;
            // Soft saturation limits peaks created by very wide / feedback-heavy presets.
            const float outL = mix <= 0.0001f ? dryLeft : std::tanh (summedL * 1.05f);
            const float outR = mix <= 0.0001f ? dryRight : std::tanh (summedR * 1.05f);
            buffer.setSample (0, start + i, outL);
            if (numChannels > 1) buffer.setSample (1, start + i, outR);
            maxOutput = std::max (maxOutput, std::max (std::abs (outL), std::abs (outR)));
        }
    }
    inputPeak.store (maxInput, std::memory_order_relaxed);
    outputPeak.store (maxOutput, std::memory_order_relaxed);
}

juce::AudioProcessorEditor* DATelescopeAudioProcessor::createEditor()
{
    return new DATelescopeAudioProcessorEditor (*this);
}

const juce::String DATelescopeAudioProcessor::getProgramName (int index)
{
    return juce::isPositiveAndBelow (index, getNumPrograms())
        ? juce::String (DA::factoryPresets[static_cast<size_t> (index)].name)
        : juce::String();
}

void DATelescopeAudioProcessor::setCurrentProgram (int index)
{
    if (! juce::isPositiveAndBelow (index, getNumPrograms())) return;
    const auto& preset = DA::factoryPresets[static_cast<size_t> (index)];
    const auto setValue = [this] (const char* id, float value)
    {
        if (auto* parameter = parameters.getParameter (id))
        {
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
            parameter->endChangeGesture();
        }
    };
    setValue ("zoom", preset.zoom);
    setValue ("spread", preset.spread);
    setValue ("distance", preset.distance);
    setValue ("motion", preset.motion);
    setValue ("mix", preset.mix);
    setValue ("feedback", preset.feedback);
    setValue ("tone", preset.tone);
    setValue ("mode", static_cast<float> (preset.mode));
    setValue ("zoomProfile", static_cast<float> (preset.zoomProfile));
    setValue ("spreadProfile", static_cast<float> (preset.spreadProfile));
    setValue ("distanceProfile", static_cast<float> (preset.distanceProfile));
    setValue ("motionProfile", static_cast<float> (preset.motionProfile));
    currentProgram.store (index);
}

void DATelescopeAudioProcessor::getStateInformation (juce::MemoryBlock& destination)
{
    auto state = parameters.copyState();
    state.setProperty ("factoryProgram", currentProgram.load(), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destination);
}

void DATelescopeAudioProcessor::setStateInformation (const void* data, int length)
{
    if (auto xml = getXmlFromBinary (data, length))
    {
        if (xml->hasTagName (parameters.state.getType().toString()))
        {
            auto restored = juce::ValueTree::fromXml (*xml);
            currentProgram.store (juce::jlimit (0, getNumPrograms() - 1,
                 static_cast<int> (restored.getProperty ("factoryProgram", 0))));
            parameters.replaceState (restored);
        }
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DATelescopeAudioProcessor();
}
