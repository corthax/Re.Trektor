#include "PluginEditor.h"

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
    setResizable (true, true);
    setResizeLimits (900, 560, 2600, 1600);
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
        refreshAll();
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
        refreshAll();
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

    timeline.onModelChanged = [this] { refreshAll(); };
    timeline.onPatternSelected = [this] (int id) { selectPattern (id); };

    tracker.onModelChanged = [this] { refreshAll(); };
    tracker.onPatternEdited = [this] { timeline.refresh(); };

    pool.onPatternSelected = [this] (int id) { selectPattern (id); };
    pool.onModelChanged = [this] { refreshAll(); };
    pool.snapBeat = [this] (double beat) { return timeline.snapBeat (beat); };

    tools.getSelection = [this]() -> std::pair<int, int>
    {
        if (! tracker.hasSelection())
            return { -1, -1 };

        return { tracker.getSelectionStart(), tracker.getSelectionEnd() };
    };
    tools.onModelChanged = [this] { refreshAll(); };

    settings.onSettingsChanged = [this]
    {
        applySettingsToModel();
        refreshAll();
    };

    macros.onModelChanged = [this] { refreshAll(); };

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
        settings.refresh();
    };

    stepSlider.onValueChange = [this]
    {
        if (syncingUi)
            return;

        song.settings.stepSkip = (int) stepSlider.getValue();
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

            // The settings "Max tracker channels" is the cap: new patterns start with
            // it, and it follows the biggest channel count actually in use, so raising
            // a pattern here above the cap raises the cap too (the Settings tab can
            // still lower it, which clamps every pattern down).
            if (song.settings.maxTrackerChannels < ch)
                song.settings.maxTrackerChannels = ch;

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

            refreshAll();
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

void ReTrektorAudioProcessorEditor::selectPattern (int patternId)
{
    song.activePatternId = patternId;

    for (auto& clip : song.clips)
    {
        if (clip.patternId == patternId)
            break;
    }

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
    for (auto& p : song.patterns)
        p.setChannels (juce::jmin (p.channels, song.settings.maxTrackerChannels));

    syncBottomBar();
}

//==============================================================================
void ReTrektorAudioProcessorEditor::timerCallback()
{
    updatePositionLabel();

    // follow the host playhead while stopped (edit playhead tracks the host position)
    const auto transport = proc.getTransport();

    // The plugin playhead always mirrors the host transport, while playing or stopped.
    if (transport.ppq >= 0.0 && std::abs (transport.ppq - song.editPlayheadBeats) > 1.0e-6)
    {
        song.editPlayheadBeats = transport.ppq;
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

    if (ctrl && key.getKeyCode() == 'C')
    {
        doCopy();
        return true;
    }

    if (ctrl && key.getKeyCode() == 'V')
    {
        doPaste (key.getModifiers().isShiftDown());
        return true;
    }

    if (ctrl && key.getKeyCode() == 'X')
    {
        doSlice();
        return true;
    }

    if (key.getKeyCode() == juce::KeyPress::F5Key)
    {
        setTab (Tab::tracker);
        return true;
    }

    return false;
}

void ReTrektorAudioProcessorEditor::doCopy()
{
    if (auto* p = song.activePattern())
    {
        song.copyPattern (p->id);
        pool.refresh();
    }
}

void ReTrektorAudioProcessorEditor::doPaste (bool forceUnique)
{
    if (proc.getTransport().playing)
        return;

    if (! song.clipboard.valid)
        return;

    const bool unique = forceUnique || song.settings.pasteAsUnique;
    const double beat = song.snapBeats (song.editPlayheadBeats, timeline.getPixelsPerBeat());

    if (auto* clip = song.pasteAt (beat, unique))
    {
        song.activePatternId = clip->patternId;
        refreshAll();
    }
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
        refreshAll();
}
