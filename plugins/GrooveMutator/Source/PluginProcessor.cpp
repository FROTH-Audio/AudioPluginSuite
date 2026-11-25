#include "PluginProcessor.h"
#include "PluginEditor.h"

#if SUITE_PRESET_BANK_AS_PROGRAMS
 #include "PresetBank.h"
#endif

#include <algorithm>
#include <cmath>
#include <cstdlib>

//==============================================================================
namespace
{
    // The routing targets, in ModTarget order (append-only; the choice index is the
    // position + 1): six master controls, then five controls for each lane.
    const SuiteModulation::TargetInfo modulationTargets[] = {
        { GrooveMutatorProcessor::chaosId,            "Chaos" },
        { GrooveMutatorProcessor::mutateScaleId,      "Mutation Scale" },
        { GrooveMutatorProcessor::fillProbId,         "Fill Probability" },
        { GrooveMutatorProcessor::swingId,            "Swing" },
        { GrooveMutatorProcessor::humaniseTimeId,     "Humanise Time" },
        { GrooveMutatorProcessor::humaniseVelocityId, "Humanise Velocity" },
        { GrooveMutatorProcessor::trackVelIds[0],     "Track 1 Velocity" },
        { GrooveMutatorProcessor::trackVelIds[1],     "Track 2 Velocity" },
        { GrooveMutatorProcessor::trackVelIds[2],     "Track 3 Velocity" },
        { GrooveMutatorProcessor::trackVelIds[3],     "Track 4 Velocity" },
        { GrooveMutatorProcessor::trackVelIds[4],     "Track 5 Velocity" },
        { GrooveMutatorProcessor::trackVelIds[5],     "Track 6 Velocity" },
        { GrooveMutatorProcessor::trackVelIds[6],     "Track 7 Velocity" },
        { GrooveMutatorProcessor::trackVelIds[7],     "Track 8 Velocity" },
        { GrooveMutatorProcessor::trackGainIds[0],    "Track 1 Level" },
        { GrooveMutatorProcessor::trackGainIds[1],    "Track 2 Level" },
        { GrooveMutatorProcessor::trackGainIds[2],    "Track 3 Level" },
        { GrooveMutatorProcessor::trackGainIds[3],    "Track 4 Level" },
        { GrooveMutatorProcessor::trackGainIds[4],    "Track 5 Level" },
        { GrooveMutatorProcessor::trackGainIds[5],    "Track 6 Level" },
        { GrooveMutatorProcessor::trackGainIds[6],    "Track 7 Level" },
        { GrooveMutatorProcessor::trackGainIds[7],    "Track 8 Level" },
        { GrooveMutatorProcessor::trackAccentIds[0],  "Track 1 Accent" },
        { GrooveMutatorProcessor::trackAccentIds[1],  "Track 2 Accent" },
        { GrooveMutatorProcessor::trackAccentIds[2],  "Track 3 Accent" },
        { GrooveMutatorProcessor::trackAccentIds[3],  "Track 4 Accent" },
        { GrooveMutatorProcessor::trackAccentIds[4],  "Track 5 Accent" },
        { GrooveMutatorProcessor::trackAccentIds[5],  "Track 6 Accent" },
        { GrooveMutatorProcessor::trackAccentIds[6],  "Track 7 Accent" },
        { GrooveMutatorProcessor::trackAccentIds[7],  "Track 8 Accent" },
        { GrooveMutatorProcessor::trackFlamIds[0],    "Track 1 Flam" },
        { GrooveMutatorProcessor::trackFlamIds[1],    "Track 2 Flam" },
        { GrooveMutatorProcessor::trackFlamIds[2],    "Track 3 Flam" },
        { GrooveMutatorProcessor::trackFlamIds[3],    "Track 4 Flam" },
        { GrooveMutatorProcessor::trackFlamIds[4],    "Track 5 Flam" },
        { GrooveMutatorProcessor::trackFlamIds[5],    "Track 6 Flam" },
        { GrooveMutatorProcessor::trackFlamIds[6],    "Track 7 Flam" },
        { GrooveMutatorProcessor::trackFlamIds[7],    "Track 8 Flam" },
        { GrooveMutatorProcessor::trackGhostIds[0],   "Track 1 Ghost" },
        { GrooveMutatorProcessor::trackGhostIds[1],   "Track 2 Ghost" },
        { GrooveMutatorProcessor::trackGhostIds[2],   "Track 3 Ghost" },
        { GrooveMutatorProcessor::trackGhostIds[3],   "Track 4 Ghost" },
        { GrooveMutatorProcessor::trackGhostIds[4],   "Track 5 Ghost" },
        { GrooveMutatorProcessor::trackGhostIds[5],   "Track 6 Ghost" },
        { GrooveMutatorProcessor::trackGhostIds[6],   "Track 7 Ghost" },
        { GrooveMutatorProcessor::trackGhostIds[7],   "Track 8 Ghost" },
    };
    constexpr int numModulationTargets = (int) std::size (modulationTargets);
}

juce::StringArray GrooveMutatorProcessor::modTargetNames()
{
    // Must follow the ModTarget enum order exactly.
    return { "None", "Chaos", "Mutation Scale", "Fill Probability", "Swing",
             "Humanise Time", "Humanise Velocity",
             "Track 1 Velocity", "Track 2 Velocity", "Track 3 Velocity", "Track 4 Velocity",
             "Track 5 Velocity", "Track 6 Velocity", "Track 7 Velocity", "Track 8 Velocity",
             "Track 1 Level", "Track 2 Level", "Track 3 Level", "Track 4 Level",
             "Track 5 Level", "Track 6 Level", "Track 7 Level", "Track 8 Level",
             "Track 1 Accent", "Track 2 Accent", "Track 3 Accent", "Track 4 Accent",
             "Track 5 Accent", "Track 6 Accent", "Track 7 Accent", "Track 8 Accent",
             "Track 1 Flam", "Track 2 Flam", "Track 3 Flam", "Track 4 Flam",
             "Track 5 Flam", "Track 6 Flam", "Track 7 Flam", "Track 8 Flam",
             "Track 1 Ghost", "Track 2 Ghost", "Track 3 Ghost", "Track 4 Ghost",
             "Track 5 Ghost", "Track 6 Ghost", "Track 7 Ghost", "Track 8 Ghost" };
}

