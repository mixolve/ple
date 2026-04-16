#import <AVFoundation/AVFoundation.h>
#import <AVFAudio/AVAudioUnitComponent.h>
#import <objc/message.h>

#if JUCE_IOS
#import <MediaPlayer/MediaPlayer.h>
#import <UIKit/UIKit.h>
#endif

#include "MainComponent.h"
#include "BinaryData.h"
#include <algorithm>
#include <functional>
#include <initializer_list>
#include <set>

#if JUCE_IOS
static NSString* juceStringToNSString (const juce::String& text)
{
    return [NSString stringWithUTF8String: text.toRawUTF8()];
}

static juce::String nsStringToJuce (NSString* string)
{
    return string != nil ? juce::String::fromCFString ((CFStringRef) string) : juce::String();
}

static UIImage* createLockScreenArtworkImage (const juce::String& title,
                                              const juce::String& artist)
{
    const CGSize size = CGSizeMake (512.0, 512.0);
    UIGraphicsImageRendererFormat* format = [UIGraphicsImageRendererFormat preferredFormat];
    format.opaque = YES;
    format.scale = 1.0f;

    UIGraphicsImageRenderer* renderer = [[UIGraphicsImageRenderer alloc] initWithSize: size format: format];

    return [renderer imageWithActions:^ (UIGraphicsImageRendererContext* context)
    {
        const CGRect bounds = CGRectMake (0.0, 0.0, size.width, size.height);
        const CGRect insetBounds = CGRectInset (bounds, 20.0, 20.0);

        [[UIColor blackColor] setFill];
        UIRectFill (bounds);

        CGContextRef cg = context.CGContext;
        CGContextSetStrokeColorWithColor (cg, [UIColor colorWithWhite: 0.3 alpha: 1.0].CGColor);
        CGContextSetLineWidth (cg, 8.0);
        CGContextStrokeRect (cg, insetBounds);

        NSString* displayTitle = [[juceStringToNSString (title) lowercaseString] length] > 0 ? [juceStringToNSString (title) lowercaseString]
                                                                                             : [NSString stringWithUTF8String: "ple"];
        NSString* displayArtist = [[juceStringToNSString (artist) lowercaseString] length] > 0 ? [juceStringToNSString (artist) lowercaseString]
                                                                                               : [NSString stringWithUTF8String: "mixolve"];

        NSDictionary* titleAttributes = @{ NSFontAttributeName: [UIFont boldSystemFontOfSize: 72.0],
                           NSForegroundColorAttributeName: [UIColor whiteColor] };
        NSDictionary* artistAttributes = @{ NSFontAttributeName: [UIFont systemFontOfSize: 28.0 weight: UIFontWeightRegular],
                            NSForegroundColorAttributeName: [UIColor colorWithWhite: 0.7 alpha: 1.0] };

        const CGSize titleSize = [displayTitle sizeWithAttributes: titleAttributes];
        const CGSize artistSize = [displayArtist sizeWithAttributes: artistAttributes];
        const CGFloat totalHeight = titleSize.height + 18.0 + artistSize.height;
        const CGFloat originY = (size.height - totalHeight) / 2.0;

        [displayTitle drawInRect: CGRectMake ((size.width - titleSize.width) / 2.0,
                                              originY,
                                              titleSize.width,
                                              titleSize.height)
                  withAttributes: titleAttributes];

        [displayArtist drawInRect: CGRectMake ((size.width - artistSize.width) / 2.0,
                                               originY + titleSize.height + 18.0,
                                               artistSize.width,
                                               artistSize.height)
                   withAttributes: artistAttributes];
    }];
}

static juce::String getMetadataValue (const juce::StringPairArray& metadataValues,
                                      std::initializer_list<const char*> candidateKeys)
{
    const auto keys = metadataValues.getAllKeys();
    const auto values = metadataValues.getAllValues();

    for (const auto* candidateKey : candidateKeys)
    {
        for (int index = 0; index < keys.size(); ++index)
        {
            if (keys[index].equalsIgnoreCase (candidateKey) && values[index].isNotEmpty())
                return values[index].trim();
        }
    }

    return {};
}

static juce::String getAssetMetadataValue (const juce::String& assetPath, NSString* commonKey)
{
    if (assetPath.isEmpty())
        return {};

    @autoreleasepool
    {
        NSURL* assetURL = [NSURL fileURLWithPath: juceStringToNSString (assetPath)];

        if (assetURL == nil)
            return {};

        AVURLAsset* asset = [AVURLAsset URLAssetWithURL: assetURL options: nil];

        for (AVMetadataItem* item in asset.commonMetadata)
        {
            if ([item.commonKey isEqualToString: commonKey] && item.stringValue != nil)
                return nsStringToJuce (item.stringValue);
        }
    }

    return {};
}

static juce::MemoryBlock getAssetArtworkData (const juce::String& assetPath)
{
    if (assetPath.isEmpty())
        return {};

    @autoreleasepool
    {
        NSURL* assetURL = [NSURL fileURLWithPath: juceStringToNSString (assetPath)];

        if (assetURL == nil)
            return {};

        AVURLAsset* asset = [AVURLAsset URLAssetWithURL: assetURL options: nil];

        auto extractArtworkData = [] (NSArray* metadataItems) -> juce::MemoryBlock
        {
            for (AVMetadataItem* item in metadataItems)
            {
                if (item == nil || item.dataValue == nil || item.dataValue.length == 0)
                    continue;

                if ([item.commonKey isEqualToString: AVMetadataCommonKeyArtwork]
                    || ([item.keySpace isEqualToString: AVMetadataKeySpaceID3]
                        && [item.key isEqualToString: AVMetadataID3MetadataKeyAttachedPicture]))
                {
                    juce::MemoryBlock artworkData;
                    artworkData.append ([item.dataValue bytes], (size_t) [item.dataValue length]);
                    return artworkData;
                }
            }

            return {};
        };

        auto artworkData = extractArtworkData (asset.commonMetadata);

        if (artworkData.getSize() > 0)
            return artworkData;

        return extractArtworkData (asset.metadata);
    }

    return {};
}

static void requestDeviceLock()
{
    auto* application = [UIApplication sharedApplication];

    if (application == nil)
        return;

    if ([application respondsToSelector: @selector (lockDevice:)])
        ((void (*)(id, SEL, void*)) objc_msgSend) (application, @selector (lockDevice:), nil);
}
#endif
namespace ple
{
class LockScreenPlaybackController final
{
public:
    using Action = std::function<void()>;
    using Query = std::function<bool()>;
    using ValueQuery = std::function<double()>;
    LockScreenPlaybackController (Action startAction,
                                  Action stopAction,
                                  Action toggleAction,
                            Action previousAction,
                            Action nextAction,
                                  Query isPlayingQuery,
                                  ValueQuery currentPositionQuery,
                                  ValueQuery durationQuery)
        : startPlayback (std::move (startAction)),
          stopPlayback (std::move (stopAction)),
          togglePlayback (std::move (toggleAction)),
            previousTrack (std::move (previousAction)),
            nextTrack (std::move (nextAction)),
          isPlaying (std::move (isPlayingQuery)),
          currentPosition (std::move (currentPositionQuery)),
          duration (std::move (durationQuery))
    {
    }

    ~LockScreenPlaybackController()
    {
        uninstall();
    }

    void ensureSessionActive()
    {
        auto* session = [AVAudioSession sharedInstance];
        NSError* error = nil;

        if (@available(iOS 13.0, *))
        {
            [session setCategory: AVAudioSessionCategoryPlayback
                            mode: AVAudioSessionModeDefault
                 routeSharingPolicy: AVAudioSessionRouteSharingPolicyLongFormAudio
                             options: 0
                               error: &error];
        }
        else
        {
            [session setCategory: AVAudioSessionCategoryPlayback error: &error];
        }

        if (error != nil)
        {
            juce::Logger::writeToLog (juce::String ("AVAudioSession category error: ") + juce::String::fromUTF8 ([[error localizedDescription] UTF8String]));
            error = nil;
        }

        [session setActive: YES error: &error];

        if (error != nil)
        {
            juce::Logger::writeToLog (juce::String ("AVAudioSession activation error: ") + juce::String::fromUTF8 ([[error localizedDescription] UTF8String]));
            error = nil;
        }
    }

    MPRemoteCommandCenter* getCommandCenter()
    {
        return [MPRemoteCommandCenter sharedCommandCenter];
    }

    MPNowPlayingInfoCenter* getInfoCenter()
    {
        return [MPNowPlayingInfoCenter defaultCenter];
    }

