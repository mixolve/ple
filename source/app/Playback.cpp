#include "App.h"
#include "Browser.h"

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

    if (browserController != nullptr && browserController->isAudioBrowserVisible())
        scheduleAudioBrowserDirectoryRefresh();

    syncPlaybackUi();
}

void MainComponent::playNextTrack()
{
    if (playbackController == nullptr)
        return;

    playbackController->playNextTrack();

    if (browserController != nullptr && browserController->isAudioBrowserVisible())
        scheduleAudioBrowserDirectoryRefresh();

    syncPlaybackUi();
}

void MainComponent::restartCurrentTrack()
{
    if (playbackController != nullptr)
        playbackController->restartCurrentTrack();
}

juce::String MainComponent::getPlaybackModeLabel() const
{
    return playbackController != nullptr ? playbackController->getPlaybackModeLabel() : "ALL";
}

int MainComponent::getTrackIndexForNavigation (bool movingForward) const
{
    return playbackController != nullptr ? playbackController->getTrackIndexForNavigation (movingForward) : -1;
}

void MainComponent::updatePlaybackModeButton()
{
    setPlaybackModeText (getPlaybackModeLabel());
}

void MainComponent::startPlayback()
{
    if (playbackController == nullptr)
        return;

    playbackController->startPlayback();
    syncPlaybackUi();
}

void MainComponent::pausePlayback()
{
    if (playbackController == nullptr)
        return;

    playbackController->pausePlayback();
    syncPlaybackUi();
}

void MainComponent::togglePlayback()
{
    if (playbackController != nullptr && playbackController->isPlaybackActive())
        pausePlayback();
    else
        startPlayback();
}

bool MainComponent::isPlaybackActive() const
{
    return playbackController != nullptr && playbackController->isPlaybackActive();
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

    if (browserController != nullptr && browserController->isAudioBrowserVisible())
        scheduleAudioBrowserDirectoryRefresh();

    syncPlaybackUi();
}
