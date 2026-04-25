#include "Paths.h"

juce::File ple::getAudioRootDirectory()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);
}