float GrooveMutatorProcessor::getModulatedValue (ModTarget target) const
{
    return modulation.getModulatedValue ((int) target);
}

float GrooveMutatorProcessor::trackVelocityFor (int track, int patternIndex) const
{
    const auto i = (size_t) std::clamp (track, 0, numTracks - 1);
    const int steps = juce::jlimit (1, CoreDSP::Rhythm::maxSteps,
                                    juce::roundToInt (stepsParams[i]->load()));
    const float t = (float) (patternIndex % steps) / (float) steps;
    const float r = CoreDSP::Sequencer::hashUnit ((std::uint32_t) juce::roundToInt (seed->load()),
                                                  (std::uint32_t) (9001 + patternIndex));

    float mul = 1.0f;
    switch (juce::jlimit (0, 3, juce::roundToInt (velCurve->load())))
    {
        case 1: mul = 0.6f + 0.4f * t; break;                       // Linear
        case 2: mul = 0.4f + 0.6f * t * t; break;                   // Exponential
        case 3: mul = 0.75f + 0.5f * r; break;                      // Random
        default: break;                                             // Flat
    }

    // The accent and the ghost are shares of the cycle: an accent hit plays louder, a
    // ghost quieter, and the two are exclusive (the accent wins).
    const auto targetForTrack = [trackIndex = (int) i] (ModTarget first)
    {
        return static_cast<ModTarget> (static_cast<int> (first) + trackIndex);
    };
    const float accentShare = juce::jlimit (0.0f, 100.0f,
                                             getModulatedValue (targetForTrack (ModTarget::TrackAccent1))) / 100.0f;
    const float ghostShare = juce::jlimit (0.0f, 100.0f,
                                            getModulatedValue (targetForTrack (ModTarget::TrackGhost1))) / 100.0f;
    if (r < accentShare)
        mul *= 1.3f;
    else if (r > 1.0f - ghostShare)
        mul *= 0.4f;

    const float velocity = juce::jlimit (30.0f, 150.0f,
                                          getModulatedValue (targetForTrack (ModTarget::TrackVel1)));
    const float level = juce::jlimit (-24.0f, 12.0f,
                                      getModulatedValue (targetForTrack (ModTarget::TrackLevel1)));
    mul *= velocity / 100.0f;
    mul *= juce::Decibels::decibelsToGain (level);

    return mul;
}

std::uint32_t GrooveMutatorProcessor::laneMask (int track) const noexcept
{
    return laneStates[(size_t) std::clamp (track, 0, numTracks - 1)].mask;
}

std::uint32_t GrooveMutatorProcessor::getLaneStepsMask (int track) const noexcept
{
    return laneStepsMask[(size_t) std::clamp (track, 0, numTracks - 1)].load (std::memory_order_relaxed);
}

std::uint32_t GrooveMutatorProcessor::laneFillMask (int track) const noexcept
{
    // The same message-thread-reads-audio-state precedent as laneMask(): a stale cycle's
    // additions cost nothing (they are recomputed on the next scheduled step).
    return laneFillAdditions[(size_t) std::clamp (track, 0, numTracks - 1)];
}

void GrooveMutatorProcessor::setLaneStep (int track, int step, bool on)
{
    const auto i = (size_t) std::clamp (track, 0, numTracks - 1);
    const int steps = juce::jlimit (1, CoreDSP::Rhythm::maxSteps,
                                    juce::roundToInt (stepsParams[i]->load()));
    if (step < 0 || step >= steps)
        return;

    const std::uint32_t bit = 1u << step;
    const std::uint32_t updated = on ? (laneStepsMask[i].load (std::memory_order_relaxed) | bit)
                                     : (laneStepsMask[i].load (std::memory_order_relaxed) & ~bit);
    laneStepsMask[i].store (updated, std::memory_order_relaxed);

    storeStepsMasksToState();
    // The next rederive catches the change through the signature (the steps mask is part of it).
}

bool GrooveMutatorProcessor::laneFlamsAt (int track, int patternIndex) const
{
    const auto i = (size_t) std::clamp (track, 0, numTracks - 1);
    const auto target = static_cast<ModTarget> (static_cast<int> (ModTarget::TrackFlam1) + (int) i);
    const float flamShare = juce::jlimit (0.0f, 100.0f, getModulatedValue (target)) / 100.0f;
    return flamShare > 0.0f
        && CoreDSP::Sequencer::hashUnit ((std::uint32_t) juce::roundToInt (seed->load()),
                                         (std::uint32_t) (7003 + patternIndex)) < flamShare;
}

bool GrooveMutatorProcessor::laneAccentsAt (int track, int patternIndex) const
{
    const auto i = (size_t) std::clamp (track, 0, numTracks - 1);
    const auto target = static_cast<ModTarget> (static_cast<int> (ModTarget::TrackAccent1) + (int) i);
    const float r = CoreDSP::Sequencer::hashUnit ((std::uint32_t) juce::roundToInt (seed->load()),
                                                  (std::uint32_t) (9001 + patternIndex));
    return r < juce::jlimit (0.0f, 100.0f, getModulatedValue (target)) / 100.0f;
}

bool GrooveMutatorProcessor::laneGhostsAt (int track, int patternIndex) const
{
    const auto i = (size_t) std::clamp (track, 0, numTracks - 1);
    const auto accentTarget = static_cast<ModTarget> (static_cast<int> (ModTarget::TrackAccent1) + (int) i);
    const auto ghostTarget = static_cast<ModTarget> (static_cast<int> (ModTarget::TrackGhost1) + (int) i);
    const float r = CoreDSP::Sequencer::hashUnit ((std::uint32_t) juce::roundToInt (seed->load()),
                                                  (std::uint32_t) (9001 + patternIndex));
    const float accentShare = juce::jlimit (0.0f, 100.0f, getModulatedValue (accentTarget)) / 100.0f;
    const float ghostShare = juce::jlimit (0.0f, 100.0f, getModulatedValue (ghostTarget)) / 100.0f;
    return r >= accentShare && r > 1.0f - ghostShare;
}

