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
              Action clearPluginAction,
              Action nowPlayingAction,
              Action aboutAction,
              Action browseAction);

    void setPlaybackModeText (const juce::String& text);
    void setStatusText (const juce::String& text);
    void setPlaybackActive (bool isPlaying);
    void setOpenPluginGuiEnabled (bool enabled);
    void setOpenPluginGuiText (const juce::String& text);

    juce::Rectangle<int> getContentArea() const;
    juce::Rectangle<int> getChoosePluginButtonBounds() const;
    juce::Rectangle<int> getPluginWindowBounds() const;
    juce::Rectangle<int> getNowPlayingWindowBounds() const;
    juce::Rectangle<int> getAudioBrowserWindowBounds() const;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    class LongPressButton final : public juce::TextButton,
                                  private juce::Timer
    {
    public:
        using juce::TextButton::TextButton;

        Action onShortRelease;
        Action onLongPressRelease;

        void mouseDown (const juce::MouseEvent& event) override;
        void mouseDrag (const juce::MouseEvent& event) override;
        void mouseUp (const juce::MouseEvent& event) override;

    private:
        void timerCallback() override;
        void resetLongPressState();

        juce::String textBeforeLongPress;
        bool pressCandidate = false;
        bool longPressReady = false;
        bool pointerInside = false;
        static constexpr int longPressDelayMs = 550;
    };

    juce::TextButton previousButton;
    juce::TextButton playButton;
    juce::TextButton nextButton;
    juce::TextButton playbackModeButton;
    juce::TextButton choosePluginButton;
    LongPressButton openPluginGuiButton;
    juce::TextButton nowButton;
    juce::TextButton browseButton;
    juce::Label statusLabel;
    juce::TextButton footerButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainView)
};
