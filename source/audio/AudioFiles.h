#pragma once

#include <JuceHeader.h>

namespace ple
{
juce::File getAudioRootDirectory();
bool isPlayableAudioFile (const juce::File& file);
bool isBrowserFileMarked (const juce::File& file);
void setBrowserFileMarked (const juce::File& file, bool shouldBeMarked);
}
