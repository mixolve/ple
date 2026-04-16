#pragma once

#include <JuceHeader.h>
#include <vector>

namespace ple
{
class LockScreenPlaybackController;
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
    enum class PlaybackMode
    {
        repeatOne,
        repeatFolder,
        shuffleFolder
    };

    void refreshAudioLibrary();
    bool loadAudioFileAtIndex(int index);
    bool loadAudioFile(const juce::File& file);
    void restartCurrentTrack();
    void handlePlaybackFinished();
    void cyclePlaybackMode();
    void updatePlaybackModeButton();
    void updatePlayButtonLabel();
    juce::String getPlaybackModeLabel() const;
    int getTrackIndexForNavigation (bool movingForward) const;
    void startPlayback();
    void stopPlayback();
    void togglePlay();
    void playPreviousTrack();
    void playNextTrack();
    std::vector<juce::File> getCurrentFolderTracks() const;
    int getCurrentFolderTrackIndex(const std::vector<juce::File>& tracks) const;
    void choosePlugin();
    void browseAudioFiles();
    void handleAudioBrowserSelection(int selectedIndex);
    void refreshAudioBrowserDirectory();
    void handlePluginMenuSelection(int selectedIndex);
    void loadPluginDescription(const juce::PluginDescription& description, bool openGuiAfterLoad = false);
    void openPluginGui();
    juce::Rectangle<int> getPluginWindowBounds() const;
    juce::Rectangle<int> getAudioBrowserWindowBounds() const;
    void configureLockScreenControls();
    void maybeScheduleAutomationLockScreen();
    void updateNowPlayingInfo();
    bool isPlaybackActive() const;

    juce::TextButton previousButton;
    juce::TextButton playButton;
    juce::TextButton nextButton;
    juce::TextButton playbackModeButton;
    juce::TextButton choosePluginButton;
    juce::TextButton openPluginGuiButton;
    juce::TextButton browseButton;
    juce::Component pluginWindowAnchor;
    juce::Label statusLabel;
    juce::Label footerLabel;

    struct AudioBrowserEntry
    {
        juce::File file;
        juce::String label;
        bool isDirectory = false;
        bool isParent = false;
    };

    juce::AudioFormatManager formatManager;
    std::unique_ptr<juce::AudioFormatReaderSource> readerSource;
    juce::AudioTransportSource transportSource;
    juce::CriticalSection audioSourceLock;

    double currentSampleRate = 44100.0;
    int currentBlockSize = 512;

    juce::AudioUnitPluginFormat audioUnitFormat;
    std::shared_ptr<juce::AudioPluginInstance> pluginInstance;
    int pluginLoadToken = 0;
    std::vector<juce::PluginDescription> installedPluginDescriptions;
    std::vector<juce::File> availableAudioFiles;
    std::vector<AudioBrowserEntry> audioBrowserEntries;
    int currentAudioFileIndex = -1;
    juce::String currentAudioFileName;
    juce::String currentTrackTitle { "NO AUDIO" };
    juce::String currentTrackArtistName;
    double currentTrackDurationSeconds = 0.0;
    PlaybackMode playbackMode = PlaybackMode::repeatFolder;
    bool playbackFinishedHandled = false;
    juce::File audioBrowserDirectory;
    std::unique_ptr<juce::Component> pluginWindowHost;
    std::unique_ptr<juce::Component> pluginMenuHost;
    std::unique_ptr<juce::Component> audioBrowserHost;
    std::unique_ptr<ple::LockScreenPlaybackController> lockScreenPlaybackController;
    juce::String automationPluginQuery;
    bool automationOpenGuiAfterLoad = false;
    bool automationShowPluginMenu = false;
    bool automationPlayOnLaunch = false;
    bool automationLockScreenOnLaunch = false;
    bool automationLockScreenScheduled = false;

    std::unique_ptr<juce::LookAndFeel_V4> lookAndFeel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
