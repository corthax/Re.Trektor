#include "PluginEditor.h"
#include "ui/AltShortcutKeys.h"

using namespace rtr;

//==============================================================================
ReTrektorAudioProcessorEditor::ReTrektorAudioProcessorEditor (ReTrektorAudioProcessor& p)
    : AudioProcessorEditor (&p),
      proc (p),
      song (p.getSong()),
      timeline (p),
      tracker (p),
      pool (p),
      tools (p),
      settings (p),
      macros (p)
{
    setLookAndFeel (&lookAndFeel);

    // read the remembered size before setResizeLimits, which resizes the fresh
    // (still zero-sized) editor to its own minimum and thereby records that
    // minimum as the size of this editor
    const auto remembered = proc.getRememberedEditorBounds();

    setResizable (true, true);
    setResizeLimits (900, 560, 2600, 1600);

    // hosts such as Reaper close a floating plug-in window by throwing it away
    // and ask the plug-in for a brand new editor when it is opened again, so
    // start from the size the closed editor had rather than the built-in one
    if (remembered.getWidth() > 0 && remembered.getHeight() > 0)
        setBoundsConstrained (remembered.withZeroOrigin());
    else
        setSize (1240, 760);

    //== tabs =================================================================
    for (auto* b : { &trackerTab, &macrosTab, &settingsTab })
    {
        b->setClickingTogglesState (false);
        b->setColour (juce::TextButton::buttonColourId, theme::panel);
        b->setColour (juce::TextButton::buttonOnColourId, theme::selection);
        addAndMakeVisible (*b);
    }

    trackerTab.onClick = [this] { setTab (Tab::tracker); };
    macrosTab.onClick = [this] { setTab (Tab::macros); };
    settingsTab.onClick = [this] { setTab (Tab::settings); };

    //== grid controls ========================================================
    snapButton.setColour (juce::ToggleButton::textColourId, theme::text);
    snapButton.setColour (juce::ToggleButton::tickColourId, theme::accent);
    snapButton.onClick = [this]
    {
        if (syncingUi)
            return;

        song.settings.snapEnabled = snapButton.getToggleState();
        settings.refresh();
        commitEdit();
    };
    addAndMakeVisible (snapButton);

    gridBox.addItem ("Adaptive", 1);
    gridBox.addItem ("1 row", 2);
    gridBox.addItem ("2 rows", 3);
    gridBox.addItem ("4 rows", 4);
    gridBox.addItem ("8 rows", 5);
    gridBox.addItem ("16 rows", 6);
    gridBox.addItem ("32 rows", 7);
    gridBox.addItem ("64 rows", 8);
    gridBox.setSelectedId (1, juce::dontSendNotification);
    gridBox.onChange = [this]
    {
        if (syncingUi)
            return;

        const int id = gridBox.getSelectedId();
        song.settings.adaptiveGrid = id == 1;
        song.settings.snapEnabled = true;
        snapButton.setToggleState (true, juce::dontSendNotification);

        if (id > 1)
            song.settings.fixedGridRows = 1 << (id - 2);

        settings.refresh();
        commitEdit();
    };
    addAndMakeVisible (gridBox);

    zoomOutButton.onClick = [this] { timeline.zoomBy (1.0 / 1.3, song.editPlayheadBeats); };
    zoomInButton.onClick = [this] { timeline.zoomBy (1.3, song.editPlayheadBeats); };
    addAndMakeVisible (zoomOutButton);
    addAndMakeVisible (zoomInButton);

    positionLabel.setFont (theme::monoFont (11.5f));
    positionLabel.setColour (juce::Label::textColourId, theme::textDim);
    positionLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (positionLabel);

    //== panels ===============================================================
    addAndMakeVisible (timeline);
    addAndMakeVisible (tracker);
    addAndMakeVisible (pool);
    addAndMakeVisible (tools);
    addChildComponent (settings);
    addChildComponent (macros);

    // every model change ends in commitEdit(), which records the state the
    // model reached as the newest point of the undo line before refreshing, so
    // Ctrl+Z walks the actions back in the order they happened. The point is
    // recorded here rather than inside refreshAll(), which tab switches and
    // layout also call, and the pattern selection gets its own inside
    // selectPattern() because moving the active pattern moves the saved song
    timeline.onModelChanged = [this] { commitEdit(); };
    timeline.onPatternSelected = [this] (int id) { selectPattern (id); };

    tracker.onModelChanged = [this] { commitEdit(); };
    tracker.onPatternEdited = [this] { timeline.refresh(); };

    pool.onPatternSelected = [this] (int id) { selectPattern (id); };
    pool.onModelChanged = [this] { commitEdit(); };
    pool.snapBeat = [this] (double beat) { return timeline.snapBeat (beat); };

    // the tools ask the tracker per cell which ones it holds, so a selection can
    // be any shape and one column of one row is as reachable as a whole pattern
    tools.isCellSelected = [this] (int row, int ch) { return tracker.isCellSelected (row, ch); };
    tools.getCursorChannel = [this] { return tracker.getCursorChannel(); };
    tools.onModelChanged = [this] { commitEdit(); };

    settings.onSettingsChanged = [this]
    {
        applySettingsToModel();
        commitEdit();
    };

    macros.onModelChanged = [this] { commitEdit(); };

    // every panel answers the Alt pair of undo and redo with the editor's own
    // two steps rather than with a copy of its own: a key press is sent to the
    // modal component and only walks up from there, so while a right-click menu
    // is open or a pattern is being dragged from the pool, the press is answered
    // by the panel the menu or drag belongs to and the editor is never
    // consulted. A pair the editor alone owned was swallowed on those paths, and
    // which panel holds the press is not something the plug-in can predict. The
    // same pair is what the tracker's Undo / Redo menu items go through rather
    // than calling the plug-in directly, so the step refreshes the whole view
    // and, unlike a model change, records no point of its own
    auto wireUndoRedo = [this] (auto& panel)
    {
        panel.undoStep = [this] { return performUndo(); };
        panel.redoStep = [this] { return performRedo(); };
    };

    wireUndoRedo (tracker);
    wireUndoRedo (timeline);
    wireUndoRedo (pool);
    wireUndoRedo (tools);
    wireUndoRedo (settings);
    wireUndoRedo (macros);

    // the cell clipboard's Alt spellings need the same treatment for the same
    // reason: the hosts answer Ctrl+C, Ctrl+V and Ctrl+X with their own clipboard
    // and cut the way they answer the undo trio with their own history, so the
    // plug-in is never handed those events and the cells it copies, pastes and
    // cuts are unreachable there. Alt is what those hosts leave free, so Alt+C
    // copies what the tracker is holding selected, Alt+V pastes it at the cursor
    // with everything, which is the plain spelling's meaning rather than one
    // field at a time, and Alt+X cuts, which is that copy followed by emptying
    // exactly the cells it took, so a cut and a following paste move them. The
    // steps are injected rather than reached through the panels, so all six walk
    // the one clipboard the Ctrl pair and the tracker's menu use, and a paste or
    // a cut that changed the model records its point of the undo line through the
    // tracker's own change and refreshes the whole view
    auto wireClipboard = [this] (auto& panel)
    {
        panel.copyStep = [this] { tracker.copySelectionToClipboard(); };
        panel.pasteStep = [this] { tracker.pasteClipboardAtCursor (kPasteAll); };
        panel.cutStep = [this] { tracker.cutSelectionToClipboard(); };
    };

    wireClipboard (tracker);
    wireClipboard (timeline);
    wireClipboard (pool);
    wireClipboard (tools);
    wireClipboard (settings);
    wireClipboard (macros);

    // the Alt slice at the playhead, injected into every panel the way the undo
    // pair and the clipboard's steps are: a press during a menu or a pool drag
    // belongs to that panel and never reaches the editor, and its Ctrl+X spelling
    // was spent by the host rather than by the plug-in, so the Alt letter is the
    // one route the slice has besides the timeline's own menu. The step is the
    // editor's rather than the timeline's so the slice still commits through the
    // one funnel that records the undo point and refreshes all six views
    auto wireSlice = [this] (auto& panel)
    {
        panel.sliceStep = [this] { doSlice(); };
    };

    wireSlice (tracker);
    wireSlice (timeline);
    wireSlice (pool);
    wireSlice (tools);
    wireSlice (settings);
    wireSlice (macros);

    // the other half of what those paths leave behind: a context menu's popup
    // and a pattern the pool started dragging are windows with no parent
    // component of their own, and closing one takes the keyboard focus with it,
    // so afterwards nothing is focused at all. JUCE sends a key press to the
    // focused component and walks up from there, so with nothing focused the
    // walk starts at the plug-in's top component and only the editor's own
    // shortcuts are answered, while the note entry, cursor moves and clears the
    // tracker owns go unheard until the mouse is clicked again. JUCE's own
    // hand-back after a menu re-grabs focus on whatever still holds it, which is
    // nothing in that case, so the panels that own a menu, an alert or a drag
    // hand it back themselves, and to the tracker rather than to themselves,
    // since focus resting on a panel would leave the tracker, the only note
    // entry there is, deaf the same way
    auto wireFocusRescue = [this] (auto& panel)
    {
        panel.focusRescue = [this, &panel]
        {
            // the tracker is what has to hear keys, so it is the focus to hand
            // back; where it is hidden, which is the Settings and Macros tabs,
            // the panel takes it instead so a press still walks up to the
            // editor rather than stopping at nothing focused at all
            if (tracker.isShowing())
                tracker.grabKeyboardFocus();
            else
                panel.grabKeyboardFocus();
        };
    };

    wireFocusRescue (tracker);
    wireFocusRescue (timeline);
    wireFocusRescue (pool);

    //== bottom bar ===========================================================
    auto setupBottomSlider = [this] (juce::Slider& s, juce::Label& label, const juce::String& text,
                                     double min, double max, double value)
    {
        label.setText (text, juce::dontSendNotification);
        label.setFont (theme::monoFont (10.5f));
        label.setColour (juce::Label::textColourId, theme::textDim);
        label.setJustificationType (juce::Justification::centredRight);
        addAndMakeVisible (label);

        s.setSliderStyle (juce::Slider::IncDecButtons);
        s.setRange (min, max, 1.0);
        s.setValue (value, juce::dontSendNotification);
        s.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 46, 18);
        s.setColour (juce::Slider::textBoxTextColourId, theme::text);
        s.setColour (juce::Slider::textBoxBackgroundColourId, theme::panelLight);
        s.setColour (juce::Slider::textBoxOutlineColourId, theme::outline);
        addAndMakeVisible (s);
    };

    setupBottomSlider (octaveSlider, octaveLabel, "OCT", 0.0, 9.0, (double) song.settings.recordOctave);
    setupBottomSlider (stepSlider, stepLabel, "STEP", 0.0, 16.0, (double) song.settings.stepSkip);
    setupBottomSlider (channelsSlider, channelsLabel, "CHANNELS", 1.0, (double) kMaxTrackerChannels, 1.0);

    octaveSlider.onValueChange = [this]
    {
        if (syncingUi)
            return;

        song.settings.recordOctave = (int) octaveSlider.getValue();

        // the bottom bar moves the model without going through a component's
        // callback, so the point it changes has to be recorded here too
        proc.pushUndoPoint();
        settings.refresh();
    };

    stepSlider.onValueChange = [this]
    {
        if (syncingUi)
            return;

        song.settings.stepSkip = (int) stepSlider.getValue();
        proc.pushUndoPoint();
        settings.refresh();
    };

    channelsSlider.onValueChange = [this]
    {
        if (syncingUi)
            return;

        if (auto* p = song.activePattern())
        {
            const int ch = (int) channelsSlider.getValue();
            p->setChannels (ch);

            // The channel limit is the constant kMaxTrackerChannels, and shrinking
            // keeps the cells of the removed columns in the pattern's capacity, so
            // widening it again restores them. The bottom bar changes the model
            // without going through a component's callback, so like the OCT and
            // STEP sliders it records the point it reached itself.
            proc.pushUndoPoint();
            proc.syncPlaybackModel();
            tracker.refresh();
            timeline.refresh();
            pool.refresh();
            settings.refresh();
        }
    };

    nameEditor.setFont (theme::monoFont (12.0f));
    nameEditor.setColour (juce::TextEditor::backgroundColourId, theme::panelLight);
    nameEditor.setColour (juce::TextEditor::textColourId, theme::text);
    nameEditor.setColour (juce::TextEditor::outlineColourId, theme::outline);
    nameEditor.setIndents (4, 2);
    nameEditor.onReturnKey = [this]
    {
        if (auto* p = song.activePattern())
        {
            p->name = nameEditor.getText();

            if (p->name.isEmpty())
                p->name = "Pattern";

            commitEdit();
        }

        tracker.grabKeyboardFocus();
    };
    nameEditor.onFocusLost = [this] { nameEditor.onReturnKey(); };
    addAndMakeVisible (nameEditor);

    gridInfoLabel.setFont (theme::monoFont (10.0f));
    gridInfoLabel.setColour (juce::Label::textColourId, theme::textDim);
    gridInfoLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (gridInfoLabel);

    setTab (Tab::tracker);
    syncBottomBar();
    syncGridControls();

    proc.onSongChanged = [this] { juce::MessageManager::callAsync ([this] { refreshAll(); }); };

    startTimerHz (20);
}

