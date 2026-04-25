#pragma once

#include "Playback.h"

#include <functional>
#include <memory>
#include <vector>

class PluginHostController final : public std::enable_shared_from_this<PluginHostController>
{
public:
    struct Dependencies
    {
        juce::Component& parentComponent;
        ple::PlaybackState& playbackState;
        std::function<ple::PlaybackController*()> getPlaybackController;
        std::function<void()> closeAudioBrowser;
        std::function<void (const juce::String&)> setStatusText;
        std::function<void (bool)> setChoosePluginEnabled;
        std::function<void (bool)> setOpenPluginGuiEnabled;
        std::function<void (const juce::String&)> setOpenPluginGuiText;
        std::function<void()> syncPlaybackUi;
        std::function<juce::Rectangle<int>()> getPluginWindowBounds;
    };

    PluginHostController() = default;

    void initialise (Dependencies dependencies);
    void reset();

    void refreshInstalledPluginDescriptions();
    const std::vector<juce::PluginDescription>& getInstalledPluginDescriptions() const;
    const juce::PluginDescription* findPluginDescriptionForQuery (const juce::String& query) const;

    void choosePlugin();
    void handlePluginMenuSelection (int selectedIndex);
    void loadPluginDescription (const juce::PluginDescription& description, bool openGuiAfterLoad = false);
    void openPluginGui();
    void closePluginMenu();
    void closePluginWindow();
    void resized();
    int getSelectedPluginIndex() const;

    PluginHostController (const PluginHostController&) = delete;
    PluginHostController& operator= (const PluginHostController&) = delete;
    PluginHostController (PluginHostController&&) = delete;
    PluginHostController& operator= (PluginHostController&&) = delete;

private:
    void ensurePluginWindowHost();
    void showPluginWindow();
    void destroyPluginWindow();

    juce::Component* parentComponent = nullptr;
    ple::PlaybackState* playbackState = nullptr;
    std::function<ple::PlaybackController*()> getPlaybackController;
    std::function<void()> closeAudioBrowser;
    std::function<void (const juce::String&)> setStatusText;
    std::function<void (bool)> setChoosePluginEnabled;
    std::function<void (bool)> setOpenPluginGuiEnabled;
    std::function<void (const juce::String&)> setOpenPluginGuiText;
    std::function<void()> syncPlaybackUi;
    std::function<juce::Rectangle<int>()> getPluginWindowBounds;

    juce::Component pluginWindowAnchor;
    std::unique_ptr<juce::Component> pluginWindowHost;
    std::unique_ptr<juce::Component> pluginMenuHost;
    bool pluginWindowVisible = false;

    std::vector<juce::PluginDescription> installedPluginDescriptions;
    juce::String currentPluginIdentifier;
    int pluginLoadToken = 0;
};
