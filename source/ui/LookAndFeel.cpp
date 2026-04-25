#include "LookAndFeel.h"

#include "BinaryData.h"

namespace
{
const auto uiGrey800 = juce::Colour (0xff242424);
const auto uiGrey700 = juce::Colour (0xff363636);
const auto uiGrey500 = juce::Colour (0xff707070);
const auto uiWhite = juce::Colour (0xffffffff);
const auto uiAccentBlue = juce::Colour (0xff9999ff);
const auto uiAccentPeach = juce::Colour (0xffffcc99);
constexpr float uiFontSize = 22.0f;

juce::Font makeUiFont (const bool bold = false, const float height = uiFontSize)
{
    jassert (juce::approximatelyEqual (height, uiFontSize));

#if JUCE_TARGET_HAS_BINARY_DATA
    static const auto regularTypeface = juce::Typeface::createSystemTypefaceFor (BinaryData::SometypeMonoRegular_ttf,
                                                                                  BinaryData::SometypeMonoRegular_ttfSize);
    static const auto boldTypeface = juce::Typeface::createSystemTypefaceFor (BinaryData::SometypeMonoBold_ttf,
                                                                               BinaryData::SometypeMonoBold_ttfSize);

    const auto typeface = bold ? boldTypeface : regularTypeface;

    if (typeface != nullptr)
        return juce::Font (juce::FontOptions (typeface).withHeight (height));
#endif

    return juce::Font (juce::FontOptions ("Sometype Mono", height, bold ? juce::Font::bold : juce::Font::plain));
}

juce::Colour getButtonAccentColour (const juce::Button& button)
{
    const auto accent = button.getProperties().getWithDefault ("accent", "grey").toString();

    if (accent == "blue")
        return uiAccentBlue;

    if (accent == "peach")
        return uiAccentPeach;

    if (accent == "white")
        return uiWhite;

    return uiGrey500;
}

class ProjectDocumentWindowButton final : public juce::Button
{
public:
    ProjectDocumentWindowButton (juce::String buttonName, juce::Colour buttonColour, juce::Path normalShape, juce::Path toggledShape)
        : juce::Button (std::move (buttonName)), colour (buttonColour), normal (std::move (normalShape)), toggled (std::move (toggledShape))
    {
        setWantsKeyboardFocus (false);
    }

    void paintButton (juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
    {
        auto background = uiGrey800;

        if (shouldDrawButtonAsDown)
            background = uiGrey700;
        else if (shouldDrawButtonAsHighlighted)
            background = uiGrey700;

        g.setColour (background);
        g.fillAll();

        g.setColour ((! isEnabled() || shouldDrawButtonAsDown) ? colour.withAlpha (0.65f)
                                                               : colour);

        if (shouldDrawButtonAsHighlighted)
        {
            g.fillAll();
            g.setColour (uiGrey500);
        }

        const auto& shape = getToggleState() ? toggled : normal;
        const auto reducedRect = juce::Justification (juce::Justification::centred)
                                     .appliedToRectangle (juce::Rectangle<int> (getHeight(), getHeight()), getLocalBounds())
                                     .toFloat()
                                     .reduced ((float) getHeight() * 0.3f);

        g.fillPath (shape, shape.getTransformToScaleToFit (reducedRect, true));
    }

private:
    juce::Colour colour;
    juce::Path normal;
    juce::Path toggled;
};

class PleLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    juce::Font getTextButtonFont (juce::TextButton&, int) override
    {
        return makeUiFont();
    }

    juce::Font getPopupMenuFont() override
    {
        return makeUiFont();
    }

    juce::Font getLabelFont (juce::Label&) override
    {
        return makeUiFont();
    }

    void drawButtonBackground (juce::Graphics& g,
                               juce::Button& button,
                               const juce::Colour&,
                               bool isMouseOverButton,
                               bool isButtonDown) override
    {
        const auto bounds = button.getLocalBounds();
        const auto accentColour = getButtonAccentColour (button);
        const auto isActive = button.getToggleState();

        g.setColour (isButtonDown ? uiGrey700 : uiGrey800);
        g.fillRect (bounds);

        g.setColour (button.isEnabled() ? (isActive ? accentColour : uiGrey500) : uiGrey500);
        g.drawRect (bounds, 1);

        if ((isActive || (isMouseOverButton && button.isEnabled() && ! isButtonDown)) && button.isEnabled())
        {
            g.setColour (accentColour);
            g.drawRect (bounds.reduced (1), 1);
        }
    }

    void drawButtonText (juce::Graphics& g,
                         juce::TextButton& button,
                         bool,
                         bool) override
    {
        g.setColour (button.isEnabled() ? uiWhite : uiGrey500);
        g.setFont (makeUiFont());
        g.drawFittedText (button.getButtonText(),
                          button.getLocalBounds().reduced (6, 0),
                          juce::Justification::centred,
                          1,
                          1.0f);
    }

    void drawLabel (juce::Graphics& g, juce::Label& label) override
    {
        const auto bounds = label.getLocalBounds();
        const auto textColour = label.isColourSpecified (juce::Label::textColourId)
                                    ? label.findColour (juce::Label::textColourId)
                                    : (label.isEnabled() ? uiWhite : uiGrey500);

        g.setColour (textColour);
        g.setFont (makeUiFont());
        g.drawFittedText (label.getText(),
                          bounds.reduced (4, 0),
                          label.getJustificationType(),
                          1,
                          1.0f);
    }

