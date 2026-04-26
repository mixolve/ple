#pragma once

#include "audio/PlaybackController.h"

#include <functional>
#include <memory>

namespace ple
{
class LockScreenController final
{
public:
    struct Callbacks
    {
        std::function<void()> play;
        std::function<void()> pause;
        std::function<void()> nextTrack;
        std::function<void()> previousTrack;
        std::function<void (double)> seekTo;
    };

    explicit LockScreenController (Callbacks callbacks);
    ~LockScreenController();

    void activate();
    void setAudioSessionActive (bool active);
    void updateNowPlaying (const NowPlayingTrack& track);
    void clearNowPlaying();

    LockScreenController (const LockScreenController&) = delete;
    LockScreenController& operator= (const LockScreenController&) = delete;
    LockScreenController (LockScreenController&&) = delete;
    LockScreenController& operator= (LockScreenController&&) = delete;

private:
    struct Impl;

    Callbacks callbacks;
    std::unique_ptr<Impl> impl;
};
}
