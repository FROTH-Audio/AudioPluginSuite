#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <algorithm>
#include <functional>
#include <memory>
#include <vector>

#include "PluginProcessor.h"
#include "SuiteLookAndFeel.h"
#include "SuiteWidgets.h"
#include "SuiteBrand.h"

//==============================================================================
// Visual widgets for the Sgraffito editor. They read the processor only through
// its public API (getParamValue, sampleBuffer, voicePool), so they stay free of
// the editor's friendship.

/** Coloured banner showing the active playback mode. */
class SgraffitoModeBanner : public juce::Component
{
public:
    explicit SgraffitoModeBanner (SgraffitoProcessor& p) : processor (p) {}

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();

        static constexpr const char* names[4] = { "SCRUB", "CHOP", "CLOUD", "SWELL" };
        static const juce::Colour colours[4] = { juce::Colour (0xff2de2ff), juce::Colour (0xffff9f1c),
                                                 juce::Colour (0xff7dff5a), juce::Colour (0xff9aa0a6) };
        const int mode = juce::jlimit (0, 3, juce::roundToInt (processor.getParamValue (SgraffitoProcessor::modeId)));
        const auto colour = colours[mode];

        g.setColour (colour.withAlpha (0.14f));
        g.fillRoundedRectangle (bounds, 6.0f);
        g.setColour (colour.withAlpha (0.45f));
        g.drawRoundedRectangle (bounds, 6.0f, 1.0f);
        g.setColour (colour);
        g.fillRect (bounds.getX() + 1.0f, bounds.getY() + 1.0f, 4.0f, bounds.getHeight() - 2.0f);

        g.setColour (SuiteUI::Colours::text);
        g.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
        g.drawText (names[mode], bounds, juce::Justification::centred);
    }

private:
    SgraffitoProcessor& processor;
};

/** Waveform display: sample envelope, the region the root key maps to, and Chop slice lines.

    The region maths mirrors the processor's note-on path exactly (span and length are
    per-cent values divided by 100, Chop picks the key's slice of the whole sample). If the
    processor changes, keep this in step. */
class SgraffitoWaveformView : public juce::Component
{
public:
    explicit SgraffitoWaveformView (SgraffitoProcessor& p) : processor (p) {}

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();

        g.setColour (SuiteUI::Colours::panel.withAlpha (0.55f));
        g.fillRoundedRectangle (bounds, 6.0f);
        g.setColour (SuiteUI::Colours::track);
        g.drawRoundedRectangle (bounds, 6.0f, 1.0f);

        const auto* sample = processor.sampleBuffer.load (std::memory_order_acquire);
        if (sample == nullptr || sample->getNumSamples() == 0)
        {
            g.setColour (SuiteUI::Colours::textDim);
            g.setFont (juce::Font (juce::FontOptions (12.0f)));
            g.drawText ("Waiting for sample...", bounds, juce::Justification::centred);
            return;
        }

        const auto accent = juce::Colour (0xff9aa0a6);
        auto plot = getLocalBounds().toFloat().reduced (6.0f);

        double regionStart = 0.0, regionEnd = 1.0;
        computeRegion (regionStart, regionEnd);

        // Region overlay.
        const float regionX0 = plot.getX() + (float) regionStart * plot.getWidth();
        const float regionX1 = plot.getX() + (float) regionEnd * plot.getWidth();
        g.setColour (accent.withAlpha (0.14f));
        g.fillRect (regionX0, plot.getY(), regionX1 - regionX0, plot.getHeight());
        g.setColour (accent.withAlpha (0.75f));
        g.drawLine (regionX0, plot.getY(), regionX0, plot.getBottom());
        g.drawLine (regionX1, plot.getY(), regionX1, plot.getBottom());

        // Chop slice lines inside the region.
        if (juce::roundToInt (processor.getParamValue (SgraffitoProcessor::modeId)) == 1)
        {
            const int sliceCount = juce::jlimit (2, 32, juce::roundToInt (processor.getParamValue (SgraffitoProcessor::sliceCountId)));
            g.setColour (accent.withAlpha (0.35f));

            for (int i = 1; i < sliceCount; ++i)
            {
                const float x = regionX0 + (regionX1 - regionX0) * i / (float) sliceCount;
                g.drawLine (x, plot.getY(), x, plot.getBottom());
            }
        }

