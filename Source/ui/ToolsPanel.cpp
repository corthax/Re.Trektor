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

    auto styleCombo = [] (juce::ComboBox& box)
    {
        box.setColour (juce::ComboBox::backgroundColourId, theme::panelLight);
        box.setColour (juce::ComboBox::textColourId, theme::text);
        box.setColour (juce::ComboBox::outlineColourId, theme::outline);
        box.setColour (juce::ComboBox::arrowColourId, theme::textDim);
    };

    // dropdown items are labelled like the tracker's instrument column: -0 .. -F
    auto addInstruments = [] (juce::ComboBox& box, int idOffset)
    {
        for (int i = 0; i < kMidiChannelCount; ++i)
            box.addItem ("-" + juce::String::toHexString (i).toUpperCase(), i + 1 + idOffset);
    };

    scopeBox.addItem ("Selection only", 1);
    scopeBox.addItem ("Whole pattern", 2);
    scopeBox.addItem ("All patterns in song", 3);
    scopeBox.addItem ("All created patterns", 4);
    scopeBox.setSelectedId (2);
    styleCombo (scopeBox);
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

    remapFromBox.addItem ("All", 1); // a source that matches every instrument
    addInstruments (remapFromBox, 1); // ids 2..17 are the -0 .. -F instruments
    remapFromBox.setSelectedId (1); // All
    styleCombo (remapFromBox);
    addAndMakeVisible (remapFromBox);

    addInstruments (remapToBox, 0); // ids 1..16 are the -0 .. -F instruments
    remapToBox.setSelectedId (1); // -0
    styleCombo (remapToBox);
    addAndMakeVisible (remapToBox);

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
    section ("REMAP", remapFromBox);
    section ("INTERPOLATE", interpolateButton);

    // an arrow between the two remap dropdowns names the direction of the edit
    if (remapFromBox.getWidth() > 0 && remapToBox.getWidth() > 0)
    {
        const auto fromArea = remapFromBox.getBounds();
        const auto toArea = remapToBox.getBounds();
        const int x0 = fromArea.getRight() + 3;
        const int x1 = toArea.getX() - 3;

        if (x1 > x0 + 8)
        {
            const float y = (float) ((fromArea.getCentreY() + toArea.getCentreY()) / 2);
            const float head = juce::jmin (5.0f, (float) (x1 - x0) * 0.4f);
            const float tail = (float) x0;
            const float tip = (float) x1;

            g.setColour (theme::textDim);
            g.drawLine (juce::Line<float> (tail, y, tip, y), 1.5f);
            g.drawLine (juce::Line<float> (tip - head, y - head, tip, y), 1.5f);
            g.drawLine (juce::Line<float> (tip - head, y + head, tip, y), 1.5f);
        }
    }
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
    remapFromBox.setBounds (remapRow.removeFromLeft (62).reduced (1));
    remapToBox.setBounds (remapRow.removeFromRight (62).reduced (1));
    area.removeFromTop (4);
    remapApply.setBounds (area.removeFromTop (22).reduced (1));

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

std::pair<int, int> ToolsPanel::rowRange (const Pattern& p) const
{
    if (scopeBox.getSelectedId() != 1)
        return { 0, p.length - 1 };

    if (! getSelection)
        return { -1, -1 };

    auto sel = getSelection();

    if (sel.first < 0 || sel.second < 0)
        return { -1, -1 };

    const int start = juce::jmax (0, juce::jmin (sel.first, sel.second));
    const int end = juce::jmin (p.length - 1, juce::jmax (sel.first, sel.second));

    return { start, end };
}

void ToolsPanel::applyToRows (Pattern& p, const std::function<void (Cell&, int row)>& fn)
{
    const auto range = rowRange (p);
    const int start = range.first;
    const int end = range.second;

    if (start < 0 || start > end)
        return;

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
    const int fromId = remapFromBox.getSelectedId();
    const int toId = remapToBox.getSelectedId();

    if (fromId <= 0 || toId <= 0)
        return;

    const bool all = fromId <= 1; // "All" is the first item in the source box
    const int from = fromId - 2; // the source box lists All, then -0 .. -F
    const int to = toId - 1; // the target box lists only -0 .. -F

    for (auto* p : scopedPatterns())
    {
        const auto range = rowRange (*p);

        if (range.first < 0 || range.first > range.second)
            continue;

        for (int ch = 0; ch < p->channels; ++ch)
        {
            // the instrument the column resolves to at this row, before and
            // after the remap: an explicit cell sets it, an empty one carries
            // the previous value (which the tracker draws darker)
            int before = kEmpty;
            int after = kEmpty;

            for (int row = 0; row <= range.second; ++row)
            {
                auto& c = p->rows[(size_t) row][(size_t) ch];

                if (c.inst != kEmpty)
                {
                    const int current = c.inst;

                    if (row >= range.first && (all || current == from))
                        c.inst = to;

                    before = current;
                    after = c.inst;
                    continue;
                }

                if (row < range.first)
                    continue;

                const bool matches = all ? before != kEmpty : before == from;

                if (! matches)
                    continue;

                // an implicit cell already resolves to the new instrument when
                // the cell above it was remapped, so only the cells that would
                // still carry the old instrument get an explicit value
                if (after != to)
                {
                    c.inst = to;
                    after = to;
                }
            }
        }
    }

    if (onModelChanged)
        onModelChanged();
}

void ToolsPanel::applyInterpolate()
{
    for (auto* p : scopedPatterns())
    {
        const auto range = rowRange (*p);
        const int start = range.first;
        const int end = range.second;

        if (start < 0 || end - start < 2)
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
