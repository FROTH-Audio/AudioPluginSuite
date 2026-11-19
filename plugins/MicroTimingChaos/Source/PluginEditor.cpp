#include "PluginEditor.h"

#include "BinaryData.h"
#include "SuiteBrand.h"
#include "SuiteSettings.h"

namespace
{
    namespace Palette = MicroTimingChaos::Palette;

    const juce::Image& brandLockup();

    // Native design size (plan 05: the ×2/3 scale factor is gone; the design is the window).
    constexpr int groupTitleHeight = 20;
    constexpr int titleBandHeight = 56;
    constexpr int helpHeight = 26;
    constexpr int readoutHeight = 24;

    // One gap for everything: the outer margin, and the space between every pair of groups (plan 05 R7).
    constexpr int groupGap = 8;

    constexpr int editorWidth = 980;
    constexpr int editorHeight = 700;
    constexpr int minEditorHeight = 660;

    /** The LabelledSlider widget centres the knob at the suite size; the cell just gives it room. */
    void placeKnob (juce::Component& knob, juce::Rectangle<int> cell)
    {
        knob.setBounds (cell);
    }

    /** Emphasised caption above a knob: bold and centred. */
    void emphasiseCaption (juce::Label& label)
    {
        label.setFont (SuiteUI::uiFont (13.0f, true));
        label.setJustificationType (juce::Justification::centred);
        label.setColour (juce::Label::textColourId, SuiteUI::Colours::textDim);
    }

    /** Splits an area into a grid of cells, with a little room around each one. */
    std::vector<juce::Rectangle<int>> gridCells (juce::Rectangle<int> area, int count, int columns)
    {
        std::vector<juce::Rectangle<int>> cells;
        const int rows = (count + columns - 1) / columns;
        const int cellWidth = area.getWidth() / columns;
        const int cellHeight = area.getHeight() / juce::jmax (1, rows);

        for (int i = 0; i < count; ++i)
        {
            const int column = i % columns;
            const int row = i / columns;
            cells.push_back (juce::Rectangle<int> (area.getX() + column * cellWidth, area.getY() + row * cellHeight,
                                                   cellWidth, cellHeight).reduced (3));
        }

        return cells;
    }

    /** The area inside a group frame, below its title. */
    juce::Rectangle<int> innerOf (juce::Rectangle<int> group)
    {
        return group.withTrimmedTop (groupTitleHeight + 4).reduced (10, 4);
    }

    void describe (juce::Component& component, const juce::String& help)
    {
        component.setHelpText (help);
    }

    /** Sets the same help text on a knob, its caption and its slider, so hovering any of them shows it. */
    void describe (SuiteUI::LabelledSlider& control, const juce::String& help)
    {
        control.setHelpText (help);
        control.label.setHelpText (help);
        control.slider.setHelpText (help);
        control.slider.setTooltip (help);
    }

    void styleCaption (juce::Label& label, const juce::String& text)
    {
        label.setText (text, juce::dontSendNotification);
        label.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
        label.setColour (juce::Label::textColourId, SuiteUI::Colours::textDim);
        label.setJustificationType (juce::Justification::centred);
    }
}

//==============================================================================
StepGridView::StepGridView (juce::Colour accentColour)
    : accent (accentColour)
{
}

void StepGridView::setState (int patternLength, int playhead, float timingRangeMs, std::vector<bool> fires,
                             std::vector<MicroTimingChaosProcessor::StepEvent> events)
{
    length = juce::jlimit (CoreDSP::Sequencer::minSteps, CoreDSP::Sequencer::maxSteps, patternLength);
    playheadStep = playhead;
    rangeMs = juce::jmax (1.0f, timingRangeMs);
    firing = std::move (fires);
    recent = std::move (events);
    repaint();
}

