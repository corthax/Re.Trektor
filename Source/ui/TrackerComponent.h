#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../PluginProcessor.h"
#include <array>
#include <cstdint>
#include <vector>

namespace rtr
{

class TrackerComponent final : public juce::Component,
                               private juce::Timer
{
public:
    enum class Field { note = 0, inst, vol, pan, cmdNum, cmdVal };

    explicit TrackerComponent (ReTrektorAudioProcessor& processor);
    ~TrackerComponent() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

    bool keyPressed (const juce::KeyPress&) override;
    void focusGained (FocusChangeType) override;
    void focusLost (FocusChangeType) override;

    void refresh();

    std::function<void()> onModelChanged;
    std::function<void()> onPatternEdited;

    int getCursorRow() const noexcept { return cursorRow; }
    int getCursorChannel() const noexcept { return cursorChannel; }
    void setCursorRow (int row);

    //== Cell selection =======================================================
    /** The selection is a set of cells rather than a band of whole rows: a
        rectangle covers rows in one direction and channels in the other, so a
        tool can be told to touch one column of one row and any shape the user
        dragged out of the tracker. */
    bool hasSelection() const noexcept;
    bool isCellSelected (int row, int ch) const noexcept;
    void clearSelection();
    /** Adds a rectangle of cells to the selection, both ends inclusive, for the
        tests and any host-facing tool. The mouse gestures reach the same path
        with the modifiers, which union or subtract the rectangle they sweep. */
    void addCellsToSelection (int row0, int ch0, int row1, int ch1);

    //== Cell clipboard =======================================================
    /** The cells Ctrl+C and the tracker's "Copy selection" menu item have
        copied, and that Ctrl+V and the two paste submenus write back. The copied
        cells keep their offsets inside the copied rectangle, so the paste
        anchored at the cursor lays them out around it again and never writes
        through a Ctrl-drag hole. Like the selection this lives in the tracker
        view rather than in the saved song, and it survives a pattern switch. */
    CellClipboard cellClipboard;
    void copySelectionToClipboard();
    /** Empties the data of the selected cells and leaves the painting, the cursor
        and everything beside them alone, which is what the cell menu's Clear /
        Selection answers and the cut below reuses: one loop, one meaning, shared
        rather than written twice. */
    void clearSelectedCells();
    /** Alt+X / "Cut selection": the copy above, then emptying exactly the cells it
        took, so the cut's data is already on the clipboard and a following paste
        moves those cells to the cursor instead of repeating them. The cursor and
        the painted selection stay where they were, which is where the paste
        anchors itself. */
    void cutSelectionToClipboard();
    int pasteClipboardAtCursor (int fieldMask);
    bool hasCellClipboard() const noexcept { return ! cellClipboard.empty(); }

    //== Undo and redo ========================================================
    /** The editor's step along the undo line, injected rather than reached
        through the plug-in so the menu pair walks the same line the Ctrl+Z and
        Ctrl+Y keys walk and refreshes the whole view, including the Settings
        tab, instead of only the grid. Stepping records no point of its own. */
    std::function<bool()> undoStep;
    std::function<bool()> redoStep;

    /** The editor's cell-clipboard steps, injected the same way the undo pair is
        rather than reached through the plug-in, so the Alt+C / Alt+V / Alt+X steps
        copy, paste and cut through the same clipboard the Ctrl pair and the cell
        menu reach and refresh the whole view. Alt is the spelling Bitwig and
        Reaper leave free, since those hosts answer Ctrl+C, Ctrl+V and Ctrl+X with
        their own clipboard and cut before the plug-in is offered the event, and a
        press aimed at undo is swallowed on the menu and drag paths for the same
        reason a pair the editor alone owned was. The paste runs with everything,
        which is those keys' plain spelling's meaning, and the cut takes the same
        copy and then empties the cells it took, so cut then paste moves them. */
    std::function<void()> copyStep;
    std::function<void()> pasteStep;
    std::function<void()> cutStep;

    /** The clip slice at the playhead, which the timeline used to answer with
        Ctrl+X: that letter is the host's own cut, found in both hosts to change
        nothing about the plug-in, so the shortcut moved to Alt+S and every panel
        answers it, with the editor's own slice injected rather than reached here
        so the one spelling still splits the timeline and refreshes all of it.
        There is no Ctrl spelling of it left to keep for a host. */
    std::function<void()> sliceStep;

    /** Closes a half-typed numeric buffer the way Esc does and reports whether
        one was there, which is what makes Ctrl+Z or the menu's Undo one press
        one effect: a press either forgets the digits or walks back. */
    bool cancelPendingEdit();

    /** Hands keyboard focus back once a menu this panel owns has closed,
        injected by the editor rather than reached here so the editor decides
        which component is meant to hear keys. A context menu is a window with
        no parent component, so closing one leaves no component focused at all
        and a key press is only answered by the editor's own shortcuts: the
        note entry, cursor moves and clears this panel owns go unheard until
        the mouse is used again, which is what the keyboard felt dead after a
        right-click menu. */
    std::function<void()> focusRescue;

    /** The command number the column resolves to above the given row (the one
        an implicit cell on that row carries), or -1 when the column never named
        a command yet. */
    int carriedCommandNumber (int row, int ch) const;

    /** The argument the column resolves to above the given row, which is the one
        the rows under it are drawn with and answers them with, or -1 when the
        column carries none. */
    int carriedCommandValue (int row, int ch) const;

    //== Geometry exposed to the tests and any host-facing tool ================
    Field getCursorField() const noexcept { return cursorField; }
    juce::Rectangle<int> fieldBoundsFor (int channel, Field field) const
    {
        return fieldBounds (channel, field);
    }
    juce::Rectangle<int> chipBoundsFor (int channel, int index) const
    {
        return toggleBounds (channel, index);
    }
    /** The whole channel column a cell occupies, so a gesture or a test can
        name a cell without adding up its fields. */
    juce::Rectangle<int> channelBoundsFor (int channel) const
    {
        return channel < 0 || channel >= (int) channelRects.size()
                    ? juce::Rectangle<int>()
                    : channelRects[(size_t) channel];
    }

private:
    struct FieldRect
    {
        Field field = Field::note;
        int channel = 0;
        juce::Rectangle<int> bounds;
    };

    void timerCallback() override;
    void layoutFields();
    void drawHeader (juce::Graphics&, juce::Rectangle<int>);
    void drawRows (juce::Graphics&, juce::Rectangle<int>);
    void markChanged();
    void commitRowsEditor();

    int rowAtY (int y) const noexcept;
    int fieldIndexAtX (int x) const;
    int channelAtX (int x) const;
    juce::Rectangle<int> fieldBounds (int channel, Field field) const;
    juce::Rectangle<int> toggleBounds (int channel, int index) const;
    juce::Rectangle<int> cursorBounds() const;
    int visibleRowCount() const noexcept;

    int maxScrollX() const noexcept;
    int scrollbarStrip() const noexcept;
    juce::Rectangle<int> scrollbarArea() const;
    juce::Rectangle<int> thumbBounds() const;
    int thumbWidthFor (int trackWidth) const noexcept;
    int scrollXFromThumbX (int thumbX) const noexcept;
    void setScrollX (int x);
    void clampScroll();
    void drawScrollbar (juce::Graphics&, juce::Rectangle<int>);
    juce::Rectangle<int> visibleColumn (juce::Rectangle<int>) const;

    int currentPlayRow() const;
    void ensureCursorVisible();

    void moveCursor (int deltaRow, int deltaField);
    void advanceRow (int amount);
    void clearFieldAt (int row, int channel, Field field);
    void clearCurrentField();
    void commitEditBuffer();
    void cancelEdit();

    //== Selection machinery ==================================================
    /** What the gesture the mouse is doing does to the set of selected cells:
        a plain click and drag takes over the selection, Shift unions what it
        sweeps into it and Ctrl takes it away. */
    enum class SelectOp { replace, add, remove };
    void applyCellRect (int rowA, int chA, int rowB, int chB, SelectOp op);
    /** What the mouse is holding the gesture to: Shift unions, Ctrl subtracts. */
    SelectOp gestureOp (const juce::MouseEvent&) const;

    int maxDigitsFor (Field field) const;
    int fieldGlyphs (Field field) const;
    int clampValue (Field field, int value) const;
    void applyValue (Field field, int value);

    Pattern& pattern();
    const Pattern& pattern() const;

    void showContextMenu (const juce::MouseEvent&);

    ReTrektorAudioProcessor& proc;
    rtr::Song& song;

    juce::TextEditor rowsEditor;

    int rowHeight = 19;
    int gutterWidth = 52;
    int headerHeight = 46;
    int scrollRow = 0;
    int scrollX = 0;
    int totalWidth = 0;
    int scrollbarHeight = 14;
    bool draggingBar = false;
    int dragOffsetX = 0;

    int cursorRow = 0;
    int cursorChannel = 0;
    Field cursorField = Field::note;

    // selection is one bit per (row, channel) cell: the channel count caps at
    // kMaxTrackerChannels, so two words hold a whole row and a rectangle is
    // added or removed whole words at a time. Rows the pattern does not show
    // have no bits, and stale bits outside the visible area read as unselected
    static constexpr int kSelWords = (kMaxTrackerChannels + 63) / 64;
    static constexpr std::array<std::uint64_t, kSelWords> kNoCells {};
    std::vector<std::array<std::uint64_t, kSelWords>> selBits; // [row][word]
    int selAnchorRow = 0; // where the gesture in progress started
    int selAnchorChannel = 0;
    int selDragRow = -1; // the cell the last drag event painted, to skip no-ops
    int selDragChannel = -1;
    bool selecting = false;
    SelectOp selectOp = SelectOp::replace;

    juce::String editBuffer;
    bool editing = false;

    std::vector<FieldRect> fieldRects;
    // one entry per channel column, the span its visible fields cover
    std::vector<juce::Rectangle<int>> channelRects;
    int playRow = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrackerComponent)
};

} // namespace rtr
