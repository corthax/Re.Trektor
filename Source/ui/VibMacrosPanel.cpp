#include "VibMacrosPanel.h"
#include "Theme.h"
#include "AltShortcutKeys.h"
#include <cmath>

namespace rtr
{

namespace
{
    constexpr int kControlH = 22; // the one line a card holds its three settings on
    constexpr int kCardGap = 4;
    constexpr int kContentTop = 70; // where the first card starts, below the labels
    constexpr int kSide = 20;
    constexpr int kBottom = 40; // the strip holding the two buttons

    constexpr int kShapeX = 104; // the dropdown, the amplitude knob and the cycle knob
    constexpr int kAmpX = 232;
    constexpr int kCycleX = 360;

    // the six shapes, spelled as the feature asks for them and read back off the
    // dropdown rather than drawn a second time, so the tab's own text is what a
    // test compares a macro's shape against
    struct ShapeName
    {
        const char* text;
        int value;
    };

    const ShapeName kShapes[] = {
        { "Sin",           kVibTypeSin },
        { "Sin Up",        kVibTypeSinUp },
        { "Sin Down",      kVibTypeSinDown },
        { "Triangle",      kVibTypeTri },
        { "Triangle Up",   kVibTypeTriUp },
        { "Triangle Down", kVibTypeTriDown },
    };
}

VibMacrosPanel::VibMacrosPanel (ReTrektorAudioProcessor& processor)
    : proc (processor), song (processor.getSong())
{
    setOpaque (true);

    // the panel takes the keyboard focus on its own tab, so a press walks here
    // rather than onto whichever control the pointer touched last: a Button holds
    // that focus by default and answers the Return key, which is what made an
    // Enter that only confirms a typed value add or clear a macro instead
    setWantsKeyboardFocus (true);

    addButton.onClick = [this]
    {
        // a fresh macro takes the lowest id that has none yet, so the ids a row
        // can call stay dense until the last one is taken, and it arrives at the
        // shape, amplitude and cycle the model gives a newly defined one, which is
        // what makes the card show a wobble rather than an empty line
        const int id = song.firstUndefVibId();

        if (id <= 0)
            return;

        song.defineVib (id);
        selectedId = id;
        rebuild();

        if (onModelChanged)
            onModelChanged();
    };

    removeButton.onClick = [this]
    {
        // the button puts away the macro the last click or the arrows picked out;
        // with nothing picked out it falls back to the highest one a row can call,
        // which keeps the ids below the removed one pointing at the same macros
        int id = selectedId;

        if (id <= 0 || ! song.vibDefined (id))
            id = song.lastDefVibId();

        if (id <= 0)
            return;

        // clearing a macro says it is not one rather than erasing a setting, so
        // the card goes away while the shape, the amplitude and the cycle the
        // knobs had stay in the macro's own room: defining it again starts over
        // from the defaults the model gives a fresh one
        song.clearVib (id);

        if (selectedId == id)
            selectedId = 0;

        rebuild();

        if (onModelChanged)
            onModelChanged();
    };

    for (auto* b : { &addButton, &removeButton })
    {
        // the buttons take no keyboard focus, so an Enter cannot fire the one the
        // pointer left last and an add or a clear only ever happens on a click
        b->setWantsKeyboardFocus (false);

        b->setColour (juce::TextButton::buttonColourId, theme::panelLight);
        b->setColour (juce::TextButton::textColourOffId, theme::text);
        addAndMakeVisible (*b);
    }

    rebuild();
}

//==============================================================================
void VibMacrosPanel::rebuild()
{
    for (int id = 1; id <= kMaxVibMacros; ++id)
    {
        auto& r = controls[(size_t) id - 1];
        const bool used = song.vibDefined (id);

        if (used && r.shape == nullptr)
        {
            r.shape = std::make_unique<juce::ComboBox>();
            r.shape->setColour (juce::ComboBox::backgroundColourId, theme::panelLight);
            r.shape->setColour (juce::ComboBox::textColourId, theme::text);
            r.shape->setColour (juce::ComboBox::outlineColourId, theme::outline);
            r.shape->setColour (juce::ComboBox::arrowColourId, theme::textDim);

            for (const auto& shape : kShapes)
                r.shape->addItem (shape.text, shape.value + 1); // ids start at one

            addAndMakeVisible (*r.shape);

            r.amp = std::make_unique<juce::Slider> (juce::Slider::IncDecButtons, juce::Slider::TextBoxLeft);

            // the knob holds the keyboard focus and its +/- buttons do not, so an
            // Enter that only confirms a typed value closes the number instead of
            // nudging whichever knob the pointer last touched
            r.amp->setWantsKeyboardFocus (true);
            // the same reason as every other knob here: until the drag is asked
            // for, the +/- pair the Slider builds answers clicks only and a
            // sideways drag over the card leaves the amplitude where it was
            r.amp->setIncDecButtonsMode (juce::Slider::incDecButtonsDraggable_Horizontal);
            // the amplitude is a share of full pitchbend rather than a number of
            // its own, which is what makes the knob read as the wheel's own half
            // height rather than as hundredths of it
            r.amp->setRange (0.0, 1.0, 0.01);
            r.amp->setTextBoxStyle (juce::Slider::TextBoxLeft, false, 44, 18);
            r.amp->setColour (juce::Slider::textBoxTextColourId, theme::text);
            r.amp->setColour (juce::Slider::textBoxBackgroundColourId, theme::panelLight);
            r.amp->setColour (juce::Slider::textBoxOutlineColourId, theme::outline);
            r.amp->setColour (juce::Slider::backgroundColourId, theme::panelLight);
            r.amp->setColour (juce::Slider::thumbColourId, theme::textDim);
            addAndMakeVisible (*r.amp);

            r.cycle = std::make_unique<juce::Slider> (juce::Slider::IncDecButtons, juce::Slider::TextBoxLeft);

            // the same reason as the amplitude knob's: the Enter belongs to the
            // number being typed, not to a knob's own +/- buttons
            r.cycle->setWantsKeyboardFocus (true);
            // the same reason as the amplitude knob's: the pair this Slider builds
            // for it answers a click only, so the drag has to be asked for
            r.cycle->setIncDecButtonsMode (juce::Slider::incDecButtonsDraggable_Horizontal);
            // the cycle is counted in subticks, the same grid an arpeggiator's
            // steps and a slide's ramp are measured over, so a cycle of one of
            // them is one subtick wide rather than one row wide
            r.cycle->setRange (1.0, (double) kVibFreqTop, 1.0);
            r.cycle->setTextBoxStyle (juce::Slider::TextBoxLeft, false, 44, 18);
            r.cycle->setColour (juce::Slider::textBoxTextColourId, theme::text);
            r.cycle->setColour (juce::Slider::textBoxBackgroundColourId, theme::panelLight);
            r.cycle->setColour (juce::Slider::textBoxOutlineColourId, theme::outline);
            r.cycle->setColour (juce::Slider::backgroundColourId, theme::panelLight);
            r.cycle->setColour (juce::Slider::thumbColourId, theme::textDim);
            addAndMakeVisible (*r.cycle);

            const int index = id;

            // a drag over either of the card's two knobs is one action of the undo
            // line rather than one point per notification: the Slider answers every
            // mouse event its host moves along its own row with a value
            // notification, and each of those reaches the handler under it and,
            // through it, the editor's change of the model, so a drag that
            // stepped over eight hundredths of the wheel used to earn eight undo
            // points and one Ctrl+Z left the amplitude still seven of them along
            // the drag rather than off it. The undo line hears the drag's two
            // ends here, keeps the one point that drag's first change made,
            // moves it along for the changes after it, and drops it when the
            // drag ends on the number it began at, which is what a knob
            // carried out and back leaves nothing to undo
            auto* ampKnob = r.amp.get();
            ampKnob->onDragStart = [this] { proc.beginKnobDrag(); };
            ampKnob->onDragEnd = [this] { proc.endKnobDrag(); };

            auto* cycleKnob = r.cycle.get();
            cycleKnob->onDragStart = [this] { proc.beginKnobDrag(); };
            cycleKnob->onDragEnd = [this] { proc.endKnobDrag(); };

            // the dropdown answers a drag or a keyboard nudge of its own: its
            // shape is a setting of the model rather than a drawing, so the
            // change has to reach the wobble that a row already calls and give
            // the undo line a point of its own, which is what a later Ctrl+Z
            // steps back over without saying so
            r.shape->onChange = [this, index]
            {
                auto& c = controls[(size_t) index - 1];

                if (c.shape == nullptr)
                    return;

                song.setVibType (index, c.shape->getSelectedId() - 1);

                if (onModelChanged)
                    onModelChanged();

                clampScroll();
                resized();
                repaint();
            };

            r.amp->onValueChange = [this, index]
            {
                auto& c = controls[(size_t) index - 1];

                if (c.amp == nullptr)
                    return;

                // the knob spells the share and the model keeps the hundredths, so
                // the share is read back through the same clamp the setter uses
                song.setVibAmplitude (index, (int) std::llround (c.amp->getValue() * (double) kVibAmpTop));

                if (onModelChanged)
                    onModelChanged();

                clampScroll();
                resized();
                repaint();
            };

            r.cycle->onValueChange = [this, index]
            {
                auto& c = controls[(size_t) index - 1];

                if (c.cycle == nullptr)
                    return;

                song.setVibFrequency (index, (int) c.cycle->getValue());

                // a cycle made narrower or wider moves every point of a wobble
                // already swinging, which is what leaves the wobble changed from
                // where it is rather than holding the cycle it had at its arming
                // row
                if (onModelChanged)
                    onModelChanged();

                clampScroll();
                resized();
                repaint();
            };
        }

        if (r.shape != nullptr)
        {
            r.shape->setVisible (used);
            r.amp->setVisible (used);
            r.cycle->setVisible (used);
        }
    }

    if (selectedId <= 0)
        selectedId = song.lastDefVibId();

    syncValues();
    clampScroll();
    resized();
    repaint();
}

void VibMacrosPanel::syncValues()
{
    for (int id = 1; id <= kMaxVibMacros; ++id)
    {
        auto& r = controls[(size_t) id - 1];
        const auto* m = song.peekVib (id);

        if (r.shape == nullptr || m == nullptr || ! m->defined())
            continue;

        r.shape->setSelectedItemIndex (m->type, juce::dontSendNotification);
        r.amp->setValue ((double) m->amplitude / (double) kVibAmpTop, juce::dontSendNotification);
        r.cycle->setValue ((double) m->frequency, juce::dontSendNotification);
    }
}

//==============================================================================
int VibMacrosPanel::contentHeight() const
{
    int total = 0;

    for (int id = 1; id <= kMaxVibMacros; ++id)
        if (song.vibDefined (id))
            total += kControlH + kCardGap;

    return total;
}

int VibMacrosPanel::cardTop (int id) const
{
    int y = kContentTop - scrollY;

    for (int below = 1; below < id; ++below)
        if (song.vibDefined (below))
            y += kControlH + kCardGap;

    return y;
}

void VibMacrosPanel::clampScroll()
{
    const int available = juce::jmax (0, getHeight() - kContentTop - kBottom);
    const int maxScroll = juce::jmax (0, contentHeight() - available);
    scrollY = juce::jlimit (0, maxScroll, scrollY);
}

//==============================================================================
// the drawn scrollbar, which the mouse is grabbed by as well as turned: its
// bounds are asked for in one place so a drag answers on exactly what paint
// draws and the thumb it centres on is the one that is visible
int VibMacrosPanel::scrollbarX() const
{
    return getWidth() - 10;
}

int VibMacrosPanel::thumbHeight() const
{
    const int available = juce::jmax (0, getHeight() - kContentTop - kBottom);
    const int total = contentHeight();
    const int trackH = juce::jmax (1, available);
    return juce::jmax (8, trackH * available / juce::jmax (1, total));
}

int VibMacrosPanel::thumbTop() const
{
    const int available = juce::jmax (0, getHeight() - kContentTop - kBottom);
    const int total = contentHeight();
    const int trackH = juce::jmax (1, available);
    const int thumbH = thumbHeight();
    return kContentTop + (trackH - thumbH) * scrollY / juce::jmax (1, total - available);
}

void VibMacrosPanel::scrollToPointer (int y)
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

int VibMacrosPanel::prevDefVibId (int id) const
{
    for (int below = id - 1; below >= 1; --below)
        if (song.vibDefined (below))
            return below;

    return 0;
}

int VibMacrosPanel::nextDefVibId (int id) const
{
    for (int above = id + 1; above <= kMaxVibMacros; ++above)
        if (song.vibDefined (above))
            return above;

    return 0;
}

juce::String VibMacrosPanel::vibShapeLabel (int type) const
{
    for (const auto& shape : kShapes)
        if (shape.value == type)
            return shape.text;

    return kShapes[0].text; // a shape outside the six reads as the plainest one,
                            // which is what the model's own clamp leaves it as
}

juce::String VibMacrosPanel::vibAmplitudeLabel (int amplitude) const
{
    const int whole = juce::jlimit (0, kVibAmpTop, amplitude) / 100;
    const int hundredths = juce::jlimit (0, kVibAmpTop, amplitude) % 100;

    return juce::String (whole) + "." + juce::String (hundredths).paddedLeft ('0', 2);
}

//==============================================================================
bool VibMacrosPanel::keyPressed (const juce::KeyPress& key)
{
    const int code = key.getKeyCode();

    // the vibrato panel answers the Alt pair rather than handing it up, because a
    // key press goes to the modal component and only walks up from there: while
    // one of this panel's dropdowns is open, this panel is what that press is
    // routed to, so a pair the editor alone owned was swallowed there
    if (answerUndoRedoKey (key, undoStep, redoStep))
        return true;

    // the clipboard's Alt spellings are answered the same way, since the same
    // reason that made the undo pair unreachable gives the clipboard's steps to
    // the host as well
    if (answerClipboardKey (key, copyStep, pasteStep, cutStep))
        return true;

    // the clip slice at the playhead, Alt+S, is answered the same way
    if (answerSliceKey (key, sliceStep))
        return true;

    //== The macro cursor =====================================================
    // the cards hold three settings each rather than a strip of cells, so the
    // arrows move between cards rather than inside one: the dropdown and the
    // knobs are what a drag or a keyboard focus turns, and the arrows are what
    // picks the card the remove button puts away
    if (code == juce::KeyPress::upKey)
    {
        selectedId = prevDefVibId (selectedId);

        if (selectedId <= 0)
            selectedId = song.lastDefVibId();

        repaint();
        return true;
    }

    if (code == juce::KeyPress::downKey || code == juce::KeyPress::tabKey)
    {
        selectedId = nextDefVibId (selectedId);
        repaint();
        return true;
    }

    if (code == juce::KeyPress::pageUpKey)
    {
        // the lowest card the list holds, so one press goes to the top of the
        // list rather than one card up
        if (song.vibDefined (1))
            selectedId = 1;
        else
        {
            for (int id = 1; id <= kMaxVibMacros; ++id)
                if (song.vibDefined (id))
                {
                    selectedId = id;
                    break;
                }
        }

        repaint();
        return true;
    }

    if (code == juce::KeyPress::pageDownKey)
    {
        selectedId = song.lastDefVibId();
        repaint();
        return true;
    }

    if (code == juce::KeyPress::returnKey)
    {
        // the dropdown and the two knobs answer a press of their own, in the text
        // box each of them holds, so the panel has nothing of its own to close: it
        // spends the Enter here rather than handing it up, which is what keeps a
        // press that only confirms a typed value from reaching a card's controls
        // or the editor's shortcuts above
        return true;
    }

    return false;
}

void VibMacrosPanel::refresh()
{
    rebuild();
}

//==============================================================================
void VibMacrosPanel::paint (juce::Graphics& g)
{
    g.fillAll (theme::background);

    g.setColour (theme::text);
    g.setFont (theme::uiFont (15.0f, true));
    g.drawText ("Vibrato macros", 20, 12, 400, 22, juce::Justification::centredLeft, false);

    // the tab names only what its own lines hold, since the model's rules are
    // what the Commands map and the README carry rather than something painted
    // above the cards
    g.setColour (theme::text);
    g.setFont (theme::monoFont (10.0f));
    g.drawText ("MACRO", kSide, 48, 70, 12, juce::Justification::centredLeft, false);
    g.drawText ("SHAPE", kShapeX, 48, 70, 12, juce::Justification::centredLeft, false);
    g.drawText ("AMPLITUDE (of full bend)", kAmpX, 48, getWidth() - kAmpX - 120, 12,
                juce::Justification::centredLeft, false);
    g.drawText ("CYCLE (subticks per swing)", kCycleX + 118, 48, getWidth() - kCycleX - 238, 12,
                juce::Justification::centredLeft, false);

    // the cards are clipped to the content area so a scrolled or overfull list
    // keeps its text off the header and off the button strip
    g.saveState();
    g.reduceClipRegion (getLocalBounds().withTrimmedTop (kContentTop));

    int card = 0;

    for (int id = 1; id <= kMaxVibMacros; ++id)
    {
        const auto* m = song.peekVib (id);

        if (m == nullptr || ! m->defined())
            continue;

        const int y = cardTop (id);

        if (y > getHeight() || y + kControlH + kCardGap < kContentTop)
        {
            ++card;
            continue;
        }

        g.setColour (card % 2 == 0 ? theme::panel.withAlpha (0.5f) : theme::background);
        g.fillRect (16, y - 3, getWidth() - 32, kControlH);

        ++card;

        // the call as a row writes it: the letter and the id it names, in the
        // lilac the wheel's own commands are read in rather than in the orange
        // of an arpeggiator call, which is what makes the two tabs' cards tell
        // the two letters apart at a glance
        g.setColour (theme::cmdOther);
        g.setFont (theme::monoFont (12.0f));
        g.drawText ("V " + juce::String (id), kSide, y + 4, 76, 16, juce::Justification::centredLeft, false);
    }

    g.restoreState();

    // the macro the last gesture picked out, marked in its own line rather than
    // over one of its settings, which is what the remove button answers against
    if (selectedId > 0 && song.vibDefined (selectedId))
    {
        const int y = cardTop (selectedId);

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
void VibMacrosPanel::resized()
{
    addButton.setBounds (20, getHeight() - 34, 110, 22);
    removeButton.setBounds (138, getHeight() - 34, 132, 22);

    const int bottomLimit = getHeight() - kBottom;

    for (int id = 1; id <= kMaxVibMacros; ++id)
    {
        auto& r = controls[(size_t) id - 1];

        if (r.shape == nullptr)
            continue;

        const bool used = song.vibDefined (id);
        const int y = cardTop (id);
        const bool visible = used && y + kControlH <= bottomLimit && y + kControlH + kCardGap >= kContentTop;

        r.shape->setVisible (visible);
        r.amp->setVisible (visible);
        r.cycle->setVisible (visible);

        if (visible)
        {
            r.shape->setBounds (kShapeX, y, 116, kControlH);
            r.amp->setBounds (kAmpX, y, 116, kControlH);
            r.cycle->setBounds (kCycleX, y, 116, kControlH);
        }
    }

    addButton.setEnabled (song.firstUndefVibId() > 0);
}

//==============================================================================
void VibMacrosPanel::mouseDown (const juce::MouseEvent& e)
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

    // a click on a card picks that macro out rather than one of its settings,
    // which is what the remove button and the arrows answer against; the
    // settings themselves are turned by the controls sitting over the card, so
    // a pointer on one of those never reaches here
    for (int id = 1; id <= kMaxVibMacros; ++id)
    {
        const auto* m = song.peekVib (id);

        if (m == nullptr || ! m->defined())
            continue;

        const int y = cardTop (id);

        if (e.y >= y && e.y < y + kControlH && e.x >= kSide && e.x < kShapeX)
        {
            selectedId = id;
            repaint();
            return;
        }
    }
}

void VibMacrosPanel::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& wheel)
{
    const int available = juce::jmax (0, getHeight() - kContentTop - kBottom);
    const int maxScroll = juce::jmax (0, contentHeight() - available);
    scrollY = juce::jlimit (0, maxScroll, scrollY + (int) std::round (-wheel.deltaY * 40.0f));
    resized();
    repaint();
}

void VibMacrosPanel::mouseDrag (const juce::MouseEvent& e)
{
    if (draggingThumb)
        scrollToPointer (e.y);
}

void VibMacrosPanel::mouseUp (const juce::MouseEvent&)
{
    draggingThumb = false;
}

} // namespace rtr