        // Envelope: one min/max bar per pixel column.
        const int numSamples = sample->getNumSamples();
        const int numChannels = sample->getNumChannels();
        const int width = juce::roundToInt (plot.getWidth());
        const float mid = plot.getCentreY();
        const float half = plot.getHeight() * 0.46f;

        g.setColour (SuiteUI::Colours::text.withAlpha (0.85f));

        for (int x = 0; x < width; ++x)
        {
            const int i0 = (int) ((double) x * numSamples / width);
            const int i1 = juce::jmax (i0 + 1, (int) ((double) (x + 1) * numSamples / width));
            float lo = 0.0f, hi = 0.0f;

            for (int i = i0; i < i1; ++i)
                for (int ch = 0; ch < numChannels; ++ch)
                {
                    const float v = sample->getSample (ch, i);
                    lo = juce::jmin (lo, v);
                    hi = juce::jmax (hi, v);
                }

            const float px = plot.getX() + (float) x;
            g.drawLine (px, mid - hi * half, px, mid - lo * half);
        }

        // Caption.
        g.setColour (SuiteUI::Colours::textDim);
        g.setFont (juce::Font (juce::FontOptions (10.0f)));
        g.drawText (regionCaption (regionStart, regionEnd),
                    getLocalBounds().removeFromBottom (16).reduced (8, 0).toFloat(),
                    juce::Justification::centredRight);
    }

private:
    /** Region maths, mirroring CoreDSP::Sampler::makeRegion / keyRegion / chopRegion
        (span and length are per-cent values divided by 100, Chop picks the key's slice of
        the whole sample over maxKeys). If the engine maps change, keep this in step. */
    void computeRegion (double& regionStart, double& regionEnd) const
    {
        const double spanFrac = std::clamp (processor.getParamValue (SgraffitoProcessor::regionSpanId) / 100.0, 0.0, 1.0);
        const double lenFrac  = std::clamp (processor.getParamValue (SgraffitoProcessor::regionLengthId) / 100.0, 0.0, 1.0);
        const int mode = juce::roundToInt (processor.getParamValue (SgraffitoProcessor::modeId));

        if (mode == 1) // Chop: the key picks a slice of the whole sample.
        {
            const int sliceCount = juce::jlimit (2, CoreDSP::Sampler::maxRegions,
                                                 juce::roundToInt (processor.getParamValue (SgraffitoProcessor::sliceCountId)));
            const int key = juce::jlimit (0, CoreDSP::Sampler::maxKeys - 1,
                                          juce::roundToInt (processor.getParamValue (SgraffitoProcessor::rootKeyId)));
            const int slice = juce::jlimit (0, sliceCount - 1, key * sliceCount / CoreDSP::Sampler::maxKeys);
            regionStart = slice / (double) sliceCount;
            regionEnd = (slice + 1) / (double) sliceCount;
        }
        else
        {
            const double keyPos = processor.getParamValue (SgraffitoProcessor::rootKeyId) / 127.0;
            const double start = std::clamp (keyPos * spanFrac, 0.0, 1.0 - lenFrac);
            regionStart = start;
            regionEnd = start + lenFrac;
        }
    }

    static juce::String regionCaption (double start, double end)
    {
        return "Region " + juce::String (juce::roundToInt (start * 100.0)) + "% - "
             + juce::String (juce::roundToInt (end * 100.0)) + "%";
    }

    SgraffitoProcessor& processor;
};

/** Key map strip: MIDI 24-96 as a mini keyboard. Shows the root key, the members of the
    locked scale, and every key with an active voice. */
class SgraffitoKeyMapStrip : public juce::Component
{
public:
    explicit SgraffitoKeyMapStrip (SgraffitoProcessor& p) : processor (p) {}

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();

        g.setColour (SuiteUI::Colours::panel.withAlpha (0.55f));
        g.fillRoundedRectangle (bounds, 4.0f);
        g.setColour (SuiteUI::Colours::track);
        g.drawRoundedRectangle (bounds, 4.0f, 1.0f);

        constexpr int firstKey = 24;
        constexpr int numKeys = 73; // MIDI 24-96
        const auto accent = juce::Colour (0xff9aa0a6);

