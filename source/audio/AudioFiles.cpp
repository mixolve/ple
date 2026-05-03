#include "audio/AudioFiles.h"
#include "BinaryData.h"

juce::File ple::getAudioRootDirectory()
{
    const auto documentsDirectory = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);
    const auto readmeFile = documentsDirectory.getChildFile ("readme.md");

    // Keep a visible file in Documents so the app shows up in Files.
    if (! readmeFile.exists())
    {
        const auto readmeText = juce::String::fromUTF8 (BinaryData::readme_md, BinaryData::readme_mdSize);
        readmeFile.replaceWithText (readmeText);
    }

    return documentsDirectory;
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