    void install()
    {
        if (installed)
            return;

        [[UIApplication sharedApplication] beginReceivingRemoteControlEvents];

        ensureSessionActive();

        auto* commandCenter = getCommandCenter();

        playToken = [commandCenter.playCommand addTargetWithHandler:^MPRemoteCommandHandlerStatus (MPRemoteCommandEvent*)
        {
            juce::MessageManager::callAsync ([action = startPlayback]
            {
                action();
            });

            return MPRemoteCommandHandlerStatusSuccess;
        }];

        pauseToken = [commandCenter.pauseCommand addTargetWithHandler:^MPRemoteCommandHandlerStatus (MPRemoteCommandEvent*)
        {
            juce::MessageManager::callAsync ([action = stopPlayback]
            {
                action();
            });

            return MPRemoteCommandHandlerStatusSuccess;
        }];

        toggleToken = [commandCenter.togglePlayPauseCommand addTargetWithHandler:^MPRemoteCommandHandlerStatus (MPRemoteCommandEvent*)
        {
            juce::MessageManager::callAsync ([action = togglePlayback]
            {
                action();
            });

            return MPRemoteCommandHandlerStatusSuccess;
        }];

        previousToken = [commandCenter.previousTrackCommand addTargetWithHandler:^MPRemoteCommandHandlerStatus (MPRemoteCommandEvent*)
        {
            juce::MessageManager::callAsync ([action = previousTrack]
            {
                action();
            });

            return MPRemoteCommandHandlerStatusSuccess;
        }];

        nextToken = [commandCenter.nextTrackCommand addTargetWithHandler:^MPRemoteCommandHandlerStatus (MPRemoteCommandEvent*)
        {
            juce::MessageManager::callAsync ([action = nextTrack]
            {
                action();
            });

            return MPRemoteCommandHandlerStatusSuccess;
        }];

        commandCenter.playCommand.enabled = YES;
        commandCenter.pauseCommand.enabled = YES;
        commandCenter.togglePlayPauseCommand.enabled = YES;
        commandCenter.previousTrackCommand.enabled = YES;
        commandCenter.nextTrackCommand.enabled = YES;

        installed = true;
        refreshNowPlayingInfo();
    }

    void uninstall()
    {
        if (! installed && playToken == nil && pauseToken == nil && toggleToken == nil)
            return;

        auto* commandCenter = getCommandCenter();

        if (playToken != nil)
            [commandCenter.playCommand removeTarget: playToken];

        if (pauseToken != nil)
            [commandCenter.pauseCommand removeTarget: pauseToken];

        if (toggleToken != nil)
            [commandCenter.togglePlayPauseCommand removeTarget: toggleToken];

        if (previousToken != nil)
            [commandCenter.previousTrackCommand removeTarget: previousToken];

        if (nextToken != nil)
            [commandCenter.nextTrackCommand removeTarget: nextToken];

        playToken = nil;
        pauseToken = nil;
        toggleToken = nil;
        previousToken = nil;
        nextToken = nil;
        installed = false;

        [[UIApplication sharedApplication] endReceivingRemoteControlEvents];

        getInfoCenter().nowPlayingInfo = nil;
    }

    void refreshNowPlayingInfo()
    {
        if (! installed)
            return;

        const auto trackDuration = juce::jmax (0.0, duration());
        const auto hasKnownDuration = trackDuration > 0.0;

        auto* info = [NSMutableDictionary dictionary];
        info[MPMediaItemPropertyTitle] = juceStringToNSString (trackTitle);
        if (artistName.isNotEmpty())
        {
            info[MPMediaItemPropertyArtist] = juceStringToNSString (artistName);
            info[MPMediaItemPropertyAlbumArtist] = juceStringToNSString (artistName);
        }
        info[MPMediaItemPropertyAlbumTitle] = juceStringToNSString (trackTitle);
        info[MPMediaItemPropertyMediaType] = @(MPMediaTypeMusic);

        UIImage* artworkImage = nil;

        if (trackArtworkData.getSize() > 0)
        {
            NSData* artworkData = [NSData dataWithBytes: trackArtworkData.getData()
                                                 length: trackArtworkData.getSize()];
            artworkImage = [UIImage imageWithData: artworkData];
        }

        if (artworkImage == nil)
            artworkImage = createLockScreenArtworkImage (trackTitle, artistName);

        if (artworkImage != nil)
            info[MPMediaItemPropertyArtwork] = [[MPMediaItemArtwork alloc] initWithImage: artworkImage];

        if (trackAssetPath.isNotEmpty())
            info[MPNowPlayingInfoPropertyAssetURL] = [NSURL fileURLWithPath: juceStringToNSString (trackAssetPath)];
        info[MPNowPlayingInfoPropertyPlaybackRate] = @(isPlaying() ? 1.0 : 0.0);
        info[MPNowPlayingInfoPropertyDefaultPlaybackRate] = @1.0;
        info[MPNowPlayingInfoPropertyElapsedPlaybackTime] = @(juce::jmax (0.0, currentPosition()));
        if (hasKnownDuration)
        {
            info[MPMediaItemPropertyPlaybackDuration] = @(trackDuration);
            info[MPNowPlayingInfoPropertyPlaybackProgress] = @(juce::jlimit (0.0, 1.0, currentPosition() / trackDuration));
            info[MPNowPlayingInfoPropertyIsLiveStream] = @NO;
        }
        else
        {
            info[MPNowPlayingInfoPropertyIsLiveStream] = @YES;
        }

        auto* commandCenter = getCommandCenter();
        commandCenter.playCommand.enabled = ! isPlaying();
        commandCenter.pauseCommand.enabled = isPlaying();
        commandCenter.togglePlayPauseCommand.enabled = YES;

        getInfoCenter().nowPlayingInfo = info;
    }

    void setTrackTitle (juce::String newTitle)
    {
        trackTitle = std::move (newTitle);
        refreshNowPlayingInfo();
    }

    void setTrackMetadata (juce::String newTitle, juce::String newArtist, juce::String newPath)
    {
        trackTitle = std::move (newTitle);
        artistName = std::move (newArtist);
        trackAssetPath = std::move (newPath);
        trackArtworkData = getAssetArtworkData (trackAssetPath);
        refreshNowPlayingInfo();
    }

    void setTrackArtistName (juce::String newArtist)
    {
        artistName = std::move (newArtist);
        refreshNowPlayingInfo();
    }

    void setTrackAssetPath (juce::String newPath)
    {
        trackAssetPath = std::move (newPath);
        trackArtworkData = getAssetArtworkData (trackAssetPath);
        refreshNowPlayingInfo();
    }

    void refresh()
    {
        refreshNowPlayingInfo();
    }

private:
    Action startPlayback;
    Action stopPlayback;
    Action togglePlayback;
    Action previousTrack;
    Action nextTrack;
    Query isPlaying;
    ValueQuery currentPosition;
    ValueQuery duration;
    juce::String trackTitle { "NO AUDIO" };
    juce::String artistName;
    juce::String trackAssetPath;
    juce::MemoryBlock trackArtworkData;
    id playToken = nil;
    id pauseToken = nil;
    id toggleToken = nil;
    id previousToken = nil;
    id nextToken = nil;
    bool installed = false;
};
}
namespace
{
const auto uiBlack = juce::Colour(0xff000000);
const auto uiGrey800 = juce::Colour(0xff242424);
const auto uiGrey700 = juce::Colour(0xff363636);
const auto uiGrey500 = juce::Colour(0xff707070);
const auto uiWhite = juce::Colour(0xffffffff);
const auto uiAccentBlue = juce::Colour(0xff9999ff);
const auto uiAccentPeach = juce::Colour(0xffffcc99);
constexpr int uiHorizontalInset = 4;
constexpr int uiVerticalInset = 40;
constexpr int uiButtonHeight = 22;
constexpr int uiSectionGap = 6;
constexpr int uiPluginWindowGap = 8;
constexpr int uiFooterHeight = 20;
constexpr int uiButtonGap = 2;
constexpr float uiFontSize = 20.0f;

juce::Font makeUiFont(const bool bold = false, const float height = uiFontSize)
{
#if JUCE_TARGET_HAS_BINARY_DATA
    static const auto regularTypeface = juce::Typeface::createSystemTypefaceFor(BinaryData::SometypeMonoRegular_ttf,
                                                                                BinaryData::SometypeMonoRegular_ttfSize);
    static const auto boldTypeface = juce::Typeface::createSystemTypefaceFor(BinaryData::SometypeMonoBold_ttf,
                                                                             BinaryData::SometypeMonoBold_ttfSize);

    const auto typeface = bold ? boldTypeface : regularTypeface;

    if (typeface != nullptr)
        return juce::Font(juce::FontOptions(typeface).withHeight(height));
#endif

    return juce::Font(juce::FontOptions("Sometype Mono", height, bold ? juce::Font::bold : juce::Font::plain));
}

juce::Colour getButtonAccentColour(const juce::Button& button)
{
    const auto accent = button.getProperties().getWithDefault("accent", "grey").toString();

    if (accent == "blue")
        return uiAccentBlue;

    if (accent == "peach")
        return uiAccentPeach;

    if (accent == "white")
        return uiWhite;

    return uiGrey500;
}

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

static juce::File getAudioRootDirectory()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);
}

static juce::File getFallbackNoiseFile()
{
    return juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("ple-white-noise.wav");
}

