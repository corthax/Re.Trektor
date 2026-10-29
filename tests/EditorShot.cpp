/*
    Renders the Re.Trektor editor to a PNG without a host, and optionally dumps
    the component tree so the layout can be verified without looking at pixels.

    Usage: ReTtrektorEditorShot /path/out.png [width height] [--dump]
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

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

    juce::String tab;
    for (int i = args.size(); --i >= 0;)
        if (args[i].startsWith ("--tab="))
        {
            tab = args[i].fromFirstOccurrenceOf ("=", false, false).toLowerCase();
            args.remove (i);
        }

    const juce::File out (args.size() > 0 ? args[0] : juce::String ("/tmp/opencode/retrektor.png"));
    const int w = args.size() > 1 ? args[1].getIntValue() : 1240;
    const int h = args.size() > 2 ? args[2].getIntValue() : 760;

    ReTtrektorAudioProcessor proc;
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
