#include "SampleLoader.h"

#include <cmath>

//==============================================================================
juce::AudioFormatManager& SampleLoader::formatManager()
{
    static juce::AudioFormatManager fm;
    static bool registered = false;

    if (! registered)
    {
        fm.registerBasicFormats();   // WAV, AIFF, FLAC, etc.
        registered = true;
    }

    return fm;
}

//==============================================================================
std::unique_ptr<LoadedSample> SampleLoader::loadFile (const juce::File& file,
                                                      double targetSampleRate)
{
    if (! file.existsAsFile())
        return nullptr;

    std::unique_ptr<juce::AudioFormatReader> reader (
        formatManager().createReaderFor (file));

    if (reader == nullptr)
        return nullptr;

    auto result = std::make_unique<LoadedSample>();
    result->sourcePath = file.getFullPathName();
    result->sourceSampleRate = reader->sampleRate;

    // Read the entire file into a buffer.
    juce::AudioBuffer<float> fileBuffer ((int) reader->numChannels,
                                         (int) reader->lengthInSamples);
    reader->read (&fileBuffer,
                  0,
                  (int) reader->lengthInSamples,
                  0,
                  true,   // use resampling? no - we do it ourselves
                  true);  // clear noisy gaps

    // Resample to the target sample rate if needed.
    if (std::abs (reader->sampleRate - targetSampleRate) > 0.1)
    {
        juce::AudioBuffer<float> resampled ((int) reader->numChannels,
                                            (int) (reader->lengthInSamples * targetSampleRate / reader->sampleRate));

        for (int ch = 0; ch < (int) reader->numChannels; ++ch)
        {
            const float* src = fileBuffer.getReadPointer (ch);
            float* dst = resampled.getWritePointer (ch);

            const double ratio = reader->sampleRate / targetSampleRate;
            for (int i = 0; i < resampled.getNumSamples(); ++i)
            {
                const double srcPos = i * ratio;
                const int srcIdx = (int) srcPos;
                const float frac = (float) (srcPos - srcIdx);

                if (srcIdx + 1 < fileBuffer.getNumSamples())
                    dst[i] = src[srcIdx] * (1.0f - frac) + src[srcIdx + 1] * frac;
                else
                    dst[i] = src[srcIdx];
            }
        }
        fileBuffer = std::move (resampled);
    }

    result->buffer = std::move (fileBuffer);
    return result;
}

//==============================================================================
juce::MemoryBlock SampleLoader::encodeFlac (const juce::AudioBuffer<float>& buffer,
                                            double sampleRate)
{
    auto* format = formatManager().findFormatForFileExtension (".flac");
    if (format == nullptr)
        return juce::MemoryBlock();

    juce::File tempFile = juce::File::createTempFile ("flac");
    std::unique_ptr<juce::FileOutputStream> fos (tempFile.createOutputStream());
    std::unique_ptr<juce::AudioFormatWriter> writer (format->createWriterFor (
        std::move (fos),
        (float) sampleRate,
        (unsigned int) buffer.getNumChannels(),
        16,   // bit depth
        {},   // metadata options
        0));  // quality index

    if (writer == nullptr)
        return juce::MemoryBlock();

    writer->writeFromAudioBuffer (buffer, 0, buffer.getNumSamples());

    juce::MemoryBlock result;
    tempFile.loadFileIntoMemory (result);
    tempFile.deleteFile();
    return result;
}

//==============================================================================
std::unique_ptr<LoadedSample> SampleLoader::decodeFlac (const juce::MemoryBlock& flacData,
                                                        double targetSampleRate)
{
    juce::MemoryInputStream stream (flacData, false);

    auto* format = formatManager().findFormatForFileExtension (".flac");
    if (format == nullptr)
        return nullptr;

    std::unique_ptr<juce::AudioFormatReader> reader (format->createReaderFor (&stream,
                                                                                /*sampleRateToUse=*/ 0,
                                                                                /*channelsToUse=*/ 0,
                                                                                /*useIntegerData=*/ false,
                                                                                /*metadataOnly=*/ false));

    if (reader == nullptr)
        return nullptr;

    auto result = std::make_unique<LoadedSample>();
    result->sourceSampleRate = reader->sampleRate;

    juce::AudioBuffer<float> fileBuffer ((int) reader->numChannels,
                                         (int) reader->lengthInSamples);
    reader->read (&fileBuffer,
                  0,
                  (int) reader->lengthInSamples,
                  0,
                  true,
                  true);

    // Resample if needed (same simple linear interpolation as loadFile).
    if (std::abs (reader->sampleRate - targetSampleRate) > 0.1)
    {
        juce::AudioBuffer<float> resampled ((int) reader->numChannels,
                                            (int) (reader->lengthInSamples * targetSampleRate / reader->sampleRate));

        for (int ch = 0; ch < (int) reader->numChannels; ++ch)
        {
            const float* src = fileBuffer.getReadPointer (ch);
            float* dst = resampled.getWritePointer (ch);

            const double ratio = reader->sampleRate / targetSampleRate;
            for (int i = 0; i < resampled.getNumSamples(); ++i)
            {
                const double srcPos = i * ratio;
                const int srcIdx = (int) srcPos;
                const float frac = (float) (srcPos - srcIdx);

                if (srcIdx + 1 < fileBuffer.getNumSamples())
                    dst[i] = src[srcIdx] * (1.0f - frac) + src[srcIdx + 1] * frac;
                else
                    dst[i] = src[srcIdx];
            }
        }
        result->buffer = std::move (resampled);
    }
    else
    {
        result->buffer = std::move (fileBuffer);
    }

    result->flacData = flacData;
    return result;
}

//==============================================================================
bool SampleLoader::exceedsEmbedLimit (const juce::File& file, size_t maxEmbedBytes)
{
    if (! file.existsAsFile())
        return true;

    return (size_t) file.getSize() > maxEmbedBytes;
}
