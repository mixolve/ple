#include "ui/MainView.h"

namespace
{
const auto uiAppBackground = juce::Colour (0xff000000);

constexpr int uiButtonHeight = 30;
constexpr int uiSectionGap = 8;
constexpr int uiPluginWindowGap = 8;
constexpr int uiFooterHeight = 30;
constexpr int uiButtonGap = 8;
constexpr float uiTopInsetRatio = 0.06f;
constexpr float uiFooterBottomInsetRatio = 0.04f;

int getRelativeTopInset (int totalHeight) noexcept
{
    return juce::jmax (0, juce::roundToInt (totalHeight * uiTopInsetRatio));
}

int getRelativeFooterBottomInset (int totalHeight) noexcept
{
    return juce::jmax (0, juce::roundToInt (totalHeight * uiFooterBottomInsetRatio));
}
}

MainView::MainView (Action previousAction,
                    Action playAction,
                    Action nextAction,
                    Action playbackModeAction,
                    Action choosePluginAction,
                    Action openPluginGuiAction,
                    Action nowPlayingAction,
                    Action aboutAction,
                    Action browseAction)
{
    setOpaque (true);

    previousButton.setButtonText ("PREV");
    previousButton.getProperties().set ("accent", "white");
    previousButton.onClick = [action = std::move (previousAction)]
    {
        if (action)
            action();
    };
    addAndMakeVisible (previousButton);

    playButton.setButtonText ("PLAY");
    playButton.getProperties().set ("accent", "blue");
    playButton.onClick = [action = std::move (playAction)]
    {
        if (action)
            action();
    };
    addAndMakeVisible (playButton);

    nextButton.setButtonText ("NEXT");
    nextButton.getProperties().set ("accent", "white");
    nextButton.onClick = [action = std::move (nextAction)]
    {
        if (action)
            action();
    };
    addAndMakeVisible (nextButton);

    playbackModeButton.setButtonText ("ALL");
    playbackModeButton.getProperties().set ("accent", "peach");
    playbackModeButton.onClick = [action = std::move (playbackModeAction)]
    {
        if (action)
            action();
    };
    addAndMakeVisible (playbackModeButton);

    choosePluginButton.setButtonText ("LIST");
    choosePluginButton.onClick = [action = std::move (choosePluginAction)]
    {
        if (action)
            action();
    };

    openPluginGuiButton.setButtonText ("PLUG");
    openPluginGuiButton.getProperties().set ("accent", "white");
    openPluginGuiButton.onClick = [action = std::move (openPluginGuiAction)]
    {
        if (action)
            action();
    };
    openPluginGuiButton.setEnabled (false);
    addAndMakeVisible (openPluginGuiButton);

    nowButton.setButtonText ("NOW");
    nowButton.getProperties().set ("accent", "white");
    nowButton.onClick = [action = std::move (nowPlayingAction)]
    {
        if (action)
            action();
    };
    addAndMakeVisible (nowButton);

    browseButton.setButtonText ("BROW");
    browseButton.getProperties().set ("accent", "peach");
    browseButton.onClick = [action = std::move (browseAction)]
    {
        if (action)
            action();
    };
    addAndMakeVisible (browseButton);

    statusLabel.setText ("ready", juce::dontSendNotification);
    statusLabel.setJustificationType (juce::Justification::centredLeft);
    statusLabel.setInterceptsMouseClicks (false, false);
    statusLabel.setVisible (false);

    footerButton.setButtonText ("PLE by MIXOLVE");
    footerButton.getProperties().set ("accent", "grey");
    footerButton.onClick = [action = std::move (aboutAction)]
    {
        if (action)
            action();
    };
    footerButton.setMouseClickGrabsKeyboardFocus (false);
    footerButton.setWantsKeyboardFocus (false);
    addAndMakeVisible (footerButton);

    setPlaybackActive (false);

    for (auto* button : { &playbackModeButton, &previousButton, &playButton, &nextButton, &choosePluginButton, &openPluginGuiButton, &nowButton, &browseButton, &footerButton })
    {
        button->setWantsKeyboardFocus (false);
        button->setMouseClickGrabsKeyboardFocus (false);
    }
}

void MainView::setPlaybackModeText (const juce::String& text)
{
    playbackModeButton.setButtonText (text);
}

void MainView::setStatusText (const juce::String& text)
{
    statusLabel.setText (text, juce::dontSendNotification);
}

