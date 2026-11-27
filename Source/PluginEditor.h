#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include <array>
#include <memory>

class NeonKnob final : public juce::Slider
{
public:
    explicit NeonKnob (const juce::Image& source) : picture (source)
    {
        setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        setRange (0.0, 100.0, 0.1);
        setRotaryParameters (juce::MathConstants<float>::pi * 1.23f,
                             juce::MathConstants<float>::pi * 2.77f, true);
        setMouseDragSensitivity (190);
    }
    void paint (juce::Graphics&) override;
private:
    juce::Image picture;
};

class DASettingsPanel final : public juce::Component
{
public:
    explicit DASettingsPanel (juce::AudioProcessorValueTreeState&);
    void paint (juce::Graphics&) override;
    void resized() override;
private:
    juce::Slider mixSlider, feedbackSlider, toneSlider;
    juce::Label mixLabel, feedbackLabel, toneLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mixAttach, feedbackAttach, toneAttach;
};

class DATelescopeAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                               private juce::Timer
{
public:
    explicit DATelescopeAudioProcessorEditor (DATelescopeAudioProcessor&);
    ~DATelescopeAudioProcessorEditor() override = default;
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
private:
    void timerCallback() override;
    bool isCentralKnobHit (float x, float y) const;
    void applyParameterValue (const char* id, float value);
    void chooseProgram (int index);
    void showPresetMenu();
    void setChoice (const char* id, int index);
    void paintText (juce::Graphics&, const juce::String&, float x, float y,
                    float w, float h, float pointSize, juce::Colour, bool centre = true);
    void paintRadioOptions (juce::Graphics&, const char* id, const juce::StringArray& names,
                            float x, float y, bool leftSide);
    void paintMeter (juce::Graphics&, float x, float y, float level, bool reverse);
    void paintButton (juce::Graphics&, juce::Rectangle<float>, const juce::String&, bool active);
    int valueAsInt (const char* id) const;
    float scaledX (float n) const { return n * getWidth() / 1440.0f; }
    float scaledY (float n) const { return n * getHeight() / 810.0f; }

    DATelescopeAudioProcessor& processor;
    juce::Image background, knobImage, galaxyImage;
    NeonKnob zoomKnob, spreadKnob, distanceKnob, motionKnob;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, 4> knobAttachments;
    DASettingsPanel settings;
    int lastProgram = -1;

    // The central galaxy HUD acts as a large zoom knob: drag to adjust, double-click to reset.
    bool centralDragging = false;
    double centralDragStartAngle = 0.0;
    float centralDragStartValue = 0.0f;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DATelescopeAudioProcessorEditor)
};
