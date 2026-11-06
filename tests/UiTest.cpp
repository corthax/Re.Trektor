/*
    Headless UI interaction test for Re.Trektor.

    Builds the real editor without a host and drives the same component
    callbacks a mouse or keyboard would, then verifies that the model and the
    playback snapshot are updated. Covers:
      - note entry through the tracker keyboard handler
      - column visibility chip clicks
      - the transpose amount field and its direction buttons in the tools panel
      - selection interpolation in the tools panel
      - cell selection: a click selects one cell, a drag a rectangle of cells,
        Shift unions another rectangle into it and Ctrl takes one away
      - the cell clipboard: Ctrl+C copies the selection, Ctrl+V pastes it at the
        cursor with everything, the paste submenu filters one field, a clipboard
        hole pastes nothing, and what does not fit inside the pattern is dropped
      - undo and redo: Ctrl+Z and Ctrl+Y / Ctrl+Shift+Z walk the states the
        model changes append, the Settings tab's Undo steps row caps how far
        back they go, and the walk covers note entry, a cell paste, a clip
        placed from the pool, a settings change and the bottom bar
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

// the selection gestures read the modifiers a mouse event carries, so the tests
// can hold Shift (union) or Ctrl (subtract) while they sweep a rectangle
juce::MouseEvent makeEvent (juce::Component& c, juce::Point<float> pos, int mods)
{
    auto source = juce::Desktop::getInstance().getMainMouseSource();
    const auto now = juce::Time::getCurrentTime();

    return juce::MouseEvent (source, pos, juce::ModifierKeys (mods), 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                             &c, &c, now, pos, now, 1, false);
}

juce::MouseEvent makeEvent (juce::Component& c, juce::Point<float> pos)
{
    return makeEvent (c, pos, 0);
}

constexpr int kShiftMod = juce::ModifierKeys::shiftModifier;
constexpr int kCtrlMod = juce::ModifierKeys::ctrlModifier;
constexpr int kAltMod = juce::ModifierKeys::altModifier;

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

    //== A command argument without a command number ==========================
    {
        check (! song.settings.hexDisplay, "command values are entered as three decimal digits here");

        // park the cursor on the command value field of channel 0
        for (int i = 0; i < 32
             && ! (tracker->getCursorField() == rtr::TrackerComponent::Field::cmdVal
                   && tracker->getCursorChannel() == 0); ++i)
        {
            tracker->keyPressed (juce::KeyPress (juce::KeyPress::rightKey, juce::ModifierKeys(), 0));
        }

        check (tracker->getCursorField() == rtr::TrackerComponent::Field::cmdVal
                   && tracker->getCursorChannel() == 0,
               "the cursor sits on the command value field of channel 0");

        // nothing has named a command above, so the entry has to make one
        tracker->setCursorRow (3);
        tracker->keyPressed (juce::KeyPress ('0', juce::ModifierKeys(), '0'));
        tracker->keyPressed (juce::KeyPress ('4', juce::ModifierKeys(), '4'));
        tracker->keyPressed (juce::KeyPress ('5', juce::ModifierKeys(), '5'));

        check (p.cell (3, 0).cmdVal == 45, "the argument is stored on the cursor row");
        check (p.cell (3, 0).cmd == 0, "with nothing carried above, the command number becomes 0");

        const auto aloneMidi = playRowAt (3);
        check (controllerValue (aloneMidi, 0) == 45, "CC0 plays it, which is all the column had");

        // now the column names a command up top
        p.cell (0, 0).cmd = 17;
        p.cell (0, 0).cmdVal = 8;
        proc.syncPlaybackModel();

        const auto topMidi = playRowAt (0);
        check (controllerValue (topMidi, 17) == 8, "the row naming CC17 plays its own argument");

        tracker->setCursorRow (5);
        tracker->keyPressed (juce::KeyPress ('0', juce::ModifierKeys(), '0'));
        tracker->keyPressed (juce::KeyPress ('6', juce::ModifierKeys(), '6'));
        tracker->keyPressed (juce::KeyPress ('3', juce::ModifierKeys(), '3'));

        check (p.cell (5, 0).cmd == rtr::kEmpty, "under a carried command the cell stays implicit");
        check (p.cell (5, 0).cmdVal == 63, "the argument is stored");

        const auto belowMidi = playRowAt (5);
        check (controllerValue (belowMidi, 17) == 63, "the CC carried down the column plays the new argument");
        check (controllerValue (belowMidi, 0) == -1, "nothing is played on CC0");
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
    {
        std::vector<juce::ComboBox*> toolCombos;
        collectCombos (*tools, toolCombos);

        std::vector<juce::Slider*> amounts;
        collectSliders (*tools, amounts);

        check (amounts.size() == 1, "the tools panel exposes one transpose amount field");

        if (toolCombos.empty() || amounts.size() != 1)
        {
            check (false, "the tools panel is missing its scope dropdown or its transpose amount field");
        }
        else
        {
            auto* scope = toolCombos[0];
            auto* amount = amounts[0];

            check (scope->getSelectedId() == 1, "a fresh tools panel opens on the selection-only scope");
            check (juce::roundToInt (amount->getValue()) == 12,
                   "a fresh transpose amount is an octave in semitones");
            check (juce::roundToInt (amount->getRange().getStart()) == 1
                        && juce::roundToInt (amount->getRange().getEnd()) == 48
                        && juce::roundToInt (amount->getInterval()) == 1,
                   "the amount is a whole number of semitones between one and four octaves");

            tracker->clearSelection();

            if (auto* down = findButton (*tools, rtr::arrowDownGlyph()))
                down->onClick();

            check (p.cell (0, 0).note == 60, "with nothing selected the default scope transposes nothing");

            scope->setSelectedId (2); // "Whole pattern"
        }
    }
    // the direction button reads the field, so one amount moves both ways
    if (auto* down = findButton (*tools, rtr::arrowDownGlyph()))
    {
        down->onClick();
        check (p.cell (0, 0).note == 48, "the whole-pattern scope moves the note down by the 12 semitones the field holds");

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
        check (false, "the tools panel exposes no transpose direction button");
    }

    if (auto* up = findButton (*tools, rtr::arrowUpGlyph()))
    {
        up->onClick();
        check (p.cell (0, 0).note == 60, "the other button moves it back by the same amount");
    }
    else
    {
        check (false, "the tools panel exposes no transpose direction button");
    }

    //== Transpose amount, clamping and note-offs ============================
    {
        std::vector<juce::Slider*> amounts;
        collectSliders (*tools, amounts);

        auto* up = findButton (*tools, rtr::arrowUpGlyph());
        auto* down = findButton (*tools, rtr::arrowDownGlyph());

        if (amounts.size() != 1 || up == nullptr || down == nullptr)
        {
            check (false, "the transpose controls could not be found");
        }
        else
        {
            auto* amount = amounts[0];
            amount->setValue (5.0, juce::dontSendNotification);

            const int savedHigh = p.cell (1, 0).note;
            const int savedOff = p.cell (2, 0).note;
            p.cell (1, 0).note = 125; // near the top of the note range
            p.cell (2, 0).note = rtr::kNoteOff; // a row that names no note at all

            up->onClick();
            check (p.cell (0, 0).note == 65, "raising by the field's 5 semitones moves the note to 65");
            check (p.cell (1, 0).note == 127, "a note that would pass the top of the note range stops there");
            check (p.cell (2, 0).note == rtr::kNoteOff, "a note-off has no number to move and stays a note-off");

            down->onClick();
            check (p.cell (0, 0).note == 60, "the other direction moves every other row back");
            check (p.cell (2, 0).note == rtr::kNoteOff, "the note-off stays a note-off as the notes come down");

            // put the pattern back the way the later blocks expect it
            p.cell (1, 0).note = savedHigh;
            p.cell (2, 0).note = savedOff;
            amount->setValue (12.0, juce::dontSendNotification);
        }
    }

    //== Tools instrument remap ==============================================
    {
        std::vector<juce::ComboBox*> combos;
        collectCombos (*tools, combos);

        check (combos.size() == 4, "the tools panel exposes the scope, the remap pair and the interpolation dropdown");

        if (combos.size() != 4)
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

            check (fromBox->getSelectedId() == 1 && toBox->getSelectedId() == 1,
                   "the remap dropdowns default to All sources and -0 as target");

            scope->setSelectedId (2); // "Whole pattern", so every row is in reach

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

                //== selection scope, one channel column of the selection ====
                p.cell (5, 1).inst = 6; // instrument only, above the selection
                p.cell (6, 1).note = 68;
                p.cell (7, 1).note = 69;
                p.cell (9, 1).note = 71;
                p.cell (9, 1).inst = 6; // below the selection: must stay -6
                p.cell (6, 0).inst = 6; // same row, another channel column

                tracker->refresh();
                const float chOne = (float) tracker->channelBoundsFor (1).getCentreX();

                tracker->mouseDown (makeEvent (*tracker, { chOne, 169.0f })); // row 6
                tracker->mouseDrag (makeEvent (*tracker, { chOne, 188.0f })); // row 7
                tracker->mouseUp (makeEvent (*tracker, { chOne, 188.0f }));

                check (tracker->isCellSelected (6, 1) && tracker->isCellSelected (7, 1),
                       "dragging down one channel column selects those two cells");
                check (! tracker->isCellSelected (5, 1) && ! tracker->isCellSelected (8, 1),
                       "the sweep stops at the rows it passed through");
                check (! tracker->isCellSelected (6, 0),
                       "the cell beside the sweep is out of reach of a channel-level selection");

                scope->setSelectedId (1); // "Selection only"
                fromBox->setSelectedId (8); // "-6"
                toBox->setSelectedId (10); // "-9"
                remap->onClick();

                check (p.cell (5, 1).inst == 6, "the source above the selection stays put");
                check (p.cell (6, 1).inst == 9, "the carried instrument is pinned inside the selection");
                check (p.cell (7, 1).inst == rtr::kEmpty, "the next row is left implicit");
                check (p.cell (9, 1).inst == 6, "the instrument below the selection keeps its value");
                check (p.cell (6, 0).inst == 6, "the cell in the same row but another channel keeps its value");

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
                           && p.cell (6, 1).inst == 11 && p.cell (9, 1).inst == 11
                           && p.cell (6, 0).inst == 11,
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

    //== Tools selection interpolation =======================================
    {
        std::vector<juce::ComboBox*> combos;
        collectCombos (*tools, combos);

        if (combos.size() != 4)
        {
            check (false, "the tools panel does not expose the scope, the remap pair and one interpolation dropdown");
        }
        else
        {
            auto* scope = combos[0];
            auto* fieldBox = combos[3];

            check (fieldBox->getNumItems() == 6
                       && fieldBox->getItemText (0) == "Note"
                       && fieldBox->getItemText (1) == "Instrument"
                       && fieldBox->getItemText (2) == "Volume"
                       && fieldBox->getItemText (3) == "Pan"
                       && fieldBox->getItemText (4) == "CC number"
                       && fieldBox->getItemText (5) == "CC value",
                   "the field dropdown offers every column the tool can ramp");

            check (fieldBox->getSelectedId() == 1,
                   "the interpolation dropdown opens on the note column");

            // the volume column is free of edits here, so its blanks are clean
            p.setChannels (2);
            p.cell (8, 0).vol = 10; // above the range: ignored
            p.cell (17, 0).vol = 5; // below the range: ignored
            p.cell (10, 0).vol = 100;
            p.cell (12, 0).vol = 70;
            p.cell (15, 0).vol = 40;
            p.cell (10, 1).vol = 0;
            p.cell (15, 1).vol = 100;
            proc.syncPlaybackModel();

            check (p.cell (11, 0).vol == rtr::kEmpty && p.cell (13, 0).vol == rtr::kEmpty
                       && p.cell (11, 1).vol == rtr::kEmpty && p.cell (14, 1).vol == rtr::kEmpty,
                   "the rows between the anchors start blank");

            auto* interpolate = findButton (*tools, "Interpolate");
            check (interpolate != nullptr, "the tools panel exposes the Interpolate button");

            if (interpolate == nullptr)
            {
                check (false, "the Interpolate button was missing");
            }
            else
            {
                // dragging down one channel column selects those cells of that
                // channel only, and parks the cursor in it
                tracker->refresh();
                const float chOne = (float) tracker->channelBoundsFor (1).getCentreX();

                tracker->mouseDown (makeEvent (*tracker, { chOne, 245.0f })); // row 10
                tracker->mouseDrag (makeEvent (*tracker, { chOne, 340.0f })); // row 15
                tracker->mouseUp (makeEvent (*tracker, { chOne, 340.0f }));

                check (tracker->isCellSelected (10, 1) && tracker->isCellSelected (15, 1),
                       "dragging down channel 1 selects its rows 10 to 15");
                check (! tracker->isCellSelected (10, 0) && ! tracker->isCellSelected (15, 0),
                       "channel 0's cells in the same rows stay out of the selection");
                check (tracker->getCursorChannel() == 1,
                       "the cursor sits in the second tracker channel");

                fieldBox->setSelectedId (3); // "Volume"

                // the whole-pattern scope is still selected, and the ramp is a
                // selection tool: widening it would ramp every blank of the pattern
                scope->setSelectedId (2); // "Whole pattern"
                interpolate->onClick();
                check (p.cell (11, 0).vol == rtr::kEmpty && p.cell (11, 1).vol == rtr::kEmpty,
                       "the tool does nothing while the scope is not the selection");

                scope->setSelectedId (1); // "Selection only"
                interpolate->onClick();

                check (p.cell (11, 1).vol == 20 && p.cell (12, 1).vol == 40
                           && p.cell (13, 1).vol == 60 && p.cell (14, 1).vol == 80,
                       "the selected cells ramp between their own anchors");
                check (p.cell (10, 1).vol == 0 && p.cell (15, 1).vol == 100,
                       "the anchors of that channel stay put");
                check (p.cell (9, 1).vol == rtr::kEmpty && p.cell (16, 1).vol == rtr::kEmpty,
                       "the ramp never leaves the selected rows");
                check (p.cell (11, 0).vol == rtr::kEmpty && p.cell (13, 0).vol == rtr::kEmpty
                           && p.cell (14, 0).vol == rtr::kEmpty,
                       "the cells outside the selection are never written");
                check (p.cell (12, 0).vol == 70, "an anchor outside the selection keeps its value");
                check (p.cell (8, 0).vol == 10 && p.cell (17, 0).vol == 5,
                       "the anchors outside the selection keep their values too");

                //== Shift unions another column, then interpolate ==========
                // Shift adds another rectangle to what the first gesture drew
                // rather than replacing it, so the selection now covers both
                // columns of these rows
                tracker->refresh();
                const float chZero = (float) tracker->channelBoundsFor (0).getCentreX();

                tracker->mouseDown (makeEvent (*tracker, { chZero, 245.0f }, kShiftMod));
                tracker->mouseDrag (makeEvent (*tracker, { chZero, 340.0f }, kShiftMod));
                tracker->mouseUp (makeEvent (*tracker, { chZero, 340.0f }, kShiftMod));

                check (tracker->isCellSelected (10, 0) && tracker->isCellSelected (15, 0),
                       "Shift selects channel 0's rows as well");
                check (tracker->isCellSelected (12, 1),
                       "Shift keeps the rectangle the first gesture drew");

                interpolate->onClick();

                check (p.cell (11, 0).vol == 85 && p.cell (13, 0).vol == 60
                           && p.cell (14, 0).vol == 50,
                       "the added cells ramp channel 0 between its own anchors");
                check (p.cell (12, 0).vol == 70, "the middle anchor is not moved");
                check (p.cell (9, 0).vol == rtr::kEmpty,
                       "the anchor above the selection is not carried into it");
                check (p.cell (16, 0).vol == rtr::kEmpty && p.cell (17, 0).vol == 5,
                       "nothing is written below the selection either");
                check (p.cell (11, 1).vol == 20 && p.cell (14, 1).vol == 80,
                       "the cells that were already ramped are left alone");

                //== Ctrl subtracts a band, so the selection has a hole =====
                // Ctrl takes the rectangle it sweeps away from the selection,
                // which leaves a hole the ramp must not reach across
                tracker->refresh();
                tracker->mouseDown (makeEvent (*tracker, { chZero, 283.0f }, kCtrlMod)); // row 12
                tracker->mouseDrag (makeEvent (*tracker, { chZero, 302.0f }, kCtrlMod)); // row 13
                tracker->mouseUp (makeEvent (*tracker, { chZero, 302.0f }, kCtrlMod));

                check (! tracker->isCellSelected (12, 0) && ! tracker->isCellSelected (13, 0),
                       "Ctrl took those two cells away from the selection");
                check (tracker->isCellSelected (11, 0) && tracker->isCellSelected (14, 0)
                           && tracker->isCellSelected (12, 1),
                       "the cells above, below and beside the hole stay selected");

                interpolate->onClick();

                check (p.cell (12, 0).vol == 70 && p.cell (13, 0).vol == 60,
                       "the hole stops the ramp: neither the anchor nor the cell beside it moves");
                check (p.cell (11, 0).vol == 85 && p.cell (14, 0).vol == 50,
                       "the anchors on either side of the hole keep their values");

                // both channels now hold 60 at row 13, so the CC7 the row plays
                // proves the filled cells reached the playback snapshot
                proc.syncPlaybackModel();
                check (controllerValue (playRowAt (13), 7) == 60,
                       "the filled volumes play CC7 60 instead of the carried 10");

                tracker->clearSelection();
                scope->setSelectedId (1); // back to the scope a fresh panel opens on
                fieldBox->setSelectedId (1);
            }
        }
    }

    //== Tracker cell clipboard copy and paste ================================
    {
        auto* p = song.activePattern();
        check (p != nullptr, "an active pattern exists for the clipboard checks");

        if (p != nullptr)
        {
            // a fresh fixture: two columns of rows 4..6, written after clearing
            // so nothing an earlier block left in these rows reaches inside
            for (int r = 4; r <= 6; ++r)
                for (int c = 0; c < 2; ++c)
                    p->cell (r, c).clear();

            p->cell (4, 1).note = 60;
            p->cell (4, 1).vol = 11;
            p->cell (4, 1).pan = 44;
            p->cell (5, 1).note = 61;
            p->cell (6, 1).note = 62;
            p->cell (6, 1).pan = 55;
            p->cell (4, 0).note = 70;
            p->cell (4, 0).pan = 100;
            p->cell (5, 0).note = 71;
            p->cell (6, 0).note = 72;
            p->cell (6, 0).inst = 6;

            tracker->refresh();
            const float chZero = (float) tracker->channelBoundsFor (0).getCentreX();
            const float chOne = (float) tracker->channelBoundsFor (1).getCentreX();

            // drag one column and Ctrl takes a row out of it, leaving a hole
            tracker->mouseDown (makeEvent (*tracker, { chOne, 131.5f })); // row 4
            tracker->mouseDrag (makeEvent (*tracker, { chOne, 169.5f })); // row 6
            tracker->mouseUp (makeEvent (*tracker, { chOne, 169.5f }));
            tracker->mouseDown (makeEvent (*tracker, { chOne, 150.5f }, kCtrlMod)); // row 5
            tracker->mouseUp (makeEvent (*tracker, { chOne, 150.5f }, kCtrlMod));

            check (tracker->isCellSelected (4, 1) && tracker->isCellSelected (6, 1)
                       && ! tracker->isCellSelected (5, 1),
                   "the drag paints a column and the Ctrl click leaves a hole in it");

            // Ctrl+C copies the selection, hole included
            editor->keyPressed (juce::KeyPress ('C', juce::ModifierKeys (kCtrlMod), 'C'));
            check (tracker->hasCellClipboard(), "Ctrl+C copies the selected cells");
            check (tracker->isCellSelected (4, 1) && tracker->isCellSelected (6, 1),
                   "copying leaves the selection in place");

            // Ctrl+V lands the block at a clicked cursor: rows 18..20 of channel 0
            head->playing = false;
            for (int r = 18; r <= 20; ++r)
                p->cell (r, 0).clear();

            p->cell (18, 0).vol = 99;
            p->cell (18, 0).pan = 98;
            p->cell (19, 0).vol = 97;

            tracker->refresh();
            tracker->mouseDown (makeEvent (*tracker, { chZero, 392.5f })); // row 18
            tracker->mouseUp (makeEvent (*tracker, { chZero, 392.5f }));
            check (tracker->getCursorRow() == 18 && tracker->getCursorChannel() == 0,
                   "the click parks the cursor at the paste's corner cell");

            editor->keyPressed (juce::KeyPress ('V', juce::ModifierKeys (kCtrlMod), 'V'));

            check (p->cell (18, 0).note == 60 && p->cell (18, 0).vol == 11
                       && p->cell (18, 0).pan == 44,
                   "Ctrl+V writes the first copied cell over the cursor cell");
            check (p->cell (18, 0).inst == rtr::kEmpty && p->cell (18, 0).cmd == rtr::kEmpty
                       && p->cell (18, 0).cmdVal == rtr::kEmpty,
                   "pasting everything clears the fields the clipboard cell left empty");
            check (p->cell (19, 0).note == rtr::kEmpty && p->cell (19, 0).vol == 97
                       && p->cell (19, 0).pan == rtr::kEmpty,
                   "the hole inside the clipboard pastes nothing: the target keeps its own");
            check (p->cell (20, 0).note == 62 && p->cell (20, 0).pan == 55
                       && p->cell (20, 0).vol == rtr::kEmpty,
                   "the last copied cell lands at its offset below the anchor");

            // a filtered paste: only the note column moves
            for (int r = 21; r <= 23; r += 2)
                p->cell (r, 0).clear();

            p->cell (21, 0).note = 77;
            p->cell (21, 0).vol = 55;
            p->cell (23, 0).pan = 66;

            tracker->refresh();
            tracker->mouseDown (makeEvent (*tracker, { chZero, 449.5f })); // row 21
            tracker->mouseUp (makeEvent (*tracker, { chZero, 449.5f }));

            tracker->pasteClipboardAtCursor (rtr::kPasteNote);

            check (p->cell (21, 0).note == 60 && p->cell (21, 0).vol == 55,
                   "the Note filter overwrites notes only and leaves the volume its own");
            check (p->cell (23, 0).note == 62 && p->cell (23, 0).pan == 66,
                   "the offset cell lands and still keeps its other fields");

            // a full 2x3 block: a plain gesture draws channel 0 and Shift unions
            // the other column into it
            tracker->refresh();
            tracker->mouseDown (makeEvent (*tracker, { chZero, 131.5f }));
            tracker->mouseDrag (makeEvent (*tracker, { chZero, 169.5f }, kShiftMod));
            tracker->mouseUp (makeEvent (*tracker, { chZero, 169.5f }, kShiftMod));
            tracker->mouseDown (makeEvent (*tracker, { chOne, 131.5f }, kShiftMod));
            tracker->mouseDrag (makeEvent (*tracker, { chOne, 169.5f }, kShiftMod));
            tracker->mouseUp (makeEvent (*tracker, { chOne, 169.5f }, kShiftMod));

            editor->keyPressed (juce::KeyPress ('C', juce::ModifierKeys (kCtrlMod), 'C'));

            // and the paste at the last channel keeps only its first column
            for (int r = 22; r <= 24; ++r)
                p->cell (r, 1).clear();

            p->cell (22, 1).note = 77;
            p->cell (22, 1).vol = 22;

            tracker->refresh();
            tracker->mouseDown (makeEvent (*tracker, { chOne, 468.5f })); // row 22
            tracker->mouseUp (makeEvent (*tracker, { chOne, 468.5f }));

            editor->keyPressed (juce::KeyPress ('V', juce::ModifierKeys (kCtrlMod), 'V'));

            check (p->cell (22, 1).note == 70 && p->cell (22, 1).pan == 100,
                   "the block starts over the cursor cell with its first column");
            check (p->cell (22, 1).vol == rtr::kEmpty && p->cell (22, 1).inst == rtr::kEmpty,
                   "the whole copied cell is written, empty fields included");
            check (p->cell (23, 1).note == 71, "the row between follows, column and all");
            check (p->rows.size() == 64 && p->cell (24, 1).note == 72
                       && p->cell (24, 1).inst == 6,
                   "the block's last row keeps its offset below the anchor");

            // the block that does not fit is dropped: shorten the pattern and
            // paste at the edge again
            p->setLength (24);
            tracker->refresh();
            tracker->setCursorRow (23); // channel stays 1: the last channel

            editor->keyPressed (juce::KeyPress ('V', juce::ModifierKeys (kCtrlMod), 'V'));

            check (p->cell (23, 1).note == 70 && p->cell (23, 1).pan == 100,
                   "the part that still fits lands at the anchor below the shortening");
            check (p->rows.size() == 25 && p->rows[24][1].note == 72
                       && p->rows[24][1].inst == 6,
                   "rows past the new end keep what the earlier paste left there");

            // copying an empty selection leaves the clipboard of before alone
            tracker->clearSelection();
            editor->keyPressed (juce::KeyPress ('C', juce::ModifierKeys (kCtrlMod), 'C'));
            check (! tracker->hasSelection() && tracker->hasCellClipboard(),
                   "copying nothing selected leaves the previous clipboard in place");

            // the pattern clipboard keeps its operations under Shift
            const int clipsBefore = (int) song.clips.size();
            editor->keyPressed (juce::KeyPress ('C', juce::ModifierKeys (kShiftMod | kCtrlMod), 'C'));
            check (song.clipboard.valid, "Ctrl+Shift+C copies the active pattern to the pattern clipboard");

            head->playing = false;
            editor->keyPressed (juce::KeyPress ('V', juce::ModifierKeys (kShiftMod | kCtrlMod), 'V'));
            check ((int) song.clips.size() == clipsBefore,
                   "Ctrl+Shift+V is the pattern paste: it lands at the playhead and overlaps block it");

            p->setLength (64); // grown back: hidden rows and their data return
            tracker->refresh();
            tracker->clearSelection();
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

    //== Undo and redo =======================================================
    {
        // undo walks the states the model changes append, so every check
        // measures what an action touched just before it ran and reads it back
        // after the step: restoring a state replaces the pattern objects, which
        // is why nothing here may hold a reference across one
        auto pressUndo = [&]
        { return editor->keyPressed (juce::KeyPress ('Z', juce::ModifierKeys (kCtrlMod), 'Z')); };
        auto pressRedo = [&]
        { return editor->keyPressed (juce::KeyPress ('Y', juce::ModifierKeys (kCtrlMod), 'Y')); };
        auto pressRedoShift = [&]
        { return editor->keyPressed (juce::KeyPress ('Z', juce::ModifierKeys (kShiftMod | kCtrlMod), 'Z')); };

        // a click parks the cursor on a row: the header is 46px and a row 19
        auto clickRow = [&] (int row)
        {
            tracker->mouseDown (makeEvent (*tracker, { 70.0f, 46.0f + 19.0f * (float) row + 9.5f }));
        };

        // every field of a cell at once, so a paste and the undo that takes it
        // back can be compared on more than the note
        auto sigAt = [&] (int row, int ch)
        {
            auto* p = song.activePattern();

            if (p == nullptr)
                return juce::String();

            const auto cell = p->cell (row, ch);
            juce::String s;
            s << cell.note << "/" << cell.inst << "/" << cell.vol << "/" << cell.pan
              << "/" << cell.cmd << "/" << cell.cmdVal;
            return s;
        };

        //== note entry, undo, redo =======================================
        clickRow (4); // a row 19px tall, so the click lands on row 4
        const int undoRow = tracker->getCursorRow();
        const int undoCh = tracker->getCursorChannel();

        const juce::String beforeNoteSig = sigAt (undoRow, undoCh);

        tracker->keyPressed (juce::KeyPress ('z', juce::ModifierKeys(), 'z'));
        const int cursorAfterEntry = tracker->getCursorRow();
        const juce::String enteredNoteSig = sigAt (undoRow, undoCh);
        auto* entryPattern = song.activePattern();
        const int enteredNote = entryPattern != nullptr
                                  ? entryPattern->cell (undoRow, undoCh).note
                                  : rtr::kEmpty;
        check (enteredNoteSig != beforeNoteSig, "a note entered at the cursor changes that cell");

        check (pressUndo(), "the editor takes Ctrl+Z");
        check (sigAt (undoRow, undoCh) == beforeNoteSig, "Ctrl+Z takes the entered note back");
        check (tracker->getCursorRow() == cursorAfterEntry && tracker->getCursorChannel() == undoCh,
               "Ctrl+Z does not enter a note of its own, so the cursor stays where it was");

        check (pressRedo(), "the editor takes Ctrl+Y");
        check (sigAt (undoRow, undoCh) == enteredNoteSig, "Ctrl+Y puts the note back");
        check (proc.getUndoRedoCount() == 0, "the newest state of the line leaves nothing to redo");
        check (pressRedoShift(), "Ctrl+Shift+Z is the other spelling of redo");
        check (sigAt (undoRow, undoCh) == enteredNoteSig && proc.getUndoRedoCount() == 0,
               "a redo at the line's end changes nothing, either spelling");

        //== the whole line, back to the start and forward again ==============
        {
            int stepsBack = 0;

            while (proc.getUndoDepth() > 0 && stepsBack <= rtr::kMaxUndoSteps + 2)
            {
                pressUndo();
                ++stepsBack;
            }

            check (proc.getUndoDepth() == 0, "Ctrl+Z reaches the oldest point the line holds");

            const int clipsAtStart = (int) song.clips.size();
            const int patternsAtStart = (int) song.patterns.size();
            const int activeAtStart = song.activePatternId;
            pressUndo();
            check (proc.getUndoDepth() == 0
                       && (int) song.clips.size() == clipsAtStart
                       && (int) song.patterns.size() == patternsAtStart
                       && song.activePatternId == activeAtStart,
                   "Ctrl+Z at that end changes nothing further, patterns and clips alone");

            int forward = 0;

            while (proc.getUndoRedoCount() > 0 && forward <= rtr::kMaxUndoSteps + 2)
            {
                // both spellings on real steps, so the alias is not only
                // exercised where it is inert
                if (forward % 2 == 0)
                    pressRedoShift();
                else
                    pressRedo();

                ++forward;
            }

            check (forward == stepsBack, "Ctrl+Y and Ctrl+Shift+Z walk the same number of points");
            check (sigAt (undoRow, undoCh) == enteredNoteSig,
                   "and the model is back to the state it was in when the walk began");
        }

        //== a structural change: the pool places a clip ====================
        {
            song.editPlayheadBeats = 66.3;
            const int clipsBefore = (int) song.clips.size();

            pool->mouseDoubleClick (makeEvent (*pool, { 80.0f, 45.0f }));
            check ((int) song.clips.size() == clipsBefore + 1, "double-clicking the pool adds a clip");

            pressUndo();
            check ((int) song.clips.size() == clipsBefore, "Ctrl+Z takes the clip and the pattern it used back");
            check (proc.getUndoRedoCount() > 0, "the undone clip is still ahead of the cursor");

            pressRedo();
            check ((int) song.clips.size() == clipsBefore + 1, "Ctrl+Y places it again");
            pressUndo(); // the clip goes back to where it was not
        }

        //== a cell paste and its undo ======================================
        {
            clickRow (undoRow); // the cell the note check left with a note
            editor->keyPressed (juce::KeyPress ('C', juce::ModifierKeys (kCtrlMod), 'C'));
            check (tracker->hasCellClipboard(), "Ctrl+C copies the cell under the cursor");

            clickRow (undoRow + 10);
            const juce::String targetBefore = sigAt (undoRow + 10, undoCh);
            editor->keyPressed (juce::KeyPress ('V', juce::ModifierKeys (kCtrlMod), 'V'));
            const juce::String targetAfter = sigAt (undoRow + 10, undoCh);
            check (targetAfter == enteredNoteSig && targetAfter != targetBefore,
                   "the paste writes the copied cell where the cursor was parked");

            pressUndo();
            check (sigAt (undoRow + 10, undoCh) == targetBefore, "Ctrl+Z takes the pasted cell back");
            check (tracker->hasCellClipboard(), "and leaves the cell clipboard alone");

            pressRedo();
            check (sigAt (undoRow + 10, undoCh) == targetAfter, "Ctrl+Y writes it again");
            pressUndo();
        }

        //== an action that changed nothing makes no undo point ==============
        {
            std::vector<juce::ComboBox*> toolCombos;
            collectCombos (*tools, toolCombos);

            if (toolCombos.empty())
            {
                check (false, "the tools panel is out of reach for the no-op check");
            }
            else
            {
                toolCombos[0]->setSelectedId (1); // "Selection only"
                tracker->clearSelection();

                const int depthBefore = proc.getUndoDepth();

                if (auto* down = findButton (*tools, rtr::arrowDownGlyph()))
                    down->onClick();

                check (proc.getUndoDepth() == depthBefore,
                       "a transpose that reaches no cell leaves the undo depth alone");
            }
        }

        //== undo also reverts the Settings tab =============================
        if (auto* hex = findToggle (*settings, "Hexadecimal values"))
        {
            hex->setToggleState (false, juce::dontSendNotification);
            hex->onClick();
            check (! song.settings.hexDisplay, "switching the hex display off is an undoable action");

            pressUndo();
            check (song.settings.hexDisplay, "Ctrl+Z puts it back on, the Settings tab included");

            pressRedo();
            check (! song.settings.hexDisplay, "Ctrl+Y switches it off again");
        }
        else
        {
            check (false, "the settings tab's hex toggle is out of reach for the undo check");
        }

        //== the bottom bar changes the model on its own ====================
        {
            juce::Slider* chSlider = nullptr;

            for (auto* child : editor->getChildren())
                if (auto* s = dynamic_cast<juce::Slider*> (child))
                    chSlider = s; // the CHANNELS slider is the last direct one

            auto* beforeWide = song.activePattern();
            const int channelsBefore = beforeWide != nullptr ? beforeWide->channels : 0;

            if (chSlider != nullptr && channelsBefore > 0)
            {
                chSlider->setValue ((double) (channelsBefore + 1), juce::sendNotificationSync);
                auto* widened = song.activePattern();
                check (widened != nullptr && widened->channels == channelsBefore + 1,
                       "the CHANNELS slider widens the pattern");

                pressUndo();
                auto* backWide = song.activePattern();
                check (backWide != nullptr && backWide->channels == channelsBefore,
                       "Ctrl+Z narrows it again, so the bottom bar's change is on the line");
            }
            else
            {
                check (false, "the bottom bar's channel slider is out of reach");
            }
        }

        //== Ctrl+Z closes a half-typed value instead of stepping ============
        {
            // one field right of the note, which needs two digits to commit in
            // decimal, so a single digit stays pending
            for (int i = 0; i < 8
                     && tracker->getCursorField() != rtr::TrackerComponent::Field::inst; ++i)
                tracker->keyPressed (juce::KeyPress (juce::KeyPress::rightKey, juce::ModifierKeys(), 0));

            const int bufferRow = tracker->getCursorRow();
            const int bufferCh = tracker->getCursorChannel();
            const juce::String instBefore = sigAt (bufferRow, bufferCh);
            const int depthBefore = proc.getUndoDepth();

            tracker->keyPressed (juce::KeyPress ('5', juce::ModifierKeys(), '5'));
            check (proc.getUndoDepth() == depthBefore, "a digit still in its buffer changes no model");

            pressUndo();
            check (proc.getUndoDepth() == depthBefore && sigAt (bufferRow, bufferCh) == instBefore,
                   "the first Ctrl+Z closes that buffer rather than walking back");

            pressUndo();
            check (proc.getUndoDepth() == depthBefore - 1,
                   "the next one steps, which is what makes the pair one press one effect");
        }

        //== the Alt spellings of the same pair =========================
        {
            // hosts such as Bitwig and Reaper bind Ctrl+Z, Ctrl+Shift+Z and
            // Ctrl+Y to their own undo and never hand the plug-in the event,
            // so Alt+Z / Alt+Y walk the same line for them, and the letter is
            // read back from the key code or the printed character because a
            // key held by Alt arrives as the character that press produced
            auto pressUndoAlt = [&]
            { return editor->keyPressed (juce::KeyPress ('Z', juce::ModifierKeys (kAltMod), 'Z')); };
            auto pressRedoAlt = [&]
            { return editor->keyPressed (juce::KeyPress ('Y', juce::ModifierKeys (kAltMod), 'Y')); };
            auto pressAltShiftZ = [&]
            { return editor->keyPressed (juce::KeyPress ('Z', juce::ModifierKeys (kShiftMod | kAltMod), 'Z')); };

            // an octave no other row around here uses, so the note this entry
            // writes is one the checks below can name on its own
            song.settings.recordOctave = 8;

            clickRow (30);
            const int altRow = tracker->getCursorRow();
            const int altCh = tracker->getCursorChannel();
            const juce::String beforeAltSig = sigAt (altRow, altCh);

            check (! tracker->keyPressed (juce::KeyPress ('z', juce::ModifierKeys (kAltMod), 'z')),
                   "the tracker lets a letter held with Alt through to the editor");
            check (sigAt (altRow, altCh) == beforeAltSig,
                   "so the Alt spelling enters no note of its own");

            tracker->keyPressed (juce::KeyPress ('z', juce::ModifierKeys(), 'z'));
            const juce::String enteredAltSig = sigAt (altRow, altCh);
            check (enteredAltSig != beforeAltSig,
                   "the same letter without Alt still enters a note there");

            const int altDepth = proc.getUndoDepth();

            check (pressUndoAlt(), "the editor takes Alt+Z");
            check (proc.getUndoDepth() == altDepth - 1, "Alt+Z steps the same line back one point");
            check (sigAt (altRow, altCh) == beforeAltSig, "Alt+Z takes the entered note back");

            check (pressRedoAlt(), "the editor takes Alt+Y");
            check (proc.getUndoDepth() == altDepth && sigAt (altRow, altCh) == enteredAltSig,
                   "Alt+Y walks the same point forward again");

            check (pressUndoAlt(), "Alt+Z steps back a second time");
            check (pressRedoAlt(), "Alt+Y reaches that state again");
            check (proc.getUndoDepth() == altDepth && sigAt (altRow, altCh) == enteredAltSig,
                   "and both Alt spellings land on the same point of the line");

            pressRedoAlt();
            check (proc.getUndoDepth() == altDepth && sigAt (altRow, altCh) == enteredAltSig,
                   "a redo over the newest state is inert in the Alt spelling too");

            // the Alt+Shift spellings are left to the host rather than spent on
            // a redo alias: the hosts swallow Alt+Shift+Z before the plug-in is
            // offered it, so claiming it would give a key the user aims at the
            // redo the opposite meaning wherever it does arrive
            const int depthAltShift = proc.getUndoDepth();
            check (! pressAltShiftZ(), "the editor leaves an Alt+Shift combination to the host");
            check (proc.getUndoDepth() == depthAltShift && sigAt (altRow, altCh) == enteredAltSig,
                   "so neither the undo nor the redo steps on that press");

            // X11 hands a key held with Alt as the character that press
            // produced, whose case follows the keyboard's level rather than the
            // bare code, so the lowercase spellings are matched as well
            auto pressUndoAltLower = [&]
            { return editor->keyPressed (juce::KeyPress ('z', juce::ModifierKeys (kAltMod), 'z')); };
            auto pressRedoAltLower = [&]
            { return editor->keyPressed (juce::KeyPress ('y', juce::ModifierKeys (kAltMod), 'y')); };

            const int lowerDepth = proc.getUndoDepth();
            check (pressUndoAltLower(), "the editor takes the lowercase Alt+Z spelling the way X11 hands it");
            check (proc.getUndoDepth() == lowerDepth - 1, "and that spelling steps the same line back");
            check (pressRedoAltLower(), "the lowercase Alt+Y spelling walks it forward again");
            check (proc.getUndoDepth() == lowerDepth && sigAt (altRow, altCh) == enteredAltSig,
                   "and reaches the state the uppercase spelling does");

            // the half-typed buffer is closed by either spelling first, which
            // is what keeps one press one effect whichever pair was used
            //
            // the Alt walking above moved the hex display along with the rest
            // of the song, and how many digits a field wants follows that flag,
            // so the display is fixed here rather than assumed: a digit still
            // in its buffer behaves the same in either spelling
            song.settings.hexDisplay = false;

            for (int i = 0; i < 8
                     && tracker->getCursorField() != rtr::TrackerComponent::Field::inst; ++i)
                tracker->keyPressed (juce::KeyPress (juce::KeyPress::rightKey, juce::ModifierKeys(), 0));

            const int altBufferRow = tracker->getCursorRow();
            const int altBufferCh = tracker->getCursorChannel();
            const juce::String altInstBefore = sigAt (altBufferRow, altBufferCh);
            const int altBufferDepth = proc.getUndoDepth();

            tracker->keyPressed (juce::KeyPress ('5', juce::ModifierKeys(), '5'));
            check (proc.getUndoDepth() == altBufferDepth,
                   "the pending digit the Alt pair starts from changes no model");

            pressUndoAlt();
            check (proc.getUndoDepth() == altBufferDepth && sigAt (altBufferRow, altBufferCh) == altInstBefore,
                   "Alt+Z closes that buffer rather than walking back");

            pressUndoAlt();
            check (proc.getUndoDepth() == altBufferDepth - 1, "and the next Alt+Z steps");
        }

        //== the depth the Settings tab asks for ============================
        {
            std::vector<juce::Slider*> rows;
            collectSliders (*settings, rows);

            if (rows.size() >= 9)
            {
                // the undo row sits with the input prefs, ahead of the volume
                // and pan CC pair, so it is the third slider from the end
                auto* depthRow = rows[rows.size() - 3];

                depthRow->setValue (2.0, juce::sendNotificationSync);
                check (song.settings.undoSteps == 2, "the Undo steps row feeds the undo depth");

                for (int row = 44; row < 47; ++row)
                {
                    clickRow (row);
                    tracker->keyPressed (juce::KeyPress ('z', juce::ModifierKeys(), 'z'));
                }

                check (proc.getUndoDepth() == 2, "only the two points the row asks for are reachable");

                pressUndo();
                pressUndo();
                check (proc.getUndoDepth() == 0, "both of them can be walked back");

                const juce::String atOldest = sigAt (46, undoCh);
                pressUndo();
                check (proc.getUndoDepth() == 0 && sigAt (46, undoCh) == atOldest,
                       "a third Ctrl+Z has nothing left to take back");

                depthRow->setValue (0.0, juce::sendNotificationSync);
                check (song.settings.undoSteps == 0, "the row can turn undoing off");

                clickRow (47);
                tracker->keyPressed (juce::KeyPress ('z', juce::ModifierKeys(), 'z'));
                const juce::String offSig = sigAt (47, undoCh);
                check (proc.getUndoDepth() == 0, "an action while undoing is off records no state");

                pressUndo();
                check (sigAt (47, undoCh) == offSig, "and Ctrl+Z changes nothing while it is off");

                // back to the shipped default so the checks after this one have
                // undo available again, which also starts a fresh line
                depthRow->setValue (50.0, juce::sendNotificationSync);
                check (song.settings.undoSteps == 50, "the row comes back to the default 50");
            }
            else
            {
                check (false, "the settings tab has too few sliders to find the undo row");
            }
        }

        //== undo while the host is playing =================================
        {
            // the pair is meant to work mid-playback: an undo refreshes the
            // snapshot the audio thread reads without resetting the engine, so
            // the rows ahead of the playhead play what the restored model
            // holds. The octave is raised first so the note the key enters is
            // one no other row of the pattern plays, which lets these checks
            // look for that single note-on instead of counting events.
            song.settings.recordOctave = 9;

            // the row is whatever the click lands on rather than a counted one,
            // because the view scrolls and the click's y is read through that
            // scroll; the playhead is parked where the note was written
            clickRow (20);
            const int playRow = tracker->getCursorRow();
            const int playCh = tracker->getCursorChannel();

            check (tracker->getCursorField() == rtr::TrackerComponent::Field::note,
                   "a click inside a row parks the cursor on its note column");

            tracker->keyPressed (juce::KeyPress ('z', juce::ModifierKeys(), 'z'));

            auto* played = song.activePattern();
            const int playedNote = played != nullptr ? played->cell (playRow, playCh).note : rtr::kEmpty;
            check (playedNote != rtr::kEmpty,
                   "the note entered at the cursor is the cell the cursor parked on");
            check (proc.getUndoDepth() > 0, "the entered note left a point Ctrl+Z can walk back to");

            head->playing = true;
            head->ppq = (double) playRow / (double) juce::jmax (1, song.settings.rowsPerBeat);
            pressUndo();
            head->playing = false;

            const auto clearedMidi = playRowAt (playRow);
            check (! hasNoteOn (clearedMidi, playedNote),
                   "undoing while the transport runs leaves the row without the note");

            check (proc.getUndoRedoCount() > 0, "the undone note is still ahead of the cursor");

            head->playing = true;
            head->ppq = (double) playRow / (double) juce::jmax (1, song.settings.rowsPerBeat);
            pressRedo();
            head->playing = false;

            const auto playedMidi = playRowAt (playRow);
            check (hasNoteOn (playedMidi, playedNote),
                   "redoing it while the transport runs puts the row back to playing the note");
        }
    }

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
