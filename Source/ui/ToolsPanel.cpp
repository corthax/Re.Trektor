#include "ToolsPanel.h"
#include "Theme.h"

namespace rtr
{

ToolsPanel::ToolsPanel (ReTrektorAudioProcessor& processor)
    : proc (processor), song (processor.getSong())
{
    setOpaque (true);

    auto styleButton = [] (juce::TextButton& b)
    {
        b.setColour (juce::TextButton::buttonColourId, theme::panelLight);
        b.setColour (juce::TextButton::textColourOnId, theme::text);
        b.setColour (juce::TextButton::textColourOffId, theme::text);
    };

    scopeBox.addItem ("Selection only", 1);
    scopeBox.addItem ("Whole pattern", 2);
    scopeBox.addItem ("All patterns in song", 3);
    scopeBox.addItem ("All created patterns", 4);
    scopeBox.setSelectedId (2);
    scopeBox.setColour (juce::ComboBox::backgroundColourId, theme::panelLight);
    scopeBox.setColour (juce::ComboBox::textColourId, theme::text);
    scopeBox.setColour (juce::ComboBox::outlineColourId, theme::outline);
    scopeBox.setColour (juce::ComboBox::arrowColourId, theme::textDim);
    addAndMakeVisible (scopeBox);

    for (auto* b : { &transposeDown12, &transposeDown1, &transposeUp1, &transposeUp12 })
    {
        styleButton (*b);
        addAndMakeVisible (*b);
    }

    transposeDown12.onClick = [this] { applyTranspose (-12); };
    transposeDown1.onClick = [this] { applyTranspose (-1); };
    transposeUp1.onClick = [this] { applyTranspose (1); };
    transposeUp12.onClick = [this] { applyTranspose (12); };

    remapBox.addItem ("-0", 1);

    for (int i = 1; i <= 15; ++i)
        remapBox.addItem ("-" + juce::String::toHexString (i).toUpperCase(), i + 1);

    remapBox.setSelectedId (1);
    remapBox.setColour (juce::ComboBox::backgroundColourId, theme::panelLight);
    remapBox.setColour (juce::ComboBox::textColourId, theme::text);
    remapBox.setColour (juce::ComboBox::outlineColourId, theme::outline);
    remapBox.setColour (juce::ComboBox::arrowColourId, theme::textDim);
    addAndMakeVisible (remapBox);

    styleButton (remapApply);
    styleButton (interpolateButton);
    addAndMakeVisible (remapApply);
    addAndMakeVisible (interpolateButton);

    remapApply.onClick = [this] { applyRemap(); };
    interpolateButton.onClick = [this] { applyInterpolate(); };
}

//==============================================================================
void ToolsPanel::paint (juce::Graphics& g)
{
    g.fillAll (theme::panel);
    g.setColour (theme::outline);
    g.drawVerticalLine (0, 0.0f, (float) getHeight());

    g.setColour (theme::textDim);
    g.setFont (theme::monoFont (11.0f));
    g.drawText ("TOOLS", 10, 8, getWidth() - 20, 14, juce::Justification::centredLeft, false);

    auto section = [&] (const juce::String& text, const juce::Component& control)
    {
        g.drawText (text, 10, control.getY() - 14, getWidth() - 20, 12,
                    juce::Justification::centredLeft, false);
    };

    section ("SCOPE", scopeBox);
    section ("TRANSPOSE", transposeDown12);
    section ("REMAP INST", remapBox);
    section ("INTERPOLATE", interpolateButton);
}

void ToolsPanel::resized()
{
    auto area = getLocalBounds().reduced (8);
    area.removeFromTop (18); // TOOLS heading

    area.removeFromTop (12); // SCOPE label
    scopeBox.setBounds (area.removeFromTop (22));

    area.removeFromTop (14);
    area.removeFromTop (12); // TRANSPOSE label
    auto row1 = area.removeFromTop (22);
    transposeDown12.setBounds (row1.removeFromLeft (row1.getWidth() / 2).reduced (1));
    transposeDown1.setBounds (row1.reduced (1));
    area.removeFromTop (2);
    auto row2 = area.removeFromTop (22);
    transposeUp1.setBounds (row2.removeFromLeft (row2.getWidth() / 2).reduced (1));
    transposeUp12.setBounds (row2.reduced (1));

    area.removeFromTop (14);
    area.removeFromTop (12); // REMAP label
    auto remapRow = area.removeFromTop (22);
    remapApply.setBounds (remapRow.removeFromRight (58).reduced (1));
    remapBox.setBounds (remapRow.reduced (1));

    area.removeFromTop (14);
    area.removeFromTop (12); // INTERPOLATE label
    interpolateButton.setBounds (area.removeFromTop (22));
}

//==============================================================================
std::vector<Pattern*> ToolsPanel::scopedPatterns()
{
    ToolScope scope = ToolScope::pattern;

    switch (scopeBox.getSelectedId())
    {
        case 1: scope = ToolScope::selection; break;
        case 2: scope = ToolScope::pattern; break;
        case 3: scope = ToolScope::songPatterns; break;
        case 4: scope = ToolScope::allPatterns; break;
        default: break;
    }

    auto patterns = song.patternsForScope (scope, song.activePatternId, {});

    if (scope == ToolScope::selection)
    {
        // Selection scope only ever edits the active pattern, and only when
        // there is an actual row selection.
        patterns.clear();

        if (getSelection)
        {
            auto sel = getSelection();

            if (sel.first >= 0 && sel.second >= 0)
                if (auto* p = song.findPattern (song.activePatternId))
                    patterns.push_back (p);
        }
    }

    return patterns;
}

void ToolsPanel::applyToRows (Pattern& p, const std::function<void (Cell&, int row)>& fn)
{
    int start = 0;
    int end = p.length - 1;

    if (scopeBox.getSelectedId() == 1)
    {
        if (! getSelection)
            return;

        auto sel = getSelection();

        if (sel.first < 0 || sel.second < 0)
            return;

        start = juce::jmax (0, juce::jmin (sel.first, sel.second));
        end = juce::jmin (p.length - 1, juce::jmax (sel.first, sel.second));
    }

    for (int row = start; row <= end; ++row)
        for (int ch = 0; ch < p.channels; ++ch)
            fn (p.rows[(size_t) row][(size_t) ch], row);
}

void ToolsPanel::applyTranspose (int semitones)
{
    for (auto* p : scopedPatterns())
    {
        applyToRows (*p, [semitones] (Cell& c, int)
        {
            if (c.note >= 0 && c.note <= 127)
                c.note = juce::jlimit (0, 127, c.note + semitones);
        });
    }

    if (onModelChanged)
        onModelChanged();
}

void ToolsPanel::applyRemap()
{
    const int inst = juce::jlimit (0, 15, remapBox.getSelectedId() - 1);

    for (auto* p : scopedPatterns())
    {
        applyToRows (*p, [inst] (Cell& c, int)
        {
            if (c.note != kEmpty)
                c.inst = inst;
        });
    }

    if (onModelChanged)
        onModelChanged();
}

void ToolsPanel::applyInterpolate()
{
    for (auto* p : scopedPatterns())
    {
        int start = 0;
        int end = p->length - 1;

        if (scopeBox.getSelectedId() == 1)
        {
            if (! getSelection)
                continue;

            auto sel = getSelection();

            if (sel.first < 0 || sel.second < 0)
                continue;

            start = juce::jmax (0, juce::jmin (sel.first, sel.second));
            end = juce::jmin (p->length - 1, juce::jmax (sel.first, sel.second));
        }

        if (end - start < 2)
            continue;

        for (int ch = 0; ch < p->channels; ++ch)
        {
            const int firstRow = start;
            const int lastRow = end;

            const int v0 = p->rows[(size_t) firstRow][(size_t) ch].vol;
            const int v1 = p->rows[(size_t) lastRow][(size_t) ch].vol;

            if (v0 != kEmpty && v1 != kEmpty)
            {
                for (int r = firstRow + 1; r < lastRow; ++r)
                {
                    const double t = (double) (r - firstRow) / (double) (lastRow - firstRow);
                    p->rows[(size_t) r][(size_t) ch].vol = (int) std::lround (v0 + (v1 - v0) * t);
                }
            }

            const int p0 = p->rows[(size_t) firstRow][(size_t) ch].pan;
            const int p1 = p->rows[(size_t) lastRow][(size_t) ch].pan;

            if (p0 != kEmpty && p1 != kEmpty)
            {
                for (int r = firstRow + 1; r < lastRow; ++r)
                {
                    const double t = (double) (r - firstRow) / (double) (lastRow - firstRow);
                    p->rows[(size_t) r][(size_t) ch].pan = (int) std::lround (p0 + (p1 - p0) * t);
                }
            }
        }
    }

    if (onModelChanged)
        onModelChanged();
}

} // namespace rtr