static bool createFallbackNoiseFileIfNeeded (const juce::File& file)
{
    if (file.existsAsFile())
        return true;

    if (auto parent = file.getParentDirectory(); parent.exists() || parent.createDirectory())
    {
        std::unique_ptr<juce::FileOutputStream> outputStream (file.createOutputStream());

        if (outputStream == nullptr || ! outputStream->openedOk())
            return false;

        juce::WavAudioFormat wavFormat;
        constexpr double sampleRate = 44100.0;
        constexpr int channels = 1;
        constexpr int bitsPerSample = 16;
        constexpr int noiseDurationSeconds = 30;

        std::unique_ptr<juce::AudioFormatWriter> writer (wavFormat.createWriterFor (outputStream.get(),
                                                                                    sampleRate,
                                                                                    channels,
                                                                                    bitsPerSample,
                                                                                    {},
                                                                                    0));

        if (writer == nullptr)
            return false;

        outputStream.release();

        juce::AudioBuffer<float> buffer (channels, 512);
        juce::Random random;
        const auto totalSamples = static_cast<int> (sampleRate) * noiseDurationSeconds;

        for (int sampleOffset = 0; sampleOffset < totalSamples; sampleOffset += buffer.getNumSamples())
        {
            const auto blockSize = juce::jmin (buffer.getNumSamples(), totalSamples - sampleOffset);
            buffer.setSize (channels, blockSize, false, false, true);

            auto* channelData = buffer.getWritePointer (0);

            for (int sample = 0; sample < blockSize; ++sample)
                channelData[sample] = random.nextFloat() * 0.2f - 0.1f;

            if (! writer->writeFromAudioSampleBuffer (buffer, 0, blockSize))
                return false;
        }

        return true;
    }

    return false;
}

