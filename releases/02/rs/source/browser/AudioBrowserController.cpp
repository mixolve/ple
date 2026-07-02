#include "browser/AudioBrowserController.h"

#include "audio/AudioFiles.h"
#include "ui/PopupViews.h"

#include <algorithm>
#include <utility>

namespace
{
bool isRootReadmeFile (const juce::File& file)
{
    return file.getFileName().equalsIgnoreCase ("readme.md");
}

juce::String getActionStatusText (const juce::String& prefix, const juce::File& file)
{
    return prefix + " " + file.getFileName().toLowerCase();
}
}

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
    removeAudioFile = std::move (dependencies.removeAudioFile);
    setBrowserFileMarked = std::move (dependencies.setBrowserFileMarked);
    startPlayback = std::move (dependencies.startPlayback);
    syncPlaybackUi = std::move (dependencies.syncPlaybackUi);
    scheduleAudioBrowserDirectoryRefresh = std::move (dependencies.scheduleAudioBrowserDirectoryRefresh);
    setStatusText = std::move (dependencies.setStatusText);
}

void AudioBrowserController::reset()
{
    closeAudioBrowser();
    closeBrowserActionPopup();
    audioBrowserEntries.clear();
}

void AudioBrowserController::closeAudioBrowser()
{
    closeBrowserActionPopup();

    if (audioBrowserHost != nullptr)
        audioBrowserHost->setVisible (false);

    audioBrowserHost.reset();
}

void AudioBrowserController::closeBrowserActionPopup()
{
    if (browserActionHost != nullptr)
        browserActionHost->setVisible (false);

    browserActionHost.reset();
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
        closeBrowserActionPopup();

        if (setStatusText)
            setStatusText ("file browser closed");

        return;
    }

    refreshAudioBrowserDirectory();
}

void AudioBrowserController::refreshAudioBrowserDirectory()
{
    closeBrowserActionPopup();
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

        browserRows.push_back ({ entry.label,
                                 isSelected,
                                 isPathActive,
                                 entry.isDirectory && ! entry.isParent,
                                 ple::isBrowserFileMarked (entry.file) });
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
                                                                    },
                                                                    [this] (int index)
                                                                    {
                                                                        handleAudioBrowserLongPress (index);
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

void AudioBrowserController::updateAudioBrowserRowsFromCache()
{
    auto* playbackController = getPlaybackController != nullptr ? getPlaybackController() : nullptr;

    if (playbackController == nullptr || audioBrowserHost == nullptr)
        return;

    auto* browserFrame = dynamic_cast<PluginWindowFrame*> (audioBrowserHost.get());

    if (browserFrame == nullptr)
        return;

    auto* browserContentView = dynamic_cast<FileBrowserContent*> (browserFrame->getContentComponent());

    if (browserContentView == nullptr)
        return;

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

        browserRows.push_back ({ entry.label,
                                 isSelected,
                                 isPathActive,
                                 entry.isDirectory && ! entry.isParent,
                                 ple::isBrowserFileMarked (entry.file) });
    }

    browserContentView->setRows (std::move (browserRows));
}

void AudioBrowserController::handleAudioBrowserLongPress (int selectedIndex)
{
    if (selectedIndex < 0 || selectedIndex >= static_cast<int> (audioBrowserEntries.size()))
        return;

    const auto entry = audioBrowserEntries[static_cast<size_t> (selectedIndex)];

    if (entry.isParent || entry.isDirectory)
        return;

    const auto currentMarkState = ple::isBrowserFileMarked (entry.file);
    const auto canRemove = ! isRootReadmeFile (entry.file);
    const auto browserBounds = getAudioBrowserWindowBounds != nullptr ? getAudioBrowserWindowBounds()
                                                                      : parentComponent != nullptr ? parentComponent->getLocalBounds()
                                                                                                  : juce::Rectangle<int>();

    closeBrowserActionPopup();

    auto actionContent = std::make_unique<BrowserActionContent> (
        entry.label,
        currentMarkState,
        canRemove,
        [this, entry]
        {
            if (removeAudioFile == nullptr)
                return;

            auto* playbackController = getPlaybackController != nullptr ? getPlaybackController() : nullptr;
            const auto currentAudioFileName = playbackController != nullptr ? playbackController->getCurrentAudioFileName() : juce::String();

            if (playbackController != nullptr && currentAudioFileName.isNotEmpty() && juce::File (currentAudioFileName) == entry.file)
            {
                playbackController->pausePlayback();
                playbackController->clearNavigationHistory();
            }

            removeAudioFile (entry.file);

            if (playbackController != nullptr)
            {
                playbackController->refreshAudioLibrary();
                playbackController->refreshPlaybackQueue();
            }

            if (setStatusText)
                setStatusText (getActionStatusText ("removed", entry.file));

            if (isAudioBrowserVisible() && scheduleAudioBrowserDirectoryRefresh)
                scheduleAudioBrowserDirectoryRefresh();

            closeBrowserActionPopup();
        },
        [this, entry, currentMarkState]
        {
            if (setBrowserFileMarked != nullptr)
                setBrowserFileMarked (entry.file, ! currentMarkState);

            if (setStatusText)
                setStatusText (getActionStatusText (currentMarkState ? "unmarked" : "marked", entry.file));

            if (isAudioBrowserVisible())
                updateAudioBrowserRowsFromCache();

            closeBrowserActionPopup();
        },
        [this]
        {
            closeBrowserActionPopup();
        });

    actionContent->setSize (browserBounds.getWidth() - 2, browserBounds.getHeight() - 2);

    browserActionHost = std::make_unique<PluginWindowFrame> (std::move (actionContent));
    browserActionHost->setBounds (browserBounds);

    if (parentComponent != nullptr)
        parentComponent->addChildComponent (*browserActionHost);

    browserActionHost->setVisible (true);
    browserActionHost->toFront (true);
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

    if (isAudioBrowserVisible())
        updateAudioBrowserRowsFromCache();
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

    if (isAudioBrowserVisible())
        updateAudioBrowserRowsFromCache();
}

void AudioBrowserController::resized()
{
    const auto browserBounds = getAudioBrowserWindowBounds != nullptr ? getAudioBrowserWindowBounds()
                                                                      : parentComponent != nullptr ? parentComponent->getLocalBounds()
                                                                                                  : juce::Rectangle<int>();

    if (audioBrowserHost == nullptr)
    {
        if (browserActionHost != nullptr)
            browserActionHost->setBounds (browserBounds);

        return;
    }

    audioBrowserHost->setBounds (browserBounds);

    if (browserActionHost != nullptr)
        browserActionHost->setBounds (browserBounds);
}
