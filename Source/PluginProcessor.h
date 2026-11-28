#pragma once
#include <JuceHeader.h>
#include "FactoryPresets.h"
#include <array>
#include <atomic>
#include <vector>

class DATelescopeAudioProcessor final : public juce::AudioProcessor
{
public:
    DATelescopeAudioProcessor();
    ~DATelescopeAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;
    bool isBusesLayoutSupported (const BusesLayout&) const override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 5.0; }

    int getNumPrograms() override { return static_cast<int> (DA::factoryPresets.size()); }
    int getCurrentProgram() override { return currentProgram.load(); }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState parameters;
    std::atomic<float> inputPeak { 0.0f }, outputPeak { 0.0f };
    std::atomic<int> currentProgram { 0 };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    float readDelay (int channel, float delaySamples) const noexcept;
    void resetEffects();
    float onePoler (int channel, float input, float coefficient) noexcept;

    // Ping-pong delay taps (cross-coupled) for a true spatial echo.
    std::array<std::vector<float>, 2> delayBuffers;
    std::array<float, 2> toneState { 0.0f, 0.0f };
    std::array<float, 2> hpState { 0.0f, 0.0f };
    // Smoothed parameter values prevent clicks when presets/automation change.
    std::array<juce::SmoothedValue<float>, 7> smooth;
    juce::AudioBuffer<float> wetScratch;
    juce::Reverb reverb;
    int writePosition = 0;
    int delayBufferLength = 1;
    double activeSampleRate = 44100.0;
    double lfoPhase = 0.0;
    double lfoPhase2 = 0.0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DATelescopeAudioProcessor)
};