ReTrektorAudioProcessorEditor::~ReTrektorAudioProcessorEditor()
{
    proc.onSongChanged = nullptr;
    setLookAndFeel (nullptr);
}

//==============================================================================
void ReTrektorAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (theme::background);

    auto top = getLocalBounds().removeFromTop (38);
    g.setColour (theme::panel);
    g.fillRect (top);
    g.setColour (theme::outline);
    g.drawHorizontalLine (top.getBottom() - 1, 0.0f, (float) getWidth());

    auto bottom = getLocalBounds().removeFromBottom (34);
    g.setColour (theme::panel);
    g.fillRect (bottom);
    g.setColour (theme::outline);
    g.drawHorizontalLine (bottom.getY(), 0.0f, (float) getWidth());

    // separator under the timeline
    if (currentTab == Tab::tracker)
    {
        auto center = getLocalBounds().withTrimmedTop (38).withTrimmedBottom (34);
        center.removeFromLeft (176);
        center.removeFromRight (156);
        auto tl = center.removeFromTop (150);
        g.setColour (theme::outline);
        g.drawHorizontalLine (tl.getBottom(), (float) tl.getX(), (float) tl.getRight());
    }
}

void ReTrektorAudioProcessorEditor::resized()
{
    // the size the host gave this GUI is the size to come back to when the host
    // destroys it and asks for another editor, so record it on every layout
    proc.rememberEditorBounds (getLocalBounds());

    auto area = getLocalBounds();

    //== top bar ==============================================================
    auto top = area.removeFromTop (38).reduced (6, 5);
    trackerTab.setBounds (top.removeFromLeft (86));
    macrosTab.setBounds (top.removeFromLeft (86));
    settingsTab.setBounds (top.removeFromLeft (86));

    top.removeFromLeft (12);

    auto snapArea = top.removeFromLeft (66);
    snapButton.setBounds (snapArea);

    top.removeFromLeft (4);
    gridBox.setBounds (top.removeFromLeft (110));

    top.removeFromLeft (8);
    zoomOutButton.setBounds (top.removeFromLeft (26));
    zoomInButton.setBounds (top.removeFromLeft (26));

    positionLabel.setBounds (top.removeFromRight (260));

    //== bottom bar ===========================================================
    auto bottom = area.removeFromBottom (34).reduced (6, 5);

    channelsLabel.setBounds (bottom.removeFromRight (70));
    channelsSlider.setBounds (bottom.removeFromRight (96));
    bottom.removeFromRight (10);

    auto nameArea = bottom.removeFromRight (180);
    nameEditor.setBounds (nameArea.withTrimmedTop (1).withTrimmedBottom (1));

    gridInfoLabel.setBounds (bottom.removeFromRight (200));

    stepLabel.setBounds (bottom.removeFromLeft (36));
    stepSlider.setBounds (bottom.removeFromLeft (86));
    bottom.removeFromLeft (8);
    octaveLabel.setBounds (bottom.removeFromLeft (34));
    octaveSlider.setBounds (bottom.removeFromLeft (86));

    //== right tools ==========================================================
    tools.setBounds (area.removeFromRight (156));

    //== left pool ============================================================
    pool.setBounds (area.removeFromLeft (176));

    //== center ===============================================================
    if (currentTab == Tab::tracker)
    {
        auto tl = area.removeFromTop (150);
        timeline.setBounds (tl);
        tracker.setBounds (area);
        tracker.setVisible (true);
        timeline.setVisible (true);
        settings.setVisible (false);
        macros.setVisible (false);
    }
    else
    {
        timeline.setVisible (false);
        tracker.setVisible (false);
        settings.setVisible (currentTab == Tab::settings);
        macros.setVisible (currentTab == Tab::macros);

        if (currentTab == Tab::settings)
            settings.setBounds (area);
        else
            macros.setBounds (area);
    }
}

