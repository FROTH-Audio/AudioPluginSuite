#include "PluginEditor.h"

#include "BinaryData.h"
#include "SuiteBrand.h"
#include "SuiteSettings.h"

namespace
{
    // Owner findings (2026-11-24): more width, less height. GRID page: step grid top half,
    // controls below. LANES page: clever horizontal layout, smaller knobs. MUTATE: more
    // space for EVOLVE RATE pills.
    constexpr int editorWidth = 1200;
    constexpr int margin = 16;
    constexpr int headerHeight = 44;
    constexpr int footerHeight = 26;
    constexpr int groupGap = 8;
    constexpr int groupTitleHeight = SuiteUI::groupTitleHeight;
    constexpr int rulerHeight = 14;
    constexpr int rowGap = 6;
    constexpr int knobDiameter = SuiteUI::knobDiameter * 0.7f;
    constexpr int knobRowHeight = 92 * 0.7f + 12;  // caption + knob + value, 30% smaller knobs
    // Plan 22, Finding 2: the GRID row's fixed parts, so the strip and the ruler share one
    // geometry (the strip fills the row between the name and the five control cells).
    constexpr int laneNameWidth = 42;   // wide enough for "F#2" (the note-name label)
    constexpr int laneNameGap = 10;
    constexpr int gridControlWidth = 64 + 52 + 64 + 64 + 64;   // division, on, level, vel, note
    constexpr int laneViewInset = 6;                           // LaneView::paint's x reduction
    // Plan 22, Finding 6: the LANES page's pattern selector cell (caption + pill + padding).
    constexpr int selectorCellHeight = SuiteUI::captionHeight + SuiteUI::pillHeight + 8;
    // The trailing 8 covers reduced()'s top-and-bottom pair, so a group's inner height is
    // exactly the sum of its rows (the plan-20 rule: a mismatch cuts the last row off).
    constexpr int gridGroupHeight = groupTitleHeight + 4 + rulerHeight + 8
                                  + 8 * knobRowHeight + 7 * rowGap + 8;
    constexpr int editorHeight = margin * 2 + headerHeight + groupGap + gridGroupHeight
                               + groupGap + footerHeight;
    constexpr int trailHeight = 260;
    constexpr int simpleGroupHeight = groupTitleHeight + 4 + knobRowHeight + 4;

    const char* pageNames[] = { "GRID", "LANES", "MUTATE", "GROOVE", "MOD" };

    // The brand lockup (plan 22, Finding 1): the embedded artwork from the CMake LOCKUP.
    const juce::Image& brandLockup()
    {
        static const juce::Image image = juce::ImageCache::getFromMemory (BinaryData::lockup_groovemutator_png,
                                                                          BinaryData::lockup_groovemutator_pngSize);
        return image;
    }
}

GrooveMutatorEditor::LaneCard::LaneCard (GrooveMutatorProcessor& p, int trackIndex,
                                         juce::Colour accentColour)
    : track (trackIndex),
      view (p, trackIndex, accentColour)
{
    name.setJustificationType (juce::Justification::centredRight);
    name.setFont (SuiteUI::uiFont (13.0f, true));
    name.setColour (juce::Label::textColourId, SuiteUI::Colours::textDim);
    name.setInterceptsMouseClicks (false, false);

    divisionCtrl.setOptions ({ "1/32", "1/16T", "1/16", "1/8T", "1/8", "1/4", "1/2", "1 Bar" });
    patternCtrl.setOptions ({ "Euclid", "Hash", "Steps" });

    auto& apvts = p.apvts;

    // The name starts from the default note; the editor's timer keeps it in step.
    const int initialNote = juce::roundToInt (apvts.getRawParameterValue (
        GrooveMutatorProcessor::trackNoteIds[track])->load());
    name.setText (juce::MidiMessage::getMidiNoteName (initialNote, true, true, 4),
                  juce::dontSendNotification);
    noteAttachment  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, GrooveMutatorProcessor::trackNoteIds[track], noteCtrl.slider);
    velAttachment   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, GrooveMutatorProcessor::trackVelIds[track], velCtrl.slider);
    levelAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, GrooveMutatorProcessor::trackGainIds[track], levelCtrl.slider);
    onAttachment    = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts, GrooveMutatorProcessor::trackOnIds[track], onToggle);
    SuiteUI::attachDropdown (divisionCtrl, *apvts.getParameter (GrooveMutatorProcessor::trackDivisionIds[track]), divisionAttachment);
    SuiteUI::attachSelector (patternCtrl, *apvts.getParameter (GrooveMutatorProcessor::trackPatternIds[track]), patternAttachment);
    stepsAttachment    = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, GrooveMutatorProcessor::trackStepsIds[track], stepsCtrl.slider);
    pulsesAttachment   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, GrooveMutatorProcessor::trackPulsesIds[track], pulsesCtrl.slider);
    rotationAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, GrooveMutatorProcessor::trackRotationIds[track], rotationCtrl.slider);
    accentAttachment   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, GrooveMutatorProcessor::trackAccentIds[track], accentCtrl.slider);
    flamAttachment     = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, GrooveMutatorProcessor::trackFlamIds[track], flamCtrl.slider);
    ghostAttachment    = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, GrooveMutatorProcessor::trackGhostIds[track], ghostCtrl.slider);
}

