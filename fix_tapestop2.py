with open('plugins/Sgraffito/Source/PluginProcessor.cpp', 'r') as f:
    content = f.read()

# Find the processBlock function and replace it
start_marker = 'void SgraffitoProcessor::processBlock'
end_marker = 'void SgraffitoProcessor::applyPresetState'

start_idx = content.find(start_marker)
end_idx = content.find(end_marker)

if start_idx == -1 or end_idx == -1:
    print('Could not find function boundaries')
    exit(1)

# Find the end of processBlock (the closing brace)
# Find the last } before end_marker
brace_count = 0
func_end = start_idx
for i in range(start_idx, end_idx):
    if content[i] == '{':
        brace_count += 1
    elif content[i] == '}':
        brace_count -= 1
        if brace_count == 0:
            func_end = i + 1
            break

new_processBlock = '''void SgraffitoProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                       juce::MidiBuffer& midiMessages)
{
    const int numSamples = buffer.getNumSamples();
    juce::ignoreUnused (numSamples);

    // Clear the output.
    buffer.clear();

    // Process MIDI messages first: note-on allocates a voice, note-off releases it.
    // This must happen even without a sample loaded, so voices are tracked correctly.
    for (const auto metadata : midiMessages)
    {
        const auto msg = metadata.getMessage();

        if (msg.isNoteOn())
        {
            const int key = msg.getNoteNumber();
            const float velocity = msg.getVelocity() / 127.0f;
            voicePool.allocateVoice (key, velocity, regionSpanParam->load(), regionLengthParam->load(),
                                     modeParam->load(), sliceCountParam->load());
        }
        else if (msg.isNoteOff())
        {
            const int key = msg.getNoteNumber();
            for (int i = 0; i < CoreDSP::Sampler::maxVoices; ++i)
            {
                if (voicePool.voices[i].active && voicePool.voices[i].key == key)
                {
                    voicePool.releaseVoice (i, metadata.samplePosition, tapeStopTimeParam->load(), tapeStopCurveParam->load());
                    break;
                }
            }
        }
    }

    midiMessages.clear();

    // Get the current sample buffer (atomic read).
    const auto* sample = getSampleBuffer();
    if (sample == nullptr || sample->getNumSamples() == 0)
        return;

    const int numChannels = buffer.getNumChannels();
    const float level = juce::Decibels::decibelsToGain (levelParam->load());
    const float attackMs = attackParam->load();
    const float releaseMs = releaseParam->load();
    const double blockStartSample = static_cast<double> (buffer.getNumSamples()) - static_cast<double> (numSamples);

    // Advance Lorenz attractor by driftSpeed * seconds in this block.
    const float driftSpeed = driftSpeedParam->load();
    const double advanceSeconds = static_cast<double> (numSamples) / preparedSampleRate;
    lorenz.advance (driftSpeed * advanceSeconds);

    const float driftAmountPercent = driftAmountParam->load() / 100.0f;

    for (int i = 0; i < CoreDSP::Sampler::maxVoices; ++i)
    {
        auto& voice = voicePool.voices[i];
        if (! voice.active)
            continue;

        // Initialize envStart on first sample.
        if (voice.envStart < 0.0)
        {
            voice.envStart = blockStartSample;
            voice.position = voice.regionStart * sample->getNumSamples();
        }

        // Apply tape-stop rate reduction.
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

        // Release voice if tape stop complete after full block.
        if (voice.tapeStopping && ! voice.tapeStop.active())
        {
            voice.active = false;
            voice.tapeStopping = false;
        }
    }
}

'''

new_content = content[:start_idx] + new_processBlock + content[end_idx:]

with open('plugins/Sgraffito/Source/PluginProcessor.cpp', 'w') as f:
    f.write(new_content)

print('Done')