static bool isPlayableAudioFile (const juce::File& file)
{
    if (! file.existsAsFile())
        return false;

    const auto extension = file.getFileExtension().toLowerCase();

    return extension == ".wav"
        || extension == ".wave"
        || extension == ".aif"
        || extension == ".aiff"
        || extension == ".mp3"
        || extension == ".m4a"
        || extension == ".aac"
        || extension == ".caf"
        || extension == ".flac"
        || extension == ".ogg";
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

static const juce::PluginDescription* findPluginDescriptionForQuery(const std::vector<juce::PluginDescription>& descriptions,
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

class PluginWindowFrame final : public juce::Component
{
public:
    explicit PluginWindowFrame (std::unique_ptr<juce::Component> contentToOwn)
        : content (std::move (contentToOwn))
    {
        jassert (content != nullptr);
        addAndMakeVisible (*content);
    }

    void paint (juce::Graphics& g) override
    {
        g.setColour (uiGrey800);
        g.fillAll();

        g.setColour (uiGrey500);
        g.drawRect (getLocalBounds(), 1);
    }

    void resized() override
    {
        if (content != nullptr)
            content->setBounds (getLocalBounds().reduced (1));
    }

private:
    std::unique_ptr<juce::Component> content;
};

class PluginMenuContent final : public juce::Component
{
public:
    using SelectionCallback = std::function<void(int)>;

    PluginMenuContent (std::vector<juce::PluginDescription> items, SelectionCallback selectionCallback)
        : descriptions (std::move (items)), onSelect (std::move (selectionCallback))
    {
        addAndMakeVisible (viewport);
        viewport.setViewedComponent (&surface, false);
        viewport.setScrollBarsShown (true, true, false, true);
        viewport.setScrollBarThickness (4);

        surface.setDescriptions (descriptions, onSelect);
    }

    void resized() override
    {
        viewport.setBounds (getLocalBounds());
        surface.setSize (getWidth(), surface.getContentHeight());
    }

private:
    class Surface final : public juce::Component
    {
    public:
        void setDescriptions (const std::vector<juce::PluginDescription>& items, SelectionCallback callback)
        {
            descriptions = items;
            onSelect = std::move (callback);
            hoveredIndex = -1;
            repaint();
        }

        int getContentHeight() const
        {
            return juce::jmax (uiButtonHeight, static_cast<int> (descriptions.size()) * uiButtonHeight);
        }

        void paint (juce::Graphics& g) override
        {
            g.setColour (uiGrey800);
            g.fillAll();

            for (size_t index = 0; index < descriptions.size(); ++index)
            {
                const auto rowBounds = juce::Rectangle<int> (0,
                                                             static_cast<int> (index) * uiButtonHeight,
                                                             getWidth(),
                                                             uiButtonHeight).reduced (1, 0);

                const auto isHighlighted = static_cast<int> (index) == hoveredIndex;

                g.setColour (isHighlighted ? uiGrey700 : uiGrey800);
                g.fillRect (rowBounds);
                g.setColour (isHighlighted ? uiAccentBlue : uiGrey500);
                g.drawRect (rowBounds, 1);

                g.setColour (uiWhite);
                g.setFont (makeUiFont());
                g.drawText (descriptions[index].name.toUpperCase(), rowBounds.reduced (10, 0), juce::Justification::centredLeft, true);
            }

            g.setColour (uiGrey500);
            g.drawRect (getLocalBounds(), 1);
        }

        void mouseMove (const juce::MouseEvent& event) override
        {
            updateHoveredRow (event.position.y);
        }

        void mouseExit (const juce::MouseEvent&) override
        {
            updateHoveredRow (-1.0f);
        }

        void mouseUp (const juce::MouseEvent& event) override
        {
            const auto selectedIndex = juce::jlimit (0,
                                                     static_cast<int> (descriptions.size()) - 1,
                                                     static_cast<int> (event.position.y) / uiButtonHeight);

            if (selectedIndex >= 0 && selectedIndex < static_cast<int> (descriptions.size()) && onSelect)
                onSelect (selectedIndex);
        }

    private:
        void updateHoveredRow (float y)
        {
            const auto nextHoveredIndex = y < 0.0f ? -1 : static_cast<int> (y) / uiButtonHeight;

            if (hoveredIndex != nextHoveredIndex)
            {
                hoveredIndex = nextHoveredIndex;
                repaint();
            }
        }

        std::vector<juce::PluginDescription> descriptions;
        SelectionCallback onSelect;
        int hoveredIndex = -1;
    };

    std::vector<juce::PluginDescription> descriptions;
    SelectionCallback onSelect;
    juce::Viewport viewport;
    Surface surface;
};

class FileBrowserContent final : public juce::Component
{
public:
    using SelectionCallback = std::function<void(int)>;

    struct Row
    {
        juce::String label;
        bool isSelected = false;
        bool isPathActive = false;
    };

    FileBrowserContent (std::vector<Row> items, SelectionCallback selectionCallback)
        : rows (std::move (items)), onSelect (std::move (selectionCallback))
    {
        addAndMakeVisible (viewport);
        viewport.setViewedComponent (&surface, false);
        viewport.setScrollBarsShown (true, true, false, true);
        viewport.setScrollBarThickness (4);

        surface.setRows (rows, onSelect);
    }

    void resized() override
    {
        viewport.setBounds (getLocalBounds());
        surface.setSize (getWidth(), surface.getContentHeight());
    }

private:
    class Surface final : public juce::Component
    {
    public:
        void setRows (const std::vector<Row>& items, SelectionCallback callback)
        {
            rows = items;
            onSelect = std::move (callback);
            hoveredIndex = -1;
            repaint();
        }

        int getContentHeight() const
        {
            return juce::jmax (uiButtonHeight, static_cast<int> (rows.size()) * uiButtonHeight);
        }

        void paint (juce::Graphics& g) override
        {
            g.setColour (uiGrey800);
            g.fillAll();

            for (size_t index = 0; index < rows.size(); ++index)
            {
                const auto rowBounds = juce::Rectangle<int> (0,
                                                             static_cast<int> (index) * uiButtonHeight,
                                                             getWidth(),
                                                             uiButtonHeight).reduced (1, 0);

                const auto isHighlighted = static_cast<int> (index) == hoveredIndex;
                const auto isSelected = rows[index].isSelected;
                const auto isPathActive = rows[index].isPathActive;
                const auto hasAccent = isSelected || isPathActive;

                g.setColour (hasAccent || isHighlighted ? uiGrey700 : uiGrey800);
                g.fillRect (rowBounds);
                g.setColour (hasAccent ? uiAccentPeach : isHighlighted ? uiAccentBlue : uiGrey500);
                g.drawRect (rowBounds, 1);

                g.setColour (uiWhite);
                g.setFont (makeUiFont());
                g.drawText (rows[index].label.toUpperCase(), rowBounds.reduced (10, 0), juce::Justification::centredLeft, true);
            }

            g.setColour (uiGrey500);
            g.drawRect (getLocalBounds(), 1);
        }

        void mouseMove (const juce::MouseEvent& event) override
        {
            updateHoveredRow (event.position.y);
        }

        void mouseExit (const juce::MouseEvent&) override
        {
            updateHoveredRow (-1.0f);
        }

        void mouseUp (const juce::MouseEvent& event) override
        {
            const auto selectedIndex = juce::jlimit (0,
                                                     static_cast<int> (rows.size()) - 1,
                                                     static_cast<int> (event.position.y) / uiButtonHeight);

            if (selectedIndex >= 0 && selectedIndex < static_cast<int> (rows.size()) && onSelect)
                onSelect (selectedIndex);
        }

    private:
        void updateHoveredRow (float y)
        {
            const auto nextHoveredIndex = y < 0.0f ? -1 : static_cast<int> (y) / uiButtonHeight;

            if (hoveredIndex != nextHoveredIndex)
            {
                hoveredIndex = nextHoveredIndex;
                repaint();
            }
        }

        std::vector<Row> rows;
        SelectionCallback onSelect;
        int hoveredIndex = -1;
    };

    std::vector<Row> rows;
    SelectionCallback onSelect;
    juce::Viewport viewport;
    Surface surface;
};

class ProjectDocumentWindowButton final : public juce::Button
{
public:
    ProjectDocumentWindowButton (juce::String buttonName, juce::Colour buttonColour, juce::Path normalShape, juce::Path toggledShape)
        : juce::Button (std::move (buttonName)), colour (buttonColour), normal (std::move (normalShape)), toggled (std::move (toggledShape))
    {
        setWantsKeyboardFocus (false);
    }

    void paintButton (juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
    {
        auto background = uiGrey800;

        if (shouldDrawButtonAsDown)
            background = uiGrey700;
        else if (shouldDrawButtonAsHighlighted)
            background = uiGrey700;

        g.setColour (background);
        g.fillAll();

        g.setColour ((! isEnabled() || shouldDrawButtonAsDown) ? colour.withAlpha (0.65f)
                                                               : colour);

        if (shouldDrawButtonAsHighlighted)
        {
            g.fillAll();
            g.setColour (uiGrey500);
        }

        const auto& shape = getToggleState() ? toggled : normal;
        const auto reducedRect = juce::Justification (juce::Justification::centred)
                                     .appliedToRectangle (juce::Rectangle<int> (getHeight(), getHeight()), getLocalBounds())
                                     .toFloat()
                                     .reduced ((float) getHeight() * 0.3f);

        g.fillPath (shape, shape.getTransformToScaleToFit (reducedRect, true));
    }

private:
    juce::Colour colour;
    juce::Path normal;
    juce::Path toggled;
};

class PleLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    juce::Font getTextButtonFont(juce::TextButton&, int) override
    {
        return makeUiFont();
    }

    juce::Font getPopupMenuFont() override
    {
        return makeUiFont();
    }

    juce::Font getLabelFont(juce::Label&) override
    {
        return makeUiFont();
    }

    void drawButtonBackground(juce::Graphics& g,
                              juce::Button& button,
                              const juce::Colour&,
                              bool isMouseOverButton,
                              bool isButtonDown) override
    {
        const auto bounds = button.getLocalBounds();
        const auto accentColour = getButtonAccentColour(button);
        const auto isActive = button.getToggleState();

        g.setColour(isButtonDown ? uiGrey700 : uiGrey800);
        g.fillRect(bounds);

        g.setColour(button.isEnabled() ? (isActive ? accentColour : uiGrey500) : uiGrey500);
        g.drawRect(bounds, 1);

        if ((isActive || (isMouseOverButton && button.isEnabled() && ! isButtonDown)) && button.isEnabled())
        {
            g.setColour(accentColour);
            g.drawRect(bounds.reduced(1), 1);
        }
    }

    void drawButtonText(juce::Graphics& g,
                        juce::TextButton& button,
                        bool,
                        bool) override
    {
        g.setColour(button.isEnabled() ? uiWhite : uiGrey500);
        g.setFont(makeUiFont());
        g.drawFittedText(button.getButtonText(),
                         button.getLocalBounds().reduced(6, 0),
                         juce::Justification::centred,
                         1,
                         1.0f);
    }

    void drawLabel(juce::Graphics& g, juce::Label& label) override
    {
        const auto bounds = label.getLocalBounds();
        const auto textColour = label.isColourSpecified (juce::Label::textColourId)
                                    ? label.findColour (juce::Label::textColourId)
                                    : (label.isEnabled() ? uiWhite : uiGrey500);

        g.setColour (textColour);
        g.setFont(makeUiFont());
        g.drawFittedText(label.getText(),
                         bounds.reduced(4, 0),
                         label.getJustificationType(),
                         1,
                         1.0f);
    }

    void drawPopupMenuBackgroundWithOptions (juce::Graphics& g,
                                             int width,
                                             int height,
                                             const juce::PopupMenu::Options&) override
    {
        g.setColour (uiGrey800);
        g.fillAll();

        g.setColour (uiGrey500);
        g.drawRect (0, 0, width, height, 1);
    }

    void drawPopupMenuSectionHeaderWithOptions (juce::Graphics& g,
                                                 const juce::Rectangle<int>& area,
                                                 const juce::String& sectionName,
                                                 const juce::PopupMenu::Options&) override
    {
        g.setFont (makeUiFont (true));
        g.setColour (uiWhite);
        g.drawText (sectionName.toUpperCase(), area.reduced (8, 0), juce::Justification::centredLeft, true);
    }

    void drawPopupMenuItemWithOptions (juce::Graphics& g,
                                       const juce::Rectangle<int>& area,
                                       bool isHighlighted,
                                       const juce::PopupMenu::Item& item,
                                       const juce::PopupMenu::Options&) override
    {
        if (item.isSeparator)
        {
            auto separatorArea = area.reduced (6, 0);
            separatorArea.removeFromTop (separatorArea.getHeight() / 2);

            g.setColour (uiGrey500);
            g.fillRect (separatorArea.removeFromTop (1));
            return;
        }

        auto itemArea = area.reduced (1);

        if (isHighlighted)
        {
            g.setColour (uiGrey700);
            g.fillRect (itemArea);
            g.setColour (uiAccentBlue);
            g.drawRect (itemArea, 1);
        }
        else
        {
            g.setColour (uiGrey800);
            g.fillRect (itemArea);
            g.setColour (uiGrey500);
            g.drawRect (itemArea, 1);
        }

        g.setColour (item.isEnabled ? uiWhite : uiGrey500);
        g.setFont (makeUiFont());
        g.drawText (item.text.toUpperCase(), itemArea.reduced (10, 0), juce::Justification::centredLeft, true);

        if (item.shortcutKeyDescription.isNotEmpty())
        {
            g.setColour (item.isEnabled ? uiGrey500 : uiGrey700);
            g.drawText (item.shortcutKeyDescription.toUpperCase(), itemArea.reduced (10, 0), juce::Justification::centredRight, true);
        }
    }

    int getPopupMenuBorderSize() override
    {
        return 1;
    }

    void drawDocumentWindowTitleBar(juce::DocumentWindow& window,
                                    juce::Graphics& g,
                                    int w,
                                    int h,
                                    int titleSpaceX,
                                    int titleSpaceW,
                                    const juce::Image* icon,
                                    bool drawTitleTextOnLeft) override
    {
        if (w <= 0 || h <= 0)
            return;

        g.setColour (uiGrey800);
        g.fillAll();

        g.setColour (uiGrey500);
        g.drawRect (0, 0, w, h, 1);

        auto font = makeUiFont (false, juce::jmax (14.0f, static_cast<float> (h) * 0.62f));
        g.setFont (font);

        auto textW = juce::GlyphArrangement::getStringWidthInt (font, window.getName());
        auto iconW = 0;
        auto iconH = 0;

        if (icon != nullptr)
        {
            iconH = static_cast<int> (font.getHeight());
            iconW = icon->getWidth() * iconH / icon->getHeight() + 4;
        }

        textW = juce::jmin (titleSpaceW, textW + iconW);
        auto textX = drawTitleTextOnLeft ? titleSpaceX
                                         : juce::jmax (titleSpaceX, (w - textW) / 2);

        if (textX + textW > titleSpaceX + titleSpaceW)
            textX = titleSpaceX + titleSpaceW - textW;

        if (icon != nullptr)
        {
            g.setOpacity (window.isActiveWindow() ? 1.0f : 0.6f);
            g.drawImageWithin (*icon, textX, (h - iconH) / 2, iconW, iconH,
                               juce::RectanglePlacement::centred, false);
            textX += iconW;
            textW -= iconW;
        }

        if (window.isColourSpecified (juce::DocumentWindow::textColourId) || isColourSpecified (juce::DocumentWindow::textColourId))
            g.setColour (window.findColour (juce::DocumentWindow::textColourId));
        else
            g.setColour (uiWhite);

        g.drawText (window.getName(), textX, 0, textW, h, juce::Justification::centredLeft, true);
    }

    juce::Button* createDocumentWindowButton (int buttonType) override
    {
        juce::Path shape;
        auto crossThickness = 0.15f;

        if (buttonType == juce::DocumentWindow::closeButton)
        {
            shape.addLineSegment ({ 0.0f, 0.0f, 1.0f, 1.0f }, crossThickness);
            shape.addLineSegment ({ 1.0f, 0.0f, 0.0f, 1.0f }, crossThickness);

            return new ProjectDocumentWindowButton ("close", uiWhite, shape, shape);
        }

        if (buttonType == juce::DocumentWindow::minimiseButton)
        {
            shape.addLineSegment ({ 0.0f, 0.5f, 1.0f, 0.5f }, crossThickness);
            return new ProjectDocumentWindowButton ("minimise", uiGrey500, shape, shape);
        }

        if (buttonType == juce::DocumentWindow::maximiseButton)
        {
            shape.addLineSegment ({ 0.5f, 0.0f, 0.5f, 1.0f }, crossThickness);
            shape.addLineSegment ({ 0.0f, 0.5f, 1.0f, 0.5f }, crossThickness);
            return new ProjectDocumentWindowButton ("maximise", uiAccentBlue, shape, shape);
        }

        jassertfalse;
        return nullptr;
    }
};
}

MainComponent::MainComponent()
{
    formatManager.registerBasicFormats();

    refreshAudioLibrary();

    lookAndFeel = std::make_unique<PleLookAndFeel>();
    setLookAndFeel(lookAndFeel.get());
    setOpaque(true);

    previousButton.setButtonText("PREV");
    previousButton.getProperties().set ("accent", "white");
    previousButton.onClick = [this] { playPreviousTrack(); };
    addAndMakeVisible(previousButton);

    playButton.setButtonText("PLAY");
    playButton.getProperties().set("accent", "blue");
    playButton.onClick = [this] { togglePlay(); };
    playButton.setToggleState(false, juce::dontSendNotification);
    addAndMakeVisible(playButton);

    nextButton.setButtonText("NEXT");
    nextButton.getProperties().set ("accent", "white");
    nextButton.onClick = [this] { playNextTrack(); };
    addAndMakeVisible(nextButton);

    playbackModeButton.setButtonText (getPlaybackModeLabel());
    playbackModeButton.getProperties().set ("accent", "peach");
    playbackModeButton.onClick = [this] { cyclePlaybackMode(); };
    addAndMakeVisible (playbackModeButton);

    choosePluginButton.setButtonText("CHOOSE");
    choosePluginButton.onClick = [this] { choosePlugin(); };
    addAndMakeVisible(choosePluginButton);

    openPluginGuiButton.setButtonText("OPEN");
    openPluginGuiButton.getProperties().set("accent", "white");
    openPluginGuiButton.onClick = [this] { openPluginGui(); };
    openPluginGuiButton.setEnabled(false);
    addAndMakeVisible(openPluginGuiButton);

    browseButton.setButtonText("BROWSE");
    browseButton.getProperties().set ("accent", "peach");
    browseButton.onClick = [this] { browseAudioFiles(); };
    addAndMakeVisible (browseButton);

    pluginWindowAnchor.setInterceptsMouseClicks(false, false);
    addChildComponent(pluginWindowAnchor);

    statusLabel.setText("ready", juce::dontSendNotification);
    statusLabel.setJustificationType(juce::Justification::centredLeft);
    statusLabel.setInterceptsMouseClicks(false, false);
    statusLabel.setVisible(false);

    footerLabel.setText("PLE by MIXOLVE", juce::dontSendNotification);
    footerLabel.setJustificationType(juce::Justification::centred);
    footerLabel.setInterceptsMouseClicks(false, false);
    addAndMakeVisible(footerLabel);

    for (auto* button : { &playbackModeButton, &previousButton, &playButton, &nextButton, &choosePluginButton, &openPluginGuiButton, &browseButton })
    {
        button->setWantsKeyboardFocus(false);
        button->setMouseClickGrabsKeyboardFocus(false);
    }

    setSize(420, 248);
    setAudioChannels(0, 2);
    startTimerHz (4);
    updatePlaybackModeButton();
    updatePlayButtonLabel();

    installedPluginDescriptions = findInstalledAudioUnitDescriptions();

    const auto diagnosticsFile = getAudioRootDirectory().getChildFile ("installed-auv3.txt");
    if (diagnosticsFile.existsAsFile())
        diagnosticsFile.deleteFile();

    automationPluginQuery = juce::SystemStats::getEnvironmentVariable("PLE_AUTOMATION_PLUGIN", {}).trim();
    automationOpenGuiAfterLoad = juce::SystemStats::getEnvironmentVariable("PLE_AUTOMATION_OPEN_GUI", "0").trim() != "0";
    automationShowPluginMenu = juce::SystemStats::getEnvironmentVariable ("PLE_AUTOMATION_SHOW_MENU", "0").trim() != "0";
    automationPlayOnLaunch = juce::SystemStats::getEnvironmentVariable ("PLE_AUTOMATION_PLAY", "0").trim() != "0";
    automationLockScreenOnLaunch = juce::SystemStats::getEnvironmentVariable ("PLE_AUTOMATION_LOCK_SCREEN", "0").trim() != "0";

    if (const auto* automationPlugin = findPluginDescriptionForQuery (installedPluginDescriptions, automationPluginQuery))
        loadPluginDescription (*automationPlugin, automationOpenGuiAfterLoad);

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

    if (automationLockScreenOnLaunch)
    {
        maybeScheduleAutomationLockScreen();
    }

}

MainComponent::~MainComponent()
{
    stopTimer();
    lockScreenPlaybackController.reset();
    setLookAndFeel(nullptr);
    shutdownAudio();
}

void MainComponent::timerCallback()
{
#if JUCE_IOS
    if (playbackFinishedHandled || currentTrackDurationSeconds <= 0.0)
        return;

    if (transportSource.isPlaying())
        return;

    const auto currentPosition = transportSource.getCurrentPosition();

    if (currentPosition + 0.05 < currentTrackDurationSeconds)
        return;

    handlePlaybackFinished();
#endif
}

void MainComponent::prepareToPlay(int samplesPerBlockExpected, double sampleRate)
{
    currentSampleRate = sampleRate;
    currentBlockSize = samplesPerBlockExpected;
    transportSource.prepareToPlay(samplesPerBlockExpected, sampleRate);
}

void MainComponent::getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill)
{
    juce::ScopedTryLock tryLock (audioSourceLock);

    if (tryLock.isLocked() && readerSource)
    {
        transportSource.getNextAudioBlock(bufferToFill);
    }
    else
    {
        bufferToFill.clearActiveBufferRegion();
    }

    auto activePlugin = std::atomic_load (&pluginInstance);

    if (activePlugin != nullptr && bufferToFill.buffer != nullptr && bufferToFill.numSamples > 0)
    {
        juce::AudioBuffer<float> pluginBufferView;
        pluginBufferView.setDataToReferTo (bufferToFill.buffer->getArrayOfWritePointers(),
                                           bufferToFill.buffer->getNumChannels(),
                                           bufferToFill.startSample,
                                           bufferToFill.numSamples);

        juce::MidiBuffer midiBuffer;
        activePlugin->processBlock (pluginBufferView, midiBuffer);
    }
}

