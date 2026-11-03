#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../PluginProcessor.h"

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
    bool hasSelection() const noexcept { return selStartRow >= 0 && selEndRow >= 0; }
    int getSelectionStart() const noexcept { return juce::jmin (selStartRow, selEndRow); }
    int getSelectionEnd() const noexcept { return juce::jmax (selStartRow, selEndRow); }
    void clearSelection();
    void setCursorRow (int row);

    /** The command number the column resolves to above the given row (the one
        an implicit cell on that row carries), or -1 when the column never named
        a command yet. */
    int carriedCommandNumber (int row, int ch) const;

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

    int selStartRow = -1;
    int selEndRow = -1;
    bool selecting = false;

    juce::String editBuffer;
    bool editing = false;

    std::vector<FieldRect> fieldRects;
    int playRow = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrackerComponent)
};

} // namespace rtr
