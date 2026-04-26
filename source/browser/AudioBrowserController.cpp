#include "browser/AudioBrowserController.h"

#include "audio/AudioFiles.h"
#include "ui/PopupViews.h"

#include <algorithm>
#include <utility>

void AudioBrowserController::initialise (Dependencies dependencies)
{
    parentComponent = &dependencies.parentComponent;
    getPlaybackController = std::move (dependencies.getPlaybackController);
    getAudioRootDirectory = std::move (dependencies.getAudioRootDirectory);
    getAudioBrowserWindowBounds = std::move (dependencies.getAudioBrowserWindowBounds);
    closePluginMenu = std::move (dependencies.closePluginMenu);
    closePluginWindow = std::move (dependencies.closePluginWindow);
    closeNowPlayingWindow = std::move (dependencies.closeNowPlayingWindow);
    loadAudioFile = std::move (dependencies.loadAudioFile);
    startPlayback = std::move (dependencies.startPlayback);
    syncPlaybackUi = std::move (dependencies.syncPlaybackUi);
    scheduleAudioBrowserDirectoryRefresh = std::move (dependencies.scheduleAudioBrowserDirectoryRefresh);
    setStatusText = std::move (dependencies.setStatusText);
}

void AudioBrowserController::reset()
{
    closeAudioBrowser();
    audioBrowserEntries.clear();
}

void AudioBrowserController::closeAudioBrowser()
{
    if (audioBrowserHost != nullptr)
        audioBrowserHost->setVisible (false);

    audioBrowserHost.reset();
}

bool AudioBrowserController::isAudioBrowserVisible() const
{
    return audioBrowserHost != nullptr;
}

void AudioBrowserController::browseAudioFiles()
{
    if (closePluginMenu)
        closePluginMenu();

    if (closePluginWindow)
        closePluginWindow();

    if (closeNowPlayingWindow)
        closeNowPlayingWindow();

    if (audioBrowserHost != nullptr)
    {
        closeAudioBrowser();

        if (setStatusText)
            setStatusText ("file browser closed");

        return;
    }

    if (auto* playbackController = getPlaybackController != nullptr ? getPlaybackController() : nullptr)
        playbackController->setAudioBrowserDirectory (getAudioRootDirectory != nullptr ? getAudioRootDirectory() : juce::File());

    refreshAudioBrowserDirectory();
}

void AudioBrowserController::refreshAudioBrowserDirectory()
{
    audioBrowserEntries.clear();

    auto* playbackController = getPlaybackController != nullptr ? getPlaybackController() : nullptr;

    if (playbackController == nullptr)
        return;

    auto audioBrowserDirectory = playbackController->getAudioBrowserDirectory();
    const auto audioRootDirectory = getAudioRootDirectory != nullptr ? getAudioRootDirectory() : juce::File();

    if (! audioBrowserDirectory.exists() || ! audioBrowserDirectory.isDirectory())
    {
        audioBrowserDirectory = audioRootDirectory;
        playbackController->setAudioBrowserDirectory (audioBrowserDirectory);
    }

    if (audioBrowserDirectory != audioRootDirectory)
        audioBrowserEntries.push_back ({ audioBrowserDirectory.getParentDirectory(), "..", true, true });

    juce::Array<juce::File> folders;
    juce::Array<juce::File> files;
    audioBrowserDirectory.findChildFiles (folders, juce::File::findDirectories, false);
    audioBrowserDirectory.findChildFiles (files, juce::File::findFiles, false);

    std::sort (folders.begin(), folders.end(), [] (const juce::File& left, const juce::File& right)
    {
        return left.getFileName().toLowerCase() < right.getFileName().toLowerCase();
    });

    std::sort (files.begin(), files.end(), [] (const juce::File& left, const juce::File& right)
    {
        return left.getFileName().toLowerCase() < right.getFileName().toLowerCase();
    });

    for (const auto& folder : folders)
    {
        if (! folder.getFileName().startsWithChar ('.'))
            audioBrowserEntries.push_back ({ folder, folder.getFileName() + "/", true, false });
    }

    for (const auto& file : files)
    {
        if (ple::isPlayableAudioFile (file))
            audioBrowserEntries.push_back ({ file, file.getFileName(), false, false });
    }

    const auto browserBounds = getAudioBrowserWindowBounds != nullptr ? getAudioBrowserWindowBounds()
                                                                       : parentComponent != nullptr ? parentComponent->getLocalBounds()
                                                                                                   : juce::Rectangle<int>();
    std::vector<FileBrowserContent::Row> browserRows;
    browserRows.reserve (audioBrowserEntries.size());

    const auto currentAudioFileName = playbackController->getCurrentAudioFileName();

    for (size_t index = 0; index < audioBrowserEntries.size(); ++index)
    {
        const auto& entry = audioBrowserEntries[index];
        const auto isSelected = currentAudioFileName.isNotEmpty()
                                && entry.file.getFullPathName().equalsIgnoreCase (currentAudioFileName);
        const auto isPathActive = entry.isDirectory
                                  && ! entry.isParent
                                  && currentAudioFileName.isNotEmpty()
                                  && juce::File (currentAudioFileName).isAChildOf (entry.file);

        browserRows.push_back ({ entry.label, isSelected, isPathActive, entry.isDirectory && ! entry.isParent });
    }

    auto createBrowserHost = [this, browserBounds] (std::vector<FileBrowserContent::Row> rows)
    {
        auto browserContent = std::make_unique<FileBrowserContent> (std::move (rows),
                                                                    [this] (int index)
                                                                    {
                                                                        handleAudioBrowserSelection (index);
                                                                    },
                                                                    [this] (int index)
                                                                    {
                                                                        handleAudioBrowserFolderPlaySelection (index);
                                                                    });

        browserContent->setSize (browserBounds.getWidth() - 2, browserBounds.getHeight() - 2);

        audioBrowserHost = std::make_unique<PluginWindowFrame> (std::move (browserContent));
        audioBrowserHost->setBounds (browserBounds);

        if (parentComponent != nullptr)
            parentComponent->addChildComponent (*audioBrowserHost);

        audioBrowserHost->setVisible (true);
        audioBrowserHost->toFront (true);
    };

    if (auto* browserFrame = dynamic_cast<PluginWindowFrame*> (audioBrowserHost.get()))
    {
        if (auto* browserContentView = dynamic_cast<FileBrowserContent*> (browserFrame->getContentComponent()))
        {
            audioBrowserHost->setBounds (browserBounds);
            browserContentView->setRows (std::move (browserRows));
            audioBrowserHost->toFront (true);
        }
        else
        {
            audioBrowserHost.reset();
            createBrowserHost (std::move (browserRows));
        }
    }
    else
    {
        createBrowserHost (std::move (browserRows));
    }

    if (setStatusText)
        setStatusText (audioBrowserDirectory == audioRootDirectory ? "browsing documents"
                                                                  : "browsing " + audioBrowserDirectory.getFileName());
}

