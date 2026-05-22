#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <memory>
#include <vector>

namespace ple
{
enum class PlaybackMode
{
    repeatOne,
    repeatFolder,
    shuffleFolder
};

enum class SeekTransitionStage
{
    idle,
    fadingOut,
    silentAfterSeek,
    fadingIn
};

struct NowPlayingTrack final
{
    juce::String filePath;
    juce::String title;
    juce::String artist;
    juce::String album;
    double durationSeconds = 0.0;
    double elapsedSeconds = 0.0;
    bool isPlaying = false;
    juce::Image artwork;
};

struct PlaybackState final
{
    juce::AudioFormatManager formatManager;
    std::unique_ptr<juce::AudioFormatReaderSource> readerSource;
    juce::TimeSliceThread readAheadThread { "PLE audio read ahead" };
    juce::AudioTransportSource transportSource;
    juce::CriticalSection audioSourceLock;
    juce::CriticalSection pluginStateLock;

    double currentSampleRate = 44100.0;
    int currentBlockSize = 512;

    std::shared_ptr<juce::AudioPluginInstance> pluginInstance;
    juce::AudioBuffer<float> pluginScratchBuffer;

    std::vector<juce::File> availableAudioFiles;
    std::vector<juce::File> playbackQueue;

    int currentAudioFileIndex = -1;
    juce::String currentAudioFileName;
    juce::String currentTrackTitle { "NO AUDIO" };
    juce::String currentTrackArtist;
    juce::String currentTrackAlbum;
    juce::Image currentTrackArtwork;
    double currentTrackDurationSeconds = 0.0;
    std::shared_ptr<std::atomic<bool>> metadataLifetime { std::make_shared<std::atomic<bool>> (true) };
    std::atomic<int64_t> metadataRequestId { 0 };
    std::atomic<bool> metadataUpdatePending { false };
    PlaybackMode playbackMode = PlaybackMode::repeatFolder;
    std::atomic<bool> seekRequestPending { false };
    std::atomic<double> seekRequestPositionSeconds { 0.0 };
    SeekTransitionStage seekTransitionStage = SeekTransitionStage::idle;
    int seekFadeOutSamplesRemaining = 0;
    int seekFadeOutSamplesTotal = 0;
    int seekFadeInSamplesRemaining = 0;
    int seekFadeInSamplesTotal = 0;
    int seekSilenceSamplesRemaining = 0;
    bool seekApplyPositionAfterBlock = false;
    bool seekPluginResetPending = false;
    std::atomic<bool> pauseMuteRequestPending { false };
    std::atomic<bool> pauseUnmuteRequestPending { false };
    bool pauseOutputMuted = false;
    bool pauseFadeOutActive = false;
    bool pauseFadeInActive = false;
    int pauseFadeSamplesRemaining = 0;
    int pauseFadeSamplesTotal = 0;
    double pausedPositionSeconds = 0.0;
    bool pausedPositionValid = false;
    bool playbackIsPlaying = false;
    bool playbackFinishedHandled = false;
    juce::File audioBrowserDirectory;
    juce::File playbackScopeDirectory;
    juce::String statusText { "ready" };
};

class PlaybackController final
{
public:
    explicit PlaybackController (PlaybackState& stateToUse);
    ~PlaybackController();

    void prepareToPlay (int samplesPerBlockExpected, double sampleRate);
    void getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill);
    void releaseResources();

    void refreshAudioLibrary();
    void refreshPlaybackQueue();
    bool loadAudioFile (const juce::File& file);
    void handlePlaybackFinished();
    void cyclePlaybackMode();
    void setPlaybackMode (PlaybackMode mode);
    void clearNavigationHistory();
    juce::String getPlaybackModeLabel() const;
    void startPlayback();
    void pausePlayback();
    void seekTo (double positionSeconds);
    void playPreviousTrack();
    void playNextTrack();
    std::vector<juce::File> getCurrentFolderTracks() const;

    bool isPlaybackActive() const;
    bool hasCurrentTrackEnded() const;
    double getCurrentPosition() const;
    double getDuration() const;
    juce::String getStatusText() const;
    double getCurrentSampleRate() const;
    int getCurrentBlockSize() const;

    juce::File getAudioBrowserDirectory() const;
    void setAudioBrowserDirectory (juce::File newDirectory);
    void setPlaybackScopeDirectory (juce::File newDirectory);

    juce::String getCurrentAudioFileName() const;
    NowPlayingTrack getNowPlayingTrack() const;
    bool consumeMetadataUpdatePending();

    std::shared_ptr<juce::AudioPluginInstance> getPluginInstance() const;
    bool hasPluginInstance() const;
    void clearPluginInstance();
    void setPluginInstance (std::shared_ptr<juce::AudioPluginInstance> newInstance);

    PlaybackController (const PlaybackController&) = delete;
    PlaybackController& operator= (const PlaybackController&) = delete;
    PlaybackController (PlaybackController&&) = delete;
    PlaybackController& operator= (PlaybackController&&) = delete;

private:
    bool loadAudioFileAtIndex (int index, bool recordCurrentTrackInHistory = false);
    void restartCurrentTrack();
    int getSequentialTrackIndexForNavigation (bool movingForward) const;
    int getTrackIndexForNavigation (bool movingForward) const;
    int getCurrentFolderTrackIndex (const std::vector<juce::File>& tracks) const;

    std::vector<juce::String> navigationHistory;
    PlaybackState& state;
};
}
