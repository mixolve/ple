#include "MainComponent.h"
#include "app/LockScreenController.h"
#include "ui/MainView.h"
#include "ui/PopupViews.h"
#include "browser/AudioBrowserController.h"
#include "plugins/AudioUnitPluginHost.h"
#include "ui/PleLookAndFeel.h"
#include "audio/AudioFiles.h"

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
        [this] { openNowPlayingWindow(); },
        [this] { openAboutWindow(); },
        [this] { browseAudioFiles(); });

    addAndMakeVisible (*mainView);

    audioBrowser = std::make_unique<AudioBrowserController>();
    pluginHost = std::make_shared<AudioUnitPluginHost>();
    lockScreenController = std::make_unique<ple::LockScreenController> (ple::LockScreenController::Callbacks {
        [this] { startPlayback(); },
        [this] { pausePlayback(); },
        [this] { playNextTrack(); },
        [this] { playPreviousTrack(); },
        [this] (double positionSeconds) { seekPlayback (positionSeconds); }
    });
    lockScreenController->activate();

    audioBrowser->initialise ({
        *this,
        [this] { return playbackController.get(); },
        [] { return ple::getAudioRootDirectory(); },
        [this]
        {
            if (mainView == nullptr)
                return getLocalBounds();

            return mainView->getAudioBrowserWindowBounds().translated (mainView->getX(), mainView->getY());
        },
        [this]
        {
            if (pluginHost != nullptr)
                pluginHost->closePluginMenu();
        },
        [this]
        {
            if (pluginHost != nullptr)
                pluginHost->closePluginWindow();
        },
        [this]
        {
            closeNowPlayingWindow();
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

    pluginHost->initialise ({
        *this,
        [this] { return playbackController.get(); },
        [this]
        {
            if (audioBrowser != nullptr)
                audioBrowser->closeAudioBrowser();
        },
        [this]
        {
            closeNowPlayingWindow();
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

    choosePluginButton.setButtonText ("LIST");
    choosePluginButton.onClick = [this]
    {
        choosePlugin();
    };
    choosePluginButton.setWantsKeyboardFocus (false);
    choosePluginButton.setMouseClickGrabsKeyboardFocus (false);
    choosePluginButton.setAlwaysOnTop (true);
    addAndMakeVisible (choosePluginButton);

    setSize (420, 248);
    ensureAudioOutputActive();
    startTimerHz (4);
    updatePlaybackModeButton();

    pluginHost->refreshInstalledPluginDescriptions();

    const auto diagnosticsFile = ple::getAudioRootDirectory().getChildFile ("installed-auv3.txt");
    if (diagnosticsFile.existsAsFile())
        diagnosticsFile.deleteFile();

    const auto automationPluginQuery = juce::SystemStats::getEnvironmentVariable ("PLE_AUTOMATION_PLUGIN", {}).trim();
    const auto automationOpenGuiAfterLoad = juce::SystemStats::getEnvironmentVariable ("PLE_AUTOMATION_OPEN_GUI", "0").trim() != "0";
    const auto automationShowPluginMenu = juce::SystemStats::getEnvironmentVariable ("PLE_AUTOMATION_SHOW_MENU", "0").trim() != "0";
    const auto automationPlayOnLaunch = juce::SystemStats::getEnvironmentVariable ("PLE_AUTOMATION_PLAY", "0").trim() != "0";

    if (const auto* automationPlugin = pluginHost->findPluginDescriptionForQuery (automationPluginQuery))
        pluginHost->loadPluginDescription (*automationPlugin, automationOpenGuiAfterLoad);

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

    if (audioBrowser != nullptr)
        audioBrowser->reset();

    if (pluginHost != nullptr)
        pluginHost->reset();

    closeNowPlayingWindow();
    closeAboutWindow();

    suspendAudioOutputForPause();
    shutdownAudio();
    audioOutputActive = false;
    lockScreenController.reset();
    playbackController.reset();
    setLookAndFeel (nullptr);
}

void MainComponent::timerCallback()
{
#if JUCE_IOS
    if (playbackController != nullptr && playbackController->hasCurrentTrackEnded())
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

    audioOutputActive = false;
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

        choosePluginButton.setBounds (mainView->getChoosePluginButtonBounds().translated (mainView->getX(), mainView->getY()));

        if (nowPlayingWindowHost != nullptr)
            nowPlayingWindowHost->setBounds (mainView->getNowPlayingWindowBounds().translated (mainView->getX(), mainView->getY()));
    }

    if (aboutWindowHost != nullptr && mainView != nullptr)
        aboutWindowHost->setBounds (mainView->getPluginWindowBounds().translated (mainView->getX(), mainView->getY()));

    if (pluginHost != nullptr)
        pluginHost->resized();

    if (audioBrowser != nullptr)
        audioBrowser->resized();
}

void MainComponent::setStatusText (const juce::String& text)
{
    if (mainView != nullptr)
        mainView->setStatusText (text);
}

void MainComponent::setPlaybackModeText (const juce::String& text)
{
    if (mainView != nullptr)
        mainView->setPlaybackModeText (text);
}

void MainComponent::setChoosePluginEnabled (bool enabled)
{
    if (mainView != nullptr)
        mainView->setChoosePluginEnabled (enabled);

    choosePluginButton.setEnabled (enabled);
}

void MainComponent::setOpenPluginGuiEnabled (bool enabled)
{
    if (mainView != nullptr)
        mainView->setOpenPluginGuiEnabled (enabled);
}

void MainComponent::setOpenPluginGuiText (const juce::String& text)
{
    if (mainView != nullptr)
        mainView->setOpenPluginGuiText (text);
}

void MainComponent::syncPlaybackUi()
{
    updatePlaybackModeButton();

    if (mainView != nullptr)
        mainView->setPlaybackActive (playbackController != nullptr && playbackController->isPlaybackActive());

    if (playbackController != nullptr)
    {
        setStatusText (playbackController->getStatusText());
        setOpenPluginGuiEnabled (playbackController->hasPluginInstance());
        refreshNowPlayingWindow();

        if (lockScreenController != nullptr)
            lockScreenController->updateNowPlaying (playbackController->getNowPlayingTrack());
    }
}

void MainComponent::browseAudioFiles()
{
    closeAboutWindow();

    if (audioBrowser != nullptr)
        audioBrowser->browseAudioFiles();
}

void MainComponent::refreshAudioBrowserDirectory()
{
    if (audioBrowser != nullptr)
        audioBrowser->refreshAudioBrowserDirectory();
}

void MainComponent::scheduleAudioBrowserDirectoryRefresh()
{
#if JUCE_IOS
    if (audioBrowserDirectoryRefreshPending)
        return;

    audioBrowserDirectoryRefreshPending = true;

    juce::MessageManager::callAsync ([safeThis = juce::Component::SafePointer<MainComponent> (this)]
    {
        if (safeThis == nullptr)
            return;

        safeThis->audioBrowserDirectoryRefreshPending = false;

        if (safeThis->audioBrowser != nullptr && safeThis->audioBrowser->isAudioBrowserVisible())
            safeThis->refreshAudioBrowserDirectory();
    });
#endif
}

void MainComponent::choosePlugin()
{
    closeAboutWindow();

    if (pluginHost != nullptr)
        pluginHost->choosePlugin();
}

void MainComponent::openPluginGui()
{
    closeAboutWindow();

    if (pluginHost != nullptr)
        pluginHost->openPluginGui();
}

void MainComponent::openAboutWindow()
{
    if (aboutWindowHost != nullptr)
    {
        closeAboutWindow();
        return;
    }

    if (pluginHost != nullptr)
    {
        pluginHost->closePluginMenu();
        pluginHost->closePluginWindow();
    }

    if (audioBrowser != nullptr)
        audioBrowser->closeAudioBrowser();

    closeNowPlayingWindow();

    auto aboutContent = std::make_unique<AboutContent>();
    aboutWindowHost = std::make_unique<PluginWindowFrame> (std::move (aboutContent));

    addAndMakeVisible (*aboutWindowHost);

    if (mainView != nullptr)
        aboutWindowHost->setBounds (mainView->getPluginWindowBounds().translated (mainView->getX(), mainView->getY()));

    aboutWindowHost->toFront (true);
}

void MainComponent::closeAboutWindow()
{
    if (aboutWindowHost == nullptr)
        return;

    aboutWindowHost->setVisible (false);
    aboutWindowHost.reset();
}

void MainComponent::openNowPlayingWindow()
{
    closeAboutWindow();

    if (nowPlayingWindowHost != nullptr)
    {
        closeNowPlayingWindow();

        setStatusText ("now window closed");

        return;
    }

    if (pluginHost != nullptr)
    {
        pluginHost->closePluginMenu();
        pluginHost->closePluginWindow();
    }

    if (audioBrowser != nullptr)
        audioBrowser->closeAudioBrowser();

    if (playbackController == nullptr || mainView == nullptr)
        return;

    auto nowPlayingContentToOwn = std::make_unique<NowPlayingContent>();
    nowPlayingContent = nowPlayingContentToOwn.get();
    nowPlayingContent->setTrack (playbackController->getNowPlayingTrack());

    nowPlayingWindowHost = std::make_unique<PluginWindowFrame> (std::move (nowPlayingContentToOwn));
    addAndMakeVisible (*nowPlayingWindowHost);

    const auto nowPlayingBounds = mainView->getNowPlayingWindowBounds().translated (mainView->getX(), mainView->getY());
    nowPlayingWindowHost->setBounds (nowPlayingBounds);
    nowPlayingWindowHost->toFront (true);

    setStatusText ("now window opened");
}

void MainComponent::closeNowPlayingWindow()
{
    nowPlayingContent = nullptr;

    if (nowPlayingWindowHost != nullptr)
        nowPlayingWindowHost->setVisible (false);

    nowPlayingWindowHost.reset();
}

void MainComponent::refreshNowPlayingWindow()
{
    if (nowPlayingContent != nullptr && playbackController != nullptr)
        nowPlayingContent->setTrack (playbackController->getNowPlayingTrack());
}

void MainComponent::refreshAudioLibrary()
{
    if (playbackController != nullptr)
        playbackController->refreshAudioLibrary();
}

bool MainComponent::loadAudioFile (const juce::File& file)
{
    if (playbackController == nullptr)
        return false;

    const auto loaded = playbackController->loadAudioFile (file);

    syncPlaybackUi();

    return loaded;
}

void MainComponent::playPreviousTrack()
{
    if (playbackController == nullptr)
        return;

    playbackController->playPreviousTrack();

    if (audioBrowser != nullptr && audioBrowser->isAudioBrowserVisible())
        scheduleAudioBrowserDirectoryRefresh();

    syncPlaybackUi();
}

void MainComponent::playNextTrack()
{
    if (playbackController == nullptr)
        return;

    playbackController->playNextTrack();

    if (audioBrowser != nullptr && audioBrowser->isAudioBrowserVisible())
        scheduleAudioBrowserDirectoryRefresh();

    syncPlaybackUi();
}

juce::String MainComponent::getPlaybackModeLabel() const
{
    return playbackController != nullptr ? playbackController->getPlaybackModeLabel() : "ALL";
}

void MainComponent::updatePlaybackModeButton()
{
    setPlaybackModeText (getPlaybackModeLabel());
}

void MainComponent::startPlayback()
{
    if (playbackController == nullptr)
        return;

    ensureAudioOutputActive();
    playbackController->startPlayback();
    syncPlaybackUi();
}

void MainComponent::pausePlayback()
{
    if (playbackController == nullptr)
        return;

    playbackController->pausePlayback();
    syncPlaybackUi();
    suspendAudioOutputForPause();
}

void MainComponent::ensureAudioOutputActive()
{
    if (lockScreenController != nullptr)
        lockScreenController->setAudioSessionActive (true);

    if (audioOutputActive)
        return;

    setAudioChannels (0, 2);
    audioOutputActive = true;
}

void MainComponent::suspendAudioOutputForPause()
{
    audioOutputActive = false;

    if (lockScreenController == nullptr)
        return;

    juce::MessageManager::callAsync ([safeThis = juce::Component::SafePointer<MainComponent> (this)]
    {
        if (safeThis == nullptr || safeThis->audioOutputActive)
            return;

        if (safeThis->lockScreenController != nullptr)
            safeThis->lockScreenController->setAudioSessionActive (false);
    });
}

void MainComponent::seekPlayback (double positionSeconds)
{
    if (playbackController == nullptr)
        return;

    playbackController->seekTo (positionSeconds);
    syncPlaybackUi();
}

void MainComponent::togglePlayback()
{
    if (playbackController != nullptr && playbackController->isPlaybackActive())
        pausePlayback();
    else
        startPlayback();
}

void MainComponent::cyclePlaybackMode()
{
    if (playbackController == nullptr)
        return;

    playbackController->cyclePlaybackMode();
    syncPlaybackUi();
}

void MainComponent::handlePlaybackFinished()
{
    if (playbackController == nullptr)
        return;

    playbackController->handlePlaybackFinished();

    if (audioBrowser != nullptr && audioBrowser->isAudioBrowserVisible())
        scheduleAudioBrowserDirectoryRefresh();

    syncPlaybackUi();
}
