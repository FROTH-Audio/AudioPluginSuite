with open('plugins/Sgraffito/Source/PluginProcessor.cpp', 'r') as f:
    content = f.read()

old = '''        // Apply tape-stop rate reduction.
        float rate = voice.rate;
        if (voice.tapeStopping)
            rate *= voice.tapeStop.tick (preparedSampleRate);

        // Apply chaos drift: chaos * driftAmount cents, converted to rate multiplier.
        const float chaos = lorenz.normalisedX();
        const float driftCents = driftAmountPercent * chaos;
        const float driftRatio = std::pow (2.0f, driftCents / 1200.0f);
        rate *= driftRatio;

        // Process each sample in the block.
        for (int s = 0; s < numSamples; ++s)
        {
            const double sampleIndex = blockStartSample + static_cast<double> (s);

            // Envelope value.
            const float env = voice.envelope (sampleIndex, attackMs, releaseMs, preparedSampleRate);

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
        }
    }'''

new = '''        // Apply tape-stop rate reduction.
        float rate = voice.rate;
        float rateFactor = 1.0f;
        if (voice.tapeStopping)
            rateFactor = voice.tapeStop.tick (preparedSampleRate);
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

        // Release voice if tape stop complete after full block.
        if (voice.tapeStopping && ! voice.tapeStop.active())
        {
            voice.active = false;
            voice.tapeStopping = false;
        }
    }'''

if old in content:
    content = content.replace(old, new, 1)
    with open('plugins/Sgraffito/Source/PluginProcessor.cpp', 'w') as f:
        f.write(content)
    print('Replacement successful')
else:
    print('Old string not found')
    # Debug: find what's around line 300
    lines = content.split('\n')
    for i in range(299, 350):
        if i < len(lines):
            print(f'{i+1}: {repr(lines[i])}')