//==============================================================================
void ReTrektorAudioProcessorEditor::setTab (Tab tab)
{
    currentTab = tab;

    trackerTab.setToggleState (tab == Tab::tracker, juce::dontSendNotification);
    macrosTab.setToggleState (tab == Tab::macros, juce::dontSendNotification);
    settingsTab.setToggleState (tab == Tab::settings, juce::dontSendNotification);

    for (auto* b : { &trackerTab, &macrosTab, &settingsTab })
        b->setColour (juce::TextButton::buttonColourId, theme::panel);

    auto* active = tab == Tab::tracker ? &trackerTab : (tab == Tab::macros ? &macrosTab : &settingsTab);
    active->setColour (juce::TextButton::buttonColourId, theme::selection);

    resized();
    repaint();

    if (tab == Tab::tracker)
        tracker.grabKeyboardFocus();
}

void ReTrektorAudioProcessorEditor::refreshAll()
{
    proc.syncPlaybackModel();
    syncBottomBar();
    syncGridControls();
    tracker.refresh();
    timeline.refresh();
    pool.refresh();
    repaint();
}

void ReTrektorAudioProcessorEditor::commitEdit()
{
    proc.pushUndoPoint();
    refreshAll();
}

bool ReTrektorAudioProcessorEditor::performUndo()
{
    if (! proc.undo())
        return false;

    refreshAll();

    // the Settings tab's rows keep their own values rather than reading them
    // off the model as the other panels do, so an undo that moved a setting has
    // to push the row back or the tab would still show the setting that was
    // undone
    settings.refresh();
    return true;
}

