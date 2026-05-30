#import <AVFAudio/AVAudioUnitComponent.h>

#include "plugins/AudioUnitPluginHost.h"

#include "ui/PopupViews.h"

#include <algorithm>
#include <set>
#include <utility>

namespace
{
const auto pluginTransitionCoverColour = juce::Colour (0xff000000);

class TransitionCover final : public juce::Component
{
public:
    TransitionCover()
    {
        setOpaque (true);
        setInterceptsMouseClicks (false, false);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (pluginTransitionCoverColour);
    }
};

static juce::String fourCharCodeToString (OSType type)
{
    const char characters[5]
    {
        static_cast<char> ((type >> 24) & 0xff),
        static_cast<char> ((type >> 16) & 0xff),
        static_cast<char> ((type >> 8) & 0xff),
        static_cast<char> (type & 0xff),
        0
    };

    return juce::String::fromUTF8 (characters);
}

static juce::String makeAudioUnitIdentifier (const AudioComponentDescription& desc)
{
    juce::String identifier ("AudioUnit:");

    if (desc.componentType == kAudioUnitType_MusicDevice)
        identifier << "Synths/";
    else if (desc.componentType == kAudioUnitType_MusicEffect
             || desc.componentType == kAudioUnitType_Effect)
        identifier << "Effects/";
    else if (desc.componentType == kAudioUnitType_Generator)
        identifier << "Generators/";
    else if (desc.componentType == kAudioUnitType_Panner)
        identifier << "Panners/";
    else if (desc.componentType == kAudioUnitType_Mixer)
        identifier << "Mixers/";
    else if (desc.componentType == kAudioUnitType_MIDIProcessor)
        identifier << "MidiEffects/";

    identifier << fourCharCodeToString (desc.componentType) << ","
               << fourCharCodeToString (desc.componentSubType) << ","
               << fourCharCodeToString (desc.componentManufacturer);

    return identifier;
}

static juce::String nsStringToJuce (NSString* string)
{
    return string != nil ? juce::String::fromCFString ((CFStringRef) string) : juce::String();
}

static juce::PluginDescription makePluginDescriptionFromComponent (AVAudioUnitComponent* component)
{
    const auto componentDescription = component.audioComponentDescription;

    juce::PluginDescription description;
    description.name = nsStringToJuce (component.name);
    description.descriptiveName = description.name;
    description.pluginFormatName = "Audio Unit";
    description.manufacturerName = nsStringToJuce (component.manufacturerName);
    description.version = nsStringToJuce (component.versionString);
    description.fileOrIdentifier = makeAudioUnitIdentifier (componentDescription);
    description.uniqueId = static_cast<int> (componentDescription.componentSubType);
    description.deprecatedUid = description.uniqueId;
    description.category = componentDescription.componentType == kAudioUnitType_MusicDevice ? "Synth"
                            : componentDescription.componentType == kAudioUnitType_MusicEffect
                           || componentDescription.componentType == kAudioUnitType_Effect        ? "Effect"
                            : componentDescription.componentType == kAudioUnitType_Generator      ? "Generator"
                            : componentDescription.componentType == kAudioUnitType_Panner         ? "Panner"
                            : componentDescription.componentType == kAudioUnitType_Mixer          ? "Mixer"
                            : componentDescription.componentType == kAudioUnitType_MIDIProcessor  ? "MidiEffect"
                                                                                                  : "Audio Unit";
    description.isInstrument = componentDescription.componentType == kAudioUnitType_MusicDevice;

    return description;
}

static std::vector<juce::PluginDescription> findInstalledAudioUnitDescriptions()
{
    @autoreleasepool
    {
        auto* manager = [AVAudioUnitComponentManager sharedAudioUnitComponentManager];

        const AudioComponentDescription searchDescriptions[]
        {
            { kAudioUnitType_MusicDevice, 0, 0, 0, 0 },
            { kAudioUnitType_MusicEffect, 0, 0, 0, 0 },
            { kAudioUnitType_Effect, 0, 0, 0, 0 },
            { kAudioUnitType_Generator, 0, 0, 0, 0 },
            { kAudioUnitType_Panner, 0, 0, 0, 0 },
            { kAudioUnitType_Mixer, 0, 0, 0, 0 },
            { kAudioUnitType_MIDIProcessor, 0, 0, 0, 0 }
        };

        std::vector<juce::PluginDescription> descriptions;
        std::set<juce::String> seenIdentifiers;

        for (const auto& searchDescription : searchDescriptions)
        {
            for (AVAudioUnitComponent* component in [manager componentsMatchingDescription:searchDescription])
            {
                auto description = makePluginDescriptionFromComponent (component);

                if (description.fileOrIdentifier.isNotEmpty()
                    && seenIdentifiers.insert (description.fileOrIdentifier).second)
                {
                    descriptions.push_back (std::move (description));
                }
            }
        }

        std::sort (descriptions.begin(), descriptions.end(), [] (const auto& left, const auto& right)
        {
            return left.name.toLowerCase() < right.name.toLowerCase();
        });

        return descriptions;
    }
}

static const juce::PluginDescription* findPluginDescriptionForQuery (const std::vector<juce::PluginDescription>& descriptions,
                                                                      const juce::String& query)
{
    const auto target = query.trim();

    if (target.isEmpty())
        return nullptr;

    for (const auto& description : descriptions)
    {
        if (description.name.equalsIgnoreCase (target)
            || description.descriptiveName.equalsIgnoreCase (target)
            || description.fileOrIdentifier.equalsIgnoreCase (target))
        {
            return &description;
        }
    }

    for (const auto& description : descriptions)
    {
        if (description.name.containsIgnoreCase (target)
            || description.descriptiveName.containsIgnoreCase (target)
            || description.fileOrIdentifier.containsIgnoreCase (target))
        {
            return &description;
        }
    }

    return nullptr;
}
}

