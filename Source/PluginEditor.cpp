#include "PluginEditor.h"
#include <BinaryData.h>
#include <algorithm>
#include <cmath>

namespace
{
constexpr float baseWidth = 1440.0f;
constexpr float baseHeight = 810.0f;
constexpr double twoPi = juce::MathConstants<double>::twoPi;
// Central galaxy HUD geometry (base-resolution coordinates), used as a large draggable zoom knob.
constexpr float centralKnobX = 720.5f;
constexpr float centralKnobY = 449.5f;
constexpr float centralKnobRadius = 235.0f;
const juce::Colour ivory (0xffeee8ff);
const juce::Colour muted (0xffbbb1d6);
const juce::Colour neon (0xffb683ff);
const juce::Colour darkGlass (0xee050510);

juce::Image loadEmbedded (const void* data, int length)
{
    return juce::ImageCache::getFromMemory (data, length);
}

// Vector icons drawn instead of font glyphs (‹ › ▤ ⚙ ☰) so no platform font
// dependency can render them as empty boxes.
void drawChevron (juce::Graphics& g, juce::Rectangle<float> r, bool left, juce::Colour colour)
{
    g.setColour (colour);
    juce::Path p;
    const float tipX = left ? r.getX() : r.getRight();
    const float baseX = left ? r.getX() + r.getWidth() * 0.62f
                             : r.getX() + r.getWidth() * 0.38f;
    p.startNewSubPath (baseX, r.getY() + r.getHeight() * 0.18f);
    p.lineTo (tipX, r.getCentreY());
    p.lineTo (baseX, r.getBottom() - r.getHeight() * 0.18f);
    g.strokePath (p, juce::PathStrokeType (2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void drawMenuIcon (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour colour)
{
    g.setColour (colour);
    const int lines = 3;
    const float top = r.getY() + r.getHeight() * 0.26f;
    const float bottom = r.getBottom() - r.getHeight() * 0.26f;
    const float x0 = r.getX() + r.getWidth() * 0.16f;
    const float x1 = r.getRight() - r.getWidth() * 0.16f;
    for (int i = 0; i < lines; ++i)
    {
        const float y = top + (bottom - top) * static_cast<float> (i) / (lines - 1);
        g.drawLine (x0, y, x1, y, 2.2f);
    }
}

void drawGearIcon (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour colour)
{
    const auto cx = r.getCentreX();
    const auto cy = r.getCentreY();
    const float outer = std::min (r.getWidth(), r.getHeight()) * 0.47f;
    const float inner = outer * 0.70f;
    const int teeth = 8;
    juce::Path p;
    for (int i = 0; i < teeth; ++i)
    {
        const float a0 = static_cast<float> (i) / teeth * static_cast<float> (twoPi);
        const float a1 = a0 + static_cast<float> (twoPi) / teeth * 0.40f;
        const float a2 = a0 + static_cast<float> (twoPi) / teeth * 0.60f;
        const float a3 = static_cast<float> (i + 1) / teeth * static_cast<float> (twoPi);
        const auto o0 = juce::Point<float> (cx + outer * std::cos (a0), cy + outer * std::sin (a0));
        const auto o1 = juce::Point<float> (cx + outer * std::cos (a1), cy + outer * std::sin (a1));
        const auto i2 = juce::Point<float> (cx + inner * std::cos (a2), cy + inner * std::sin (a2));
        const auto i3 = juce::Point<float> (cx + inner * std::cos (a3), cy + inner * std::sin (a3));
        if (i == 0)
            p.startNewSubPath (o0);
        else
            p.lineTo (o0);
        p.lineTo (o1);
        p.lineTo (i2);
        p.lineTo (i3);
    }
    p.closeSubPath();
    g.setColour (colour);
    g.fillPath (p);
    g.setColour (juce::Colour (0xff14121c)); // matches the header pill background
    g.fillEllipse (cx, cy, inner * 0.84f, inner * 0.84f);
}
}

void NeonKnob::paint (juce::Graphics& g)
{
    const auto square = getLocalBounds().toFloat().reduced (2.0f);
    const auto center = square.getCentre();
    const float value = static_cast<float> ((getValue() - getMinimum()) / (getMaximum() - getMinimum()));

    // Rotate the knob picture with the value so its engraved indicator line
    // sweeps along the arc exactly like a real rotary control.
    g.saveState();
    g.addTransform (juce::AffineTransform::rotation (value * juce::MathConstants<float>::pi * 1.54f,
                                                     center.x, center.y));
    if (picture.isValid())
        g.drawImageWithin (picture, static_cast<int> (square.getX()), static_cast<int> (square.getY()),
                           static_cast<int> (square.getWidth()), static_cast<int> (square.getHeight()),
                           juce::RectanglePlacement::centred);
    else
    {
        g.setColour (juce::Colours::black);
        g.fillEllipse (square);
    }
    g.restoreState();

    const float radius = 0.419f * std::min (getWidth(), getHeight());
    const float startAngle = juce::MathConstants<float>::pi * 1.23f;
    const float finalAngle = startAngle + value * juce::MathConstants<float>::pi * 1.54f;
    juce::Path progress;
    progress.addCentredArc (center.x, center.y, radius, radius, 0.0f,
                           startAngle, finalAngle, true);
    g.setColour (juce::Colour (0x55cb9aff));
    g.strokePath (progress, juce::PathStrokeType (4.0f));
    g.setColour (neon);
    g.strokePath (progress, juce::PathStrokeType (1.7f));
    const float markerRadius = radius * 0.71f;
    // Point exactly along the arc-end radial, using the same angle convention as addCentredArc:
    // angle 0 points east and positive angles rotate clockwise on screen (y-down).
    const auto dx = std::cos (finalAngle), dy = std::sin (finalAngle);
    g.setColour (juce::Colours::white.withAlpha (0.9f));
    g.drawLine (center.x + dx * (markerRadius - 7.0f), center.y + dy * (markerRadius - 7.0f),
                center.x + dx * (markerRadius + 5.0f), center.y + dy * (markerRadius + 5.0f), 2.0f);
}

DASettingsPanel::DASettingsPanel (juce::AudioProcessorValueTreeState& parameters)
{
    auto init = [this] (juce::Slider& slider, juce::Label& label, const char* text)
    {
        addAndMakeVisible (label);
        label.setText (text, juce::dontSendNotification);
        label.setColour (juce::Label::textColourId, ivory);
        label.setFont (juce::Font (juce::FontOptions (14.0f)));
        addAndMakeVisible (slider);
        slider.setSliderStyle (juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 56, 25);
        slider.setColour (juce::Slider::trackColourId, neon);
        slider.setColour (juce::Slider::backgroundColourId, juce::Colour (0xff292037));
        slider.setColour (juce::Slider::thumbColourId, ivory);
        slider.setColour (juce::Slider::textBoxTextColourId, ivory);
        slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    };
    init (mixSlider, mixLabel, "WET / DRY MIX");
    init (feedbackSlider, feedbackLabel, "FEEDBACK");
    init (toneSlider, toneLabel, "TONE");
    mixAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (parameters, "mix", mixSlider);
    feedbackAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (parameters, "feedback", feedbackSlider);
    toneAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (parameters, "tone", toneSlider);
}

void DASettingsPanel::paint (juce::Graphics& g)
{
    g.setColour (juce::Colour (0xf9080712));
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 15.0f);
    g.setColour (neon.withAlpha (0.72f));
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (1.0f), 15.0f, 1.25f);
    g.setColour (ivory);
    g.setFont (juce::Font (juce::FontOptions (16.0f)).boldened());
    g.drawText ("ENGINE SETTINGS", 18, 11, getWidth() - 36, 24, juce::Justification::centredLeft);
}

void DASettingsPanel::resized()
{
    auto row = [this] (juce::Label& label, juce::Slider& slider, int y)
    {
        label.setBounds (18, y, 146, 24);
        slider.setBounds (16, y + 22, getWidth() - 34, 30);
    };
    row (mixLabel, mixSlider, 40);
    row (feedbackLabel, feedbackSlider, 100);
    row (toneLabel, toneSlider, 160);
}

DATelescopeAudioProcessorEditor::DATelescopeAudioProcessorEditor (DATelescopeAudioProcessor& p)
    : juce::AudioProcessorEditor (&p),
      processor (p),
      background (loadEmbedded (BinaryData::gui_background_png, BinaryData::gui_background_pngSize)),
      knobImage (loadEmbedded (BinaryData::knob_png, BinaryData::knob_pngSize)),
      galaxyImage (loadEmbedded (BinaryData::galaxy_png, BinaryData::galaxy_pngSize)),
      zoomKnob (knobImage), spreadKnob (knobImage), distanceKnob (knobImage), motionKnob (knobImage),
      settings (p.parameters)
{
    const std::array<const char*, 4> ids { "zoom", "spread", "distance", "motion" };
    const std::array<NeonKnob*, 4> knobs { &zoomKnob, &spreadKnob, &distanceKnob, &motionKnob };
    for (size_t i = 0; i < knobs.size(); ++i)
    {
        addAndMakeVisible (*knobs[i]);
        knobAttachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>
            (p.parameters, ids[i], *knobs[i]);
        knobs[i]->setTooltip (juce::String (ids[i]) + " — drag to adjust, double-click to reset");
        knobs[i]->setDoubleClickReturnValue (true, i == 0 ? 72.0 : i == 1 ? 58.0 : i == 2 ? 63.0 : 41.0);
    }
    addChildComponent (settings);
    setResizable (true, true);
    setResizeLimits (960, 540, 1920, 1080);
    setSize (1440, 810);
    startTimerHz (15);
}

void DATelescopeAudioProcessorEditor::resized()
{
    // Uniform scale keeps the 16:9 layout undistorted at any window aspect;
    // the content is centred and the remaining area stays as dark letterbox.
    contentScale = std::min (static_cast<float> (getWidth()) / baseWidth,
                             static_cast<float> (getHeight()) / baseHeight);
    contentOffsetX = (static_cast<float> (getWidth()) - baseWidth * contentScale) * 0.5f;
    contentOffsetY = (static_cast<float> (getHeight()) - baseHeight * contentScale) * 0.5f;

    struct Area { int x, y, size; };
    const std::array<Area, 4> areas {{ {225, 193, 225}, {225, 430, 225},
                                       {989, 193, 225}, {989, 430, 225} }};
    const std::array<NeonKnob*, 4> knobs { &zoomKnob, &spreadKnob, &distanceKnob, &motionKnob };
    for (size_t i = 0; i < knobs.size(); ++i)
        knobs[i]->setBounds (juce::Rectangle<int> (juce::roundToInt (scaledX ((float) areas[i].x)),
                                                   juce::roundToInt (scaledY ((float) areas[i].y)),
                                                   juce::roundToInt (areas[i].size * contentScale),
                                                   juce::roundToInt (areas[i].size * contentScale)));
    settings.setBounds (juce::roundToInt (scaledX (1094)), juce::roundToInt (scaledY (105)),
                        juce::roundToInt (320.0f * contentScale), juce::roundToInt (238.0f * contentScale));
}

void DATelescopeAudioProcessorEditor::paintText (juce::Graphics& g, const juce::String& txt,
                                                   float x, float y, float w, float h, float size,
                                                   juce::Colour colour, bool centre)
{
    g.setColour (colour);
    g.setFont (juce::Font (juce::FontOptions (size)));
    g.drawFittedText (txt, juce::Rectangle<int> (juce::roundToInt (x), juce::roundToInt (y),
                       juce::roundToInt (w), juce::roundToInt (h)),
                      centre ? juce::Justification::centred : juce::Justification::centredLeft, 1);
}

int DATelescopeAudioProcessorEditor::valueAsInt (const char* id) const
{
    if (const auto* value = processor.parameters.getRawParameterValue (id))
        return juce::roundToInt (value->load());
    return 0;
}

void DATelescopeAudioProcessorEditor::paintRadioOptions (juce::Graphics& g, const char* id,
                                                           const juce::StringArray& labels,
                                                           float x, float y, bool leftSide)
{
    const int selected = valueAsInt (id);
    for (int i = 0; i < labels.size(); ++i)
    {
        const float py = y + i * 28.0f;
        const float radioX = leftSide ? x + 102.0f : x;
        const float textX = leftSide ? x : x + 24.0f;
        g.setColour (selected == i ? juce::Colour (0xffd3b2ff) : juce::Colour (0xff575169));
        g.drawEllipse (radioX, py + 9.0f, 11.0f, 11.0f, 1.2f);
        if (selected == i)
        {
            g.setColour (neon.withAlpha (0.24f));
            g.fillEllipse (radioX - 6.0f, py + 3.0f, 23.0f, 23.0f);
            g.setColour (ivory);
            g.fillEllipse (radioX + 2.5f, py + 11.5f, 6.0f, 6.0f);
        }
        paintText (g, labels[i].toUpperCase(), textX, py, leftSide ? 95.0f : 94.0f, 28.0f,
                   11.0f, i == selected ? ivory : muted, false);
    }
}

void DATelescopeAudioProcessorEditor::paintMeter (juce::Graphics& g, float x, float y,
                                                    float level, bool reverse)
{
    const float meter = juce::jlimit (0.0f, 1.0f, level);
    for (int i = 0; i < 42; ++i)
    {
        const float t = static_cast<float> (i) / 41.0f;
        const float envelope = 0.20f + 0.80f * std::pow (std::abs (std::sin (t * 10.4f + (reverse ? 1.5f : 0.2f))), 5.0f);
        const float height = 2.0f + meter * 23.0f * envelope;
        g.setColour (juce::Colour (0xfff0deff).withAlpha (0.2f + meter * 0.8f));
        g.fillRoundedRectangle (x + i * 3.65f, y - 0.5f * height, 1.7f, height, 0.75f);
    }
}

void DATelescopeAudioProcessorEditor::paintButton (juce::Graphics& g, juce::Rectangle<float> box,
                                                     const juce::String& title, bool active)
{
    if (active)
    {
        g.setColour (neon.withAlpha (0.13f));
        g.fillRoundedRectangle (box.expanded (5.0f), 23.0f);
    }
    g.setColour (active ? juce::Colour (0xff271137) : juce::Colour (0xdd080910));
    g.fillRoundedRectangle (box, 22.0f);
    g.setColour (active ? juce::Colour (0xffdfbaff) : juce::Colour (0xff5e536d));
    g.drawRoundedRectangle (box.reduced (0.8f), 22.0f, active ? 2.5f : 1.0f);
    paintText (g, title, box.getX(), box.getY(), box.getWidth(), box.getHeight(),
               12.0f, active ? juce::Colours::white : muted);
}

void DATelescopeAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff020106));
    g.addTransform (juce::AffineTransform::scale (contentScale, contentScale)
                                      .translated (contentOffsetX, contentOffsetY));
    if (background.isValid())
        g.drawImage (background, 0.0f, 0.0f, baseWidth, baseHeight, 0, 0,
                     background.getWidth(), background.getHeight());
    const auto panel = juce::Rectangle<float> (45.0f, 44.0f, 1350.0f, 723.0f);
    g.setColour (juce::Colour (0xee060610));
    g.fillRoundedRectangle (panel, 26.0f);
    g.setColour (juce::Colour (0xff75608d));
    g.drawRoundedRectangle (panel.reduced (1), 26.0f, 1.6f);
    g.setColour (juce::Colour (0x777d4ba2));
    g.drawRoundedRectangle (panel.reduced (5), 22.0f, 0.8f);
    g.setColour (juce::Colour (0xd9080710));
    g.fillRoundedRectangle (juce::Rectangle<float> (54, 52, 1332, 71), 19.0f);
    g.setColour (juce::Colour (0x445a4776));
    g.drawHorizontalLine (122, 78.0f, 1362.0f);

    // Brand and preset browser.
    g.setColour (juce::Colour (0xffdfc5ff));
    g.drawEllipse (98.0f, 73.0f, 24.0f, 24.0f, 3.0f);
    paintText (g, "D.A PRODUCTION", 134.0f, 68.0f, 244.0f, 34.0f, 17.0f, ivory, false);
    g.setColour (juce::Colour (0xff14121c));
    g.fillRoundedRectangle (juce::Rectangle<float> (548.0f, 68.0f, 347.0f, 37.0f), 18.0f);
    g.setColour (juce::Colour (0xff584762));
    g.drawRoundedRectangle (juce::Rectangle<float> (548.0f, 68.0f, 347.0f, 37.0f), 18.0f, 1.0f);
    drawChevron (g, {559.0f, 69.0f, 30.0f, 36.0f}, true, ivory);
    paintText (g, processor.getProgramName (processor.currentProgram.load()), 594, 70, 252, 34, 15, ivory);
    drawChevron (g, {845.0f, 69.0f, 35.0f, 36.0f}, false, ivory);
    drawMenuIcon (g, {906.0f, 69.0f, 28.0f, 35.0f}, ivory);
    paintText (g, "SPATIAL FX ENGINE", 1017, 70, 183, 30, 11, muted);
    drawGearIcon (g, {1230.0f, 70.0f, 41.0f, 31.0f}, ivory);
    drawMenuIcon (g, {1296.0f, 70.0f, 37.0f, 31.0f}, ivory);

    paintText (g, "T E L E S C O P E   F X", 432, 127, 576, 51, 27, juce::Colour (0xffe5cbff));
    paintText (g, "S P A C E    •    M O T I O N    •    D I M E N S I O N",
               474, 173, 495, 23, 11, muted);

    // Starry central window. The nebula drifts and wobbles at a rate driven by
    // the incoming signal level, so the stars keep moving with the audio.
    if (galaxyImage.isValid())
    {
        const float peak = processor.inputPeak.load();
        const float wobble = static_cast<float> (std::sin (galaxyWobblePhase)) * 0.06f * peak;
        g.saveState();
        g.addTransform (juce::AffineTransform::rotation (static_cast<float> (galaxyAngle) + wobble,
                                                          centralKnobX, centralKnobY));
        g.drawImageWithin (galaxyImage, 473, 202, 495, 495, juce::RectanglePlacement::centred);
        g.restoreState();
    }

    paintText (g, "ZOOM", 236, 407, 204, 26, 16, ivory);
    paintText (g, juce::String (valueAsInt ("zoom")) + "%", 236, 431, 204, 24, 14, muted);
    paintText (g, "SPREAD", 236, 643, 204, 26, 16, ivory);
    paintText (g, juce::String (valueAsInt ("spread")) + "%", 236, 667, 204, 24, 14, muted);
    paintText (g, "DISTANCE", 998, 407, 204, 26, 16, ivory);
    paintText (g, juce::String (valueAsInt ("distance")) + "%", 998, 431, 204, 24, 14, muted);
    paintText (g, "MOTION", 998, 643, 204, 26, 16, ivory);
    paintText (g, juce::String (valueAsInt ("motion")) + "%", 998, 667, 204, 24, 14, muted);

    paintRadioOptions (g, "zoomProfile", { "Close", "Balanced", "Far" }, 93, 278, true);
    paintRadioOptions (g, "spreadProfile", { "Mono", "Wide", "Galactic" }, 93, 514, true);
    paintRadioOptions (g, "distanceProfile", { "Near", "Mid", "Deep" }, 1225, 278, false);
    paintRadioOptions (g, "motionProfile", { "Static", "Orbit", "Warp" }, 1225, 514, false);

    // Bottom FX mode tabs and input/output waveform meters.
    g.setColour (juce::Colour (0xf1080710));
    g.fillRoundedRectangle (juce::Rectangle<float> (75, 706, 1289, 50), 24.0f);
    g.setColour (juce::Colour (0x555c4c71));
    g.drawRoundedRectangle (juce::Rectangle<float> (75, 706, 1289, 50), 24.0f, 1.0f);
    paintText (g, "INPUT", 90, 718, 64, 27, 11, muted, false);
    paintMeter (g, 160, 730, processor.inputPeak.load(), false);
    const std::array<juce::String, 5> names { "TELESCOPE", "ORBIT", "WARP", "DIMENSION", "ATMOSPHERE" };
    const int selectedMode = valueAsInt ("mode");
    for (int i = 0; i < 5; ++i)
        paintButton (g, { 355.0f + i * 149.0f, 710.0f, 137.0f, 43.0f }, names[static_cast<size_t> (i)], selectedMode == i);
    paintMeter (g, 1126, 730, processor.outputPeak.load(), true);
    paintText (g, "OUTPUT", 1293, 718, 65, 27, 11, muted);
    paintText (g, "E X P A N D   Y O U R   S O U N D", 82, 771, 264, 17, 10, muted, false);
    paintText (g, "A   H I G H E R   P E R S P E C T I V E", 1097, 771, 277, 17, 10, muted);
}