void GrooveMutatorProcessor::advanceAttractorTo (double beat) noexcept
{
    const double delta = beat - attractorBeat;

    if (delta > 0.0)
    {
        double time = delta;

        while (time > 0.0)
        {
            const double chunk = std::min (time, CoreDSP::Chaos::LorenzAttractor::maxAdvance);
            attractor.advance (chunk);
            time -= chunk;
        }
    }

    attractorBeat = beat;
}

void GrooveMutatorProcessor::publishTrailPoint() noexcept
{
    const int slot = trail.head.load (std::memory_order_relaxed);

    trail.x[(size_t) slot].store (attractor.normalisedX(), std::memory_order_relaxed);
    trail.y[(size_t) slot].store (attractor.normalisedY(), std::memory_order_relaxed);
    trail.head.store ((slot + 1) % trailLength, std::memory_order_release);
}

std::vector<juce::Point<float>> GrooveMutatorProcessor::getTrail() const
{
    std::vector<juce::Point<float>> points;
    points.reserve (trailLength);

    const int head = trail.head.load (std::memory_order_acquire);

    for (int i = 0; i < trailLength; ++i)
    {
        const auto slot = (size_t) ((head + i) % trailLength);
        points.emplace_back (trail.x[slot].load (std::memory_order_relaxed),
                             trail.y[slot].load (std::memory_order_relaxed));
    }

    return points;
}

void GrooveMutatorProcessor::loadStepsMasksFromState()
{
    const auto text = apvts.state.getProperty (stepsMasksProperty).toString();
    if (text.isEmpty())
        return;

    auto tokens = juce::StringArray::fromTokens (text, ";", "");
    for (int track = 0; track < numTracks; ++track)
    {
        if (track >= tokens.size())
            break;
        const auto token = tokens[(size_t) track].trim();
        if (token.isEmpty())
            continue;
        laneStepsMask[(size_t) track].store ((std::uint32_t) std::strtoul (token.toRawUTF8(),
                                                                           nullptr, 16),
                                             std::memory_order_relaxed);
    }
}

void GrooveMutatorProcessor::storeStepsMasksToState()
{
    juce::String text;
    for (int track = 0; track < numTracks; ++track)
    {
        if (track > 0)
            text << ";";
        text << juce::String::toHexString ((int) laneStepsMask[(size_t) track].load (std::memory_order_relaxed));
    }
    apvts.state.setProperty (stepsMasksProperty, text, nullptr);
}

//==============================================================================
GrooveMutatorProcessor::GrooveMutatorProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout()),
      modulation (apvts, modulationTargets, numModulationTargets, nullptr)
{
    sync            = apvts.getRawParameterValue (syncId);
    tempo           = apvts.getRawParameterValue (tempoId);
    triggerMode     = apvts.getRawParameterValue (triggerModeId);
    triggerNote     = apvts.getRawParameterValue (triggerNoteId);
    swing           = apvts.getRawParameterValue (swingId);
    humaniseTime    = apvts.getRawParameterValue (humaniseTimeId);
    humaniseVelocity = apvts.getRawParameterValue (humaniseVelocityId);
    velCurve        = apvts.getRawParameterValue (velCurveId);
    polyRatio       = apvts.getRawParameterValue (polyRatioId);
    chaos           = apvts.getRawParameterValue (chaosId);
    evolveRate      = apvts.getRawParameterValue (evolveRateId);
    mutateScale     = apvts.getRawParameterValue (mutateScaleId);
    freeze          = apvts.getRawParameterValue (freezeId);
    fillProb        = apvts.getRawParameterValue (fillProbId);
    fillStyle       = apvts.getRawParameterValue (fillStyleId);
    fillBars        = apvts.getRawParameterValue (fillBarsId);
    seed            = apvts.getRawParameterValue (seedId);

    for (int track = 0; track < numTracks; ++track)
    {
        const auto i = (size_t) track;
        onParams[i]       = apvts.getRawParameterValue (trackOnIds[i]);
        patternParams[i]  = apvts.getRawParameterValue (trackPatternIds[i]);
        stepsParams[i]    = apvts.getRawParameterValue (trackStepsIds[i]);
        pulsesParams[i]   = apvts.getRawParameterValue (trackPulsesIds[i]);
        rotationParams[i] = apvts.getRawParameterValue (trackRotationIds[i]);
        divisionParams[i] = apvts.getRawParameterValue (trackDivisionIds[i]);
        noteParams[i]     = apvts.getRawParameterValue (trackNoteIds[i]);
        velParams[i]      = apvts.getRawParameterValue (trackVelIds[i]);
        accentParams[i]   = apvts.getRawParameterValue (trackAccentIds[i]);
        flamParams[i]     = apvts.getRawParameterValue (trackFlamIds[i]);
        ghostParams[i]    = apvts.getRawParameterValue (trackGhostIds[i]);
        gainParams[i]     = apvts.getRawParameterValue (trackGainIds[i]);
    }

    for (int track = 0; track < numTracks; ++track)
    {
        laneParams[(size_t) track].note = defaultTrackNotes[track];
        rederiveLaneIfNeeded (track);
        playheadStep[(size_t) track].store (-1, std::memory_order_relaxed);
    }

    loadStepsMasksFromState();
    lastTriggerMode = juce::roundToInt (triggerMode->load());
}

