#include "MacrosPanel.h"
#include "Theme.h"

namespace rtr
{

MacrosPanel::MacrosPanel (ReTtrektorAudioProcessor& processor)
    : proc (processor), song (processor.getSong())
{
    setOpaque (true);

    addButton.onClick = [this]
    {
        if ((int) song.macros.size() < kMaxRows)
        {
            song.macros.push_back ({ "Macro " + juce::String ((int) song.macros.size() + 1), 0, 7, 4 });
            rebuild();

            if (onModelChanged)
                onModelChanged();
        }
    };

    removeButton.onClick = [this]
    {
        if (! song.macros.empty())
        {
            song.macros.pop_back();
            rebuild();

            if (onModelChanged)
                onModelChanged();
        }
    };

    for (auto* b : { &addButton, &removeButton })
    {
        b->setColour (juce::TextButton::buttonColourId, theme::panelLight);
        b->setColour (juce::TextButton::textColourOffId, theme::text);
        addAndMakeVisible (*b);
    }

    rebuild();
}

//==============================================================================
void MacrosPanel::rebuild()
{
    for (int i = 0; i < kMaxRows; ++i)
    {
        auto& r = controls[(size_t) i];
        const bool used = i < (int) song.macros.size();

        if (used && r.name == nullptr)
        {
            r.name = std::make_unique<juce::TextEditor>();
            r.name->setFont (theme::monoFont (12.0f));
            r.name->setColour (juce::TextEditor::backgroundColourId, theme::panelLight);
            r.name->setColour (juce::TextEditor::textColourId, theme::text);
            r.name->setColour (juce::TextEditor::outlineColourId, theme::outline);
            addAndMakeVisible (*r.name);

            r.kind = std::make_unique<juce::ComboBox>();
            r.kind->addItem ("Arpeggiator", 1);
            r.kind->addItem ("Pitch glide", 2);
            r.kind->addItem ("Slide up/down", 3);
            r.kind->setColour (juce::ComboBox::backgroundColourId, theme::panelLight);
            r.kind->setColour (juce::ComboBox::textColourId, theme::text);
            r.kind->setColour (juce::ComboBox::outlineColourId, theme::outline);
            r.kind->setColour (juce::ComboBox::arrowColourId, theme::textDim);
            addAndMakeVisible (*r.kind);

            r.p1 = std::make_unique<juce::Slider> (juce::Slider::IncDecButtons, juce::Slider::TextBoxLeft);
            r.p1->setRange (0.0, 127.0, 1.0);
            r.p1->setTextBoxStyle (juce::Slider::TextBoxLeft, false, 44, 18);
            r.p1->setColour (juce::Slider::textBoxTextColourId, theme::text);
            r.p1->setColour (juce::Slider::textBoxBackgroundColourId, theme::panelLight);
            r.p1->setColour (juce::Slider::textBoxOutlineColourId, theme::outline);
            addAndMakeVisible (*r.p1);

            r.p2 = std::make_unique<juce::Slider> (juce::Slider::IncDecButtons, juce::Slider::TextBoxLeft);
            r.p2->setRange (0.0, 127.0, 1.0);
            r.p2->setTextBoxStyle (juce::Slider::TextBoxLeft, false, 44, 18);
            r.p2->setColour (juce::Slider::textBoxTextColourId, theme::text);
            r.p2->setColour (juce::Slider::textBoxBackgroundColourId, theme::panelLight);
            r.p2->setColour (juce::Slider::textBoxOutlineColourId, theme::outline);
            addAndMakeVisible (*r.p2);

            const int index = i;

            r.name->onTextChange = [this, index]
            {
                if (index < (int) song.macros.size())
                    song.macros[(size_t) index].name = controls[(size_t) index].name->getText();
            };

            r.kind->onChange = [this, index]
            {
                if (index < (int) song.macros.size())
                    song.macros[(size_t) index].kind = controls[(size_t) index].kind->getSelectedId() - 1;
            };

            r.p1->onValueChange = [this, index]
            {
                if (index < (int) song.macros.size())
                    song.macros[(size_t) index].param1 = (int) controls[(size_t) index].p1->getValue();
            };

            r.p2->onValueChange = [this, index]
            {
                if (index < (int) song.macros.size())
                    song.macros[(size_t) index].param2 = (int) controls[(size_t) index].p2->getValue();
            };
        }

        if (r.name != nullptr)
        {
            r.name->setVisible (used);
            r.kind->setVisible (used);
            r.p1->setVisible (used);
            r.p2->setVisible (used);
        }
    }

    syncValues();
    resized();
    repaint();
}

void MacrosPanel::syncValues()
{
    for (int i = 0; i < (int) song.macros.size() && i < kMaxRows; ++i)
    {
        auto& m = song.macros[(size_t) i];
        auto& r = controls[(size_t) i];

        if (r.name == nullptr)
            continue;

        r.name->setText (m.name, false);
        r.kind->setSelectedId (juce::jlimit (1, 3, m.kind + 1), juce::dontSendNotification);
        r.p1->setValue ((double) m.param1, juce::dontSendNotification);
        r.p2->setValue ((double) m.param2, juce::dontSendNotification);
    }
}

void MacrosPanel::refresh()
{
    rebuild();
}

//==============================================================================
void MacrosPanel::paint (juce::Graphics& g)
{
    g.fillAll (theme::background);

    g.setColour (theme::text);
    g.setFont (theme::uiFont (15.0f, true));
    g.drawText ("Command macros", 20, 12, 400, 22, juce::Justification::centredLeft, false);

    g.setColour (theme::textDim);
    g.setFont (theme::monoFont (10.5f));
    g.drawText ("Macro definitions are stored with the song. Playback integration is not enabled yet.", 20, 34, getWidth() - 40, 14,
                juce::Justification::centredLeft, false);

    g.setFont (theme::monoFont (10.0f));
    g.drawText ("NAME", 20, 58, 200, 12, juce::Justification::centredLeft, false);
    g.drawText ("TYPE", 250, 58, 120, 12, juce::Justification::centredLeft, false);
    g.drawText ("PARAM 1", 380, 58, 70, 12, juce::Justification::centredLeft, false);
    g.drawText ("PARAM 2", 460, 58, 70, 12, juce::Justification::centredLeft, false);

    const int step = rowStep();

    for (int i = 0; i < (int) song.macros.size() && i < kMaxRows; ++i)
    {
        const int y = 76 + i * step;

        if (y + 28 > getHeight() - 40)
            break;

        g.setColour (i % 2 == 0 ? theme::panel.withAlpha (0.5f) : theme::background);
        g.fillRect (16, y - 3, getWidth() - 32, 26);
    }
}

int MacrosPanel::rowStep() const
{
    const int count = juce::jmax (1, (int) song.macros.size());
    const int available = juce::jmax (0, getHeight() - 116);
    return juce::jlimit (22, 28, available / count);
}

void MacrosPanel::resized()
{
    addButton.setBounds (20, getHeight() - 34, 110, 22);
    removeButton.setBounds (138, getHeight() - 34, 120, 22);

    const int step = rowStep();
    const int bottomLimit = getHeight() - 38;

    for (int i = 0; i < kMaxRows; ++i)
    {
        auto& r = controls[(size_t) i];

        if (r.name == nullptr)
            continue;

        const bool used = i < (int) song.macros.size();
        const int y = 74 + i * step;
        const bool visible = used && y + 23 <= bottomLimit;

        r.name->setVisible (visible);
        r.kind->setVisible (visible);
        r.p1->setVisible (visible);
        r.p2->setVisible (visible);

        if (visible)
        {
            r.name->setBounds (20, y, 222, 22);
            r.kind->setBounds (250, y, 120, 22);
            r.p1->setBounds (380, y, 70, 22);
            r.p2->setBounds (460, y, 70, 22);
        }
    }

    addButton.setEnabled ((int) song.macros.size() < kMaxRows);
}

} // namespace rtr
