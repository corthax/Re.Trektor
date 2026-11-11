#include "MacrosPanel.h"
#include "Theme.h"
#include "AltShortcutKeys.h"
#include <cmath>

namespace rtr
{

namespace
{
    constexpr int kStepsPerLine = 16; // how wide a macro's steps wrap
    constexpr int kControlH = 26; // a card's first line, which holds its controls
    constexpr int kStepLineH = 20; // one line of step cells
    constexpr int kCardGap = 4;
    constexpr int kContentTop = 70; // where the first card starts, below the labels
    constexpr int kSide = 20;
    constexpr int kBottom = 40; // the strip holding the two buttons

    /** A step as it is drawn: signed semitones off the base note, and a step
        that sounds nothing, which sustains the one before it. */
    juce::String arpStepText (std::int8_t step)
    {
        if (step == kArpEmptyStep)
            return "--";

        if (step > 0)
            return "+" + juce::String ((int) step);

        return juce::String ((int) step);
    }
}

MacrosPanel::MacrosPanel (ReTrektorAudioProcessor& processor)
    : proc (processor), song (processor.getSong())
{
    setOpaque (true);

    addButton.onClick = [this]
    {
        // defining takes the lowest id that has none yet, so the ids a row can
        // name stay dense until the last one is taken
        const int id = song.firstUndefArpId();

        if (id <= 0)
            return;

        song.defineArp (id);
        setStepCursor (id, 0);
        rebuild();

        if (onModelChanged)
            onModelChanged();
    };

    removeButton.onClick = [this]
    {
        // the button clears the macro the step cursor sits on, which is the one
        // a click of a card or the arrows last picked out; with no cursor yet it
        // falls back to the highest one a row can call, keeping the ids below the
        // cleared one pointing at the same macros
        int id = stepId;

        if (id <= 0 || ! song.arpDefined (id))
            id = song.lastDefArpId();

        if (id <= 0)
            return;

        song.clearArp (id);

        if (stepId == id)
            stepId = 0;

        rebuild();

        if (onModelChanged)
            onModelChanged();
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
    for (int id = 1; id <= kMaxArpMacros; ++id)
    {
        auto& r = controls[(size_t) id - 1];
        const bool used = song.arpDefined (id);

        if (used && r.len == nullptr)
        {
            r.len = std::make_unique<juce::Slider> (juce::Slider::IncDecButtons, juce::Slider::TextBoxLeft);
            r.len->setRange (1.0, (double) kMaxArpSteps, 1.0);
            r.len->setTextBoxStyle (juce::Slider::TextBoxLeft, false, 44, 18);
            r.len->setColour (juce::Slider::textBoxTextColourId, theme::text);
            r.len->setColour (juce::Slider::textBoxBackgroundColourId, theme::panelLight);
            r.len->setColour (juce::Slider::textBoxOutlineColourId, theme::outline);
            r.len->setColour (juce::Slider::backgroundColourId, theme::panelLight);
            r.len->setColour (juce::Slider::thumbColourId, theme::textDim);
            addAndMakeVisible (*r.len);

            r.loop = std::make_unique<juce::ToggleButton> ("LOOPED");
            r.loop->setColour (juce::ToggleButton::textColourId, theme::text);
            r.loop->setColour (juce::ToggleButton::tickColourId, theme::accent);
            r.loop->setColour (juce::ToggleButton::tickDisabledColourId, theme::textDim);
            addAndMakeVisible (*r.loop);

            const int index = id;

            r.len->onValueChange = [this, index]
            {
                song.setArpLength (index, (int) controls[(size_t) index - 1].len->getValue());

                // shortening a macro can leave the step cursor past its end
                const auto* m = song.peekArp (index);

                if (stepId == index && m != nullptr && m->defined() && stepIndex >= m->length())
                    setStepCursor (index, m->length() - 1);

                // dragging the length is an action of its own, so the undo line
                // gets a point for it: leaving it out would have a later Ctrl+Z
                // step back over the length change without saying so
                if (onModelChanged)
                    onModelChanged();

                clampScroll();
                resized();
                repaint();
            };

            r.loop->onClick = [this, index]
            {
                // clicking the toggle is what flips it: write that state into the
                // macro and read it back, so the panel and the model agree
                song.setArpLooped (index, controls[(size_t) index - 1].loop->getToggleState());

                if (onModelChanged)
                    onModelChanged();

                repaint();
            };
        }

        if (r.len != nullptr)
        {
            r.len->setVisible (used);
            r.loop->setVisible (used);
        }
    }

    if (stepId <= 0)
        stepId = song.lastDefArpId();

    syncValues();
    clampScroll();
    resized();
    repaint();
}

void MacrosPanel::syncValues()
{
    for (int id = 1; id <= kMaxArpMacros; ++id)
    {
        auto& r = controls[(size_t) id - 1];
        const auto* m = song.peekArp (id);

        if (r.len == nullptr || m == nullptr || ! m->defined())
            continue;

        r.len->setValue ((double) m->length(), juce::dontSendNotification);
        r.loop->setToggleState (m->looped, juce::dontSendNotification);
    }
}

//==============================================================================
int MacrosPanel::cardHeight (int length) const
{
    const int lines = (juce::jmax (1, length) + kStepsPerLine - 1) / kStepsPerLine;
    return kControlH + lines * kStepLineH + kCardGap;
}

int MacrosPanel::contentHeight() const
{
    int total = 0;

    for (int id = 1; id <= kMaxArpMacros; ++id)
        if (song.arpDefined (id))
            total += cardHeight (song.peekArp (id)->length());

    return total;
}

int MacrosPanel::blockTop (int id) const
{
    int y = kContentTop - scrollY;

    for (int below = 1; below < id; ++below)
        if (song.arpDefined (below))
            y += cardHeight (song.peekArp (below)->length());

    return y;
}

void MacrosPanel::clampScroll()
{
    const int available = juce::jmax (0, getHeight() - kContentTop - kBottom);
    const int maxScroll = juce::jmax (0, contentHeight() - available);
    scrollY = juce::jlimit (0, maxScroll, scrollY);
}

//==============================================================================
// the drawn scrollbar, which the mouse is grabbed by as well as turned: its
// bounds are asked for in one place so a drag answers on exactly what paint
// draws and the thumb it centres on is the one that is visible
int MacrosPanel::scrollbarX() const
{
    return getWidth() - 10;
}

int MacrosPanel::thumbHeight() const
{
    const int available = juce::jmax (0, getHeight() - kContentTop - kBottom);
    const int total = contentHeight();
    const int trackH = juce::jmax (1, available);
    return juce::jmax (8, trackH * available / juce::jmax (1, total));
}

int MacrosPanel::thumbTop() const
{
    const int available = juce::jmax (0, getHeight() - kContentTop - kBottom);
    const int total = contentHeight();
    const int trackH = juce::jmax (1, available);
    const int thumbH = thumbHeight();
    return kContentTop + (trackH - thumbH) * scrollY / juce::jmax (1, total - available);
}

void MacrosPanel::scrollToPointer (int y)
{
    const int available = juce::jmax (0, getHeight() - kContentTop - kBottom);
    const int total = contentHeight();
    const int trackH = juce::jmax (1, available);
    const int thumbH = thumbHeight();
    const int maxScroll = juce::jmax (0, total - available);
    const int span = juce::jmax (1, trackH - thumbH);

    // the thumb is put under the pointer, its own middle at the pointer's height
    const int offset = juce::jlimit (0, span, y - thumbH / 2 - kContentTop);
    scrollY = (int) std::round ((double) offset * (double) maxScroll / (double) span);

    resized();
    repaint();
}

int MacrosPanel::prevDefArpId (int id) const
{
    for (int below = id - 1; below >= 1; --below)
        if (song.arpDefined (below))
            return below;

    return 0;
}

int MacrosPanel::nextDefArpId (int id) const
{
    for (int above = id + 1; above <= kMaxArpMacros; ++above)
        if (song.arpDefined (above))
            return above;

    return 0;
}

void MacrosPanel::setStepCursor (int id, int index)
{
    // a number that was still being typed belongs to the step the cursor was on
    // whatever gesture moves it away, so it closes ahead of the move
    if (typingStep)
        commitStepText();

    if (id < 1 || ! song.arpDefined (id))
        return;

    stepId = id;
    stepIndex = juce::jlimit (0, song.peekArp (id)->length() - 1, index);
}

void MacrosPanel::commitStepText()
{
    if (! typingStep)
        return;

    typingStep = false;

    if (stepId > 0 && song.arpDefined (stepId))
    {
        // a step that was only given a sign means the base note itself, which is
        // what makes typing "-" on and then Enter a step of nothing but the note,
        // and the number a sign and up to three digits typed is read as one value
        // kept inside the jump the field can hold
        const bool onlyASign = stepText.isEmpty()
            || (stepText.length() == 1 && (stepText[0] == '-' || stepText[0] == '+'));
        const int value = onlyASign ? 0 : stepText.getIntValue();
        song.setArpStep (stepId, stepIndex, (std::int8_t) juce::jlimit (-127, 127, value));

        if (onModelChanged)
            onModelChanged();
    }

    stepText.clear();
    repaint();
}

//==============================================================================
bool MacrosPanel::keyPressed (const juce::KeyPress& key)
{
    const int code = key.getKeyCode();
    const juce::juce_wchar text = key.getTextCharacter();

    // a step that is still half typed belongs to the cursor the press was aimed
    // at whatever the press turns out to be, so it closes before any other
    // effect a press can have: one further character of that step's own number
    // is the only thing that does not close it, which is what lets a sign and
    // more than one digit be typed into a step rather than only its last figure
    const bool anotherStepChar = stepId > 0 && song.arpDefined (stepId)
        && (text == '-' || text == '+' || (text >= '0' && text <= '9'));

    if (typingStep && ! anotherStepChar && code != juce::KeyPress::escapeKey)
        commitStepText();

    // the macros panel answers the Alt pair rather than handing it up, because a
    // key press goes to the modal component and only walks up from there: while
    // the macros panel's own context menu is open, this panel is what that press
    // is routed to, so a pair the editor alone owned was swallowed there
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
    // one of its menus still finds the slice somewhere
    if (answerSliceKey (key, sliceStep))
        return true;

    //== Step cursor ==========================================================
    // the strip is drawn rather than built out of one control per step, so the
    // arrows and a click are what put the cursor on a step and a typed sign and
    // digits land on it
    if (code == juce::KeyPress::upKey)
    {
        setStepCursor (prevDefArpId (stepId), stepIndex);
        repaint();
        return true;
    }

    if (code == juce::KeyPress::downKey || code == juce::KeyPress::tabKey)
    {
        setStepCursor (nextDefArpId (stepId), stepIndex);
        repaint();
        return true;
    }

    if (code == juce::KeyPress::leftKey)
    {
        const auto* m = song.peekArp (stepId);

        if (m != nullptr && m->defined())
            setStepCursor (stepId, (stepIndex - 1 + m->length()) % m->length());

        repaint();
        return true;
    }

    if (code == juce::KeyPress::rightKey)
    {
        const auto* m = song.peekArp (stepId);

        if (m != nullptr && m->defined())
            setStepCursor (stepId, (stepIndex + 1) % m->length());

        repaint();
        return true;
    }

    if (code == juce::KeyPress::pageUpKey)
    {
        setStepCursor (stepId, 0);
        repaint();
        return true;
    }

    if (code == juce::KeyPress::pageDownKey)
    {
        const auto* m = song.peekArp (stepId);

        if (m != nullptr && m->defined())
            setStepCursor (stepId, m->length() - 1);

        repaint();
        return true;
    }

    if (code == juce::KeyPress::deleteKey || code == juce::KeyPress::backspaceKey)
    {
        // clearing a step makes it sound nothing, which sustains the step before
        // it rather than the base note
        if (stepId > 0 && song.arpDefined (stepId))
        {
            song.setArpStep (stepId, stepIndex, kArpEmptyStep);

            if (onModelChanged)
                onModelChanged();
        }

        repaint();
        return true;
    }

    if (code == juce::KeyPress::escapeKey)
    {
        // the value that was half typed was not meant after all: it is dropped,
        // and the cursor stays on the step it was on
        typingStep = false;
        stepText.clear();
        repaint();
        return true;
    }

    //== Typing a step value ==================================================
    if (stepId <= 0 || ! song.arpDefined (stepId))
        return false;

    if (text == '-' || text == '+' || (text >= '0' && text <= '9'))
    {
        // a step's number is typed one character at a time into the cell the
        // cursor sits on, so a sign and up to three digits build one value:
        // three of them reach the jump the field can hold, and the sign leads
        // whatever digits were typed, which is what puts a step below the base
        // note rather than only taking its last figure
        if (! typingStep)
            stepText.clear();

        if (text == '-' || text == '+')
        {
            const juce::String sign = juce::String::charToString (text);

            if (stepText.isEmpty())
                stepText = sign; // the number starts with its sign
            else if (stepText.startsWithChar (text))
            {
                // the sign that was meant is still there, and a repeat of it
                // changes nothing, which is what keeps a step's number from
                // losing its sign under a second press of the same key
            }
            else if (stepText.startsWithChar ('-') || stepText.startsWithChar ('+'))
                stepText = sign + stepText.substring (1); // the other side of the base note
            else
                stepText = sign + stepText; // a sign typed after digits belongs to them
        }
        else if (stepText.length() < 4)
            stepText << juce::String::charToString (text);

        typingStep = true;
        repaint();
        return true;
    }

    return false;
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
    g.drawText ("Arpeggiator macros", 20, 12, 400, 22, juce::Justification::centredLeft, false);

    // the tab names only what its own rows hold, since the model's rules are
    // what the Commands map and the README carry rather than something painted
    // above the cards
    g.setColour (theme::text);
    g.setFont (theme::monoFont (10.0f));
    g.drawText ("MACRO", kSide, 48, 70, 12, juce::Justification::centredLeft, false);
    g.drawText ("LENGTH", 104, 48, 70, 12, juce::Justification::centredLeft, false);
    g.drawText ("LOOPED", 224, 48, 70, 12, juce::Justification::centredLeft, false);
    g.drawText ("STEPS, 16 to a line", 344, 48, getWidth() - 384, 12, juce::Justification::centredLeft, false);

    const int cellW = juce::jmax (1, (getWidth() - 2 * kSide) / kStepsPerLine);

    // the cards are clipped to the content area so a scrolled or overfull list
    // keeps its text off the header and off the button strip
    g.saveState();
    g.reduceClipRegion (getLocalBounds().withTrimmedTop (kContentTop));

    int card = 0;

    for (int id = 1; id <= kMaxArpMacros; ++id)
    {
        const auto* m = song.peekArp (id);

        if (m == nullptr || ! m->defined())
            continue;

        const int len = m->length();
        const int y = blockTop (id);
        const int cardH = cardHeight (len);

        if (y > getHeight() || y + cardH < kContentTop)
        {
            ++card;
            continue;
        }

        g.setColour (card % 2 == 0 ? theme::panel.withAlpha (0.5f) : theme::background);
        g.fillRect (16, y - 3, getWidth() - 32, cardH - kCardGap);

        ++card;

        // the call as a row writes it: the letter and the id it names
        g.setColour (theme::cmdMacro);
        g.setFont (theme::monoFont (12.0f));
        g.drawText ("A " + juce::String (id), kSide, y + 4, 76, 16, juce::Justification::centredLeft, false);

        for (int i = 0; i < len; ++i)
        {
            const int line = i / kStepsPerLine;
            const int col = i % kStepsPerLine;
            const int x = kSide + col * cellW;
            const int sy = y + kControlH + line * kStepLineH;

            if (sy > getHeight())
                break;

            const bool here = id == stepId && i == stepIndex;

            if (here)
            {
                g.setColour (theme::selection);
                g.fillRoundedRectangle (juce::Rectangle<int> (x, sy, cellW - 1, kStepLineH - 2).toFloat(), 2.0f);
            }

            // the number being typed at the cursor is what the cell shows while
            // it lasts, so a sign and its digits are read as they are entered
            // rather than only once the press that closes them arrives
            const bool pending = here && typingStep && stepText.isNotEmpty();
            const juce::String shown = pending ? stepText : arpStepText (m->steps[(size_t) i]);

            g.setColour (pending || m->steps[(size_t) i] != kArpEmptyStep ? theme::cmdMacro : theme::noteOff);
            g.setFont (theme::monoFont (11.0f));
            g.drawText (shown, x + 2, sy + 3, cellW - 4, kStepLineH - 4,
                        juce::Justification::centredLeft, true);
        }
    }

    g.restoreState();

    // the step cursor in the card's own control line, which is where a macro was
    // clicked at rather than one of its steps
    if (stepId > 0 && song.arpDefined (stepId))
    {
        const int y = blockTop (stepId);

        if (y >= kContentTop && y + kControlH <= getHeight() - kBottom)
        {
            g.setColour (theme::cursor);
            g.drawVerticalLine (kSide - 4, y + 2.0f, y + kControlH - 3.0f);
        }
    }

    // the cards that do not fit scroll out, so the strip shows where they are
    const int available = juce::jmax (0, getHeight() - kContentTop - kBottom);
    const int total = contentHeight();

    if (total > available)
    {
        const int trackX = scrollbarX();
        const int trackTop = kContentTop;
        const int trackH = juce::jmax (1, available);

        g.setColour (theme::outline);
        g.fillRect (trackX, trackTop, 4, trackH);

        g.setColour (theme::textDim);
        g.fillRect (trackX - 1, thumbTop(), 6, thumbHeight());
    }
}

//==============================================================================
void MacrosPanel::resized()
{
    addButton.setBounds (20, getHeight() - 34, 110, 22);
    removeButton.setBounds (138, getHeight() - 34, 132, 22);

    const int bottomLimit = getHeight() - kBottom;

    for (int id = 1; id <= kMaxArpMacros; ++id)
    {
        auto& r = controls[(size_t) id - 1];

        if (r.len == nullptr)
            continue;

        const bool used = song.arpDefined (id);
        const int y = blockTop (id);
        const bool visible = used && y + kControlH <= bottomLimit && y + cardHeight (song.peekArp (id)->length()) >= kContentTop;

        r.len->setVisible (visible);
        r.loop->setVisible (visible);

        if (visible)
        {
            r.len->setBounds (104, y, 112, 22);
            r.loop->setBounds (224, y, 96, 22);
        }
    }

    addButton.setEnabled (song.firstUndefArpId() > 0);
}

//==============================================================================
void MacrosPanel::mouseDown (const juce::MouseEvent& e)
{
    // the strip is drawn rather than built, so the grab on it is answered here
    // ahead of the cards: a pointer on its column takes the thumb to the click
    // and keeps it on the pointer for as long as the button is held, which is
    // what lets many macros be scrolled by mouse as well as by the wheel
    const int available = juce::jmax (0, getHeight() - kContentTop - kBottom);
    const int total = contentHeight();

    if (total > available)
    {
        const int trackX = scrollbarX();
        const int trackH = juce::jmax (1, available);

        if (e.x >= trackX - 1 && e.x < trackX + 5 && e.y >= kContentTop && e.y < kContentTop + trackH)
        {
            draggingThumb = true;
            scrollToPointer (e.y);
            return;
        }
    }

    const int cellW = juce::jmax (1, (getWidth() - 2 * kSide) / kStepsPerLine);

    for (int id = 1; id <= kMaxArpMacros; ++id)
    {
        const auto* m = song.peekArp (id);

        if (m == nullptr || ! m->defined())
            continue;

        const int y = blockTop (id);
        const int len = m->length();
        const int lines = (len + kStepsPerLine - 1) / kStepsPerLine;

        if (e.y >= y && e.y < y + kControlH)
        {
            setStepCursor (id, 0); // the card's control line, its first step
            repaint();
            return;
        }

        if (e.y >= y + kControlH && e.y < y + kControlH + lines * kStepLineH)
        {
            const int line = (e.y - y - kControlH) / kStepLineH;
            const int col = (e.x - kSide) / cellW;

            if (col >= 0 && col < kStepsPerLine)
            {
                const int index = line * kStepsPerLine + col;

                if (index < len)
                {
                    setStepCursor (id, index);
                    repaint();
                    return;
                }
            }
        }
    }
}

void MacrosPanel::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& wheel)
{
    const int available = juce::jmax (0, getHeight() - kContentTop - kBottom);
    const int maxScroll = juce::jmax (0, contentHeight() - available);
    scrollY = juce::jlimit (0, maxScroll, scrollY + (int) std::round (-wheel.deltaY * 40.0f));
    resized();
    repaint();
}

void MacrosPanel::mouseDrag (const juce::MouseEvent& e)
{
    if (draggingThumb)
        scrollToPointer (e.y);
}

void MacrosPanel::mouseUp (const juce::MouseEvent&)
{
    draggingThumb = false;
}

} // namespace rtr