juce::AudioProcessorValueTreeState::ParameterLayout GrooveMutatorProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Master block, in §1's order (never reorder after first commit).
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { syncId, 1 }, "Host Sync", true));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { tempoId, 1 }, "Tempo",
        juce::NormalisableRange<float> (40.0f, 240.0f, 0.1f), 120.0f,
        juce::AudioParameterFloatAttributes().withLabel ("BPM")));
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { triggerModeId, 1 }, "Trigger Mode",
        juce::StringArray { "Latch", "Gate" }, 0));
    params.push_back (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { triggerNoteId, 1 }, "Trigger Key",
        24, 96, 36,
        juce::AudioParameterIntAttributes().withStringFromValueFunction (
            [] (int value, int) { return juce::MidiMessage::getMidiNoteName (value, true, true, 4)
                                      + " (" + juce::String (value) + ")"; })));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { swingId, 1 }, "Swing",
        juce::NormalisableRange<float> (-50.0f, 50.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("%")));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { humaniseTimeId, 1 }, "Humanise Time",
        juce::NormalisableRange<float> (0.0f, 20.0f, 0.1f), 2.0f,
        juce::AudioParameterFloatAttributes().withLabel ("ms")));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { humaniseVelocityId, 1 }, "Humanise Velocity",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 10.0f,
        juce::AudioParameterFloatAttributes().withLabel ("%")));
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { velCurveId, 1 }, "Velocity Curve",
        juce::StringArray { "Flat", "Linear", "Exponential", "Random" }, 0));
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { polyRatioId, 1 }, "Polyrhythm",
        juce::StringArray { "1:1", "3:4", "5:4", "7:8", "4:3", "8:7" }, 0));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { chaosId, 1 }, "Chaos",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 25.0f,
        juce::AudioParameterFloatAttributes().withLabel ("%")));
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { evolveRateId, 1 }, "Evolve Rate",
        juce::StringArray { "per Beat", "per Bar", "2 Bars", "4 Bars", "8 Bars" }, 1));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { mutateScaleId, 1 }, "Mutation Scale",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 50.0f,
        juce::AudioParameterFloatAttributes().withLabel ("%")));
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { freezeId, 1 }, "Freeze", false));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { fillProbId, 1 }, "Fill Probability",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 15.0f,
        juce::AudioParameterFloatAttributes().withLabel ("%")));
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { fillStyleId, 1 }, "Fill Style",
        juce::StringArray { "Tom Roll", "Snare Build", "Hat Drop", "Chaos Burst" }, 0));
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { fillBarsId, 1 }, "Fill Length",
        juce::StringArray { "1 Bar", "2 Bars", "4 Bars" }, 0));
    params.push_back (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { seedId, 1 }, "Seed", 0, 999, 42));

    // Per track ×8, in the §1 order. The defaults walk the GM drum map and keep the INIT
    // groove musical (the kick's four-on-floor, the snare's backbeat, and so on).
    for (int track = 0; track < numTracks; ++track)
    {
        const auto i = (size_t) track;
        const juce::String n (track + 1);

        params.push_back (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID { trackOnIds[i], 1 }, "Track " + n + " On", track < 4));
        params.push_back (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { trackPatternIds[i], 1 }, "Track " + n + " Pattern",
            juce::StringArray { "Euclid", "Hash", "Steps" }, 0));
        params.push_back (std::make_unique<juce::AudioParameterInt> (
            juce::ParameterID { trackStepsIds[i], 1 }, "Track " + n + " Steps", 1, 32, 16));
        params.push_back (std::make_unique<juce::AudioParameterInt> (
            juce::ParameterID { trackPulsesIds[i], 1 }, "Track " + n + " Pulses", 0, 32,
            track == 0 ? 4 : track == 1 ? 2 : track == 2 ? 8 : track == 3 ? 2 : 3));
        params.push_back (std::make_unique<juce::AudioParameterInt> (
            juce::ParameterID { trackRotationIds[i], 1 }, "Track " + n + " Rotation", 0, 32,
            track == 1 ? 4 : track == 3 ? 2 : track == 4 ? 7 : track == 5 ? 3 : 0));
        params.push_back (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { trackDivisionIds[i], 1 }, "Track " + n + " Division",
            juce::StringArray { "1/32", "1/16T", "1/16", "1/8T", "1/8", "1/4", "1/2", "1 Bar" }, 2));
        params.push_back (std::make_unique<juce::AudioParameterInt> (
            juce::ParameterID { trackNoteIds[i], 1 }, "Track " + n + " Note",
            24, 96, defaultTrackNotes[i],
            juce::AudioParameterIntAttributes().withStringFromValueFunction (
                [] (int value, int) { return juce::MidiMessage::getMidiNoteName (value, true, true, 4)
                                          + " (" + juce::String (value) + ")"; })));
        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { trackVelIds[i], 1 }, "Track " + n + " Velocity",
            juce::NormalisableRange<float> (30.0f, 150.0f, 0.1f), 100.0f,
            juce::AudioParameterFloatAttributes().withLabel ("%")));
        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { trackAccentIds[i], 1 }, "Track " + n + " Accent",
            juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 30.0f,
            juce::AudioParameterFloatAttributes().withLabel ("%")));
        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { trackFlamIds[i], 1 }, "Track " + n + " Flam",
            juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 0.0f,
            juce::AudioParameterFloatAttributes().withLabel ("%")));
        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { trackGhostIds[i], 1 }, "Track " + n + " Ghost",
            juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 10.0f,
        juce::AudioParameterFloatAttributes().withLabel ("%")));
        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { trackGainIds[i], 1 }, "Track " + n + " Level",
            juce::NormalisableRange<float> (-24.0f, 12.0f, 0.01f), 0.0f,
            juce::AudioParameterFloatAttributes().withLabel ("dB")));
    }

    // The shared LFOs and routing, appended last exactly as every other modulation plugin.
    SuiteModulation::Router::addParameters (params, modTargetNames());

    return { params.begin(), params.end() };
}

