#if __has_include(<AVFoundation/AVFoundation.h>)
#import <AVFoundation/AVFoundation.h>
#endif

#include "audio/PlaybackController.h"
#include "audio/AudioFiles.h"

#include <initializer_list>
#include <thread>

namespace ple
{
namespace
{
constexpr double seekFadeOutSeconds = 0.010;
constexpr double seekDecodePreRollSeconds = 0.120;
constexpr double seekSilenceSeconds = 0.0;
constexpr double seekFadeInSeconds = 0.030;
constexpr double pauseGateFadeSeconds = 0.006;
constexpr int audioReadAheadBufferSamples = 32768;

static int getSampleCountForSeconds (const PlaybackState& state, double seconds, int minSamples, int maxSamples)
{
    const auto sampleRate = state.currentSampleRate > 0.0 ? state.currentSampleRate : 44100.0;
    return juce::jlimit (minSamples, maxSamples, juce::roundToInt (sampleRate * seconds));
}

static void clearSeekTransitionState (PlaybackState& state)
{
    state.seekRequestPending.store (false);
    state.seekTransitionStage = SeekTransitionStage::idle;
    state.seekFadeOutSamplesRemaining = 0;
    state.seekFadeOutSamplesTotal = 0;
    state.seekFadeInSamplesRemaining = 0;
    state.seekFadeInSamplesTotal = 0;
    state.seekSilenceSamplesRemaining = 0;
    state.seekApplyPositionAfterBlock = false;
    state.seekPluginResetPending = false;
}

static void beginPendingSeekIfNeeded (PlaybackState& state)
{
    if (! state.seekRequestPending.exchange (false))
        return;

    state.seekTransitionStage = SeekTransitionStage::fadingOut;
    state.seekFadeOutSamplesTotal = getSampleCountForSeconds (state, seekFadeOutSeconds, 64, 1024);
    state.seekFadeOutSamplesRemaining = state.seekFadeOutSamplesTotal;
    state.seekFadeInSamplesTotal = getSampleCountForSeconds (state, seekFadeInSeconds, 128, 4096);
    state.seekFadeInSamplesRemaining = state.seekFadeInSamplesTotal;
    state.seekSilenceSamplesRemaining = getSampleCountForSeconds (state, seekSilenceSeconds, 0, 512);
    state.seekApplyPositionAfterBlock = false;
}

static void applyGainToRange (juce::AudioBuffer<float>& buffer,
                              int startSample,
                              int numSamples,
                              float gain)
{
    if (numSamples <= 0)
        return;

    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        buffer.applyGain (channel, startSample, numSamples, gain);
}

static void applySeekTransition (PlaybackState& state,
                                 juce::AudioBuffer<float>& buffer,
                                 int startSample,
                                 int numSamples)
{
    if (numSamples <= 0 || state.seekTransitionStage == SeekTransitionStage::idle)
        return;

    auto offset = 0;

    while (offset < numSamples)
    {
        switch (state.seekTransitionStage)
        {
            case SeekTransitionStage::idle:
                return;

            case SeekTransitionStage::fadingOut:
            {
                const auto samplesThisStep = juce::jmin (numSamples - offset, state.seekFadeOutSamplesRemaining);
                const auto totalSamples = juce::jmax (1, state.seekFadeOutSamplesTotal);

                for (int sample = 0; sample < samplesThisStep; ++sample)
                {
                    const auto gain = (float) (state.seekFadeOutSamplesRemaining - sample) / (float) totalSamples;
                    applyGainToRange (buffer, startSample + offset + sample, 1, gain);
                }

                state.seekFadeOutSamplesRemaining -= samplesThisStep;
                offset += samplesThisStep;

                if (state.seekFadeOutSamplesRemaining <= 0)
                {
                    state.seekApplyPositionAfterBlock = true;

                    if (offset < numSamples)
                        buffer.clear (startSample + offset, numSamples - offset);

                    return;
                }

                break;
            }

            case SeekTransitionStage::silentAfterSeek:
            {
                const auto samplesThisStep = juce::jmin (numSamples - offset, state.seekSilenceSamplesRemaining);
                buffer.clear (startSample + offset, samplesThisStep);
                state.seekSilenceSamplesRemaining -= samplesThisStep;
                offset += samplesThisStep;

                if (state.seekSilenceSamplesRemaining <= 0)
                    state.seekTransitionStage = SeekTransitionStage::fadingIn;

                break;
            }

            case SeekTransitionStage::fadingIn:
            {
                const auto samplesThisStep = juce::jmin (numSamples - offset, state.seekFadeInSamplesRemaining);
                const auto totalSamples = juce::jmax (1, state.seekFadeInSamplesTotal);
                const auto samplesAlreadyFaded = state.seekFadeInSamplesTotal - state.seekFadeInSamplesRemaining;

                for (int sample = 0; sample < samplesThisStep; ++sample)
                {
                    const auto gain = (float) (samplesAlreadyFaded + sample) / (float) totalSamples;
                    applyGainToRange (buffer, startSample + offset + sample, 1, gain);
                }

                state.seekFadeInSamplesRemaining -= samplesThisStep;
                offset += samplesThisStep;

                if (state.seekFadeInSamplesRemaining <= 0)
                {
                    state.seekTransitionStage = SeekTransitionStage::idle;
                    state.seekFadeInSamplesRemaining = 0;
                    state.seekFadeInSamplesTotal = 0;
                    return;
                }

                break;
            }
        }
    }
}

static void applySeekPositionAfterFadeOutIfNeeded (PlaybackState& state)
{
    if (! state.seekApplyPositionAfterBlock)
        return;

    const auto targetPosition = state.seekRequestPositionSeconds.load();
    const auto preRollStartPosition = juce::jmax (0.0, targetPosition - seekDecodePreRollSeconds);
    const auto preRollSeconds = juce::jmax (0.0, targetPosition - preRollStartPosition);
    const auto preRollSamples = getSampleCountForSeconds (state, preRollSeconds, 0, 8192);

    state.transportSource.setPosition (preRollStartPosition);
    state.playbackFinishedHandled = false;
    state.seekApplyPositionAfterBlock = false;
    state.seekPluginResetPending = true;
    state.seekSilenceSamplesRemaining += preRollSamples;
    state.seekTransitionStage = state.seekSilenceSamplesRemaining > 0 ? SeekTransitionStage::silentAfterSeek
                                                                      : SeekTransitionStage::fadingIn;
}

static void resetPluginAfterSeekIfNeeded (PlaybackState& state, juce::AudioPluginInstance& plugin)
{
    if (! state.seekPluginResetPending)
        return;

    plugin.reset();
    state.pluginScratchBuffer.clear();
    state.seekPluginResetPending = false;
}

static void clearPauseGateState (PlaybackState& state)
{
    state.pauseOutputMuted = false;
    state.pauseFadeOutActive = false;
    state.pauseFadeInActive = false;
    state.pauseFadeSamplesRemaining = 0;
    state.pauseFadeSamplesTotal = 0;
}

static void startPauseFadeOut (PlaybackState& state)
{
    state.pauseOutputMuted = false;
    state.pauseFadeInActive = false;
    state.pauseFadeOutActive = true;
    state.pauseFadeSamplesTotal = getSampleCountForSeconds (state, pauseGateFadeSeconds, 32, 512);
    state.pauseFadeSamplesRemaining = state.pauseFadeSamplesTotal;
}

static void startPauseFadeIn (PlaybackState& state)
{
    state.pauseOutputMuted = false;
    state.pauseFadeOutActive = false;
    state.pauseFadeInActive = true;
    state.pauseFadeSamplesTotal = getSampleCountForSeconds (state, pauseGateFadeSeconds, 32, 512);
    state.pauseFadeSamplesRemaining = state.pauseFadeSamplesTotal;
}

static void beginPendingPauseGateIfNeeded (PlaybackState& state)
{
    if (state.pauseUnmuteRequestPending.exchange (false))
    {
        state.pauseMuteRequestPending.store (false);

        if (state.pauseOutputMuted || state.pauseFadeOutActive)
            startPauseFadeIn (state);
        else
            clearPauseGateState (state);
    }

    if (state.pauseMuteRequestPending.exchange (false))
    {
        state.pauseUnmuteRequestPending.store (false);

        if (! state.pauseOutputMuted)
            startPauseFadeOut (state);
    }
}

static void applyPauseGateIfNeeded (PlaybackState& state,
                                    juce::AudioBuffer<float>& buffer,
                                    int startSample,
                                    int numSamples)
{
    if (numSamples <= 0)
        return;

    beginPendingPauseGateIfNeeded (state);

    if (state.pauseFadeOutActive)
    {
        const auto samplesThisStep = juce::jmin (numSamples, state.pauseFadeSamplesRemaining);
        const auto totalSamples = juce::jmax (1, state.pauseFadeSamplesTotal);

        for (int sample = 0; sample < samplesThisStep; ++sample)
        {
            const auto gain = (float) (state.pauseFadeSamplesRemaining - sample) / (float) totalSamples;
            applyGainToRange (buffer, startSample + sample, 1, gain);
        }

        state.pauseFadeSamplesRemaining -= samplesThisStep;

        if (state.pauseFadeSamplesRemaining <= 0)
        {
            if (samplesThisStep < numSamples)
                buffer.clear (startSample + samplesThisStep, numSamples - samplesThisStep);

            state.pauseFadeOutActive = false;
            state.pauseOutputMuted = true;
            state.pauseFadeSamplesRemaining = 0;
            state.pauseFadeSamplesTotal = 0;
        }

        return;
    }

    if (state.pauseOutputMuted)
    {
        buffer.clear (startSample, numSamples);
        return;
    }

    if (state.pauseFadeInActive)
    {
        const auto samplesThisStep = juce::jmin (numSamples, state.pauseFadeSamplesRemaining);
        const auto totalSamples = juce::jmax (1, state.pauseFadeSamplesTotal);
        const auto samplesAlreadyFaded = state.pauseFadeSamplesTotal - state.pauseFadeSamplesRemaining;

        for (int sample = 0; sample < samplesThisStep; ++sample)
        {
            const auto gain = (float) (samplesAlreadyFaded + sample) / (float) totalSamples;
            applyGainToRange (buffer, startSample + sample, 1, gain);
        }

        state.pauseFadeSamplesRemaining -= samplesThisStep;

        if (state.pauseFadeSamplesRemaining <= 0)
            clearPauseGateState (state);
    }
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
        auto fileOutputStream = file.createOutputStream();

        if (fileOutputStream == nullptr || ! fileOutputStream->openedOk())
            return false;

        juce::WavAudioFormat wavFormat;
        constexpr double sampleRate = 44100.0;
        constexpr int channels = 1;
        constexpr int bitsPerSample = 16;
        constexpr int noiseDurationSeconds = 30;

        std::unique_ptr<juce::OutputStream> outputStream (fileOutputStream.release());
        auto writer = wavFormat.createWriterFor (outputStream,
                                                 juce::AudioFormatWriterOptions()
                                                     .withSampleRate (sampleRate)
                                                     .withNumChannels (channels)
                                                     .withBitsPerSample (bitsPerSample));

        if (writer == nullptr)
            return false;

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

#if JUCE_IOS
static juce::String juceStringFromNSString (NSString* string)
{
    return string != nil ? juce::String::fromCFString ((CFStringRef) string) : juce::String();
}

static juce::Image loadImageFromNSData (NSData* data)
{
    if (data == nil || [data length] == 0)
        return {};

    juce::MemoryInputStream input ([data bytes], [data length], false);
    return juce::ImageFileFormat::loadFrom (input);
}

static NSData* getArtworkDataFromMetadataValue (id value)
{
    if ([value isKindOfClass:[NSData class]])
        return (NSData*) value;

    if ([value isKindOfClass:[NSDictionary class]])
    {
        id data = [(NSDictionary*) value objectForKey:AVMetadataKeySpaceID3];

        if ([data isKindOfClass:[NSData class]])
            return (NSData*) data;

        data = [(NSDictionary*) value objectForKey:@"data"];

        if ([data isKindOfClass:[NSData class]])
            return (NSData*) data;
    }

    return nil;
}

static void mergeNativeMetadata (const juce::File& file,
                                 juce::String& title,
                                 juce::String& artist,
                                 juce::String& album,
                                 juce::Image& artwork)
{
    @autoreleasepool
    {
        NSString* path = [NSString stringWithUTF8String:file.getFullPathName().toRawUTF8()];
        NSURL* url = [NSURL fileURLWithPath:path];
        AVURLAsset* asset = [AVURLAsset URLAssetWithURL:url options:nil];

        for (AVMetadataItem* item in [asset commonMetadata])
        {
            NSString* commonKey = [item commonKey];

            if ([commonKey isEqualToString:AVMetadataCommonKeyTitle] && title.isEmpty())
                title = juceStringFromNSString ((NSString*) [item stringValue]).trim();
            else if ([commonKey isEqualToString:AVMetadataCommonKeyArtist] && artist.isEmpty())
                artist = juceStringFromNSString ((NSString*) [item stringValue]).trim();
            else if ([commonKey isEqualToString:AVMetadataCommonKeyAlbumName] && album.isEmpty())
                album = juceStringFromNSString ((NSString*) [item stringValue]).trim();
            else if ([commonKey isEqualToString:AVMetadataCommonKeyArtwork] && ! artwork.isValid())
            {
                if (auto* data = getArtworkDataFromMetadataValue ([item value]))
                    artwork = loadImageFromNSData (data);
            }
        }
    }
}
#else
static void mergeNativeMetadata (const juce::File&,
                                 juce::String&,
                                 juce::String&,
                                 juce::String&,
                                 juce::Image&)
{
}
#endif

static void loadNativeMetadataAsync (PlaybackState& state,
                                     const juce::File& file,
                                     juce::String title,
                                     juce::String artist,
                                     juce::String album)
{
    const auto requestId = state.metadataRequestId.fetch_add (1) + 1;
    const auto lifetime = state.metadataLifetime;
    const auto filePath = file.getFullPathName();
    auto initialTitle = std::move (title);
    auto initialArtist = std::move (artist);
    auto initialAlbum = std::move (album);

    std::thread ([&state, lifetime, requestId, file, filePath, initialTitle, initialArtist, initialAlbum] () mutable
    {
        juce::Image nativeArtwork;
        mergeNativeMetadata (file, initialTitle, initialArtist, initialAlbum, nativeArtwork);

        if (lifetime == nullptr || ! lifetime->load())
            return;

        auto resolvedTitle = std::move (initialTitle);
        auto resolvedArtist = std::move (initialArtist);
        auto resolvedAlbum = std::move (initialAlbum);

        juce::MessageManager::callAsync ([&state,
                                          lifetime,
                                          requestId,
                                          filePath,
                                          asyncTitle = std::move (resolvedTitle),
                                          asyncArtist = std::move (resolvedArtist),
                                          asyncAlbum = std::move (resolvedAlbum),
                                          resolvedArtwork = std::move (nativeArtwork)] () mutable
        {
            if (lifetime == nullptr || ! lifetime->load())
                return;

            if (state.metadataRequestId.load() != requestId
                || ! state.currentAudioFileName.equalsIgnoreCase (filePath))
            {
                return;
            }

            if (asyncTitle.isNotEmpty())
                state.currentTrackTitle = asyncTitle;

            state.currentTrackArtist = asyncArtist;
            state.currentTrackAlbum = asyncAlbum;

            if (resolvedArtwork.isValid())
                state.currentTrackArtwork = resolvedArtwork;

            state.statusText = (state.playbackIsPlaying ? "playing " : "loaded ") + state.currentTrackTitle.toLowerCase();
            state.metadataUpdatePending.store (true);
        });
    }).detach();
}
}

PlaybackController::PlaybackController (PlaybackState& stateToUse)
    : state (stateToUse)
{
    state.formatManager.registerBasicFormats();
    state.readAheadThread.startThread();
}

PlaybackController::~PlaybackController()
{
    if (state.metadataLifetime != nullptr)
        state.metadataLifetime->store (false);

    releaseResources();
    clearPluginInstance();

    {
        const juce::ScopedLock lock (state.audioSourceLock);
        state.transportSource.setSource (nullptr);
        state.readerSource.reset();
    }

    state.readAheadThread.stopThread (1000);
}

void PlaybackController::prepareToPlay (int samplesPerBlockExpected, double sampleRate)
{
    state.currentSampleRate = sampleRate;
    state.currentBlockSize = samplesPerBlockExpected;
    state.transportSource.prepareToPlay (samplesPerBlockExpected, sampleRate);

    auto activePlugin = std::atomic_load (&state.pluginInstance);

    if (activePlugin != nullptr)
    {
        activePlugin->prepareToPlay (sampleRate, samplesPerBlockExpected);

        const auto scratchChannels = juce::jmax (2,
                                                 activePlugin->getTotalNumInputChannels(),
                                                 activePlugin->getTotalNumOutputChannels());

        const juce::ScopedLock pluginLock (state.pluginStateLock);
        state.pluginScratchBuffer.setSize (scratchChannels,
                                           samplesPerBlockExpected,
                                           false,
                                           false,
                                           true);
        state.pluginScratchBuffer.clear();
    }
}

void PlaybackController::getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill)
{
    juce::ScopedTryLock tryLock (state.audioSourceLock);
    auto renderedPlaybackBlock = false;

    if (tryLock.isLocked() && state.readerSource)
    {
        beginPendingSeekIfNeeded (state);
        state.transportSource.getNextAudioBlock (bufferToFill);
        renderedPlaybackBlock = bufferToFill.buffer != nullptr && bufferToFill.numSamples > 0;
    }
    else
    {
        bufferToFill.clearActiveBufferRegion();
    }

    juce::ScopedTryLock pluginLock (state.pluginStateLock);

    if (pluginLock.isLocked())
    {
        auto activePlugin = std::atomic_load (&state.pluginInstance);

        if (activePlugin != nullptr && bufferToFill.buffer != nullptr && bufferToFill.numSamples > 0)
        {
            const auto outputChannels = bufferToFill.buffer->getNumChannels();
            const auto requiredChannels = juce::jmax (2,
                                                      juce::jmax (activePlugin->getTotalNumInputChannels(),
                                                                  activePlugin->getTotalNumOutputChannels()));

            if (state.pluginScratchBuffer.getNumChannels() < requiredChannels
                || state.pluginScratchBuffer.getNumSamples() < bufferToFill.numSamples)
            {
                if (renderedPlaybackBlock)
                {
                    applySeekTransition (state, *bufferToFill.buffer, bufferToFill.startSample, bufferToFill.numSamples);
                    applySeekPositionAfterFadeOutIfNeeded (state);
                    applyPauseGateIfNeeded (state, *bufferToFill.buffer, bufferToFill.startSample, bufferToFill.numSamples);
                }

                return;
            }

            for (int channel = 0; channel < state.pluginScratchBuffer.getNumChannels(); ++channel)
            {
                if (channel < outputChannels)
                    state.pluginScratchBuffer.copyFrom (channel, 0, *bufferToFill.buffer, channel, bufferToFill.startSample, bufferToFill.numSamples);
                else
                    state.pluginScratchBuffer.clear (channel, 0, bufferToFill.numSamples);
            }

            juce::AudioBuffer<float> pluginBufferView;
            pluginBufferView.setDataToReferTo (state.pluginScratchBuffer.getArrayOfWritePointers(),
                                               state.pluginScratchBuffer.getNumChannels(),
                                               0,
                                               bufferToFill.numSamples);

            juce::MidiBuffer midiBuffer;
            resetPluginAfterSeekIfNeeded (state, *activePlugin);
            activePlugin->processBlock (pluginBufferView, midiBuffer);

            for (int channel = 0; channel < outputChannels; ++channel)
                bufferToFill.buffer->copyFrom (channel, bufferToFill.startSample, state.pluginScratchBuffer, channel, 0, bufferToFill.numSamples);
        }
    }

    if (renderedPlaybackBlock)
    {
        applySeekTransition (state, *bufferToFill.buffer, bufferToFill.startSample, bufferToFill.numSamples);
        applySeekPositionAfterFadeOutIfNeeded (state);
        applyPauseGateIfNeeded (state, *bufferToFill.buffer, bufferToFill.startSample, bufferToFill.numSamples);
    }

}

void PlaybackController::releaseResources()
{
    const juce::ScopedLock lock (state.audioSourceLock);
    state.transportSource.releaseResources();

    auto activePlugin = std::atomic_load (&state.pluginInstance);

    if (activePlugin != nullptr)
        activePlugin->releaseResources();
}

void PlaybackController::refreshAudioLibrary()
{
    state.availableAudioFiles.clear();

    juce::Array<juce::File> discoveredFiles;
    getAudioRootDirectory().findChildFiles (discoveredFiles, juce::File::findFiles, true);

    for (const auto& file : discoveredFiles)
    {
        if (isPlayableAudioFile (file))
            state.availableAudioFiles.push_back (file);
    }

    std::sort (state.availableAudioFiles.begin(), state.availableAudioFiles.end(), [] (const auto& left, const auto& right)
    {
        return left.getFullPathName().toLowerCase() < right.getFullPathName().toLowerCase();
    });

    if (state.availableAudioFiles.empty())
    {
        state.currentAudioFileIndex = -1;
        state.currentAudioFileName.clear();
        state.playbackQueue.clear();
        return;
    }

    if (state.currentAudioFileIndex < 0 || state.currentAudioFileIndex >= static_cast<int> (state.availableAudioFiles.size()))
    {
        state.currentAudioFileIndex = 0;
        state.currentAudioFileName = state.availableAudioFiles.front().getFullPathName();
        return;
    }

    if (state.currentAudioFileName.isNotEmpty())
    {
        for (size_t index = 0; index < state.availableAudioFiles.size(); ++index)
        {
            if (state.availableAudioFiles[index].getFullPathName().equalsIgnoreCase (state.currentAudioFileName))
            {
                state.currentAudioFileIndex = static_cast<int> (index);
                break;
            }
        }
    }

    refreshPlaybackQueue();
}

void PlaybackController::refreshPlaybackQueue()
{
    state.playbackQueue.clear();

    auto scopeDirectory = state.playbackScopeDirectory;

    if (! scopeDirectory.exists() || ! scopeDirectory.isDirectory())
    {
        if (state.audioBrowserDirectory.exists() && state.audioBrowserDirectory.isDirectory())
            scopeDirectory = state.audioBrowserDirectory;
        else if (state.currentAudioFileName.isNotEmpty())
            scopeDirectory = juce::File (state.currentAudioFileName).getParentDirectory();
        else
            scopeDirectory = getAudioRootDirectory();
    }

    state.playbackScopeDirectory = scopeDirectory;

    for (const auto& file : state.availableAudioFiles)
    {
        if (file.isAChildOf (scopeDirectory))
            state.playbackQueue.push_back (file);
    }

    if (state.playbackQueue.empty())
        state.playbackQueue = state.availableAudioFiles;
}

bool PlaybackController::loadAudioFileAtIndex (int index, bool recordCurrentTrackInHistory)
{
    if (state.availableAudioFiles.empty())
    {
        const auto fallbackNoiseFile = getFallbackNoiseFile();

        if (! createFallbackNoiseFileIfNeeded (fallbackNoiseFile))
        {
            state.transportSource.stop();
            state.readerSource.reset();
            state.currentAudioFileIndex = -1;
            state.currentAudioFileName.clear();
            state.currentTrackArtist.clear();
            state.currentTrackAlbum.clear();
            state.currentTrackArtwork = {};
            state.currentTrackDurationSeconds = 0.0;
            state.currentTrackTitle = "NO AUDIO";
            state.statusText = "no audio files in documents";
            return false;
        }

        if (! loadAudioFile (fallbackNoiseFile))
            return false;

        state.currentAudioFileIndex = -1;
        state.currentAudioFileName = fallbackNoiseFile.getFullPathName();
        state.currentTrackTitle = "WHITE NOISE";
        state.currentTrackArtist = "PLE";
        state.currentTrackAlbum.clear();
        state.currentTrackArtwork = {};
        state.statusText = "loaded white noise";
        return true;
    }

    const auto fileCount = static_cast<int> (state.availableAudioFiles.size());

    if (index < 0)
        index = (index % fileCount + fileCount) % fileCount;
    else if (index >= fileCount)
        index %= fileCount;

    const auto historySizeBeforeLoad = navigationHistory.size();

    if (recordCurrentTrackInHistory && state.currentAudioFileName.isNotEmpty())
        navigationHistory.push_back (state.currentAudioFileName);

    const auto loaded = loadAudioFile (state.availableAudioFiles[static_cast<size_t> (index)]);

    if (! loaded && navigationHistory.size() > historySizeBeforeLoad)
        navigationHistory.pop_back();

    return loaded;
}

void PlaybackController::clearNavigationHistory()
{
    navigationHistory.clear();
}

int PlaybackController::getSequentialTrackIndexForNavigation (bool movingForward) const
{
    if (state.availableAudioFiles.empty())
        return -1;

    const auto folderTracks = getCurrentFolderTracks();

    if (folderTracks.empty())
        return state.currentAudioFileIndex;

    const auto currentFolderIndex = getCurrentFolderTrackIndex (folderTracks);
    const auto safeCurrentIndex = juce::jlimit (0,
                                                static_cast<int> (folderTracks.size()) - 1,
                                                currentFolderIndex < 0 ? 0 : currentFolderIndex);
    const auto nextFolderIndex = (safeCurrentIndex + (movingForward ? 1 : static_cast<int> (folderTracks.size()) - 1))
                               % static_cast<int> (folderTracks.size());

    const auto& targetFile = folderTracks[static_cast<size_t> (nextFolderIndex)];

    for (size_t index = 0; index < state.availableAudioFiles.size(); ++index)
    {
        if (state.availableAudioFiles[index].getFullPathName().equalsIgnoreCase (targetFile.getFullPathName()))
            return static_cast<int> (index);
    }

    return state.currentAudioFileIndex;
}

bool PlaybackController::loadAudioFile (const juce::File& file)
{
    int matchingIndex = -1;

    for (size_t index = 0; index < state.availableAudioFiles.size(); ++index)
    {
        if (state.availableAudioFiles[index].getFullPathName().equalsIgnoreCase (file.getFullPathName()))
        {
            matchingIndex = static_cast<int> (index);
            break;
        }
    }

    if (matchingIndex >= 0)
        state.currentAudioFileIndex = matchingIndex;

    std::unique_ptr<juce::AudioFormatReader> reader (state.formatManager.createReaderFor (file));

    if (reader == nullptr)
    {
        state.currentTrackDurationSeconds = 0.0;
        state.currentTrackArtist.clear();
        state.currentTrackAlbum.clear();
        state.currentTrackArtwork = {};
        state.statusText = "failed to load " + file.getFileName().toLowerCase();
        return false;
    }

    auto metadataTitle = getMetadataValue (reader->metadataValues, { "TITLE", "Title", "title", "TIT2" });
    auto metadataArtist = getMetadataValue (reader->metadataValues, { "ARTIST", "Artist", "artist", "ALBUMARTIST", "Album Artist", "album artist", "TPE1", "TPE2" });
    auto metadataAlbum = getMetadataValue (reader->metadataValues, { "ALBUM", "Album", "album", "TALB" });

    const auto sampleRate = reader->sampleRate;
    state.currentTrackDurationSeconds = sampleRate > 0.0 ? static_cast<double> (reader->lengthInSamples) / sampleRate : 0.0;
    auto nextReaderSource = std::make_unique<juce::AudioFormatReaderSource> (reader.release(), true);
    auto previousReaderSource = std::move (state.readerSource);

    {
        const juce::ScopedLock lock (state.audioSourceLock);

        state.readerSource = std::move (nextReaderSource);
        clearSeekTransitionState (state);
        state.transportSource.setSource (state.readerSource.get(), audioReadAheadBufferSamples, &state.readAheadThread, sampleRate);
        state.transportSource.setPosition (0.0);
        previousReaderSource.reset();
    }

    state.currentAudioFileName = file.getFullPathName();
    state.currentTrackTitle = metadataTitle.isNotEmpty() ? metadataTitle
                             : file.getFileNameWithoutExtension();
    state.currentTrackArtist = metadataArtist;
    state.currentTrackAlbum = metadataAlbum;
    state.currentTrackArtwork = {};

    state.playbackFinishedHandled = false;
    state.pausedPositionSeconds = 0.0;
    state.pausedPositionValid = ! state.playbackIsPlaying;
    state.statusText = "loaded " + state.currentTrackTitle.toLowerCase();
    loadNativeMetadataAsync (state, file, metadataTitle, metadataArtist, metadataAlbum);
    return true;
}

void PlaybackController::playPreviousTrack()
{
    const auto shouldKeepPlaying = state.playbackIsPlaying;

    if (state.playbackMode == PlaybackMode::repeatOne)
    {
        restartCurrentTrack();

        if (shouldKeepPlaying)
            startPlayback();
        else
            state.statusText = "loaded " + state.currentTrackTitle.toLowerCase();

        return;
    }

    if (state.playbackMode == PlaybackMode::shuffleFolder)
    {
        while (! navigationHistory.empty())
        {
            const auto previousTrackPath = navigationHistory.back();
            navigationHistory.pop_back();

            if (previousTrackPath.isNotEmpty() && loadAudioFile (juce::File (previousTrackPath)))
            {
                if (shouldKeepPlaying)
                    startPlayback();
                else
                    state.statusText = "loaded " + state.currentTrackTitle.toLowerCase();

                return;
            }
        }
    }

    const auto targetIndex = getSequentialTrackIndexForNavigation (false);

    if (! loadAudioFileAtIndex (targetIndex))
        return;

    if (shouldKeepPlaying)
        startPlayback();
    else
        state.statusText = "loaded " + state.currentTrackTitle.toLowerCase();
}

void PlaybackController::playNextTrack()
{
    const auto shouldKeepPlaying = state.playbackIsPlaying;

    if (state.playbackMode == PlaybackMode::repeatOne)
    {
        restartCurrentTrack();

        if (shouldKeepPlaying)
            startPlayback();
        else
            state.statusText = "loaded " + state.currentTrackTitle.toLowerCase();

        return;
    }

    const auto targetIndex = getTrackIndexForNavigation (true);

    const auto shouldRecordHistory = state.playbackMode == PlaybackMode::shuffleFolder
                                   && targetIndex >= 0
                                   && targetIndex != state.currentAudioFileIndex
                                   && ! state.availableAudioFiles.empty();

    if (! loadAudioFileAtIndex (targetIndex, shouldRecordHistory))
        return;

    if (shouldKeepPlaying)
        startPlayback();
    else
        state.statusText = "loaded " + state.currentTrackTitle.toLowerCase();
}

void PlaybackController::restartCurrentTrack()
{
    if (state.readerSource == nullptr)
        return;

    const juce::ScopedLock lock (state.audioSourceLock);
    clearSeekTransitionState (state);
    state.transportSource.setPosition (0.0);
    state.playbackFinishedHandled = false;
}

std::vector<juce::File> PlaybackController::getCurrentFolderTracks() const
{
    return state.playbackQueue.empty() ? state.availableAudioFiles : state.playbackQueue;
}

int PlaybackController::getCurrentFolderTrackIndex (const std::vector<juce::File>& tracks) const
{
    if (state.currentAudioFileName.isEmpty())
        return -1;

    const auto currentPath = juce::File (state.currentAudioFileName).getFullPathName();

    for (size_t index = 0; index < tracks.size(); ++index)
    {
        if (tracks[index].getFullPathName().equalsIgnoreCase (currentPath))
            return static_cast<int> (index);
    }

    return -1;
}

juce::String PlaybackController::getPlaybackModeLabel() const
{
    switch (state.playbackMode)
    {
        case PlaybackMode::repeatOne: return "ONE";
        case PlaybackMode::repeatFolder: return "ALL";
        case PlaybackMode::shuffleFolder: return "RND";
    }

    return "ALL";
}

int PlaybackController::getTrackIndexForNavigation (bool movingForward) const
{
    if (state.availableAudioFiles.empty())
        return -1;

    const auto folderTracks = getCurrentFolderTracks();

    if (folderTracks.empty())
        return state.currentAudioFileIndex;

    const auto currentFolderIndex = getCurrentFolderTrackIndex (folderTracks);
    const auto safeCurrentIndex = juce::jlimit (0,
                                                static_cast<int> (folderTracks.size()) - 1,
                                                currentFolderIndex < 0 ? 0 : currentFolderIndex);

    int nextFolderIndex = safeCurrentIndex;

    switch (state.playbackMode)
    {
        case PlaybackMode::repeatOne:
            nextFolderIndex = safeCurrentIndex;
            break;

        case PlaybackMode::repeatFolder:
            nextFolderIndex = (safeCurrentIndex + (movingForward ? 1 : static_cast<int> (folderTracks.size()) - 1)) % static_cast<int> (folderTracks.size());
            break;

        case PlaybackMode::shuffleFolder:
        {
            if (folderTracks.size() == 1)
            {
                nextFolderIndex = safeCurrentIndex;
                break;
            }

            auto& random = juce::Random::getSystemRandom();
            auto candidateIndex = safeCurrentIndex;

            for (int attempt = 0; attempt < 8 && candidateIndex == safeCurrentIndex; ++attempt)
                candidateIndex = random.nextInt (static_cast<int> (folderTracks.size()));

            if (candidateIndex == safeCurrentIndex)
                candidateIndex = (safeCurrentIndex + (movingForward ? 1 : static_cast<int> (folderTracks.size()) - 1))
                                 % static_cast<int> (folderTracks.size());

            nextFolderIndex = candidateIndex;
            break;
        }
    }

    const auto& targetFile = folderTracks[static_cast<size_t> (nextFolderIndex)];

    for (size_t index = 0; index < state.availableAudioFiles.size(); ++index)
    {
        if (state.availableAudioFiles[index].getFullPathName().equalsIgnoreCase (targetFile.getFullPathName()))
            return static_cast<int> (index);
    }

    return state.currentAudioFileIndex;
}

void PlaybackController::cyclePlaybackMode()
{
    switch (state.playbackMode)
    {
        case PlaybackMode::repeatOne: state.playbackMode = PlaybackMode::repeatFolder; break;
        case PlaybackMode::repeatFolder: state.playbackMode = PlaybackMode::shuffleFolder; break;
        case PlaybackMode::shuffleFolder: state.playbackMode = PlaybackMode::repeatOne; break;
    }

    state.statusText = "playback mode: " + getPlaybackModeLabel();
}

void PlaybackController::setPlaybackMode (PlaybackMode mode)
{
    state.playbackMode = mode;
}

void PlaybackController::startPlayback()
{
    if (state.readerSource == nullptr)
    {
        if (! loadAudioFileAtIndex (state.currentAudioFileIndex < 0 ? 0 : state.currentAudioFileIndex))
            return;
    }

    if (state.pausedPositionValid)
    {
        auto resumePosition = state.pausedPositionSeconds;

        if (state.currentTrackDurationSeconds > 0.0 && resumePosition >= state.currentTrackDurationSeconds)
            resumePosition = 0.0;

        const juce::ScopedLock lock (state.audioSourceLock);
        clearSeekTransitionState (state);
        state.transportSource.setPosition (juce::jmax (0.0, resumePosition));
        state.pausedPositionValid = false;
    }
    else if (state.currentTrackDurationSeconds > 0.0 && state.transportSource.getCurrentPosition() >= state.currentTrackDurationSeconds)
    {
        restartCurrentTrack();
    }

    state.playbackFinishedHandled = false;

    if (! state.transportSource.isPlaying())
        state.transportSource.start();

    state.playbackIsPlaying = true;
    state.pauseMuteRequestPending.store (false);
    state.pauseUnmuteRequestPending.store (true);
    state.statusText = "playing " + state.currentTrackTitle.toLowerCase();
}

void PlaybackController::pausePlayback()
{
    state.pausedPositionSeconds = state.transportSource.getCurrentPosition();
    state.pausedPositionValid = state.readerSource != nullptr;
    state.playbackIsPlaying = false;
    state.seekRequestPending.store (false);
    state.pauseUnmuteRequestPending.store (false);
    state.pauseMuteRequestPending.store (true);
    state.statusText = state.readerSource == nullptr ? "paused" : "paused " + state.currentTrackTitle.toLowerCase();
}

void PlaybackController::seekTo (double positionSeconds)
{
    if (state.readerSource == nullptr)
        return;

    const auto targetPosition = state.currentTrackDurationSeconds > 0.0
                              ? juce::jlimit (0.0, state.currentTrackDurationSeconds, positionSeconds)
                              : juce::jmax (0.0, positionSeconds);

    if (! state.playbackIsPlaying)
    {
        const juce::ScopedLock lock (state.audioSourceLock);
        clearSeekTransitionState (state);
        state.transportSource.setPosition (targetPosition);
        state.pausedPositionSeconds = targetPosition;
        state.pausedPositionValid = true;
        state.playbackFinishedHandled = false;
        return;
    }

    state.seekRequestPositionSeconds.store (targetPosition);
    state.seekRequestPending.store (true);
}

void PlaybackController::handlePlaybackFinished()
{
    if (state.playbackFinishedHandled)
        return;

    state.playbackFinishedHandled = true;

    if (state.playbackMode == PlaybackMode::repeatOne)
    {
        if (state.readerSource != nullptr)
        {
            restartCurrentTrack();
            startPlayback();
        }

        return;
    }

    const auto folderTracks = getCurrentFolderTracks();

    if (folderTracks.empty())
    {
        if (state.transportSource.isPlaying())
            state.transportSource.stop();

        state.playbackIsPlaying = false;
        state.statusText = "stopped";
        return;
    }

    const auto currentIndex = juce::jlimit (0, static_cast<int> (folderTracks.size()) - 1, getCurrentFolderTrackIndex (folderTracks));

    switch (state.playbackMode)
    {
        case PlaybackMode::repeatOne:
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

            if (state.currentAudioFileName.isNotEmpty())
                navigationHistory.push_back (state.currentAudioFileName);

            if (loadAudioFile (folderTracks[static_cast<size_t> (nextIndex)]))
            {
                startPlayback();
            }
            else if (! navigationHistory.empty())
            {
                navigationHistory.pop_back();
            }

            break;
        }
    }
}

