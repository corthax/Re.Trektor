/*
    Renders the Re.Trektor editor to a PNG without a host, and optionally dumps
    the component tree so the layout can be verified without looking at pixels.

    Usage: ReTrektorEditorShot /path/out.png [width height] [--dump]
           [--tab=settings] [--channels=N] [--rows=N] [--scroll=PCT]
           [--click-timeline=PCT] [--hide=0v1c] [--expand-pool]
           [--drag-preview=IDX] [--select=ROW0,CH0,ROW1,CH1] [--paste-at=ROW,CH]
           [--undo=N] [--redo=N] [--arp=0,+4,off,-8] [--arp-loop] [--arp-call=ROW]
           [--arp-call-id=VAL] [--arp-note-off=ROW] [--bend=ROW] [--bend-arg=VAL]
           [--slide=ROW] [--slide-down=ROW] [--slide-arg=VAL]
           [--macros=N] [--cmd-carry]

    --arp= writes signed semitone steps into arpeggiator id 1, so a Macros-tab
    shot shows a step strip of its own length rather than one that is all empty;
    a step written as a lone dot stays empty, one written off steps as the note-off
    the note column's backtick writes so the cell reads OFF, and --arp-loop marks
    that macro looped so its card shows the looped tick. The strip's steps are one
    subtick
    wide and a subtick is a row over the Settings row "Subticks per row", so the
    card shades every second group of that many steps a little darker, which is
    what makes a macro that reaches past its own row read as row spans rather than
    as one long run of numbers. --macros=N has the song define N
    macros, ids 1..N, which is what makes a Macros-tab shot long enough that the
    cards do not fit and the tab has to draw the scrollbar and thumb it offers.
    --arp-call=ROW names an arpeggiator
    call in tracker channel 0 at that row, which is what paints the command
    column's orange letter A and its decimal id; --arp-call-id=VAL names the id
    that call writes, so --arp-call-id=0 makes it the release, whose argument is
    drawn OFF rather than as a 0 and which leaves the rows below it carrying no
    command at all. --arp-note-off=ROW holds the column's note-off in channel 0
    at that row, which is what puts a call beside a note-off rather than beside a
    note or an empty cell. --bend=ROW names a pitch bend in
    tracker channel 0 at that row, which paints the lilac letter B, and --bend-arg=VAL
    names the point on the wheel it bends to so the argument is drawn in decimal
    beside it; with no --bend-arg the row bends to 96, above the wheel's centre
    rather than sitting on it. --slide=ROW names a wheel slide up in tracker
    channel 0 at that row, which paints the lilac letter U, and --slide-down=ROW
    names a slide down there, the letter D, whose command value is one the number
    field cannot be typed as and only the letter itself can name; --slide-arg=VAL
    says how many rows the slide takes to reach its extreme, so the argument is
    drawn in decimal beside the letter, and with no --slide-arg the row slides
    over 12 rows rather than over the default 8 its letter entry leaves.

    --cmd-carry writes the command column's carried case in both channels: a row
    that names only an argument under the explicit CC the demo already has at row
    20, so the rows under it are drawn with the argument that row entered rather
    than the one the letter above called with, and in the other channel a call its
    letter names followed by a row naming only a 0 there, which answers as the
    release and leaves the rows under it carrying neither the letter nor an
    argument.

    --hide= takes channel/letter pairs (i instrument, v volume, p pan, c
    command), so --hide=0v1c hides the volume of channel 0 and the command of
    channel 1: the chips above a channel hide one field of that channel only.

    --select= holds a rectangle of cells selected for the shot, given as row0,
    channel0,row1,channel1 and inclusive at both ends, so --select=6,1,7,1
    paints only channel 1's rows 6 and 7 rather than a band across the pattern.

    --paste-at= goes with --select=: the selected cells take the copy Ctrl+C
    makes and then land at ROW,CH with everything, so the shot shows the pasted
    block at the cursor and what does not fit inside the pattern dropped.

    --undo= and --redo= walk that many points of the undo line after the
    actions above ran, so --select= / --paste-at= followed by --undo=1 shows
    the pasted block retracted the way Ctrl+Z takes it back; a walk that runs
    out of states stops at the end rather than failing the shot.

    --drag-preview= composites the image JUCE floats at the cursor while pool
    item IDX is dragged out of the pool, so the shot can show that only that
    item's cell follows the pointer instead of the whole pool panel.
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ui/PoolComponent.h"
#include "ui/TrackerComponent.h"
#include "ui/TimelineComponent.h"

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_graphics/juce_graphics.h>
#include <iostream>

namespace
{
juce::String describe (juce::Component& c)
{
    juce::String s = juce::String (c.getX()) + "," + juce::String (c.getY()) + " "
                   + juce::String (c.getWidth()) + "x" + juce::String (c.getHeight());

    if (auto* label = dynamic_cast<juce::Label*> (&c))
        s << "  text=\"" << label->getText() << "\"";
    else if (auto* button = dynamic_cast<juce::Button*> (&c))
        s << "  text=\"" << button->getButtonText() << "\""
          << (button->getToggleState() ? " [on]" : " [off]");
    else if (auto* slider = dynamic_cast<juce::Slider*> (&c))
        s << "  value=" << juce::String (slider->getValue(), 2);
    else if (auto* tabs = dynamic_cast<juce::TabbedComponent*> (&c))
        s << "  tabs=" << tabs->getNumTabs();

    if (! c.isVisible())
        s << "  (hidden)";

    return s;
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

juce::MouseEvent makeEvent (juce::Component& c, juce::Point<float> pos)
{
    auto source = juce::Desktop::getInstance().getMainMouseSource();
    const auto now = juce::Time::getCurrentTime();

    return juce::MouseEvent (source, pos, juce::ModifierKeys(), 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                             &c, &c, now, pos, now, 1, false);
}

void dump (juce::Component& c, int depth, int parentW, int parentH)
{
    const juce::Rectangle<int> b (c.getX(), c.getY(), c.getWidth(), c.getHeight());
    const bool clipped = b.getX() < 0 || b.getY() < 0
                      || b.getRight() > parentW || b.getBottom() > parentH;

    std::cout << juce::String::repeatedString ("  ", depth)
              << c.getName() << " [" << typeid (c).name() << "] " << describe (c);

    if (clipped)
        std::cout << "  << OUTSIDE PARENT " << parentW << "x" << parentH << ">>";

    std::cout << "\n";

    for (auto* child : c.getChildren())
        dump (*child, depth + 1, c.getWidth(), c.getHeight());
}
} // namespace

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    juce::StringArray args;

    for (int i = 1; i < argc; ++i)
        args.add (juce::String (argv[i]));

    const bool doDump = args.contains ("--dump");
    args.removeString ("--dump");

    const bool expandPool = args.contains ("--expand-pool");
    args.removeString ("--expand-pool");

    // a row that only names a command argument, under the explicit command the
    // demo already has at row 20: the argument shows under the number the column
    // carries, and the channel that carries nothing shows the explicit 0 the
    // editor writes instead
    const bool cmdCarry = args.contains ("--cmd-carry");
    args.removeString ("--cmd-carry");

    // signed semitone steps to write into arpeggiator id 1, so the Macros tab
    // shot shows a real step strip rather than the default ramp, and a row of
    // the tracker shot that names the call so the letter A and its decimal id
    // are visible
    juce::String arpStepsList;
    for (int i = args.size(); --i >= 0;)
        if (args[i].startsWith ("--arp="))
        {
            arpStepsList = args[i].fromFirstOccurrenceOf ("=", false, false);
            args.remove (i);
        }

    const bool arpLoop = args.contains ("--arp-loop");
    args.removeString ("--arp-loop");

    int arpCallRow = -1;
    for (int i = args.size(); --i >= 0;)
        if (args[i].startsWith ("--arp-call="))
        {
            arpCallRow = args[i].fromFirstOccurrenceOf ("=", false, false).getIntValue();
            args.remove (i);
        }

    int arpCallId = 1; // the macro id the call names, so --arp-call-id=0 makes the
    // shot's call a release: the letter A with the 0 the release writes, drawn OFF
    for (int i = args.size(); --i >= 0;)
        if (args[i].startsWith ("--arp-call-id="))
        {
            arpCallId = args[i].fromFirstOccurrenceOf ("=", false, false).getIntValue();
            args.remove (i);
        }

    int noteOffRow = -1; // a row holding the column's note-off, which is what makes
    // a shot's call sit beside a note-off rather than beside a note or an empty cell
    for (int i = args.size(); --i >= 0;)
        if (args[i].startsWith ("--arp-note-off="))
        {
            noteOffRow = args[i].fromFirstOccurrenceOf ("=", false, false).getIntValue();
            args.remove (i);
        }

    int macrosToDefine = 1; // how many macros to have defined, so a Macros-tab
    // shot can be asked for a list long enough to need the scrollbar it draws
    for (int i = args.size(); --i >= 0;)
        if (args[i].startsWith ("--macros="))
        {
            macrosToDefine = args[i].fromFirstOccurrenceOf ("=", false, false).getIntValue();
            args.remove (i);
        }

    int bendRow = -1;
    for (int i = args.size(); --i >= 0;)
        if (args[i].startsWith ("--bend="))
        {
            bendRow = args[i].fromFirstOccurrenceOf ("=", false, false).getIntValue();
            args.remove (i);
        }

    int bendArg = 96;
    for (int i = args.size(); --i >= 0;)
        if (args[i].startsWith ("--bend-arg="))
        {
            bendArg = args[i].fromFirstOccurrenceOf ("=", false, false).getIntValue();
            args.remove (i);
        }

    int slideRow = -1; // a wheel slide up, so a shot paints the lilac letter U
    for (int i = args.size(); --i >= 0;)
        if (args[i].startsWith ("--slide="))
        {
            slideRow = args[i].fromFirstOccurrenceOf ("=", false, false).getIntValue();
            args.remove (i);
        }

    int slideDownRow = -1; // and a slide down, the letter D, which is a value the
    // number field cannot be typed as and only the letter itself can name
    for (int i = args.size(); --i >= 0;)
        if (args[i].startsWith ("--slide-down="))
        {
            slideDownRow = args[i].fromFirstOccurrenceOf ("=", false, false).getIntValue();
            args.remove (i);
        }

    int slideArg = 12; // how many rows the slide takes to reach its extreme, so
    // the argument beside the letter is drawn in decimal like every other
    // command's, the row above the wheel's centre rather than on it
    for (int i = args.size(); --i >= 0;)
        if (args[i].startsWith ("--slide-arg="))
        {
            slideArg = args[i].fromFirstOccurrenceOf ("=", false, false).getIntValue();
            args.remove (i);
        }

    int shotChannels = 0;
    for (int i = args.size(); --i >= 0;)
        if (args[i].startsWith ("--channels="))
        {
            shotChannels = args[i].fromFirstOccurrenceOf ("=", false, false).getIntValue();
            args.remove (i);
        }

    int shotRows = 0;
    for (int i = args.size(); --i >= 0;)
        if (args[i].startsWith ("--rows="))
        {
            shotRows = args[i].fromFirstOccurrenceOf ("=", false, false).getIntValue();
            args.remove (i);
        }

    // how far along the horizontal scrollbar to jump (percent), so a wide
    // pattern's far channels can be shown in a screenshot
    int shotScroll = -1;
    for (int i = args.size(); --i >= 0;)
        if (args[i].startsWith ("--scroll="))
        {
            shotScroll = args[i].fromFirstOccurrenceOf ("=", false, false).getIntValue();
            args.remove (i);
        }

    // percent along the timeline to click: a click parks the playhead at the
    // clicked beat, so the shot shows where a paste would land
    int shotPlayheadPct = -1;
    for (int i = args.size(); --i >= 0;)
        if (args[i].startsWith ("--click-timeline="))
        {
            shotPlayheadPct = args[i].fromFirstOccurrenceOf ("=", false, false).getIntValue();
            args.remove (i);
        }

    // pool item index whose drag preview is composited into the shot: this is
    // the image JUCE floats at the cursor while a pattern is dragged out of the
    // pool, so the shot shows that only the item's cell follows the pointer
    int shotDragPreview = -1;
    for (int i = args.size(); --i >= 0;)
        if (args[i].startsWith ("--drag-preview="))
        {
            shotDragPreview = args[i].fromFirstOccurrenceOf ("=", false, false).getIntValue();
            args.remove (i);
        }

    juce::String tab;
    for (int i = args.size(); --i >= 0;)
        if (args[i].startsWith ("--tab="))
        {
            tab = args[i].fromFirstOccurrenceOf ("=", false, false).toLowerCase();
            args.remove (i);
        }

    // column visibility for the shot: each pair is a channel digit plus i/v/p/c
    // (instrument, volume, pan, command), so "--hide=0v1c" hides the volume of
    // channel 0 and the command of channel 1
    juce::String shotHide;
    for (int i = args.size(); --i >= 0;)
        if (args[i].startsWith ("--hide="))
        {
            shotHide = args[i].fromFirstOccurrenceOf ("=", false, false);
            args.remove (i);
        }

    // cells to hold selected for the shot: four numbers row0,ch0,row1,ch1
    // (inclusive, both corners in any order), so "--select=6,1,7,1" selects
    // channel 1's rows 6 and 7 and the shot paints those two cells only
    juce::String shotSelect;
    for (int i = args.size(); --i >= 0;)
        if (args[i].startsWith ("--select="))
        {
            shotSelect = args[i].fromFirstOccurrenceOf ("=", false, false);
            args.remove (i);
        }

    // paste anchor: row and channel, so --paste-at=12,0 lands the selection
    // copied by --select= starting at that cell, everything pasted
    juce::String shotPasteAt;
    for (int i = args.size(); --i >= 0;)
        if (args[i].startsWith ("--paste-at="))
        {
            shotPasteAt = args[i].fromFirstOccurrenceOf ("=", false, false);
            args.remove (i);
        }

    // undo/redo steps to walk once the actions above have run, so a shot can
    // show the state an action left behind taken back
    int shotUndo = 0, shotRedo = 0;
    for (int i = args.size(); --i >= 0;)
        if (args[i].startsWith ("--undo="))
        {
            shotUndo = juce::jmax (0, args[i].fromFirstOccurrenceOf ("=", false, false).getIntValue());
            args.remove (i);
        }
    for (int i = args.size(); --i >= 0;)
        if (args[i].startsWith ("--redo="))
        {
            shotRedo = juce::jmax (0, args[i].fromFirstOccurrenceOf ("=", false, false).getIntValue());
            args.remove (i);
        }

    const juce::File out (args.size() > 0 ? args[0] : juce::String ("/tmp/opencode/retrektor.png"));
    const int w = args.size() > 1 ? args[1].getIntValue() : 1240;
    const int h = args.size() > 2 ? args[2].getIntValue() : 760;

    ReTrektorAudioProcessor proc;
    proc.prepareToPlay (48000.0, 512);

    // Give the demo song some content so the tracker and timeline are not empty.
    {
        const juce::ScopedLock sl (proc.getModelLock());
        auto& song = proc.getSong();
        auto& p = song.patterns.front();
        p.setChannels (2);
        p.setLength (64);
        p.name = "Main";

        const int notes[] = { 60, 63, 67, 63, 60, 63, 67, 70 };

        for (int i = 0; i < 8; ++i)
        {
            auto& c = p.cell (i * 2, i % 2);
            c.note = notes[i];
            c.inst = 0;
            c.vol = (juce::uint8) (80 + i * 4);
        }

        p.cell (20, 0).cmd = 74;
        p.cell (20, 0).cmdVal = 64;

        if (cmdCarry)
        {
            p.cell (21, 0).cmdVal = 99; // only an argument: the newest one the column entered
            p.cell (24, 1).cmd = 0;     // the channel 1 has never named: the editor's own 0
            p.cell (24, 1).cmdVal = 12;

            // a call the other channel names, followed by a row naming only its
            // 0: that row answers as the release and leaves the column carrying
            // neither the letter nor an argument for the rows under it
            p.cell (18, 1).cmd = rtr::kArpCmd;
            p.cell (18, 1).cmdVal = 3; // A 3: the call the column carries down
            p.cell (19, 1).cmdVal = 0; // -- 0: the release the carried call answers as
        }

        if (arpCallRow >= 0)
        {
            p.cell (arpCallRow, 0).cmd = rtr::kArpCmd;
            p.cell (arpCallRow, 0).cmdVal = arpCallId; // A 1: the letter command and its macro id
        }

        if (noteOffRow >= 0)
            p.cell (noteOffRow, 0).note = rtr::kNoteOff; // === : the note the column held is off

        if (bendRow >= 0)
        {
            p.cell (bendRow, 0).cmd = rtr::kBendCmd;
            p.cell (bendRow, 0).cmdVal = bendArg; // B 96: the lilac letter and the point on the wheel
        }

        if (slideRow >= 0)
        {
            p.cell (slideRow, 0).cmd = rtr::kSlideUpCmd;
            p.cell (slideRow, 0).cmdVal = slideArg; // U 12: the lilac letter and the span to its top
        }

        if (slideDownRow >= 0)
        {
            p.cell (slideDownRow, 0).cmd = rtr::kSlideDownCmd;
            p.cell (slideDownRow, 0).cmdVal = slideArg; // D 12: and the span down to its bottom
        }

        if (arpStepsList.isNotEmpty())
        {
            // the id's own steps, so a Macros-tab shot shows a strip of the
            // length the list names: a step written as a lone dot stays empty,
            // and the flag says nothing about the loop unless asked
            song.defineArp (1);
            song.setArpLooped (1, arpLoop);

            const juce::StringArray steps = juce::StringArray::fromTokens (arpStepsList, ",", "");

            song.setArpLength (1, (int) steps.size());

            for (int i = 0; i < steps.size(); ++i)
            {
                const juce::String token = steps[i].removeCharacters ("+");

                int value = rtr::kArpEmptyStep;

                if (token.equalsIgnoreCase ("off") || token == "`")
                    // a step that opens whatever the run was holding there: the
                    // note column's own note-off, written by its backtick
                    value = rtr::kArpNoteOffStep;
                else if (! token.isEmpty() && token != ".")
                    value = token.getIntValue(); // the setter keeps it inside the jump

                song.setArpStep (1, i, value);
            }
        }

        if (macrosToDefine > 1)
        {
            // the ids a row can name, all of them fresh and holding steps that
            // sound nothing, which is what makes the list long enough that the
            // cards do not fit and the tab has to draw its scrollbar and thumb
            for (int id = 2; id <= macrosToDefine; ++id)
                song.defineArp (id);
        }

        p.cell (20, 1).note = 48;
        p.cell (20, 1).inst = 1;

        auto& q = song.addPattern ("Fill", 16, 2);
        q.cell (0, 0).note = 72;
        song.addClip (q.id, 4.0);
    }

    std::unique_ptr<juce::AudioProcessorEditor> editor (proc.createEditor());

    if (editor == nullptr)
    {
        std::cerr << "no editor\n";
        return 1;
    }

    editor->setSize (w, h);
    editor->setVisible (true);

    if (tab.isNotEmpty())
    {
        for (auto* child : editor->getChildren())
            if (auto* button = dynamic_cast<juce::TextButton*> (child))
                if (button->getButtonText().toLowerCase() == tab && button->onClick != nullptr)
                    button->onClick();
    }

    if (expandPool)
    {
        std::function<rtr::PoolComponent* (juce::Component&)> findPool;
        findPool = [&findPool] (juce::Component& c) -> rtr::PoolComponent*
        {
            if (auto* pool = dynamic_cast<rtr::PoolComponent*> (&c))
                return pool;

            for (auto* child : c.getChildren())
                if (auto* found = findPool (*child))
                    return found;

            return nullptr;
        };

        if (auto* pool = findPool (*editor))
        {
            const juce::ScopedLock sl (proc.getModelLock());

            if (! proc.getSong().patterns.empty())
            {
                pool->toggleGridPreview (proc.getSong().patterns.front().id);
                pool->refresh();
            }
        }
    }

    if (shotChannels > 1)
    {
        const juce::ScopedLock sl (proc.getModelLock());

        if (auto* p = proc.getSong().activePattern())
        {
            p->setChannels (shotChannels);

            for (int ch = 1; ch < shotChannels; ++ch)
            {
                // keep the notes inside the rows a plain shot can see, so
                // scrolling right reveals whole columns of them
                auto& cell = p->cell (juce::jmin (ch * 3, 23), ch);
                cell.note = 57 + ch;
                cell.inst = ch % 16;
                cell.vol = (juce::uint8) juce::jlimit (1, 127, 90 + ch);
            }

            p->setChannels (juce::jmax (1, shotChannels / 2)); // narrow ...
            p->setChannels (shotChannels);                     // ... widen back: columns restored
        }
    }

    if (shotHide.isNotEmpty())
    {
        const juce::ScopedLock sl (proc.getModelLock());

        if (auto* p = proc.getSong().activePattern())
        {
            for (int i = 0; i + 1 < shotHide.length(); ++i)
            {
                const int ch = shotHide[i] - '0';

                if (ch < 0 || ch >= p->channels)
                    continue;

                int field = -1;
                const juce::juce_wchar letter = shotHide[i + 1];

                if (letter == 'i') field = 0;
                else if (letter == 'v') field = 1;
                else if (letter == 'p') field = 2;
                else if (letter == 'c') field = 3;
                else
                    continue;

                p->setColumn (ch, field, false);
                ++i;
            }
        }
    }

    if (shotRows > 1)
    {
        const juce::ScopedLock sl (proc.getModelLock());

        if (auto* p = proc.getSong().activePattern())
        {
            p->setLength (shotRows / 2); // shortened first ...
            p->setLength (shotRows);     // then grown back: hidden rows come back
        }
    }

    if (shotScroll >= 0)
    {
        // simulate a click on the tracker's bottom scrollbar strip: the thumb
        // jumps to the click, so the shot shows the far channel columns
        if (auto* tr = findChild<rtr::TrackerComponent> (*editor))
        {
            tr->refresh(); // lay the columns out first, like a painted frame would

            const int tw = tr->getWidth();
            const int usable = juce::jmax (1, tw - 70);
            const float x = (float) (tw - 8 - (int) ((double) usable * (100 - shotScroll) / 100.0));

            tr->mouseDown (makeEvent (*tr, { x, (float) (tr->getHeight() - 7) }));
            tr->mouseUp (makeEvent (*tr, { x, (float) (tr->getHeight() - 7) }));
        }
        else
        {
            std::cerr << "no tracker child for --scroll\n";
        }
    }

    if (! shotSelect.isEmpty())
    {
        // the selection goes in through the same path a Shift-drag takes, so the
        // shot shows the per-cell tint instead of a band across the pattern
        if (auto* tr = findChild<rtr::TrackerComponent> (*editor))
        {
            const auto parts = juce::StringArray::fromTokens (shotSelect, ",", {});
            int v[4] = { 0, 0, 0, 0 };

            for (int i = 0; i < 4; ++i)
                v[i] = i < parts.size() ? parts[i].getIntValue() : 0;

            tr->refresh(); // the selection reads the pattern the tracker shows
            tr->addCellsToSelection (v[0], v[1], v[2], v[3]);

            std::cout << "selection over cells (" << v[0] << "," << v[1] << ")..("
                      << v[2] << "," << v[3] << ")\n";
        }
        else
        {
            std::cerr << "no tracker child for --select\n";
        }
    }

    if (! shotPasteAt.isEmpty())
    {
        // the selection copied by --select= goes to the clipboard through the
        // Ctrl+C path and then lands at the clicked cursor with everything, so
        // the shot shows the pasted block where the tools write it
        if (auto* tr = findChild<rtr::TrackerComponent> (*editor))
        {
            tr->refresh();
            tr->copySelectionToClipboard();

            if (! tr->hasCellClipboard())
            {
                std::cerr << "cell clipboard empty for --paste-at: pair it with --select=\n";
            }
            else
            {
                const auto parts = juce::StringArray::fromTokens (shotPasteAt, ",", {});
                const int row = parts.size() > 0 ? parts[0].getIntValue() : 0;
                const int ch = parts.size() > 1 ? parts[1].getIntValue() : 0;

                tr->refresh(); // the anchor click reads the channel columns
                const float x = (float) tr->channelBoundsFor (ch).getCentreX();
                const float y = 46.0f + 19.0f * (float) row + 9.5f; // header plus half a row

                tr->mouseDown (makeEvent (*tr, { x, y }));
                tr->mouseUp (makeEvent (*tr, { x, y }));

                std::cout << "paste wrote " << tr->pasteClipboardAtCursor (rtr::kPasteAll)
                          << " cells at (" << row << "," << ch << ")\n";
                tr->repaint();
            }
        }
        else
        {
            std::cerr << "no tracker child for --paste-at\n";
        }
    }

    if (shotUndo > 0 || shotRedo > 0)
    {
        // the same walk the Ctrl+Z / Ctrl+Y keys and the tracker's menu make:
        // the editor steps the plug-in's line and refreshes every panel, so the
        // shot below is drawn from the state the walk landed on
        auto* view = dynamic_cast<ReTrektorAudioProcessorEditor*> (editor.get());

        if (view == nullptr)
        {
            std::cerr << "no editor for --undo/--redo\n";
        }
        else
        {
            int walked = 0;

            for (int i = 0; i < shotUndo; ++i)
            {
                if (! view->performUndo())
                    break;

                ++walked;
            }

            for (int i = 0; i < shotRedo; ++i)
            {
                if (! view->performRedo())
                    break;

                ++walked;
            }

            std::cout << "undo/redo walked " << walked << " of " << (shotUndo + shotRedo)
                      << " steps, undo depth " << proc.getUndoDepth()
                      << " redo " << proc.getUndoRedoCount() << "\n";
        }
    }

    if (shotPlayheadPct >= 0)
    {
        // a click on the timeline parks the playhead at the clicked beat, so the
        // shot shows where a paste would land
        if (auto* tl = findChild<rtr::TimelineComponent> (*editor))
        {
            tl->refresh();

            const int tlw = juce::jmax (40, tl->getWidth());
            const int x = juce::jlimit (2, tlw - 3, (int) ((double) (tlw - 12) * shotPlayheadPct / 100.0) + 6);
            const int y = tl->getHeight() / 3;

            tl->mouseDown (makeEvent (*tl, { (float) x, (float) y }));
            tl->mouseUp (makeEvent (*tl, { (float) x, (float) y }));

            std::cout << "timeline click at x=" << x << "/" << (double) tlw
                      << " parked the playhead at beat " << proc.getSong().editPlayheadBeats
                      << " (ppb " << tl->getPixelsPerBeat() << ")\n";
        }
        else
        {
            std::cerr << "no timeline child for --click-timeline\n";
        }
    }

    if (doDump)
        dump (*editor, 0, w, h);

    auto image = editor->createComponentSnapshot (editor->getLocalBounds());

    if (! image.isValid())
    {
        std::cerr << "snapshot failed\n";
        return 1;
    }

    if (shotDragPreview >= 0)
    {
        // the image JUCE floats at the cursor while a pattern is dragged out of
        // the pool: only the item's cell, so the shot can be checked for its
        // size (one row of the pool, never the whole panel)
        if (auto* pl = findChild<rtr::PoolComponent> (*editor))
        {
            const auto cell = pl->dragCellBounds (shotDragPreview);
            const auto preview = pl->dragPreviewImage (shotDragPreview);

            if (cell.getWidth() > 0 && preview.getImage().getWidth() > 0)
            {
                const auto editorPos = editor->getBounds().getPosition();
                const auto poolPos = pl->getBounds().getPosition() - editorPos;

                // hover it just right of the pool, mid height, as a drag would
                const int cx = juce::jlimit (0, juce::jmax (0, w - cell.getWidth()),
                                             poolPos.x + pl->getWidth() + 12);
                const int cy = juce::jlimit (0, juce::jmax (0, h - cell.getHeight()),
                                             poolPos.y + pl->getHeight() / 2 - cell.getHeight() / 2);

                juce::Graphics g (image);
                g.drawImage (preview.getImage(), cx, cy, cell.getWidth(), cell.getHeight(),
                             0, 0, preview.getImage().getWidth(), preview.getImage().getHeight());

                std::cout << "drag preview of item " << shotDragPreview << ": cell "
                          << cell.getWidth() << "x" << cell.getHeight()
                          << ", image " << preview.getImage().getWidth() << "x"
                          << preview.getImage().getHeight() << " (scale " << preview.getScale()
                          << "), composited at " << cx << "," << cy << "\n";
            }
            else
            {
                std::cerr << "no drag preview for item " << shotDragPreview << "\n";
            }
        }
        else
        {
            std::cerr << "no pool child for --drag-preview\n";
        }
    }

    out.getParentDirectory().createDirectory();
    out.deleteFile();
    auto stream = out.createOutputStream();

    if (stream == nullptr)
    {
        std::cerr << "cannot write " << out.getFullPathName() << "\n";
        return 1;
    }

    juce::PNGImageFormat png;
    png.writeImageToStream (image, *stream);
    std::cout << "wrote " << out.getFullPathName() << "\n";
    return 0;
}
