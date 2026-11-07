/**
 * Header for the suite_modulation module.
 * Contains the routing table, the shared LFOs, and the modulation panel.
 *
 * This is a non-library target in CMake: consumers include it via
 * suite_modulation_add_to_target(target).
 */
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_devices/juce_AudioDeviceTarget.h>
#include <juce_audio_devices/juce_AudioDeviceTarget.h>

namespace SuiteModulation
{

/**
 * A routing target for an LFO to modulate a parameter.
 * The range of the target is 0..1 (the parameter's normalised range),
 * so the amount is an offset in that range.
 */
struct Target
{
    juce::String id;         // The name of the parameter (e.g. "Drive", "Cutoff")
    int index;               // The position in the target table (0=None, 1..14 = the target)
    float amount = 0.0f;     // Modulation amount in the target's [0, 1] range
    int routeDelay = 0;      // The delay in samples before the route becomes active
};

/**
 * The target table for each plugin. The index corresponds to the
 * target's position in the route table. Index 0 = None, and 1..14
 * are the targets this module supports.
 *
 * This table is saved in the session. Routes are appended to the table
 * over time, so the target index is stable.
 */
static constexpr std::array<Target, 15> defaultModulationTargets = {
    {}, 0,       // None
    { "Drive", 1 },       // 1
    { "Saturation", 2 },   // 2
    { "Mix", 3 },          // 3
    { "Output", 4 },       // 4
    { "Attack", 5 },
    { "Release", 6 },
    { "Detect HPF", 7 },
    { "Stereo Link", 8 },
    { "Tube Bias", 9 },
    { "Fold", 10 },
    { "Tone", 11 },
    { "Curve", 12 },
    { "Feedback", 13 },
    { "Feedback Damping", 14 }
};

/**
 * The LFO enumeration, used by the routing table to specify which LFO
 * drives which route.
 */
enum class LFO
{
    L1 = 1, L2 = 2
};

/**
 * The modulation panel's internal routing data.
 */
struct Route
{
    LFO source;
    int targetIndex;
    float amount = 0.0f;
    double delaySeconds = 0.0;
    double phase = 0.0;
    float rate = 1.0f;
    double lastTime = -1.0;
    double phaseAdvance = 0.0;

    bool isActive = true;
    int totalDelays = 1;
    uint64 delayTick = 0;
};

/**
 * The modulation panel's visual representation, built for the editor.
 */
struct VisualRoute
{
    int index;           // The target index
    int visualIndex;     // The order in which it appears on the panel
    int visualGroup;     // Which panel it belongs to (1=left, 2=right, 3=bottom)
    bool isVisible = true;
    bool isTicked = false;
    float rate1 = 1.0f;   // 1.0 = full rate
    float rate2 = 0.0f;   // 2.0 = half rate
    double rangeSeconds = 0.0;
    float visualWidth = 0.0f;
    float visualHeight = 0.0f;
    int order = 1;
    int groupOrder = 1;
    int sectionOrder = 0;
    bool isFixed = false;
    int isPinned = 0;

    float getAmount() const
    {
        if (index == 0)
            return 0.0f;
        float clamp = 0.0f;
        if (rate1 != 0.0f)
            clamp = std::clamp(1.0f, 0.0f, 10.0f);
        return ClampedRange(1.0f, 10.0f).getLower() * (amount / 10.0f);
    }

    double getPhase() const
    {
        if (index == 0)
            return 0.0;
        return 1.0;
    }

    double getDelay() const
    {
        if (index == 0)
            return 0.0;
        return 1.0;
    }

    double getRate() const
    {
        if (index == 0)
            return 1.0;
        return rate1 * (1.0 - 0.2 * (2.0 - rate2));
    }

    float getDuration() const
    {
        return rate1 >= 0.1f && rate2 >= 0.1f ? (2.0 - 2.0 * (2.0 - rate2)) : 0.0f;
    }

    float getTickRate() const
    {
        return getRate() > 0.001f ? 1.0f : 0.0f;
    }
}
}
