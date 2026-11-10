with open('plugins/Sgraffito/Source/PluginProcessor.cpp', 'r') as f:
    content = f.read()

# Find start and end of processBlock function
start_marker = 'void SgraffitoProcessor::processBlock'
end_marker = 'void SgraffitoProcessor::applyPresetState'

start_idx = content.find(start_marker)
end_idx = content.find(end_marker)

if start_idx == -1:
    print("Could not find start_marker")
    exit(1)
if end_idx == -1:
    print("Could not find end_marker")
    exit(1)

print(f"start_idx={start_idx}, end_idx={end_idx}")
print(f"Found start at {start_idx}")

# Find the closing brace of processBlock
brace_count = 0
in_func = False
func_end = start_idx
for i in range(start_idx, end_idx):
    if content[i] == '{':
        brace_count += 1
    elif content[i] == '}':
        brace_count -= 1
        if brace_count == 0:
            func_end = i + 1
            break

print(f"func_end={func_end}")

# Now extract the current function body between start_idx and end_idx
# Let's just rewrite the whole thing

new_func = '''void SgraffitoProcessor::processBlock (juce::AudioBuffer<float>& buffer,
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

            // Ghost note: if ghost on and velocity below threshold, trigger ghost.
            if (ghostOnParam->load() > 0.5f && velocity < ghostThresholdParam->load())
            {
                const float ghostVel = velocity * ghostLevelParam->load();
                const double ghostDelay = ghostDelayParam->load() * preparedSampleRate / 1000.0;

                for (int i = 0; i < CoreDSP::Sampler::maxGhostVoices; ++i)
                {
                    if (! ghostNotes[i].active)
                    {
                        ghostNotes[i].active = true;
                        ghostNotes[i].key = key;
                        ghostNotes[i].velocity = ghostVel;
                        ghostNotes[i].regionStart = voicePool.voices[i].regionStart;
                        ghostNotes[i].regionEnd = voicePool.voices[i].regionEnd;
                        ghostNotes[i].position = voicePool.voices[i].position;
                        ghostNotes[i].delayRemaining = ghostDelay;
                        break;
                    }
                }
            }
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
    }

    // Process ghost notes: advance delay remaining and trigger ghost notes when delay elapses.
    for (auto& ghost : ghostNotes)
    {
        if (! ghost.active)
            continue;

        // Count down delay remaining.
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
                    voicePool.voices[i] = Voice {};
                    voicePool.voices[i].active = true;
                    voicePool.voices[i].key = ghost.key;
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

new_content = content[:start_idx] + new_processBlock + content[end_idx:]

with open('plugins/Sgraffito/Source/PluginProcessor.cpp', 'w') as f:
    f.write(new_content)

print('Replacement successful')