void StepGridView::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    constexpr float axisWidth = 58.0f;
    constexpr float numberHeight = 26.0f;

    // Panel: a soft vertical gradient
    g.setGradientFill (juce::ColourGradient (SuiteUI::Colours::panelLight, 0.0f, bounds.getY(),
                                             SuiteUI::Colours::panel, 0.0f, bounds.getBottom(), false));
    g.fillRoundedRectangle (bounds, 8.0f);

    const auto plot = bounds.withTrimmedLeft (axisWidth).withTrimmedBottom (numberHeight).reduced (8.0f, 10.0f);
    const float columnWidth = plot.getWidth() / (float) juce::jmax (1, length);
    const float halfHeight = plot.getHeight() * 0.5f;
    const float range = juce::jmax (1.0f, rangeMs);
    const float centreY = plot.getCentreY();

    auto yFor = [=] (float offsetMs) { return centreY - juce::jlimit (-1.0f, 1.0f, offsetMs / range) * halfHeight; };

    // Timing guides, with labels on the left: +range, 0 and -range
    g.setFont (juce::Font (juce::FontOptions (13.0f)));
    const float guides[] = { range, 0.0f, -range };
    for (const float value : guides)
    {
        const float y = yFor (value);
        g.setColour (value == 0.0f
            ? SuiteUI::Colours::track.brighter (0.35f)
            : SuiteUI::Colours::track.withAlpha (0.55f));
        g.drawLine (plot.getX(), y, plot.getRight(), y, value == 0.0f ? 1.5f : 1.0f);

        const juce::String text = value == 0.0f
            ? juce::String ("0")
            : juce::String (value > 0.0f ? "+" : "-") + juce::String (juce::roundToInt (std::abs (value))) + " ms";
        g.setColour (SuiteUI::Colours::textDim);
        g.drawText (text, juce::Rectangle<float> (bounds.getX() + 4.0f, y - 10.0f, axisWidth - 8.0f, 20.0f),
                    juce::Justification::centredRight);
    }

    // Steps: separators, the lit playhead column, the firing bars, and the step numbers
    for (int step = 0; step < length; ++step)
    {
        const auto column = juce::Rectangle<float> (plot.getX() + (float) step * columnWidth, plot.getY(),
                                                    columnWidth, plot.getHeight());
        const bool isPlayhead = step == playheadStep;
        const bool fires = step < (int) firing.size() && firing[(size_t) step];

        g.setColour (SuiteUI::Colours::track.withAlpha (0.35f));
        g.drawLine (column.getX(), plot.getY(), column.getX(), plot.getBottom(), 1.0f);

        if (isPlayhead)
        {
            const auto lit = column.reduced (2.0f, 0.0f);
            g.setGradientFill (juce::ColourGradient (accent.withAlpha (0.22f), lit.getCentreX(), plot.getY(),
                                                     accent.withAlpha (0.02f), lit.getCentreX(), plot.getBottom(), false));
            g.fillRoundedRectangle (lit, 6.0f);
            g.setColour (accent.withAlpha (0.8f));
            g.drawRoundedRectangle (lit, 6.0f, 2.0f);
        }

        if (fires)
        {
            const auto body = column.reduced (columnWidth * 0.18f, 0.0f);
            const float height = plot.getHeight() * 0.20f;
            const juce::Rectangle<float> bar (body.getX(), plot.getBottom() - height, body.getWidth(), height);
            g.setGradientFill (juce::ColourGradient (accent.withAlpha (0.55f), 0.0f, bar.getY(),
                                                     accent.withAlpha (0.10f), 0.0f, bar.getBottom(), false));
            g.fillRoundedRectangle (bar, 5.0f);
        }
        else
        {
            g.setColour (SuiteUI::Colours::track.brighter (0.2f));
            g.fillEllipse (column.getCentreX() - 3.0f, plot.getBottom() - 9.0f, 6.0f, 6.0f);
        }

        g.setColour (isPlayhead ? accent : SuiteUI::Colours::textDim);
        g.setFont (juce::Font (juce::FontOptions (14.0f, isPlayhead ? juce::Font::bold : juce::Font::plain)));
        g.drawText (juce::String (step + 1),
                    juce::Rectangle<float> (column.getX(), bounds.getBottom() - numberHeight, column.getWidth(), numberHeight),
                    juce::Justification::centred);
    }

    // Recent notes, oldest first: a trail through the notes, then each note as a glowing dot that fades with age
    const int count = (int) recent.size();
    juce::Point<float> previous;
    bool hasPrevious = false;

    for (int i = 0; i < count; ++i)
    {
        const auto& event = recent[(size_t) i];
        if (event.velocity <= 0.0f || event.patternStep < 0 || event.patternStep >= length)
        {
            hasPrevious = false;
            continue;
        }

        const float age = count > 1 ? (float) i / (float) (count - 1) : 1.0f;
        const float alpha = 0.2f + 0.8f * age;
        const juce::Point<float> point (plot.getX() + ((float) event.patternStep + 0.5f) * columnWidth, yFor (event.offsetMs));
        const float radius = 3.5f + event.velocity / 127.0f * 5.5f;

        if (hasPrevious)
        {
            g.setColour (accent.withAlpha (0.3f * alpha));
            g.drawLine (previous.x, previous.y, point.x, point.y, 1.5f);
        }

        g.setColour (accent.withAlpha (0.15f * alpha));
        g.fillEllipse (point.x - radius * 2.2f, point.y - radius * 2.2f, radius * 4.4f, radius * 4.4f);
        g.setColour (accent.withAlpha (alpha));
        g.fillEllipse (point.x - radius, point.y - radius, radius * 2.0f, radius * 2.0f);
        g.setColour (juce::Colours::white.withAlpha (0.35f * alpha));
        g.fillEllipse (point.x - radius * 0.35f, point.y - radius * 0.55f, radius * 0.5f, radius * 0.5f);

        previous = point;
        hasPrevious = true;
    }
}