void MainComponent::releaseResources()
{
    const juce::ScopedLock lock (audioSourceLock);
    transportSource.releaseResources();
    readerSource.reset();
}

void MainComponent::refreshAudioLibrary()
{
    availableAudioFiles.clear();

    juce::Array<juce::File> discoveredFiles;
    getAudioRootDirectory().findChildFiles (discoveredFiles, juce::File::findFiles, true);

    for (const auto& file : discoveredFiles)
    {
        if (isPlayableAudioFile (file))
            availableAudioFiles.push_back (file);
    }

    std::sort (availableAudioFiles.begin(), availableAudioFiles.end(), [] (const auto& left, const auto& right)
    {
        return left.getFileName().toLowerCase() < right.getFileName().toLowerCase();
    });

    if (availableAudioFiles.empty())
    {
        currentAudioFileIndex = -1;
        currentAudioFileName.clear();
        currentTrackArtistName.clear();
        return;
    }

    if (currentAudioFileIndex < 0 || currentAudioFileIndex >= static_cast<int> (availableAudioFiles.size()))
    {
        currentAudioFileIndex = 0;
            currentAudioFileName = availableAudioFiles.front().getFullPathName();
        return;
    }

    if (currentAudioFileName.isNotEmpty())
    {
        for (size_t index = 0; index < availableAudioFiles.size(); ++index)
        {
                if (availableAudioFiles[index].getFullPathName().equalsIgnoreCase (currentAudioFileName))
            {
                currentAudioFileIndex = static_cast<int> (index);
                break;
            }
        }
    }
}

