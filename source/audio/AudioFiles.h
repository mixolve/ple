#pragma once

#include <JuceHeader.h>

namespace ple
{
juce::File getAudioRootDirectory();
bool isPlayableAudioFile (const juce::File& file);
}
