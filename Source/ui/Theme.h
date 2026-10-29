#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace rtr::theme
{

inline const juce::Colour background { 0xff13151a };
inline const juce::Colour panel { 0xff1b1e24 };
inline const juce::Colour panelLight { 0xff23272f };
inline const juce::Colour outline { 0xff2e333d };
inline const juce::Colour text { 0xffc9d1d9 };
inline const juce::Colour textDim { 0xff7b838f };
inline const juce::Colour rowHighlight { 0xff1f242c };
inline const juce::Colour beatLine { 0xff313846 };
inline const juce::Colour playhead { 0xffe05a5a };
inline const juce::Colour selection { 0xff2d4a6b };
inline const juce::Colour cursor { 0xffe8e8e8 };
inline const juce::Colour accent { 0xff57b6ff };

inline const juce::Colour note { 0xffa8e05f };
inline const juce::Colour noteOff { 0xff6e7681 };
inline const juce::Colour inst { 0xffe0a84f };
inline const juce::Colour vol { 0xff4fa3e0 };
inline const juce::Colour pan { 0xffc77fe0 };
inline const juce::Colour cmdCC { 0xff48c9b0 };
inline const juce::Colour cmdMacro { 0xffe06c4f };
inline const juce::Colour cmdOther { 0xffd8b4e8 };

inline const juce::Colour clipFill { 0xff25303c };
inline const juce::Colour clipFillSelected { 0xff2f4a63 };
inline const juce::Colour clipOutline { 0xff3d5a75 };

inline juce::Font monoFont (float height)
{
    return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), height, juce::Font::plain));
}

inline juce::Font uiFont (float height, bool bold = false)
{
    return juce::Font (juce::FontOptions (height, bold ? juce::Font::bold : juce::Font::plain));
}

/** Dark look for the plugin window. */
class LookAndFeel final : public juce::LookAndFeel_V4
{
public:
    LookAndFeel()
        : juce::LookAndFeel_V4 (juce::LookAndFeel_V4::getDarkColourScheme())
    {
        setColour (juce::ResizableWindow::backgroundColourId, background);
        setColour (juce::DocumentWindow::backgroundColourId, background);
        setColour (juce::PopupMenu::backgroundColourId, panelLight);
        setColour (juce::PopupMenu::textColourId, text);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, selection);
        setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
        setColour (juce::TextEditor::backgroundColourId, panelLight);
        setColour (juce::TextEditor::textColourId, text);
        setColour (juce::TextEditor::outlineColourId, outline);
        setColour (juce::TextEditor::focusedOutlineColourId, accent);
        setColour (juce::TextEditor::highlightColourId, selection);
        setColour (juce::TextEditor::highlightedTextColourId, juce::Colours::white);
        setColour (juce::ComboBox::backgroundColourId, panelLight);
        setColour (juce::ComboBox::textColourId, text);
        setColour (juce::ComboBox::outlineColourId, outline);
        setColour (juce::ComboBox::arrowColourId, textDim);
        setColour (juce::ComboBox::buttonColourId, panelLight);
        setColour (juce::Slider::thumbColourId, accent);
        setColour (juce::Slider::trackColourId, outline);
        setColour (juce::Slider::backgroundColourId, panelLight);
        setColour (juce::Slider::textBoxTextColourId, text);
        setColour (juce::Slider::textBoxBackgroundColourId, panelLight);
        setColour (juce::Slider::textBoxOutlineColourId, outline);
        setColour (juce::ToggleButton::textColourId, text);
        setColour (juce::ToggleButton::tickColourId, accent);
        setColour (juce::Label::textColourId, text);
        setColour (juce::TextButton::buttonColourId, panelLight);
        setColour (juce::TextButton::buttonOnColourId, selection);
        setColour (juce::TextButton::textColourOffId, text);
        setColour (juce::TextButton::textColourOnId, juce::Colours::white);
        setColour (juce::ScrollBar::thumbColourId, outline.brighter (0.2f));
    }
};

} // namespace rtr::theme
