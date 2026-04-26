#pragma once

#include <JuceHeader.h>
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
    PlaybackMode playbackMode = PlaybackMode::repeatFolder;
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

    std::shared_ptr<juce::AudioPluginInstance> getPluginInstance() const;
    bool hasPluginInstance() const;
    void clearPluginInstance();
    void setPluginInstance (std::shared_ptr<juce::AudioPluginInstance> newInstance);

    PlaybackController (const PlaybackController&) = delete;
    PlaybackController& operator= (const PlaybackController&) = delete;
    PlaybackController (PlaybackController&&) = delete;
    PlaybackController& operator= (PlaybackController&&) = delete;

private:
    bool loadAudioFileAtIndex (int index);
    void restartCurrentTrack();
    int getTrackIndexForNavigation (bool movingForward) const;
    int getCurrentFolderTrackIndex (const std::vector<juce::File>& tracks) const;

    PlaybackState& state;
};
}
