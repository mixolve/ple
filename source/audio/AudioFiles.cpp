#include "audio/AudioFiles.h"
#include "BinaryData.h"

namespace
{
constexpr auto browserMarksFileName = "browser-marks.txt";

juce::File getBrowserMarksFile()
{
    return ple::getAudioRootDirectory().getChildFile (browserMarksFileName);
}

juce::String getBrowserFileMarkPath (const juce::File& file)
{
    const auto rootDirectory = ple::getAudioRootDirectory();
    auto path = file.getRelativePathFrom (rootDirectory).trim();

    if (path.isEmpty() || path.startsWithChar ('/'))
        path = file.getFullPathName().trim();

    return path.toLowerCase();
}

juce::StringArray& getBrowserMarkedFiles()
{
    static juce::StringArray markedFiles;
    static bool hasLoadedMarkedFiles = false;

    if (! hasLoadedMarkedFiles)
    {
        hasLoadedMarkedFiles = true;

        const auto marksFile = getBrowserMarksFile();

        if (marksFile.existsAsFile())
        {
            markedFiles.addLines (marksFile.loadFileAsString());
            markedFiles.trim();
            markedFiles.removeEmptyStrings();
            markedFiles.removeDuplicates (true);
        }
    }

    return markedFiles;
}

void saveBrowserMarkedFiles (const juce::StringArray& markedFiles)
{
    const auto marksFile = getBrowserMarksFile();

    if (markedFiles.isEmpty())
    {
        if (marksFile.existsAsFile())
            marksFile.deleteFile();

        return;
    }

    marksFile.replaceWithText (markedFiles.joinIntoString ("\n"));
}
}

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

bool ple::isBrowserFileMarked (const juce::File& file)
{
    const auto path = getBrowserFileMarkPath (file);

    if (path.isEmpty())
        return false;

    return getBrowserMarkedFiles().contains (path, true);
}

void ple::setBrowserFileMarked (const juce::File& file, bool shouldBeMarked)
{
    const auto path = getBrowserFileMarkPath (file);

    if (path.isEmpty())
        return;

    auto& markedFiles = getBrowserMarkedFiles();
    const auto alreadyMarked = markedFiles.contains (path, true);

    if (shouldBeMarked == alreadyMarked)
        return;

    if (shouldBeMarked)
        markedFiles.add (path);
    else
        markedFiles.removeString (path, true);

    saveBrowserMarkedFiles (markedFiles);
}
