// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#include "PluginProcessor.h"

#include <array>
#include <memory>

namespace theme
{
inline const juce::Colour background { 0xff121318 };
inline const juce::Colour panel      { 0xff1b1d24 };
inline const juce::Colour outline    { 0xff2e313c };
inline const juce::Colour text       { 0xffe8e8ec };
inline const juce::Colour textDim    { 0xff8b8e9a };

inline const std::array<juce::Colour, GnomonProcessor::numLanes> lanes {
    juce::Colour { 0xffff7a45 }, // kick: orange
    juce::Colour { 0xffff4d8d }, // snare: pink
    juce::Colour { 0xff3cc8f0 }, // closed hat: cyan
    juce::Colour { 0xff9be15d }, // open hat: lime
};
} // namespace theme

/** Thin-arc knobs with a pointer, coloured per slider. */
class GnomonLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos,
                           float startAngle, float endAngle, juce::Slider&) override;
};

/** One ring per lane (lane 1 outermost), with a dot per step. Hits are large dots; each
    ring's hand shows where that lane is in its own cycle. Lanes with equal step counts
    have aligned hands, so with the defaults they form a single dial. */
class RingView final : public juce::Component
{
public:
    explicit RingView (GnomonProcessor&);

    void paint (juce::Graphics&) override;

private:
    struct LaneParams
    {
        std::atomic<float>* steps = nullptr;
        std::atomic<float>* pulses = nullptr;
        std::atomic<float>* rotation = nullptr;
    };

    GnomonProcessor& gnomon;
    std::array<LaneParams, GnomonProcessor::numLanes> lanes;
    juce::RangedAudioParameter* rate = nullptr;
};

/** The knobs for one lane. */
class LaneStrip final : public juce::Component
{
public:
    LaneStrip (juce::AudioProcessorValueTreeState&, int lane);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Control
    {
        juce::Slider slider;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
        juce::String label;
    };

    static constexpr int numControls = 5;

    std::array<Control, numControls> controls;
    juce::String name;
    juce::Colour colour;
    juce::Rectangle<int> nameArea;
};

class GnomonEditor final : public juce::AudioProcessorEditor,
                           private juce::Timer
{
public:
    explicit GnomonEditor (GnomonProcessor&);
    ~GnomonEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    GnomonLookAndFeel lookAndFeel; // declared first so it outlives the components using it
    GnomonProcessor& gnomon;
    RingView ringView;
    juce::ComboBox rateBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> rateAttachment;
    std::array<std::unique_ptr<LaneStrip>, GnomonProcessor::numLanes> strips;
    juce::Rectangle<int> titleArea, rateLabelArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GnomonEditor)
};