GrooveMutatorEditor::GrooveMutatorEditor (GrooveMutatorProcessor& p)
    : AudioProcessorEditor (&p),
      processor (p)
{
    setLookAndFeel (&lookAndFeel);

    // The bar exists before setSize(), because setSize() runs resized(), which lays it out.
    presetBar.reset (new SuitePresets::PresetBar (static_cast<SuitePresets::StateHolder&> (processor),
                                                  juce::Colour (0xffdcf014)));

    // The eight lane cards before setSize() as well: the first resized() walks them.
    for (int track = 0; track < GrooveMutatorProcessor::numTracks; ++track)
        lanes[(size_t) track] = std::make_unique<LaneCard> (processor, track, juce::Colour (0xffdcf014));

    setSize (editorWidth, editorHeight);
    setResizable (true, true);
    setResizeLimits (editorWidth - 60, editorHeight - 60, 1200, 1200);

    // The lane widgets into the tree.
    for (int track = 0; track < GrooveMutatorProcessor::numTracks; ++track)
    {
        auto& card = *lanes[(size_t) track];
        addAndMakeVisible (card.view);
        addAndMakeVisible (card.name);
        addAndMakeVisible (card.noteCtrl);
        addAndMakeVisible (card.velCtrl);
        addAndMakeVisible (card.levelCtrl);
        addAndMakeVisible (card.onToggle);
        addAndMakeVisible (card.divisionCtrl);
        addAndMakeVisible (card.patternCtrl);
        addAndMakeVisible (card.stepsCtrl);
        addAndMakeVisible (card.pulsesCtrl);
        addAndMakeVisible (card.rotationCtrl);
        addAndMakeVisible (card.accentCtrl);
        addAndMakeVisible (card.flamCtrl);
        addAndMakeVisible (card.ghostCtrl);

        SuiteUI::describe (card.view, "The lane's pattern. A filled box fires, a brighter box is an accent, a ring a ghost, a hollow box a fill, and a hole a flam. In Steps mode, click cells to paint the pattern.");
        SuiteUI::describe (card.noteCtrl, "The GM note the lane sends.");
        SuiteUI::describe (card.velCtrl, "The lane's velocity share.");
        SuiteUI::describe (card.levelCtrl, "The lane's level stage: it scales the notes' velocity contribution.");
        SuiteUI::describe (card.onToggle, "The lane plays while it is on.");
        SuiteUI::describe (card.divisionCtrl, "The lane's own step length.");
        SuiteUI::describe (card.patternCtrl, "Euclid spreads the pulses evenly; Hash uses the seeded pattern; Steps paints the strip directly.");
        SuiteUI::describe (card.stepsCtrl, "The pattern length, in steps.");
        SuiteUI::describe (card.pulsesCtrl, "The hit count. Euclid spreads it; Hash reads it as density.");
        SuiteUI::describe (card.rotationCtrl, "Shifts the Euclid pattern around the grid.");
        SuiteUI::describe (card.accentCtrl, "The share of accent hits (louder).");
        SuiteUI::describe (card.flamCtrl, "The share of flammed hits (a second, quieter hit a few milliseconds later).");
        SuiteUI::describe (card.ghostCtrl, "The share of ghost hits (quieter).");
    }

    // MUTATE page.
    SuiteUI::describe (trailPlot, "The attractor's phase-space trail. The mutation tick feeds from it.");
    SuiteUI::describe (chaosCtrl, "How hard the patterns mutate on each evolve tick.");
    SuiteUI::describe (mutateScaleCtrl, "Picks the mutation-operator tier: small shifts first, whole re-shapes at 100 %.");
    SuiteUI::describe (seedCtrl, "The determinism anchor. A change re-derives every lane's pattern.");
    SuiteUI::describe (evolveRateCtrl, "How often the mutation tick fires, in musical time.");
    SuiteUI::describe (freezeToggle, "Locks the current patterns: the mutation tick stops touching them.");
    SuiteUI::describe (fillProbCtrl, "The chance of a fill on the cycle's last bars.");
    SuiteUI::describe (fillStyleCtrl, "The fill's shape: a tom roll, a snare build, a hat drop or a chaos burst.");
    SuiteUI::describe (fillBarsCtrl, "How much of the cycle the fill can occupy.");

    // GROOVE page.
    SuiteUI::describe (swingCtrl, "Moves every second step late (or, negative, early).");
    SuiteUI::describe (humaniseTimeCtrl, "Random timing jitter, deterministic from the seed.");
    SuiteUI::describe (humaniseVelCtrl, "Random velocity jitter, deterministic from the seed.");
    SuiteUI::describe (velCurveCtrl, "Shapes the velocity across each pattern.");
    SuiteUI::describe (polyRatioCtrl, "Metric modulation: scales every lane's division.");
    SuiteUI::describe (syncToggle, "On: the host's position and tempo drive the groove. Off: the Tempo parameter.");
    SuiteUI::describe (tempoCtrl, "The tempo while Host Sync is off.");
    SuiteUI::describe (triggerModeCtrl, "Latch starts the groove on one press; Gate runs only while the key is held.");
    SuiteUI::describe (triggerKeyCtrl, "The key that starts and stops the groove.");
    SuiteUI::describe (pageSelector, "GRID: the eight lanes. LANES: pattern and articulation. MUTATE: chaos and fills. GROOVE: the feel and the clock. MOD: the LFOs and routing.");
    SuiteUI::describe (*presetBar, "Factory presets, INIT and SAVE. The menu lists user presets.");
    SuiteUI::describe (headerMenu, "Settings... and About...");

    // The master selectors: the option lists must exist before the attachments read them.
    evolveRateCtrl.setOptions ({ "per Beat", "per Bar", "2 Bars", "4 Bars", "8 Bars" });
    fillStyleCtrl.setOptions ({ "Tom Roll", "Snare Build", "Hat Drop", "Chaos Burst" });
    fillBarsCtrl.setOptions ({ "1 Bar", "2 Bars", "4 Bars" });
    velCurveCtrl.setOptions ({ "Flat", "Linear", "Exponential", "Random" });
    polyRatioCtrl.setOptions ({ "1:1", "3:4", "5:4", "7:8", "4:3", "8:7" });
    triggerModeCtrl.setOptions ({ "Latch", "Gate" });

    // Attachments for the master widgets.
    auto& apvts = processor.apvts;
    chaosAttachment      = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, GrooveMutatorProcessor::chaosId, chaosCtrl.slider);
    mutateScaleAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, GrooveMutatorProcessor::mutateScaleId, mutateScaleCtrl.slider);
    seedAttachment       = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, GrooveMutatorProcessor::seedId, seedCtrl.slider);
    SuiteUI::attachSelector (evolveRateCtrl, *apvts.getParameter (GrooveMutatorProcessor::evolveRateId), evolveRateAttachment);
    freezeAttachment     = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts, GrooveMutatorProcessor::freezeId, freezeToggle);
    fillProbAttachment   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, GrooveMutatorProcessor::fillProbId, fillProbCtrl.slider);
    SuiteUI::attachSelector (fillStyleCtrl, *apvts.getParameter (GrooveMutatorProcessor::fillStyleId), fillStyleAttachment);
    SuiteUI::attachSelector (fillBarsCtrl, *apvts.getParameter (GrooveMutatorProcessor::fillBarsId), fillBarsAttachment);

    swingAttachment        = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, GrooveMutatorProcessor::swingId, swingCtrl.slider);
    humaniseTimeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, GrooveMutatorProcessor::humaniseTimeId, humaniseTimeCtrl.slider);
    humaniseVelAttachment  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, GrooveMutatorProcessor::humaniseVelocityId, humaniseVelCtrl.slider);
    SuiteUI::attachSelector (velCurveCtrl, *apvts.getParameter (GrooveMutatorProcessor::velCurveId), velCurveAttachment);
    SuiteUI::attachSelector (polyRatioCtrl, *apvts.getParameter (GrooveMutatorProcessor::polyRatioId), polyRatioAttachment);
    syncAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts, GrooveMutatorProcessor::syncId, syncToggle);
    tempoAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, GrooveMutatorProcessor::tempoId, tempoCtrl.slider);
    SuiteUI::attachSelector (triggerModeCtrl, *apvts.getParameter (GrooveMutatorProcessor::triggerModeId), triggerModeAttachment);
    triggerKeyAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, GrooveMutatorProcessor::triggerNoteId, triggerKeyCtrl.slider);

    // Each control that the routing can move, and the target it shows (plan 19 Phase 4).
    using Target = GrooveMutatorProcessor::ModTarget;
    std::vector<SuiteModulation::ControlDisplay::Entry> displayEntries;
    for (int track = 0; track < GrooveMutatorProcessor::numTracks; ++track)
        displayEntries.push_back ({ lanes[(size_t) track]->velCtrl.slider,
                                    (int) Target::TrackVel1 + track });
    displayEntries.push_back ({ chaosCtrl.slider,        (int) Target::Chaos });
    displayEntries.push_back ({ mutateScaleCtrl.slider,  (int) Target::MutationScale });
    displayEntries.push_back ({ fillProbCtrl.slider,     (int) Target::FillProbability });
    displayEntries.push_back ({ swingCtrl.slider,        (int) Target::Swing });
    displayEntries.push_back ({ humaniseTimeCtrl.slider, (int) Target::HumaniseTime });
    displayEntries.push_back ({ humaniseVelCtrl.slider,  (int) Target::HumaniseVelocity });
    controls = std::make_unique<SuiteModulation::ControlDisplay> (processor.getRouter(),
                                                                  std::move (displayEntries));

    // The help line (plan 12, R14a).
    helpLine.setIdleText ("Eight mutated percussion lanes behind one trigger key. Play the trigger key to start the groove.");
    helpLine.setComponentID ("groovemutator-help");
    helpLine.setEnablePredicate ([] { return SuiteSettings::shared().getBool (SuiteSettings::tooltipsKey, true); });
    SuiteUI::attachHelpLine (*this, helpLine, helpTracker);
    addAndMakeVisible (helpLine);


    // MUTATE and GROOVE widgets into the tree.
    for (auto* component : { static_cast<juce::Component*> (&trailPlot),
                             static_cast<juce::Component*> (&chaosCtrl),
                             static_cast<juce::Component*> (&mutateScaleCtrl),
                             static_cast<juce::Component*> (&seedCtrl),
                             static_cast<juce::Component*> (&evolveRateCtrl),
                             static_cast<juce::Component*> (&freezeToggle),
                             static_cast<juce::Component*> (&fillProbCtrl),
                             static_cast<juce::Component*> (&fillStyleCtrl),
                             static_cast<juce::Component*> (&fillBarsCtrl),
                             static_cast<juce::Component*> (&swingCtrl),
                             static_cast<juce::Component*> (&humaniseTimeCtrl),
                             static_cast<juce::Component*> (&humaniseVelCtrl),
                             static_cast<juce::Component*> (&velCurveCtrl),
                             static_cast<juce::Component*> (&polyRatioCtrl),
                             static_cast<juce::Component*> (&syncToggle),
                             static_cast<juce::Component*> (&tempoCtrl),
                             static_cast<juce::Component*> (&triggerModeCtrl),
                             static_cast<juce::Component*> (&triggerKeyCtrl),
                             static_cast<juce::Component*> (&pageSelector),
                             static_cast<juce::Component*> (&modulationPanel) })
        addAndMakeVisible (component);

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

    if (presetBar->getPreferredHeight() > 0)
        addAndMakeVisible (*presetBar);
    presetBar->onPresetLoaded = [this] { repaint(); };

    // R8: the page switch in the header row, left of the preset bar.
    pageSelector.setOptions ({ pageNames[0], pageNames[1], pageNames[2], pageNames[3], pageNames[4] });
    pageSelector.onChange = [this] (int index) { setPage (index); };

    setPage (0);
    startTimerHz (SuiteUI::kRefreshHz);
}

