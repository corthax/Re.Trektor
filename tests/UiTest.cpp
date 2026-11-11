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
        cursor with everything, the Alt+C / Alt+V / Alt+X steps the hosts leave
        free do the same at every panel, Alt+X also empties the cells it took so
        a cut and a following paste move them, the paste submenu filters one
        field, a clipboard hole pastes nothing, and what does not fit inside the
        pattern is dropped
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
    auto* macros = findChild<rtr::MacrosPanel> (*editor);

    check (pool != nullptr && timeline != nullptr && tracker != nullptr
               && tools != nullptr && settings != nullptr && macros != nullptr,
           "editor exposes pool, timeline, tracker, tools, settings and macros");

    if (pool == nullptr || timeline == nullptr || tracker == nullptr
        || tools == nullptr || settings == nullptr || macros == nullptr)
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
        // the row that used to offer a hex spelling is gone, so the digits the
        // command fields take are the three decimal ones whatever the view shows
        check (findToggle (*settings, "Hexadecimal values") == nullptr,
               "command values are entered as three decimal digits here, the Settings tab having no row that used to switch them");

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

    //== The arpeggiator letter and the Macros tab ===========================
    {
        // park the cursor on the command number field of channel 0
        for (int i = 0; i < 32
             && ! (tracker->getCursorField() == rtr::TrackerComponent::Field::cmdNum
                   && tracker->getCursorChannel() == 0); ++i)
        {
            tracker->keyPressed (juce::KeyPress (juce::KeyPress::rightKey, juce::ModifierKeys(), 0));
        }

        check (tracker->getCursorField() == rtr::TrackerComponent::Field::cmdNum
                   && tracker->getCursorChannel() == 0,
               "the cursor sits on the command number field of channel 0");

        p.cell (6, 0).note = 60; // the row names the note the run steps away from
        tracker->setCursorRow (6);
        check (p.cell (6, 0).cmd == rtr::kEmpty, "that row names no command of its own yet");

        tracker->keyPressed (juce::KeyPress ('A', juce::ModifierKeys(), 'A'));
        check (p.cell (6, 0).cmd == rtr::kArpCmd,
               "the letter A claims the command number and writes command 128");

        tracker->keyPressed (juce::KeyPress (juce::KeyPress::rightKey, juce::ModifierKeys(), 0));
        tracker->keyPressed (juce::KeyPress ('1', juce::ModifierKeys(), '1'));
        tracker->keyPressed (juce::KeyPress (juce::KeyPress::returnKey, juce::ModifierKeys(), 0));
        check (p.cell (6, 0).cmdVal == 1, "the argument typed next names arpeggiator id 1");

        // a row naming only the argument below that call stays implicit and names
        // another id, which nothing defined, so it sounds nothing at all
        tracker->setCursorRow (8);
        tracker->keyPressed (juce::KeyPress ('7', juce::ModifierKeys(), '7'));
        tracker->keyPressed (juce::KeyPress (juce::KeyPress::returnKey, juce::ModifierKeys(), 0));
        check (p.cell (8, 0).cmd == rtr::kEmpty,
               "under the carried call an argument-only row stays implicit");
        check (p.cell (8, 0).cmdVal == 7, "and names arpeggiator id 7, which nothing defined");

        // the release written as its letter and its 0 leaves the column calling
        // nothing rather than calling id 0: a fixture write rather than a typed
        // edit, so the undo line the sections below lean on stays the same length
        p.cell (12, 0).cmd = rtr::kArpCmd;
        p.cell (12, 0).cmdVal = 0; // A 0: the release the letter A names

        check (tracker->carriedCommandNumber (8, 0) == rtr::kArpCmd,
               "above the release the call is still carried down the column");
        check (tracker->carriedCommandNumber (13, 0) == rtr::kEmpty,
               "below it the column carries no command at all, which is what stops a letter and an argument being drawn over those rows");

        p.cell (12, 0).cmd = rtr::kEmpty;
        p.cell (12, 0).cmdVal = rtr::kEmpty; // the fixture's row is put back

        // the letter only claims the field as its first character, so the CC 10
        // a column asks for keeps its own 10 spelling, typed as three digits at
        // the keyboard the way the field is now written in decimal
        for (int i = 0; i < 8 && tracker->getCursorField() != rtr::TrackerComponent::Field::cmdNum; ++i)
            tracker->keyPressed (juce::KeyPress (juce::KeyPress::leftKey, juce::ModifierKeys(), 0));

        check (tracker->getCursorField() == rtr::TrackerComponent::Field::cmdNum,
               "one field back, the cursor is on the command number again");

        tracker->setCursorRow (10);
        tracker->keyPressed (juce::KeyPress ('1', juce::ModifierKeys(), '1'));
        tracker->keyPressed (juce::KeyPress ('0', juce::ModifierKeys(), '0'));
        tracker->keyPressed (juce::KeyPress (juce::KeyPress::returnKey, juce::ModifierKeys(), 0));
        check (p.cell (10, 0).cmd == 10, "typing 10 gives CC 10, the one spelling the number field has");

        tracker->keyPressed (juce::KeyPress (juce::KeyPress::deleteKey, juce::ModifierKeys(), 0));
        check (p.cell (10, 0).cmd == rtr::kEmpty, "and clearing that field leaves the row without a command");

        // a letter typed after digits still only claims the field, which drops
        // the number that was being typed rather than reading it as a command
        tracker->setCursorRow (11);
        tracker->keyPressed (juce::KeyPress ('1', juce::ModifierKeys(), '1'));
        tracker->keyPressed (juce::KeyPress ('0', juce::ModifierKeys(), '0'));
        tracker->keyPressed (juce::KeyPress ('A', juce::ModifierKeys(), 'A'));
        check (p.cell (11, 0).cmd == rtr::kArpCmd,
               "the A typed last claims the field on its own and writes the call, the 10 it interrupts having been dropped");
        check (p.cell (11, 0).cmdVal == 0, "and the entry leaves the id 0 it has to write beside it");

        tracker->keyPressed (juce::KeyPress (juce::KeyPress::deleteKey, juce::ModifierKeys(), 0));
        check (p.cell (11, 0).cmd == rtr::kEmpty, "and clearing that row's field leaves nothing behind for the rows below");

        // the run the row names plays over the row and the ones below it, a
        // sixth of a row per step in 64-sample blocks the way a host hands them
        {
            // a fresh macro arrives holding steps that sound nothing, so this
            // test says the shape it wants the run to step through
            for (int i = 0; i < rtr::kArpDefaultSteps; ++i)
                song.setArpStep (1, i, (std::int8_t) (i * 4));

            proc.syncPlaybackModel (); // the hand-written steps reach the engine

            std::vector<int> ons;

            for (int i = 0; i < 140; ++i)
            {
                juce::AudioBuffer<float> audio (2, 64);
                juce::MidiBuffer midi;
                head->playing = true;
                head->ppq = 1.5 + (double) (i * 64) / 48000.0 * 2.0;
                proc.processBlock (audio, midi);

                for (const auto m : midi)
                    if (m.getMessage().isNoteOn())
                        ons.push_back (m.getMessage().getNoteNumber());
            }

            static const int ramp[] = { 60, 64, 68, 72, 76, 80, 84, 88 };
            check (ons == std::vector<int> (ramp, ramp + 8),
                   "the row's own note is the run's first step and its eight steps play in order a sixth of a row apart, spilling past the row that called them");

            head->playing = false;
        }

        // the Macros tab holds one length field and one looped tick per macro
        // that exists, and the default song has one
        std::vector<juce::Slider*> lengths;
        collectSliders (*macros, lengths);
        check (lengths.size() == 1, "the tab exposes one length field for the one macro the song defines");

        if (! lengths.empty())
        {
            check (lengths[0]->getRange().getStart() == 1.0
                       && lengths[0]->getRange().getEnd() == (double) rtr::kMaxArpSteps,
                   "that field steps a single step and reaches the step limit");
            check (lengths[0]->getValue() == (double) rtr::kArpDefaultSteps,
                   "and it opens at the default eight steps");
        }

        check (findToggle (*macros, "LOOPED") != nullptr, "the tab exposes a looped tick beside it");
        check (findToggle (*macros, "LOOPED") != nullptr
                   && ! song.arpLooped (1), "the default macro is not looped");

        if (auto* add = findButton (*macros, "+ Add macro"))
            add->onClick();

        check (song.arpDefined (2) && song.firstUndefArpId() == 3,
               "a defined macro brings the next free id up and takes that one further");
        check (song.peekArp (2) != nullptr && song.peekArp (2)->length() == rtr::kArpDefaultSteps
                   && song.peekArp (2)->steps[0] == rtr::kArpEmptyStep && ! song.arpLooped (2),
               "and it comes in with steps that sound nothing, not looped");

        if (auto* remove = findButton (*macros, "- Remove selected"))
            remove->onClick();

        check (! song.arpDefined (2) && song.lastDefArpId() == 1,
               "clearing the macro the step cursor was left on takes it back and leaves the ones below naming the same macros");

        // the drawn steps are entered at the cursor the arrows and the buttons
        // leave on, so a typed sign and digits land on one step of that macro
        const int depthBefore = proc.getUndoDepth();

        // a number is built up in the cell before any of it is written, so a
        // digit typed and then dropped with Escape writes nothing and keeps the
        // undo line alone: the press that closes the number is what lands, and
        // it lands once
        macros->keyPressed (juce::KeyPress ('4', juce::ModifierKeys(), '4'));
        check (song.peekArp (1) != nullptr && song.peekArp (1)->steps[0] == 0,
               "a digit typed into the step the cursor sits on writes nothing until the number is closed");
        check (proc.getUndoDepth() == depthBefore,
               "and the number still being typed has no point of its own on the undo line");

        macros->keyPressed (juce::KeyPress (juce::KeyPress::escapeKey, juce::ModifierKeys(), 0));
        check (song.peekArp (1) != nullptr && song.peekArp (1)->steps[0] == 0,
               "and Escape drops the number being typed rather than writing half of it");

        // a step's number is typed one character at a time, so a sign and more
        // than one digit reach one step rather than only its last figure
        macros->keyPressed (juce::KeyPress ('-', juce::ModifierKeys(), '-'));
        macros->keyPressed (juce::KeyPress ('2', juce::ModifierKeys(), '2'));
        macros->keyPressed (juce::KeyPress ('5', juce::ModifierKeys(), '5'));
        check (song.peekArp (1) != nullptr && song.peekArp (1)->steps[0] == 0,
               "a sign and three digits still being typed keep the step's own value");
        check (proc.getUndoDepth() == depthBefore,
               "and none of those presses has a point of its own on the line");

        macros->keyPressed (juce::KeyPress (juce::KeyPress::returnKey, juce::ModifierKeys(), 0));
        check (song.peekArp (1) != nullptr && song.peekArp (1)->steps[0] == -25,
               "the press that closes them lands the whole number on the step the cursor sits on");
        check (proc.getUndoDepth() == depthBefore + 1, "and typing that number is one undoable action");

        // the length field rewrites the macro the way the row does: the steps
        // already entered stay, growing pads with the base note itself
        if (! lengths.empty())
        {
            lengths[0]->setValue (3.0, juce::sendNotificationSync);
            check (song.peekArp (1) != nullptr && song.peekArp (1)->length() == 3
                       && song.peekArp (1)->steps[0] == -25,
                   "shortening keeps the steps still inside it rather than shifting them");

            lengths[0]->setValue (8.0, juce::sendNotificationSync);
            check (song.peekArp (1) != nullptr && song.peekArp (1)->length() == 8
                       && song.peekArp (1)->steps[7] == rtr::kArpEmptyStep,
                   "and growing back adds steps that sound nothing, so the steps already entered keep their places");
        }

        check (findToggle (*macros, "LOOPED") != nullptr
                   && findToggle (*macros, "LOOPED")->onClick != nullptr,
               "the looped tick is a click the panel answers");

        if (auto* looped = findToggle (*macros, "LOOPED"))
        {
            // a click is what flips the tick and what writes the flag, in that
            // order, so the pair stands in for the one gesture a user makes
            looped->setToggleState (true, juce::sendNotificationSync);
            looped->onClick();
            check (song.arpLooped (1), "clicking the tick writes the looped flag into the macro");

            looped->setToggleState (false, juce::sendNotificationSync);
            looped->onClick();
            check (! song.arpLooped (1), "clicking it again clears it");
        }

        // every change the panel makes is an action of its own, so the undo line
        // holds a point for each of them and Ctrl+Z can take each one back: a drag
        // of a length field, a click of the looped tick, and the steps the macro
        // held when the earlier point was written rather than the ones it has now
        if (! lengths.empty())
        {
            const int depthBeforeLength = proc.getUndoDepth();

            lengths[0]->setValue (4.0, juce::sendNotificationSync);
            check (proc.getUndoDepth() == depthBeforeLength + 1,
                   "a drag of the length field is one undoable action");

            check (proc.undo() && song.peekArp (1) != nullptr && song.peekArp (1)->length() == 8,
                   "and Ctrl+Z takes the length back to what the earlier state held");
            check (song.peekArp (1)->steps[7] == rtr::kArpEmptyStep,
                   "with the steps that state held rather than the ones the macro had kept");

            check (proc.redo() && song.peekArp (1) != nullptr && song.peekArp (1)->length() == 4,
                   "and Ctrl+Y puts the new length back");
        }

        if (auto* looped = findToggle (*macros, "LOOPED"))
        {
            const int depthBeforeTick = proc.getUndoDepth();

            looped->setToggleState (true, juce::sendNotificationSync);
            looped->onClick();
            check (proc.getUndoDepth() == depthBeforeTick + 1,
                   "a click of the looped tick is one undoable action");

            check (proc.undo() && ! song.arpLooped (1),
                   "and Ctrl+Z takes the tick back, the flag the macro holds with it");
            check (proc.getUndoDepth() == depthBeforeTick, "and the line's depth goes back with it");

            looped->setToggleState (false, juce::sendNotificationSync);
            looped->onClick();
            check (! song.arpLooped (1), "clicking the tick again leaves the macro unlooped");
        }
    }

    //== The pitch bend letter and its argument ==============================
    {
        auto bendValue = [] (const juce::MidiBuffer& midi)
        {
            int found = -1;

            for (const auto metadata : midi)
                if (metadata.getMessage().isPitchWheel())
                    found = metadata.getMessage().getPitchWheelValue();

            return found;
        };

        // restoring a state replaces the pattern objects, so these read the
        // cell back through the song every time rather than through a held
        // reference
        auto cmdAt = [&] (int row)
        {
            auto* pp = song.activePattern();
            return pp != nullptr ? pp->cell (row, 0).cmd : rtr::kEmpty;
        };
        auto argAt = [&] (int row)
        {
            auto* pp = song.activePattern();
            return pp != nullptr ? pp->cell (row, 0).cmdVal : rtr::kEmpty;
        };

        for (int i = 0; i < 32
             && ! (tracker->getCursorField() == rtr::TrackerComponent::Field::cmdNum
                   && tracker->getCursorChannel() == 0); ++i)
        {
            tracker->keyPressed (juce::KeyPress (juce::KeyPress::rightKey, juce::ModifierKeys(), 0));
        }

        check (tracker->getCursorField() == rtr::TrackerComponent::Field::cmdNum
                   && tracker->getCursorChannel() == 0,
               "the cursor sits on the command number field of channel 0 again");

        tracker->setCursorRow (20);
        check (cmdAt (20) == rtr::kEmpty, "that row names no command of its own yet");

        const int depthBeforeLetter = proc.getUndoDepth();
        tracker->keyPressed (juce::KeyPress ('B', juce::ModifierKeys(), 'B'));
        check (cmdAt (20) == rtr::kBendCmd,
               "the letter B claims the command number and writes command 144");
        check (argAt (20) == rtr::kBendCentreVal,
               "and leaves the argument at the wheel's centre, so a row naming only the letter bends nowhere");
        check (proc.getUndoDepth() == depthBeforeLetter + 1, "and claiming the letter is one undoable action");

        check (bendValue (playRowAt (20)) == rtr::kBendCentreVal * rtr::kBendStep,
               "that row moves the wheel to its centre, which is where the argument's centre is read");

        // typing the argument the letter left behind names a new point, and the
        // argument field takes the digits on its own rather than going through
        // the number field, which is what keeps the letter the letter
        tracker->keyPressed (juce::KeyPress (juce::KeyPress::rightKey, juce::ModifierKeys(), 0));
        tracker->keyPressed (juce::KeyPress ('9', juce::ModifierKeys(), '9'));
        tracker->keyPressed (juce::KeyPress ('6', juce::ModifierKeys(), '6'));
        tracker->keyPressed (juce::KeyPress (juce::KeyPress::returnKey, juce::ModifierKeys(), 0));
        check (argAt (20) == 96, "the argument typed next names a point on that wheel");
        check (bendValue (playRowAt (20)) == 96 * rtr::kBendStep,
               "and the point plays where the argument is read in steps of 128");

        // a row below that only names an argument stays implicit and moves the
        // same wheel, which is what re-arms the bend a release point had taken
        tracker->setCursorRow (21);
        tracker->keyPressed (juce::KeyPress ('8', juce::ModifierKeys(), '8'));
        tracker->keyPressed (juce::KeyPress (juce::KeyPress::returnKey, juce::ModifierKeys(), 0));
        check (cmdAt (21) == rtr::kEmpty,
               "under the carried letter an argument-only row stays implicit");
        check (argAt (21) == 8, "and names its own point on the wheel");
        check (bendValue (playRowAt (21)) == 8 * rtr::kBendStep,
               "which plays on the wheel the letter above is still carrying");

        // as with the other letter, the claim only owns the field's first
        // character, so the CC 11 a column asks for keeps its own 11 spelling
        for (int i = 0; i < 8 && tracker->getCursorField() != rtr::TrackerComponent::Field::cmdNum; ++i)
            tracker->keyPressed (juce::KeyPress (juce::KeyPress::leftKey, juce::ModifierKeys(), 0));

        check (tracker->getCursorField() == rtr::TrackerComponent::Field::cmdNum,
               "one field back, the cursor is on the command number again");

        tracker->setCursorRow (22);
        tracker->keyPressed (juce::KeyPress ('1', juce::ModifierKeys(), '1'));
        tracker->keyPressed (juce::KeyPress ('1', juce::ModifierKeys(), '1'));
        tracker->keyPressed (juce::KeyPress (juce::KeyPress::returnKey, juce::ModifierKeys(), 0));
        check (cmdAt (22) == 11, "typing 11 gives CC 11 rather than the letter bend, the number field having one spelling");

        tracker->keyPressed (juce::KeyPress (juce::KeyPress::deleteKey, juce::ModifierKeys(), 0));
        check (cmdAt (22) == rtr::kEmpty, "and clearing that field leaves the row without a command");

        // the letter typed after a run of digits still only claims the field
        tracker->keyPressed (juce::KeyPress ('1', juce::ModifierKeys(), '1'));
        tracker->keyPressed (juce::KeyPress ('1', juce::ModifierKeys(), '1'));
        tracker->keyPressed (juce::KeyPress ('B', juce::ModifierKeys(), 'B'));
        check (cmdAt (22) == rtr::kBendCmd,
               "the B typed last claims the field and writes the bend, the 11 it interrupts having been dropped");
        check (argAt (22) == rtr::kBendCentreVal, "and the entry leaves the wheel's centre as its argument");

        tracker->keyPressed (juce::KeyPress (juce::KeyPress::deleteKey, juce::ModifierKeys(), 0));
        check (cmdAt (22) == rtr::kEmpty, "and clearing that row's field leaves nothing behind for the rows below");

        // a digit run cannot reach the letter's value: the number field tops at
        // the highest CC, so the letters are the only way to name a command kind
        tracker->setCursorRow (23);
        tracker->keyPressed (juce::KeyPress ('1', juce::ModifierKeys(), '1'));
        tracker->keyPressed (juce::KeyPress ('4', juce::ModifierKeys(), '4'));
        tracker->keyPressed (juce::KeyPress ('4', juce::ModifierKeys(), '4'));
        tracker->keyPressed (juce::KeyPress (juce::KeyPress::returnKey, juce::ModifierKeys(), 0));
        check (cmdAt (23) == rtr::kCcTop, "typing 144 into the number field gives the highest CC rather than the bend letter");

        // the rows this section parked the cursor on are below the window the
        // checks after it click into, so the view is brought back to the top
        tracker->keyPressed (juce::KeyPress (juce::KeyPress::homeKey, juce::ModifierKeys(), 0));
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
                       && fromBox->getItemText (rtr::kMidiChannelCount) == "-15",
                   "the source dropdown offers All plus every instrument");

            check (toBox->getNumItems() == rtr::kMidiChannelCount
                       && toBox->getItemText (0) == "-0"
                       && toBox->getItemText (rtr::kMidiChannelCount - 1) == "-15",
                   "the target dropdown offers only instruments, in the tracker's own dash-and-number style");

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

            // the paste submenu's "Additive (everything)" leaf, reached through
            // the same paste call the callback makes: the buffer's empty fields
            // leave the target's own values in place rather than clearing them,
            // which is what the plain Everything above just did to them
            for (int r = 25; r <= 27; ++r)
            {
                p->cell (r, 0).clear();
                p->cell (r, 0).cmd = 12;
                p->cell (r, 0).inst = 7;
            }

            tracker->refresh();
            tracker->mouseDown (makeEvent (*tracker, { chZero, 525.5f })); // row 25
            tracker->mouseUp (makeEvent (*tracker, { chZero, 525.5f }));
            check (tracker->getCursorRow() == 25 && tracker->getCursorChannel() == 0,
                   "the click parks the cursor over the additive paste's corner cell");

            const int depthAdditive = proc.getUndoDepth();

            tracker->pasteClipboardAtCursor (rtr::kPasteAll | rtr::kPasteAdditive);

            check (p->cell (25, 0).note == 60 && p->cell (25, 0).vol == 11
                           && p->cell (25, 0).pan == 44,
                   "the additive paste lands the fields the buffer carries on top of what the target had");
            check (p->cell (25, 0).inst == 7 && p->cell (25, 0).cmd == 12
                           && p->cell (25, 0).cmdVal == rtr::kEmpty,
                   "the fields the buffer left empty keep the target's own value, where the plain Everything cleared them");
            check (p->cell (27, 0).note == 62 && p->cell (27, 0).pan == 55,
                   "the offset cell lands below the anchor and its pan goes on top");
            check (p->cell (27, 0).inst == 7 && p->cell (27, 0).cmd == 12,
                   "and the target's command and instrument survive under an empty buffer field");
            check (p->cell (26, 0).inst == 7 && p->cell (26, 0).cmd == 12,
                   "the hole of the clipboard is skipped as before, so that row never gets pasted");
            check (proc.getUndoDepth() == depthAdditive + 1,
                   "a paste that did write something still records its one point of the undo line");

            // an all-empty clipboard cell additively writes nothing at all, so
            // nothing about the song changes and the undo line stays alone, the
            // way the plain spelling above clears the cell it lands on
            rtr::Cell nothing;
            tracker->cellClipboard.entries = { { 0, 0, nothing } };
            tracker->setCursorRow (21);
            check (p->cell (21, 0).note == 60 && p->cell (21, 0).vol == 55,
                   "the cell the additive paste is about to skip still carries what the earlier paste left");

            const int depthSkipped = proc.getUndoDepth();
            tracker->pasteClipboardAtCursor (rtr::kPasteAll | rtr::kPasteAdditive);
            check (proc.getUndoDepth() == depthSkipped && p->cell (21, 0).note == 60
                           && p->cell (21, 0).vol == 55,
                   "an all-empty clipboard cell additively writes nothing, which leaves no change and no point of the line");

            // one of the additive submenu's single-field leaves, the same call the
            // callback makes with the field bit and the mode bit together: the mode
            // narrows the leaf to the fields the buffer really carries, so a buffer
            // cell whose one named field is empty is skipped whole and the target
            // keeps its own value there, where the plain leaf would have cleared it.
            // this paste stays over the cursor the block above left, so the view
            // below still maps its clicks to the rows its fixture expects
            rtr::Cell bufferCell;
            bufferCell.inst = 9; // its volume stays empty, which is what gets skipped
            rtr::Cell withVolume;
            withVolume.vol = 33;
            withVolume.note = 71; // the note of the same cell never reaches a volume leaf
            tracker->cellClipboard.entries = { { 0, 0, bufferCell }, { 2, 0, withVolume } };

            const int depthVolume = proc.getUndoDepth();

            tracker->pasteClipboardAtCursor (rtr::kPasteVol | rtr::kPasteAdditive);

            check (p->cell (21, 0).vol == 55 && p->cell (21, 0).inst == rtr::kEmpty,
                   "the volume leaf skips the buffer cell whose own volume is empty, so the target's volume survives and nothing of that cell is written");
            check (p->cell (23, 0).vol == 33 && p->cell (23, 0).note == 62 && p->cell (23, 0).pan == 66,
                   "the buffer cell that does carry a volume lands at its offset below the anchor and writes nothing of its other fields");
            check (proc.getUndoDepth() == depthVolume + 1,
                   "a one-field additive paste that lands something still records its one point of the line");

            // the fixture's rows go back to empty so the blocks below keep seeing
            // the pattern they had: hidden rows past a shortened end are kept
            // only while they hold something, and these just got given a command
            // and an instrument of their own
            for (int r = 25; r <= 27; ++r)
                p->cell (r, 0).clear();

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

            // the pattern clipboard keeps no keyboard spelling of its own: the
            // Ctrl+Shift pair was found swallowed on the way to both hosts the way
            // the plain Ctrl pair is, so it was dropped completely rather than
            // moved to another letter, and copying or pasting a whole pattern is
            // only what the timeline's clip menu, its empty-area menu and the
            // pool's menu do
            const int clipsBefore = (int) song.clips.size();
            const bool clipboardBefore = song.clipboard.valid;

            check (! editor->keyPressed (juce::KeyPress ('C', juce::ModifierKeys (kShiftMod | kCtrlMod), 'C')),
                   "Ctrl+Shift+C is not answered any more, since the pair was dropped completely");
            check (song.clipboard.valid == clipboardBefore
                       && (int) song.clips.size() == clipsBefore,
                   "and the press copies nothing, so the pattern clipboard stays exactly as the menus left it");
            check (! editor->keyPressed (juce::KeyPress ('V', juce::ModifierKeys (kShiftMod | kCtrlMod), 'V')),
                   "Ctrl+Shift+V is not answered either");
            check ((int) song.clips.size() == clipsBefore && song.clipboard.valid == clipboardBefore,
                   "and no pattern is pasted at the playhead, so the clip count never moved");

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

    //== The Macros tab's vertical scrollbar =================================
    {
        // the tab has to be the one showing before its own strip can be grabbed,
        // which is what a host does by clicking the tab's name
        if (auto* tab = findButton (*editor, "MACROS"))
            tab->onClick();

        // the fill and the scrolls are model writes and view changes rather than
        // edits, which is what keeps the undo line the sections below lean on the
        // same length it was at while this one is about
        const int depthBeforeScroll = proc.getUndoDepth();

        for (int id = 2; id <= 25; ++id)
            song.defineArp (id);

        macros->refresh();

        check (song.arpDefined (25) && song.lastDefArpId() == 25 && song.firstUndefArpId() == 26,
               "the tab holds as many macros as a row has ids to name");

        // the card shades its step strip in groups of "Subticks per row" steps
        // rather than in the lines of cells the strip wraps at
        check (macros->stepBand (0) == 0 && macros->stepBand (5) == 0
               && macros->stepBand (6) == 1 && macros->stepBand (12) == 2,
               "the step strip's shade changes every Subticks per row steps, six here, rather than every line of sixteen cells");

        std::vector<juce::Slider*> lens;
        collectSliders (*macros, lens);
        check (lens.size() == 25, "each macro the song defines gets its own length field");

        const int pw = macros->getWidth();
        const int ph = macros->getHeight();

        check (lens.back()->isVisible() == false && lens[0]->isVisible(),
               "with nothing scrolled the cards past the panel's foot are already scrolled out of view");

        std::cout << "  macros panel is " << pw << "x" << ph
                  << " with 25 cards of 50px each\n";

        // the wheel has moved the list before, which is what the user could do
        juce::MouseWheelDetails wheelDown { 0.0f, -1.0f, false, false, false };
        macros->mouseWheelMove (makeEvent (*macros, { (float) (pw - 8), (float) (ph - 60) }), wheelDown);
        check (macros->getScrollY() == 40, "turning the wheel moves the list by the amount it rolled");

        // the strip's own column is grabbed rather than turned: a click near its
        // bottom puts the drawn thumb at the pointer, which takes the list to
        // full scroll and leaves the first macro out of view
        macros->mouseDown (makeEvent (*macros, { (float) (pw - 8), (float) (ph - 42) }));
        macros->mouseUp (makeEvent (*macros, { (float) (pw - 8), (float) (ph - 42) }));
        check (macros->getScrollY() > 40, "a click of the strip's column jumps the thumb there rather than rolling a fixed amount");
        check (lens[0]->isVisible() == false, "and the first macro is scrolled out while the last is on view");

        // holding that grab and moving the mouse carries the thumb along, which
        // is what lets the list be dragged to a chosen place by mouse
        macros->mouseDown (makeEvent (*macros, { (float) (pw - 8), (float) (ph - 42) }));
        macros->mouseDrag (makeEvent (*macros, { (float) (pw - 8), 82.0f }));
        macros->mouseUp (makeEvent (*macros, { (float) (pw - 8), 82.0f }));
        check (macros->getScrollY() == 0, "dragging the thumb back to the top of the track takes the list back to its head");
        check (lens[0]->isVisible() && lens[1]->isVisible(), "and the macros at the head of the list are on view again");

        // a drag that started away from the strip is a drag of a card, which
        // picks a step cursor up rather than moving the list
        macros->mouseDown (makeEvent (*macros, { (float) (pw / 2), 120.0f }));
        macros->mouseDrag (makeEvent (*macros, { (float) (pw / 2), 300.0f }));
        macros->mouseUp (makeEvent (*macros, { (float) (pw / 2), 300.0f }));
        check (macros->getScrollY() == 0, "a drag of a card rather than of the strip leaves the list where it was");

        for (int id = 2; id <= 25; ++id)
            song.clearArp (id);

        macros->refresh();

        check (song.lastDefArpId() == 1 && song.firstUndefArpId() == 2,
               "taking the macros back leaves the default one and nothing else a row can name");
        check (lens[0]->isVisible() && lens.back()->isVisible() == false,
               "and the fields the fill had built for the macros now gone keep their room but stay hidden");
        check (macros->getScrollY() == 0, "with the list back to what fits, nothing is scrolled any more");
        check (proc.getUndoDepth() == depthBeforeScroll,
               "filling the tab and dragging its scroll are a fixture and a view rather than edits, so the undo line is left the same length");

        // the tab the sections below click through is the tracker's again
        if (auto* tab = findButton (*editor, "TRACKER"))
            tab->onClick();
    }

    //== Settings ============================================================
    if (auto* snap = findToggle (*settings, "Snap to grid"))
    {
        snap->setToggleState (false, juce::dontSendNotification);
        snap->onClick();
        check (! song.settings.snapEnabled, "the settings tab's own row writes the state it names");
    }
    else
    {
        check (false, "the settings panel exposes the snap row");
    }

    // the row that used to switch the values between hex and decimal is gone
    // altogether, as is the setting it wrote: every number the plug-in reads
    // and draws is decimal now, so an older state that still carries the hex
    // property simply has nothing to read it into, the way the dropped paste
    // row's property does just below
    check (findToggle (*settings, "Hexadecimal values") == nullptr,
           "the Settings tab no longer shows the row that used to switch values to hex");

    // the pattern paste's default row is dropped rather than left saved and
    // shown: its only reader went away with the pattern clipboard's dropped
    // Ctrl+Shift pair and the timeline's and pool menus name their own
    // instance or unique choice, so the panel loses the row and the saved
    // settings block loses the property, an older song that still carries it
    // simply having nothing to read it into
    check (findToggle (*settings, "Paste as unique pattern by default") == nullptr,
           "the Settings panel no longer shows the pattern paste's default row");
    check (! song.toValueTree().toXmlString().contains ("pasteAsUnique"),
           "the saved settings block no longer carries the dropped property");

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

        //== the bend letter as one undoable action ======================
        clickRow (6);

        for (int i = 0; i < 32
             && tracker->getCursorField() != rtr::TrackerComponent::Field::cmdNum; ++i)
        {
            tracker->keyPressed (juce::KeyPress (juce::KeyPress::rightKey, juce::ModifierKeys(), 0));
        }

        check (tracker->getCursorField() == rtr::TrackerComponent::Field::cmdNum,
               "a short run of fields lands the cursor on a command number");

        const int bendRow = tracker->getCursorRow();
        const int bendCh = tracker->getCursorChannel();

        auto bendCellOf = [&]
        {
            auto* pp = song.activePattern();

            if (pp != nullptr && bendRow < (int) pp->rows.size()
                && bendCh < (int) pp->rows[(size_t) bendRow].size())
                return pp->cell (bendRow, bendCh);

            return rtr::Cell();
        };

        const int argumentBefore = bendCellOf().cmdVal;
        const juce::String beforeBendSig = sigAt (bendRow, bendCh);

        tracker->keyPressed (juce::KeyPress ('B', juce::ModifierKeys(), 'B'));
        check (bendCellOf().cmd == rtr::kBendCmd, "the letter claims that field and writes the bend command");
        check (bendCellOf().cmdVal == (argumentBefore != rtr::kEmpty ? argumentBefore : rtr::kBendCentreVal),
               "and leaves the wheel's centre as the argument when the cell had none of its own");

        const juce::String bendSig = sigAt (bendRow, bendCh);
        check (bendSig != beforeBendSig, "and the claim is one change of the model");

        check (pressUndo(), "the editor takes Ctrl+Z over the claim");
        check (sigAt (bendRow, bendCh) == beforeBendSig,
               "Ctrl+Z takes the letter and the argument it wrote with it back");

        check (pressRedo(), "Ctrl+Y puts the bend pair back");
        check (sigAt (bendRow, bendCh) == bendSig, "and the same command number and argument come back");

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
        if (auto* snap = findToggle (*settings, "Snap to grid"))
        {
            snap->setToggleState (true, juce::dontSendNotification);
            snap->onClick();
            check (song.settings.snapEnabled, "switching the grid snap back on is an undoable action");

            pressUndo();
            check (! song.settings.snapEnabled, "Ctrl+Z puts it back off, the Settings tab included");

            pressRedo();
            check (song.settings.snapEnabled, "Ctrl+Y switches it on again");
        }
        else
        {
            check (false, "the settings tab's snap row is out of reach for the undo check");
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

            // the Alt pair belongs to every panel rather than only to the
            // editor, because JUCE sends a key press to the modal component and
            // walks up only from there: while a right-click menu is open or a
            // pattern is being dragged from the pool, the editor is never
            // consulted at all, so the panel the press was routed to has to
            // spend it on the undo line itself. The tracker is one of those
            // panels, and it is the one the cell menu is attached to
            tracker->keyPressed (juce::KeyPress ('z', juce::ModifierKeys(), 'z'));
            const juce::String enteredAltSig = sigAt (altRow, altCh);
            check (enteredAltSig != beforeAltSig,
                   "the same letter without Alt still enters a note there");

            const int depthEnteredAlt = proc.getUndoDepth();

            check (tracker->keyPressed (juce::KeyPress ('z', juce::ModifierKeys (kAltMod), 'z')),
                   "the tracker answers Alt+Z of its own rather than letting it through to the editor");
            check (proc.getUndoDepth() == depthEnteredAlt - 1,
                   "and the answer spends the press on the undo line instead of dropping it");
            check (sigAt (altRow, altCh) == beforeAltSig,
                   "so the Alt spelling still enters no note of its own");

            check (tracker->keyPressed (juce::KeyPress ('y', juce::ModifierKeys (kAltMod), 'y')),
                   "the tracker answers Alt+Y too");
            check (proc.getUndoDepth() == depthEnteredAlt && sigAt (altRow, altCh) == enteredAltSig,
                   "and that answer walks the same point of the line forward again");

            // a letter the pair does not name stays out of the tracker's own key
            // entry as well, and Alt held with Shift is left alone rather than
            // spent on a redo alias, which is what the hosts found they swallow
            check (! tracker->keyPressed (juce::KeyPress ('q', juce::ModifierKeys (kAltMod), 'q')),
                   "a letter the pair does not name is still left for the editor to handle");
            check (proc.getUndoDepth() == depthEnteredAlt && sigAt (altRow, altCh) == enteredAltSig,
                   "and that letter neither steps the line nor types a note there");

            const int depthLeftAlt = proc.getUndoDepth();
            check (! tracker->keyPressed (juce::KeyPress ('z', juce::ModifierKeys (kShiftMod | kAltMod), 'z')),
                   "the tracker leaves an Alt+Shift combination alone rather than answering it");
            check (proc.getUndoDepth() == depthLeftAlt && sigAt (altRow, altCh) == enteredAltSig,
                   "so that press neither types a note nor steps the line");

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
            // nothing about the walk above has to be fixed here any more: the
            // digits a field takes at the keyboard are its three decimal ones
            // whatever state the song is walked back into, so a digit still in
            // its buffer behaves the same on either spelling
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

        //== the same pair answered by every panel =========================
        {
            // the undo line belongs to the editor, so what each panel answers
            // with is the editor's own step rather than a copy of it: one press
            // walks one point no matter which panel the host routed it to, which
            // is what the right-click menu actions and the pool's drag of a
            // pattern onto the timeline need, where JUCE answers the press with
            // the menu's or the drag's panel and never reaches the editor
            auto pressUndoOn = [&] (auto* panel)
            { return panel->keyPressed (juce::KeyPress ('Z', juce::ModifierKeys (kAltMod), 'Z')); };
            auto pressRedoOn = [&] (auto* panel)
            { return panel->keyPressed (juce::KeyPress ('Y', juce::ModifierKeys (kAltMod), 'Y')); };
            auto pressAltShiftOn = [&] (auto* panel)
            { return panel->keyPressed (juce::KeyPress ('Z', juce::ModifierKeys (kShiftMod | kAltMod), 'Z')); };

            // one entered note so the line has a point ahead of the cursor that
            // every panel below can be offered a step for, and one the checks
            // after this section can name on its own
            clickRow (31);
            const int panelRow = tracker->getCursorRow();
            const int panelCh = tracker->getCursorChannel();
            const juce::String panelSigBefore = sigAt (panelRow, panelCh);

            tracker->keyPressed (juce::KeyPress ('z', juce::ModifierKeys(), 'z'));
            const juce::String panelSigEntered = sigAt (panelRow, panelCh);
            check (panelSigEntered != panelSigBefore,
                   "one entered note gives the line a point to walk back");

            const int panelDepth = proc.getUndoDepth();
            check (panelDepth >= 1, "the entered note leaves a point behind the cursor");

            auto walkPairOn = [&] (auto* panel, const juce::String& name)
            {
                const int depthHere = proc.getUndoDepth();

                check (pressUndoOn (panel),
                       name + " answers Alt+Z with a step of its own");
                check (proc.getUndoDepth() == depthHere - 1,
                       name + " spends the press on the editor's line rather than on one of its own");
                check (pressRedoOn (panel),
                       name + " answers Alt+Y too");
                check (proc.getUndoDepth() == depthHere && sigAt (panelRow, panelCh) == panelSigEntered,
                       name + " walks that same point forward again, for one step per press");

                const int depthShift = proc.getUndoDepth();
                check (! pressAltShiftOn (panel),
                       name + " leaves an Alt+Shift combination to the host");
                check (proc.getUndoDepth() == depthShift,
                       name + " neither steps nor redoes on that press");
            };

            walkPairOn (timeline, "the timeline");
            walkPairOn (pool, "the pool");
            walkPairOn (settings, "the settings panel");
            walkPairOn (tools, "the tools panel");
            walkPairOn (macros, "the macros panel");

            check (proc.getUndoDepth() == panelDepth && sigAt (panelRow, panelCh) == panelSigEntered,
                   "and answering the pair in five panels walks the line back and forth without net effect");
        }

        //== the keyboard focus a menu or a drag hands back =================
        {
            // a context menu's popup and a pattern the pool started dragging are
            // windows with no parent component, so closing one takes the keyboard
            // focus with it and afterwards nothing is focused at all: a press is
            // then answered only as far as the walk up from the plug-in's top
            // component reaches, which leaves the note entry, the cursor moves
            // and the clears the tracker owns unheard until the mouse is clicked
            // again. The panels that own a menu, an alert or a drag hand that
            // focus back, and to the tracker rather than to themselves, since
            // focus resting on a panel would leave the tracker deaf the same way.
            // A modal popup cannot be opened by these tests off a desktop, so what
            // is pinned here is that the hand-back is wired, that the reachable
            // sites run it ahead of their guards, and that it spends nothing on
            // the undo line
            check (tracker->focusRescue != nullptr && timeline->focusRescue != nullptr
                       && pool->focusRescue != nullptr,
                   "the tracker, timeline and pool hand keyboard focus back");

            int rescues = 0;
            auto countRescue = [&] { ++rescues; };

            tracker->focusRescue = countRescue;
            timeline->focusRescue = countRescue;
            pool->focusRescue = countRescue;

            const int depthBeforeDrop = proc.getUndoDepth();

            // a drop of something the timeline is not interested in still ends the
            // drag, so the hand-back has to run ahead of the guard that ignores
            // the change rather than only behind it
            const juce::DragAndDropTarget::SourceDetails unrelated (
                "other-pattern:7", pool, { 816, 80 });

            timeline->itemDropped (unrelated);
            check (rescues == 1, "a drop hands keyboard focus back even when nothing is modelled");
            check (proc.getUndoDepth() == depthBeforeDrop,
                   "and the hand-back spends no point of the undo line");

            // releasing a drag the pool started ends it wherever the pointer was,
            // including over the pool itself, where no drop ever happens
            pool->mouseUp (makeEvent (*pool, { 20.0f, 20.0f }));
            check (rescues == 2, "releasing a drag hands keyboard focus back too");

            // and the guard that refuses a change while the host plays comes after
            // the hand-back, since a drag is still possible then
            auto runBlock = [&]
            {
                juce::AudioBuffer<float> audio (2, 64);
                juce::MidiBuffer midi;
                proc.processBlock (audio, midi);
            };

            head->playing = true;
            head->ppq = 200.0;
            runBlock();
            check (proc.getTransport().playing, "the transport reports the host is playing");

            timeline->itemDropped (unrelated);
            check (rescues == 3, "the hand-back runs ahead of the guard while the host plays");
            check (proc.getUndoDepth() == depthBeforeDrop,
                   "and still nothing is modelled or recorded while the host plays");

            head->playing = false;
            runBlock();
            check (! proc.getTransport().playing, "the stopped transport leaves the model editable");

            // what the pinned scenario asks for: the pair heard straight after a
            // pattern was dragged out of the pool and dropped onto the timeline,
            // with no click in between
            const int activeBeforeDrop = song.activePatternId;
            const int dropPatternId = song.patterns.back().id;
            const int clipsBefore = (int) song.clips.size();
            const juce::DragAndDropTarget::SourceDetails drop (
                "rtr-pattern:" + juce::String (dropPatternId), pool, { 60000, 80 });

            timeline->itemDropped (drop);
            check ((int) song.clips.size() == clipsBefore + 1, "the drop lands a clip");
            check (proc.getUndoDepth() == depthBeforeDrop + 1,
                   "and the drop leaves a point the next press can walk back");

            check (tracker->keyPressed (juce::KeyPress ('z', juce::ModifierKeys (kAltMod), 'z')),
                   "the Alt spelling heard right after the drop takes that clip back");
            check (proc.getUndoDepth() == depthBeforeDrop && (int) song.clips.size() == clipsBefore,
                   "one press walks one point of the line");

            check (editor->keyPressed (juce::KeyPress ('y', juce::ModifierKeys (kAltMod), 'y')),
                   "and the redo heard at the editor puts the dropped clip back");
            check (proc.getUndoDepth() == depthBeforeDrop + 1
                       && (int) song.clips.size() == clipsBefore + 1,
                   "one press at the editor walks that same point forward");

            // the walk is asked for again rather than left mid-line, so the checks
            // after this section read the model the drop left it in: undoing the
            // redone drop brings the clip, and the pattern it made active, back
            pressUndo();
            check ((int) song.clips.size() == clipsBefore && song.activePatternId == activeBeforeDrop
                       && proc.getUndoDepth() == depthBeforeDrop,
                   "undoing the redone drop brings the model and the active pattern back");
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

        //== the Alt spellings of the cell clipboard =========================
        {
            // the hosts answer Ctrl+C and Ctrl+V with their own clipboard the way
            // they answer the Ctrl trio with their own undo, so Bitwig and Reaper
            // never hand the plug-in those events and the cells this plug-in copies
            // and pastes are unreachable there. Alt is the spelling they leave free,
            // and the pair needs both halves of what the undo pair needed: it is
            // answered by every panel, since a press routed to a panel never reaches
            // the editor, and it claims a press whose step does nothing, since a
            // note entered by letters would otherwise type the pair's own c as a C
            // at the cursor and a paste landed at a panel and at the editor would
            // walk two points of the line for one press
            auto pressCopyOn = [&] (auto* panel)
            { return panel->keyPressed (juce::KeyPress ('C', juce::ModifierKeys (kAltMod), 'C')); };
            auto pressPasteOn = [&] (auto* panel)
            { return panel->keyPressed (juce::KeyPress ('V', juce::ModifierKeys (kAltMod), 'V')); };
            auto pressCutOn = [&] (auto* panel)
            { return panel->keyPressed (juce::KeyPress ('X', juce::ModifierKeys (kAltMod), 'X')); };
            auto pressCopyAltShiftOn = [&] (auto* panel)
            { return panel->keyPressed (juce::KeyPress ('C', juce::ModifierKeys (kShiftMod | kAltMod), 'C')); };
            auto pressCutAltShiftOn = [&] (auto* panel)
            { return panel->keyPressed (juce::KeyPress ('X', juce::ModifierKeys (kShiftMod | kAltMod), 'X')); };
            auto pressAltLetterOn = [&] (auto* panel)
            { return panel->keyPressed (juce::KeyPress ('q', juce::ModifierKeys (kAltMod), 'q')); };
            auto pressCtrlCopyOn = [&] (auto* panel)
            { return panel->keyPressed (juce::KeyPress ('C', juce::ModifierKeys (kCtrlMod), 'C')); };
            auto pressCtrlCutOn = [&] (auto* panel)
            { return panel->keyPressed (juce::KeyPress ('X', juce::ModifierKeys (kCtrlMod), 'X')); };

            // the signature a cell that lost every one of its fields reads back as,
            // which is what a cut leaves the cells it took holding
            const auto clearedCell = []
            {
                juce::String s;
                const rtr::Cell emptied;
                s << emptied.note << "/" << emptied.inst << "/" << emptied.vol
                  << "/" << emptied.pan << "/" << emptied.cmd << "/" << emptied.cmdVal;
                return s;
            }();

            check (tracker->copyStep != nullptr && timeline->copyStep != nullptr
                   && pool->copyStep != nullptr && settings->copyStep != nullptr
                   && tools->copyStep != nullptr && macros->copyStep != nullptr
                   && tracker->pasteStep != nullptr && timeline->pasteStep != nullptr
                   && pool->pasteStep != nullptr && settings->pasteStep != nullptr
                   && tools->pasteStep != nullptr && macros->pasteStep != nullptr
                   && tracker->cutStep != nullptr && timeline->cutStep != nullptr
                   && pool->cutStep != nullptr && settings->cutStep != nullptr
                   && tools->cutStep != nullptr && macros->cutStep != nullptr,
                   "all six panels were given a copy, a paste and a cut step of their own");

            auto* p = song.activePattern();
            check (p != nullptr, "an active pattern is there for the clipboard's Alt pair");

            if (p != nullptr)
            {
                // the fixture's rows come from where a click lands rather than from
                // a counted row, since the view scrolls and the click's y is read
                // through that scroll
                // the paste's target walks down the pattern one panel at a time, so
                // every panel gets a fresh anchor: a paste that lands cells the model
                // already holds leaves the line alone, and these checks want one
                // point per press rather than the dedup's silence
                int pasteIndex = 0;

                auto copyPairOn = [&] (auto* panel, const juce::String& name)
                {
                    clickRow (0);
                    const int topRow = tracker->getCursorRow();
                    const int srcCh = tracker->getCursorChannel();
                    const int dstRow = topRow + 4 + 2 * (pasteIndex++);

                    if (topRow + 1 >= p->length || dstRow + 1 >= p->length)
                    {
                        check (false, name + " has no room left for the fixture the pair needs");
                        return;
                    }

                    tracker->mouseDown (makeEvent (*tracker, { 70.0f, 55.5f }));
                    tracker->mouseDrag (makeEvent (*tracker, { 70.0f, 74.5f }));
                    tracker->mouseUp (makeEvent (*tracker, { 70.0f, 74.5f }));

                    int selCount = 0;
                    int selMinRow = p->length;
                    int selMaxRow = -1;
                    int selMinCh = p->channels;
                    int selMaxCh = -1;

                    for (int r = 0; r < p->length; ++r)
                        for (int c = 0; c < p->channels; ++c)
                        {
                            if (! tracker->isCellSelected (r, c))
                                continue;

                            ++selCount;
                            selMinRow = juce::jmin (selMinRow, r);
                            selMaxRow = juce::jmax (selMaxRow, r);
                            selMinCh = juce::jmin (selMinCh, c);
                            selMaxCh = juce::jmax (selMaxCh, c);
                        }

                    check (selCount == 2 && selMaxCh == selMinCh && selMaxRow == selMinRow + 1,
                           name + " is offered a two-cell column of one channel to copy");

                    // the data the fixture puts into the selected cells comes
                    // through the clipboard and a paste rather than a silent write
                    // into the model, so the undo line has seen it: a cut that
                    // brings the model back to the state the cursor already sits
                    // on is deduped and records no point of its own, and here the
                    // cut has to leave one
                    rtr::Cell seeded;
                    seeded.note = 101;
                    seeded.vol = 41;
                    seeded.pan = 42;

                    rtr::Cell taken;
                    taken.inst = 3;

                    tracker->cellClipboard.entries = { { 0, 0, seeded }, { 1, 0, taken } };

                    // the drag left the cursor at its own anchor, which is the
                    // selection's top-left, so the fixture's paste lands on the
                    // very two cells the panel is about to cut
                    check (tracker->getCursorRow() == selMinRow && tracker->getCursorChannel() == selMinCh,
                           name + ": the fixture's anchor is the selection's top-left, where its paste has to land");

                    tracker->pasteClipboardAtCursor (rtr::kPasteAll);

                    tracker->setCursorRow (dstRow);
                    check (tracker->getCursorRow() == dstRow && tracker->getCursorChannel() == srcCh,
                           name + ": the cursor is parked below the fixture, where the paste has to land");

                    const juce::String copiedFirst = sigAt (selMinRow, selMinCh);
                    const juce::String copiedSecond = sigAt (selMaxRow, selMinCh);
                    const int depthHere = proc.getUndoDepth();
                    const juce::String targetBefore = sigAt (dstRow, srcCh);

                    check (pressCopyOn (panel),
                           name + " answers Alt+C of its own rather than leaving the press for the editor");
                    check (tracker->hasCellClipboard(),
                           name + ": Alt+C copies what the tracker holds selected to the tracker's clipboard");
                    check (proc.getUndoDepth() == depthHere,
                           name + ": copying records no point of the undo line");
                    check (sigAt (dstRow, srcCh) == targetBefore,
                           name + ": the Alt held c enters no note of its own at the cursor, where a bare c would type one");
                    check (tracker->getCursorRow() == dstRow && tracker->getCursorChannel() == srcCh,
                           name + ": copying leaves the cursor where it was for the paste");

                    //== the cut ==================================================
                    // Alt+X takes the same copy and then empties exactly the cells
                    // it took, so the selection's data is already on the clipboard
                    // by the time the paste below looks for its anchor and the
                    // cells move to the cursor rather than repeat themselves there
                    check (pressCutOn (panel),
                           name + " answers Alt+X of its own rather than leaving the press for the editor");
                    check (tracker->hasCellClipboard(),
                           name + ": Alt+X puts the selected cells on the clipboard before it empties them");
                    check (proc.getUndoDepth() == depthHere + 1,
                           name + ": the cut records one point of the undo line, which is the wipe's alone");
                    check (sigAt (selMinRow, selMinCh) == clearedCell && sigAt (selMaxRow, selMinCh) == clearedCell,
                           name + ": Alt+X empties exactly the cells it took and nothing beside them");
                    check (tracker->isCellSelected (selMinRow, selMinCh) && tracker->isCellSelected (selMaxRow, selMinCh),
                           name + ": the cut leaves the selection painted where it was, so the next paste still anchors at the cursor");
                    check (sigAt (dstRow, srcCh) == targetBefore,
                           name + ": the Alt held x types no D sharp of its own at the cursor, where a bare x would type one");
                    check (tracker->getCursorRow() == dstRow && tracker->getCursorChannel() == srcCh,
                           name + ": the cut leaves the cursor where it was for the paste");

                    check (pressPasteOn (panel),
                           name + " answers Alt+V too");
                    check (proc.getUndoDepth() == depthHere + 2,
                           name + ": one press walks one point, and the cut and a paste that wrote cells leave those two");
                    check (sigAt (dstRow, srcCh) == copiedFirst,
                           name + ": Alt+V pastes with everything, so the whole cell the cut took lands at the cursor, empty fields included");
                    check (sigAt (dstRow + 1, srcCh) == copiedSecond,
                           name + ": the second cell the cut took keeps its offset below the cursor's anchor, which is what makes the cut and the paste a move of those cells");

                    const int depthPasted = proc.getUndoDepth();

                    check (! pressCopyAltShiftOn (panel),
                           name + " leaves an Alt+Shift combination to the host rather than copying");
                    check (proc.getUndoDepth() == depthPasted,
                           name + ": that press copies or pastes nothing");
                    check (! pressAltLetterOn (panel),
                           name + " leaves a letter neither pair names alone");
                    check (proc.getUndoDepth() == depthPasted,
                           name + ": and it types or writes nothing for it either");
                    check (! pressCtrlCopyOn (panel),
                           name + " leaves the Ctrl spelling to the editor rather than claiming it, so a host that does forward it still finds the plug-in's clipboard");
                    check (! pressCutAltShiftOn (panel),
                           name + " leaves an Alt+Shift x to the host rather than cutting, the whole Alt+Shift family's meaning");
                    check (proc.getUndoDepth() == depthPasted,
                           name + ": that press copies, pastes or cuts nothing of the cells");
                    check (! pressCtrlCutOn (panel),
                           name + " leaves the Ctrl x spelling to the host rather than spending it, since the clip slice moved to the Alt+S letter below");
                    check (proc.getUndoDepth() == depthPasted && tracker->hasCellClipboard(),
                           name + ": no panel spends the Ctrl x spelling, so the cells and the clipboard stay exactly as the cut left them");
                };

                copyPairOn (tracker, "the tracker");
                copyPairOn (timeline, "the timeline");
                copyPairOn (pool, "the pool");
                copyPairOn (settings, "the settings panel");
                copyPairOn (tools, "the tools panel");
                copyPairOn (macros, "the macros panel");

                // X11 hands a key held with Alt over as the character that press
                // printed, so the pair is matched over that letter too, which is
                // how the lowercase spellings the hosts send are heard
                {
                    clickRow (0);
                    const int topRow = tracker->getCursorRow();
                    const int srcCh = tracker->getCursorChannel();
                    const int dstRow = topRow + 4 + 2 * pasteIndex++;

                    tracker->mouseDown (makeEvent (*tracker, { 70.0f, 55.5f }));
                    tracker->mouseDrag (makeEvent (*tracker, { 70.0f, 74.5f }));
                    tracker->mouseUp (makeEvent (*tracker, { 70.0f, 74.5f }));

                    // the row the cut left empty has to be filled again, and
                    // through the clipboard rather than silently: the cut's point
                    // of the line is what the checks below count
                    rtr::Cell seeded;
                    seeded.note = 111;
                    seeded.vol = 51;
                    seeded.pan = 52;

                    rtr::Cell taken;
                    taken.inst = 4;

                    tracker->cellClipboard.entries = { { 0, 0, seeded }, { 1, 0, taken } };
                    tracker->pasteClipboardAtCursor (rtr::kPasteAll);

                    const int depthHere = proc.getUndoDepth();

                    const juce::String copiedFirst = sigAt (topRow, srcCh);
                    const juce::String copiedSecond = sigAt (topRow + 1, srcCh);

                    tracker->setCursorRow (dstRow);
                    const juce::String targetBefore = sigAt (dstRow, srcCh);

                    check (tracker->keyPressed (juce::KeyPress ('c', juce::ModifierKeys (kAltMod), 'c')),
                           "the tracker takes the lowercase Alt+c spelling the way X11 hands it");
                    check (proc.getUndoDepth() == depthHere, "and the lowercase copy still records nothing");
                    check (tracker->keyPressed (juce::KeyPress ('x', juce::ModifierKeys (kAltMod), 'x')),
                           "the lowercase Alt+x cut spelling is heard the same way");
                    check (proc.getUndoDepth() == depthHere + 1, "and the lowercase cut leaves its one point of the line");
                    check (sigAt (topRow, srcCh) == clearedCell && sigAt (topRow + 1, srcCh) == clearedCell,
                           "the lowercase cut empties the cells it took the way the uppercase one does");
                    check (tracker->keyPressed (juce::KeyPress ('v', juce::ModifierKeys (kAltMod), 'v')),
                           "the lowercase Alt+v spelling is taken as well");
                    check (proc.getUndoDepth() == depthHere + 2, "for one more point of the line");
                    check (sigAt (dstRow, srcCh) == copiedFirst && sigAt (dstRow + 1, srcCh) == copiedSecond,
                           "and the lowercase steps land the same two cells at the cursor, which is the cut's data moved from where it was cut");
                }

                // where a host does forward the Ctrl pair, it still means the
                // plug-in's own clipboard rather than a meaning that changed per
                // host, so neither spelling has to be resolved by who answered
                {
                    const int depthHere = proc.getUndoDepth();
                    check (editor->keyPressed (juce::KeyPress ('C', juce::ModifierKeys (kCtrlMod), 'C')),
                           "the editor still answers the Ctrl spelling");
                    check (tracker->hasCellClipboard() && proc.getUndoDepth() == depthHere,
                           "and the Ctrl pair still copies the same cells and records nothing of its own");
                }
            }
        }
    }

    //== The clip slice's Alt+S spelling ==================================
    {
        // the timeline answered the slice with Ctrl+X, but that letter is the
        // host's own cut: the user's check found Alt+Shift+X and Ctrl+X changed
        // nothing about the plug-in in either host, so the shortcut moved to
        // Alt+S, the free letter, and it has one spelling shared by every panel
        // rather than a Ctrl letter per host. The letter is one the tracker's
        // note entry types as C sharp, so a panel holding the slice has to claim
        // the press before its own key entry can leave a note at the cursor
        auto pressSliceOn = [&] (auto* panel)
        { return panel->keyPressed (juce::KeyPress ('S', juce::ModifierKeys (kAltMod), 'S')); };
        auto pressSliceAltShiftOn = [&] (auto* panel)
        { return panel->keyPressed (juce::KeyPress ('S', juce::ModifierKeys (kShiftMod | kAltMod), 'S')); };
        auto pressCtrlSliceOn = [&] (auto* panel)
        { return panel->keyPressed (juce::KeyPress ('X', juce::ModifierKeys (kCtrlMod), 'X')); };

        check (tracker->sliceStep != nullptr && timeline->sliceStep != nullptr
               && pool->sliceStep != nullptr && settings->sliceStep != nullptr
               && tools->sliceStep != nullptr && macros->sliceStep != nullptr,
               "all six panels were given the slice step the editor injects");

        // the slice belongs to the timeline's edits, so it is refused while the
        // transport runs and needs the grid on; the fixture places a fresh clip
        // of a fresh pattern per press, starts it on a beat the zoom's grid can
        // never snap off, and splits it at its own middle row, which stays clear
        // of both ends however coarse the grid is
        head->playing = false;

        // the transport snapshot the slice's lock reads only refreshes through a
        // block, and an earlier section left the host running, so the press below
        // is offered the state the transport is really in
        juce::AudioBuffer<float> parkedAudio (2, 64);
        juce::MidiBuffer parkedMidi;
        proc.processBlock (parkedAudio, parkedMidi);

        song.settings.snapEnabled = true;
        song.settings.adaptiveGrid = false;
        song.settings.fixedGridRows = 4;

        const float chCentre = (float) tracker->channelBoundsFor (0).getCentreX();
        int sliceFixture = 0;

        auto sliceOneOn = [&] (auto* panel, const juce::String& name, const auto& pressOn)
        {
            const double startBeats = 4000.0 + (double) (sliceFixture++) * 32.0;
            const int patternId = song.addPattern (name + " fixture", 64, 1).id;

            check (song.addClip (patternId, startBeats) != nullptr,
                   name + ": the fixture has a clip placed for the slice to split");

            // the cursor is parked in the view so the check below can read the
            // very cell the tracker's note entry would have taken this s for,
            // which is a C sharp
            tracker->refresh();
            tracker->mouseDown (makeEvent (*tracker, { chCentre, 55.5f }));
            tracker->mouseUp (makeEvent (*tracker, { chCentre, 55.5f }));
            const int cursorRow = tracker->getCursorRow();
            const int cursorCh = tracker->getCursorChannel();

            const int heldId = song.activePatternId;
            auto* heldBefore = song.findPattern (heldId);
            const rtr::Cell cellBefore = heldBefore != nullptr
                                            ? heldBefore->cell (cursorRow, cursorCh)
                                            : rtr::Cell();

            song.editPlayheadBeats = startBeats + 32.0 * song.beatsPerRow();

            const int clipsBefore = (int) song.clips.size();
            const int patternsBefore = (int) song.patterns.size();
            const int depthBefore = proc.getUndoDepth();

            check (pressOn (panel),
                   name + " claims the Alt+S spelling with the clip slice");
            check ((int) song.clips.size() == clipsBefore + 1,
                   name + ": one clip becomes two");
            check ((int) song.patterns.size() == patternsBefore + 2,
                   name + ": and the pattern gets its two new copies to hold the halves");
            check (proc.getUndoDepth() == depthBefore + 1,
                   name + ": and the slice records exactly one point of the undo line, however far the press was routed");

            // the Alt held letter stays out of the tracker's note entry, which
            // would otherwise type this very s as a C sharp at the cursor: the
            // pattern the tracker was holding is still in the pool, the slice
            // only copies out of it, so its own cell reads back the same way
            auto* heldAfter = song.findPattern (heldId);
            const rtr::Cell cellAfter = heldAfter != nullptr
                                            ? heldAfter->cell (cursorRow, cursorCh)
                                            : rtr::Cell();
            check (cellAfter.note == cellBefore.note && cellAfter.inst == cellBefore.inst
                       && cellAfter.vol == cellBefore.vol && cellAfter.pan == cellBefore.pan
                       && cellAfter.cmd == cellBefore.cmd && cellAfter.cmdVal == cellBefore.cmdVal,
                   name + ": and the Alt held s types no C sharp at the cell the cursor sits on");
        };

        sliceOneOn (tracker, "the tracker", pressSliceOn);
        sliceOneOn (timeline, "the timeline", pressSliceOn);
        sliceOneOn (pool, "the pool", pressSliceOn);
        sliceOneOn (settings, "the settings panel", pressSliceOn);
        sliceOneOn (tools, "the tools panel", pressSliceOn);
        sliceOneOn (macros, "the macros panel", pressSliceOn);

        // X11 hands a key held with Alt over as the character that press printed,
        // so the slice is matched over that letter too, which is how the
        // lowercase spelling a host sends is heard
        {
            const double startBeats = 4000.0 + (double) (sliceFixture++) * 32.0;
            const int patternId = song.addPattern ("lowercase fixture", 64, 1).id;

            check (song.addClip (patternId, startBeats) != nullptr,
                   "the lowercase fixture has a clip of its own to split");
            song.editPlayheadBeats = startBeats + 32.0 * song.beatsPerRow();

            const int clipsBefore = (int) song.clips.size();
            const int depthBefore = proc.getUndoDepth();

            check (tracker->keyPressed (juce::KeyPress ('s', juce::ModifierKeys (kAltMod), 's')),
                   "the tracker takes the lowercase Alt+s spelling the way X11 hands it");
            check ((int) song.clips.size() == clipsBefore + 1 && proc.getUndoDepth() == depthBefore + 1,
                   "and it splits the same way and leaves its one point of the line");
        }

        // the letters the Alt family refuses rather than spending: Alt+Shift
        // belongs whole to the host and Ctrl+X is the host's own cut, so neither
        // is answered here, and the plug-in no longer owns a Ctrl spelling of the
        // slice at all
        {
            const int clipsBefore = (int) song.clips.size();
            const int patternsBefore = (int) song.patterns.size();
            const int depthBefore = proc.getUndoDepth();

            check (! pressSliceAltShiftOn (tracker),
                   "the tracker leaves an Alt+Shift s to the host rather than slicing, the whole Alt+Shift family's meaning");
            check (! pressCtrlSliceOn (tracker),
                   "and no panel spends the Ctrl x spelling, which belongs to the host's cut");
            check ((int) song.clips.size() == clipsBefore && (int) song.patterns.size() == patternsBefore
                       && proc.getUndoDepth() == depthBefore,
                   "and neither press splits anything, so the clips and the undo line stay as they were");
        }

        // the Ctrl spellings the plug-in no longer answers at all: Ctrl+X used to
        // be the slice and Ctrl+Shift+C / Ctrl+Shift+V were the pattern
        // clipboard's pair, and all three of them belong to the host, so a host
        // that forwards them finds the plug-in silent rather than a meaning that
        // changed per host
        {
            const int clipsBefore = (int) song.clips.size();
            const int patternsBefore = (int) song.patterns.size();
            const int depthBefore = proc.getUndoDepth();
            const bool clipboardBefore = song.clipboard.valid;

            check (! editor->keyPressed (juce::KeyPress ('X', juce::ModifierKeys (kCtrlMod), 'X')),
                   "the editor no longer answers the Ctrl+X slice, since that letter is the host's cut");
            check (! editor->keyPressed (juce::KeyPress ('C', juce::ModifierKeys (kShiftMod | kCtrlMod), 'C')),
                   "and the pattern clipboard's Ctrl+Shift+C is not answered at all, the pair's own half");
            check (! editor->keyPressed (juce::KeyPress ('V', juce::ModifierKeys (kShiftMod | kCtrlMod), 'V')),
                   "nor its Ctrl+Shift+V, so a whole pattern only moves through the timeline's and the pool's menus");
            check (song.clipboard.valid == clipboardBefore
                       && (int) song.clips.size() == clipsBefore
                       && (int) song.patterns.size() == patternsBefore
                       && proc.getUndoDepth() == depthBefore,
                   "and none of the three presses copies, pastes or splits anything, so the pattern clipboard, the clips and the undo line all stay exactly as the menus left them");
        }

        // the slice stays refused while the transport runs, the way every other
        // timeline edit is locked, and the press is still claimed rather than
        // walked over to a note entered at the cursor
        {
            juce::AudioBuffer<float> audio (2, 64);
            juce::MidiBuffer midi;

            head->playing = true;
            proc.processBlock (audio, midi);

            const int clipsBefore = (int) song.clips.size();
            const int depthBefore = proc.getUndoDepth();

            check (pressSliceOn (timeline),
                   "the timeline still claims the Alt+S press while the transport runs");
            check ((int) song.clips.size() == clipsBefore && proc.getUndoDepth() == depthBefore,
                   "and that press splits nothing, since a clip cannot be cut while its rows are playing");

            head->playing = false;
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
