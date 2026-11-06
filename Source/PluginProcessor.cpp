#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
ReTrektorAudioProcessor::ReTrektorAudioProcessor()
#if RETREKTOR_INSTRUMENT_FALLBACK
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true))
#else
    : AudioProcessor (BusesProperties())
#endif
{
    syncPlaybackModel();

    // the song the plug-in starts with is the first point of the undo line, so
    // Ctrl+Z from the very first edit reaches back to it
    resetUndoHistory();
}

ReTrektorAudioProcessor::~ReTrektorAudioProcessor() = default;

//==============================================================================
void ReTrektorAudioProcessor::prepareToPlay (double sampleRateIn, int samplesPerBlock)
{
    const juce::ScopedLock sl (modelLock);

    // The transport maths reads the rate back with getSampleRate(), so record
    // what the host asked for here: hosts reach this through the JUCE wrapper,
    // but a bare prepareToPlay call (tests, other hosts) has to set it too.
    setRateAndBufferSizeDetails (sampleRateIn, samplesPerBlock);

    // Keep note state: the synth can still be holding notes from a previous
    // transport run (hosts like Reaper may not call a MIDI effect while
    // stopped), so forget only the transport tracking.
    engine.forgetTransport();
}

void ReTrektorAudioProcessor::releaseResources()
{
    sendPanic();
}

bool ReTrektorAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    juce::ignoreUnused (layouts);
    return true;
}

void ReTrektorAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = juce::jmax (1, buffer.getNumSamples());

    // Keep the audio buffer silent (the plugin is MIDI only).
    buffer.clear();

    rtr::PlaybackEngine::Transport transport;

    if (auto* playHead = getPlayHead())
    {
        if (auto position = playHead->getPosition())
        {
            transport.playing = position->getIsPlaying();
            transport.bpm = position->getBpm().orFallback (120.0);

            if (auto ppq = position->getPpqPosition())
                transport.ppq = *ppq;

            if (auto time = position->getTimeInSeconds())
                transportSeconds.store (*time);

            if (auto sig = position->getTimeSignature())
            {
                timeSigNum.store (sig->numerator);
                timeSigDen.store (sig->denominator);
            }
        }
    }

    transportPpq.store (transport.ppq);
    transportBpm.store (transport.bpm);
    transportPlaying.store (transport.playing);

    // Split the block into MIDI output (forwarded input + generated events).
    juce::MidiBuffer output;

    // Copy the snapshot pointer under a tiny spin lock; the audio thread never
    // blocks and never reads the live song while the editor is editing it.
    std::shared_ptr<const rtr::Song> snapshot;

    {
        const juce::SpinLock::ScopedTryLockType sl (playbackLock);

        if (sl.isLocked())
            snapshot = playbackSong;
    }

    if (snapshot != nullptr)
    {
        engine.process (*snapshot, output, transport, numSamples, getSampleRate());

        // Pass through incoming MIDI (live playing / upstream sequencers).
        if (! midiMessages.isEmpty())
        {
            for (const auto metadata : midiMessages)
                output.addEvent (metadata.getMessage(), metadata.samplePosition);
        }

        keyboardState.processNextMidiBuffer (midiMessages, 0, numSamples, false);
    }
    else
    {
        output.swapWith (midiMessages);
    }

    midiMessages.swapWith (output);
}

//==============================================================================
ReTrektorAudioProcessor::TransportSnapshot ReTrektorAudioProcessor::getTransport() const
{
    TransportSnapshot s;
    s.playing = transportPlaying.load();
    s.ppq = transportPpq.load();
    s.bpm = transportBpm.load();
    s.timeSeconds = transportSeconds.load();
    s.timeSigNumerator = timeSigNum.load();
    s.timeSigDenominator = timeSigDen.load();
    s.timeSigValid = s.timeSigNumerator > 0;

    if (s.timeSigValid && s.timeSigDenominator > 0)
        s.ppqPerBar = (double) s.timeSigNumerator * 4.0 / (double) s.timeSigDenominator;
    else
        s.ppqPerBar = 4.0;

    s.barStartPpq = std::floor (s.ppq / juce::jmax (1.0e-9, s.ppqPerBar)) * s.ppqPerBar;

    return s;
}

void ReTrektorAudioProcessor::sendPanic()
{
    const juce::ScopedLock sl (modelLock);
    engine.reset();
}

void ReTrektorAudioProcessor::syncPlaybackModel()
{
    auto copy = std::make_shared<rtr::Song> (song);

    // playback only reads rows inside each pattern's length, so the snapshot can
    // drop the hidden capacity rows kept by pattern resizing
    for (auto& p : copy->patterns)
        p.rows.resize ((size_t) p.length);

    const juce::SpinLock::ScopedLockType sl (playbackLock);
    playbackSong = std::move (copy);
}

void ReTrektorAudioProcessor::notifySongChanged()
{
    if (onSongChanged)
        onSongChanged();
}

//==============================================================================
juce::AudioProcessorEditor* ReTrektorAudioProcessor::createEditor()
{
    return new ReTrektorAudioProcessorEditor (*this);
}

//==============================================================================
void ReTrektorAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::ValueTree tree;

    {
        const juce::ScopedLock sl (modelLock);
        tree = song.toValueTree();
    }

    if (auto xml = tree.createXml())
        copyXmlToBinary (*xml, destData);
}

void ReTrektorAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);

    if (xml == nullptr)
        return;

    auto tree = juce::ValueTree::fromXml (*xml);

    if (! tree.isValid())
        return;

    {
        const juce::ScopedLock sl (modelLock);
        song.fromValueTree (tree);
        engine.reset();
    }

    syncPlaybackModel();

    // a state the host hands back starts a fresh undo line: walking Ctrl+Z
    // through states from before the reload would mean nothing
    resetUndoHistory();

    notifySongChanged();
}

//==============================================================================
//== Undo history ================================================================
// Undo belongs to the plug-in and not to the host: the VST3 spec hands a
// plug-in no undo of its own, so the model changes the editor makes append the
// state each one produced to a line here and Ctrl+Z / Ctrl+Y walk along it. The
// states are saved in the form the host's state blocks use, which is what makes
// one step back restore the same model a reload of that saved state would have.
// The depth the line keeps comes from the Settings tab (Settings::undoSteps)
// and from a byte budget, so a pattern big enough that the states do not fit is
// allowed fewer of them instead of the plug-in growing without bound. States
// are counted as the UTF-8 size of their XML; JUCE holds strings as UTF-16, so
// the memory the budget allows is up to about twice the states it counts.
constexpr std::size_t kUndoByteBudget = 32u * 1024u * 1024u;

juce::String ReTrektorAudioProcessor::serialiseSong() const
{
    return song.toValueTree().toXmlString();
}

void ReTrektorAudioProcessor::recountUndoBytes()
{
    undoBytes = 0;

    for (const auto& state : undoStates)
        undoBytes += state.bytes;
}

void ReTrektorAudioProcessor::anchorUndoLine()
{
    const auto xml = serialiseSong();
    undoStates.clear();
    undoStates.push_back ({ xml, xml.getNumBytesAsUTF8() });
    undoCursor = 0;
    recountUndoBytes();
}

void ReTrektorAudioProcessor::resetUndoHistory()
{
    const juce::ScopedLock sl (modelLock);
    anchorUndoLine();
}

void ReTrektorAudioProcessor::pushUndoPoint()
{
    const juce::ScopedLock sl (modelLock);

    if (song.settings.undoSteps <= 0)
    {
        // undoing switched off in the Settings tab also stops the plug-in from
        // holding states at all, and leaves the current model as the anchor a
        // raised depth can later start walking back from
        anchorUndoLine();
        return;
    }

    const auto xml = serialiseSong();

    if (xml.isEmpty())
        return;

    // an action that left the model exactly as it was gets no point of its own,
    // so a paste whose cells did not fit or a transpose at the range's edge
    // leaves the Ctrl+Z depth the user has untouched
    if (undoCursor < (int) undoStates.size()
        && undoStates[(size_t) undoCursor].xml == xml)
        return;

    // the action being recorded forgets everything the cursor had walked back
    // over, which is what keeps Ctrl+Y able to retrace only the undone steps
    undoStates.resize ((size_t) (undoCursor + 1));
    undoStates.push_back ({ xml, xml.getNumBytesAsUTF8() });
    ++undoCursor;
    trimUndoLine();
}

void ReTrektorAudioProcessor::trimUndoLine()
{
    const int steps = song.settings.undoSteps;

    // the state the model is in and the ones Ctrl+Y would walk forward to stay
    // in the line: what gets dropped is what sits behind the cursor, oldest
    // first, and once the cursor is at the line's start only redo states are
    // left, so the newest of those drop out instead
    while (undoStates.size() > 1
           && ((undoCursor > steps && undoCursor > 0) || undoBytes > kUndoByteBudget))
    {
        if (undoCursor > 0)
        {
            undoStates.erase (undoStates.begin());
            --undoCursor;
        }
        else
        {
            undoStates.pop_back();

            if (undoCursor >= (int) undoStates.size())
                undoCursor = juce::jmax (0, (int) undoStates.size() - 1);
        }

        recountUndoBytes();
    }
}

bool ReTrektorAudioProcessor::stepUndoHistory (int delta)
{
    const juce::ScopedLock sl (modelLock);

    // lowering the depth in the Settings tab takes effect here as well, so the
    // points above what the new number asks for are forgotten the next time
    // the line is walked rather than only when the next action appends
    trimUndoLine();

    const int next = undoCursor + delta;

    if (next < 0 || next >= (int) undoStates.size())
        return false;

    // what the saved state does not carry is not rewound either: the pattern
    // clipboard, the edit playhead the timeline clicks move and the position
    // mirrored from the host keep their current values, so an undo stays a
    // rewind of the song rather than a jump of where the user is working in it
    auto keptClipboard = std::move (song.clipboard);
    const double editPlayhead = song.editPlayheadBeats;
    const double mirror = song.transportMirrorBeats;

    const auto tree = juce::ValueTree::fromXml (undoStates[(size_t) next].xml);

    if (! tree.isValid())
        return false;

    song.fromValueTree (tree);
    song.clipboard = std::move (keptClipboard);
    song.editPlayheadBeats = editPlayhead;
    song.transportMirrorBeats = mirror;

    undoCursor = next;
    return true;
}

bool ReTrektorAudioProcessor::undo()
{
    if (! stepUndoHistory (-1))
        return false;

    // the same path a cell edit takes: the snapshot the audio thread reads is
    // refreshed and the engine keeps its note and command state, so rows after
    // the playhead play the restored data while whatever was already sent to
    // the host stays where it was
    syncPlaybackModel();
    return true;
}

bool ReTrektorAudioProcessor::redo()
{
    if (! stepUndoHistory (1))
        return false;

    syncPlaybackModel();
    return true;
}
//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ReTrektorAudioProcessor();
}