//==============================================================================
MicroTimingChaosEditor::MicroTimingChaosEditor (MicroTimingChaosProcessor& p)
    : AudioProcessorEditor (&p),
      processor (p),
      curveEditor (p)
{
    setLookAndFeel (&lookAndFeel);

    // The bar exists before setSize(), because setSize() runs resized(), which lays the bar out.
    presetBar = std::make_unique<SuitePresets::PresetBar>(processor, juce::Colour(0xff7dff5a));

    setSize (editorWidth, editorHeight);
    setResizable (true, true);
    setResizeLimits (editorWidth - 40, minEditorHeight, 1400, 1000);

    pageSelector.setOptions ({ "SEQUENCE", "DRIFT", "MOD" });
    pageSelector.setFontHeight (12.0f);
    pageSelector.onChange = [this] (int index) { setPage (index); };

    helpTracker.onMove = [this] (const juce::MouseEvent& e) { showHelp (e); };
    addMouseListener (&helpTracker, true);

    helpLabel.setFont (juce::Font (juce::FontOptions (14.0f)));
    helpLabel.setColour (juce::Label::textColourId, SuiteUI::Colours::text);
    helpLabel.setText ("Hover a control to see what it does.", juce::dontSendNotification);

    readout.setJustificationType (juce::Justification::centredLeft);
    readout.setColour (juce::Label::textColourId, SuiteUI::Colours::text);
    readout.setFont (juce::Font (juce::FontOptions (14.0f, juce::Font::bold)));

    hostTempoLabel.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
    hostTempoLabel.setColour (juce::Label::textColourId, SuiteUI::Colours::textDim);
    hostTempoLabel.setJustificationType (juce::Justification::centred);

    styleCaption (scaleCaption, "SCALE");

    // Knob captions: all the same size and weight
    for (auto* caption : { &lengthSlider.label, &densitySlider.label, &seedSlider.label, &gateSlider.label,
                           &swingSlider.label, &velocitySlider.label, &rootSlider.label, &octavesSlider.label,
                           &chaosSlider.label, &timingRangeSlider.label, &velocityChaosSlider.label,
                           &lerpSlider.label, &lerpXMultSlider.label, &lerpYMultSlider.label, &lerpZMultSlider.label,
                           &tempoSlider.label, &speedSlider.label })
        emphasiseCaption (*caption);

    rateSelector.setFontHeight (15.0f);
    lerpViewSelector.setFontHeight (15.0f);

    for (auto* component : { static_cast<juce::Component*> (&helpLabel), static_cast<juce::Component*> (&attractor),
                              static_cast<juce::Component*> (&readout), static_cast<juce::Component*> (&grid),
                              static_cast<juce::Component*> (&lengthSlider), static_cast<juce::Component*> (&densitySlider),
                              static_cast<juce::Component*> (&seedSlider), static_cast<juce::Component*> (&gateSlider),
                              static_cast<juce::Component*> (&swingSlider), static_cast<juce::Component*> (&velocitySlider),
                              static_cast<juce::Component*> (&rootSlider), static_cast<juce::Component*> (&octavesSlider),
                              static_cast<juce::Component*> (&scaleCaption), static_cast<juce::Component*> (&scaleBox),
                              static_cast<juce::Component*> (&chaosSlider), static_cast<juce::Component*> (&timingRangeSlider),
                              static_cast<juce::Component*> (&velocityChaosSlider),
                              static_cast<juce::Component*> (&lerpViewSelector), static_cast<juce::Component*> (&curveEditor),
                              static_cast<juce::Component*> (&lerpPreview), static_cast<juce::Component*> (&modulation),
                              static_cast<juce::Component*> (&lerpSlider), static_cast<juce::Component*> (&lerpXMultSlider),
                              static_cast<juce::Component*> (&lerpYMultSlider), static_cast<juce::Component*> (&lerpZMultSlider),
                              static_cast<juce::Component*> (&tempoSlider), static_cast<juce::Component*> (&speedSlider),
                              static_cast<juce::Component*> (&rateSelector), static_cast<juce::Component*> (&syncButton),
                              static_cast<juce::Component*> (&hostTempoLabel),
                              static_cast<juce::Component*> (&pageSelector) })
        addAndMakeVisible (component);

    // Help text for every control, and tooltips on the knobs.
    describe (lengthSlider, "Pattern length in steps, 2 to 16.");
    describe (densitySlider, "How many steps fire. The seed decides which ones.");
    describe (seedSlider, "Seed for the fixed pattern and melody. The same seed gives the same pattern.");
    describe (gateSlider, "Note length as a share of a step. Lorenz Y lengthens or shortens it when Lerp is on.");
    describe (swingSlider, "Delays every second step.");
    describe (velocitySlider, "Base velocity of each note, before chaos.");
    describe (rootSlider, "Lowest melody note, as a MIDI note number.");
    describe (octavesSlider, "How many octaves the melody spans.");
    describe (scaleBox, "The scale the melody is chosen from.");
    describe (chaosSlider, "Chaos, 0 to 100 %. It is the input to the lerp curve. At 0 the pattern never moves.");
    describe (timingRangeSlider, "Maximum timing shift in ms, reached at the attractor's X extremes.");
    describe (velocityChaosSlider, "How strongly the attractor's Z moves velocity.");
    describe (lerpSlider, "How far each step drifts from the fixed pattern toward the attractor. 0 % turns the drift off.");
    describe (lerpXMultSlider, "Timing drift multiplier, 0 to 2x.");
    describe (lerpYMultSlider, "Pitch and gate drift multiplier, 0 to 2x.");
    describe (lerpZMultSlider, "Velocity drift multiplier, 0 to 2x.");
    describe (tempoSlider, "Tempo used when BPM Sync is off.");
    describe (speedSlider, "How fast the attractor moves, relative to the tempo.");
    describe (rateSelector, "Step length: quarter, eighth, sixteenth or thirty-second notes.");
    describe (syncButton, "BPM Sync: follow the host's tempo and transport.");
    curveEditor.setHelpText ("The curve maps chaos (0 to 1) to timing, pitch and velocity drift. "
                             "The multipliers scale each axis. Drag points; right-click a point to reset it, "
                             "double-click to reset the curve.");
    lerpPreview.setHelpText ("Live drift per axis, and the timing, pitch and velocity of the last steps.");
    lerpViewSelector.setHelpText ("Switch between the curve editor and the live preview.");

    rateSelector.setOptions ({ "1/4", "1/8", "1/16", "1/32" });
    rateAttachment = std::make_unique<juce::ParameterAttachment> (
        *processor.apvts.getParameter (MicroTimingChaosProcessor::rateId),
        [this] (float value) { rateSelector.setSelectedIndex (juce::roundToInt (value)); });
    rateSelector.onChange = [this] (int index) { rateAttachment->setValueAsCompleteGesture ((float) index); };
    rateAttachment->sendInitialUpdate();

    lerpViewSelector.setOptions ({ "CURVE", "PREVIEW" });
    lerpViewSelector.setSelectedIndex (0);
    lerpViewSelector.onChange = [this] (int index)
    {
        curveEditor.setVisible (index == 0);
        lerpPreview.setVisible (index == 1);
    };
    lerpPreview.setVisible (false);

    scaleBox.addItemList ({ "Chromatic", "Major", "Minor", "Minor Pentatonic", "Dorian", "Whole Tone" }, 1);
    scaleBox.setTextWhenNothingSelected ("Scale");

    if (presetBar->getPreferredHeight() > 0)
        addAndMakeVisible (*presetBar);
    presetBar->onPresetLoaded = [this] { curveEditor.repaint(); };

    addAndMakeVisible (headerMenu);
    headerMenu.onAbout = [this]
    {
        SuiteUI::showAboutDialog (brandLockup(), SUITE_PLUGIN_NAME, SUITE_PLUGIN_VERSION,
                                  SUITE_PLUGIN_CODE, SUITE_PLUGIN_ID, this);
    };
    headerMenu.onSettings = [this]
    {
        SuiteSettings::showSettingsDialog (SuiteSettings::shared(), this);
    };

    hostTempoLabel.setText ("FREE", juce::dontSendNotification);

    // Units and formatting
    lengthSlider.slider.setNumDecimalPlacesToDisplay (0);
    densitySlider.slider.setTextValueSuffix (" %");
    seedSlider.slider.setNumDecimalPlacesToDisplay (0);
    gateSlider.slider.setTextValueSuffix (" %");
    swingSlider.slider.setTextValueSuffix (" %");
    velocitySlider.slider.setNumDecimalPlacesToDisplay (0);
    rootSlider.slider.setNumDecimalPlacesToDisplay (0);
    octavesSlider.slider.setNumDecimalPlacesToDisplay (0);
    chaosSlider.slider.setTextValueSuffix (" %");
    velocityChaosSlider.slider.setTextValueSuffix (" %");
    lerpSlider.slider.setTextValueSuffix (" %");
    lerpXMultSlider.slider.setTextValueSuffix (" x");
    lerpYMultSlider.slider.setTextValueSuffix (" x");
    lerpZMultSlider.slider.setTextValueSuffix (" x");
    tempoSlider.slider.setTextValueSuffix (" BPM");

    auto& apvts = processor.apvts;

    auto bind = [this, &apvts] (const char* id, juce::Slider& slider)
    {
        sliderAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, id, slider));
    };

    bind (MicroTimingChaosProcessor::lengthId, lengthSlider.slider);
    bind (MicroTimingChaosProcessor::densityId, densitySlider.slider);
    bind (MicroTimingChaosProcessor::seedId, seedSlider.slider);
    bind (MicroTimingChaosProcessor::gateId, gateSlider.slider);
    bind (MicroTimingChaosProcessor::swingId, swingSlider.slider);
    bind (MicroTimingChaosProcessor::baseVelocityId, velocitySlider.slider);
    bind (MicroTimingChaosProcessor::rootId, rootSlider.slider);
    bind (MicroTimingChaosProcessor::octavesId, octavesSlider.slider);
    bind (MicroTimingChaosProcessor::chaosId, chaosSlider.slider);
    bind (MicroTimingChaosProcessor::timingRangeId, timingRangeSlider.slider);
    bind (MicroTimingChaosProcessor::velocityChaosId, velocityChaosSlider.slider);
    bind (MicroTimingChaosProcessor::lerpId, lerpSlider.slider);
    bind (MicroTimingChaosProcessor::lerpXMultId, lerpXMultSlider.slider);
    bind (MicroTimingChaosProcessor::lerpYMultId, lerpYMultSlider.slider);
    bind (MicroTimingChaosProcessor::lerpZMultId, lerpZMultSlider.slider);
    bind (MicroTimingChaosProcessor::bpmId, tempoSlider.slider);
    bind (MicroTimingChaosProcessor::speedId, speedSlider.slider);

    scaleAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        apvts, MicroTimingChaosProcessor::scaleId, scaleBox);
    syncAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        apvts, MicroTimingChaosProcessor::syncId, syncButton);

    // Each control that the routing can move, and the target it shows. Whole-number targets are rounded by the router.
    using Target = MicroTimingChaosProcessor::ModTarget;
    controls = std::make_unique<SuiteModulation::ControlDisplay> (processor.getRouter(),
        std::vector<SuiteModulation::ControlDisplay::Entry> {
            { chaosSlider.slider,         (int) Target::Chaos },
            { timingRangeSlider.slider,   (int) Target::TimingRange },
            { velocityChaosSlider.slider, (int) Target::VelocityChaos },
            { lerpSlider.slider,          (int) Target::Lerp },
            { lerpXMultSlider.slider,     (int) Target::LerpX },
            { lerpYMultSlider.slider,     (int) Target::LerpY },
            { lerpZMultSlider.slider,     (int) Target::LerpZ },
            { densitySlider.slider,       (int) Target::Density },
            { gateSlider.slider,          (int) Target::Gate },
            { swingSlider.slider,         (int) Target::Swing },
            { velocitySlider.slider,      (int) Target::Velocity },
            { speedSlider.slider,         (int) Target::Speed },
            { tempoSlider.slider,         (int) Target::Tempo },
            { seedSlider.slider,          (int) Target::Seed },
        });

    // The first page lays everything out (the ctor set the curve tab visible above).
    startTimerHz (SuiteUI::kRefreshHz);
    setPage (0);
}