    void drawPopupMenuBackgroundWithOptions (juce::Graphics& g,
                                             int width,
                                             int height,
                                             const juce::PopupMenu::Options&) override
    {
        g.setColour (uiGrey800);
        g.fillAll();

        g.setColour (uiGrey500);
        g.drawRect (0, 0, width, height, 1);
    }

    void drawPopupMenuSectionHeaderWithOptions (juce::Graphics& g,
                                                const juce::Rectangle<int>& area,
                                                const juce::String& sectionName,
                                                const juce::PopupMenu::Options&) override
    {
        g.setFont (makeUiFont (true));
        g.setColour (uiWhite);
        g.drawText (sectionName.toUpperCase(), area.reduced (8, 0), juce::Justification::centredLeft, true);
    }

    void drawPopupMenuItemWithOptions (juce::Graphics& g,
                                       const juce::Rectangle<int>& area,
                                       bool isHighlighted,
                                       const juce::PopupMenu::Item& item,
                                       const juce::PopupMenu::Options&) override
    {
        if (item.isSeparator)
        {
            auto separatorArea = area.reduced (6, 0);
            separatorArea.removeFromTop (separatorArea.getHeight() / 2);

            g.setColour (uiGrey500);
            g.fillRect (separatorArea.removeFromTop (1));
            return;
        }

        auto itemArea = area.reduced (1);

        if (isHighlighted)
        {
            g.setColour (uiGrey700);
            g.fillRect (itemArea);
            g.setColour (uiAccentBlue);
            g.drawRect (itemArea, 1);
        }
        else
        {
            g.setColour (uiGrey800);
            g.fillRect (itemArea);
            g.setColour (uiGrey500);
            g.drawRect (itemArea, 1);
        }

        g.setColour (item.isEnabled ? uiWhite : uiGrey500);
        g.setFont (makeUiFont());
        g.drawText (item.text.toUpperCase(), itemArea.reduced (10, 0), juce::Justification::centredLeft, true);
        if (item.shortcutKeyDescription.isNotEmpty())
        {
            g.setColour (item.isEnabled ? uiGrey500 : uiGrey700);
            g.drawText (item.shortcutKeyDescription.toUpperCase(), itemArea.reduced (10, 0), juce::Justification::centredRight, true);
        }
    }

    int getPopupMenuBorderSize() override
    {
        return 1;
    }

    void drawDocumentWindowTitleBar (juce::DocumentWindow& window,
                                     juce::Graphics& g,
                                     int w,
                                     int h,
                                     int titleSpaceX,
                                     int titleSpaceW,
                                     const juce::Image* icon,
                                     bool drawTitleTextOnLeft) override
    {
        if (w <= 0 || h <= 0)
            return;

        g.setColour (uiGrey800);
        g.fillAll();

        g.setColour (uiGrey500);
        g.drawRect (0, 0, w, h, 1);

        auto font = makeUiFont (false);
        g.setFont (font);

        auto textW = juce::GlyphArrangement::getStringWidthInt (font, window.getName());
        auto iconW = 0;
        auto iconH = 0;

        if (icon != nullptr)
        {
            iconH = static_cast<int> (font.getHeight());
            iconW = icon->getWidth() * iconH / icon->getHeight() + 4;
        }

        textW = juce::jmin (titleSpaceW, textW + iconW);
        auto textX = drawTitleTextOnLeft ? titleSpaceX
                                         : juce::jmax (titleSpaceX, (w - textW) / 2);

        if (textX + textW > titleSpaceX + titleSpaceW)
            textX = titleSpaceX + titleSpaceW - textW;

        if (icon != nullptr)
        {
            g.setOpacity (window.isActiveWindow() ? 1.0f : 0.6f);
            g.drawImageWithin (*icon, textX, (h - iconH) / 2, iconW, iconH,
                               juce::RectanglePlacement::centred, false);
            textX += iconW;
            textW -= iconW;
        }

        if (window.isColourSpecified (juce::DocumentWindow::textColourId) || isColourSpecified (juce::DocumentWindow::textColourId))
            g.setColour (window.findColour (juce::DocumentWindow::textColourId));
        else
            g.setColour (uiWhite);

        g.drawText (window.getName(), textX, 0, textW, h, juce::Justification::centredLeft, true);
    }

    juce::Button* createDocumentWindowButton (int buttonType) override
    {
        juce::Path shape;
        auto crossThickness = 0.15f;

        if (buttonType == juce::DocumentWindow::closeButton)
        {
            shape.addLineSegment ({ 0.0f, 0.0f, 1.0f, 1.0f }, crossThickness);
            shape.addLineSegment ({ 1.0f, 0.0f, 0.0f, 1.0f }, crossThickness);

            return new ProjectDocumentWindowButton ("close", uiWhite, shape, shape);
        }

        if (buttonType == juce::DocumentWindow::minimiseButton)
        {
            shape.addLineSegment ({ 0.0f, 0.5f, 1.0f, 0.5f }, crossThickness);
            return new ProjectDocumentWindowButton ("minimise", uiGrey500, shape, shape);
        }

        if (buttonType == juce::DocumentWindow::maximiseButton)
        {
            shape.addLineSegment ({ 0.5f, 0.0f, 0.5f, 1.0f }, crossThickness);
            shape.addLineSegment ({ 0.0f, 0.5f, 1.0f, 0.5f }, crossThickness);
            return new ProjectDocumentWindowButton ("maximise", uiAccentBlue, shape, shape);
        }

        jassertfalse;
        return nullptr;
    }
};
}

std::unique_ptr<juce::LookAndFeel_V4> ple::makeMainLookAndFeel()
{
    return std::make_unique<PleLookAndFeel>();
}
