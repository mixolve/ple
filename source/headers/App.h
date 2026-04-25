#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <memory>
#include "Playback.h"
#include <vector>

class MainView;
class BrowserController;
class PluginHostController;

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
    void restartCurrentTrack();
    void handlePlaybackFinished();
    void cyclePlaybackMode();
    void updatePlaybackModeButton();
    juce::String getPlaybackModeLabel() const;
    int getTrackIndexForNavigation (bool movingForward) const;
    void startPlayback();
    void pausePlayback();
    void togglePlayback();
    void playPreviousTrack();
    void playNextTrack();
    void choosePlugin();
    void browseAudioFiles();
    void handleAudioBrowserSelection(int selectedIndex);
    void handleAudioBrowserFolderPlaySelection(int selectedIndex);
    void refreshAudioBrowserDirectory();
    void handlePluginMenuSelection(int selectedIndex);
    void loadPluginDescription(const juce::PluginDescription& description, bool openGuiAfterLoad = false);
    void openPluginGui();
    void syncPlaybackUi();
    void scheduleAudioBrowserDirectoryRefresh();
    bool isPlaybackActive() const;
    int getSelectedPluginIndex() const;
    juce::Rectangle<int> getContentArea() const;
    void setStatusText (const juce::String& text);
    void setPlaybackModeText (const juce::String& text);
    void setChoosePluginEnabled (bool enabled);
    void setOpenPluginGuiEnabled (bool enabled);
    void setOpenPluginGuiText (const juce::String& text);

    std::unique_ptr<BrowserController> browserController;
    std::shared_ptr<PluginHostController> pluginHostController;
    std::unique_ptr<MainView> mainView;

    ple::PlaybackState playbackState;
    std::unique_ptr<ple::PlaybackController> playbackController;
    bool automationPlayOnLaunch = false;
    bool audioBrowserDirectoryRefreshPending = false;

    std::unique_ptr<juce::LookAndFeel_V4> lookAndFeel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};