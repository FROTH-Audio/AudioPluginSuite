#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>

#include <array>
#include <atomic>
#include <deque>
#include <memory>

#include "LoopEngine.h"

/**
    One Cyclical slot's loaded loop (plan 6.1).

    The audio thread only ever loads the atomic pointer. Every install retires the previous
    loop into a capped two-generation queue (the plan 09 1.1 / Sgraffito pattern): an audio
    block that started before a swap can still hold the old buffer pointer, and two installs
    inside one block must not free it. The queue lives on the message thread only.
*/
struct LoadedLoop
{
    juce::AudioBuffer<float> buffer;       // as decoded, at its own sample rate
    juce::String sourcePath;               // file path, or "embedded:<file name>"
    double sourceSampleRate = 0.0;

    // One read pointer per source channel, built once at load, so a playing voice's Source
    // keeps pointing at a stable array even after the slot installs a new loop.
    std::array<const float*, 2> channelPointers {};

    /** The non-owning view the voices capture at start(). */
    CoreDSP::Loop::Source source() const
    {
        CoreDSP::Loop::Source view;
        view.channels = channelPointers.data();
        view.numChannels = buffer.getNumChannels();
        view.numFrames = buffer.getNumSamples();
        view.sourceSampleRate = sourceSampleRate;
        return view;
    }
};

/** Builds a LoadedLoop around a decoded buffer, filling the channel pointer table. */
inline std::unique_ptr<LoadedLoop> makeLoadedLoop (juce::AudioBuffer<float> buffer,
                                                   double sourceSampleRate, const juce::String& path)
{
    auto loop = std::make_unique<LoadedLoop>();
    loop->buffer = std::move (buffer);
    loop->sourcePath = path;
    loop->sourceSampleRate = sourceSampleRate;

    const int channels = std::min (2, loop->buffer.getNumChannels());
    for (int ch = 0; ch < channels; ++ch)
        loop->channelPointers[(size_t) ch] = loop->buffer.getReadPointer (ch);

    // A mono loop reads channel 0 for every output channel (the Sgraffito rule); a slot
    // never sees more than two source channels.
    for (int ch = channels; ch < 2; ++ch)
        loop->channelPointers[(size_t) ch] = loop->channelPointers[(size_t) 0];

    return loop;
}

class LoopSlot
{
public:
    /** The loop the audio thread plays right now (may be null: nothing loaded yet). */
    const LoadedLoop* current() const noexcept { return loop.load (std::memory_order_acquire); }

    /** Message thread. Installs the new loop, retiring the previous one into the capped
        two-generation queue so a running block's pointer stays valid. */
    void install (std::unique_ptr<LoadedLoop> next)
    {
        if (owned != nullptr)
        {
            retired.push_back (std::move (owned));

            if (retired.size() > 2)
                retired.pop_front();
        }

        owned = std::move (next);
        loop.store (owned.get(), std::memory_order_release);
    }

private:
    // The audio thread reads this; the message thread writes it (release/acquire).
    std::atomic<const LoadedLoop*> loop { nullptr };

    // The message thread owns the current loop and the retired generations.
    std::unique_ptr<LoadedLoop> owned;
    std::deque<std::unique_ptr<LoadedLoop>> retired;
};
