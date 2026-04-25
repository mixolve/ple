#pragma once

#include <JuceHeader.h>
#include <functional>

class MainView final : public juce::Component
{
public:
    using Action = std::function<void()>;

    MainView (Action previousAction,
              Action playAction,
              Action nextAction,
              Action playbackModeAction,
              Action choosePluginAction,
              Action openPluginGuiAction,
              Action browseAction);

    void setPlaybackModeText (const juce::String& text);
    void setStatusText (const juce::String& text);
    void setPlaybackActive (bool isPlaying);
    void setChoosePluginEnabled (bool enabled);
    void setOpenPluginGuiEnabled (bool enabled);
    void setOpenPluginGuiText (const juce::String& text);

    juce::Rectangle<int> getContentArea() const;
    juce::Rectangle<int> getPluginWindowBounds() const;
    juce::Rectangle<int> getAudioBrowserWindowBounds() const;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    juce::TextButton previousButton;
    juce::TextButton playButton;
    juce::TextButton nextButton;
    juce::TextButton playbackModeButton;
    juce::TextButton choosePluginButton;
    juce::TextButton openPluginGuiButton;
    juce::TextButton browseButton;
    juce::Label statusLabel;
    juce::TextButton footerButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainView)
};