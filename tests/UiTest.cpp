/*
    Headless UI interaction test for Re.Trektor.

    Builds the real editor without a host and drives the same component
    callbacks a mouse or keyboard would, then verifies that the model and the
    playback snapshot are updated. Covers:
      - note entry through the tracker keyboard handler
      - column visibility chip clicks
      - transpose buttons in the tools panel
      - timeline drag-and-drop from the pool
      - pool double-click placement with snapping
      - settings toggles
      - MIDI output reflecting editor edits
*/

#include "PluginEditor.h"
#include "ui/PoolComponent.h"
#include "ui/TimelineComponent.h"
#include "ui/TrackerComponent.h"
#include "ui/ToolsPanel.h"
#include "ui/SettingsPanel.h"
#include "ui/MacrosPanel.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>
#include <iostream>

namespace
{
struct MockPlayHead final : juce::AudioPlayHead
{
    bool playing = false;
    double ppq = 0.0;
    double bpm = 120.0;

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

juce::MouseEvent makeEvent (juce::Component& c, juce::Point<float> pos)
{
    auto source = juce::Desktop::getInstance().getMainMouseSource();
    const auto now = juce::Time::getCurrentTime();

    return juce::MouseEvent (source, pos, juce::ModifierKeys(), 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                             &c, &c, now, pos, now, 1, false);
}

template <typename T>
T* findChild (juce::Component& root)
{
    for (auto* child : root.getChildren())
    {
        if (auto* match = dynamic_cast<T*> (child))
            return match;

        if (auto* nested = findChild<T> (*child))
            return nested;
    }

    return nullptr;
}

juce::TextButton* findButton (juce::Component& root, const juce::String& text)
{
    for (auto* child : root.getChildren())
    {
        if (auto* button = dynamic_cast<juce::TextButton*> (child))
            if (button->getButtonText() == text)
                return button;

        if (auto* nested = findButton (*child, text))
            return nested;
    }

    return nullptr;
}

juce::ToggleButton* findToggle (juce::Component& root, const juce::String& text)
{
    for (auto* child : root.getChildren())
    {
        if (auto* toggle = dynamic_cast<juce::ToggleButton*> (child))
            if (toggle->getButtonText() == text)
                return toggle;

        if (auto* nested = findToggle (*child, text))
            return nested;
    }

    return nullptr;
}

bool hasNoteOn (const juce::MidiBuffer& midi, int note)
{
    for (const auto metadata : midi)
    {
        const auto m = metadata.getMessage();

        if (m.isNoteOn() && m.getNoteNumber() == note)
            return true;
    }

    return false;
}
} // namespace

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    int failures = 0;
    auto check = [&] (bool ok, const juce::String& what)
    {
        std::cout << (ok ? "  ok   " : "  FAIL ") << what << "\n";

        if (! ok)
            ++failures;
    };

    ReTrektorAudioProcessor proc;
    proc.setPlayHead (new MockPlayHead());
    proc.prepareToPlay (48000.0, 64);

    auto* head = dynamic_cast<MockPlayHead*> (proc.getPlayHead());
    check (head != nullptr, "mock playhead installed");

    if (head == nullptr)
        return 1;

    std::unique_ptr<juce::AudioProcessorEditor> editor (proc.createEditor());
    check (editor != nullptr, "editor created");

    if (editor == nullptr)
        return 1;

    auto* pool = findChild<rtr::PoolComponent> (*editor);
    auto* timeline = findChild<rtr::TimelineComponent> (*editor);
    auto* tracker = findChild<rtr::TrackerComponent> (*editor);
    auto* tools = findChild<rtr::ToolsPanel> (*editor);
    auto* settings = findChild<rtr::SettingsPanel> (*editor);

    check (pool != nullptr && timeline != nullptr && tracker != nullptr
               && tools != nullptr && settings != nullptr,
           "editor exposes pool, timeline, tracker, tools and settings");

    if (pool == nullptr || timeline == nullptr || tracker == nullptr
        || tools == nullptr || settings == nullptr)
        return 1;

    auto& song = proc.getSong();
    auto& p = song.patterns.front();

    //== Tracker keyboard entry ==============================================
    tracker->keyPressed (juce::KeyPress ('z', juce::ModifierKeys(), 'z'));
    check (p.cell (0, 0).note == 60, "z enters C-4 on row 0");

    tracker->keyPressed (juce::KeyPress ('q', juce::ModifierKeys(), 'q'));
    check (p.cell (1, 0).note == 72, "q enters C-5 on the next row");

    tracker->keyPressed (juce::KeyPress ('`', juce::ModifierKeys(), '`'));
    check (p.cell (2, 0).note == rtr::kNoteOff, "backtick enters a note-off");

