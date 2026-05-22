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
    const auto top = area.getY();
    const auto right = nextButton.getRight();
    const auto bottom = playbackModeButton.getY() - uiPluginWindowGap;

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
    area.removeFromBottom (uiPluginWindowGap);
    auto topRow = area.removeFromBottom (uiButtonHeight);
    const auto buttonWidth = juce::jmax (0, (topRow.getWidth() - (uiButtonGap * 3)) / 4);

    const auto layoutRow = [buttonWidth] (juce::Rectangle<int> row,
                                          juce::Button& first,
                                          juce::Button& second,
                                          juce::Button& third,
                                          juce::Button& fourth)
    {
        first.setBounds (row.removeFromLeft (buttonWidth));
        row.removeFromLeft (uiButtonGap);
        second.setBounds (row.removeFromLeft (buttonWidth));
        row.removeFromLeft (uiButtonGap);
        third.setBounds (row.removeFromLeft (buttonWidth));
        row.removeFromLeft (uiButtonGap);
        fourth.setBounds (row.removeFromLeft (buttonWidth));
    };

    layoutRow (topRow, playbackModeButton, previousButton, playButton, nextButton);
    layoutRow (bottomRow, choosePluginButton, openPluginGuiButton, nowButton, browseButton);
}