bool DATelescopeAudioProcessorEditor::isCentralKnobHit (float x, float y) const
{
    const auto dx = x - centralKnobX;
    const auto dy = y - centralKnobY;
    return dx * dx + dy * dy <= centralKnobRadius * centralKnobRadius;
}

void DATelescopeAudioProcessorEditor::applyParameterValue (const char* id, float value)
{
    if (auto* parameter = processor.parameters.getParameter (id))
    {
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
        parameter->endChangeGesture();
    }
}

void DATelescopeAudioProcessorEditor::setChoice (const char* id, int index)
{
    if (auto* parameter = processor.parameters.getParameter (id))
    {
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (static_cast<float> (index)));
        parameter->endChangeGesture();
    }
}

void DATelescopeAudioProcessorEditor::chooseProgram (int index)
{
    processor.setCurrentProgram ((index + processor.getNumPrograms()) % processor.getNumPrograms());
    repaint();
}

void DATelescopeAudioProcessorEditor::showPresetMenu()
{
    juce::PopupMenu menu;
    static const std::array<const char*, 10> groups {
        "NEBULA", "ORBIT", "WARP", "DEEP SPACE", "DIMENSION", "ATMOSPHERE",
        "VOCAL", "RHYTHM", "GUITAR", "CINEMA"
    };
    for (int category = 0; category < 10; ++category)
    {
        juce::PopupMenu submenu;
        for (int j = 0; j < 10; ++j)
        {
            const int idx = category * 10 + j;
            submenu.addItem (idx + 1, processor.getProgramName (idx), true,
                             processor.currentProgram.load() == idx);
        }
        menu.addSubMenu (groups[static_cast<size_t> (category)], submenu);
    }
    juce::Component::SafePointer<DATelescopeAudioProcessorEditor> safeThis (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
                        [safeThis] (int picked)
                        {
                            if (safeThis != nullptr && picked > 0)
                                safeThis->chooseProgram (picked - 1);
                        });
}

