#include "audio/AudioFiles.h"

juce::File ple::getAudioRootDirectory()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);
}

bool ple::isPlayableAudioFile (const juce::File& file)
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