bool ReTrektorAudioProcessorEditor::performRedo()
{
    if (! proc.redo())
        return false;

    refreshAll();
    settings.refresh();
    return true;
}

void ReTrektorAudioProcessorEditor::selectPattern (int patternId)
{
    song.activePatternId = patternId;

    for (auto& clip : song.clips)
    {
        if (clip.patternId == patternId)
            break;
    }

    // moving which pattern is the active one moves the saved song, so it is an
    // undo point of its own: an edit made in a freshly selected pattern that
    // rewound to the pattern that was active before would read as a jump
    proc.pushUndoPoint();

    syncBottomBar();
    tracker.refresh();
    timeline.refresh();
    pool.refresh();

    if (currentTab == Tab::tracker)
        tracker.grabKeyboardFocus();
}

void ReTrektorAudioProcessorEditor::syncBottomBar()
{
    syncingUi = true;

    auto* p = song.activePattern();
    channelsSlider.setRange (1.0, (double) kMaxTrackerChannels, 1.0);
    channelsSlider.setValue (p != nullptr ? (double) p->channels : 1.0, juce::dontSendNotification);
    octaveSlider.setValue ((double) song.settings.recordOctave, juce::dontSendNotification);
    stepSlider.setValue ((double) song.settings.stepSkip, juce::dontSendNotification);

    if (p != nullptr && ! nameEditor.hasKeyboardFocus (true))
        nameEditor.setText (p->name, false);

    juce::String info;

    if (p != nullptr)
        info << p->id << " · " << p->length << " rows · " << p->channels << " ch";

    gridInfoLabel.setText (info, juce::dontSendNotification);

    syncingUi = false;
}

