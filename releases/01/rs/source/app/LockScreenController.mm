#if __has_include(<AVFoundation/AVFoundation.h>)
#import <AVFoundation/AVFoundation.h>
#endif

#if __has_include(<MediaPlayer/MediaPlayer.h>)
#import <MediaPlayer/MediaPlayer.h>
#endif

#if __has_include(<UIKit/UIKit.h>)
#import <UIKit/UIKit.h>
#endif

#include "app/LockScreenController.h"

namespace ple
{
namespace
{
#if JUCE_IOS
static NSString* nsStringFromJuce (const juce::String& string)
{
    return [NSString stringWithUTF8String:string.toRawUTF8()];
}

static UIImage* uiImageFromJuceImage (const juce::Image& image)
{
    if (! image.isValid())
        return nil;

    juce::MemoryOutputStream output;
    juce::PNGImageFormat pngFormat;

    if (! pngFormat.writeImageToStream (image, output))
        return nil;

    NSData* data = [NSData dataWithBytes:output.getData() length:output.getDataSize()];
    return [UIImage imageWithData:data];
}

static UIImage* fallbackArtworkImage()
{
    NSString* path = [[NSBundle mainBundle] pathForResource:@"Icon" ofType:@"png"];

    if (path == nil)
        return nil;

    return [UIImage imageWithContentsOfFile:path];
}

static MPMediaItemArtwork* createArtwork (const juce::Image& image)
{
    UIImage* uiImage = uiImageFromJuceImage (image);

    if (uiImage == nil)
        uiImage = fallbackArtworkImage();

    if (uiImage == nil)
        return nil;

    MPMediaItemArtwork* artwork = [[MPMediaItemArtwork alloc] initWithBoundsSize:uiImage.size
                                                                  requestHandler:^UIImage* (CGSize)
                                                                  {
                                                                      return uiImage;
                                                                  }];

#if ! __has_feature(objc_arc)
    [artwork autorelease];
#endif

    return artwork;
}

static MPRemoteCommandHandlerStatus runAsync (const std::function<void()>& action)
{
    if (! action)
        return MPRemoteCommandHandlerStatusNoActionableNowPlayingItem;

    juce::MessageManager::callAsync ([action]
    {
        action();
    });

    return MPRemoteCommandHandlerStatusSuccess;
}

static MPRemoteCommandHandlerStatus seekAsync (const std::function<void (double)>& action,
                                               MPRemoteCommandEvent* event)
{
    if (! action || ! [event isKindOfClass:[MPChangePlaybackPositionCommandEvent class]])
        return MPRemoteCommandHandlerStatusNoActionableNowPlayingItem;

    const auto positionSeconds = [(MPChangePlaybackPositionCommandEvent*) event positionTime];

    juce::MessageManager::callAsync ([action, positionSeconds]
    {
        action (positionSeconds);
    });

    return MPRemoteCommandHandlerStatusSuccess;
}

static void setCommandEnabled (MPRemoteCommand* command, BOOL enabled)
{
    command.enabled = enabled;
}

static void setTransportCommandState (bool isPlaying)
{
    auto* commandCenter = [MPRemoteCommandCenter sharedCommandCenter];

    juce::ignoreUnused (isPlaying);
    setCommandEnabled (commandCenter.playCommand, NO);
    setCommandEnabled (commandCenter.pauseCommand, NO);
    setCommandEnabled (commandCenter.togglePlayPauseCommand, YES);
}

static void setNowPlayingPlaybackState (bool isPlaying)
{
    if (@available(iOS 13.0, *))
    {
        [MPNowPlayingInfoCenter defaultCenter].playbackState = isPlaying
                                                            ? MPNowPlayingPlaybackStatePlaying
                                                            : MPNowPlayingPlaybackStatePaused;
    }
}

static void setPlaybackAudioSessionActive (bool active)
{
    NSError* error = nil;
    auto* session = [AVAudioSession sharedInstance];

    if (active)
    {
        [session setCategory:AVAudioSessionCategoryPlayback error:&error];
        [session setActive:YES error:&error];
        return;
    }

    [session setActive:NO
           withOptions:AVAudioSessionSetActiveOptionNotifyOthersOnDeactivation
                 error:&error];
}
#endif
}

struct LockScreenController::Impl
{
#if JUCE_IOS
    id togglePlayPauseTarget = nil;
    id nextTrackTarget = nil;
    id previousTrackTarget = nil;
    id changePlaybackPositionTarget = nil;
    bool isPlaying = false;
    juce::String lastFilePath;
    juce::String lastTitle;
    juce::String lastArtist;
    juce::String lastAlbum;
    double lastDurationSeconds = -1.0;
#endif
};

LockScreenController::LockScreenController (Callbacks callbacksToUse)
    : callbacks (std::move (callbacksToUse)),
      impl (std::make_unique<Impl>())
{
}

LockScreenController::~LockScreenController()
{
    clearNowPlaying();

#if JUCE_IOS
    auto* commandCenter = [MPRemoteCommandCenter sharedCommandCenter];

    if (impl->togglePlayPauseTarget != nil)
        [commandCenter.togglePlayPauseCommand removeTarget:impl->togglePlayPauseTarget];

    if (impl->nextTrackTarget != nil)
        [commandCenter.nextTrackCommand removeTarget:impl->nextTrackTarget];

    if (impl->previousTrackTarget != nil)
        [commandCenter.previousTrackCommand removeTarget:impl->previousTrackTarget];

    if (impl->changePlaybackPositionTarget != nil)
        [commandCenter.changePlaybackPositionCommand removeTarget:impl->changePlaybackPositionTarget];

    setCommandEnabled (commandCenter.playCommand, NO);
    setCommandEnabled (commandCenter.pauseCommand, NO);
    setCommandEnabled (commandCenter.togglePlayPauseCommand, NO);
    setCommandEnabled (commandCenter.nextTrackCommand, NO);
    setCommandEnabled (commandCenter.previousTrackCommand, NO);
    setCommandEnabled (commandCenter.changePlaybackPositionCommand, NO);
    setCommandEnabled (commandCenter.skipForwardCommand, NO);
    setCommandEnabled (commandCenter.skipBackwardCommand, NO);
    setCommandEnabled (commandCenter.seekForwardCommand, NO);
    setCommandEnabled (commandCenter.seekBackwardCommand, NO);

    [[UIApplication sharedApplication] endReceivingRemoteControlEvents];
#endif
}

void LockScreenController::activate()
{
#if JUCE_IOS
    setPlaybackAudioSessionActive (true);
    [[UIApplication sharedApplication] beginReceivingRemoteControlEvents];

    auto* commandCenter = [MPRemoteCommandCenter sharedCommandCenter];

    setTransportCommandState (false);
    setCommandEnabled (commandCenter.nextTrackCommand, YES);
    setCommandEnabled (commandCenter.previousTrackCommand, YES);
    setCommandEnabled (commandCenter.changePlaybackPositionCommand, YES);
    setCommandEnabled (commandCenter.skipForwardCommand, NO);
    setCommandEnabled (commandCenter.skipBackwardCommand, NO);
    setCommandEnabled (commandCenter.seekForwardCommand, NO);
    setCommandEnabled (commandCenter.seekBackwardCommand, NO);

    impl->togglePlayPauseTarget = [commandCenter.togglePlayPauseCommand addTargetWithHandler:^MPRemoteCommandHandlerStatus (MPRemoteCommandEvent*)
    {
        return runAsync (impl->isPlaying ? callbacks.pause : callbacks.play);
    }];

    impl->nextTrackTarget = [commandCenter.nextTrackCommand addTargetWithHandler:^MPRemoteCommandHandlerStatus (MPRemoteCommandEvent*)
    {
        return runAsync (callbacks.nextTrack);
    }];

    impl->previousTrackTarget = [commandCenter.previousTrackCommand addTargetWithHandler:^MPRemoteCommandHandlerStatus (MPRemoteCommandEvent*)
    {
        return runAsync (callbacks.previousTrack);
    }];

    impl->changePlaybackPositionTarget = [commandCenter.changePlaybackPositionCommand addTargetWithHandler:^MPRemoteCommandHandlerStatus (MPRemoteCommandEvent* event)
    {
        return seekAsync (callbacks.seekTo, event);
    }];
#endif
}

void LockScreenController::setAudioSessionActive (bool active)
{
#if JUCE_IOS
    setPlaybackAudioSessionActive (active);
#else
    juce::ignoreUnused (active);
#endif
}

void LockScreenController::updateNowPlaying (const NowPlayingTrack& track)
{
#if JUCE_IOS
    if (track.filePath.isEmpty() && track.title.isEmpty())
    {
        clearNowPlaying();
        return;
    }

    const auto title = track.title.isNotEmpty() ? track.title : juce::File (track.filePath).getFileNameWithoutExtension();
    const auto metadataChanged = track.filePath != impl->lastFilePath
                              || title != impl->lastTitle
                              || track.artist != impl->lastArtist
                              || track.album != impl->lastAlbum
                              || ! juce::approximatelyEqual (track.durationSeconds, impl->lastDurationSeconds);

    NSMutableDictionary* info = metadataChanged ? [[NSMutableDictionary alloc] init]
                                                : [[MPNowPlayingInfoCenter defaultCenter].nowPlayingInfo mutableCopy];

    if (info == nil)
        info = [[NSMutableDictionary alloc] init];

    if (metadataChanged)
    {
        if (title.isNotEmpty())
            [info setObject:nsStringFromJuce (title) forKey:MPMediaItemPropertyTitle];

        if (track.artist.isNotEmpty())
            [info setObject:nsStringFromJuce (track.artist) forKey:MPMediaItemPropertyArtist];

        if (track.album.isNotEmpty())
            [info setObject:nsStringFromJuce (track.album) forKey:MPMediaItemPropertyAlbumTitle];

        if (track.durationSeconds > 0.0)
            [info setObject:@(track.durationSeconds) forKey:MPMediaItemPropertyPlaybackDuration];

        if (auto* artwork = createArtwork (track.artwork))
            [info setObject:artwork forKey:MPMediaItemPropertyArtwork];

        impl->lastFilePath = track.filePath;
        impl->lastTitle = title;
        impl->lastArtist = track.artist;
        impl->lastAlbum = track.album;
        impl->lastDurationSeconds = track.durationSeconds;
    }

    [info setObject:@(juce::jmax (0.0, track.elapsedSeconds)) forKey:MPNowPlayingInfoPropertyElapsedPlaybackTime];
    [info setObject:@(track.isPlaying ? 1.0 : 0.0) forKey:MPNowPlayingInfoPropertyPlaybackRate];
    [info setObject:@1.0 forKey:MPNowPlayingInfoPropertyDefaultPlaybackRate];

    impl->isPlaying = track.isPlaying;

    [MPNowPlayingInfoCenter defaultCenter].nowPlayingInfo = info;
    setNowPlayingPlaybackState (track.isPlaying);
    setTransportCommandState (track.isPlaying);
#if ! __has_feature(objc_arc)
    [info autorelease];
#endif
#else
    juce::ignoreUnused (track);
#endif
}

void LockScreenController::clearNowPlaying()
{
#if JUCE_IOS
    setCommandEnabled ([MPRemoteCommandCenter sharedCommandCenter].playCommand, NO);
    setCommandEnabled ([MPRemoteCommandCenter sharedCommandCenter].pauseCommand, NO);
    setCommandEnabled ([MPRemoteCommandCenter sharedCommandCenter].togglePlayPauseCommand, NO);
    impl->isPlaying = false;
    impl->lastFilePath.clear();
    impl->lastTitle.clear();
    impl->lastArtist.clear();
    impl->lastAlbum.clear();
    impl->lastDurationSeconds = -1.0;

    if (@available(iOS 13.0, *))
        [MPNowPlayingInfoCenter defaultCenter].playbackState = MPNowPlayingPlaybackStateStopped;

    [MPNowPlayingInfoCenter defaultCenter].nowPlayingInfo = nil;
#endif
}
}
