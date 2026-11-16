#include "ToolsPanel.h"
#include "Theme.h"
#include "AltShortcutKeys.h"

namespace rtr
{

ToolsPanel::ToolsPanel (ReTrektorAudioProcessor& processor)
    : proc (processor), song (processor.getSong())
{
    setOpaque (true);

    auto styleButton = [] (juce::TextButton& b)
    {
        // the same reason as the tabs' and the checkboxes': these answer the
        // Return key while they hold the keyboard focus, so an Enter meant to
        // confirm a typed value would otherwise transpose or remap again. The
        // panel keeps no focus of its own, since it stays on screen on every tab
        // and focus resting here would leave the tracker deaf
        b.setWantsKeyboardFocus (false);

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

    // dropdown items are labelled like the tracker's instrument column: -0 .. -15,
    // the decimal spelling the column shows them in now the hex view is gone
    auto addInstruments = [] (juce::ComboBox& box, int idOffset)
    {
        for (int i = 0; i < kMidiChannelCount; ++i)
            box.addItem ("-" + juce::String (i), i + 1 + idOffset);
    };

    scopeBox.addItem ("Selection only", 1);
    scopeBox.addItem ("Whole pattern", 2);
    scopeBox.addItem ("All patterns in song", 3);
    scopeBox.addItem ("All created patterns", 4);
    // the panel opens on the one scope that acts on what the user last selected,
    // which is now a set of cells rather than a band of whole rows: transpose and
    // remap then need the dropdown moved to a wider scope when nothing is
    // selected, and the wider scopes ignore the selection entirely
    scopeBox.setSelectedId (1); // "Selection only"
    styleCombo (scopeBox);
    addAndMakeVisible (scopeBox);

    // the amount is an input field in semitones that both direction buttons read,
    // so the panel offers one value and one click instead of four fixed steps:
    // the drag buttons step by a single semitone, and typing into the text box
    // sets the amount directly. The default is an octave, the commonest move
    // the knob holds the keyboard focus rather than its +/- buttons, so an Enter
    // that only confirms a typed value closes the number instead of stepping this
    // knob; the panel keeps no focus of its own, since it stays on screen on every
    // tab and focus resting here would leave the tracker deaf
    transposeAmount.setWantsKeyboardFocus (true);
    // the same reason as the knobs everywhere else in the editor: a Slider in this
    // style has no thumb, so until the drag is asked for the arrows' own amount
    // field can only be stepped by clicking its +/- pair and a sideways drag over
    // it leaves the semitones where they were
    transposeAmount.setIncDecButtonsMode (juce::Slider::incDecButtonsDraggable_Horizontal);
    // this row is the one place a knob is asked for a drag but not for the undo
    // line's two ends of it: the number belongs to the arrows rather than to the
    // model, since a drag here writes the amount the next arrow reads rather than
    // a state of the song, so the drag leaves no point of the undo line to keep
    // as one action and there is nothing for beginKnobDrag and endKnobDrag to do
    transposeAmount.setRange (1.0, 48.0, 1.0); // whole semitones, up to four octaves
    transposeAmount.setValue (12.0, juce::dontSendNotification);
    transposeAmount.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 46, 18);
    transposeAmount.setColour (juce::Slider::textBoxTextColourId, theme::text);
    transposeAmount.setColour (juce::Slider::textBoxBackgroundColourId, theme::panelLight);
    transposeAmount.setColour (juce::Slider::textBoxOutlineColourId, theme::outline);
    addAndMakeVisible (transposeAmount);

    styleButton (transposeUp);
    styleButton (transposeDown);
    addAndMakeVisible (transposeUp);
    addAndMakeVisible (transposeDown);

    transposeUp.onClick = [this] { applyTranspose ((int) transposeAmount.getValue()); };
    transposeDown.onClick = [this] { applyTranspose (- (int) transposeAmount.getValue()); };

    remapFromBox.addItem ("All", 1); // a source that matches every instrument
    addInstruments (remapFromBox, 1); // ids 2..17 are the -0 .. -F instruments
    remapFromBox.setSelectedId (1); // All
    styleCombo (remapFromBox);
    addAndMakeVisible (remapFromBox);

    addInstruments (remapToBox, 0); // ids 1..16 are the -0 .. -F instruments
    remapToBox.setSelectedId (1); // -0
    styleCombo (remapToBox);
    addAndMakeVisible (remapToBox);

    // ids follow the InterpField enumerators so the tool can map them directly.
    // Which column the ramp fills is all it needs: which cells it fills is the
    // selection, so the old "all channels / active channel" dropdown is gone
    interpolateFieldBox.addItem ("Note", 1);
    interpolateFieldBox.addItem ("Instrument", 2);
    interpolateFieldBox.addItem ("Volume", 3);
    interpolateFieldBox.addItem ("Pan", 4);
    interpolateFieldBox.addItem ("CC number", 5);
    interpolateFieldBox.addItem ("CC value", 6);
    interpolateFieldBox.setSelectedId (1); // Note
    styleCombo (interpolateFieldBox);
    addAndMakeVisible (interpolateFieldBox);

    styleButton (remapApply);
    styleButton (interpolateButton);
    addAndMakeVisible (remapApply);
    addAndMakeVisible (interpolateButton);

    remapApply.onClick = [this] { applyRemap(); };
    interpolateButton.onClick = [this] { applyInterpolate(); };
}

bool ToolsPanel::keyPressed (const juce::KeyPress& key)
{
    // the tools panel answers the Alt pair rather than handing it up, because a
    // key press goes to the modal component and only walks up from there: while
    // a macro or pattern context menu was opened over this panel, this panel is
    // what that press is routed to, so a pair the editor alone owned would be
    // swallowed there
    if (answerUndoRedoKey (key, undoStep, redoStep))
        return true;

    // the clipboard's Alt spellings are answered the same way, since the same
    // reason that made the undo pair unreachable gives the clipboard's steps to
    // the host as well: this panel is what routes the press while its menus are
    // about, and its own fields type no letters for a press to fall into
    if (answerClipboardKey (key, copyStep, pasteStep, cutStep))
        return true;

    // the clip slice at the playhead, Alt+S, is answered the same way, since its
    // old Ctrl+X spelling was spent by the host's own cut rather than reaching
    // the plug-in: the Alt letter is claimed by this panel too, so a press during
    // one of its menus or a drag still finds the slice somewhere
    return answerSliceKey (key, sliceStep);
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
    // the label names the unit so the field and the two buttons read as one
    // instruction: move the notes by this many semitones, this way
    section ("TRANSPOSE SEMITONES", transposeAmount);
    section ("REMAP", remapFromBox);
    section ("INTERPOLATE", interpolateFieldBox);

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
    // the amount and both directions share one row: the field is the same width
    // as the bottom bar's number sliders, the two buttons sit to its right
    auto transposeRow = area.removeFromTop (22);
    transposeAmount.setBounds (transposeRow.removeFromLeft (84).reduced (1));
    transposeUp.setBounds (transposeRow.removeFromLeft (28).reduced (1));
    transposeDown.setBounds (transposeRow.reduced (1)); // the last 28 px

    area.removeFromTop (14);
    area.removeFromTop (12); // REMAP label
    auto remapRow = area.removeFromTop (22);
    remapFromBox.setBounds (remapRow.removeFromLeft (62).reduced (1));
    remapToBox.setBounds (remapRow.removeFromRight (62).reduced (1));
    area.removeFromTop (4);
    remapApply.setBounds (area.removeFromTop (22).reduced (1));

    area.removeFromTop (14);
    area.removeFromTop (12); // INTERPOLATE label
    interpolateFieldBox.setBounds (area.removeFromTop (22));

    area.removeFromTop (4);
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
        // the selection scope only ever reaches the active pattern, since the
        // cells the user selected are a view of what the tracker shows. An
        // empty selection costs nothing: the tools ask per cell and find
        // nothing in reach.
        patterns.clear();

        if (isCellSelected)
            if (auto* p = song.findPattern (song.activePatternId))
                patterns.push_back (p);
    }

