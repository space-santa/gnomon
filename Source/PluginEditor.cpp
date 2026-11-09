// SPDX-License-Identifier: AGPL-3.0-or-later

#include "PluginEditor.h"
#include "Euclid.h"

#include <cmath>

namespace
{
constexpr int editorWidth = 940;
constexpr int editorHeight = 520;
constexpr float twoPi = juce::MathConstants<float>::twoPi;

double positiveModulo (double x, double m)
{
    const auto r = std::fmod (x, m);
    return r < 0.0 ? r + m : r;
}

juce::Rectangle<float> circle (juce::Point<float> centre, float diameter)
{
    return juce::Rectangle<float> (diameter, diameter).withCentre (centre);
}
} // namespace

//==============================================================================
void GnomonLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                                          float startAngle, float endAngle, juce::Slider& slider)
{
    constexpr float lineWidth = 3.0f;
    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (2.0f);
    const auto centre = bounds.getCentre();
    const auto radius = 0.5f * juce::jmin (bounds.getWidth(), bounds.getHeight()) - lineWidth;
    const auto valueAngle = startAngle + sliderPos * (endAngle - startAngle);
    const juce::PathStrokeType stroke (lineWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);

    juce::Path track;
    track.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, startAngle, endAngle, true);
    g.setColour (slider.findColour (juce::Slider::rotarySliderOutlineColourId));
    g.strokePath (track, stroke);

    if (sliderPos > 0.0f)
    {
        juce::Path value;
        value.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, startAngle, valueAngle, true);
        g.setColour (slider.findColour (juce::Slider::rotarySliderFillColourId));
        g.strokePath (value, stroke);
    }

    g.setColour (theme::text);
    g.drawLine ({ centre.getPointOnCircumference (radius * 0.2f, valueAngle),
                  centre.getPointOnCircumference (radius * 0.75f, valueAngle) },
                2.0f);
}

//==============================================================================
RingView::RingView (GnomonProcessor& p)
    : gnomon (p),
      rate (p.params.getParameter ("rate"))
{
    setOpaque (true);

    for (int i = 0; i < GnomonProcessor::numLanes; ++i)
    {
        const auto n = juce::String (i + 1);
        lanes[(size_t) i] = { p.params.getRawParameterValue ("steps" + n),
                              p.params.getRawParameterValue ("pulses" + n),
                              p.params.getRawParameterValue ("rotate" + n) };
    }
}