GrooveMutatorEditor::~GrooveMutatorEditor()
{
    stopTimer();
    removeMouseListener (&helpTracker);
    setLookAndFeel (nullptr);
}

SuiteModulation::ModulationPanel::Style GrooveMutatorEditor::modulationStyle()
{
    return { { juce::Colour (0xff252525), juce::Colour (0xff252525) },
             SuiteUI::Colours::text, SuiteUI::Colours::textDim,
             SuiteUI::Colours::panel, SuiteUI::Colours::track };
}

void GrooveMutatorEditor::setPage (int newPage)
{
    currentPage = juce::jlimit (0, 4, newPage);
    pageSelector.setSelectedIndex (currentPage);

    for (auto& card : lanes)
    {
        card->view.setVisible (currentPage == 0);
        card->name.setVisible (currentPage == 0 || currentPage == 1);
        card->noteCtrl.setVisible (currentPage == 0);
        card->velCtrl.setVisible (currentPage == 0);
        card->levelCtrl.setVisible (currentPage == 0);
        card->onToggle.setVisible (currentPage == 0);
        card->divisionCtrl.setVisible (currentPage == 0);

        card->patternCtrl.setVisible (currentPage == 1);
        card->stepsCtrl.setVisible (currentPage == 1);
        card->pulsesCtrl.setVisible (currentPage == 1);
        card->rotationCtrl.setVisible (currentPage == 1);
        card->accentCtrl.setVisible (currentPage == 1);
        card->flamCtrl.setVisible (currentPage == 1);
        card->ghostCtrl.setVisible (currentPage == 1);
    }

    trailPlot.setVisible (currentPage == 2);
    chaosCtrl.setVisible (currentPage == 2);
    mutateScaleCtrl.setVisible (currentPage == 2);
    seedCtrl.setVisible (currentPage == 2);
    evolveRateCtrl.setVisible (currentPage == 2);
    freezeToggle.setVisible (currentPage == 2);
    fillProbCtrl.setVisible (currentPage == 2);
    fillStyleCtrl.setVisible (currentPage == 2);
    fillBarsCtrl.setVisible (currentPage == 2);

    swingCtrl.setVisible (currentPage == 3);
    humaniseTimeCtrl.setVisible (currentPage == 3);
    humaniseVelCtrl.setVisible (currentPage == 3);
    velCurveCtrl.setVisible (currentPage == 3);
    polyRatioCtrl.setVisible (currentPage == 3);
    syncToggle.setVisible (currentPage == 3);
    tempoCtrl.setVisible (currentPage == 3);
    triggerModeCtrl.setVisible (currentPage == 3);
    triggerKeyCtrl.setVisible (currentPage == 3);

    modulationPanel.setVisible (currentPage == 4);

    updateAffinity();
    resized();
    repaint();
}