        const float keyW = bounds.getWidth() / (float) numKeys;
        const int rootKey = juce::jlimit (firstKey, firstKey + numKeys - 1,
                                          juce::roundToInt (processor.getParamValue (SgraffitoProcessor::rootKeyId)));
        const bool scaleLock = processor.getParamValue (SgraffitoProcessor::scaleLockId) > 0.5f;
        const int scale = juce::jlimit (0, 5, juce::roundToInt (processor.getParamValue (SgraffitoProcessor::scaleId)));

        bool lit[128] = {};
        for (const auto& voice : processor.voicePool.voices)
            if (voice.active)
                lit[juce::jlimit (0, 127, voice.key)] = true;

        for (int pass = 0; pass < 2; ++pass) // white keys first, then black keys on top
        {
            for (int i = 0; i < numKeys; ++i)
            {
                const int note = firstKey + i;

                if (isBlackKey (note) != (pass == 1))
                    continue;

                const float height = isBlackKey (note) ? bounds.getHeight() * 0.62f : bounds.getHeight();
                auto keyRect = juce::Rectangle<float> (bounds.getX() + i * keyW, bounds.getY(), keyW, height);
                keyRect.reduce (0.5f, 0.0f);

                if (isBlackKey (note))
                {
                    g.setColour (SuiteUI::Colours::track.withAlpha (0.85f));
                    g.fillRect (keyRect);
                }
                else
                {
                    g.setColour (SuiteUI::Colours::panelLight.withAlpha (0.55f));
                    g.fillRect (keyRect);
                }

                if (scaleLock && scale != 0 && isScaleMember (scale, note - rootKey))
                {
                    g.setColour (accent.withAlpha (0.22f));
                    g.fillRect (keyRect);
                }

                if (note == rootKey)
                {
                    g.setColour (accent.withAlpha (0.75f));
                    g.fillRect (keyRect);

                    if (keyW >= 14.0f)
                    {
                        g.setColour (SuiteUI::Colours::background);
                        g.setFont (juce::Font (juce::FontOptions (9.0f, juce::Font::bold)));
                        g.drawText (noteName (note), keyRect, juce::Justification::centredBottom);
                    }
                }

                if (lit[note])
                {
                    g.setColour (accent.withAlpha (0.95f));
                    const float dot = juce::jlimit (2.0f, 5.0f, keyW * 0.5f);
                    g.fillEllipse (keyRect.getCentreX() - dot, keyRect.getCentreY() - dot, dot * 2.0f, dot * 2.0f);
                }
            }
        }
    }

private:
    static bool isBlackKey (int note) noexcept
    {
        const int pc = ((note % 12) + 12) % 12;
        return pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10;
    }

    static bool isScaleMember (int scale, int semitonesFromRoot)
    {
        const int s = ((semitonesFromRoot % 12) + 12) % 12;

        switch (scale)
        {
            case 1:  return s == 0 || s == 2 || s == 4 || s == 5 || s == 7 || s == 9 || s == 11; // Major
            case 2:  return s == 0 || s == 2 || s == 3 || s == 5 || s == 7 || s == 8 || s == 10; // Minor
            case 3:  return s == 0 || s == 3 || s == 5 || s == 7 || s == 10;                     // Minor pentatonic
            case 4:  return s == 0 || s == 2 || s == 3 || s == 5 || s == 7 || s == 9 || s == 10; // Dorian
            case 5:  return (s % 2) == 0;                                                       // Whole tone
            default: return true;                                                               // Chromatic
        }
    }

    static juce::String noteName (int note)
    {
        static const char* names[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        return names[((note % 12) + 12) % 12] + juce::String (note / 12 - 1);
    }

    SgraffitoProcessor& processor;
};

