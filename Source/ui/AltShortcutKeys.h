#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

namespace rtr
{

// The Alt spellings of the shortcuts the plug-in owns belong to every panel rather
// than only to the editor, because JUCE sends a key press to the focused or modal
// component and walks up only from there: while a right-click menu is open or a
// pattern is being dragged out of the pool, the panel that menu or drag belongs to
// is what answers the press and the editor is never consulted at all, so a pair
// the editor alone owned was swallowed on those paths. Hosts such as Bitwig and
// Reaper also answer the Ctrl spellings with their own undo and clipboard before
// the plug-in is offered the event, so the Alt pair is the spelling they leave
// free. Both pairs and the clip slice read the letter over the key code and over
// the character the press printed, since a key held by Alt arrives over X11 as
// that character rather than as a bare code, and no Alt+Shift spelling is claimed
// at all: Alt+Shift+Z was found swallowed on the way to both hosts, so answering
// it would give a key the user aims at the redo the opposite meaning wherever it
// does arrive, and the whole Alt+Shift family is left to the host rather than
// claimed by any panel.

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

// the shared body a panel answers the undo pair with: the press is only claimed
// when that panel actually holds the step, which leaves an event no panel can
// spend free to walk up to the editor. closePending runs before the undo and is
// how a panel whose number is still half typed closes that buffer on the press
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

// the cell clipboard's steps, which the same reasoning gives the hosts: Ctrl+C,
// Ctrl+V and Ctrl+X are the host's own clipboard and cut, so the plug-in is never
// handed those events and the cells it copies, pastes and cuts are unreachable there.
// Alt+C copies the selected cells and Alt+V pastes them at the cursor with
// everything, the plain spelling's meaning, so a paste of what was copied lands
// wherever the paste filters would put it rather than one field at a time. Alt+X
// is the cut, which takes the same copy and then empties the very cells it took,
// so the cursor and the painted selection stay where they were and a following
// Alt+V moves the cut cells rather than repeating them
enum class ClipboardKey { none = 0, copy, paste, cut };

inline ClipboardKey matchClipboardKey (const juce::KeyPress& key)
{
    if (! key.getModifiers().isAltDown()
        || key.getModifiers().isCommandDown() || key.getModifiers().isCtrlDown()
        || key.getModifiers().isShiftDown())
        return ClipboardKey::none;

    const int code = key.getKeyCode();
    const auto printed = key.getTextCharacter();

    if (code == 'C' || code == 'c' || printed == 'C' || printed == 'c')
        return ClipboardKey::copy;

    if (code == 'V' || code == 'v' || printed == 'V' || printed == 'v')
        return ClipboardKey::paste;

    // the cut is the letter the note entry also uses for D sharp, so a panel
    // holding it has to claim the press before its own key entry can type it
    if (code == 'X' || code == 'x' || printed == 'X' || printed == 'x')
        return ClipboardKey::cut;

    return ClipboardKey::none;
}

// the clipboard's steps claim the press whenever the panel holds one of them, even when
// that step has nothing to do, which is what keeps an Alt held letter from
// falling through and being typed as a note at the cursor: the three letters the
// steps answer, c, v and x, are all notes this plug-in's key entry types
inline bool answerClipboardKey (const juce::KeyPress& key,
                                const std::function<void()>& copy,
                                const std::function<void()>& paste,
                                const std::function<void()>& cut)
{
    const auto step = matchClipboardKey (key);

    if (step == ClipboardKey::copy && copy != nullptr)
    {
        copy();
        return true;
    }

    if (step == ClipboardKey::paste && paste != nullptr)
    {
        paste();
        return true;
    }

    if (step == ClipboardKey::cut && cut != nullptr)
    {
        cut();
        return true;
    }

    return false;
}

// the clip slice at the playhead, which the timeline used to answer with Ctrl+X:
// that letter is the host's own cut, found to change nothing about the plug-in's
// cells in either host, so the shortcut moved to Alt+S, the free letter. there is
// one spelling of it shared by every panel rather than a Ctrl letter per host,
// because a clip slice has no per-host meaning to keep. the letter is one the
// note entry uses for C sharp, so a panel holding the step has to claim the press
// before its own key entry can type it, and a press is claimed whenever the panel
// holds the step even when the slice itself refuses, which is what keeps an Alt
// held letter from falling through as a note
inline bool matchSliceKey (const juce::KeyPress& key)
{
    if (! key.getModifiers().isAltDown()
        || key.getModifiers().isCommandDown() || key.getModifiers().isCtrlDown()
        || key.getModifiers().isShiftDown())
        return false;

    const int code = key.getKeyCode();
    const auto printed = key.getTextCharacter();

    return code == 'S' || code == 's' || printed == 'S' || printed == 's';
}

inline bool answerSliceKey (const juce::KeyPress& key,
                            const std::function<void()>& slice)
{
    if (! matchSliceKey (key) || slice == nullptr)
        return false;

    slice();
    return true;
}

} // namespace rtr
