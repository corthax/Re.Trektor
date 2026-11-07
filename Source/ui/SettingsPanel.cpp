#include "SettingsPanel.h"
#include "Theme.h"
#include "AltShortcutKeys.h"

namespace rtr
{

SettingsPanel::SettingsPanel (ReTrektorAudioProcessor& processor)
    : proc (processor), song (processor.getSong())
{
    setOpaque (true);

    auto refresh = [this]
    {
        if (onSettingsChanged)
            onSettingsChanged();
    };

    hexToggle = addToggle ("Hexadecimal values", "Show values as hex instead of decimal", song.settings.hexDisplay,
                           [this, refresh] (bool v)
                           {
                               song.settings.hexDisplay = v;
                               refresh();
                           });

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

    pasteUniqueToggle = addToggle ("Paste as unique pattern by default", "Ctrl+V creates a copy of the pattern data",
                                   song.settings.pasteAsUnique,
                                   [this, refresh] (bool v)
                                   {
                                       song.settings.pasteAsUnique = v;
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
    slider->setRange (min, max, interval);
    slider->setValue (value, juce::dontSendNotification);
    slider->setTextBoxStyle (juce::Slider::TextBoxLeft, false, 52, 20);
    slider->setColour (juce::Slider::textBoxTextColourId, theme::text);
    slider->setColour (juce::Slider::textBoxBackgroundColourId, theme::panelLight);
    slider->setColour (juce::Slider::textBoxOutlineColourId, theme::outline);
    slider->setColour (juce::Slider::backgroundColourId, theme::panelLight);

    auto* raw = slider.get();
    raw->onValueChange = [raw, cb = std::move (onChange)] { cb (raw->getValue()); };

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
    return answerClipboardKey (key, copyStep, pasteStep, cutStep);
}

void SettingsPanel::refresh()
{
    if (hexToggle != nullptr)
    {
        hexToggle->setToggleState (song.settings.hexDisplay, juce::dontSendNotification);
        midRowToggle->setToggleState (song.settings.midRowIgnored, juce::dontSendNotification);
        subticksSlider->setValue ((double) song.settings.subticksPerRow, juce::dontSendNotification);
        rowsPerBeatSlider->setValue ((double) song.settings.rowsPerBeat, juce::dontSendNotification);
        defaultRowsSlider->setValue ((double) song.settings.defaultPatternRows, juce::dontSendNotification);
        pasteUniqueToggle->setToggleState (song.settings.pasteAsUnique, juce::dontSendNotification);
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