void MainView::setPlaybackActive (bool isPlaying)
{
    playButton.setButtonText (isPlaying ? "PAUSE" : "PLAY");
    playButton.getProperties().set ("accentless", isPlaying);
    playButton.getProperties().set ("accent", isPlaying ? "grey" : "blue");
}

void MainView::setChoosePluginEnabled (bool enabled)
{
    choosePluginButton.setEnabled (enabled);
}

void MainView::setOpenPluginGuiEnabled (bool enabled)
{
    openPluginGuiButton.setEnabled (enabled);
}

void MainView::setOpenPluginGuiText (const juce::String& text)
{
    openPluginGuiButton.setButtonText (text);
}

juce::Rectangle<int> MainView::getContentArea() const
{
    auto area = getLocalBounds();
    area.removeFromTop (getRelativeTopInset (getHeight()));
    area.removeFromBottom (uiFooterHeight + getRelativeFooterBottomInset (getHeight()));
    return area;
}

juce::Rectangle<int> MainView::getChoosePluginButtonBounds() const
{
    return choosePluginButton.getBounds();
}

juce::Rectangle<int> MainView::getPluginWindowBounds() const
{
    const auto area = getContentArea();
    const auto left = playbackModeButton.getX();
    const auto top = previousButton.getBottom() + uiPluginWindowGap;
    const auto right = nextButton.getRight();
    const auto bottom = choosePluginButton.getY() - uiPluginWindowGap;

    return juce::Rectangle<int> (left,
                                 top,
                                 juce::jmax (0, juce::jmin (area.getRight(), right) - left),
                                 juce::jmax (0, bottom - top));
}

juce::Rectangle<int> MainView::getNowPlayingWindowBounds() const
{
    return getPluginWindowBounds();
}

juce::Rectangle<int> MainView::getAudioBrowserWindowBounds() const
{
    return getPluginWindowBounds();
}

void MainView::paint (juce::Graphics& g)
{
    g.fillAll (uiAppBackground);
}

void MainView::resized()
{
    auto area = getLocalBounds();
    const auto topInset = getRelativeTopInset (getHeight());
    const auto footerBottomInset = getRelativeFooterBottomInset (getHeight());

    area.removeFromTop (topInset);

    auto footerBand = area.removeFromBottom (uiFooterHeight + footerBottomInset);
    footerButton.setBounds (footerBand.withHeight (uiFooterHeight));

    area.removeFromBottom (uiSectionGap);

    auto bottomRow = area.removeFromBottom (uiButtonHeight);
    auto topRow = area.removeFromTop (uiButtonHeight);
    const auto topRowTotalWidth = topRow.getWidth();

    const auto buttonWidth = juce::jmax (0, (topRow.getWidth() - (uiButtonGap * 3)) / 4);

    const auto bottomAvailableWidth = juce::jmax (0, topRowTotalWidth - (uiButtonGap * 3));
    const auto bottomButtonWidth = bottomAvailableWidth / 4;
    const auto bottomButtonRemainder = bottomAvailableWidth % 4;

    const auto chooseWidth = bottomButtonWidth + (bottomButtonRemainder > 0 ? 1 : 0);
    const auto openWidth = bottomButtonWidth + (bottomButtonRemainder > 1 ? 1 : 0);
    const auto nowWidth = bottomButtonWidth + (bottomButtonRemainder > 2 ? 1 : 0);
    const auto browseWidth = bottomButtonWidth;

    choosePluginButton.setBounds (bottomRow.removeFromLeft (chooseWidth));
    bottomRow.removeFromLeft (uiButtonGap);
    openPluginGuiButton.setBounds (bottomRow.removeFromLeft (openWidth));
    bottomRow.removeFromLeft (uiButtonGap);
    nowButton.setBounds (bottomRow.removeFromLeft (nowWidth));
    bottomRow.removeFromLeft (uiButtonGap);
    browseButton.setBounds (bottomRow.removeFromLeft (browseWidth));

    playbackModeButton.setBounds (topRow.removeFromLeft (buttonWidth));
    topRow.removeFromLeft (uiButtonGap);
    previousButton.setBounds (topRow.removeFromLeft (buttonWidth));
    topRow.removeFromLeft (uiButtonGap);
    playButton.setBounds (topRow.removeFromLeft (buttonWidth));
    topRow.removeFromLeft (uiButtonGap);
    nextButton.setBounds (topRow);
}
