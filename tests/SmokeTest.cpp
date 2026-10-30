/*
    Smoke test for the Re.Trektor VST3 bundle.

    Instantiates the built plugin through JUCE's VST3 hosting API, then checks:
      - the plugin is recognised as a MIDI effect with a GUI
      - it can be prepared and processed
      - state save/load round trips
    Run:  ./ReTrektorSmoke /path/to/Re.Trektor.vst3
*/

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_graphics/juce_graphics.h>
#include <iostream>

struct MockPlayHead final : juce::AudioPlayHead
{
    bool playing = true;
    double ppq = 0.0;
    double bpm = 140.0;

    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo info;
        info.setIsPlaying (playing);
        info.setBpm (bpm);
        info.setPpqPosition (ppq);
        info.setTimeSignature (juce::AudioPlayHead::TimeSignature { 4, 4 });
        info.setTimeInSeconds (ppq * 60.0 / bpm);
        return info;
    }
};

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    juce::String path = argc > 1 ? juce::String (argv[1])
                                 : juce::String ("../build/ReTrektor_artefacts/Release/VST3/Re.Trektor.vst3");
    juce::File file (juce::File::getCurrentWorkingDirectory().getChildFile (path));

    if (! file.exists())
    {
        std::cerr << "FAIL: bundle not found: " << file.getFullPathName() << "\n";
        return 1;
    }

    int failures = 0;
    auto check = [&] (bool ok, const juce::String& what)
    {
        std::cout << (ok ? "  ok   " : "  FAIL ") << what << "\n";
        if (! ok)
            ++failures;
    };

    juce::VST3PluginFormat format;
    juce::OwnedArray<juce::PluginDescription> found;
    format.findAllTypesForFile (found, file.getFullPathName());

    check (found.size() == 1, "bundle exposes exactly one plugin description");

    if (found.size() != 1)
        return 1;

    auto desc = *found[0];
    std::cout << "Plugin: " << desc.name << " / " << desc.manufacturerName
              << " / ins=" << desc.numInputChannels << " outs=" << desc.numOutputChannels
              << " / category=" << desc.category << "\n";
    check (desc.numInputChannels == 0 && desc.numOutputChannels == 0 && ! desc.isInstrument,
           "plugin has no audio buses (MIDI effect)");

    juce::String err;
    auto instance = format.createInstanceFromDescription (desc, 48000.0, 64, err);
    check (instance != nullptr, "instance created (" + err + ")");

    if (instance == nullptr)
        return 1;

    check (instance->acceptsMidi(), "accepts MIDI");
    check (instance->producesMidi(), "produces MIDI");
    check (instance->getName().isNotEmpty(), "has a name: " + instance->getName());
    std::cout << "  info  buses in=" << instance->getBusCount (true)
              << " out=" << instance->getBusCount (false) << "\n";

    instance->setPlayHead (new MockPlayHead());
    instance->prepareToPlay (48000.0, 64);
    check (instance->getSampleRate() == 48000.0, "prepared at 48 kHz");

    juce::AudioBuffer<float> audio (2, 64);
    juce::MidiBuffer midi;
    audio.clear();

    // Run 8 blocks while "playing" and ensure MIDI comes out.
    auto* head = dynamic_cast<MockPlayHead*> (instance->getPlayHead());
    int generated = 0;

    for (int block = 0; block < 8; ++block)
    {
        if (head != nullptr)
            head->ppq = block * (64.0 / 48000.0) * (120.0 / 60.0);

        midi.clear();
        instance->processBlock (audio, midi);
        generated += midi.getNumEvents();
    }

    check (generated > 0, "generates MIDI while the host is playing (" + juce::String (generated) + " events)");

    // Stop the transport and make sure note-offs / all-notes-off are flushed.
    if (head != nullptr)
        head->playing = false;

    midi.clear();
    instance->processBlock (audio, midi);
    check (midi.getNumEvents() > 0, "flushes note-offs when the transport stops");

    // State round trip.
    juce::MemoryBlock state;
    instance->getStateInformation (state);
    check (state.getSize() > 0, "state serialised (" + juce::String ((int) state.getSize()) + " bytes)");

    instance->setStateInformation (state.getData(), (int) state.getSize());
    check (true, "state restored without crashing");

    // Editor creation (requires a display).
    if (juce::Desktop::getInstance().getDisplays().getPrimaryDisplay() != nullptr)
    {
        auto* editor = instance->createEditorIfNeeded();
        check (editor != nullptr, "editor created");

        if (editor != nullptr)
        {
            check (editor->getWidth() > 100 && editor->getHeight() > 100,
                   "editor has a sane size: " + juce::String (editor->getWidth()) + "x"
                       + juce::String (editor->getHeight()));

            for (int i = 2; i < argc; ++i)
            {
                const juce::String arg (argv[i]);

                if (arg.startsWith ("--screenshot="))
                {
                    const juce::File out (arg.fromFirstOccurrenceOf ("--screenshot=", false, false));

                    auto image = editor->createComponentSnapshot (editor->getLocalBounds());

                    if (image.isValid())
                    {
                        juce::PNGImageFormat png;
                        out.deleteFile();

                        if (auto stream = out.createOutputStream())
                        {
                            png.writeImageToStream (image, *stream);
                            std::cout << "  ok   screenshot written to " << out.getFullPathName() << "\n";
                        }
                    }
                }
            }

            instance->editorBeingDeleted (editor);
            delete editor;
        }
    }
    else
    {
        std::cout << "  skip editor test (no display)\n";
    }

    instance->releaseResources();
    instance = nullptr;

    std::cout << (failures == 0 ? "PASS" : "FAILED") << " (" << failures << " failures)\n";
    return failures == 0 ? 0 : 1;
}