void RingView::paint (juce::Graphics& g)
{
    g.fillAll (theme::background);

    const auto bounds = getLocalBounds().toFloat().reduced (24.0f);
    const auto centre = bounds.getCentre();
    const auto outerRadius = 0.5f * juce::jmin (bounds.getWidth(), bounds.getHeight());
    const auto hubRadius = outerRadius * 0.16f;
    const auto bandWidth = (outerRadius - hubRadius) / (float) GnomonProcessor::numLanes;

    // Where the transport is right now. The audio thread only reports once per block, so
    // extrapolate from the last report to keep the hands moving smoothly between blocks.
    const auto transport = gnomon.getTransportSnapshot();
    auto position = transport.stepPosition;

    if (transport.playing)
    {
        const auto secondsSinceReport = (juce::Time::getMillisecondCounterHiRes() - transport.timeMs) / 1000.0;
        position += juce::jlimit (0.0, 0.25, secondsSinceReport) * transport.stepsPerSecond;
    }

    const auto currentStep = (juce::int64) std::floor (position);
    const auto stepPhase = (float) (position - std::floor (position)); // 0 at the start of a step, rising to 1

    // Hub, showing the step rate.
    g.setColour (theme::panel);
    g.fillEllipse (circle (centre, hubRadius * 2.0f));

    if (rate != nullptr)
    {
        g.setColour (theme::textDim);
        g.setFont (juce::FontOptions (13.0f));
        g.drawText (rate->getCurrentValueAsText(), circle (centre, hubRadius * 2.0f), juce::Justification::centred);
    }

    for (int lane = 0; lane < GnomonProcessor::numLanes; ++lane)
    {
        const auto& p = lanes[(size_t) lane];
        const int steps = juce::jmax (1, juce::roundToInt (p.steps->load()));
        const int pulses = juce::roundToInt (p.pulses->load());
        const int rotation = juce::roundToInt (p.rotation->load());

        const auto colour = theme::lanes[(size_t) lane];
        const auto radius = outerRadius - bandWidth * ((float) lane + 0.5f);
        const auto stepArc = twoPi * radius / (float) steps;
        const auto hitSize = juce::jmin (bandWidth * 0.5f, stepArc * 0.8f);
        const auto restSize = hitSize * 0.32f;

        g.setColour (colour.withAlpha (0.18f));
        g.drawEllipse (circle (centre, radius * 2.0f), 1.5f);

        // This lane's hand, under the dots so it never hides a hit.
        if (transport.valid)
        {
            const auto angle = (float) (twoPi * positiveModulo (position, steps) / steps);
            const auto from = centre.getPointOnCircumference (radius - bandWidth * 0.46f, angle);
            const auto to = centre.getPointOnCircumference (radius + bandWidth * 0.46f, angle);
            g.setColour (juce::Colours::white.withAlpha (transport.playing ? 0.8f : 0.3f));
            g.drawLine ({ from, to }, 2.0f);
        }

        const auto stepNow = (int) (((currentStep % steps) + steps) % steps);

        for (int i = 0; i < steps; ++i)
        {
            // Step 0 is at 12 o'clock, and steps run clockwise.
            const auto point = centre.getPointOnCircumference (radius, twoPi * (float) i / (float) steps);

            if (! euclid::isHit (i, steps, pulses, rotation))
            {
                g.setColour (colour.withAlpha (0.35f));
                g.fillEllipse (circle (point, restSize));
                continue;
            }

            // The hit under the hand flashes, then fades out over the step.
            const auto flash = transport.playing && i == stepNow ? std::pow (1.0f - stepPhase, 2.0f) : 0.0f;

            if (flash > 0.0f)
            {
                g.setColour (colour.withAlpha (0.35f * flash));
                g.fillEllipse (circle (point, hitSize * (1.0f + 1.4f * flash)));
            }

            g.setColour (colour.interpolatedWith (juce::Colours::white, 0.5f * flash));
            g.fillEllipse (circle (point, hitSize * (1.0f + 0.35f * flash)));
        }
    }
}

//==============================================================================
LaneStrip::LaneStrip (juce::AudioProcessorValueTreeState& state, int lane)
    : name (GnomonProcessor::getLaneName (lane)),
      colour (theme::lanes[(size_t) lane])
{
    const char* ids[numControls] { "steps", "pulses", "rotate", "note", "velocity" };
    const char* labels[numControls] { "Steps", "Pulses", "Rotate", "Note", "Velocity" };

    for (int i = 0; i < numControls; ++i)
    {
        auto& control = controls[(size_t) i];
        auto& slider = control.slider;
        const auto paramID = ids[i] + juce::String (lane + 1);

        control.label = juce::String (labels[i]).toUpperCase();

        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 56, 18);
        slider.setColour (juce::Slider::rotarySliderFillColourId, colour);
        slider.setColour (juce::Slider::rotarySliderOutlineColourId, theme::outline);
        slider.setColour (juce::Slider::thumbColourId, colour);
        slider.setColour (juce::Slider::textBoxTextColourId, theme::text);
        slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        slider.setColour (juce::Slider::textBoxHighlightColourId, colour.withAlpha (0.4f));
        addAndMakeVisible (slider);

        control.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, paramID, slider);

        // Double-click a knob to reset it.
        if (auto* param = state.getParameter (paramID))
            slider.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));
    }
}

