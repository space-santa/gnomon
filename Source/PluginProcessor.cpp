// SPDX-License-Identifier: AGPL-3.0-or-later

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Euclid.h"

#include <cmath>

namespace
{
constexpr double rateInQuarterNotes[] { 1.0, 0.5, 0.25, 0.125 };

// Note lengths as a fraction of one step. Drum Rack one-shots ignore this, but
// pitched instruments will hear it.
constexpr double gateFraction = 0.5;

// Default notes match Ableton Drum Rack's standard layout (C1 = 36).
struct LaneDefaults
{
    const char* name;
    int steps, pulses, rotation, note;
};

constexpr LaneDefaults laneDefaults[GnomonProcessor::numLanes] {
    { "Kick",       16, 4, 0, 36 }, // four on the floor
    { "Snare",      16, 2, 4, 38 }, // backbeat on 2 and 4
    { "Closed hat", 16, 8, 1, 42 }, // every other sixteenth, off the beat
    { "Open hat",   16, 4, 2, 46 }, // offbeat eighths
};

// Note names use Live's convention, where middle C (60) is C3.
juce::String noteToText (int note)
{
    return juce::MidiMessage::getMidiNoteName (note, true, true, 3);
}

// Accepts a plain number ("36") or a note name ("C1", "F#1", "Bb-1").
int textToNote (const juce::String& text)
{
    const auto t = text.trim().toUpperCase();

    if (t.isEmpty() || t[0] < 'A' || t[0] > 'G')
        return juce::jlimit (0, 127, t.getIntValue());

    constexpr int semitoneOfLetter[] { 9, 11, 0, 2, 4, 5, 7 }; // A B C D E F G
    int semitone = semitoneOfLetter[t[0] - 'A'];
    int i = 1;

    if (t[i] == '#')      { ++semitone; ++i; }
    else if (t[i] == 'B') { --semitone; ++i; } // a flat ("Bb" is upper-cased to "BB")

    const int octave = t.substring (i).getIntValue();
    return juce::jlimit (0, 127, (octave + 2) * 12 + semitone);
}
} // namespace

juce::String GnomonProcessor::getLaneName (int lane)
{
    return laneDefaults[juce::jlimit (0, numLanes - 1, lane)].name;
}

GnomonProcessor::GnomonProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      params (*this, nullptr, "Gnomon", createParameterLayout())
{
    rate = params.getRawParameterValue ("rate");

    for (int i = 0; i < numLanes; ++i)
    {
        const auto n = juce::String (i + 1);
        auto& lane = lanes[(size_t) i];
        lane.steps    = params.getRawParameterValue ("steps" + n);
        lane.pulses   = params.getRawParameterValue ("pulses" + n);
        lane.rotation = params.getRawParameterValue ("rotate" + n);
        lane.note     = params.getRawParameterValue ("note" + n);
        lane.velocity = params.getRawParameterValue ("velocity" + n);
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout GnomonProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "rate", 1 }, "Rate",
                                                              juce::StringArray { "1/4", "1/8", "1/16", "1/32" }, 2));

    const auto noteName = juce::AudioParameterIntAttributes{}
                              .withStringFromValueFunction ([] (int value, int) { return noteToText (value); })
                              .withValueFromStringFunction ([] (const juce::String& text) { return textToNote (text); });

    for (int i = 0; i < numLanes; ++i)
    {
        const auto& d = laneDefaults[i];
        const auto id = [i] (const char* base) { return juce::ParameterID { base + juce::String (i + 1), 1 }; };
        const auto name = [&d] (const char* label) { return juce::String (d.name) + " " + label; };

        layout.add (std::make_unique<juce::AudioParameterInt> (id ("steps"),    name ("steps"),    1, 32,  d.steps),
                    std::make_unique<juce::AudioParameterInt> (id ("pulses"),   name ("pulses"),   0, 32,  d.pulses),
                    std::make_unique<juce::AudioParameterInt> (id ("rotate"),   name ("rotate"),   0, 31,  d.rotation),
                    std::make_unique<juce::AudioParameterInt> (id ("note"),     name ("note"),     0, 127, d.note, noteName),
                    std::make_unique<juce::AudioParameterInt> (id ("velocity"), name ("velocity"), 1, 127, 100));
    }

    return layout;
}

void GnomonProcessor::prepareToPlay (double sampleRate, int)
{
    currentSampleRate = sampleRate;
    lastTriggeredStep = std::numeric_limits<juce::int64>::min();
    wasPlaying = false;

    for (auto& lane : lanes)
        lane.activeNote = -1;
}

bool GnomonProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

void GnomonProcessor::releaseLane (Lane& lane, juce::MidiBuffer& midi, int sampleOffset)
{
    midi.addEvent (juce::MidiMessage::noteOff (midiChannel, lane.activeNote), sampleOffset);
    lane.activeNote = -1;
}

void GnomonProcessor::releaseAllLanes (juce::MidiBuffer& midi, int sampleOffset)
{
    for (auto& lane : lanes)
        if (lane.activeNote >= 0)
            releaseLane (lane, midi, sampleOffset);
}

void GnomonProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    buffer.clear(); // no audio, just MIDI
    midi.clear();   // only emit our own notes

    const int numSamples = buffer.getNumSamples();

    if (numSamples == 0)
        return;

    auto* playHead = getPlayHead();
    const auto position = playHead != nullptr ? playHead->getPosition()
                                              : juce::Optional<juce::AudioPlayHead::PositionInfo>{};
    const auto ppq = position ? position->getPpqPosition() : juce::Optional<double>{};
    const auto bpm = position ? position->getBpm() : juce::Optional<double>{};
    const bool playing = position && position->getIsPlaying() && ppq && bpm && *bpm > 0.0;

    const auto rateIndex = juce::jlimit (0, (int) std::size (rateInQuarterNotes) - 1, juce::roundToInt (rate->load()));
    const double stepPpq = rateInQuarterNotes[rateIndex];

    publishTransport (ppq, bpm, stepPpq, playing);

    if (! playing)
    {
        if (wasPlaying)
            releaseAllLanes (midi, 0);

        wasPlaying = false;
        lastTriggeredStep = std::numeric_limits<juce::int64>::min();
        return;
    }

    wasPlaying = true;

    const double ppqPerSample = *bpm / (60.0 * currentSampleRate);
    const double startPpq = *ppq;
    const double endPpq = startPpq + numSamples * ppqPerSample;
    const auto gateSamples = juce::jmax ((juce::int64) 1, (juce::int64) (gateFraction * stepPpq / ppqPerSample));

    // Find the step boundaries that fall inside this block. Steps are counted from the
    // start of the song, so the pattern stays locked to the host's bar grid.
    struct StepEvent
    {
        juce::int64 index;
        int sampleOffset;
    };

    std::array<StepEvent, 64> steps {};
    int numSteps = 0;

    for (auto s = (juce::int64) std::ceil (startPpq / stepPpq - 1.0e-9);
         (double) s * stepPpq < endPpq && numSteps < (int) steps.size();
         ++s)
    {
        // Rounding can put the same step at the end of one block and the start of the next.
        if (s == lastTriggeredStep)
            continue;

        const auto offset = (int) std::llround (((double) s * stepPpq - startPpq) / ppqPerSample);
        steps[(size_t) numSteps++] = { s, juce::jlimit (0, numSamples - 1, offset) };
        lastTriggeredStep = s;
    }

    for (auto& lane : lanes)
    {
        const int laneSteps = juce::roundToInt (lane.steps->load());
        const int lanePulses = juce::roundToInt (lane.pulses->load());
        const int laneRotation = juce::roundToInt (lane.rotation->load());
        const int laneNote = juce::roundToInt (lane.note->load());
        const auto laneVelocity = (juce::uint8) juce::jlimit (1, 127, juce::roundToInt (lane.velocity->load()));

        for (int i = 0; i < numSteps; ++i)
        {
            const auto& step = steps[(size_t) i];

            if (lane.activeNote >= 0 && lane.samplesUntilOff <= step.sampleOffset)
                releaseLane (lane, midi, (int) lane.samplesUntilOff);

            if (! euclid::isHit (step.index, laneSteps, lanePulses, laneRotation))
                continue;

            if (lane.activeNote >= 0)
                releaseLane (lane, midi, step.sampleOffset);

            midi.addEvent (juce::MidiMessage::noteOn (midiChannel, laneNote, laneVelocity), step.sampleOffset);
            lane.activeNote = laneNote;
            lane.samplesUntilOff = step.sampleOffset + gateSamples;
        }

        if (lane.activeNote >= 0 && lane.samplesUntilOff < numSamples)
            releaseLane (lane, midi, (int) lane.samplesUntilOff);

        if (lane.activeNote >= 0)
            lane.samplesUntilOff -= numSamples;
    }
}

void GnomonProcessor::publishTransport (const juce::Optional<double>& ppq, const juce::Optional<double>& bpm,
                                        double stepPpq, bool playing)
{
    const juce::SpinLock::ScopedTryLockType lock (transportLock);

    if (! lock.isLocked())
        return; // the UI is reading; skip this update rather than wait

    transport.valid = ppq.hasValue();
    transport.stepPosition = ppq ? *ppq / stepPpq : transport.stepPosition;
    transport.stepsPerSecond = bpm && *bpm > 0.0 ? *bpm / (60.0 * stepPpq) : 0.0;
    transport.playing = playing;
    transport.timeMs = juce::Time::getMillisecondCounterHiRes();
}

GnomonProcessor::TransportSnapshot GnomonProcessor::getTransportSnapshot() const
{
    const juce::SpinLock::ScopedLockType lock (transportLock);
    return transport;
}

juce::AudioProcessorEditor* GnomonProcessor::createEditor()
{
    return new GnomonEditor (*this);
}

void GnomonProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = params.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void GnomonProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (params.state.getType()))
            params.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new GnomonProcessor();
}
