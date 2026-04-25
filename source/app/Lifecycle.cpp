#include "App.h"
#include "Panel.h"
#include "Browser.h"
#include "PluginHost.h"
#include "LookAndFeel.h"
#include "Paths.h"

namespace
{
const auto uiAppBackground = juce::Colour (0xff000000);
constexpr float uiAppSideInsetRatio = 0.08f;

int getRelativeAppSideInset (int totalWidth) noexcept
{
    return juce::jmax (0, juce::roundToInt (totalWidth * uiAppSideInsetRatio));
}
}

MainComponent::MainComponent()
{
    playbackController = std::make_unique<ple::PlaybackController> (playbackState);

    refreshAudioLibrary();

    lookAndFeel = ple::makeMainLookAndFeel();
    setLookAndFeel (lookAndFeel.get());
    setOpaque (true);

    mainView = std::make_unique<MainView> (
        [this] { playPreviousTrack(); },
        [this] { togglePlayback(); },
        [this] { playNextTrack(); },
        [this] { cyclePlaybackMode(); },
        [this] { choosePlugin(); },
        [this] { openPluginGui(); },
        [this] { browseAudioFiles(); });

    addAndMakeVisible (*mainView);

    browserController = std::make_unique<BrowserController>();
    pluginHostController = std::make_shared<PluginHostController>();

    browserController->initialise ({
        *this,
        playbackState,
        [this] { return playbackController.get(); },
        [] { return ple::getAudioRootDirectory(); },
        [this]
        {
            if (mainView == nullptr)
                return getLocalBounds();

            return mainView->getPluginWindowBounds().translated (mainView->getX(), mainView->getY());
        },
        [this]
        {
            if (pluginHostController != nullptr)
                pluginHostController->closePluginMenu();
        },
        [this]
        {
            if (pluginHostController != nullptr)
                pluginHostController->closePluginWindow();
        },
        [this] (const juce::File& file)
        {
            return loadAudioFile (file);
        },
        [this]
        {
            startPlayback();
        },
        [this]
        {
            syncPlaybackUi();
        },
        [this]
        {
            scheduleAudioBrowserDirectoryRefresh();
        },
        [this] (const juce::String& text)
        {
            setStatusText (text);
        }
    });

    pluginHostController->initialise ({
        *this,
        playbackState,
        [this] { return playbackController.get(); },
        [this]
        {
            if (browserController != nullptr)
                browserController->closeAudioBrowser();
        },
        [this] (const juce::String& text)
        {
            setStatusText (text);
        },
        [this] (bool enabled)
        {
            setChoosePluginEnabled (enabled);
        },
        [this] (bool enabled)
        {
            setOpenPluginGuiEnabled (enabled);
        },
        [this] (const juce::String& text)
        {
            setOpenPluginGuiText (text);
        },
        [this]
        {
            syncPlaybackUi();
        },
        [this]
        {
            if (mainView == nullptr)
                return getLocalBounds();

            return mainView->getPluginWindowBounds().translated (mainView->getX(), mainView->getY());
        }
    });

    setSize (420, 248);
    setAudioChannels (0, 2);
    startTimerHz (4);
    updatePlaybackModeButton();

    pluginHostController->refreshInstalledPluginDescriptions();

    const auto diagnosticsFile = ple::getAudioRootDirectory().getChildFile ("installed-auv3.txt");
    if (diagnosticsFile.existsAsFile())
        diagnosticsFile.deleteFile();

    const auto automationPluginQuery = juce::SystemStats::getEnvironmentVariable ("PLE_AUTOMATION_PLUGIN", {}).trim();
    const auto automationOpenGuiAfterLoad = juce::SystemStats::getEnvironmentVariable ("PLE_AUTOMATION_OPEN_GUI", "0").trim() != "0";
    const auto automationShowPluginMenu = juce::SystemStats::getEnvironmentVariable ("PLE_AUTOMATION_SHOW_MENU", "0").trim() != "0";
    automationPlayOnLaunch = juce::SystemStats::getEnvironmentVariable ("PLE_AUTOMATION_PLAY", "0").trim() != "0";

    if (const auto* automationPlugin = pluginHostController->findPluginDescriptionForQuery (automationPluginQuery))
        pluginHostController->loadPluginDescription (*automationPlugin, automationOpenGuiAfterLoad);

    if (automationShowPluginMenu)
    {
        juce::MessageManager::callAsync ([safeThis = juce::Component::SafePointer<MainComponent> (this)]
        {
            if (safeThis != nullptr)
                safeThis->choosePlugin();
        });
    }

    if (automationPlayOnLaunch)
    {
        juce::MessageManager::callAsync ([safeThis = juce::Component::SafePointer<MainComponent> (this)]
        {
            if (safeThis != nullptr)
                safeThis->startPlayback();
        });
    }
}

MainComponent::~MainComponent()
{
    stopTimer();

    if (browserController != nullptr)
        browserController->reset();

    if (pluginHostController != nullptr)
        pluginHostController->reset();

    playbackController.reset();
    setLookAndFeel (nullptr);
    shutdownAudio();
}

void MainComponent::timerCallback()
{
#if JUCE_IOS
    if (playbackController == nullptr
        || ! playbackController->isPlaybackActive()
        || playbackState.playbackFinishedHandled
        || playbackController->getDuration() <= 0.0)
        return;

    if (playbackState.transportSource.isPlaying())
        return;

    const auto currentPosition = playbackController->getCurrentPosition();

    if (currentPosition + 0.05 < playbackController->getDuration())
        return;

    handlePlaybackFinished();
#endif
}

void MainComponent::prepareToPlay (int samplesPerBlockExpected, double sampleRate)
{
    if (playbackController != nullptr)
        playbackController->prepareToPlay (samplesPerBlockExpected, sampleRate);
}

void MainComponent::getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill)
{
    if (playbackController != nullptr)
        playbackController->getNextAudioBlock (bufferToFill);
    else
        bufferToFill.clearActiveBufferRegion();
}

void MainComponent::releaseResources()
{
    if (playbackController != nullptr)
        playbackController->releaseResources();
}

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (uiAppBackground);
}

void MainComponent::resized()
{
    if (mainView != nullptr)
    {
        auto mainViewBounds = getLocalBounds();
        const auto sideInset = getRelativeAppSideInset (getWidth());
        mainViewBounds.removeFromLeft (sideInset);
        mainViewBounds.removeFromRight (sideInset);
        mainView->setBounds (mainViewBounds);
    }

    if (pluginHostController != nullptr)
        pluginHostController->resized();

    if (browserController != nullptr)
        browserController->resized();
}
