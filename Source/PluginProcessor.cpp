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
    notifySongChanged();
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ReTrektorAudioProcessor();
}