void GrooveMutatorEditor::paint (juce::Graphics& g)
{
    juce::ColourGradient background (SuiteUI::Colours::panelLight, 0.0f, 0.0f,
                                     SuiteUI::Colours::background, 0.0f, (float) getHeight(), false);
    g.setGradientFill (background);
    g.fillAll();

    // Header: the brand lockup (plan 22, Finding 1; the embedded LOCKUP artwork).
    SuiteUI::drawBrandLockup (g, getLocalBounds().removeFromTop (headerHeight).reduced (16, 0),
                              brandLockup());

    // The group frames per page (R14b).
    if (currentPage == 0)
        SuiteUI::drawGroupFrame (g, gridBounds, "STEP GRID", lookAndFeel.getAccent());
    else if (currentPage == 1)
    {
        // One framed column per lane (plan 22, Finding 6): eight channel strips, each with
        // the lane label and note name, the pattern selector and the six knobs.
        for (int track = 0; track < GrooveMutatorProcessor::numTracks; ++track)
            SuiteUI::drawGroupFrame (g, laneBounds[(size_t) track],
                                     "LANE " + juce::String (track + 1), lookAndFeel.getAccent());
    }
    else if (currentPage == 2)
    {
        SuiteUI::drawGroupFrame (g, chaosBounds, "CHAOS", lookAndFeel.getAccent());
        SuiteUI::drawGroupFrame (g, fillBounds, "FILL", lookAndFeel.getAccent());
    }
    else if (currentPage == 3)
    {
        SuiteUI::drawGroupFrame (g, grooveBounds, "GROOVE", lookAndFeel.getAccent());
        SuiteUI::drawGroupFrame (g, clockBounds, "CLOCK & TRIGGER", lookAndFeel.getAccent());
    }
    else
        SuiteUI::drawGroupFrame (g, modBounds, "MODULATION", lookAndFeel.getAccent());

    // The shared step ruler above the GRID rows: ticks every 4 steps, numbers at them.
    if (currentPage == 0 && rulerBounds.getWidth() > 0)
    {
        g.setFont (SuiteUI::uiFont (9.0f));
        for (int step = 0; step <= CoreDSP::Rhythm::maxSteps; step += 4)
        {
            const float x = rulerBounds.getX()
                          + rulerBounds.getWidth() * (float) step / (float) CoreDSP::Rhythm::maxSteps;
            g.setColour (SuiteUI::Colours::track);
            g.drawVerticalLine ((int) x, rulerBounds.getY(), (float) rulerBounds.getBottom());

            if (step > 0 && step < CoreDSP::Rhythm::maxSteps)
            {
                g.setColour (SuiteUI::Colours::textDim);
                g.drawText (juce::String (step + 1),
                            (int) x - 20, rulerBounds.getY(), 40, rulerBounds.getHeight(),
                            juce::Justification::centred);
            }
        }
    }
}