bool MainComponent::loadAudioFileAtIndex (int index)
{
    configureLockScreenControls();

    if (availableAudioFiles.empty())
    {
        juce::Logger::writeToLog ("No user audio files found; using fallback white noise track.");

        const auto fallbackNoiseFile = getFallbackNoiseFile();

        if (! createFallbackNoiseFileIfNeeded (fallbackNoiseFile))
        {
            transportSource.stop();
            readerSource.reset();
            currentAudioFileIndex = -1;
            currentAudioFileName.clear();
            currentTrackDurationSeconds = 0.0;
            currentTrackTitle = "NO AUDIO";
            currentTrackArtistName.clear();
            statusLabel.setText ("no audio files in documents", juce::dontSendNotification);
            updateNowPlayingInfo();
            return false;
        }

        if (! loadAudioFile (fallbackNoiseFile))
            return false;

        currentAudioFileIndex = -1;
        currentAudioFileName = fallbackNoiseFile.getFullPathName();
        currentTrackTitle = "WHITE NOISE";
        currentTrackArtistName.clear();
        statusLabel.setText ("loaded white noise", juce::dontSendNotification);
        updateNowPlayingInfo();
        return true;
    }

    const auto fileCount = static_cast<int> (availableAudioFiles.size());

    if (index < 0)
        index = (index % fileCount + fileCount) % fileCount;
    else if (index >= fileCount)
        index %= fileCount;

    return loadAudioFile (availableAudioFiles[static_cast<size_t> (index)]);
}

bool MainComponent::loadAudioFile (const juce::File& file)
{
    int matchingIndex = -1;

    for (size_t index = 0; index < availableAudioFiles.size(); ++index)
    {
        if (availableAudioFiles[index].getFullPathName().equalsIgnoreCase (file.getFullPathName()))
        {
            matchingIndex = static_cast<int> (index);
            break;
        }
    }

    if (matchingIndex >= 0)
        currentAudioFileIndex = matchingIndex;

    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (file));

    if (reader == nullptr)
    {
        currentTrackDurationSeconds = 0.0;
        statusLabel.setText ("failed to load " + file.getFileName().toLowerCase(), juce::dontSendNotification);
        return false;
    }

    const auto metadataTitle = getMetadataValue (reader->metadataValues, { "TITLE", "Title", "title" });
    const auto metadataArtist = getMetadataValue (reader->metadataValues, { "ARTIST", "Artist", "artist", "ALBUM ARTIST", "Album Artist", "album artist", "AUTHOR", "Author", "author", "COMPOSER", "Composer", "composer" });

#if JUCE_IOS
    const auto assetTitle = getAssetMetadataValue (file.getFullPathName(), AVMetadataCommonKeyTitle);
    const auto assetArtist = getAssetMetadataValue (file.getFullPathName(), AVMetadataCommonKeyArtist);
#endif

    const auto sampleRate = reader->sampleRate;
    currentTrackDurationSeconds = sampleRate > 0.0 ? static_cast<double> (reader->lengthInSamples) / sampleRate : 0.0;
    auto nextReaderSource = std::make_unique<juce::AudioFormatReaderSource> (reader.release(), true);
    auto previousReaderSource = std::move (readerSource);

    {
        const juce::ScopedLock lock (audioSourceLock);

        transportSource.stop();
        readerSource = std::move (nextReaderSource);
        transportSource.setSource (readerSource.get(), 0, nullptr, sampleRate);
        transportSource.setPosition (0.0);
        previousReaderSource.reset();
    }

    currentAudioFileName = file.getFullPathName();
    currentTrackTitle = metadataTitle.isNotEmpty() ? metadataTitle
                                                   : (assetTitle.isNotEmpty() ? assetTitle
                                                                              : file.getFileNameWithoutExtension());
    currentTrackArtistName = metadataArtist;
    playbackFinishedHandled = false;

#if JUCE_IOS
    if (currentTrackArtistName.isEmpty())
    currentTrackArtistName = assetArtist;
#endif

    if (lockScreenPlaybackController != nullptr)
    {
        lockScreenPlaybackController->setTrackMetadata (currentTrackTitle,
                                                        currentTrackArtistName,
                                                        currentAudioFileName);
    }
    configureLockScreenControls();
    statusLabel.setText ("loaded " + currentTrackTitle.toLowerCase(), juce::dontSendNotification);
    updateNowPlayingInfo();
    return true;
}

void MainComponent::playPreviousTrack()
{
    configureLockScreenControls();

    const auto shouldKeepPlaying = transportSource.isPlaying();
    const auto targetIndex = getTrackIndexForNavigation (false);

    if (! loadAudioFileAtIndex (targetIndex))
        return;

    if (shouldKeepPlaying)
        transportSource.start();

    playButton.setToggleState (shouldKeepPlaying, juce::dontSendNotification);
    statusLabel.setText (shouldKeepPlaying ? ("playing " + currentTrackTitle.toLowerCase())
                                           : ("loaded " + currentTrackTitle.toLowerCase()),
                         juce::dontSendNotification);
    if (audioBrowserHost != nullptr)
        refreshAudioBrowserDirectory();
    updateNowPlayingInfo();
}

void MainComponent::playNextTrack()
{
    configureLockScreenControls();

    const auto shouldKeepPlaying = transportSource.isPlaying();

    const auto targetIndex = getTrackIndexForNavigation (true);

    if (! loadAudioFileAtIndex (targetIndex))
        return;

    if (shouldKeepPlaying)
        transportSource.start();

    playButton.setToggleState (shouldKeepPlaying, juce::dontSendNotification);
    statusLabel.setText (shouldKeepPlaying ? ("playing " + currentTrackTitle.toLowerCase())
                                           : ("loaded " + currentTrackTitle.toLowerCase()),
                         juce::dontSendNotification);
    if (audioBrowserHost != nullptr)
        refreshAudioBrowserDirectory();
    updateNowPlayingInfo();
}

void MainComponent::restartCurrentTrack()
{
    if (readerSource == nullptr)
        return;

    const juce::ScopedLock lock (audioSourceLock);
    transportSource.setPosition (0.0);
    playbackFinishedHandled = false;
}

std::vector<juce::File> MainComponent::getCurrentFolderTracks() const
{
    std::vector<juce::File> tracks;

    if (currentAudioFileName.isEmpty())
        return tracks;

    const auto currentFolder = juce::File (currentAudioFileName).getParentDirectory();

    for (const auto& file : availableAudioFiles)
    {
        if (file.getParentDirectory().getFullPathName().equalsIgnoreCase (currentFolder.getFullPathName()))
            tracks.push_back (file);
    }

    return tracks;
}

int MainComponent::getCurrentFolderTrackIndex (const std::vector<juce::File>& tracks) const
{
    if (currentAudioFileName.isEmpty())
        return -1;

    const auto currentPath = juce::File (currentAudioFileName).getFullPathName();

    for (size_t index = 0; index < tracks.size(); ++index)
    {
        if (tracks[index].getFullPathName().equalsIgnoreCase (currentPath))
            return static_cast<int> (index);
    }

    return -1;
}

juce::String MainComponent::getPlaybackModeLabel() const
{
    switch (playbackMode)
    {
        case PlaybackMode::repeatOne: return "ONE";
        case PlaybackMode::repeatFolder: return "ALL";
        case PlaybackMode::shuffleFolder: return "RND";
    }

    return "ALL";
}

int MainComponent::getTrackIndexForNavigation (bool movingForward) const
{
    if (availableAudioFiles.empty())
        return -1;

    const auto fileCount = static_cast<int> (availableAudioFiles.size());
    const auto currentIndex = juce::jlimit (0, fileCount - 1, currentAudioFileIndex < 0 ? 0 : currentAudioFileIndex);

    switch (playbackMode)
    {
        case PlaybackMode::repeatOne:
            return currentIndex;

        case PlaybackMode::repeatFolder:
            return (currentIndex + (movingForward ? 1 : fileCount - 1)) % fileCount;

        case PlaybackMode::shuffleFolder:
        {
            if (fileCount == 1)
                return currentIndex;

            auto& random = juce::Random::getSystemRandom();
            auto nextIndex = currentIndex;

            for (int attempt = 0; attempt < 8 && nextIndex == currentIndex; ++attempt)
                nextIndex = random.nextInt (fileCount);

            if (nextIndex == currentIndex)
                nextIndex = (currentIndex + (movingForward ? 1 : fileCount - 1)) % fileCount;

            return nextIndex;
        }
    }

    return currentIndex;
}

void MainComponent::updatePlaybackModeButton()
{
    playbackModeButton.setButtonText (getPlaybackModeLabel());
}

void MainComponent::cyclePlaybackMode()
{
    switch (playbackMode)
    {
        case PlaybackMode::repeatOne: playbackMode = PlaybackMode::repeatFolder; break;
        case PlaybackMode::repeatFolder: playbackMode = PlaybackMode::shuffleFolder; break;
        case PlaybackMode::shuffleFolder: playbackMode = PlaybackMode::repeatOne; break;
    }

    updatePlaybackModeButton();
    statusLabel.setText ("playback mode: " + getPlaybackModeLabel(), juce::dontSendNotification);
}