MicroTimingChaosEditor::~MicroTimingChaosEditor()
{
    stopTimer();
    removeMouseListener (&helpTracker);
    setLookAndFeel (nullptr);
}

void MicroTimingChaosEditor::showHelp (const juce::MouseEvent& e)
{
    if (auto* component = e.eventComponent)
    {
        const auto text = component->getHelpText();

        if (text.isNotEmpty())
            helpLabel.setText (text, juce::dontSendNotification);
    }
}

void MicroTimingChaosEditor::setPage (int newPage)
{
    page = juce::jlimit (0, 2, newPage);
    pageSelector.setSelectedIndex (page);

    const bool sequencePage = page == 0;
    const bool driftPage = page == 1;
    const bool modPage = page == 2;

    // The lerp view state decides which of the two views shows on the DRIFT page, so the
    // first open shows the curve, not whichever view sits on top (plan 05.1 bug fix).
    const bool showCurve = driftPage && lerpViewSelector.getSelectedIndex() == 0;
    const bool showPreview = driftPage && lerpViewSelector.getSelectedIndex() == 1;

    for (auto* component : { static_cast<juce::Component*> (&lengthSlider), static_cast<juce::Component*> (&densitySlider),
                              static_cast<juce::Component*> (&seedSlider), static_cast<juce::Component*> (&gateSlider),
                              static_cast<juce::Component*> (&swingSlider), static_cast<juce::Component*> (&velocitySlider),
                              static_cast<juce::Component*> (&grid),
                              static_cast<juce::Component*> (&rootSlider), static_cast<juce::Component*> (&octavesSlider),
                              static_cast<juce::Component*> (&scaleCaption), static_cast<juce::Component*> (&scaleBox),
                              static_cast<juce::Component*> (&tempoSlider), static_cast<juce::Component*> (&speedSlider),
                              static_cast<juce::Component*> (&rateSelector), static_cast<juce::Component*> (&syncButton),
                              static_cast<juce::Component*> (&hostTempoLabel) })
        component->setVisible (sequencePage);

    for (auto* component : { static_cast<juce::Component*> (&attractor), static_cast<juce::Component*> (&readout),
                              static_cast<juce::Component*> (&lerpViewSelector), static_cast<juce::Component*> (&curveEditor),
                              static_cast<juce::Component*> (&lerpPreview),
                              static_cast<juce::Component*> (&lerpSlider), static_cast<juce::Component*> (&lerpXMultSlider),
                              static_cast<juce::Component*> (&lerpYMultSlider), static_cast<juce::Component*> (&lerpZMultSlider),
                              static_cast<juce::Component*> (&chaosSlider), static_cast<juce::Component*> (&timingRangeSlider),
                              static_cast<juce::Component*> (&velocityChaosSlider) })
        component->setVisible (driftPage);

    curveEditor.setVisible (showCurve);
    lerpPreview.setVisible (showPreview);

    for (auto* component : { static_cast<juce::Component*> (&modulation) })
        component->setVisible (modPage);

    resized();
    repaint();
}