void GrooveMutatorProcessor::prepareToPlay (double newSampleRate, int)
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;

    // Fixed reported latency: the timing-drift budget never changes with a knob (the MTC law).
    latencySamples = (std::int64_t) std::llround (50.0 * sampleRate / 1000.0);
    setLatencySamples ((int) latencySamples);

    modulation.prepare (sampleRate);
    streamSample = 0;
    beatPosition = 0.0;
    attractorBeat = 0.0;
    resyncNeeded = true;
    wasRunning = false;
    grooveWasActive = false;
    lastMutationTick = -1;

    pending.clear();
    pendingOffs.clear();
    lastNoteOn.clear();
    sounding.clear();

    // A re-preparation restarts the plugin: the trigger latch is stale state too, so the
    // groove only runs again after a fresh trigger note-on.
    triggerActive = false;
    lastTriggerMode = juce::roundToInt (triggerMode->load());

    for (int track = 0; track < numTracks; ++track)
    {
        laneNextStep[(size_t) track] = 0;
        laneFillCycle[(size_t) track] = -1;
        laneFillAdditions[(size_t) track] = 0;
        playheadStep[(size_t) track].store (-1, std::memory_order_relaxed);
    }
}

void GrooveMutatorProcessor::releaseResources()
{
    // The host is tearing the plugin down: cancel everything (nothing will deliver).
    pending.clear();
    pendingOffs.clear();
    lastNoteOn.clear();
    sounding.clear();
}

bool GrooveMutatorProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& in = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();
    return in == out && (in == juce::AudioChannelSet::stereo() || in == juce::AudioChannelSet::mono());
}

double GrooveMutatorProcessor::currentBpm (const juce::Optional<juce::AudioPlayHead::PositionInfo>& position,
                                           float freeTempo) const
{
    // Host Sync only takes the host tempo while it is actually on (the MTC law).
    if (sync->load() > 0.5f && position.hasValue())
        if (const auto reported = position->getBpm(); reported.hasValue() && *reported > 1.0)
            return juce::jlimit (20.0, 300.0, (double) *reported);

    return juce::jlimit (40.0, 240.0, (double) freeTempo);
}

double GrooveMutatorProcessor::laneStepBeats (int track) const noexcept
{
    const auto i = (size_t) std::clamp (track, 0, numTracks - 1);
    const int division = juce::jlimit (0, CoreDSP::Rhythm::numDivisions - 1,
                                       juce::roundToInt (divisionParams[i]->load()));
    return CoreDSP::Rhythm::divisionBeats (division)
           * CoreDSP::Rhythm::polyScale (juce::jlimit (0, CoreDSP::Rhythm::numPolyRatios - 1,
                                                       juce::roundToInt (polyRatio->load())));
}

void GrooveMutatorProcessor::rederiveLaneIfNeeded (int track)
{
    const auto i = (size_t) track;

    CoreDSP::Rhythm::LaneParams p;
    p.on = onParams[i]->load() > 0.5f;
    p.patternKind = juce::jlimit (0, CoreDSP::Rhythm::patternKinds - 1,
                                  juce::roundToInt (patternParams[i]->load()));
    p.steps = juce::jlimit (1, CoreDSP::Rhythm::maxSteps, juce::roundToInt (stepsParams[i]->load()));
    p.pulses = juce::jlimit (0, CoreDSP::Rhythm::maxSteps, juce::roundToInt (pulsesParams[i]->load()));
    p.rotation = juce::jlimit (0, CoreDSP::Rhythm::maxSteps, juce::roundToInt (rotationParams[i]->load()));
    p.divisionBeats = laneStepBeats (track);
    p.note = juce::jlimit (24, 96, juce::roundToInt (noteParams[i]->load()));
    p.velocity = juce::jlimit (30.0f, 150.0f, velParams[i]->load()) / 100.0f;
    p.accent = juce::jlimit (0.0f, 100.0f, accentParams[i]->load()) / 100.0f;
    p.flam = juce::jlimit (0.0f, 100.0f, flamParams[i]->load()) / 100.0f;
    p.ghost = juce::jlimit (0.0f, 100.0f, ghostParams[i]->load()) / 100.0f;
    p.gain = juce::Decibels::decibelsToGain (juce::jlimit (-24.0f, 12.0f, gainParams[i]->load()));
    p.stepsMask = laneStepsMask[i].load (std::memory_order_relaxed);

    // The plan-19 rule: a Seed change / preset load / Freeze-off re-derives the mask, and
    // so does any pattern-defining value — the painted steps included. The signature
    // catches all of them.
    const std::uint32_t signature = (std::uint32_t) (p.patternKind * 7919u + (std::uint32_t) p.steps * 104729u
                                  + (std::uint32_t) p.pulses * 1299709u + (std::uint32_t) p.rotation * 15485863u
                                  + p.stepsMask * 2654435761u + juce::roundToInt (seed->load()) * 31u);
    if (signature != laneBaseSignature[i])
    {
        laneStates[i].rederive (p, (std::uint32_t) juce::roundToInt (seed->load()));
        laneBaseSignature[i] = signature;
    }
    laneParams[i] = p;
}

void GrooveMutatorProcessor::runMutationTick (double beat)
{
    const int evolveChoice = juce::jlimit (0, 4, juce::roundToInt (evolveRate->load()));
    static constexpr double evolveBeats[5] = { 1.0, 4.0, 8.0, 16.0, 32.0 };
    const std::int64_t tick = (std::int64_t) std::floor (std::max (beat, 0.0) / evolveBeats[evolveChoice]);

    if (tick == lastMutationTick)
        return;

    const bool firstTick = lastMutationTick < 0;
    lastMutationTick = tick;

    if (firstTick)
        return;   // the boundary at beat 0 is not a crossing

    // Freeze locks the current pattern; Chaos 0 means nothing mutates.
    if (freeze->load() > 0.5f)
        return;
    const float chaosAmount = juce::jlimit (0.0f, 100.0f, getModulatedValue (ModTarget::Chaos)) / 100.0f;
    if (chaosAmount <= 0.0f)
        return;

    const float scaleAmount = juce::jlimit (0.0f, 100.0f,
                                             getModulatedValue (ModTarget::MutationScale)) / 100.0f;
    const auto seedValue = (std::uint32_t) juce::roundToInt (seed->load());

    for (int track = 0; track < numTracks; ++track)
    {
        if (onParams[(size_t) track]->load() <= 0.5f)
            continue;

        // Steps lanes are the authoring mode (plan 21, C3): what you painted is what
        // plays, so the mutation operators touch only the generative kinds (Euclid, Hash).
        if (laneParams[(size_t) track].patternKind == 2)
            continue;

        // The chaos gates whether this lane mutates on this tick (deterministic per tick
        // and lane), and the scale picks the operator tier.
        if (CoreDSP::Sequencer::hashUnit (seedValue, (std::uint32_t) (tick * 13u + (std::uint32_t) track))
                >= chaosAmount)
            continue;

        auto& state = laneStates[(size_t) track];
        state.mutateTick = (std::uint64_t) tick;
        state.mask = CoreDSP::Rhythm::mutatePattern (state.mask, laneParams[(size_t) track].steps,
                                                     scaleAmount,
                                                     CoreDSP::Rhythm::mutationHash (seedValue, (std::uint64_t) tick, track));
    }
}