//==============================================================================
class SgraffitoEditor : public juce::AudioProcessorEditor,
                        private juce::Timer
{
public:
    explicit SgraffitoEditor (SgraffitoProcessor&);
    ~SgraffitoEditor() override;

    void paint (juce::Graphics&) override;

    /** Switches between the sound page (0) and the texture page (1). */
    void setPage (int page);
    int getPage() const noexcept { return page; }

private:
    void timerCallback() override;

    /** Forwards mouse moves from every child to the editor, so the help line follows the pointer. */
    struct HelpTracker : public juce::MouseListener
    {
        std::function<void (const juce::MouseEvent&)> onMove;
        void mouseMove (const juce::MouseEvent& e) override { if (onMove) onMove (e); }
    };

    void showHelp (const juce::MouseEvent&);
    void layoutSoundPage (juce::Rectangle<int> area);
    void layoutTexturePage (juce::Rectangle<int> area);

    SgraffitoProcessor& processor;

    // Declared first so it is destroyed last, after every component that uses it.
    SuiteUI::SuiteLookAndFeel lookAndFeel { juce::Colour (0xff9aa0a6) };
    HelpTracker helpTracker;

    juce::Label helpLabel;

    // Page switch.
    SuiteUI::SegmentedSelector pageSelector { juce::Colour (0xff9aa0a6) };

    // SOUND page controls.
    juce::Label modeCaption;
    SuiteUI::SegmentedSelector modeSelector { juce::Colour (0xff9aa0a6) };
    SuiteUI::LabelledSlider regionSpanSlider { "REGION SPAN", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider regionLengthSlider { "REGION LENGTH", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider rootKeySlider { "ROOT KEY", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider pitchFineSlider { "PITCH FINE", juce::Slider::RotaryVerticalDrag };
    juce::ToggleButton scaleLockButton { "SCALE LOCK" };
    SuiteUI::SegmentedSelector scaleSelector { juce::Colour (0xff9aa0a6) };
    SuiteUI::LabelledSlider glideSlider { "GLIDE", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider levelSlider { "LEVEL", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider velocityCurveSlider { "VELOCITY CURVE", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider attackSlider { "ATTACK", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider releaseSlider { "RELEASE", juce::Slider::RotaryVerticalDrag };

    // Visual row: sample waveform and key map.
    SgraffitoModeBanner modeBanner { processor };
    SgraffitoWaveformView waveformView { processor };
    SgraffitoKeyMapStrip keyStrip { processor };

    // TEXTURE page controls.
    SuiteUI::LabelledSlider tapeStopTimeSlider { "TAPE STOP TIME", juce::Slider::RotaryVerticalDrag };
    SuiteUI::SegmentedSelector tapeStopCurveSelector { juce::Colour (0xff9aa0a6) };
    SuiteUI::LabelledSlider memoryAmountSlider { "MEMORY AMOUNT", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider memoryDecaySlider { "MEMORY DECAY", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider driftAmountSlider { "DRIFT AMOUNT", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider driftSpeedSlider { "DRIFT SPEED", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider layBackSlider { "LAY BACK", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider swingSlider { "SWING", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider humaniseSlider { "HUMANISE", juce::Slider::RotaryVerticalDrag };
    juce::ToggleButton ghostOnButton { "GHOST ON" };
    SuiteUI::LabelledSlider ghostThresholdSlider { "GHOST THRESHOLD", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider ghostLevelSlider { "GHOST LEVEL", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider ghostDelaySlider { "GHOST DELAY", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider sliceCountSlider { "SLICE COUNT", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider grainSizeSlider { "GRAIN SIZE", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider grainDensitySlider { "GRAIN DENSITY", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider grainSpreadSlider { "GRAIN SPREAD", juce::Slider::RotaryVerticalDrag };
    juce::ToggleButton sustainHoldButton { "SUSTAIN HOLD" };
    SuiteUI::LabelledSlider swellTimeSlider { "SWELL TIME", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider swellCrossfadeSlider { "SWELL CROSSFADE", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider dustSlider { "DUST", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider crackleSlider { "CRACKLE", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider wowSlider { "WOW", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider flutterSlider { "FLUTTER", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider shuffleSeedSlider { "SHUFFLE SEED", juce::Slider::RotaryVerticalDrag };

    juce::Rectangle<int> textureGroupBounds;
    int page = 0;

    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sliderAttachments;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> scaleLockAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> ghostOnAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> sustainHoldAttachment;
    std::unique_ptr<juce::ParameterAttachment> modeAttachment;
    std::unique_ptr<juce::ParameterAttachment> scaleAttachment;
    std::unique_ptr<juce::ParameterAttachment> tapeStopCurveAttachment;

    void resized() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SgraffitoEditor)
};