namespace
{
    const juce::Image& brandLockup()
    {
        static const juce::Image image = juce::ImageCache::getFromMemory (BinaryData::lockup_micro_timing_chaos_png,
                                                                         BinaryData::lockup_micro_timing_chaos_pngSize);
        return image;
    }
}

void MicroTimingChaosEditor::setLerpView (int index)
{
    lerpViewSelector.setSelectedIndex (index);
    curveEditor.setVisible (index == 0);
    lerpPreview.setVisible (index == 1);
}

void MicroTimingChaosEditor::paint (juce::Graphics& g)
{
    lookAndFeel.paint (g, *this);

    // Title band: a flat background, so the grid texture does not run through the title
    g.setColour (SuiteUI::Colours::background);
    g.fillRect (0, 0, getWidth(), titleBandHeight);

    const auto header = getLocalBounds().reduced (groupGap, 0).removeFromTop (titleBandHeight - 8).toFloat();

    SuiteUI::drawBrandLockup (g, header.toNearestInt(), brandLockup());

    // Every group title has the same colour and size; groups on other pages are not drawn.
    for (const auto& group : groups)
    {
        if (group.page != page)
            continue;

        const auto bounds = group.bounds.toFloat();

        g.setColour (SuiteUI::Colours::panel.withAlpha (0.55f));
        g.fillRoundedRectangle (bounds, 8.0f);
        g.setColour (SuiteUI::Colours::track);
        g.drawRoundedRectangle (bounds, 8.0f, 1.0f);

        g.setColour (lookAndFeel.getAccent());
        g.setFont (juce::Font (juce::FontOptions (15.0f, juce::Font::bold)));
        g.drawText (group.title, group.bounds.withHeight (groupTitleHeight).translated (0, 4).reduced (12, 0),
                    juce::Justification::centredLeft);
    }
}

