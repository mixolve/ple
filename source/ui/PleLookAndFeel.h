#pragma once

#include <JuceHeader.h>
#include <memory>

namespace ple
{
std::unique_ptr<juce::LookAndFeel_V4> makeMainLookAndFeel();
}
