#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

namespace rtr
{

// The Alt spellings of undo and redo belong to every panel rather than only to
// the editor, because JUCE sends a key press to the modal component and walks
// up that chain (ComponentPeer::getTargetForKeyPress): while a right-click menu
// is open, or while a pattern is being dragged across the DragAndDropContainer
// from the pool, the modal target is the popup or the dragged item and its
// keyPressed is answered by the panel the menu was attached to, so the editor
// is never consulted and a press aimed at undo is swallowed there. The letter
// is matched over both the key code and the printed character, because a key
// held by Alt arrives over X11 as the character that press produced rather than
// as a bare code. No Alt+Shift spelling is claimed at all: Alt+Shift+Z is
// swallowed on the way to Bitwig and Reaper alike, so answering it would give a
// key the user aims at the redo the opposite meaning wherever it does arrive.

enum class UndoRedoKey { none = 0, undo, redo };

inline UndoRedoKey matchUndoRedoKey (const juce::KeyPress& key)
{
    if (! key.getModifiers().isAltDown()
        || key.getModifiers().isCommandDown() || key.getModifiers().isCtrlDown()
        || key.getModifiers().isShiftDown())
        return UndoRedoKey::none;

    const int code = key.getKeyCode();
    const auto printed = key.getTextCharacter();

    if (code == 'Z' || code == 'z' || printed == 'Z' || printed == 'z')
        return UndoRedoKey::undo;

    if (code == 'Y' || code == 'y' || printed == 'Y' || printed == 'y')
        return UndoRedoKey::redo;

    return UndoRedoKey::none;
}

// the shared body a panel answers the pair with: the press is only claimed when
// that panel actually holds the step, which leaves the event free to walk up to
// the editor when it does not. closePending runs before the undo and is how a
// panel whose number is still half typed closes that buffer on the press
// instead of walking over it, which is what keeps one press one effect
inline bool answerUndoRedoKey (const juce::KeyPress& key,
                               const std::function<bool()>& undo,
                               const std::function<bool()>& redo,
                               const std::function<bool()>& closePending = {})
{
    const auto step = matchUndoRedoKey (key);

    if (step == UndoRedoKey::undo && undo != nullptr)
    {
        if (! (closePending != nullptr && closePending()))
            undo();

        return true;
    }

    if (step == UndoRedoKey::redo && redo != nullptr)
    {
        redo();
        return true;
    }

    return false;
}

} // namespace rtr