juce::Point<float> DATelescopeAudioProcessorEditor::baseCoords (const juce::Point<float>& screen) const
{
    return (screen - juce::Point<float> (contentOffsetX, contentOffsetY)) / contentScale;
}

void DATelescopeAudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    const auto c = baseCoords (e.position.toFloat());
    const auto x = c.x, y = c.y;

    // The central galaxy HUD doubles as a large zoom knob.
    if (isCentralKnobHit (x, y))
    {
        if (e.getNumberOfClicks() == 2)
        {
            applyParameterValue ("zoom", 72.0f); // factory default
            return;
        }
        centralDragging = true;
        centralDragStartAngle = std::atan2 (y - centralKnobY, x - centralKnobX);
        centralDragStartValue = static_cast<float> (valueAsInt ("zoom"));
        return;
    }

    if (y > 68 && y < 107)
    {
        if (x >= 548 && x <= 595) { chooseProgram (processor.currentProgram.load() - 1); return; }
        if (x >= 843 && x <= 901) { chooseProgram (processor.currentProgram.load() + 1); return; }
        if (x >= 596 && x <= 941) { showPresetMenu(); return; }
        if (x >= 1220 && x <= 1290) { settings.setVisible (! settings.isVisible()); return; }
        if (x >= 1290 && x <= 1360) { showPresetMenu(); return; }
    }
    if (y >= 709 && y <= 755 && x >= 355 && x <= 1090)
    {
        const int index = static_cast<int> ((x - 355) / 149);
        if (index >= 0 && index < 5) setChoice ("mode", index);
        return;
    }
    const auto tryOption = [this, x, y] (const char* id, float xmin, float top)
    {
        if (x < xmin || x > xmin + 125 || y < top || y > top + 85) return false;
        setChoice (id, juce::jlimit (0, 2, static_cast<int> ((y - top) / 28.0f)));
        return true;
    };
    if (tryOption ("zoomProfile", 88, 278)) return;
    if (tryOption ("spreadProfile", 88, 514)) return;
    if (tryOption ("distanceProfile", 1220, 278)) return;
    (void) tryOption ("motionProfile", 1220, 514);
}