void AudioUnitPluginHost::initialise (Dependencies dependencies)
{
    parentComponent = &dependencies.parentComponent;
    getPlaybackController = std::move (dependencies.getPlaybackController);
    closeAudioBrowser = std::move (dependencies.closeAudioBrowser);
    closeNowPlayingWindow = std::move (dependencies.closeNowPlayingWindow);
    setStatusText = std::move (dependencies.setStatusText);
    setChoosePluginEnabled = std::move (dependencies.setChoosePluginEnabled);
    setOpenPluginGuiEnabled = std::move (dependencies.setOpenPluginGuiEnabled);
    setOpenPluginGuiText = std::move (dependencies.setOpenPluginGuiText);
    syncPlaybackUi = std::move (dependencies.syncPlaybackUi);
    getPluginWindowBounds = std::move (dependencies.getPluginWindowBounds);

    pluginWindowAnchor.setInterceptsMouseClicks (false, false);

    if (parentComponent != nullptr)
        parentComponent->addChildComponent (pluginWindowAnchor);
}

void AudioUnitPluginHost::reset()
{
    closePluginMenu();
    destroyPluginWindow();
}

void AudioUnitPluginHost::refreshInstalledPluginDescriptions()
{
    installedPluginDescriptions = findInstalledAudioUnitDescriptions();
}

const juce::PluginDescription* AudioUnitPluginHost::findPluginDescriptionForQuery (const juce::String& query) const
{
    return ::findPluginDescriptionForQuery (installedPluginDescriptions, query);
}