void MainComponent::handlePlaybackFinished()
{
    if (playbackFinishedHandled)
        return;

    playbackFinishedHandled = true;

    const auto folderTracks = getCurrentFolderTracks();

    if (folderTracks.empty())
    {
        stopPlayback();
        return;
    }

    const auto currentIndex = juce::jlimit (0, static_cast<int> (folderTracks.size()) - 1, getCurrentFolderTrackIndex (folderTracks));

    switch (playbackMode)
    {
        case PlaybackMode::repeatOne:
            if (loadAudioFile (folderTracks[static_cast<size_t> (currentIndex)]))
                startPlayback();
            break;

        case PlaybackMode::repeatFolder:
        {
            const auto nextIndex = (currentIndex + 1) % static_cast<int> (folderTracks.size());

            if (loadAudioFile (folderTracks[static_cast<size_t> (nextIndex)]))
                startPlayback();

            break;
        }

        case PlaybackMode::shuffleFolder:
        {
            if (folderTracks.size() == 1)
            {
                if (loadAudioFile (folderTracks.front()))
                    startPlayback();

                break;
            }

            auto* random = &juce::Random::getSystemRandom();
            int nextIndex = currentIndex;

            for (int attempt = 0; attempt < 8 && nextIndex == currentIndex; ++attempt)
                nextIndex = random->nextInt ((int) folderTracks.size());

            if (nextIndex == currentIndex)
                nextIndex = (currentIndex + 1) % static_cast<int> (folderTracks.size());

            if (loadAudioFile (folderTracks[static_cast<size_t> (nextIndex)]))
                startPlayback();

            break;
        }
    }

    if (audioBrowserHost != nullptr)
        refreshAudioBrowserDirectory();
}

void MainComponent::browseAudioFiles()
{
    pluginMenuHost.reset();
    pluginWindowHost.reset();
    pluginWindowAnchor.setVisible (false);

    if (audioBrowserHost != nullptr)
    {
        audioBrowserHost.reset();
        statusLabel.setText ("file browser closed", juce::dontSendNotification);
        return;
    }

    audioBrowserDirectory = getAudioRootDirectory();
    refreshAudioBrowserDirectory();
}

void MainComponent::refreshAudioBrowserDirectory()
{
    audioBrowserEntries.clear();

    if (! audioBrowserDirectory.exists() || ! audioBrowserDirectory.isDirectory())
        audioBrowserDirectory = getAudioRootDirectory();

    if (audioBrowserDirectory != getAudioRootDirectory())
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
        audioBrowserEntries.push_back ({ folder, folder.getFileName() + "/", true, false });

    for (const auto& file : files)
    {
        if (isPlayableAudioFile (file))
            audioBrowserEntries.push_back ({ file, file.getFileName(), false, false });
    }

    const auto browserBounds = getAudioBrowserWindowBounds();
    std::vector<FileBrowserContent::Row> browserRows;
    browserRows.reserve (audioBrowserEntries.size());

    const juce::File currentFile (currentAudioFileName);

    for (size_t index = 0; index < audioBrowserEntries.size(); ++index)
    {
        const auto& entry = audioBrowserEntries[index];
        const auto isSelected = currentAudioFileName.isNotEmpty()
                                && entry.file.getFullPathName().equalsIgnoreCase (currentAudioFileName);
        const auto isPathActive = entry.isDirectory
                                  && currentAudioFileName.isNotEmpty()
                                  && currentFile.isAChildOf (entry.file);

        browserRows.push_back ({ entry.label, isSelected, isPathActive });
    }

    auto browserContent = std::make_unique<FileBrowserContent> (std::move (browserRows),
                                                                [safeThis = juce::Component::SafePointer<MainComponent> (this)] (int index)
                                                                {
                                                                    if (safeThis != nullptr)
                                                                        safeThis->handleAudioBrowserSelection (index);
                                                                });

    browserContent->setSize (browserBounds.getWidth() - 2, browserBounds.getHeight() - 2);

    audioBrowserHost.reset();
    audioBrowserHost = std::make_unique<PluginWindowFrame> (std::move (browserContent));
    addAndMakeVisible (*audioBrowserHost);
    audioBrowserHost->setBounds (browserBounds);
    audioBrowserHost->toFront (true);

    statusLabel.setText (audioBrowserDirectory == getAudioRootDirectory() ? "browsing documents"
                                                                          : "browsing " + audioBrowserDirectory.getFileName(),
                         juce::dontSendNotification);
}

void MainComponent::handleAudioBrowserSelection (int selectedIndex)
{
    if (selectedIndex < 0 || selectedIndex >= static_cast<int> (audioBrowserEntries.size()))
        return;

    const auto entry = audioBrowserEntries[static_cast<size_t> (selectedIndex)];

    if (entry.isParent || entry.isDirectory)
    {
        audioBrowserDirectory = entry.file;
        refreshAudioBrowserDirectory();
        return;
    }

    if (! loadAudioFile (entry.file))
        return;

    startPlayback();
    if (audioBrowserHost != nullptr)
        refreshAudioBrowserDirectory();
    updateNowPlayingInfo();
}

void MainComponent::startPlayback()
{
    configureLockScreenControls();
    juce::Logger::writeToLog ("startPlayback requested for track: " + currentTrackTitle);

    if (readerSource == nullptr)
    {
        if (! loadAudioFileAtIndex (currentAudioFileIndex < 0 ? 0 : currentAudioFileIndex))
            return;
    }

    if (currentTrackDurationSeconds > 0.0 && transportSource.getCurrentPosition() >= currentTrackDurationSeconds)
        restartCurrentTrack();

    playbackFinishedHandled = false;

    if (! transportSource.isPlaying())
    {
        transportSource.start();
        playButton.setToggleState (true, juce::dontSendNotification);
    }

    updatePlayButtonLabel();

    statusLabel.setText ("playing " + currentTrackTitle.toLowerCase(), juce::dontSendNotification);
    updateNowPlayingInfo();
    maybeScheduleAutomationLockScreen();
}

void MainComponent::stopPlayback()
{
    configureLockScreenControls();

    if (transportSource.isPlaying())
        transportSource.stop();

    playButton.setToggleState (false, juce::dontSendNotification);

    if (readerSource == nullptr)
    {
        statusLabel.setText ("paused", juce::dontSendNotification);
    }
    else
    {
        statusLabel.setText ("paused " + currentTrackTitle.toLowerCase(), juce::dontSendNotification);
    }

    updatePlayButtonLabel();

    updateNowPlayingInfo();
}

void MainComponent::togglePlay()
{
    configureLockScreenControls();

    if (transportSource.isPlaying())
        stopPlayback();
    else
        startPlayback();

    updatePlayButtonLabel();
}

void MainComponent::updatePlayButtonLabel()
{
    playButton.setButtonText (transportSource.isPlaying() ? "PAUSE" : "PLAY");
}

bool MainComponent::isPlaybackActive() const
{
    return transportSource.isPlaying();
}

void MainComponent::configureLockScreenControls()
{
#if JUCE_IOS
    if (lockScreenPlaybackController == nullptr)
    {
        lockScreenPlaybackController = std::make_unique<ple::LockScreenPlaybackController> (
            [safeThis = juce::Component::SafePointer<MainComponent> (this)]
            {
                if (safeThis != nullptr)
                    safeThis->startPlayback();
            },
            [safeThis = juce::Component::SafePointer<MainComponent> (this)]
            {
                if (safeThis != nullptr)
                    safeThis->stopPlayback();
            },
            [safeThis = juce::Component::SafePointer<MainComponent> (this)]
            {
                if (safeThis != nullptr)
                    safeThis->togglePlay();
            },
            [safeThis = juce::Component::SafePointer<MainComponent> (this)]
            {
                if (safeThis != nullptr)
                    safeThis->playPreviousTrack();
            },
            [safeThis = juce::Component::SafePointer<MainComponent> (this)]
            {
                if (safeThis != nullptr)
                    safeThis->playNextTrack();
            },
            [safeThis = juce::Component::SafePointer<MainComponent> (this)]
            {
                return safeThis != nullptr && safeThis->isPlaybackActive();
            },
            [safeThis = juce::Component::SafePointer<MainComponent> (this)]
            {
                return safeThis != nullptr ? safeThis->transportSource.getCurrentPosition() : 0.0;
            },
            [safeThis = juce::Component::SafePointer<MainComponent> (this)]
            {
                return safeThis != nullptr ? safeThis->currentTrackDurationSeconds : 0.0;
            });
    }

    lockScreenPlaybackController->install();
    lockScreenPlaybackController->setTrackMetadata (currentTrackTitle,
                                                    currentTrackArtistName,
                                                    currentAudioFileName);
#endif
}

void MainComponent::updateNowPlayingInfo()
{
#if JUCE_IOS
    if (lockScreenPlaybackController != nullptr)
    {
        lockScreenPlaybackController->setTrackMetadata (currentTrackTitle,
                                                        currentTrackArtistName,
                                                        currentAudioFileName);
        juce::Logger::writeToLog ("Now Playing updated: " + currentTrackTitle + " | " + currentAudioFileName);
    }

    maybeScheduleAutomationLockScreen();
#endif
}