void MicroTimingChaosEditor::resized()
{
    groups.clear();

    // Reserve space for the preset bar (360x34, right-aligned in the title band). The window
    // height does not change.
    auto titleBand = getLocalBounds().removeFromTop (titleBandHeight);
    const int barWidth = 360;
    const int barHeight = 34;

    // The menu button sits at the far right of the header, right of the preset bar (plan 08 Q4).
    headerMenu.setBounds (titleBand.removeFromRight (28)
                            .withSizeKeepingCentre (28, 28));

    if (presetBar != nullptr && presetBar->getPreferredHeight() > 0)
        presetBar->setBounds (titleBand.removeFromRight (barWidth).withHeight (barHeight)
                                .withY (titleBand.getCentreY() - barHeight / 2));

    pageSelector.setBounds (titleBand.removeFromRight (240)
                                .withSizeKeepingCentre (240, SuiteUI::pillHeight));

    // Every gap is groupGap: the outer margin, and the space between groups in both directions.
    auto content = getLocalBounds().withTrimmedTop (titleBandHeight).reduced (groupGap);
    helpLabel.setBounds (content.removeFromBottom (helpHeight));
    content.removeFromBottom (groupGap);

    const int x0 = content.getX();
    const int width = content.getWidth();

    // Page 0 (SEQUENCE): the pattern, then melody and transport, the step grid takes the rest
    // (plan 05.1 pass 2: melody and transport moved here from the MOD page).
    if (page == 0)
    {
        const juce::Rectangle<int> patternGroup (x0, content.getY(), width,
                                                 groupTitleHeight + 2 * SuiteUI::knobRowHeight + groupGap + 10);
        const int rowHeight = groupTitleHeight + SuiteUI::knobRowHeight + 20;
        const juce::Rectangle<int> melodyGroup (x0, patternGroup.getBottom() + groupGap, width / 2 - groupGap / 2, rowHeight);
        const juce::Rectangle<int> transportGroup (melodyGroup.getRight() + groupGap, melodyGroup.getY(),
                                                   width - melodyGroup.getWidth() - groupGap, rowHeight);
        const juce::Rectangle<int> sequencerGroup (x0, melodyGroup.getBottom() + groupGap, width,
                                                   content.getBottom() - melodyGroup.getBottom() - groupGap);

        groups.push_back ({ "PATTERN", patternGroup, 0 });
        groups.push_back ({ "MELODY", melodyGroup, 0 });
        groups.push_back ({ "TRANSPORT", transportGroup, 0 });
        groups.push_back ({ "SEQUENCER", sequencerGroup, 0 });

        grid.setBounds (innerOf (sequencerGroup));

        const auto patternCells = gridCells (innerOf (patternGroup), 6, 3);
        placeKnob (lengthSlider, patternCells[0]);
        placeKnob (densitySlider, patternCells[1]);
        placeKnob (seedSlider, patternCells[2]);
        placeKnob (gateSlider, patternCells[3]);
        placeKnob (swingSlider, patternCells[4]);
        placeKnob (velocitySlider, patternCells[5]);

        // Melody: root, octaves, and the scale box.
        const auto melodyCells = gridCells (innerOf (melodyGroup), 3, 3);
        placeKnob (rootSlider, melodyCells[0]);
        placeKnob (octavesSlider, melodyCells[1]);
        auto scaleCell = melodyCells[2];
        scaleCaption.setBounds (scaleCell.removeFromTop (18));
        scaleBox.setBounds (scaleCell.withSizeKeepingCentre (scaleCell.getWidth(), SuiteUI::pillHeight));

        // Transport: tempo and speed knobs on the left; on the right, the rate switches with
        // BPM Sync below them, and the host tempo readout underneath.
        auto transportInner = innerOf (transportGroup);
        auto knobArea = transportInner.removeFromLeft (juce::roundToInt ((float) transportInner.getWidth() * 0.4f));
        placeKnob (tempoSlider, knobArea.removeFromLeft (knobArea.getWidth() / 2));
        placeKnob (speedSlider, knobArea);
        transportInner.removeFromLeft (groupGap);

        auto pillRows = transportInner.removeFromTop (2 * SuiteUI::pillHeight + 2);
        rateSelector.setBounds (pillRows.removeFromTop (SuiteUI::pillHeight)
                                    .withSizeKeepingCentre (150, SuiteUI::pillHeight));
        pillRows.removeFromTop (2);
        syncButton.setBounds (pillRows.removeFromTop (SuiteUI::pillHeight)
                                  .withSizeKeepingCentre (150, SuiteUI::pillHeight));
        hostTempoLabel.setBounds (transportInner.removeFromTop (readoutHeight).withSizeKeepingCentre (200, readoutHeight));
        return;
    }

    // Page 1 (DRIFT): the lerp group fills the full height on the left (curve or preview on
    // top, the four lerp knobs below it); the attractor sits above the chaos group in the
    // right column. The chaos group is only as tall as its knob row, so its knobs sit on the
    // same Y as the lerp knobs at the bottom of the lerp group (plan 05.1 pass 2).
    if (page == 1)
    {
        const int rightWidth = juce::roundToInt (content.getWidth() * 0.40f);
        const int leftWidth = content.getWidth() - rightWidth - groupGap;
        const int chaosHeight = groupTitleHeight + SuiteUI::knobRowHeight + 10;
        const int attractorHeight = content.getHeight() - chaosHeight - groupGap;

        const juce::Rectangle<int> lerpGroup (x0, content.getY(), leftWidth, content.getHeight());
        const juce::Rectangle<int> attractorGroup (lerpGroup.getRight() + groupGap, content.getY(),
                                                   rightWidth, attractorHeight);
        const juce::Rectangle<int> chaosGroup (lerpGroup.getRight() + groupGap, attractorGroup.getBottom() + groupGap,
                                               rightWidth, chaosHeight);

        groups.push_back ({ "LERP CURVE", lerpGroup, 1 });
        groups.push_back ({ "ATTRACTOR VISUALIZATION", attractorGroup, 1 });
        groups.push_back ({ "CHAOS", chaosGroup, 1 });

        auto attractorInner = innerOf (attractorGroup);
        readout.setBounds (attractorInner.removeFromBottom (readoutHeight));
        attractor.setBounds (attractorInner);

        // Lerp curve: the tabbed view spans the group width; the four knobs sit in one row
        // below it.
        auto lerpInner = innerOf (lerpGroup);
        lerpViewSelector.setBounds (lerpInner.removeFromTop (SuiteUI::pillHeight).withWidth (240));
        lerpInner.removeFromTop (groupGap);
        auto lerpKnobs = lerpInner.removeFromBottom (SuiteUI::knobRowHeight);
        curveEditor.setBounds (lerpInner);
        lerpPreview.setBounds (lerpInner);

        const auto lerpCells = gridCells (lerpKnobs, 4, 4);
        placeKnob (lerpSlider, lerpCells[0]);
        placeKnob (lerpXMultSlider, lerpCells[1]);
        placeKnob (lerpYMultSlider, lerpCells[2]);
        placeKnob (lerpZMultSlider, lerpCells[3]);

        const auto chaosCells = gridCells (innerOf (chaosGroup), 3, 3);
        placeKnob (chaosSlider, chaosCells[0]);
        placeKnob (timingRangeSlider, chaosCells[1]);
        placeKnob (velocityChaosSlider, chaosCells[2]);
        return;
    }

    // Page 2 (MOD): the LFO and routing panel takes the whole page (melody and transport
    // moved to SEQUENCE, plan 05.1 pass 2).
    const juce::Rectangle<int> modGroup (x0, content.getY(), width, content.getHeight());

    groups.push_back ({ "MODULATION", modGroup, 2 });

    modulation.setBounds (modGroup.withTrimmedTop (groupTitleHeight + 4).reduced (10, 4));
}

