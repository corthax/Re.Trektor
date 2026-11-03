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

// dropdowns in the order they were added to the component tree
void collectCombos (juce::Component& root, std::vector<juce::ComboBox*>& out)
{
    for (auto* child : root.getChildren())
    {
        if (auto* combo = dynamic_cast<juce::ComboBox*> (child))
            out.push_back (combo);

        collectCombos (*child, out);
    }
}

// sliders in the order they were added to the component tree
void collectSliders (juce::Component& root, std::vector<juce::Slider*>& out)
{
    for (auto* child : root.getChildren())
    {
        if (auto* slider = dynamic_cast<juce::Slider*> (child))
            out.push_back (slider);

        collectSliders (*child, out);
    }
}

// the instrument a note is played on (JUCE numbers channels from 1, the tracker
// names them from 0), or -1 when the note is not in the buffer
int noteChannel (const juce::MidiBuffer& midi, int note)
{
    for (const auto metadata : midi)
    {
        const auto m = metadata.getMessage();

        if (m.isNoteOn() && m.getNoteNumber() == note)
            return m.getChannel() - 1;
    }

    return -1;
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

// the velocity of a note-on, or -1 when the note is not in the buffer
int noteVelocity (const juce::MidiBuffer& midi, int note)
{
    for (const auto metadata : midi)
    {
        const auto m = metadata.getMessage();

        if (m.isNoteOn() && m.getNoteNumber() == note)
            return m.getVelocity();
    }

    return -1;
}

// the value a controller change carries, or -1 when that CC is not in the buffer
int controllerValue (const juce::MidiBuffer& midi, int number)
{
    for (const auto metadata : midi)
    {
        const auto m = metadata.getMessage();

        if (m.isController() && m.getControllerNumber() == number)
            return m.getControllerValue();
    }

    return -1;
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

    // run one 64-sample block with the transport parked just before a row
    auto playRowAt = [&] (int row)
    {
        juce::AudioBuffer<float> audio (2, 64);
        juce::MidiBuffer midi;
        head->playing = true;
        head->ppq = (double) row / (double) juce::jmax (1, song.settings.rowsPerBeat) - 0.001;
        proc.processBlock (audio, midi);
        head->playing = false;
        return midi;
    };

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

    //== Volume column ========================================================
    {
        const auto volRect = tracker->fieldBoundsFor (0, rtr::TrackerComponent::Field::vol);
        const auto panRect = tracker->fieldBoundsFor (0, rtr::TrackerComponent::Field::pan);

        check (volRect.getWidth() == panRect.getWidth() && volRect.getWidth() > 0,
               "the volume field is as wide as the pan field, so three decimal digits fit");

        // park the cursor on the volume field of channel 0
        for (int i = 0; i < 24
             && ! (tracker->getCursorField() == rtr::TrackerComponent::Field::vol
                   && tracker->getCursorChannel() == 0); ++i)
        {
            tracker->keyPressed (juce::KeyPress (juce::KeyPress::rightKey, juce::ModifierKeys(), 0));
        }

        check (tracker->getCursorField() == rtr::TrackerComponent::Field::vol && tracker->getCursorChannel() == 0,
               "the cursor can move onto the volume field");

        tracker->setCursorRow (5);

        tracker->keyPressed (juce::KeyPress ('1', juce::ModifierKeys(), '1'));
        tracker->keyPressed (juce::KeyPress ('0', juce::ModifierKeys(), '0'));
        tracker->keyPressed (juce::KeyPress ('0', juce::ModifierKeys(), '0'));

        check (p.cell (5, 0).vol == 100, "three decimal digits enter a volume above 99");

        const auto volMidi = playRowAt (5);
        check (controllerValue (volMidi, 7) == 100, "a volume without a note emits CC7");

        // with a note the volume also drives the note-on velocity
        p.cell (0, 0).vol = 42;
        proc.syncPlaybackModel();

        const auto noteMidi = playRowAt (0);
        check (controllerValue (noteMidi, 7) == 42, "a volume next to a note emits CC7 too");
        check (noteVelocity (noteMidi, 60) == 42, "and sets the note-on velocity");

        // the Settings tab names the controllers the columns drive
        std::vector<juce::Slider*> sliders;
        collectSliders (*settings, sliders);
        check (sliders.size() >= 8, "the settings tab exposes the volume and pan CC sliders");

        if (sliders.size() >= 8)
        {
            sliders[sliders.size() - 2]->setValue (5.0, juce::sendNotificationSync);
            sliders.back()->setValue (11.0, juce::sendNotificationSync);

            check (song.settings.volumeCc == 5 && song.settings.panCc == 11,
                   "moving the sliders changes the CC numbers");

            p.cell (5, 0).pan = 77;
            proc.syncPlaybackModel();

            const auto ccMidi = playRowAt (5);
            check (controllerValue (ccMidi, 5) == 100, "the volume column follows its configured CC");
            check (controllerValue (ccMidi, 11) == 77, "the pan column follows its configured CC");
            check (controllerValue (ccMidi, 7) == -1 && controllerValue (ccMidi, 10) == -1,
                   "no CC7/CC10 once the settings name other controllers");

            song.settings.volumeCc = 7;
            song.settings.panCc = 10;
            proc.syncPlaybackModel();
        }
    }

    //== Column visibility chips =============================================
    {
        const bool before = p.showColumn (0, 0);
        tracker->mouseDown (makeEvent (*tracker, { 55.0f, 24.0f }));
        check (p.showColumn (0, 0) == ! before, "clicking the I chip hides the instrument column of channel 0");

        tracker->mouseDown (makeEvent (*tracker, { 55.0f, 24.0f }));
        check (p.showColumn (0, 0) == before, "clicking the I chip again restores the column");
    }

    //== Column visibility is per channel =====================================
    {
        p.setChannels (3);
        tracker->refresh();

        const auto volChip = tracker->chipBoundsFor (1, 1);
        check (! volChip.isEmpty(), "channel 1 has its own volume chip");

        tracker->mouseDown (makeEvent (*tracker, { (float) volChip.getCentreX(), (float) volChip.getCentreY() }));
        check (! p.showColumn (1, 1), "clicking channel 1's V chip hides its volume");
        check (p.showColumn (0, 1) && p.showColumn (2, 1), "the volume stays visible on the other channels");

        const auto cmdChip = tracker->chipBoundsFor (2, 3);
        tracker->mouseDown (makeEvent (*tracker, { (float) cmdChip.getCentreX(), (float) cmdChip.getCentreY() }));
        check (! p.showColumn (2, 3), "clicking channel 2's C chip hides its command");
        check (p.showColumn (0, 3) && p.showColumn (1, 3), "the command stays visible on the other channels");

        // hidden columns are only hidden on screen: they still play
        p.cell (8, 1).vol = 90;
        p.cell (8, 1).pan = 60;
        proc.syncPlaybackModel();

        const auto visMidi = playRowAt (8);
        check (controllerValue (visMidi, 7) == 90 && controllerValue (visMidi, 10) == 60,
               "hidden columns still play their controllers");

        // and they come back when their chip is clicked again
        tracker->refresh();
        tracker->mouseDown (makeEvent (*tracker, { (float) tracker->chipBoundsFor (1, 1).getCentreX(),
                                                  (float) tracker->chipBoundsFor (1, 1).getCentreY() }));
        check (p.showColumn (1, 1), "clicking the chip again restores channel 1's volume column");

        tracker->refresh();
        tracker->mouseDown (makeEvent (*tracker, { (float) tracker->chipBoundsFor (2, 3).getCentreX(),
                                                  (float) tracker->chipBoundsFor (2, 3).getCentreY() }));
        check (p.showColumn (2, 3), "channel 2's command column comes back too");
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

    //== Tools instrument remap ==============================================
    {
        std::vector<juce::ComboBox*> combos;
        collectCombos (*tools, combos);

        check (combos.size() == 3, "the tools panel exposes the scope and the two remap dropdowns");

        if (combos.size() != 3)
        {
            check (false, "the remap dropdowns could not be found");
        }
        else
        {
            auto* scope = combos[0];
            auto* fromBox = combos[1];
            auto* toBox = combos[2];

            check (fromBox->getNumItems() == rtr::kMidiChannelCount + 1
                       && fromBox->getItemText (0) == "All"
                       && fromBox->getItemText (1) == "-0"
                       && fromBox->getItemText (rtr::kMidiChannelCount) == "-F",
                   "the source dropdown offers All plus every instrument");

            check (toBox->getNumItems() == rtr::kMidiChannelCount
                       && toBox->getItemText (0) == "-0"
                       && toBox->getItemText (rtr::kMidiChannelCount - 1) == "-F",
                   "the target dropdown offers only instruments, in the tracker's hex style");

            check (scope->getSelectedId() == 2 && fromBox->getSelectedId() == 1
                       && toBox->getSelectedId() == 1,
                   "the defaults are the whole pattern, All sources and -0 as target");

            p.setChannels (2);
            p.cell (3, 0).note = 65;
            p.cell (3, 0).inst = 3; // explicit source instrument
            p.cell (4, 0).note = 66; // no instrument: carries the one above
            p.cell (3, 1).note = 67;
            p.cell (3, 1).inst = 7; // another instrument, must stay put

            auto* remap = findButton (*tools, "Remap");
            check (remap != nullptr, "the tools panel exposes the Remap button");

            auto playRow = [&] (int row)
            {
                juce::AudioBuffer<float> audio (2, 64);
                juce::MidiBuffer midi;
                head->playing = true;
                head->ppq = (double) row / (double) juce::jmax (1, song.settings.rowsPerBeat) - 0.001;
                proc.processBlock (audio, midi);
                return midi;
            };

            if (remap == nullptr)
            {
                check (false, "the Remap button was missing");
            }
            else
            {
                proc.syncPlaybackModel(); // the hand-written cells reach the engine

                //== whole pattern: -3 -> -5 ==============================
                fromBox->setSelectedId (5); // "-3"
                toBox->setSelectedId (6); // "-5"
                remap->onClick();

                check (p.cell (3, 0).inst == 5, "the explicit source instrument becomes the target");
                check (p.cell (3, 1).inst == 7, "an instrument that is not the source keeps its value");
                check (p.cell (4, 0).inst == rtr::kEmpty, "the row that carried it stays implicit");

                check (noteChannel (playRow (3), 65) == 5, "the remapped note plays on MIDI channel 5");
                check (noteChannel (playRow (4), 66) == 5, "the implicit row follows the new instrument");
                check (noteChannel (playRow (3), 67) == 7, "the other channel still plays its own instrument");

                //== selection scope, source above the selection =========
                p.cell (5, 1).inst = 6; // instrument only, above the selection
                p.cell (6, 1).note = 68;
                p.cell (7, 1).note = 69;
                p.cell (9, 1).note = 71;
                p.cell (9, 1).inst = 6; // below the selection: must stay -6

                tracker->refresh();
                tracker->mouseDown (makeEvent (*tracker, { 57.0f, 169.0f })); // row 6
                tracker->mouseDrag (makeEvent (*tracker, { 57.0f, 188.0f })); // row 7
                tracker->mouseUp (makeEvent (*tracker, { 57.0f, 188.0f }));

                check (tracker->getSelectionStart() == 6 && tracker->getSelectionEnd() == 7,
                       "dragging in the tracker selects rows 6 and 7");

                scope->setSelectedId (1); // "Selection only"
                fromBox->setSelectedId (8); // "-6"
                toBox->setSelectedId (10); // "-9"
                remap->onClick();

                check (p.cell (5, 1).inst == 6, "the source above the selection stays put");
                check (p.cell (6, 1).inst == 9, "the carried instrument is pinned inside the selection");
                check (p.cell (7, 1).inst == rtr::kEmpty, "the next row is left implicit");
                check (p.cell (9, 1).inst == 6, "the instrument below the selection keeps its value");

                check (noteChannel (playRow (6), 68) == 9, "the pinned row plays on MIDI channel 9");
                check (noteChannel (playRow (7), 69) == 9, "the implicit row carries the new instrument");
                check (noteChannel (playRow (9), 71) == 6, "the untouched instrument still plays channel 6");

                //== whole pattern, All -> -B ============================
                tracker->clearSelection();
                scope->setSelectedId (2);
                fromBox->setSelectedId (1); // "All"
                toBox->setSelectedId (12); // "-B"
                remap->onClick();

                check (p.cell (3, 0).inst == 11 && p.cell (3, 1).inst == 11
                           && p.cell (6, 1).inst == 11 && p.cell (9, 1).inst == 11,
                       "All in the source box moves every explicit instrument to the target");
                check (p.cell (4, 0).inst == rtr::kEmpty && p.cell (7, 1).inst == rtr::kEmpty,
                       "rows that already resolve to the target are left implicit");
                check (p.cell (0, 0).inst == rtr::kEmpty && p.cell (0, 1).inst == rtr::kEmpty,
                       "All does not invent an instrument where none is carried");

                check (noteChannel (playRow (3), 65) == 11 && noteChannel (playRow (3), 67) == 11,
                       "both notes at row 3 moved to MIDI channel 11");
                check (noteChannel (playRow (4), 66) == 11, "the carried note follows to channel 11");
                check (noteChannel (playRow (6), 68) == 11 && noteChannel (playRow (7), 69) == 11,
                       "the channel that was pinned to -9 now plays on 11 as well");
                check (noteChannel (playRow (9), 71) == 11, "the instrument below the scope moved too");

                tracker->clearSelection();
            }
        }
    }

    //== Clicking the timeline parks the playhead ==
    {
        head->playing = false;

        auto* ed = dynamic_cast<ReTrektorAudioProcessorEditor*> (editor.get());
        check (ed != nullptr, "editor exposes the playhead follow");

        if (ed != nullptr)
        {
            auto tickHost = [&] (double ppq)
            {
                head->ppq = ppq;
                head->playing = false;

                juce::AudioBuffer<float> audio (2, 64);
                juce::MidiBuffer midi;
                proc.processBlock (audio, midi); // the host position reaches the plugin here

                ed->followHostTransport(); // one editor timer tick
            };

            tickHost (0.0); // the playhead mirrors the host at a stopped position
            check (song.transportMirrorBeats == 0.0, "a host tick records the position it mirrored");

            timeline->mouseDown (makeEvent (*timeline, { 300.0f, 100.0f }));
            timeline->mouseUp (makeEvent (*timeline, { 300.0f, 100.0f }));

            const double clicked = timeline->snapBeat (timeline->beatAtX (300.0f));
            check (song.editPlayheadBeats == clicked,
                   "a click on the timeline parks the playhead at the clicked, snapped beat");

            tickHost (0.0);
            check (song.editPlayheadBeats == clicked,
                   "a stopped host leaves the parked playhead alone");

            timeline->mouseDown (makeEvent (*timeline, { 120.0f, 100.0f }));
            timeline->mouseUp (makeEvent (*timeline, { 120.0f, 100.0f }));
            const double nearer = timeline->snapBeat (timeline->beatAtX (120.0f));
            check (std::abs (clicked - nearer) > 1.0e-6 && song.editPlayheadBeats == nearer,
                   "another click moves the playhead to the new spot");

            tickHost (7.25);
            check (song.editPlayheadBeats == 7.25,
                   "the playhead follows the host once its position moves");
            check (song.transportMirrorBeats == 7.25,
                   "the follow records the new host position");

            tickHost (0.0);
            check (song.editPlayheadBeats == 0.0,
                   "the playhead keeps following the host back");
        }
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

    //== Pool drag preview ===================================================
    {
        const auto flat = pool->dragCellBounds (0);
        const auto flatPreview = pool->dragPreviewImage (0);

        check (flat.getWidth() > 0 && flat.getHeight() > 0, "the first pool item has a cell");
        check (flatPreview.getImage().getWidth() == flat.getWidth() * 2
                   && flatPreview.getImage().getHeight() == flat.getHeight() * 2,
                   "the drag image is one pool item, captured twice as large to stay sharp");
        check (flat.getHeight() < pool->getHeight(),
               "the drag image is one pool row, not the whole pool panel");

        pool->toggleGridPreview (p.id);
        pool->refresh();

        const auto expanded = pool->dragCellBounds (0);
        const auto expandedPreview = pool->dragPreviewImage (0);

        check (expanded.getHeight() > flat.getHeight(),
               "an expanded item drags its taller grid preview");
        check (expandedPreview.getImage().getWidth() == expanded.getWidth() * 2
                   && expandedPreview.getImage().getHeight() == expanded.getHeight() * 2,
                   "the expanded preview covers that item's whole cell");

        pool->toggleGridPreview (p.id);
        pool->refresh();
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

            if (p != nullptr)
                p->cell (0, 2).note = 60; // a note in the widest column

            chSlider->setValue (2.0, juce::sendNotificationSync);
            p = song.activePattern();
            check (p != nullptr && p->channels == 2, "the slider can shrink the pattern again");
            check (p != nullptr && p->channelCapacity() == 3,
                   "shrinking keeps the column that holds data in the capacity");
            check (p != nullptr && p->rows[0].size() == 3 && p->rows[0][2].note == 60,
                   "the hidden column's note survives the shrink");

            chSlider->setValue (3.0, juce::sendNotificationSync);
            p = song.activePattern();
            check (p != nullptr && p->channels == 3,
                   "widening again restores the channel count");
            check (p != nullptr && p->cell (0, 2).note == 60,
                   "the hidden column's note is back after widening");
        }
    }

    //== Tracker horizontal scrollbar ==========================================
    {
        auto* p = song.activePattern();

        if (p != nullptr)
            p->setChannels (30); // far wider than the window: the strip appears

        tracker->refresh();

        const int tw = tracker->getWidth();
        const int th = tracker->getHeight();

        if (tw < 300 || th < 200)
        {
            check (false, "tracker is big enough to need a horizontal scrollbar");
        }
        else
        {
            const int barY = th - 7; // inside the bottom scrollbar strip
            const int rowY = th / 2; // a row in the middle of the view

            auto probeChannel = [&] (int x)
            {
                tracker->mouseDown (makeEvent (*tracker, { (float) x, (float) rowY }));
                tracker->mouseUp (makeEvent (*tracker, { (float) x, (float) rowY }));
                return tracker->getCursorChannel();
            };

            // a click at the strip's right end jumps the thumb there, so the
            // window shows the last columns
            tracker->mouseDown (makeEvent (*tracker, { (float) (tw - 4), (float) barY }));
            tracker->mouseUp (makeEvent (*tracker, { (float) (tw - 4), (float) barY }));

            const int far = probeChannel (tw - 12);
            const int leftAtMax = probeChannel (57); // just right of the 52px gutter
            check (far == 29, "at full scroll the window ends at the last channel");
            check (leftAtMax > 0, "at full scroll the first columns are scrolled out of view");

            // dragging the thumb back to the left shifts the window
            tracker->mouseDown (makeEvent (*tracker, { (float) (tw / 2), (float) barY }));
            tracker->mouseDrag (makeEvent (*tracker, { (float) (tw / 4), (float) barY }));
            tracker->mouseUp (makeEvent (*tracker, { (float) (tw / 4), (float) barY }));

            const int mid = probeChannel (tw - 12);
            check (mid >= 0 && mid < far, "dragging the thumb shifts the visible channel window");
            std::cout << "  scrollbar probe: far=" << far << " leftAtMax=" << leftAtMax
                      << " mid=" << mid << " (tracker " << tw << "x" << th << ")\n";

            // the row-number gutter stays fixed while the columns scroll
            const int beforeCh = tracker->getCursorChannel();
            tracker->mouseDown (makeEvent (*tracker, { 30.0f, (float) rowY }));
            check (tracker->getCursorChannel() == beforeCh,
                   "clicking the gutter does not pick a scrolled column");

            p->setChannels (1); // one channel always fits: no strip any more
            tracker->refresh();

            const int rowBefore = tracker->getCursorRow();
            const int chBefore = tracker->getCursorChannel();
            tracker->mouseDown (makeEvent (*tracker, { 70.0f, (float) barY }));
            check (tracker->getCursorRow() != rowBefore,
                   "when the columns fit, the bottom edge selects a row instead of scrolling");
            check (tracker->getCursorChannel() == chBefore && chBefore == 0,
                   "the narrow pattern has no strip to drag, so the channel is unchanged");
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

    //== Floating window size ==========================================
    {
        // Reaper closes a floating plug-in window by destroying the editor and
        // later asks the plug-in for a brand new one, which has to come back at
        // the size the closed one had
        editor->setSize (1000, 640);

        check (proc.getRememberedEditorBounds().getWidth() == 1000
                   && proc.getRememberedEditorBounds().getHeight() == 640,
               "the size the GUI was laid out at is remembered");

        proc.editorBeingDeleted (editor.get()); // the wrapper does this before deleting
        editor.reset();

        std::unique_ptr<juce::AudioProcessorEditor> reopened (proc.createEditor());
        check (reopened != nullptr, "the host gets a new editor after the old one was closed");

        if (reopened == nullptr)
            return 1;

        check (reopened->getWidth() == 1000 && reopened->getHeight() == 640,
               "a new editor comes back at the remembered size");

        // a size outside the resize limits must come back inside them
        reopened->setSize (4000, 3000);
        proc.editorBeingDeleted (reopened.get());
        reopened.reset();

        std::unique_ptr<juce::AudioProcessorEditor> clamped (proc.createEditor());
        check (clamped != nullptr, "a further editor is created");

        if (clamped == nullptr)
            return 1;

        check (clamped->getWidth() == 2600 && clamped->getHeight() == 1600,
               "a remembered size outside the resize limits comes back clamped");
    }

    std::cout << (failures == 0 ? "PASS" : "FAIL")
              << " (" << failures << " failures)\n";
    return failures == 0 ? 0 : 1;
}