bool PlaybackController::isPlaybackActive() const
{
    return state.playbackIsPlaying;
}

bool PlaybackController::hasCurrentTrackEnded() const
{
    if (! isPlaybackActive()
        || state.playbackFinishedHandled
        || state.currentTrackDurationSeconds <= 0.0
        || state.transportSource.isPlaying())
    {
        return false;
    }

    return state.transportSource.getCurrentPosition() + 0.05 >= state.currentTrackDurationSeconds;
}

double PlaybackController::getCurrentPosition() const
{
    if (! state.playbackIsPlaying && state.pausedPositionValid)
        return state.pausedPositionSeconds;

    return state.transportSource.getCurrentPosition();
}

double PlaybackController::getDuration() const
{
    return state.currentTrackDurationSeconds;
}

juce::String PlaybackController::getStatusText() const
{
    return state.statusText;
}

double PlaybackController::getCurrentSampleRate() const
{
    return state.currentSampleRate;
}

int PlaybackController::getCurrentBlockSize() const
{
    return state.currentBlockSize;
}

juce::File PlaybackController::getAudioBrowserDirectory() const
{
    return state.audioBrowserDirectory;
}

void PlaybackController::setAudioBrowserDirectory (juce::File newDirectory)
{
    state.audioBrowserDirectory = std::move (newDirectory);
}