void ReTrektorAudioProcessorEditor::syncGridControls()
{
    syncingUi = true;
    snapButton.setToggleState (song.settings.snapEnabled, juce::dontSendNotification);

    if (song.settings.adaptiveGrid)
    {
        gridBox.setSelectedId (1, juce::dontSendNotification);
    }
    else
    {
        const int rows = song.settings.fixedGridRows;

        if (rows == 1) gridBox.setSelectedId (2, juce::dontSendNotification);
        else if (rows == 2) gridBox.setSelectedId (3, juce::dontSendNotification);
        else if (rows == 4) gridBox.setSelectedId (4, juce::dontSendNotification);
        else if (rows == 8) gridBox.setSelectedId (5, juce::dontSendNotification);
        else if (rows == 16) gridBox.setSelectedId (6, juce::dontSendNotification);
        else if (rows == 32) gridBox.setSelectedId (7, juce::dontSendNotification);
        else if (rows == 64) gridBox.setSelectedId (8, juce::dontSendNotification);
    }

    syncingUi = false;
}

void ReTrektorAudioProcessorEditor::applySettingsToModel()
{
    // settings no longer clamp the channel count (the limit is the constant
    // kMaxTrackerChannels), the bottom bar just re-reads the current model
    syncBottomBar();
}

//==============================================================================
void ReTrektorAudioProcessorEditor::timerCallback()
{
    updatePositionLabel();
    followHostTransport();
}

