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

// The bundled PNG assets are authored with very dark nebula/star fields, so a
// plain draw leaves them nearly invisible against the dark glass panel. Load
// each one and apply an HSV gain + saturation lift so the textures read clearly
// while preserving hue and alpha (a straight RGB multiply would muddy colours).
juce::Image brightenImage (juce::Image src, float gain, float saturation)
{
    if (! src.isValid())
        return src;

    juce::Image dst = src.createCopy();
    juce::Image::BitmapData bm (dst, juce::Image::BitmapData::readWrite);

    const int w = dst.getWidth();
    const int h = dst.getHeight();

    for (int y = 0; y < h; ++y)
    {
        for (int x = 0; x < w; ++x)
        {
            const auto c = bm.getPixelColour (x, y);
            if (c.getAlpha() == 0)
                continue;

            const float hue = c.getHue();
            const float sat = juce::jmin (1.0f, c.getSaturation() * saturation);
            const float bri = juce::jmin (1.0f, c.getBrightness() * gain);
            bm.setPixelColour (x, y, juce::Colour::fromHSV (hue, sat, bri, c.getAlpha() / 255.0f));
        }
    }

    return dst;
}

juce::Image loadEmbedded (const void* data, int length, float gain = 1.0f, float saturation = 1.0f)
{
    return brightenImage (juce::ImageCache::getFromMemory (data, length), gain, saturation);
}

// Embedded UI font (Poppins) so every label renders correctly regardless of the
// host's installed fonts. Falling back to the platform default can yield empty
// "tofu" boxes on machines where the default family is missing. Returned by
// value so callers never hold a dangling reference.
juce::Font getUiFont (float size)
{
    static const juce::Typeface::Ptr typeface = juce::Typeface::createSystemTypefaceFor (
        static_cast<const void*> (BinaryData::poppins_regular_ttf),
        static_cast<size_t> (BinaryData::poppins_regular_ttfSize));
    if (typeface != nullptr)
        return juce::Font (juce::FontOptions (typeface).withPointHeight (size));
    return juce::Font (juce::FontOptions (size));
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

// Shared text helper usable by both the editor and the preset browser.
// Uses drawText (not drawFittedText): drawFittedText squashes/scales the font
// to fit the rectangle, and the embedded font rasterises as "tofu" boxes at the
// fractional scaled sizes it produces. drawText draws at the exact font size, so
// glyphs render correctly (truncating with an ellipsis only if truly too wide).
void paintTextFitted (juce::Graphics& g, const juce::String& txt, float x, float y,
                      float w, float h, float size, juce::Colour colour, bool centre)
{
    g.setColour (colour);
    g.setFont (getUiFont (size));
    g.drawText (txt, juce::Rectangle<int> (juce::roundToInt (x), juce::roundToInt (y),
                       juce::roundToInt (w), juce::roundToInt (h)),
                centre ? juce::Justification::centred : juce::Justification::centredLeft);
}
}

void NeonKnob::paint (juce::Graphics& g)
{
    const auto square = getLocalBounds().toFloat().reduced (2.0f);
    const auto center = square.getCentre();
    const float value = static_cast<float> ((getValue() - getMinimum()) / (getMaximum() - getMinimum()));

    // Static knob texture (the dark band is part of the source artwork).
    if (picture.isValid())
        g.drawImageWithin (picture, static_cast<int> (square.getX()), static_cast<int> (square.getY()),
                           static_cast<int> (square.getWidth()), static_cast<int> (square.getHeight()),
                           juce::RectanglePlacement::centred);
    else
    {
        g.setColour (juce::Colours::black);
        g.fillEllipse (square);
    }

    // Progress arc + marker, driven directly by the value so the line always
    // tracks the knob position. JUCE angles: 0 = east (3 o'clock), positive
    // rotates clockwise on screen (y-down). The sweep starts at 7 o'clock
    // (120° = 2π/3) for 0% and ends at 5 o'clock (60° = π/3) for 100%, going the
    // long way round through 12 o'clock (300° total = 5π/3).
    const float radius = 0.419f * std::min (getWidth(), getHeight());
    const float startAngle = twoPi / 3.0f;             // 7 o'clock = 0%
    const float sweep = twoPi * (5.0f / 6.0f);         // to 5 o'clock = 100%
    const float finalAngle = startAngle + value * sweep;
    juce::Path progress;
    progress.addCentredArc (center.x, center.y, radius, radius, 0.0f,
                           startAngle, finalAngle, true);
    g.setColour (juce::Colour (0x55cb9aff));
    g.strokePath (progress, juce::PathStrokeType (4.0f));
    g.setColour (neon);
    g.strokePath (progress, juce::PathStrokeType (1.7f));

    // Full track for context (same 7-o'clock -> 5-o'clock arc).
    juce::Path track;
    track.addCentredArc (center.x, center.y, radius, radius, 0.0f,
                         startAngle, startAngle + sweep, true);
    g.setColour (juce::Colour (0x33ffffff));
    g.strokePath (track, juce::PathStrokeType (1.0f));

    // Marker line exactly along the arc-end radial. Angle 0 points east and
    // positive angles rotate clockwise on screen (y-down), matching addCentredArc.
    const float markerRadius = radius * 0.71f;
    const auto dx = std::cos (finalAngle), dy = std::sin (finalAngle);
    g.setColour (juce::Colours::white.withAlpha (0.95f));
    g.drawLine (center.x + dx * (markerRadius - 7.0f), center.y + dy * (markerRadius - 7.0f),
                center.x + dx * (markerRadius + 6.0f), center.y + dy * (markerRadius + 6.0f), 2.2f);
    g.setColour (neon);
    g.fillEllipse (center.x + dx * markerRadius, center.y + dy * markerRadius, 4.0f, 4.0f);
}

