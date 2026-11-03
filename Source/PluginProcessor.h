#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Model.h"
#include "PlaybackEngine.h"
#include <memory>

//==============================================================================
class ReTrektorAudioProcessor final : public juce::AudioProcessor
{
public:
    ReTrektorAudioProcessor();
    ~ReTrektorAudioProcessor() override;

    //== AudioProcessor ========================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Re.Trektor"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return true; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //== Re.Trektor ============================================================
    rtr::Song& getSong() noexcept { return song; }
    rtr::PlaybackEngine& getEngine() noexcept { return engine; }

    struct TransportSnapshot
    {
        bool playing = false;
        double ppq = 0.0;
        double bpm = 120.0;
        double timeSeconds = 0.0;
        double barStartPpq = 0.0;
        bool timeSigValid = false;
        int timeSigNumerator = 4;
        int timeSigDenominator = 4;
        double ppqPerBar = 4.0;
    };

    TransportSnapshot getTransport() const;

    /** Guards the song model against concurrent access from the audio thread. */
    juce::CriticalSection& getModelLock() noexcept { return modelLock; }

    /** The size the last GUI this plug-in drew had. Hosts such as Reaper close a
        floating plug-in window by destroying it and ask for a brand new editor
        when the user opens it again, so the new editor starts from this size
        instead of always the built-in default. Empty until a GUI has been shown,
        and kept for the plug-in instance rather than saved with the song. */
    juce::Rectangle<int> getRememberedEditorBounds() const noexcept
    { return rememberedEditorBounds; }

    /** Called by the editor each time it is laid out, so the size the host last
        gave this GUI is the size a later editor starts from. */
    void rememberEditorBounds (juce::Rectangle<int> bounds) noexcept
    { rememberedEditorBounds = bounds; }

    /** Grabs all notes off (used when the transport stops from the editor). */
    void sendPanic();

    /** Copies the editable song into the playback snapshot used by the audio
        thread. Call on the message thread after any model change. */
    void syncPlaybackModel();

    /** Live notes coming from the host (for the on-screen keyboard). */
    juce::MidiKeyboardState keyboardState;

    std::function<void()> onSongChanged;

private:
    void notifySongChanged();

    rtr::Song song;
    rtr::PlaybackEngine engine;
    juce::CriticalSection modelLock;
    // Size of the last GUI shown, so a host that destroys the floating window
    // and later asks for another editor gets the same size back.
    juce::Rectangle<int> rememberedEditorBounds;

    // Immutable snapshot handed to the audio thread, so the engine never reads
    // the song while the editor is modifying it.
    juce::SpinLock playbackLock;
    std::shared_ptr<const rtr::Song> playbackSong;

    std::atomic<bool> transportPlaying { false };
    std::atomic<double> transportPpq { 0.0 };
    std::atomic<double> transportBpm { 120.0 };
    std::atomic<double> transportSeconds { 0.0 };
    std::atomic<int> timeSigNum { 4 }, timeSigDen { 4 };

    double lastPpq = 0.0;
    bool hadPpq = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ReTrektorAudioProcessor)
};