//==============================================================================
void ReTrektorAudioProcessorEditor::followHostTransport()
{
    // The plugin playhead follows the host transport, but only while the host
    // position is actually moving. A click on the timeline parks it at the
    // clicked beat (see TimelineComponent::jumpPlayheadToClickedX) so pasting
    // happens where the mouse was, and a host that sits stopped must not drag
    // the parked playhead back to the transport position.
    const auto transport = proc.getTransport();

    if (transport.ppq >= 0.0
        && (transport.playing || std::abs (transport.ppq - song.transportMirrorBeats) > 1.0e-6))
    {
        song.editPlayheadBeats = transport.ppq;
        song.transportMirrorBeats = transport.ppq;
        timeline.repaint();
    }
}

void ReTrektorAudioProcessorEditor::updatePositionLabel()
{
    const auto t = proc.getTransport();
    juce::String s;

    const double ppqPerBar = t.ppqPerBar > 0.0 ? t.ppqPerBar : 4.0;
    const int bar = (int) std::floor (t.ppq / ppqPerBar) + 1;
    const int beat = (int) std::floor (std::fmod (t.ppq, ppqPerBar)) + 1;

    s << (t.playing ? "PLAY" : "STOP") << "   "
      << juce::String (bar).paddedLeft ('0', 3) << " : " << juce::String (beat)
      << "   " << juce::String (t.bpm, 2) << " BPM";

    positionLabel.setText (s, juce::dontSendNotification);
}