void LaneStrip::paint (juce::Graphics& g)
{
    g.setColour (theme::panel);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 8.0f);

    const auto nameBounds = nameArea.toFloat();
    g.setColour (colour);
    g.fillEllipse (circle ({ nameBounds.getX() + 5.0f, nameBounds.getCentreY() }, 10.0f));

    g.setColour (theme::text);
    g.setFont (juce::FontOptions (14.0f, juce::Font::bold));
    g.drawFittedText (name, nameArea.withTrimmedLeft (16), juce::Justification::centredLeft, 2);

    g.setColour (theme::textDim);
    g.setFont (juce::FontOptions (10.5f));

    for (const auto& control : controls)
    {
        const auto labelArea = control.slider.getBounds().withHeight (14).translated (0, -14);
        g.drawText (control.label, labelArea, juce::Justification::centred);
    }
}

void LaneStrip::resized()
{
    auto area = getLocalBounds().reduced (12, 6);
    nameArea = area.removeFromLeft (88);
    area.removeFromTop (14); // control labels

    const auto columnWidth = area.getWidth() / numControls;

    for (auto& control : controls)
        control.slider.setBounds (area.removeFromLeft (columnWidth).reduced (2, 0));
}

//==============================================================================
GnomonEditor::GnomonEditor (GnomonProcessor& p)
    : AudioProcessorEditor (p),
      gnomon (p),
      ringView (p)
{
    setLookAndFeel (&lookAndFeel);
    addAndMakeVisible (ringView);

    if (auto* rate = dynamic_cast<juce::AudioParameterChoice*> (p.params.getParameter ("rate")))
        rateBox.addItemList (rate->choices, 1);

    rateBox.setColour (juce::ComboBox::backgroundColourId, theme::panel);
    rateBox.setColour (juce::ComboBox::outlineColourId, theme::outline);
    rateBox.setColour (juce::ComboBox::textColourId, theme::text);
    rateBox.setColour (juce::ComboBox::arrowColourId, theme::textDim);
    addAndMakeVisible (rateBox);
    rateAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (p.params, "rate", rateBox);

    for (int i = 0; i < GnomonProcessor::numLanes; ++i)
    {
        strips[(size_t) i] = std::make_unique<LaneStrip> (p.params, i);
        addAndMakeVisible (*strips[(size_t) i]);
    }

    setSize (editorWidth, editorHeight);
    startTimerHz (60);
}

GnomonEditor::~GnomonEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void GnomonEditor::timerCallback()
{
    ringView.repaint();
}

void GnomonEditor::paint (juce::Graphics& g)
{
    g.fillAll (theme::background);

    auto title = titleArea.toFloat();

    g.setColour (theme::text);
    g.setFont (juce::Font (juce::FontOptions (26.0f, juce::Font::bold)).withExtraKerningFactor (0.2f));
    g.drawText ("GNOMON", title.removeFromTop (title.getHeight() * 0.62f), juce::Justification::bottomLeft);

    g.setColour (theme::textDim);
    g.setFont (juce::FontOptions (12.0f));
    g.drawText ("Euclidean drum sequencer", title, juce::Justification::topLeft);
    g.drawText ("RATE", rateLabelArea, juce::Justification::centredRight);
}

void GnomonEditor::resized()
{
    auto area = getLocalBounds();
    ringView.setBounds (area.removeFromLeft (area.getHeight()));

    auto panel = area.withTrimmedRight (16).withTrimmedTop (12).withTrimmedBottom (16);

    auto header = panel.removeFromTop (56);
    rateBox.setBounds (header.removeFromRight (96).withSizeKeepingCentre (96, 28));
    rateLabelArea = header.removeFromRight (48);
    titleArea = header;

    panel.removeFromTop (8);

    constexpr int gap = 8;
    const auto stripHeight = (panel.getHeight() - gap * (GnomonProcessor::numLanes - 1)) / GnomonProcessor::numLanes;

    for (auto& strip : strips)
    {
        strip->setBounds (panel.removeFromTop (stripHeight));
        panel.removeFromTop (gap);
    }
}
