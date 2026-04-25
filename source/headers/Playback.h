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

struct PlaybackState final
{
    juce::AudioFormatManager formatManager;
    std::unique_ptr<juce::AudioFormatReaderSource> readerSource;
    juce::AudioTransportSource transportSource;
    juce::CriticalSection audioSourceLock;
    juce::CriticalSection pluginStateLock;

    double currentSampleRate = 44100.0;
    int currentBlockSize = 512;

    juce::AudioUnitPluginFormat audioUnitFormat;
    std::shared_ptr<juce::AudioPluginInstance> pluginInstance;
    juce::AudioBuffer<float> pluginScratchBuffer;

    std::vector<juce::File> availableAudioFiles;
    std::vector<juce::File> playbackQueue;

    int currentAudioFileIndex = -1;
    juce::String currentAudioFileName;
    juce::String currentTrackTitle { "NO AUDIO" };
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
    bool loadAudioFileAtIndex (int index);
    bool loadAudioFile (const juce::File& file);
    void restartCurrentTrack();
    void handlePlaybackFinished();
    void cyclePlaybackMode();
    juce::String getPlaybackModeLabel() const;
    int getTrackIndexForNavigation (bool movingForward) const;
    void startPlayback();
    void pausePlayback();
    void togglePlayback();
    void playPreviousTrack();
    void playNextTrack();
    std::vector<juce::File> getCurrentFolderTracks() const;
    int getCurrentFolderTrackIndex (const std::vector<juce::File>& tracks) const;

    bool isPlaybackActive() const;
    double getCurrentPosition() const;
    double getDuration() const;
    juce::String getStatusText() const;
    double getCurrentSampleRate() const;
    int getCurrentBlockSize() const;

    const std::vector<juce::File>& getAvailableAudioFiles() const;
    const std::vector<juce::File>& getPlaybackQueue() const;
    juce::File getAudioBrowserDirectory() const;
    juce::File getPlaybackScopeDirectory() const;
    void setAudioBrowserDirectory (juce::File newDirectory);
    void setPlaybackScopeDirectory (juce::File newDirectory);

    int getCurrentAudioFileIndex() const;
    juce::String getCurrentAudioFileName() const;
    juce::String getCurrentTrackTitle() const;

    std::shared_ptr<juce::AudioPluginInstance> getPluginInstance() const;
    bool hasPluginInstance() const;
    void clearPluginInstance();
    void setPluginInstance (std::shared_ptr<juce::AudioPluginInstance> newInstance);

    PlaybackController (const PlaybackController&) = delete;
    PlaybackController& operator= (const PlaybackController&) = delete;
    PlaybackController (PlaybackController&&) = delete;
    PlaybackController& operator= (PlaybackController&&) = delete;

private:
    PlaybackState& state;
};
}