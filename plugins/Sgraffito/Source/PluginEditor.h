#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <memory>
#include <vector>

#include "PluginProcessor.h"
#include "SuiteHeaderMenu.h"
#include "SuiteLookAndFeel.h"
#include "SuiteWidgets.h"
#include "SuiteBrand.h"
#include "SuiteModulationPanel.h"
#include "SuiteModulationControls.h"

//==============================================================================
// Visual widgets for the Sgraffito editor. They read the processor only through
// its public API (getParamValue, sampleBuffer, voicePool), so they stay free of
// the editor's friendship.

/** Waveform display: sample envelope, the region the root key maps to, and Chop slice lines.

    The region maths mirrors the processor's note-on path exactly, through the shared
    `CoreDSP::Sampler` maps (plan 04). If the engine maps change, keep this in step. */
class SgraffitoWaveformView : public juce::Component
{
public:
    explicit SgraffitoWaveformView (SgraffitoProcessor& p) : processor (p) {}

    /** Called when the missing-sample overlay is clicked; the editor wires it to its
        browse flow (the FileChooser must stay alive on the editor, plan 07 P9). */
    std::function<void()> onLocateClicked;

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();

        g.setColour (SuiteUI::Colours::panel.withAlpha (0.55f));
        g.fillRoundedRectangle (bounds, SuiteUI::controlRadius);
        g.setColour (SuiteUI::Colours::track);
        g.drawRoundedRectangle (bounds, SuiteUI::controlRadius, 1.0f);

        const auto* sample = processor.sampleBuffer.load (std::memory_order_acquire);
        if (sample == nullptr || sample->getNumSamples() == 0)
        {
            g.setColour (SuiteUI::Colours::textDim);
            g.setFont (SuiteUI::uiFont (12.0f));
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
            const int sliceCount = juce::jlimit (2, CoreDSP::Sampler::maxRegions,
                                                 juce::roundToInt (processor.getParamValue (SgraffitoProcessor::sliceCountId)));
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
        g.setFont (SuiteUI::uiFont (10.0f));
        g.drawText (regionCaption (regionStart, regionEnd),
                    getLocalBounds().removeFromBottom (16).reduced (8, 0).toFloat(),
                    juce::Justification::centredRight);

        // Missing-sample overlay (plan 07 2.3): the current sample keeps playing, so
        // the waveform stays visible underneath and the report sits on top. Accent
        // border and text, not a colour-only signal (suite rule R6).
        if (processor.isSampleMissing())
        {
            g.setColour (SuiteUI::Colours::panel.withAlpha (0.62f));
            g.fillRoundedRectangle (bounds, SuiteUI::controlRadius);
            g.setColour (accent);
            g.drawRoundedRectangle (bounds, SuiteUI::controlRadius, 2.0f);

            auto title = bounds.withSizeKeepingCentre (juce::jmax (0.0f, bounds.getWidth() - 24.0f), 16.0f);

            g.setColour (SuiteUI::Colours::text);
            g.setFont (SuiteUI::uiFont (13.0f, true));
            g.drawText ("Sample missing - click to locate", title.translated (0.0f, -12.0f),
                        juce::Justification::centred);

            g.setColour (SuiteUI::Colours::textDim);
            g.setFont (SuiteUI::uiFont (10.0f));
            g.drawText (juce::File (processor.getMissingSamplePath()).getFileName() + " - not found",
                        title.translated (0.0f, 12.0f), juce::Justification::centred);
        }
    }

    void mouseDown (const juce::MouseEvent&) override
    {
        if (processor.isSampleMissing() && onLocateClicked)
            onLocateClicked();
    }