    return patterns;
}

bool ToolsPanel::cellInScope (int row, int ch) const
{
    if (scopeBox.getSelectedId() != 1)
        return true; // the wider scopes name whole patterns, selection aside

    // the selection scope reaches exactly the cells the tracker is holding, and
    // an unset hook means there is no selection to reach
    return isCellSelected ? isCellSelected (row, ch) : false;
}

void ToolsPanel::applyToCells (Pattern& p, const std::function<void (Cell&, int row, int ch)>& fn)
{
    for (int row = 0; row < p.length; ++row)
    {
        for (int ch = 0; ch < p.channels; ++ch)
            if (cellInScope (row, ch))
                fn (p.rows[(size_t) row][(size_t) ch], row, ch);
    }
}

void ToolsPanel::applyTranspose (int semitones)
{
    for (auto* p : scopedPatterns())
    {
        applyToCells (*p, [semitones] (Cell& c, int, int)
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
        for (int ch = 0; ch < p->channels; ++ch)
        {
            // the instrument the column resolves to at this row, before and
            // after the remap: an explicit cell sets it, an empty one carries
            // the previous value (which the tracker draws darker)
            int before = kEmpty;
            int after = kEmpty;

            // the walk covers the rows above the cells in reach on purpose: a
            // carry has to be known before it can be pinned inside the
            // selection, and pinning means writing an explicit value only where
            // the cell is actually selected
            for (int row = 0; row < p->length; ++row)
            {
                auto& c = p->rows[(size_t) row][(size_t) ch];

                if (c.inst != kEmpty)
                {
                    const int current = c.inst;

                    if ((all || current == from) && cellInScope (row, ch))
                        c.inst = to;

                    before = current;
                    after = c.inst;
                    continue;
                }

                if (! cellInScope (row, ch))
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
    // the ramp only works on what the user picked: outside the selection scope
    // the reach is a whole pattern, where ramping blanks is rarely useful
    if (scopeBox.getSelectedId() != 1)
        return;

    const int fieldId = interpolateFieldBox.getSelectedId();

    if (fieldId <= 0)
        return;

    const auto field = (InterpField) (fieldId - 1);

    for (auto* p : scopedPatterns())
    {
        // the selection is the range the ramp is confined to: anchors come only
        // from selected cells, only selected cells are ever written, and a cell
        // left out of the selection ends the run, so a ragged selection ramps
        // in pieces rather than across the hole
        p->interpolate (field, [this] (int row, int ch) { return cellInScope (row, ch); });
    }

    if (onModelChanged)
        onModelChanged();
}

} // namespace rtr
