#pragma once

#include "Playback.h"

#include <functional>
#include <memory>
#include <vector>

class BrowserController final
{
public:
    struct Dependencies
    {
        juce::Component& parentComponent;
        ple::PlaybackState& playbackState;
        std::function<ple::PlaybackController*()> getPlaybackController;
        std::function<juce::File()> getAudioRootDirectory;
        std::function<juce::Rectangle<int>()> getAudioBrowserWindowBounds;
        std::function<void()> closePluginMenu;
        std::function<void()> closePluginWindow;
        std::function<bool (const juce::File&)> loadAudioFile;
        std::function<void()> startPlayback;
        std::function<void()> syncPlaybackUi;
        std::function<void()> scheduleAudioBrowserDirectoryRefresh;
        std::function<void (const juce::String&)> setStatusText;
    };

    BrowserController() = default;

    void initialise (Dependencies dependencies);
    void reset();

    void browseAudioFiles();
    void refreshAudioBrowserDirectory();
    void handleAudioBrowserSelection (int selectedIndex);
    void handleAudioBrowserFolderPlaySelection (int selectedIndex);
    void resized();
    void closeAudioBrowser();

    bool isAudioBrowserVisible() const;

    BrowserController (const BrowserController&) = delete;
    BrowserController& operator= (const BrowserController&) = delete;
    BrowserController (BrowserController&&) = delete;
    BrowserController& operator= (BrowserController&&) = delete;

private:
    struct AudioBrowserEntry
    {
        juce::File file;
        juce::String label;
        bool isDirectory = false;
        bool isParent = false;
    };

    juce::Component* parentComponent = nullptr;
    ple::PlaybackState* playbackState = nullptr;
    std::function<ple::PlaybackController*()> getPlaybackController;
    std::function<juce::File()> getAudioRootDirectory;
    std::function<juce::Rectangle<int>()> getAudioBrowserWindowBounds;
    std::function<void()> closePluginMenu;
    std::function<void()> closePluginWindow;
    std::function<bool (const juce::File&)> loadAudioFile;
    std::function<void()> startPlayback;
    std::function<void()> syncPlaybackUi;
    std::function<void()> scheduleAudioBrowserDirectoryRefresh;
    std::function<void (const juce::String&)> setStatusText;

    std::vector<AudioBrowserEntry> audioBrowserEntries;
    std::unique_ptr<juce::Component> audioBrowserHost;
};