void DATelescopeAudioProcessorEditor::mouseDrag (const juce::MouseEvent& e)
{
    if (! centralDragging) return;

    const auto c = baseCoords (e.position.toFloat());
    const auto x = c.x, y = c.y;
    const auto angle = std::atan2 (y - centralKnobY, x - centralKnobX);
    auto delta = angle - centralDragStartAngle;
    while (delta > juce::MathConstants<double>::pi) delta -= juce::MathConstants<double>::twoPi;
    while (delta < -juce::MathConstants<double>::pi) delta += juce::MathConstants<double>::twoPi;

    // One full rotation around the knob sweeps the whole 0..100 zoom range.
    const auto value = centralDragStartValue + delta / juce::MathConstants<double>::twoPi * 100.0;
    applyParameterValue ("zoom", juce::jlimit (0.0f, 100.0f, static_cast<float> (value)));
}

void DATelescopeAudioProcessorEditor::mouseUp (const juce::MouseEvent&)
{
    centralDragging = false;
}

void DATelescopeAudioProcessorEditor::timerCallback()
{
    if (lastProgram != processor.currentProgram.load())
        lastProgram = processor.currentProgram.load();

    // The nebula drifts/shakes at a rate driven by the incoming signal level
    // (the entered frequency), so it visibly moves with the audio.
    const float peak = processor.inputPeak.load();
    const double rate = 0.05 + peak * 1.5; // base drift + audio-driven motion
    galaxyAngle += rate * 0.0167;
    galaxyWobblePhase += rate * 0.35;
    repaint();
}