DASettingsPanel::DASettingsPanel (juce::AudioProcessorValueTreeState& parameters)
{
    auto init = [this] (juce::Slider& slider, juce::Label& label, const char* text)
    {
        addAndMakeVisible (label);
        label.setText (text, juce::dontSendNotification);
        label.setColour (juce::Label::textColourId, ivory);
        label.setFont (getUiFont (14.0f));
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
    g.setFont (getUiFont (16.0f).boldened());
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

namespace
{
// Preset categories in display order; index 0 is the "ALL" entry.
const std::array<const char*, 10> presetCategories {
    "NEBULA", "ORBIT", "WARP", "DEEP SPACE", "DIMENSION",
    "ATMOSPHERE", "VOCAL", "RHYTHM", "GUITAR", "CINEMA"
};
int categoryOfPreset (const juce::String& name)
{
    for (int c = 0; c < 10; ++c)
        if (name.startsWithIgnoreCase (presetCategories[static_cast<size_t> (c)]))
            return c + 1;
    return 0;
}
juce::String categoryLabelFor (const juce::String& name)
{
    const int c = categoryOfPreset (name);
    if (c == 0) return juce::String ("MISC");
    return juce::String (presetCategories[static_cast<size_t> (c - 1)]);
}
}

DAPresetBrowser::DAPresetBrowser (DATelescopeAudioProcessor& proc) : processor (proc)
{
    setOpaque (true);
}

void DAPresetBrowser::setScale (float s, float ox, float oy)
{
    scale = s; offsetX = ox; offsetY = oy;
}

int DAPresetBrowser::contentHeight() const
{
    int count = 0;
    const int total = processor.getNumPrograms();
    if (category == 0) count = total;
    else
        for (int i = 0; i < total; ++i)
            if (categoryOfPreset (processor.getProgramName (i)) == category) ++count;
    const int rows = (count + cols - 1) / cols;
    return rows * 86 + 16;
}

juce::Rectangle<float> DAPresetBrowser::cardRect (int index) const
{
    const int row = index / cols;
    const int col = index % cols;
    const float x = 70.0f + col * 257.0f;
    const float y = 196.0f + row * 86.0f;
    return { x, y, 245.0f, 74.0f };
}

void DAPresetBrowser::setCategory (int cat) { category = juce::jlimit (0, 10, cat); scrollPx = 0; repaint(); }

void DAPresetBrowser::setScroll (int px)
{
    const int maxScroll = std::max (0, contentHeight() - static_cast<int> (getHeight()) + 24);
    scrollPx = juce::jlimit (0, maxScroll, px);
    repaint();
}

void DAPresetBrowser::setSelected (int index)
{
    (void) index; // selection is reflected by highlighting the current program in paint()
}

void DAPresetBrowser::paint (juce::Graphics& g)
{
    // Draw in base coordinates using the editor's uniform transform.
    g.saveState();
    g.addTransform (juce::AffineTransform::scale (scale, scale).translated (offsetX, offsetY));

    const auto bounds = juce::Rectangle<float> (45.0f, 44.0f, 1350.0f, 723.0f);
    g.setColour (juce::Colour (0xee050510));
    g.fillRoundedRectangle (bounds, 26.0f);
    g.setColour (neon.withAlpha (0.72f));
    g.drawRoundedRectangle (bounds.reduced (1), 26.0f, 1.6f);

    // Title and back button.
    paintTextFitted (g, "PRESET BROWSER", 70.0f, 74.0f, 500.0f, 30.0f, 18.0f, ivory, false);
    const auto backBox = juce::Rectangle<float> (1240.0f, 68.0f, 130.0f, 40.0f);
    g.setColour (juce::Colour (0xdd080910));
    g.fillRoundedRectangle (backBox, 20.0f);
    g.setColour (neon.withAlpha (0.72f));
    g.drawRoundedRectangle (backBox.reduced (1), 20.0f, 1.2f);
    drawChevron (g, { backBox.getX() + 24.0f, backBox.getY() + 8.0f, 18.0f, 24.0f }, true, ivory);
    paintTextFitted (g, "BACK", backBox.getX() + 50.0f, backBox.getY() + 10.0f, 70.0f, 22.0f, 12.0f, ivory, false);

    // Category tabs.
    const int totalCats = 11;
    const float tabW = 118.0f, tabH = 34.0f, tabGap = 8.0f;
    const float tabX0 = 70.0f, tabY = 122.0f;
    for (int c = 0; c < totalCats; ++c)
    {
        const auto r = juce::Rectangle<float> (tabX0 + c * (tabW + tabGap), tabY, tabW, tabH);
        const bool active = (c == category);
        g.setColour (active ? juce::Colour (0xff271137) : juce::Colour (0xdd080910));
        g.fillRoundedRectangle (r, 16.0f);
        g.setColour (active ? neon : juce::Colour (0xff5e536d));
        g.drawRoundedRectangle (r.reduced (0.8f), 16.0f, active ? 2.0f : 1.0f);
        const juce::String label = (c == 0 ? juce::String ("ALL")
                                            : juce::String (presetCategories[static_cast<size_t> (c - 1)]));
        paintTextFitted (g, label, r.getX(), r.getY() + 7.0f, tabW, 22.0f, 11.0f,
                   active ? ivory : muted, false);
    }

    // Clip to the grid area so cards scroll within bounds.
    const auto gridArea = juce::Rectangle<float> (62.0f, 170.0f, 1276.0f, 560.0f);
    g.saveState();
    juce::Path gridPath; gridPath.addRectangle (gridArea);
    g.reduceClipRegion (gridPath);
    g.addTransform (juce::AffineTransform::translation (0.0f, -static_cast<float> (scrollPx)));

    const int total = processor.getNumPrograms();
    int shown = 0;
    for (int i = 0; i < total; ++i)
    {
        if (category != 0 && categoryOfPreset (processor.getProgramName (i)) != category) continue;
        const auto r = cardRect (shown);
        const bool current = (i == processor.currentProgram.load());
        g.setColour (current ? juce::Colour (0xff271137) : juce::Colour (0xd2080712));
        g.fillRoundedRectangle (r, 14.0f);
        g.setColour (current ? neon : juce::Colour (0x555c4c71));
        g.drawRoundedRectangle (r.reduced (0.8f), 14.0f, current ? 2.0f : 1.0f);
        paintTextFitted (g, juce::String (i + 1).paddedLeft (3, '0') + "  " + processor.getProgramName (i),
                   r.getX() + 16.0f, r.getY() + 12.0f, r.getWidth() - 32.0f, 26.0f, 12.0f,
                   current ? ivory : juce::Colour (0xffd8cee8), false);
        paintTextFitted (g, categoryLabelFor (processor.getProgramName (i)),
                   r.getX() + 16.0f, r.getY() + 42.0f, r.getWidth() - 32.0f, 22.0f, 10.0f, muted, false);
        ++shown;
    }
    g.restoreState();
    g.restoreState();
}

void DAPresetBrowser::resized()
{
}

void DAPresetBrowser::mouseDown (const juce::MouseEvent& e)
{
    const auto base = (e.getPosition().toFloat() - juce::Point<float> (offsetX, offsetY)) / scale;

    // Back button.
    const auto backBox = juce::Rectangle<float> (1240.0f, 68.0f, 130.0f, 40.0f);
    if (backBox.contains (base)) { if (onClose) onClose(); return; }

    // Category tabs.
    const int totalCats = 11;
    const float tabW = 118.0f, tabH = 34.0f, tabGap = 8.0f;
    const float tabX0 = 70.0f, tabY = 122.0f;
    if (base.y >= tabY && base.y < tabY + tabH)
    {
        const int c = static_cast<int> ((base.x - tabX0) / (tabW + tabGap));
        if (c >= 0 && c < totalCats) { setCategory (c); return; }
    }

    // Preset cards (account for scroll offset).
    const float y = base.y + static_cast<float> (scrollPx);
    int shown = 0;
    const int total = processor.getNumPrograms();
    for (int i = 0; i < total; ++i)
    {
        if (category != 0 && categoryOfPreset (processor.getProgramName (i)) != category) continue;
        const auto r = cardRect (shown);
        if (r.contains (juce::Point<float> (base.x, y)))
        {
            if (onSelect) onSelect (i);
            return;
        }
        ++shown;
    }

    // Otherwise start a vertical drag to scroll the grid.
    dragging = true;
    dragStartY = e.getPosition().y;
    dragStartScroll = scrollPx;
}

void DAPresetBrowser::mouseDrag (const juce::MouseEvent& e)
{
    if (! dragging) return;
    const auto dy = e.getPosition().y - dragStartY;
    setScroll (dragStartScroll - static_cast<int> (dy));
}

void DAPresetBrowser::mouseUp (const juce::MouseEvent&)
{
    dragging = false;
}

void DAPresetBrowser::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& wheel)
{
    setScroll (scrollPx + static_cast<int> (wheel.deltaY) * 46);
}

DATelescopeAudioProcessorEditor::DATelescopeAudioProcessorEditor (DATelescopeAudioProcessor& p)
    : juce::AudioProcessorEditor (&p),
      processor (p),
      background (loadEmbedded (BinaryData::gui_background_png, BinaryData::gui_background_pngSize, 1.6f, 1.3f)),
      knobImage (loadEmbedded (BinaryData::knob_png, BinaryData::knob_pngSize, 1.3f, 1.35f)),
      galaxyImage (loadEmbedded (BinaryData::galaxy_png, BinaryData::galaxy_pngSize, 1.7f, 1.35f)),
      zoomKnob (knobImage), spreadKnob (knobImage), distanceKnob (knobImage), motionKnob (knobImage),
      settings (p.parameters),
      presetBrowser (p)
{
    const std::array<const char*, 4> ids { "zoom", "spread", "distance", "motion" };
    const std::array<NeonKnob*, 4> knobs { &zoomKnob, &spreadKnob, &distanceKnob, &motionKnob };
    for (size_t i = 0; i < knobs.size(); ++i)
    {
        addAndMakeVisible (*knobs[i]);
        knobAttachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>
            (p.parameters, ids[i], *knobs[i]);
        knobs[i]->setTooltip (juce::String (ids[i]) + " - drag to adjust, double-click to reset");
        knobs[i]->setDoubleClickReturnValue (true, i == 0 ? 72.0 : i == 1 ? 58.0 : i == 2 ? 63.0 : 41.0);
    }
    addChildComponent (settings);
    addChildComponent (presetBrowser);
    presetBrowser.setVisible (false);
    presetBrowser.onSelect = [this] (int idx)
    {
        // Load the chosen preset but keep the browser open so the user stays on
        // the preset page and sees the selected card highlighted; BACK closes it.
        chooseProgram (idx);
        repaint();
    };
    presetBrowser.onClose = [this] { closePresetBrowser(); };
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

    // The preset browser overlays the whole window; it draws in base
    // coordinates using the same uniform transform as the main paint().
    presetBrowser.setBounds (0, 0, getWidth(), getHeight());
    presetBrowser.setScale (contentScale, contentOffsetX, contentOffsetY);
}

void DATelescopeAudioProcessorEditor::openPresetBrowser()
{
    presetBrowser.setVisible (true);
    presetBrowser.setCategory (0);
    presetBrowser.toFront (true);
    repaint();
}

void DATelescopeAudioProcessorEditor::closePresetBrowser()
{
    presetBrowser.setVisible (false);
    repaint();
}

void DATelescopeAudioProcessorEditor::paintText (juce::Graphics& g, const juce::String& txt,
                                                   float x, float y, float w, float h, float size,
                                                   juce::Colour colour, bool centre)
{
    paintTextFitted (g, txt, x, y, w, h, size, colour, centre);
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
    // Fill the whole window with the panel's dark tone so the letterbox bands
    // (top/bottom when the window isn't 16:9) blend in seamlessly instead of
    // showing as stark black.
    g.fillAll (juce::Colour (0xff060610));
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
    paintText (g, "SPATIAL FX ENGINE", 1017, 70, 183, 30, 11, muted);
    drawGearIcon (g, {1230.0f, 70.0f, 41.0f, 31.0f}, ivory);

    paintText (g, "T E L E S C O P E   F X", 432, 127, 576, 51, 27, juce::Colour (0xffe5cbff));
    // Draw the separator dots as vector circles (not font glyphs) to avoid tofu boxes.
    paintText (g, "S P A C E", 474, 173, 150, 23, 11, muted);
    paintText (g, "M O T I O N", 640, 173, 150, 23, 11, muted);
    paintText (g, "D I M E N S I O N", 806, 173, 163, 23, 11, muted);
    g.setColour (neon.withAlpha (0.8f));
    g.fillEllipse (628.0f, 184.0f, 3.0f, 3.0f);
    g.fillEllipse (794.0f, 184.0f, 3.0f, 3.0f);

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
        // The preset name area opens the full preset browser page (names +
        // categories) instead of a popup menu.
        if (x >= 596 && x <= 941) { openPresetBrowser(); return; }
        if (x >= 1220 && x <= 1290) { settings.setVisible (! settings.isVisible()); return; }
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
