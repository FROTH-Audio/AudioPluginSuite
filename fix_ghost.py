with open('plugins/Sgraffito/Source/PluginProcessor.cpp', 'r') as f:
    content = f.read()

# Find and replace the voice processing section
# We need to:
# 1. Add ghost note ghost note ghost note processing after the voice processing loop
# 2. Initialize ghost notes array in constructor (already done in header)

old_block = '''        // Apply tape-stop rate reduction.
        float rate = voice.rate;
        float rateFactor = 1.0f;
        if (voice.tapeStopping)
            rateFactor = voice.tapeStop.tick (preparedSampleRate);

        // Apply chaos drift: chaos * driftAmount cents, converted to rate multiplier.
        const float chaos = lorenz.normalisedX();
        const float driftCents = driftAmountPercent * chaos;
        const float driftRatio = std::pow (2.0f, driftCents / 1200.0f);
        rate *= driftRatio * rateFactor;

        // Process each sample in the block.
        for (int s = 0; s < numSamples; ++s)
        {
            const double sampleIndex = blockStartSample + static_cast<double> (s);

            // Envelope value.
            const float env = voice.envelope (sampleIndex, attackMs, releaseMs, preparedSampleRate) * rateFactor;

            // Read sample with linear interpolation.
            const double pos = voice.position;
            const int idx0 = (int) std::floor (pos);
            const int idx1 = idx0 + 1;
            const float frac = static_cast<float> (pos - idx0);

            float sampleValue = 0.0f;
            if (idx0 >= 0 && idx1 < sample->getNumSamples())
            {
                for (int ch = 0; ch < numChannels; ++ch)
                {
                    const float* src = sample->getReadPointer (ch);
                    const float s0 = src[std::clamp (idx0, 0, sample->getNumSamples() - 1)];
                    const float s1 = src[std::clamp (idx1, 0, sample->getNumSamples() - 1)];
                    const float v = s0 * (1.0f - frac) + s1 * frac;

                    buffer.addSample (ch, i, v * voice.gain * voice.velocity * level * env);
                }
            }

            // Advance position.
            voice.position += rate;

            // Release voice if past region end.
            if (voice.position >= voice.regionEnd * sample->getNumSamples())
            {
                voice.active = false;
                break;
            }

            // Release voice if tape stop complete.
            if (voice.tapeStopping && rateFactor < 0.001f)
            {
                voice.active = false;
                voice.tapeStopping = false;
                break;
            }
        }
    }
}'''

new_block = '''        // Apply tape-stop rate reduction.
        float rate = voice.rate;
        float rateFactor = 1.0f;
        if (voice.tapeStopping)
            rateFactor = voice.tapeStop.tick (preparedSampleRate);

        // Apply chaos drift: chaos * driftAmount cents, converted to rate multiplier.
        const float chaos = lorenz.normalisedX();
        const float driftCents = driftAmountPercent * chaos;
        const float driftRatio = std::pow (2.0f, driftCents / 1200.0f);
        rate *= driftRatio * rateFactor;

        // Process each sample in the block.
        for (int s = 0; s < numSamples; ++s)
        {
            const double sampleIndex = blockStartSample + static_cast<double> (s);

            // Envelope value.
            const float env = voice.envelope (sampleIndex, attackMs, releaseMs, preparedSampleRate) * rateFactor;

            // Read sample with linear interpolation.
            const double pos = voice.position;
            const int idx0 = (int) std::floor (pos);
            const int idx1 = idx0 + 1;
            const float frac = static_cast<float> (pos - idx0);

            float sampleValue = 0.0f;
            if (idx0 >= 0 && idx1 < sample->getNumSamples())
            {
                for (int ch = 0; ch < numChannels; ++ch)
                {
                    const float* src = sample->getReadPointer (ch);
                    const float s0 = src[std::clamp (idx0, 0, sample->getNumSamples() - 1)];
                    const float s1 = src[std::clamp (idx1, 0, sample->getNumSamples() - 1)];
                    const float v = s0 * (1.0f - frac) + s1 * frac;

                    buffer.addSample (ch, i, v * voice.gain * voice.velocity * level * env);
                }
            }

            // Advance position.
            voice.position += rate;

            // Release voice if past region end.
            if (voice.position >= voice.regionEnd * sample->getNumSamples())
            {
                voice.active = false;
                break;
            }

            // Release voice if tape stop complete.
            if (voice.tapeStopping && rateFactor < 0.001f)
            {
                voice.active = false;
                voice.tapeStopping = false;
                break;
            }
        }
    }

    // Process ghost notes: advance delay remaining and trigger ghost notes when delay elapses.
    for (auto& ghost : ghostNotes)
    {
        if (! ghost.active)
            continue;

        // Count down delay remaining in samples.
        ghost.delayRemaining -= 1.0;

        // Trigger ghost note when delay has elapsed.
        if (ghost.delayRemaining <= 0.0)
        {
            ghost.delayRemaining = 0.0;

            // Find a free voice for the ghost note.
            for (int i = 0; i < CoreDSP::Sampler::maxVoices; ++i)
            {
                if (! voicePool.voices[i].active)
                {
                    voices[i] = Voice {};
                    voices[i].active = true;
                    voices[i].key = ghost.key;
                    voices[i].velocity = ghost.velocity;
                    voices[i].regionStart = ghost.regionStart;
                    voices[i].regionEnd = ghost.regionEnd;
                    voices[i].position = ghost.position;
                    voices[i].envStart = blockStartSample;
                    break;
                }
            }

            ghost.active = false;
        }
    }
}'''

if old_block in content:
    content = content.replace(old_block, new_block, 1)
    with open('plugins/Sgraffito/Source/PluginProcessor.cpp', 'w') as f:
        f.write(content)
    print('Replacement successful')
else:
    print('Old block not found')
    # Debug: find where the differences are
    import re
    lines = content.split('\n')
    for i in range(293, 360):
        if i < len(lines):
            print(f'{i+1}: {repr(lines[i])}')
