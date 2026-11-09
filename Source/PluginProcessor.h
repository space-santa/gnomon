// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <atomic>
#include <limits>

class GnomonProcessor final : public juce::AudioProcessor
{
public:
    static constexpr int numLanes = 4;
    static constexpr int midiChannel = 1;

    GnomonProcessor();
    ~GnomonProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::String getLaneName (int lane);

    /** Where the host's transport was at the last audio block, for the UI to animate. */
    struct TransportSnapshot
    {
        double stepPosition = 0.0;   // in steps (at the current rate) since the start of the song
        double stepsPerSecond = 0.0; // for extrapolating between audio blocks
        double timeMs = 0.0;         // juce::Time::getMillisecondCounterHiRes() when captured
        bool playing = false;
        bool valid = false;          // false until the host has reported a position
    };

    /** Safe to call from the message thread. */
    TransportSnapshot getTransportSnapshot() const;

    juce::AudioProcessorValueTreeState params;

private:
    struct Lane
    {
        std::atomic<float>* steps = nullptr;
        std::atomic<float>* pulses = nullptr;
        std::atomic<float>* rotation = nullptr;
        std::atomic<float>* note = nullptr;
        std::atomic<float>* velocity = nullptr;

        // Audio-thread state for the note this lane is currently holding.
        int activeNote = -1;
        juce::int64 samplesUntilOff = 0;
    };

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    static void releaseLane (Lane& lane, juce::MidiBuffer& midi, int sampleOffset);
    void releaseAllLanes (juce::MidiBuffer& midi, int sampleOffset);
    void publishTransport (const juce::Optional<double>& ppq, const juce::Optional<double>& bpm, double stepPpq, bool playing);

    // The audio thread only ever try-locks this, so the UI can never block audio.
    mutable juce::SpinLock transportLock;
    TransportSnapshot transport;

    std::array<Lane, numLanes> lanes;
    std::atomic<float>* rate = nullptr;

    double currentSampleRate = 44100.0;
    juce::int64 lastTriggeredStep = std::numeric_limits<juce::int64>::min();
    bool wasPlaying = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GnomonProcessor)
};