void GrooveMutatorProcessor::scheduleGroove (double blockStartBeat, double blockEndBeat, double samplesPerBeat)
{
    // The resync decision is captured before the loop: a host jump re-aligns every lane's
    // grid, and the flag clears once the pass is done.
    const bool needResync = resyncNeeded || ! grooveWasActive;
    resyncNeeded = false;

    for (int track = 0; track < numTracks; ++track)
    {
        if (onParams[(size_t) track]->load() <= 0.5f)
            continue;

        rederiveLaneIfNeeded (track);
        const double stepLen = laneStepBeats (track);

        if (needResync
            || (double) laneNextStep[(size_t) track] * stepLen > blockEndBeat + stepLen
            || (double) laneNextStep[(size_t) track] * stepLen < blockStartBeat - stepLen)
        {
            laneNextStep[(size_t) track] = (std::int64_t) std::ceil (blockStartBeat / stepLen - 1.0e-9);
        }

        while ((double) laneNextStep[(size_t) track] * stepLen < blockEndBeat)
        {
            const double stepBeat = (double) laneNextStep[(size_t) track] * stepLen;
            if (stepBeat >= blockStartBeat - 1.0e-9)
                scheduleLaneStep (track, laneNextStep[(size_t) track], stepBeat, blockStartBeat, samplesPerBeat);
            ++laneNextStep[(size_t) track];
        }
    }
}

void GrooveMutatorProcessor::scheduleLaneStep (int track, std::int64_t stepIndex, double stepBeat,
                                               double blockStartBeat, double samplesPerBeat)
{
    const auto i = (size_t) track;
    auto& params = laneParams[i];
    auto& state = laneStates[i];
    const int steps = params.steps;
    const int patternIndex = (int) (((stepIndex % steps) + steps) % steps);

    // The fill additions recompute once per cycle; the window ORs them into the mask.
    const std::int64_t cycle = stepIndex / steps;
    if (cycle != laneFillCycle[i])
    {
        laneFillCycle[i] = cycle;
        laneFillAdditions[i] = CoreDSP::Rhythm::fillMask (state.mask, steps,
                                                          juce::jlimit (0, CoreDSP::Rhythm::numFillStyles - 1,
                                                                        juce::roundToInt (fillStyle->load())),
                                                          juce::jlimit (0.0f, 100.0f,
                                                              getModulatedValue (ModTarget::FillProbability)) / 100.0f,
                                                          CoreDSP::Rhythm::mutationHash (
                                                              (std::uint32_t) juce::roundToInt (seed->load()),
                                                              (std::uint64_t) cycle, track));
    }

    bool fires = ((state.mask >> patternIndex) & 1u) != 0u;
    if (CoreDSP::Rhythm::inFillWindow (patternIndex, steps, params.divisionBeats,
                                       CoreDSP::Rhythm::fillBarCount (
                                           juce::roundToInt (fillBars->load()))))
        fires = fires || ((laneFillAdditions[i] >> patternIndex) & 1u) != 0u;

    if (! fires)
        return;

    const int note = params.note;
    const float stepLenBeats = params.divisionBeats;

    // The velocity: the track's percentage, shaped by the curve, the accent/ghost shares,
    // the level stage and the humanise jitter.
    const float velocityMul = trackVelocityFor (track, patternIndex)
                              * (1.0f + (CoreDSP::Sequencer::hashUnit (
                                             (std::uint32_t) juce::roundToInt (seed->load()),
                                             (std::uint32_t) (5003 + patternIndex)) - 0.5f)
                                             * 2.0f * juce::jlimit (0.0f, 100.0f,
                                                 getModulatedValue (ModTarget::HumaniseVelocity)) / 100.0f);
    const int velocity = juce::jlimit (1, 127,
                                       juce::roundToInt (100.0f * velocityMul));

    // The timing: the grid sample, the swing (odd steps late, the MTC law), the humanise
    // jitter, then the fixed latency.
    const double swingSamples = CoreDSP::Sequencer::swingBeats (patternIndex,
                                                                juce::jlimit (-50.0f, 50.0f,
                                                                    getModulatedValue (ModTarget::Swing)) / 100.0f,
                                                                stepLenBeats) * samplesPerBeat;
    const float humaniseMs = (CoreDSP::Sequencer::hashUnit ((std::uint32_t) juce::roundToInt (seed->load()),
                                                            (std::uint32_t) (7001 + patternIndex)) - 0.5f)
                             * 2.0f * juce::jlimit (0.0f, 20.0f,
                                 getModulatedValue (ModTarget::HumaniseTime));
    const double humaniseSamples = (double) humaniseMs * sampleRate / 1000.0;
    const double gridSample = (double) streamSample + (stepBeat - blockStartBeat) * samplesPerBeat;
    const auto onTarget = std::max<std::int64_t> (streamSample,
        (std::int64_t) std::llround (gridSample + swingSamples + humaniseSamples) + latencySamples);

    // The note rings until the next step boundary (90 % of it, so hits separate cleanly).
    const auto length = std::max<std::int64_t> (1,
        (std::int64_t) std::llround (stepLenBeats * samplesPerBeat * 0.9));

    queueGrooveNote (track, note, velocity, onTarget, length);

    // A flam is a second, quieter hit a few milliseconds later (6-12 ms, deterministic).
    if (laneFlamsAt (track, patternIndex))
    {
        const float flamMs = 6.0f + 6.0f * CoreDSP::Sequencer::hashUnit (
            (std::uint32_t) juce::roundToInt (seed->load()), (std::uint32_t) (7005 + patternIndex));
        const auto flamTarget = onTarget + (std::int64_t) std::llround ((double) flamMs * sampleRate / 1000.0);
        const int flamVelocity = juce::jlimit (1, 127, (int) std::lround ((float) velocity * 0.6f));
        queueGrooveNote (track, note, flamVelocity, flamTarget, length / 2);
    }
}