void GrooveMutatorEditor::resized()
{
    auto area = getLocalBounds().reduced (margin);

    // Header: menu at the far right, the preset bar flat to its left, then the page switch.
    auto headerArea = area.removeFromTop (headerHeight);
    headerMenu.setBounds (headerArea.removeFromRight (26).withHeight (26)
                            .withY (headerArea.getCentreY() - 13));
    const int barHeight = presetBar != nullptr ? presetBar->getPreferredHeight() : 0;
    if (barHeight > 0)
        presetBar->setBounds (headerArea.removeFromRight (300).withHeight (barHeight)
                                .withY (headerArea.getCentreY() - barHeight / 2));
    pageSelector.setBounds (headerArea.removeFromRight (300).withHeight (SuiteUI::pillHeight)
                                .withY (headerArea.getCentreY() - SuiteUI::pillHeight / 2));

    if (currentPage == 0)
    {
        gridBounds = area.removeFromTop (gridGroupHeight);

        auto inner = gridBounds.withTrimmedTop (groupTitleHeight + 4).reduced (10, 4);

        // The shared step ruler above the eight lane rows; each row carries the lane's
        // strip on the left and its controls on the right. The group height is the sum of
        // these rows (gridGroupHeight), so nothing is clipped.
        rulerBounds = inner.removeFromTop (rulerHeight);
        // Plan 22, Finding 2: the ruler spans exactly the strips' cells - the same left
        // inset (name + gap) and right control block as every row, minus LaneView's 6 px.
        rulerBounds = { rulerBounds.getX() + laneNameWidth + laneNameGap + laneViewInset,
                        rulerBounds.getY(),
                        rulerBounds.getWidth() - laneNameWidth - laneNameGap
                            - gridControlWidth - 2 * laneViewInset,
                        rulerHeight };
        inner.removeFromTop (8);   // spacer between the ruler and the first row

        for (int track = 0; track < GrooveMutatorProcessor::numTracks; ++track)
        {
            auto& card = *lanes[(size_t) track];
            auto row = inner.removeFromTop (knobRowHeight);

            // From the right: division, on, level, velocity, note; the strip gets the rest.
            auto cell = row.removeFromRight (64).reduced (3);  // smaller cells
            card.divisionCtrl.setBounds (cell);
            cell = row.removeFromRight (52).reduced (3);  // toggle
            card.onToggle.setBounds (cell.withSizeKeepingCentre (cell.getWidth(), SuiteUI::pillHeight));
            cell = row.removeFromRight (64).reduced (3);
            card.levelCtrl.setBounds (cell);
            cell = row.removeFromRight (64).reduced (3);
            card.velCtrl.setBounds (cell);
            cell = row.removeFromRight (64).reduced (3);
            card.noteCtrl.setBounds (cell);

            // Plan 22, Finding 2: the strip fills the row between the name and the controls
            // (it used to be carved out of the row, leaving a dead gutter on its left).
            card.name.setBounds (row.removeFromLeft (laneNameWidth));
            row.removeFromLeft (laneNameGap);
            card.view.setBounds (row);

            if (track < GrooveMutatorProcessor::numTracks - 1)
                inner.removeFromTop (rowGap);
        }
    }
    else if (currentPage == 1)
    {
        // Plan 22, Finding 6: eight columns, one framed card per lane. Every card is the
        // page's full height: the label band, the pattern selector, then the six knobs
        // evenly spread down the column (a channel strip). The cards stop above the
        // footer band, exactly where the GRID group ends on page 0.
        area.removeFromBottom (footerHeight);
        constexpr int numLanes = GrooveMutatorProcessor::numTracks;
        const int cardWidth = (area.getWidth() - (numLanes - 1) * groupGap) / numLanes;

        for (int track = 0; track < numLanes; ++track)
        {
            auto& card = *lanes[(size_t) track];
            auto cardArea = area.removeFromLeft (cardWidth);
            laneBounds[(size_t) track] = cardArea;

            // The note name rides in the title band, right of the "LANE n" caption (the
            // timer keeps it in step with the Note parameter).
            card.name.setBounds (cardArea.withHeight (groupTitleHeight + 4).translated (0, 4)
                                          .removeFromRight (44).reduced (4, 0));

            auto inner = cardArea.withTrimmedTop (groupTitleHeight + 4).reduced (4, 0)
                                 .withTrimmedBottom (8);

            card.patternCtrl.setBounds (inner.removeFromTop (selectorCellHeight).reduced (2, 0));

            // Six knobs, one per row, with the spare height shared between the rows.
            constexpr int rows = 6;
            const int spare = inner.getHeight() - rows * knobRowHeight;
            const int gap = juce::jmax (rowGap, spare / rows);
            inner.removeFromTop (gap);

            SuiteUI::LabelledSlider* knobRows[rows] =
                { &card.stepsCtrl, &card.pulsesCtrl, &card.rotationCtrl,
                  &card.accentCtrl, &card.flamCtrl, &card.ghostCtrl };

            for (int row = 0; row < rows; ++row)
            {
                knobRows[row]->setBounds (inner.removeFromTop (knobRowHeight));

                if (row < rows - 1)
                    inner.removeFromTop (gap);
            }

            if (track < numLanes - 1)
                area.removeFromLeft (groupGap);
        }
    }
    else if (currentPage == 2)
    {
        trailBounds = area.removeFromTop (trailHeight);
        trailPlot.setBounds (trailBounds);

        area.removeFromTop (groupGap);
        chaosBounds = area.removeFromTop (simpleGroupHeight);
        area.removeFromTop (groupGap);
        fillBounds = area.removeFromTop (simpleGroupHeight);

        {
            auto inner = chaosBounds.withTrimmedTop (groupTitleHeight + 4).reduced (10, 4);
            auto row = inner.removeFromTop (knobRowHeight);
            auto cell = row.removeFromLeft (64);
            chaosCtrl.setBounds (cell);
            cell = row.removeFromLeft (64);
            mutateScaleCtrl.setBounds (cell);
            cell = row.removeFromLeft (64);
            seedCtrl.setBounds (cell);
            // Plan 22, Finding 7: FREEZE shares the knobs' cell size and sits beside them,
            // and its pill sits in the knob band (below the caption row), so the row reads
            // as one baseline instead of a small pill floating at the far right.
            cell = row.removeFromLeft (64);
            auto pillCell = cell.withTrimmedTop (SuiteUI::captionHeight);
            freezeToggle.setBounds (pillCell.withSizeKeepingCentre (pillCell.getWidth(), SuiteUI::pillHeight));
            // R14d: owner findings - more space for EVOLVE RATE pills
            auto cellWide = row.removeFromLeft (row.getWidth() - 20 - 15).reduced (3);
            evolveRateCtrl.setBounds (cellWide);
        }

        {
            auto inner = fillBounds.withTrimmedTop (groupTitleHeight + 4).reduced (10, 4);
            auto row = inner.removeFromTop (knobRowHeight);
            auto cell = row.removeFromLeft (64);
            fillProbCtrl.setBounds (cell);
            auto cellWide = row.removeFromLeft (row.getWidth() - 64 - 15 - 15).reduced (3);
            fillStyleCtrl.setBounds (cellWide);
            auto cell2 = row.removeFromLeft (row.getWidth() - 64 - cellWide.getWidth() - 15).reduced (3);
            fillBarsCtrl.setBounds (cell2);
        }
    }
    else if (currentPage == 3)
    {
        grooveBounds = area.removeFromTop (simpleGroupHeight);
        area.removeFromTop (groupGap);
        clockBounds = area.removeFromTop (simpleGroupHeight);

        {
            auto inner = grooveBounds.withTrimmedTop (groupTitleHeight + 4).reduced (10, 4);
            auto row = inner.removeFromTop (SuiteUI::knobRowHeight);
            auto cell = row.removeFromLeft (84);
            swingCtrl.setBounds (cell);
            cell = row.removeFromLeft (84);
            humaniseTimeCtrl.setBounds (cell);
            cell = row.removeFromLeft (84);
            humaniseVelCtrl.setBounds (cell);
            cell = row.removeFromLeft (320).reduced (3);
            velCurveCtrl.setBounds (cell);
            cell = row.removeFromLeft (170).reduced (3);
            polyRatioCtrl.setBounds (cell);
        }

        {
            auto inner = clockBounds.withTrimmedTop (groupTitleHeight + 4).reduced (10, 4);
            auto row = inner.removeFromTop (SuiteUI::knobRowHeight);
            auto cell = row.removeFromLeft (90).reduced (3);
            syncToggle.setBounds (cell.withSizeKeepingCentre (cell.getWidth(), SuiteUI::pillHeight));
            cell = row.removeFromLeft (84);
            tempoCtrl.setBounds (cell);
            cell = row.removeFromLeft (130).reduced (3);
            triggerModeCtrl.setBounds (cell);
            cell = row.removeFromLeft (84);
            triggerKeyCtrl.setBounds (cell);
        }
    }
    else
    {
        modBounds = area;
        modulationPanel.setBounds (modBounds.withTrimmedTop (groupTitleHeight + 4).reduced (12, 6));
    }

    // The help line lives in the footer band.
    helpLine.setBounds (getLocalBounds().removeFromBottom (footerHeight));
}

