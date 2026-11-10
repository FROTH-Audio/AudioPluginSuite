#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_core/juce_core.h>

#include <memory>

/**
    A single loaded sample: the decoded audio buffer at the host sample rate,
    plus the FLAC-encoded bytes (for embedding) and the original file path
    (for path-based loading above the embed limit).
*/
struct LoadedSample
{
    juce::AudioBuffer<float> buffer;      // decoded at target sample rate
    juce::MemoryBlock flacData;           // FLAC-encoded bytes (empty if not embedded)
    juce::String sourcePath;              // original file path (empty if embedded)
    double sourceSampleRate = 0.0;        // original file sample rate
};

/**
    Loads WAV, AIFF and FLAC files, resamples to the host sample rate,
    and can encode/decode FLAC for embedding. All file I/O happens on the
    message thread; the audio thread only sees the finished buffer via an
    atomic pointer.
*/
class SampleLoader
{
public:
    /** Loads a file from disk. Returns nullptr if the file cannot be read. */
    static std::unique_ptr<LoadedSample> loadFile (const juce::File& file,
                                                   double targetSampleRate);

    /** Decodes FLAC bytes back into a buffer at the target sample rate. */
    static std::unique_ptr<LoadedSample> decodeFlac (const juce::MemoryBlock& flacData,
                                                     double targetSampleRate);

    /** Encodes a buffer to FLAC bytes for embedding. */
    static juce::MemoryBlock encodeFlac (const juce::AudioBuffer<float>& buffer,
                                         double sampleRate);

    /** True when the file's on-disk size exceeds the embed limit. */
    static bool exceedsEmbedLimit (const juce::File& file, size_t maxEmbedBytes);

private:
    static juce::AudioFormatManager& formatManager();
};
