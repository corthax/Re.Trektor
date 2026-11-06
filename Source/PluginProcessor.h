#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Model.h"
#include "PlaybackEngine.h"
#include <memory>

//==============================================================================
/** One state of the undo line: the saved song as the XML the host's state
    blocks use, which is why undoing restores exactly what a reload of that
    saved state would. The byte count drives the budget that drops the states
    a huge pattern would otherwise keep by the score. */
struct UndoState
{
    juce::String xml;
    std::size_t bytes = 0;
};

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

    //== Undo history ========================================================
    /** Records the state the model is in now as the newest point of the undo
        line, which is where every model change ends up: an action that wrote
        nothing (a paste whose cells did not fit, a transpose at the range's
        edge) makes no point, and anything the user had walked back over with
        Ctrl+Z is forgotten by the next action, the way redo lines work. The
        depth comes from the Settings tab (Settings::undoSteps) and a byte
        budget drops the oldest states as well, so a huge pattern keeps fewer
        of them instead of the plug-in growing without bound. */
    void pushUndoPoint();

    /** Steps the model back one point of the undo line (Ctrl+Z) and refreshes
        the playback snapshot, the way a cell edit does: the engine keeps its
        note and command state, so rows past the playhead play the restored
        data while what was already sent to the host stays as it was. Returns
        whether the model changed, which is false at the line's start. */
    bool undo();

    /** Walks the undo line forward again (Ctrl+Y). */
    bool redo();

    /** Starts a fresh undo line whose single state is the model now, which is
        what a saved state handed back by the host leaves: undo across a
        project reload would mean nothing. */
    void resetUndoHistory();

    /** How many Ctrl+Z presses would still change the model, and how many
        Ctrl+Y presses would, so the tracker's menu can grey the pair out. */
    int getUndoDepth() const noexcept { return undoCursor; }
    int getUndoRedoCount() const noexcept
    { return juce::jmax (0, (int) undoStates.size() - 1 - undoCursor); }

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

    //== Undo line ============================================================
    // The states the model changes append, and the cursor in the middle of
    // them. They live on the plug-in rather than in the editor, because a host
    // such as Reaper closes a floating plug-in window by destroying it and
    // asks for a brand new editor when it is opened again: the points the
    // user stepped through have to be where the new editor finds them too.
    std::vector<UndoState> undoStates;
    int undoCursor = 0; // which state of the line the model is in right now
    std::size_t undoBytes = 0; // what the line holds in total

    void trimUndoLine();
    bool stepUndoHistory (int delta);
    juce::String serialiseSong() const;
    void recountUndoBytes();
    void anchorUndoLine(); // the line's single state, taken with the model lock

    double lastPpq = 0.0;
    bool hadPpq = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ReTrektorAudioProcessor)
};
