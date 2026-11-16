#include "SettingsPanel.h"
#include "Theme.h"
#include "AltShortcutKeys.h"

namespace rtr
{

SettingsPanel::SettingsPanel (ReTrektorAudioProcessor& processor)
    : proc (processor), song (processor.getSong())
{
    setOpaque (true);

    // the panel takes the keyboard focus on its own tab, so a press walks here
    // rather than onto whichever control the pointer touched last: a Button holds
    // that focus by default and answers the Return key, which is what made an
    // Enter that only confirms a typed value flip the checkbox near it
    setWantsKeyboardFocus (true);

    auto refresh = [this]
    {
        if (onSettingsChanged)
            onSettingsChanged();
    };

    // the row that used to switch the values between hex and decimal is gone
    // altogether: every number the plug-in shows and reads is decimal now, so the
    // panel has nothing to offer for a spelling a host could second-guess
    midRowToggle = addToggle ("Ignore mid-row start", "When the transport starts in the middle of a row, skip that row's events",
                              song.settings.midRowIgnored,
                              [this, refresh] (bool v)
                              {
                                  song.settings.midRowIgnored = v;
                                  refresh();
                              });

    subticksSlider = addSlider ("Subticks per row", "Internal resolution used by command interpolation",
                                1.0, 32.0, 1.0, (double) song.settings.subticksPerRow,
                                [this, refresh] (double v)
                                {
                                    song.settings.subticksPerRow = (int) std::lround (v);
                                    refresh();
                                });

    rowsPerBeatSlider = addSlider ("Rows per beat", "Tracker rows per quarter note",
                                   1.0, 16.0, 1.0, (double) song.settings.rowsPerBeat,
                                   [this, refresh] (double v)
                                   {
                                       song.settings.rowsPerBeat = (int) std::lround (v);
                                       refresh();
                                   });

    defaultRowsSlider = addSlider ("New pattern length (rows)", "Row count used for newly created patterns (1-1024)",
                                   1.0, (double) kMaxRows, 1.0, (double) song.settings.defaultPatternRows,
                                   [this, refresh] (double v)
                                   {
                                       song.settings.defaultPatternRows = (int) std::lround (v);
                                       refresh();
                                   });

    snapToggle = addToggle ("Snap to grid", "Enable snapping of timeline edits and paste",
                            song.settings.snapEnabled,
                            [this, refresh] (bool v)
                            {
                                song.settings.snapEnabled = v;
                                refresh();
                            });

    adaptiveToggle = addToggle ("Adaptive grid", "Grid follows zoom (1, 2, 4, 8 ... rows)",
                                song.settings.adaptiveGrid,
                                [this, refresh] (bool v)
                                {
                                    song.settings.adaptiveGrid = v;
                                    refresh();
                                });

    fixedGridSlider = addSlider ("Fixed grid size (rows)", "Grid step used when adaptive grid is disabled",
                                 1.0, 64.0, 1.0, (double) song.settings.fixedGridRows,
                                 [this, refresh] (double v)
                                 {
                                     song.settings.fixedGridRows = (int) std::lround (v);
                                     refresh();
                                 });

    octaveSlider = addSlider ("Record octave", "Base octave for computer keyboard note entry",
                              0.0, 9.0, 1.0, (double) song.settings.recordOctave,
                              [this, refresh] (double v)
                              {
                                  song.settings.recordOctave = (int) std::lround (v);
                                  refresh();
                              });

    stepSkipSlider = addSlider ("Step skip", "Rows to advance after entering a value",
                                0.0, 16.0, 1.0, (double) song.settings.stepSkip,
                                [this, refresh] (double v)
                                {
                                    song.settings.stepSkip = (int) std::lround (v);
                                    refresh();
                                });

    undoStepsSlider = addSlider ("Undo steps", "Model states Ctrl+Z can walk back (0 turns undoing off)",
                                 0.0, (double) kMaxUndoSteps, 1.0, (double) song.settings.undoSteps,
                                 [this, refresh] (double v)
                                 {
                                     song.settings.undoSteps = (int) std::lround (v);
                                     refresh();
                                 });

    volumeCcSlider = addSlider ("Volume column CC", "Controller driven by the volume column (0-127, CC7 is channel volume)",
                                0.0, 127.0, 1.0, (double) song.settings.volumeCc,
                                [this, refresh] (double v)
                                {
                                    song.settings.volumeCc = (int) std::lround (v);
                                    refresh();
                                });

    panCcSlider = addSlider ("Pan column CC", "Controller driven by the pan column (0-127, CC10 is channel pan)",
                             0.0, 127.0, 1.0, (double) song.settings.panCc,
                             [this, refresh] (double v)
                             {
                                 song.settings.panCc = (int) std::lround (v);
                                 refresh();
                             });
}

//==============================================================================
juce::ToggleButton* SettingsPanel::addToggle (const juce::String& label, const juce::String& hint, bool value,
                                             std::function<void (bool)> onChange)
{
    auto toggle = std::make_unique<juce::ToggleButton> (label);

    // a Button takes the keyboard focus and answers the Return key, so a row's
    // checkbox would otherwise hold the focus the pointer left in it and an Enter
    // confirming a value typed into a field would flip that row again; the
    // checkbox keeps its click, it only stops being the thing a Return hits
    toggle->setWantsKeyboardFocus (false);
    toggle->setToggleState (value, juce::dontSendNotification);
    toggle->setColour (juce::ToggleButton::textColourId, theme::text);
    toggle->setColour (juce::ToggleButton::tickColourId, theme::accent);
    toggle->setColour (juce::ToggleButton::tickDisabledColourId, theme::textDim);

    auto* raw = toggle.get();
    raw->onClick = [raw, cb = std::move (onChange)] { cb (raw->getToggleState()); };

    addAndMakeVisible (*raw);
    owned.push_back (std::move (toggle));
    rows.push_back ({ label, hint, raw, nullptr });
    return raw;
}

juce::Slider* SettingsPanel::addSlider (const juce::String& label, const juce::String& hint,
                                       double min, double max, double interval, double value,
                                       std::function<void (double)> onChange)
{
    auto slider = std::make_unique<juce::Slider> (juce::Slider::IncDecButtons, juce::Slider::TextBoxLeft);

    // the knob keeps the keyboard focus rather than its own +/- buttons: the
    // Slider is what holds the text box a value is typed into, and it asks its
    // look and feel for the buttons, which take the focus away. A Button answers
    // the Return key while it holds the focus, so the Enter that confirms a typed
    // value would otherwise move the knob the pointer left last
    slider->setWantsKeyboardFocus (true);

    // a Slider in this style has no thumb to grab: the area beside its text box is
    // the pair of +/- buttons it asks the look and feel for, and those buttons
    // answer a click only until the drag is asked for, which leaves a sideways
    // drag over the row doing nothing at all, the value staying on whatever the
    // last click left it at. Asking for the horizontal drag puts a drag on that
    // pair, and on the row's own width, onto the value: the number moves with the
    // pointer, the button the drag runs towards lights up, and a click still costs
    // one step of the interval, so the row keeps both ways of moving it
    slider->setIncDecButtonsMode (juce::Slider::incDecButtonsDraggable_Horizontal);
    slider->setRange (min, max, interval);
    slider->setValue (value, juce::dontSendNotification);
    slider->setTextBoxStyle (juce::Slider::TextBoxLeft, false, 52, 20);
    slider->setColour (juce::Slider::textBoxTextColourId, theme::text);
    slider->setColour (juce::Slider::textBoxBackgroundColourId, theme::panelLight);
    slider->setColour (juce::Slider::textBoxOutlineColourId, theme::outline);
    slider->setColour (juce::Slider::backgroundColourId, theme::panelLight);

    auto* raw = slider.get();
    raw->onValueChange = [raw, cb = std::move (onChange)] { cb (raw->getValue()); };

    // the drag along the row is one action of the undo line rather than one
    // point per notification: the Slider answers every mouse event its host
    // moves along the row with its own value notification, and each of those
    // reaches this row and, through it, the editor's change of the model, so a
    // drag that stepped over eight numbers used to earn eight undo points and
    // one Ctrl+Z left the number still three numbers along the drag rather
    // than off it. The undo line hears the drag's two ends here and keeps one
    // point of its own for the whole gesture, which is what the row's own undo
    // steps then cap the reach of
    raw->onDragStart = [this] { proc.beginKnobDrag(); };
    raw->onDragEnd = [this] { proc.endKnobDrag(); };

    addAndMakeVisible (*raw);
    owned.push_back (std::move (slider));
    rows.push_back ({ label, hint, nullptr, raw });
    return raw;
}

bool SettingsPanel::keyPressed (const juce::KeyPress& key)
{
    // the settings panel answers the Alt pair rather than handing it up, because
    // a key press goes to the modal component and only walks up from there:
    // while a pattern's context menu was opened over this panel, this panel is
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
    if (answerSliceKey (key, sliceStep))
        return true;

    if (key.getKeyCode() == juce::KeyPress::returnKey)
    {
        // every field on this tab types into a text box of its own, which answers
        // the Enter by committing the value it was building; the panel has nothing
        // of its own to close, so it spends the press here rather than handing it
        // up, which is what keeps a confirming Enter from reaching the editor's
        // shortcuts and, above all, from firing a checkbox the pointer left last
        return true;
    }

    return false;
}

void SettingsPanel::refresh()
{
    if (midRowToggle != nullptr)
    {
        midRowToggle->setToggleState (song.settings.midRowIgnored, juce::dontSendNotification);
        subticksSlider->setValue ((double) song.settings.subticksPerRow, juce::dontSendNotification);
        rowsPerBeatSlider->setValue ((double) song.settings.rowsPerBeat, juce::dontSendNotification);
        defaultRowsSlider->setValue ((double) song.settings.defaultPatternRows, juce::dontSendNotification);
        snapToggle->setToggleState (song.settings.snapEnabled, juce::dontSendNotification);
        adaptiveToggle->setToggleState (song.settings.adaptiveGrid, juce::dontSendNotification);
        fixedGridSlider->setValue ((double) song.settings.fixedGridRows, juce::dontSendNotification);
        octaveSlider->setValue ((double) song.settings.recordOctave, juce::dontSendNotification);
        stepSkipSlider->setValue ((double) song.settings.stepSkip, juce::dontSendNotification);
        undoStepsSlider->setValue ((double) song.settings.undoSteps, juce::dontSendNotification);
        volumeCcSlider->setValue ((double) song.settings.volumeCc, juce::dontSendNotification);
        panCcSlider->setValue ((double) song.settings.panCc, juce::dontSendNotification);
    }

    repaint();
}

//==============================================================================
int SettingsPanel::rowH() const
{
    const int count = juce::jmax (1, (int) rows.size());
    return juce::jlimit (26, 40, (getHeight() - 56) / count);
}

void SettingsPanel::paint (juce::Graphics& g)
{
    g.fillAll (theme::background);

    g.setColour (theme::text);
    g.setFont (theme::uiFont (15.0f, true));
    g.drawText ("Settings", 20, 12, 300, 22, juce::Justification::centredLeft, false);

    g.setColour (theme::textDim);
    g.setFont (theme::monoFont (10.5f));

    const int rh = rowH();
    const bool showHints = rh >= 34;
    int y = 48;

    for (auto& r : rows)
    {
        if (r.toggle == nullptr)
        {
            g.setColour (theme::text);
            g.setFont (theme::monoFont (12.0f));
            g.drawText (r.label, 20, y + 1, juce::jmax (100, getWidth() - 320), 18,
                        juce::Justification::centredLeft, false);
        }

        g.setColour (theme::outline.withAlpha (0.4f));
        g.drawHorizontalLine (y + rh - 3, 20.0f, (float) getWidth() - 20.0f);

        if (showHints)
        {
            g.setColour (theme::textDim);
            g.setFont (theme::monoFont (10.0f));
            g.drawText (r.hint, 20, y + 21, juce::jmax (100, getWidth() - 320), 13,
                        juce::Justification::centredLeft, false);
        }

        y += rh;
    }
}

void SettingsPanel::resized()
{
    const int rh = rowH();
    int y = 48;

    for (auto& r : rows)
    {
        auto area = juce::Rectangle<int> (20, y, getWidth() - 40, juce::jmin (21, rh - 4));

        if (r.toggle != nullptr)
        {
            r.toggle->setBounds (area);
        }
        else if (r.slider != nullptr)
        {
            r.slider->setBounds (area.removeFromRight (150));
        }

        y += rh;
    }
}

} // namespace rtr
