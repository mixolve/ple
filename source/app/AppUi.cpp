#include "App.h"
#include "Panel.h"
#include "Browser.h"
#include "PluginHost.h"

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
    }
}

void MainComponent::browseAudioFiles()
{
    if (browserController != nullptr)
        browserController->browseAudioFiles();
}

void MainComponent::refreshAudioBrowserDirectory()
{
    if (browserController != nullptr)
        browserController->refreshAudioBrowserDirectory();
}

void MainComponent::handleAudioBrowserSelection (int selectedIndex)
{
    if (browserController != nullptr)
        browserController->handleAudioBrowserSelection (selectedIndex);
}

void MainComponent::handleAudioBrowserFolderPlaySelection (int selectedIndex)
{
    if (browserController != nullptr)
        browserController->handleAudioBrowserFolderPlaySelection (selectedIndex);
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

        if (safeThis->browserController != nullptr && safeThis->browserController->isAudioBrowserVisible())
            safeThis->refreshAudioBrowserDirectory();
    });
#endif
}

void MainComponent::choosePlugin()
{
    if (pluginHostController != nullptr)
        pluginHostController->choosePlugin();
}

void MainComponent::handlePluginMenuSelection (int selectedIndex)
{
    if (pluginHostController != nullptr)
        pluginHostController->handlePluginMenuSelection (selectedIndex);
}

void MainComponent::loadPluginDescription (const juce::PluginDescription& description, bool openGuiAfterLoad)
{
    if (pluginHostController != nullptr)
        pluginHostController->loadPluginDescription (description, openGuiAfterLoad);
}

void MainComponent::openPluginGui()
{
    if (pluginHostController != nullptr)
        pluginHostController->openPluginGui();
}