void AudioUnitPluginHost::choosePlugin()
{
    if (closeAudioBrowser)
        closeAudioBrowser();

    if (closeNowPlayingWindow)
        closeNowPlayingWindow();

    if (pluginMenuHost != nullptr)
    {
        closePluginMenu();

        if (setStatusText)
            setStatusText ("plugin selection closed");

        return;
    }

    destroyPluginWindow();

        if (setOpenPluginGuiText)
            setOpenPluginGuiText ("PLUG");

    if (setStatusText)
        setStatusText ("scanning installed auv3 plugins");

    refreshInstalledPluginDescriptions();

    if (installedPluginDescriptions.empty())
    {
        if (setStatusText)
            setStatusText ("no installed auv3 plugins");

        return;
    }

    if (setStatusText)
        setStatusText ("found " + juce::String (installedPluginDescriptions.size()) + " installed auv3 plugin(s)");

    const auto pluginWindowBounds = getPluginWindowBounds != nullptr ? getPluginWindowBounds()
                                                                     : parentComponent != nullptr ? parentComponent->getLocalBounds()
                                                                                                 : juce::Rectangle<int>();

    auto menuContent = std::make_unique<PluginMenuContent> (installedPluginDescriptions,
                                                            [this] (int selectedIndex)
                                                            {
                                                                handlePluginMenuSelection (selectedIndex);
                                                            },
                                                            [this]
                                                            {
                                                                return getSelectedPluginIndex();
                                                            });

    menuContent->setSize (pluginWindowBounds.getWidth() - 2, pluginWindowBounds.getHeight() - 2);
    pluginMenuHost = std::make_unique<PluginWindowFrame> (std::move (menuContent));

    if (parentComponent != nullptr)
        parentComponent->addAndMakeVisible (*pluginMenuHost);

    pluginMenuHost->setBounds (pluginWindowBounds);
    pluginMenuHost->toFront (true);
}

void AudioUnitPluginHost::handlePluginMenuSelection (int selectedIndex)
{
    if (selectedIndex < 0 || selectedIndex >= static_cast<int> (installedPluginDescriptions.size()))
    {
        if (setStatusText)
            setStatusText ("plugin selection cancelled");

        return;
    }

    if (setStatusText)
        setStatusText ("loading plugin");

    loadPluginDescription (installedPluginDescriptions[static_cast<size_t> (selectedIndex)]);
}

void AudioUnitPluginHost::loadPluginDescription (const juce::PluginDescription& description, bool openGuiAfterLoad)
{
    auto* playbackController = getPlaybackController != nullptr ? getPlaybackController() : nullptr;

    destroyPluginWindow();

    if (playbackController != nullptr)
        playbackController->clearPluginInstance();

    if (syncPlaybackUi)
        syncPlaybackUi();

    if (setOpenPluginGuiEnabled)
        setOpenPluginGuiEnabled (false);

    const auto loadToken = ++pluginLoadToken;
    const auto currentSampleRate = playbackController != nullptr ? playbackController->getCurrentSampleRate()
                                                                 : 44100.0;
    const auto currentBlockSize = playbackController != nullptr ? playbackController->getCurrentBlockSize()
                                                                : 512;

    audioUnitFormat.createPluginInstanceAsync (description,
                                               currentSampleRate,
                                               currentBlockSize,
                                               [weakSelf = weak_from_this(), loadToken, openGuiAfterLoad, description] (std::unique_ptr<juce::AudioPluginInstance> instance,
                                                                                                                        const juce::String& errorMessage) mutable
                                               {
                                                   if (auto self = weakSelf.lock())
                                                   {
                                                       if (self->pluginLoadToken != loadToken)
                                                           return;

                                                       if (instance != nullptr)
                                                       {
                                                           auto sharedInstance = std::shared_ptr<juce::AudioPluginInstance> (instance.release());
                                                           self->currentPluginIdentifier = description.fileOrIdentifier;

                                                           if (self->pluginMenuHost != nullptr)
                                                               self->pluginMenuHost->repaint();

                                                           if (auto* controller = self->getPlaybackController != nullptr ? self->getPlaybackController() : nullptr)
                                                               controller->setPluginInstance (std::move (sharedInstance));

                                                           self->destroyPluginWindow();
                                                           self->ensurePluginWindowHost();

                                                           if (self->syncPlaybackUi)
                                                               self->syncPlaybackUi();

                                                           if (self->setStatusText)
                                                               self->setStatusText ("plugin loaded");

                                                           if (openGuiAfterLoad)
                                                           {
                                                               juce::MessageManager::callAsync ([weakSelf, loadToken]
                                                               {
                                                                   if (auto host = weakSelf.lock())
                                                                   {
                                                                       if (host->pluginLoadToken == loadToken)
                                                                           host->openPluginGui();
                                                                   }
                                                               });
                                                           }
                                                       }
                                                       else if (self->setStatusText)
                                                       {
                                                           if (errorMessage.isNotEmpty())
                                                               self->setStatusText ("plugin load failed: " + errorMessage.toLowerCase());
                                                           else
                                                               self->setStatusText ("plugin load failed");
                                                       }
                                                   }
                                               });
}