//==============================================================================
bool ReTrektorAudioProcessorEditor::keyPressed (const juce::KeyPress& key)
{
    const bool ctrl = key.getModifiers().isCommandDown() || key.getModifiers().isCtrlDown();

    // Ctrl+C and Ctrl+V are the cell clipboard: they copy the selected cells and
    // paste them at the tracker cursor with everything. The pattern clipboard has
    // no keyboard spelling of its own any more, since the Ctrl+Shift pair spent on
    // it was found unreachable in the hosts the same way the plain Ctrl trio is.
    // That pair is refused rather than left to read as the plain one, so a host
    // that spends Ctrl+Shift+C or Ctrl+Shift+V on its own duplication keeps its
    // meaning, and copying or pasting a whole pattern is only what the timeline's
    // and the pool's menus do
    if (ctrl && ! key.getModifiers().isShiftDown() && key.getKeyCode() == 'C')
    {
        tracker.copySelectionToClipboard();
        return true;
    }

    if (ctrl && ! key.getModifiers().isShiftDown() && key.getKeyCode() == 'V')
    {
        tracker.pasteClipboardAtCursor (kPasteAll);
        return true;
    }

    // undo and redo walk the undo line the model changes append: Ctrl+Z steps
    // back one point and Ctrl+Y or Ctrl+Shift+Z steps forward again. They reach
    // the editor rather than the tracker, which lets a Ctrl or Cmd modifier
    // through its keyboard handler untouched, and both work while the host is
    // playing: the rows after the playhead play the restored data, while what
    // was already sent to the host cannot be taken back.
    // While a numeric edit buffer is half typed the press only closes that
    // buffer, the way Esc does, so the same press never both forgets the digits
    // and rewinds the model
    if (ctrl && key.getModifiers().isShiftDown() && key.getKeyCode() == 'Z')
    {
        performRedo();
        return true;
    }

    if (ctrl && key.getKeyCode() == 'Z')
    {
        if (! tracker.cancelPendingEdit())
            performUndo();

        return true;
    }

    if (ctrl && key.getKeyCode() == 'Y')
    {
        performRedo();
        return true;
    }

    // Alt+Z and Alt+Y are the same two steps for the hosts that take the Ctrl
    // pair for their own undo: Bitwig and Reaper answer Ctrl+Z, Ctrl+Shift+Z
    // and Ctrl+Y themselves, so the plug-in is never handed those events and
    // only the tracker's menu pair is left reachable. Alt is what those hosts
    // leave free, and the letter is matched over both the key code and the
    // printed character because a key held by Alt arrives as the character that
    // press produced rather than as a bare code. The Alt+Shift spellings are
    // left alone rather than spent on a redo alias, which is what the check in
    // the hosts found: Alt+Shift+Z is swallowed on the way to both of them, so
    // answering it with an undo would give a key the user aims at the redo the
    // opposite meaning
    if (key.getModifiers().isAltDown() && ! ctrl
        && ! key.getModifiers().isShiftDown())
    {
        const int code = key.getKeyCode();
        const auto printed = key.getTextCharacter();
        const bool keyZ = code == 'Z' || code == 'z' || printed == 'Z' || printed == 'z';
        const bool keyY = code == 'Y' || code == 'y' || printed == 'Y' || printed == 'y';

        if (keyZ)
        {
            if (! tracker.cancelPendingEdit())
                performUndo();

            return true;
        }

        if (keyY)
        {
            performRedo();
            return true;
        }
    }

    // the cell clipboard's Alt spellings, matched by the shared matcher so the
    // panels and the editor cannot disagree about what the three letters mean:
    // the hosts answer Ctrl+C, Ctrl+V and Ctrl+X with their own clipboard and
    // cut before the plug-in is offered the event, so Alt+C copies what the
    // tracker is holding selected, Alt+V pastes it at the cursor with
    // everything, the plain spelling's meaning, and Alt+X cuts, which is that
    // copy followed by emptying exactly the cells it took. These are the same
    // steps every panel was given, so a press the tracker answered there and one
    // that walked up to here do the same thing. The Ctrl spellings of the copy and
    // paste above keep their meaning, while neither the Ctrl+Shift pair nor the
    // Ctrl+X slice is answered any more: all three belonged to the host rather
    // than to the plug-in, and the slice now reads the Alt+S spelling below
    if (const auto step = matchClipboardKey (key); step != ClipboardKey::none)
    {
        if (step == ClipboardKey::copy)
            tracker.copySelectionToClipboard();
        else if (step == ClipboardKey::cut)
            tracker.cutSelectionToClipboard();
        else
            tracker.pasteClipboardAtCursor (kPasteAll);

        return true;
    }

    // the clip slice at the playhead, Alt+S, answered by the shared matcher so no
    // panel and the editor can disagree about it either: it is the only spelling
    // of the slice left, since Ctrl+X belongs to the host's cut, and it reaches
    // the same doSlice the panels were given, so a press answered here and one
    // answered by a panel split the same clip at the same beat and left the same
    // single point of the undo line behind
    if (answerSliceKey (key, [this] { doSlice(); }))
        return true;

    if (key.getKeyCode() == juce::KeyPress::F5Key)
    {
        setTab (Tab::tracker);
        return true;
    }

    return false;
}

void ReTrektorAudioProcessorEditor::doSlice()
{
    if (proc.getTransport().playing)
        return;

    if (! song.settings.snapEnabled)
        return;

    const double beat = song.snapBeats (song.editPlayheadBeats, timeline.getPixelsPerBeat());
    Clip* target = song.clipAtBeat (beat);

    if (target == nullptr)
        return;

    if (song.sliceClipAt (target->uid, beat))
        commitEdit();
}