void MainComponent::maybeScheduleAutomationLockScreen()
{
#if JUCE_IOS
    if (! automationLockScreenOnLaunch || automationLockScreenScheduled)
        return;

    if (automationPlayOnLaunch && ! transportSource.isPlaying())
        return;

    automationLockScreenScheduled = true;

    juce::MessageManager::callAsync ([]
    {
        juce::Timer::callAfterDelay (800, []
        {
            requestDeviceLock();
        });
    });
#endif
}

void MainComponent::choosePlugin()
{
    if (! choosePluginButton.isEnabled())
        return;

    audioBrowserHost.reset();

    if (pluginMenuHost != nullptr)
    {
        pluginMenuHost.reset();
        statusLabel.setText ("plugin selection cancelled", juce::dontSendNotification);
        return;
    }

    choosePluginButton.setEnabled (false);
    statusLabel.setText ("scanning installed auv3 plugins", juce::dontSendNotification);

    installedPluginDescriptions = findInstalledAudioUnitDescriptions();
    choosePluginButton.setEnabled (true);

    if (installedPluginDescriptions.empty())
    {
        statusLabel.setText ("no installed auv3 plugins", juce::dontSendNotification);
        return;
    }

    statusLabel.setText ("found " + juce::String (installedPluginDescriptions.size()) + " installed auv3 plugin(s)", juce::dontSendNotification);

    const auto pluginWindowBounds = getPluginWindowBounds();

    auto menuContent = std::make_unique<PluginMenuContent> (installedPluginDescriptions,
                                                            [safeThis = juce::Component::SafePointer<MainComponent> (this)] (int selectedIndex)
                                                            {
                                                                if (safeThis == nullptr)
                                                                    return;

                                                                safeThis->handlePluginMenuSelection (selectedIndex);
                                                            });

    menuContent->setSize (pluginWindowBounds.getWidth() - 2, pluginWindowBounds.getHeight() - 2);
    pluginMenuHost = std::make_unique<PluginWindowFrame> (std::move (menuContent));
    addAndMakeVisible (*pluginMenuHost);
    pluginMenuHost->setBounds (pluginWindowBounds);
    pluginMenuHost->toFront (true);
}

void MainComponent::handlePluginMenuSelection (int selectedIndex)
{
    pluginMenuHost.reset();

    if (selectedIndex < 0 || selectedIndex >= static_cast<int> (installedPluginDescriptions.size()))
    {
        statusLabel.setText ("plugin selection cancelled", juce::dontSendNotification);
        return;
    }

    statusLabel.setText ("loading plugin", juce::dontSendNotification);
    loadPluginDescription (installedPluginDescriptions[static_cast<size_t> (selectedIndex)]);
}

void MainComponent::loadPluginDescription(const juce::PluginDescription& description, bool openGuiAfterLoad)
{
    std::atomic_store (&pluginInstance, std::shared_ptr<juce::AudioPluginInstance> {});
    openPluginGuiButton.setEnabled(false);

    const auto loadToken = ++pluginLoadToken;

    audioUnitFormat.createPluginInstanceAsync(description,
                                              currentSampleRate,
                                              currentBlockSize,
                                              [safeThis = juce::Component::SafePointer<MainComponent>(this), loadToken, openGuiAfterLoad] (std::unique_ptr<juce::AudioPluginInstance> instance,
                                                                                                                         const juce::String& errorMessage) mutable
                                              {
                                                  if (safeThis == nullptr || safeThis->pluginLoadToken != loadToken)
                                                      return;

                                                  if (instance != nullptr)
                                                  {
                                                      auto sharedInstance = std::shared_ptr<juce::AudioPluginInstance> (instance.release());
                                                      sharedInstance->prepareToPlay (safeThis->currentSampleRate,
                                                                                     safeThis->currentBlockSize);
                                                      std::atomic_store (&safeThis->pluginInstance, std::move (sharedInstance));
                                                      safeThis->openPluginGuiButton.setEnabled(true);
                                                      safeThis->statusLabel.setText("plugin loaded", juce::dontSendNotification);

                                                      if (openGuiAfterLoad)
                                                      {
                                                          juce::MessageManager::callAsync ([safeThis, loadToken]
                                                          {
                                                              if (safeThis != nullptr && safeThis->pluginLoadToken == loadToken)
                                                                  safeThis->openPluginGui();
                                                          });
                                                      }
                                                  }
                                                  else
                                                  {
                                                      if (errorMessage.isNotEmpty())
                                                          safeThis->statusLabel.setText("plugin load failed: " + errorMessage.toLowerCase(), juce::dontSendNotification);
                                                      else
                                                          safeThis->statusLabel.setText("plugin load failed", juce::dontSendNotification);
                                                  }
                                              });
}

void MainComponent::openPluginGui()
{
    pluginMenuHost.reset();
    audioBrowserHost.reset();

    if (pluginWindowHost != nullptr)
    {
        pluginWindowHost.reset();
        pluginWindowAnchor.setVisible (false);
        openPluginGuiButton.setButtonText ("OPEN");
        statusLabel.setText ("plugin gui closed", juce::dontSendNotification);
        return;
    }

    auto activePlugin = std::atomic_load (&pluginInstance);

    if (! activePlugin)
    {
        statusLabel.setText("no plugin loaded", juce::dontSendNotification);
        return;
    }

    if (! activePlugin->hasEditor())
    {
        statusLabel.setText("plugin has no gui", juce::dontSendNotification);
        return;
    }

    if (auto* editor = activePlugin->createEditorIfNeeded())
    {
        pluginWindowHost = std::make_unique<PluginWindowFrame> (std::unique_ptr<juce::Component> (editor));
        pluginWindowAnchor.addAndMakeVisible (*pluginWindowHost);
        pluginWindowAnchor.setVisible (true);

        pluginWindowAnchor.setBounds (getPluginWindowBounds());
        pluginWindowHost->setBounds (pluginWindowAnchor.getLocalBounds());
        openPluginGuiButton.setButtonText ("CLOSE");
        statusLabel.setText("plugin gui opened", juce::dontSendNotification);
    }
    else
    {
        statusLabel.setText("failed to open plugin gui", juce::dontSendNotification);
    }
}

void MainComponent::paint(juce::Graphics& g)
{
    g.fillAll(uiBlack);
}

void MainComponent::resized()
{
    auto area = getLocalBounds();
    area.removeFromTop (uiVerticalInset);
    area.removeFromBottom (uiVerticalInset);
    area.removeFromLeft (uiHorizontalInset);
    area.removeFromRight (uiHorizontalInset);

    auto footerBounds = area.removeFromBottom (uiFooterHeight);
    footerLabel.setBounds (footerBounds);

    area.removeFromBottom (uiSectionGap);

    auto bottomRow = area.removeFromBottom (uiButtonHeight);
    bottomRow.removeFromLeft (uiHorizontalInset);
    bottomRow.removeFromRight (uiHorizontalInset);

    const auto buttonWidth = juce::jmax (0, (bottomRow.getWidth() - (uiButtonGap * 2)) / 3);

    choosePluginButton.setBounds (bottomRow.removeFromLeft (buttonWidth));
    bottomRow.removeFromLeft (uiButtonGap);
    openPluginGuiButton.setBounds (bottomRow.removeFromLeft (buttonWidth));
    bottomRow.removeFromLeft (uiButtonGap);
    browseButton.setBounds (bottomRow);

    auto topRow = area.removeFromTop (uiButtonHeight);
    topRow.removeFromLeft (uiHorizontalInset);
    topRow.removeFromRight (uiHorizontalInset);

    const auto transportButtonWidth = juce::jmax (0, (topRow.getWidth() - (uiButtonGap * 3)) / 4);

    playbackModeButton.setBounds (topRow.removeFromLeft (transportButtonWidth));
    topRow.removeFromLeft (uiButtonGap);
    previousButton.setBounds (topRow.removeFromLeft (transportButtonWidth));
    topRow.removeFromLeft (uiButtonGap);
    playButton.setBounds (topRow.removeFromLeft (transportButtonWidth));
    topRow.removeFromLeft (uiButtonGap);
    nextButton.setBounds (topRow);
    const auto pluginWindowBounds = getPluginWindowBounds();
    pluginWindowAnchor.setBounds (pluginWindowBounds);

    if (pluginWindowHost != nullptr)
        pluginWindowHost->setBounds (pluginWindowAnchor.getLocalBounds());

    if (pluginMenuHost != nullptr)
        pluginMenuHost->setBounds (pluginWindowBounds);

    if (audioBrowserHost != nullptr)
        audioBrowserHost->setBounds (pluginWindowBounds);

    pluginWindowAnchor.setVisible (pluginWindowHost != nullptr);
}

juce::Rectangle<int> MainComponent::getPluginWindowBounds() const
{
    return juce::Rectangle<int> (playbackModeButton.getX(),
                                 previousButton.getBottom() + uiPluginWindowGap,
                                 nextButton.getRight() - playbackModeButton.getX(),
                                 choosePluginButton.getY() - (previousButton.getBottom() + (uiPluginWindowGap * 2)));
}

juce::Rectangle<int> MainComponent::getAudioBrowserWindowBounds() const
{
    return getPluginWindowBounds();
}