void GrooveMutatorProcessor::queueGrooveNote (int track, int note, int velocity, std::int64_t onTarget,
                                              std::int64_t lengthSamples)
{
    const int key = track * 128 + note;
    const auto found = pendingOffs.find (key);
    const auto earlier = lastNoteOn.find (key);

    // The MTC retrigger rule: when the earlier copy's note-off is still pending beyond the
    // new target, the earlier note is cut just before the new one (it never hangs).
    if (found != pendingOffs.end() && found->second->first >= onTarget && earlier != lastNoteOn.end())
    {
        onTarget = std::max (onTarget, earlier->second + 1);
        const std::int64_t cutAt = std::max (onTarget - 1, earlier->second);

        pending.erase (found->second);
        pendingOffs.erase (found);
        pendingOffs[key] = pending.emplace (cutAt, juce::MidiMessage::noteOff (1, note));
    }

    lastNoteOn[key] = onTarget;
    pending.emplace (onTarget, juce::MidiMessage::noteOn (1, note, (std::uint8_t) velocity));
    pendingOffs[key] = pending.emplace (onTarget + std::max<std::int64_t> (1, lengthSamples),
                                        juce::MidiMessage::noteOff (1, note));
}

void GrooveMutatorProcessor::stopAllNotes()
{
    pending.clear();
    pendingOffs.clear();
    lastNoteOn.clear();

    for (const int key : sounding)
        pending.emplace (streamSample, juce::MidiMessage::noteOff (1, key % 128));
    sounding.clear();
}

void GrooveMutatorProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();

    const auto* playHead = getPlayHead();
    const auto position = playHead != nullptr ? playHead->getPosition()
                                              : juce::Optional<juce::AudioPlayHead::PositionInfo>();
    const bool followHost = sync->load() > 0.5f && position.hasValue();
    const bool transportRunning = ! followHost || position->getIsPlaying();

    // Follow the host position first (the MTC block start): a jump resets the grid.
    if (transportRunning && followHost)
    {
        const auto ppq = position->getPpqPosition();
        if (ppq.hasValue() && (! wasRunning || std::abs (*ppq - beatPosition) > 1.0e-3))
        {
            beatPosition = *ppq;
            resyncNeeded = true;
        }
    }

    const float freeTempo = juce::jlimit (40.0f, 240.0f, tempo->load());
    const double samplesPerBeat = 60.0 / currentBpm (position, freeTempo) * sampleRate;

    // The trigger: scan the input for the trigger key's note-on and note-off. Latch starts
    // the groove and keeps it running; Gate runs only while the key is held.
    const int wantedNote = juce::jlimit (24, 96, juce::roundToInt (triggerNote->load()));
    const bool gateMode = juce::roundToInt (triggerMode->load()) == 1;

    for (const auto metadata : midi)
    {
        const auto& message = metadata.getMessage();
        if (! message.isNoteOnOrOff() || message.getNoteNumber() != wantedNote)
            continue;

        if (message.isNoteOn())
            triggerActive = true;
        else if (message.isNoteOff() && gateMode)
            triggerActive = false;
    }

    // A stopped transport releases every sounding note and clears the held-trigger state
    // (the plan-19 ending path).
    if (! transportRunning && triggerActive)
        triggerActive = false;

    // The trigger-mode change is an ending path: the mode switch stops the groove. The
    // cache initialises to the current value, so the first block is not a "change".
    if (lastTriggerMode != juce::roundToInt (triggerMode->load()))
    {
        lastTriggerMode = juce::roundToInt (triggerMode->load());
        if (grooveWasActive)
            stopAllNotes();
        triggerActive = false;
        grooveWasActive = false;
    }

    // The run sources (plan 21): synced, the host transport runs the groove; free (sync
    // off, or no host playhead), the trigger key runs it (Latch or Gate).
    const bool running = followHost ? transportRunning : triggerActive;

    // Block-start values: the LFO positions for the UI, published once per block.
    const auto blockOffsets = modulation.computeOffsets ((double) streamSample, beatPosition, (double) streamSample);
    modulation.publish (blockOffsets, (double) streamSample, beatPosition);

    if (! running)
    {
        if (grooveWasActive)
        {
            // The groove stops: every sounding note receives its note-off.
            stopAllNotes();
            grooveWasActive = false;
            triggerActive = false;
        }

        for (int track = 0; track < numTracks; ++track)
            playheadStep[track].store (-1, std::memory_order_relaxed);
    }
    else
    {
        // Panic flag from a preset/state load: every sounding note receives its note-off,
        // and the grid restarts (the MTC rule — but the note-offs survive delivery).
        if (panicRequested.load (std::memory_order_relaxed))
        {
            panicRequested.store (false, std::memory_order_relaxed);
            stopAllNotes();
        }

        // Delays count from the start of playback.
        if (! wasRunning)
            modulation.restartDelayClock();

        if (! grooveWasActive)
        {
            grooveWasActive = true;
            resyncNeeded = true;   // the lanes re-align to the current beat on activation
        }

        const double blockStart = beatPosition;
        const double blockEnd = blockStart + numSamples / samplesPerBeat;

        // The attractor follows the musical time (the MTC pattern) and feeds the trail.
        advanceAttractorTo (blockStart);
        publishTrailPoint();

        runMutationTick (blockStart);
        scheduleGroove (blockStart, blockEnd, samplesPerBeat);

        for (int track = 0; track < numTracks; ++track)
        {
            const int steps = laneParams[(size_t) track].steps;
            const int step = (int) std::floor (blockStart / std::max (laneStepBeats (track), 1.0e-9) + 1.0e-9);
            playheadStep[track].store (((step % steps) + steps) % steps, std::memory_order_relaxed);
        }

        beatPosition = blockEnd;
    }

    // Incoming MIDI passes through with the same latency (the MTC invariant) — except the
    // trigger note itself, which is a control signal: it starts the groove and is consumed.
    for (const auto metadata : midi)
    {
        const auto& message = metadata.getMessage();
        if (message.isNoteOnOrOff() && message.getNoteNumber() == wantedNote)
            continue;
        pending.emplace (streamSample + metadata.samplePosition + latencySamples, message);
    }

    midi.clear();

    // Free LFOs keep running whether or not anything is playing.
    modulation.advanceBlock (numSamples);

    const std::int64_t blockEndSample = streamSample + numSamples;
    for (auto it = pending.begin(); it != pending.end() && it->first < blockEndSample;)
    {
        const auto& message = it->second;
        const auto offset = (int) std::max<std::int64_t> (0, it->first - streamSample);

        if (message.isNoteOn())
            sounding.insert (message.getNoteNumber());
        else if (message.isNoteOff())
            sounding.erase (message.getNoteNumber());

        // Any pendingOffs entry pointing at the entry being delivered would dangle once
        // it is erased — drop it here (the MTC matching-erase rule, per note-off).
        for (auto fit = pendingOffs.begin(); fit != pendingOffs.end();)
        {
            if (fit->second == it)
                fit = pendingOffs.erase (fit);
            else
                ++fit;
        }

        midi.addEvent (message, offset);
        it = pending.erase (it);
    }

    streamSample = blockEndSample;
}