void AudioUnitPluginHost::closePluginMenu()
{
    if (pluginMenuHost != nullptr)
        pluginMenuHost->setVisible (false);

    pluginMenuHost.reset();
}

void AudioUnitPluginHost::closePluginWindow()
{
    pluginWindowVisible = false;
    pluginWindowAnchor.setVisible (false);

    if (pluginWindowHost != nullptr)
        pluginWindowHost->setVisible (false);
}

void AudioUnitPluginHost::openPluginGui()
{
    const auto closeTransientSurfaces = [this]
    {
        if (closeAudioBrowser)
            closeAudioBrowser();

        if (closeNowPlayingWindow)
            closeNowPlayingWindow();

        closePluginMenu();
    };

    const auto armPluginWindowPaintCallback = [this]
    {
        if (auto* frame = dynamic_cast<PluginWindowFrame*> (pluginWindowHost.get()))
        {
            frame->setPaintCallback ([weakSelf = weak_from_this()]
            {
                if (auto self = weakSelf.lock())
                    self->hidePluginTransitionCover();
            });
        }
    };

    if (pluginWindowHost != nullptr)
    {
        if (pluginWindowVisible)
        {
            closeTransientSurfaces();
            closePluginWindow();

            if (setOpenPluginGuiText)
                setOpenPluginGuiText ("PLUG");

            if (setStatusText)
                setStatusText ("plugin gui closed");

            return;
        }

        armPluginWindowPaintCallback();
        showPluginWindow();
        showPluginTransitionCover();
        closeTransientSurfaces();

        if (setOpenPluginGuiText)
            setOpenPluginGuiText ("PLUG");

        if (setStatusText)
            setStatusText ("plugin gui opened");

        return;
    }

    auto* playbackController = getPlaybackController != nullptr ? getPlaybackController() : nullptr;
    auto activePlugin = playbackController != nullptr ? playbackController->getPluginInstance() : std::shared_ptr<juce::AudioPluginInstance> {};

    if (! activePlugin)
    {
        if (setStatusText)
            setStatusText ("no plugin loaded");

        return;
    }

    if (! activePlugin->hasEditor())
    {
        if (setStatusText)
            setStatusText ("plugin has no gui");

        return;
    }

    if (auto* editor = activePlugin->createEditorIfNeeded())
    {
        pluginWindowHost = std::make_unique<PluginWindowFrame> (std::unique_ptr<juce::Component> (editor));

        const auto pluginWindowBounds = getPluginWindowBounds != nullptr ? getPluginWindowBounds()
                                                                         : parentComponent != nullptr ? parentComponent->getLocalBounds()
                                                                                                     : juce::Rectangle<int>();
        pluginWindowAnchor.setBounds (pluginWindowBounds);
        pluginWindowHost->setBounds (pluginWindowAnchor.getLocalBounds());

        pluginWindowAnchor.addChildComponent (*pluginWindowHost);
        pluginWindowHost->setVisible (false);
        pluginWindowAnchor.setVisible (false);

        armPluginWindowPaintCallback();
        showPluginWindow();
        showPluginTransitionCover();
        closeTransientSurfaces();

        if (setOpenPluginGuiText)
            setOpenPluginGuiText ("PLUG");

        if (setStatusText)
            setStatusText ("plugin gui opened");
    }
    else
    {
        if (setStatusText)
            setStatusText ("failed to open plugin gui");
    }
}

