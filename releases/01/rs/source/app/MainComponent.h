#pragma once

#include <JuceHeader.h>
#include <memory>
#include "audio/PlaybackController.h"

class MainView;
class AudioBrowserController;
class AudioUnitPluginHost;
class PluginWindowFrame;
class NowPlayingContent;

namespace ple
{
class LockScreenController;
}

class MainComponent : public juce::AudioAppComponent
                     , private juce::Timer
{
public:
    MainComponent();
    ~MainComponent() override;

    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override;
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override;
    void releaseResources() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

private:
    void refreshAudioLibrary();
    bool loadAudioFile(const juce::File& file);
    void handlePlaybackFinished();
    void cyclePlaybackMode();
    void updatePlaybackModeButton();
    juce::String getPlaybackModeLabel() const;
    void startPlayback();
    void pausePlayback();
    void ensureAudioOutputActive();
    void suspendAudioOutputForPause();
    void seekPlayback (double positionSeconds);
    void togglePlayback();
    void playPreviousTrack();
    void playNextTrack();
    void choosePlugin();
    void browseAudioFiles();
    void refreshAudioBrowserDirectory();
    void openPluginGui();
    void openAboutWindow();
    void closeAboutWindow();
    void openNowPlayingWindow();
    void closeNowPlayingWindow();
    void refreshNowPlayingWindow();
    void syncPlaybackUi();
    void scheduleAudioBrowserDirectoryRefresh();
    void setStatusText (const juce::String& text);
    void setPlaybackModeText (const juce::String& text);
    void setChoosePluginEnabled (bool enabled);
    void setOpenPluginGuiEnabled (bool enabled);
    void setOpenPluginGuiText (const juce::String& text);

    std::unique_ptr<AudioBrowserController> audioBrowser;
    std::shared_ptr<AudioUnitPluginHost> pluginHost;
    std::unique_ptr<ple::LockScreenController> lockScreenController;
    std::unique_ptr<MainView> mainView;
    std::unique_ptr<PluginWindowFrame> aboutWindowHost;
    std::unique_ptr<PluginWindowFrame> nowPlayingWindowHost;
    NowPlayingContent* nowPlayingContent = nullptr;
    juce::TextButton choosePluginButton;

    ple::PlaybackState playbackState;
    std::unique_ptr<ple::PlaybackController> playbackController;
    bool audioBrowserDirectoryRefreshPending = false;
    bool audioOutputActive = false;

    std::unique_ptr<juce::LookAndFeel_V4> lookAndFeel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