juce::AudioProcessorEditor* GrooveMutatorProcessor::createEditor()
{
    return new GrooveMutatorEditor (*this);
}

void GrooveMutatorProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    destData = SuiteState::write (apvts, stateVersion);
}

void GrooveMutatorProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    SuiteState::read (data, sizeInBytes, apvts, stateVersion, migrations);

    // A state load means everything restarts: the next block stops the sounding notes and
    // clears the grid (the MTC panic rule). The preset identity rides as a property (plan
    // 09 1.4, D3): restore the bar's selection from the replaced state, and the painted
    // Steps masks theirs (absent in the incoming state, they reset to empty).
    panicRequested.store (true, std::memory_order_relaxed);
    loadStepsMasksFromState();
    lastPresetSelection = SuitePresets::decodePresetSelection (
        apvts.state.getProperty (presetSelectionProperty).toString());
}

void GrooveMutatorProcessor::setPresetSelection (const SuitePresets::PresetSelection& selection)
{
    lastPresetSelection = selection;
    apvts.state.setProperty (presetSelectionProperty, SuitePresets::encodePresetSelection (selection), nullptr);
}

void GrooveMutatorProcessor::applyPresetState (const juce::ValueTree& tree)
{
    // A preset is a delta on the default state, so resetMissing is true. The tree goes
    // through the same read path as a saved session: version check, migrations, replaceState.
    const juce::MemoryBlock block = SuiteState::detail::encodeXml (*tree.createXml());
    const auto result = SuiteState::read (block.getData(), (int) block.getSize(), apvts, stateVersion,
                                          migrations, {}, true);

    if (result == SuiteState::LoadResult::loaded)
    {
        // The caller (the bar or the host program bridge) re-states the selection right
        // after, when it knows the preset. The next block clears the pending panic.
        lastPresetSelection = {};
        panicRequested.store (true, std::memory_order_relaxed);

#if SUITE_PRESET_BANK_AS_PROGRAMS
        currentProgramIndex.store (-1, std::memory_order_relaxed);
#endif
    }
}

//==============================================================================
// Host program list. Program 0 is Init; 1 to N are the factory presets in bank order. The
// list only exists in a Pro build with SUITE_PRESET_BANK_AS_PROGRAMS on; otherwise the
// plugin reports one program, as it did before the preset system.

int GrooveMutatorProcessor::getNumPrograms()
{
#if SUITE_PRESET_BANK_AS_PROGRAMS
    return SuitePresets::PresetPrograms::getNumPrograms (getPresetPluginId());
#else
    return 1;
#endif
}

int GrooveMutatorProcessor::getCurrentProgram()
{
#if SUITE_PRESET_BANK_AS_PROGRAMS
    return currentProgramIndex.load (std::memory_order_relaxed);
#else
    return 0;
#endif
}

void GrooveMutatorProcessor::setCurrentProgram (int index)
{
#if SUITE_PRESET_BANK_AS_PROGRAMS
    if (SuitePresets::PresetPrograms::applyProgram (*this, index))
    {
        currentProgramIndex.store (index, std::memory_order_relaxed);

        // The host program and the bar's selection describe the same event (plan 09 1.4).
        SuitePresets::PresetSelection selection;
        if (index > 0)
        {
            selection.kind = SuitePresets::PresetSelection::Kind::factory;
            selection.factoryIndex = index - 1;
        }
        setPresetSelection (selection);
    }
#else
    juce::ignoreUnused (index);
#endif
}

const juce::String GrooveMutatorProcessor::getProgramName (int index)
{
#if SUITE_PRESET_BANK_AS_PROGRAMS
    return SuitePresets::PresetPrograms::getProgramName (getPresetPluginId(), index);
#else
    juce::ignoreUnused (index);
    return {};
#endif
}

void GrooveMutatorProcessor::changeProgramName (int, const juce::String&)
{
    // The factory programs are read-only.
}

void GrooveMutatorProcessor::getCurrentProgramStateInformation (juce::MemoryBlock& destData)
{
    getStateInformation (destData);
}

void GrooveMutatorProcessor::setCurrentProgramStateInformation (const void* data, int sizeInBytes)
{
    setStateInformation (data, sizeInBytes);

#if SUITE_PRESET_BANK_AS_PROGRAMS
    currentProgramIndex.store (-1, std::memory_order_relaxed);
#endif
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new GrooveMutatorProcessor();
}