void AudioUnitPluginHost::resized()
{
    const auto pluginWindowBounds = getPluginWindowBounds != nullptr ? getPluginWindowBounds()
                                                                     : parentComponent != nullptr ? parentComponent->getLocalBounds()
                                                                                                 : juce::Rectangle<int>();

    pluginWindowAnchor.setBounds (pluginWindowBounds);

    if (pluginWindowHost != nullptr)
        pluginWindowHost->setBounds (pluginWindowAnchor.getLocalBounds());

    if (pluginMenuHost != nullptr)
        pluginMenuHost->setBounds (pluginWindowBounds);

    pluginWindowAnchor.setVisible (pluginWindowHost != nullptr && pluginWindowVisible);
}

void AudioUnitPluginHost::ensurePluginWindowHost()
{
    if (pluginWindowHost != nullptr)
        return;

    auto* playbackController = getPlaybackController != nullptr ? getPlaybackController() : nullptr;

    if (playbackController == nullptr)
        return;

    auto activePlugin = playbackController->getPluginInstance();

    if (! activePlugin || ! activePlugin->hasEditor())
        return;

    if (auto* editor = activePlugin->createEditorIfNeeded())
    {
        pluginWindowHost = std::make_unique<PluginWindowFrame> (std::unique_ptr<juce::Component> (editor));
        pluginWindowHost->setVisible (false);

        const auto pluginWindowBounds = getPluginWindowBounds != nullptr ? getPluginWindowBounds()
                                                                         : parentComponent != nullptr ? parentComponent->getLocalBounds()
                                                                                                     : juce::Rectangle<int>();
        pluginWindowAnchor.setBounds (pluginWindowBounds);
        pluginWindowHost->setBounds (pluginWindowAnchor.getLocalBounds());

        pluginWindowAnchor.addChildComponent (*pluginWindowHost);
    }
}

void AudioUnitPluginHost::showPluginWindow()
{
    ensurePluginWindowHost();

    if (pluginWindowHost == nullptr)
        return;

    const auto pluginWindowBounds = getPluginWindowBounds != nullptr ? getPluginWindowBounds()
                                                                     : parentComponent != nullptr ? parentComponent->getLocalBounds()
                                                                                                 : juce::Rectangle<int>();
    pluginWindowAnchor.setBounds (pluginWindowBounds);
    pluginWindowAnchor.toFront (false);
    pluginWindowHost->setBounds (pluginWindowAnchor.getLocalBounds());
    pluginWindowHost->setVisible (true);
    pluginWindowAnchor.setVisible (true);
    pluginWindowHost->toFront (true);
    pluginWindowVisible = true;
}

void AudioUnitPluginHost::destroyPluginWindow()
{
    hidePluginTransitionCover();

    if (pluginWindowHost != nullptr)
        pluginWindowHost->setVisible (false);

    pluginWindowVisible = false;
    pluginWindowAnchor.setVisible (false);
    pluginWindowHost.reset();
}

void AudioUnitPluginHost::showPluginTransitionCover()
{
    if (pluginWindowHost == nullptr)
        return;

    if (pluginTransitionCover == nullptr)
    {
        pluginTransitionCover = std::make_unique<TransitionCover>();
        pluginWindowAnchor.addChildComponent (*pluginTransitionCover);
    }

    const auto pluginWindowBounds = getPluginWindowBounds != nullptr ? getPluginWindowBounds()
                                                                     : parentComponent != nullptr ? parentComponent->getLocalBounds()
                                                                                                 : juce::Rectangle<int>();

    pluginWindowAnchor.setBounds (pluginWindowBounds);
    pluginTransitionCover->setBounds (pluginWindowAnchor.getLocalBounds());
    pluginTransitionCover->setVisible (true);
    pluginTransitionCover->toFront (true);
}

void AudioUnitPluginHost::hidePluginTransitionCover()
{
    if (pluginTransitionCover != nullptr)
        pluginTransitionCover->setVisible (false);
}

int AudioUnitPluginHost::getSelectedPluginIndex() const
{
    if (currentPluginIdentifier.isEmpty())
        return -1;

    for (size_t index = 0; index < installedPluginDescriptions.size(); ++index)
    {
        if (installedPluginDescriptions[index].fileOrIdentifier == currentPluginIdentifier)
            return static_cast<int> (index);
    }

    return -1;
}
