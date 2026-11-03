/*
    Renders the Re.Trektor editor to a PNG without a host, and optionally dumps
    the component tree so the layout can be verified without looking at pixels.

    Usage: ReTrektorEditorShot /path/out.png [width height] [--dump]
           [tab=settings] [--channels=N] [--rows=N] [--scroll=PCT]
           [--click-timeline=PCT] [--hide=0v1c] [--expand-pool] [--hex]

    --hide= takes channel/letter pairs (i instrument, v volume, p pan, c
    command), so --hide=0v1c hides the volume of channel 0 and the command of
    channel 1: the chips above a channel hide one field of that channel only.
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

    // show values in hex, so the narrower hex field widths can be checked
    const bool shotHex = args.contains ("--hex");
    args.removeString ("--hex");

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
        song.settings.hexDisplay = shotHex;

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
