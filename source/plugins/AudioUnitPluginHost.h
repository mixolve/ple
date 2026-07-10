#pragma once

#include "audio/PlaybackController.h"

#include <functional>
#include <memory>
#include <vector>

class AudioUnitPluginHost final : public std::enable_shared_from_this<AudioUnitPluginHost>
{
public:
    struct Dependencies
    {
        juce::Component& parentComponent;
        std::function<ple::PlaybackController*()> getPlaybackController;
        std::function<void()> closeAudioBrowser;
        std::function<void()> closeNowPlayingWindow;
        std::function<void (const juce::String&)> setStatusText;
        std::function<void (bool)> setOpenPluginGuiEnabled;
        std::function<void (const juce::String&)> setOpenPluginGuiText;
        std::function<void()> syncPlaybackUi;
        std::function<juce::Rectangle<int>()> getPluginWindowBounds;
    };

    AudioUnitPluginHost() = default;

    void initialise (Dependencies dependencies);
    void reset();

    void refreshInstalledPluginDescriptions();
    const juce::PluginDescription* findPluginDescriptionForQuery (const juce::String& query) const;

    void choosePlugin();
    void handlePluginMenuSelection (int selectedIndex);
    void loadPluginDescription (const juce::PluginDescription& description,
                                bool openGuiAfterLoad = false,
                                bool restoreSavedState = false);
    void restoreLastPlugin (bool openGuiAfterLoad = false);
    void unloadPlugin();
    void openPluginGui();
    void closePluginMenu();
    void closePluginWindow();
    void resized();
    int getSelectedPluginIndex() const;
    bool isPluginMenuVisible() const;
    bool isPluginWindowVisible() const;
    bool isPluginSurfaceVisible() const;

    AudioUnitPluginHost (const AudioUnitPluginHost&) = delete;
    AudioUnitPluginHost& operator= (const AudioUnitPluginHost&) = delete;
    AudioUnitPluginHost (AudioUnitPluginHost&&) = delete;
    AudioUnitPluginHost& operator= (AudioUnitPluginHost&&) = delete;

private:
    class PluginWindowAnchor final : public juce::Component
    {
    public:
        void paint (juce::Graphics& g) override;
    };

    void setPluginStatus (const juce::String& text) const;
    void setPluginGuiButtonText (const juce::String& text) const;
    juce::Rectangle<int> getResolvedPluginWindowBounds() const;
    void closeTransientSurfaces();
    void armPluginWindowPaintCallback();
    bool ensurePluginWindowHost();
    void showPluginWindow();
    void destroyPluginWindow();
    void showPluginTransitionCover();
    void hidePluginTransitionCover();
    void saveCurrentPluginState() const;
    juce::MemoryBlock loadSavedPluginState (const juce::PluginDescription& description) const;
    const juce::PluginDescription* findSavedPluginDescription() const;

    juce::Component* parentComponent = nullptr;
    std::function<ple::PlaybackController*()> getPlaybackController;
    std::function<void()> closeAudioBrowser;
    std::function<void()> closeNowPlayingWindow;
    std::function<void (const juce::String&)> setStatusText;
    std::function<void (bool)> setOpenPluginGuiEnabled;
    std::function<void (const juce::String&)> setOpenPluginGuiText;
    std::function<void()> syncPlaybackUi;
    std::function<juce::Rectangle<int>()> getPluginWindowBounds;

    PluginWindowAnchor pluginWindowAnchor;
    std::unique_ptr<juce::Component> pluginWindowHost;
    std::unique_ptr<juce::Component> pluginMenuHost;
    std::unique_ptr<juce::Component> pluginTransitionCover;
    bool pluginWindowVisible = false;

    juce::AudioUnitPluginFormat audioUnitFormat;
    std::vector<juce::PluginDescription> installedPluginDescriptions;
    juce::PluginDescription currentPluginDescription;
    juce::String currentPluginIdentifier;
    bool hasCurrentPluginDescription = false;
    int pluginLoadToken = 0;
};