SuiteModulation::ModulationPanel::Style MicroTimingChaosEditor::modulationStyle()
{
    namespace Palette = MicroTimingChaos::Palette;

    return { { Palette::lfo1, Palette::lfo2 },
             SuiteUI::Colours::text, SuiteUI::Colours::textDim,
             SuiteUI::Colours::panel, SuiteUI::Colours::track };
}

void MicroTimingChaosEditor::timerCallback()
{
    attractor.setTrail (processor.getTrail());

    const auto& apvts = processor.apvts;
    const int lengthValue = juce::roundToInt (apvts.getRawParameterValue (MicroTimingChaosProcessor::lengthId)->load());
    // The pattern shown is the one the sequencer plays: the modulated seed and density, with the same density curve.
    using Target = MicroTimingChaosProcessor::ModTarget;
    const auto seedValue = (std::uint32_t) juce::roundToInt (processor.getModulatedValue (Target::Seed, MicroTimingChaosProcessor::seedId));
    const float densityValue = CoreDSP::Sequencer::densityCurve (
        processor.getModulatedValue (Target::Density, MicroTimingChaosProcessor::densityId) / 100.0f);
    const float timingValue = processor.getModulatedValue (Target::TimingRange, MicroTimingChaosProcessor::timingRangeId);
    const int rootValue = juce::roundToInt (apvts.getRawParameterValue (MicroTimingChaosProcessor::rootId)->load());

    std::vector<bool> fires;
    for (int step = 0; step < CoreDSP::Sequencer::maxSteps; ++step)
        fires.push_back (step < lengthValue && CoreDSP::Sequencer::stepFires (seedValue, step, densityValue));

    const int playhead = processor.getPlayheadStep();
    grid.setState (lengthValue, playhead, timingValue, fires, processor.getRecentSteps());

    readout.setText (playhead >= 0 ? "Playhead: step " + juce::String (playhead + 1) + " of " + juce::String (lengthValue)
                                   : juce::String ("Transport stopped. Press play, or turn off BPM Sync."),
                      juce::dontSendNotification);

    // The marker follows the modulated chaos, so it matches what the sequencer uses.
    curveEditor.setChaosValue (processor.getModulatedValue (MicroTimingChaosProcessor::ModTarget::Chaos, MicroTimingChaosProcessor::chaosId) / 100.0f);
    controls->update();
    lerpPreview.refresh (processor.getRecentSteps(), processor.getDriftAmounts(), rootValue, timingValue);

    // Tempo: follows the host when BPM Sync is on
    modulation.refresh();

    const bool synced = apvts.getRawParameterValue (MicroTimingChaosProcessor::syncId)->load() > 0.5f;
    tempoSlider.setEnabled (! synced);

    if (synced)
    {
        const auto hostBpm = processor.getHostBpm();
        hostTempoLabel.setText (hostBpm.hasValue() ? "HOST " + juce::String (*hostBpm, 1) + " BPM"
                                                   : juce::String ("NO HOST TEMPO"),
                                juce::dontSendNotification);
    }
    else
    {
        hostTempoLabel.setText ("FREE RUNNING", juce::dontSendNotification);
    }

    // Middle C is C3 here, so MIDI 60 reads C3.
}