void GrooveMutatorEditor::timerCallback()
{
    modulationPanel.refresh();
    controls->update();

    // The trail and the live patterns.
    trailPlot.setTrail (processor.getTrail());
    trailPlot.repaint();
    for (auto& card : lanes)
        card->view.repaint();

    // The lane names follow the notes (the MTC/Sgraffito note-name pattern).
    for (auto& card : lanes)
    {
        const int note = juce::roundToInt (processor.apvts.getRawParameterValue (
            GrooveMutatorProcessor::trackNoteIds[(size_t) card->track])->load());
        const juce::String text = juce::MidiMessage::getMidiNoteName (note, true, true, 4);
        if (card->name.getText() != text)
            card->name.setText (text, juce::dontSendNotification);
    }

    updateAffinity();
}

void GrooveMutatorEditor::updateAffinity()
{
    // Per-mode affinity (plan 19 Phase 3, decision G1): dimmed, never hidden.
    for (auto& card : lanes)
    {
        const int kind = juce::jlimit (0, CoreDSP::Rhythm::patternKinds - 1,
                                       juce::roundToInt (processor.apvts.getRawParameterValue (
                                           GrooveMutatorProcessor::trackPatternIds[(size_t) card->track])->load()));
        card->pulsesCtrl.setEnabled (kind != 2);
        card->rotationCtrl.setEnabled (kind == 0);
    }

    tempoCtrl.setEnabled (processor.apvts.getRawParameterValue (GrooveMutatorProcessor::syncId)->load() < 0.5f);
}