    juce::MouseCursor getMouseCursor() override
    {
        return processor.isSampleMissing() ? juce::MouseCursor (juce::MouseCursor::PointingHandCursor)
                                           : juce::MouseCursor();
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

//==============================================================================
class SgraffitoEditor : public juce::AudioProcessorEditor,
                        private juce::Timer
{
public:
    explicit SgraffitoEditor (SgraffitoProcessor&);
    ~SgraffitoEditor() override;

    void paint (juce::Graphics&) override;

    /** Switches between the sound (0), texture (1) and mod (2) pages. */
    void setPage (int page);
    int getPage() const noexcept { return page; }

private:
    void timerCallback() override;

    struct Group
    {
        juce::String title;
        juce::Rectangle<int> bounds;
        int page = 0;
    };

    void layoutSoundPage (juce::Rectangle<int> area);
    void layoutTexturePage (juce::Rectangle<int> area);
    void layoutModPage (juce::Rectangle<int> area);

    SgraffitoProcessor& processor;

    // Declared first so it is destroyed last, after every component that uses it.
    SuiteUI::SuiteLookAndFeel lookAndFeel { juce::Colour (0xff9aa0a6) };
    std::vector<Group> groups;

    // Preset bar (right of the header).
    std::unique_ptr<SuitePresets::PresetBar> presetBar;

    // Gear at the far right of the header: settings and about (plan 08).
    SuiteUI::HeaderMenuButton headerMenu { juce::Colour (0xff9aa0a6) };

    // Page switch.
    SuiteUI::SegmentedSelector pageSelector { juce::Colour (0xff9aa0a6) };
    int page = 0;

    // SOUND page controls.
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

    // Visual: sample waveform. The missing-sample overlay clicks back into the
    // editor's browse flow (the FileChooser lives on the editor, plan 07 P9).
    SgraffitoWaveformView waveformView { processor };

    // Playable keyboard: the very bottom of every page (plan 05.1).
    SuiteUI::PlayableKeyboardStrip keyboard;

    // TEXTURE page controls, grouped by meaning (plan 05.1).
    SuiteUI::LabelledSlider tapeStopTimeSlider { "TAPE STOP TIME", juce::Slider::RotaryVerticalDrag };
    SuiteUI::SegmentedSelector tapeStopCurveSelector { juce::Colour (0xff9aa0a6) };
    SuiteUI::LabelledSlider memoryAmountSlider { "MEMORY AMOUNT", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider memoryDecaySlider { "MEMORY DECAY", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider driftAmountSlider { "DRIFT AMOUNT", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider driftSpeedSlider { "DRIFT SPEED", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider layBackSlider { "LAY BACK", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider swingSlider { "SWING", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider humaniseSlider { "HUMANISE", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider shuffleSeedSlider { "SHUFFLE SEED", juce::Slider::RotaryVerticalDrag };
    juce::ToggleButton ghostOnButton { "GHOST ON" };
    SuiteUI::LabelledSlider ghostThresholdSlider { "GHOST THRESHOLD", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider ghostLevelSlider { "GHOST LEVEL", juce::Slider::RotaryVerticalDrag };
    SuiteUI::LabelledSlider ghostDelaySlider { "GHOST DELAY", juce::Slider::RotaryVerticalDrag };

    // MOD page: the LFO and routing panel. Cloud and swell/surface live on the TEXTURE page.
    SuiteModulation::ModulationPanel modulation { processor.getRouter(), modulationStyle() };
    std::unique_ptr<SuiteModulation::ControlDisplay> controls;  // built in the constructor, once the sliders exist
    SuiteUI::LabelledSlider sliceCountSlider { "SLICE COUNT", juce::Slider::RotaryVerticalDrag };
    
    // Sample loading button (top right of header, next to preset bar).
    juce::TextButton loadSampleButton { "LOAD SOUND" };
    std::unique_ptr<juce::FileChooser> sampleChooser;
    
    /** The LFO curve colours, in the slate accent. */
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

    /** The LFO curve colours, in the slate accent. */
    static SuiteModulation::ModulationPanel::Style modulationStyle();

    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sliderAttachments;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> scaleLockAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> ghostOnAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> sustainHoldAttachment;
    std::unique_ptr<juce::ParameterAttachment> modeAttachment;
    std::unique_ptr<juce::ParameterAttachment> scaleAttachment;
    std::unique_ptr<juce::ParameterAttachment> tapeStopCurveAttachment;

    void resized() override;
    
    void browseForSample();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SgraffitoEditor)
};