void PlaybackController::setPlaybackScopeDirectory (juce::File newDirectory)
{
    state.playbackScopeDirectory = std::move (newDirectory);
}

juce::String PlaybackController::getCurrentAudioFileName() const
{
    return state.currentAudioFileName;
}

NowPlayingTrack PlaybackController::getNowPlayingTrack() const
{
    NowPlayingTrack track;
    track.filePath = state.currentAudioFileName;
    track.title = state.currentTrackTitle;
    track.artist = state.currentTrackArtist;
    track.album = state.currentTrackAlbum;
    track.durationSeconds = state.currentTrackDurationSeconds;
    track.elapsedSeconds = getCurrentPosition();
    track.isPlaying = isPlaybackActive();
    track.artwork = state.currentTrackArtwork;
    return track;
}

bool PlaybackController::consumeMetadataUpdatePending()
{
    return state.metadataUpdatePending.exchange (false);
}

std::shared_ptr<juce::AudioPluginInstance> PlaybackController::getPluginInstance() const
{
    return std::atomic_load (&state.pluginInstance);
}

bool PlaybackController::hasPluginInstance() const
{
    return getPluginInstance() != nullptr;
}

bool PlaybackController::copyPluginStateInformation (juce::MemoryBlock& destination)
{
    const juce::ScopedLock lock (state.pluginStateLock);
    auto activePlugin = std::atomic_load (&state.pluginInstance);

    if (activePlugin == nullptr)
        return false;

    destination.reset();
    activePlugin->getStateInformation (destination);
    return true;
}