    //== Playback reflects the edit ==========================================
    {
        juce::AudioBuffer<float> audio (2, 64);
        juce::MidiBuffer midi;
        head->playing = true;
        head->ppq = 0.0;
        proc.processBlock (audio, midi);
        check (hasNoteOn (midi, 60), "playback emits the note entered in the tracker");

        midi.clear();
        head->playing = false;
        proc.processBlock (audio, midi);
    }

    //== Column visibility chips =============================================
    {
        const bool before = p.showInst;
        tracker->mouseDown (makeEvent (*tracker, { 55.0f, 24.0f }));
        check (p.showInst == ! before, "clicking the I chip hides the instrument column");

        tracker->mouseDown (makeEvent (*tracker, { 55.0f, 24.0f }));
        check (p.showInst == before, "clicking the I chip again restores the column");
    }

    //== Tools transpose =====================================================
    if (auto* down12 = findButton (*tools, "-12"))
    {
        down12->onClick();
        check (p.cell (0, 0).note == 48, "transpose -12 shifts the whole pattern");

        juce::AudioBuffer<float> audio (2, 64);
        juce::MidiBuffer midi;
        head->playing = true;
        head->ppq = 0.0;
        proc.processBlock (audio, midi);
        check (hasNoteOn (midi, 48), "playback picks up the transposed note");

        midi.clear();
        head->playing = false;
        proc.processBlock (audio, midi);
    }
    else
    {
        check (false, "tools panel exposes the -12 button");
    }

    if (auto* up12 = findButton (*tools, "+12"))
    {
        up12->onClick();
        check (p.cell (0, 0).note == 60, "transpose +12 restores the note");
    }
    else
    {
        check (false, "tools panel exposes the +12 button");
    }

    //== Timeline drop =======================================================
    {
        const int beforeClips = (int) song.clips.size();
        const int patternId = p.id;
        const juce::DragAndDropTarget::SourceDetails details (
            "rtr-pattern:" + juce::String (patternId), pool, { 816, 80 });

        check (timeline->isInterestedInDragSource (details), "timeline accepts pattern drags");

        timeline->itemDragEnter (details);
        timeline->itemDropped (details);

        check ((int) song.clips.size() == beforeClips + 1, "dropping a pattern adds a clip");

        const rtr::Clip* dropped = nullptr;

        for (auto& c : song.clips)
            if (c.startBeats > 16.0 && c.startBeats < 18.0)
                dropped = &c;

        check (dropped != nullptr && std::abs (dropped->startBeats - 17.0) < 1.0e-6,
               "dropped clip snaps to the grid at beat 17");
    }

    //== Pool double-click placement =========================================
    {
        const int beforeClips = (int) song.clips.size();
        song.editPlayheadBeats = 40.3;

        pool->mouseDoubleClick (makeEvent (*pool, { 80.0f, 45.0f }));

        check ((int) song.clips.size() == beforeClips + 1, "double-clicking the pool places a clip");

        const rtr::Clip* placed = nullptr;

        for (auto& c : song.clips)
            if (c.startBeats > 40.0)
                placed = &c;

        check (placed != nullptr && std::abs (placed->startBeats - 40.5) < 1.0e-6,
               "pool placement is snapped to the timeline grid");
    }

    //== Bottom-bar channels slider =========================================
    {
        juce::Slider* chSlider = nullptr;

        for (auto* child : editor->getChildren())
            if (auto* s = dynamic_cast<juce::Slider*> (child))
                chSlider = s; // last direct slider child = CHANNELS (added after OCT/STEP)

        if (chSlider == nullptr)
        {
            check (false, "editor exposes a bottom-bar slider");
        }
        else
        {
            auto* p = song.activePattern();
            check (p != nullptr, "an active pattern exists");

            chSlider->setValue (3.0, juce::sendNotificationSync);

            p = song.activePattern();
            check (p != nullptr && p->channels == 3,
                   "CHANNELS slider grows the active pattern to 3 channels");
            check (song.settings.maxTrackerChannels >= 3,
                   "raising past the global max bumps the settings max");

            chSlider->setValue (2.0, juce::sendNotificationSync);
            p = song.activePattern();
            check (p != nullptr && p->channels == 2, "the slider can shrink the pattern again");
            check (song.settings.maxTrackerChannels >= 3,
                   "shrinking a pattern leaves the settings max unchanged");
        }
    }

    //== Settings ============================================================
    if (auto* hex = findToggle (*settings, "Hexadecimal values"))
    {
        hex->setToggleState (true, juce::dontSendNotification);
        hex->onClick();
        check (song.settings.hexDisplay, "settings toggle writes the hex display flag");
    }
    else
    {
        check (false, "settings panel exposes the hex toggle");
    }

    song.settings.defaultPatternRows = 32;
    auto& created = song.addPattern();
    check (created.length == 32, "new patterns use the configured length");

    std::cout << (failures == 0 ? "PASS" : "FAIL")
              << " (" << failures << " failures)\n";
    return failures == 0 ? 0 : 1;
}
