#pragma once

#include "audio/PlaybackController.h"

#include <functional>
#include <memory>
#include <vector>

class AudioBrowserController final
{
public:
    struct Dependencies
    {
        juce::Component& parentComponent;
        std::function<ple::PlaybackController*()> getPlaybackController;
        std::function<juce::File()> getAudioRootDirectory;
        std::function<juce::Rectangle<int>()> getAudioBrowserWindowBounds;
        std::function<void()> closePluginMenu;
        std::function<void()> closePluginWindow;
        std::function<void()> closeNowPlayingWindow;
        std::function<bool (const juce::File&)> loadAudioFile;
        std::function<void()> startPlayback;
        std::function<void()> syncPlaybackUi;
        std::function<void()> scheduleAudioBrowserDirectoryRefresh;
        std::function<void (const juce::String&)> setStatusText;
    };

    AudioBrowserController() = default;

    void initialise (Dependencies dependencies);
    void reset();

    void browseAudioFiles();
    void refreshAudioBrowserDirectory();
    void handleAudioBrowserSelection (int selectedIndex);
    void handleAudioBrowserFolderPlaySelection (int selectedIndex);
    void resized();
    void closeAudioBrowser();

    bool isAudioBrowserVisible() const;

    AudioBrowserController (const AudioBrowserController&) = delete;
    AudioBrowserController& operator= (const AudioBrowserController&) = delete;
    AudioBrowserController (AudioBrowserController&&) = delete;
    AudioBrowserController& operator= (AudioBrowserController&&) = delete;

private:
    struct AudioBrowserEntry
    {
        juce::File file;
        juce::String label;
        bool isDirectory = false;
        bool isParent = false;
    };

    juce::Component* parentComponent = nullptr;
    std::function<ple::PlaybackController*()> getPlaybackController;
    std::function<juce::File()> getAudioRootDirectory;
    std::function<juce::Rectangle<int>()> getAudioBrowserWindowBounds;
    std::function<void()> closePluginMenu;
    std::function<void()> closePluginWindow;
    std::function<void()> closeNowPlayingWindow;
    std::function<bool (const juce::File&)> loadAudioFile;
    std::function<void()> startPlayback;
    std::function<void()> syncPlaybackUi;
    std::function<void()> scheduleAudioBrowserDirectoryRefresh;
    std::function<void (const juce::String&)> setStatusText;

    std::vector<AudioBrowserEntry> audioBrowserEntries;
    std::unique_ptr<juce::Component> audioBrowserHost;
};
