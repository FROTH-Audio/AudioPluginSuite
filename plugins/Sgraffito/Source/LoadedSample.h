#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_core/juce_core.h>

#include <memory>

/**
    A single loaded sample: the decoded audio buffer at the host sample rate,
    plus the source path. The processor swaps the pointer atomically, so the
    audio thread only ever sees a finished buffer.
*/
struct LoadedSample
{
    juce::AudioBuffer<float> buffer;      // decoded at target sample rate
    juce::String sourcePath;              // file path, or "embedded:<resource>" for bundled samples
    double sourceSampleRate = 0.0;        // original file sample rate
};