bool PlaybackController::applyPluginStateInformation (const juce::MemoryBlock& source)
{
    const juce::ScopedLock lock (state.pluginStateLock);
    auto activePlugin = std::atomic_load (&state.pluginInstance);

    if (activePlugin == nullptr || source.getSize() == 0)
        return false;

    activePlugin->setStateInformation (source.getData(), static_cast<int> (source.getSize()));
    return true;
}

void PlaybackController::clearPluginInstance()
{
    const juce::ScopedLock lock (state.pluginStateLock);
    auto previousInstance = std::atomic_exchange (&state.pluginInstance, std::shared_ptr<juce::AudioPluginInstance> {});

    if (previousInstance != nullptr)
        previousInstance->releaseResources();

    state.pluginScratchBuffer.setSize (0, 0);
}

void PlaybackController::setPluginInstance (std::shared_ptr<juce::AudioPluginInstance> newInstance)
{
    const juce::ScopedLock lock (state.pluginStateLock);

    auto previousInstance = std::atomic_exchange (&state.pluginInstance, std::shared_ptr<juce::AudioPluginInstance> {});

    if (previousInstance != nullptr)
        previousInstance->releaseResources();

    if (newInstance == nullptr)
    {
        state.pluginScratchBuffer.setSize (0, 0);
        return;
    }

    newInstance->prepareToPlay (state.currentSampleRate, state.currentBlockSize);

    const auto scratchChannels = juce::jmax (2,
                                            newInstance->getTotalNumInputChannels(),
                                            newInstance->getTotalNumOutputChannels());

    state.pluginScratchBuffer.setSize (scratchChannels,
                                       state.currentBlockSize,
                                       false,
                                       false,
                                       true);
    state.pluginScratchBuffer.clear();
    std::atomic_store (&state.pluginInstance, std::move (newInstance));
}
}