void AudioBrowserController::handleAudioBrowserSelection (int selectedIndex)
{
    if (selectedIndex < 0 || selectedIndex >= static_cast<int> (audioBrowserEntries.size()))
        return;

    const auto entry = audioBrowserEntries[static_cast<size_t> (selectedIndex)];
    auto* playbackController = getPlaybackController != nullptr ? getPlaybackController() : nullptr;

    if (entry.isParent || entry.isDirectory)
    {
        if (playbackController != nullptr)
        {
            playbackController->setAudioBrowserDirectory (entry.file);
            refreshAudioBrowserDirectory();
        }

        return;
    }

    if (playbackController != nullptr)
    {
        playbackController->setPlaybackScopeDirectory (playbackController->getAudioBrowserDirectory());
        playbackController->refreshPlaybackQueue();
    }

    if (loadAudioFile && ! loadAudioFile (entry.file))
        return;

    if (startPlayback)
        startPlayback();

    if (isAudioBrowserVisible() && scheduleAudioBrowserDirectoryRefresh)
        scheduleAudioBrowserDirectoryRefresh();
}

void AudioBrowserController::handleAudioBrowserFolderPlaySelection (int selectedIndex)
{
    if (selectedIndex < 0 || selectedIndex >= static_cast<int> (audioBrowserEntries.size()))
        return;

    const auto entry = audioBrowserEntries[static_cast<size_t> (selectedIndex)];

    if (! entry.isDirectory || entry.isParent)
        return;

    auto* playbackController = getPlaybackController != nullptr ? getPlaybackController() : nullptr;

    if (playbackController != nullptr)
    {
        playbackController->setPlaybackScopeDirectory (entry.file);
        playbackController->setPlaybackMode (ple::PlaybackMode::repeatFolder);
        playbackController->refreshPlaybackQueue();
    }

    if (syncPlaybackUi)
        syncPlaybackUi();

    const auto scopedTracks = playbackController != nullptr ? playbackController->getCurrentFolderTracks()
                                                            : std::vector<juce::File> {};

    if (scopedTracks.empty())
        return;

    if (! loadAudioFile || ! loadAudioFile (scopedTracks.front()))
        return;

    if (startPlayback)
        startPlayback();

    if (isAudioBrowserVisible() && scheduleAudioBrowserDirectoryRefresh)
        scheduleAudioBrowserDirectoryRefresh();
}

void AudioBrowserController::resized()
{
    if (audioBrowserHost == nullptr)
        return;

    const auto browserBounds = getAudioBrowserWindowBounds != nullptr ? getAudioBrowserWindowBounds()
                                                                       : parentComponent != nullptr ? parentComponent->getLocalBounds()
                                                                                                   : juce::Rectangle<int>();
    audioBrowserHost->setBounds (browserBounds);
}
