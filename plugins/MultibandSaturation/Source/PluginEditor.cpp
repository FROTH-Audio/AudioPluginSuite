#include "PluginEditor.h"

#include "SuiteBrand.h"
#include "BinaryData.h"
#include "SuiteSettings.h"

namespace
{
    // Plan 12: the owner asked for a narrower window. 760 keeps the SHAPER group's
    // five-knob row comfortable (~140 px cells) and still leaves the header free.
    constexpr int editorWidth = 760;
    constexpr int editorHeight = 860;   // plan 17 Phase 9: the SHAPER group grew to three knob rows (+100)

    // Same law as SuiteUI::LevelMeter / StereoLevelMeter (plan 13).
    constexpr float meterFloorDb = -60.0f;

    float unitInterval (float value, float low, float high) noexcept
    {
        return juce::jlimit (0.0f, 1.0f, (value - low) / (high - low));
    }
    constexpr int headerHeight = 44;
    constexpr int footerHeight = 26;
    constexpr int groupGap = 8;   // the suite's one-gap rule (plan 05 R7)
    constexpr int groupTitleHeight = SuiteUI::groupTitleHeight;
    // Plan 17 Phase 9: the SHAPER group grows to three knob rows (drive/fold/thermal,
    // sensitivity envelope and tones, then the mix family).
    constexpr int groupInnerHeight = 3 * SuiteUI::knobRowHeight + 2 * groupGap;
    constexpr int groupHeight = groupTitleHeight + 4 + groupInnerHeight + 4;
    // SPLIT is one row of content now (crossover + mode), so its frame is one row tall.
    constexpr int splitGroupHeight = groupTitleHeight + 4 + SuiteUI::knobRowHeight + 4;
    // The freed height goes to the visualiser (plan 12: at least 50 % higher than the
    // old 130). 242 is the tallest that leaves the one 8 px gap above the help band.
    constexpr int visualHeight = 242;

    const juce::Image& brandLockup()
    {
        static const juce::Image image = juce::ImageCache::getFromMemory (BinaryData::lockup_multiband_saturation_png,
                                                                          BinaryData::lockup_multiband_saturation_pngSize);
        return image;
    }
}

MultibandSaturationEditor::MultibandSaturationEditor (MultibandSaturationProcessor& p)
    : AudioProcessorEditor (&p),
      processor (p)
{
    setLookAndFeel (&lookAndFeel);

    // The bar exists before setSize(), because setSize() runs resized(), which lays the bar out.
    presetBar.reset (new SuitePresets::PresetBar (static_cast<SuitePresets::StateHolder&> (processor),
                                                  juce::Colour (0xffFF6B6B)));

    setSize (editorWidth, editorHeight);
    setResizable (true, true);
    setResizeLimits (editorWidth - 60, editorHeight - 60, 1200, 1200);

    // Rule #3 (plan 11): three options, so a pill.
    modeCell.setOptions ({ "Tube", "Tape", "Diode" });
    SuiteUI::attachSelector (modeCell, *processor.apvts.getParameter (MultibandSaturationProcessor::modeId), modeAttachment);

    // Page switch (plan 13): SOUND / MOD, following the SD/DS pattern.
    pageSelector.setOptions ({ "SOUND", "MOD" });
    pageSelector.onChange = [this] (int index) { setPage (index); };

    // Help text on every control (plan 12), written through the shared describe() helper
    // so the wrapper, the caption and the knob all answer a hover.
    SuiteUI::describe (crossoverCtrl, "The Linkwitz-Riley split point between the two bands.");
    SuiteUI::describe (driveLowCtrl, "Pre-gain of the low band's shaper.");
    SuiteUI::describe (driveHighCtrl, "Pre-gain of the high band's shaper.");
    SuiteUI::describe (foldLowCtrl, "The low band's wavefolder, after its shaper. 0 is off.");
    SuiteUI::describe (foldHighCtrl, "The high band's wavefolder, after its shaper. 0 is off.");
    SuiteUI::describe (thermalCtrl, "The thermal drift: a very slow follower sags the drive and wows the crossover with the signal. 0 is off.");
    SuiteUI::describe (toneLowCtrl, "Tilt around 1 kHz on the shaped low band.");
    SuiteUI::describe (toneHighCtrl, "Tilt around 1 kHz on the shaped high band.");
    SuiteUI::describe (sensitivityCtrl, "How far the envelopes push the drives.");
    SuiteUI::describe (attackCtrl, "Envelope rise, both bands.");
    SuiteUI::describe (releaseCtrl, "Envelope fall, both bands.");
    SuiteUI::describe (mixLowCtrl, "The low band's processed signal against its clean band. 100 % is fully processed.");
    SuiteUI::describe (mixHighCtrl, "The high band's processed signal against its clean band. 100 % is fully processed.");
    SuiteUI::describe (mixCtrl, "Dry/wet balance.");
    SuiteUI::describe (outputCtrl, "Output level after the mix.");
    SuiteUI::describe (modeCell, "The saturation curve, shared by both bands.");
    SuiteUI::describe (visualizer, "The two band envelopes with the crossover marked between them, and the input/output meters.");
    SuiteUI::describe (pageSelector, "SOUND: the band controls. MOD: the two LFOs and the six routing slots.");
    pageSelector.setComponentID ("multiband-pages");
    SuiteUI::describe (*presetBar, "Factory presets, INIT and SAVE. The menu lists user presets.");
    SuiteUI::describe (headerMenu, "Settings... and About...");

    // Each control that the routing can move, and the target it shows (plan 13).
    using Target = MultibandSaturationProcessor::ModTarget;
    controls = std::make_unique<SuiteModulation::ControlDisplay> (processor.getRouter(),
        std::vector<SuiteModulation::ControlDisplay::Entry> {
            { crossoverCtrl.slider,    (int) Target::Crossover },
            { driveLowCtrl.slider,     (int) Target::DriveLow },
            { driveHighCtrl.slider,    (int) Target::DriveHigh },
            { sensitivityCtrl.slider,  (int) Target::Sensitivity },
            { attackCtrl.slider,       (int) Target::Attack },
            { releaseCtrl.slider,      (int) Target::Release },
            { toneLowCtrl.slider,      (int) Target::ToneLow },
            { toneHighCtrl.slider,     (int) Target::ToneHigh },
            { mixCtrl.slider,          (int) Target::Mix },
            { outputCtrl.slider,       (int) Target::Output },
        });

    // The help line (plan 12): the footer band, the stale-hover rule and the settings'
    // tooltips toggle, shared by every plugin in the suite.
    helpLine.setIdleText ("Two bands, each with its own drive and tone. The bands sum back to the input's level.");
    helpLine.setComponentID ("multiband-help");
    helpLine.setEnablePredicate ([] { return SuiteSettings::shared().getBool (SuiteSettings::tooltipsKey, true); });
    SuiteUI::attachHelpLine (*this, helpLine, helpTracker);
    addAndMakeVisible (helpLine);

    for (auto* component : { static_cast<juce::Component*> (&crossoverCtrl),
                             static_cast<juce::Component*> (&driveLowCtrl),
                             static_cast<juce::Component*> (&driveHighCtrl),
                             static_cast<juce::Component*> (&foldLowCtrl),
                             static_cast<juce::Component*> (&foldHighCtrl),
                             static_cast<juce::Component*> (&thermalCtrl),
                             static_cast<juce::Component*> (&toneLowCtrl),
                             static_cast<juce::Component*> (&toneHighCtrl),
                             static_cast<juce::Component*> (&sensitivityCtrl),
                             static_cast<juce::Component*> (&attackCtrl),
                             static_cast<juce::Component*> (&releaseCtrl),
                             static_cast<juce::Component*> (&mixLowCtrl),
                             static_cast<juce::Component*> (&mixHighCtrl),
                             static_cast<juce::Component*> (&mixCtrl),
                             static_cast<juce::Component*> (&outputCtrl),
                             static_cast<juce::Component*> (&modeCell),
                             static_cast<juce::Component*> (&visualizer),
                             static_cast<juce::Component*> (&pageSelector),
                             static_cast<juce::Component*> (&modulation) })
        addAndMakeVisible (component);

    addAndMakeVisible (headerMenu);

    // Menu (R13): About… and Settings…, the same two actions every suite plugin has.
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

    setPage (0);
    startTimerHz (SuiteUI::kRefreshHz);
}

MultibandSaturationEditor::~MultibandSaturationEditor()
{
    stopTimer();
    removeMouseListener (&helpTracker);
    setLookAndFeel (nullptr);
}

SuiteModulation::ModulationPanel::Style MultibandSaturationEditor::modulationStyle()
{
    return { { juce::Colour (0xffFF6B6B), juce::Colour (0xffFF6B6B) },
             SuiteUI::Colours::text, SuiteUI::Colours::textDim,
             SuiteUI::Colours::panel, SuiteUI::Colours::track };
}

void MultibandSaturationEditor::setPage (int newPage)
{
    currentPage = juce::jlimit (0, 1, newPage);
    pageSelector.setSelectedIndex (currentPage);

    for (auto* component : { static_cast<juce::Component*> (&crossoverCtrl),
                             static_cast<juce::Component*> (&driveLowCtrl),
                             static_cast<juce::Component*> (&driveHighCtrl),
                             static_cast<juce::Component*> (&foldLowCtrl),
                             static_cast<juce::Component*> (&foldHighCtrl),
                             static_cast<juce::Component*> (&thermalCtrl),
                             static_cast<juce::Component*> (&toneLowCtrl),
                             static_cast<juce::Component*> (&toneHighCtrl),
                             static_cast<juce::Component*> (&sensitivityCtrl),
                             static_cast<juce::Component*> (&attackCtrl),
                             static_cast<juce::Component*> (&releaseCtrl),
                             static_cast<juce::Component*> (&mixLowCtrl),
                             static_cast<juce::Component*> (&mixHighCtrl),
                             static_cast<juce::Component*> (&mixCtrl),
                             static_cast<juce::Component*> (&outputCtrl),
                             static_cast<juce::Component*> (&modeCell) })
        component->setVisible (currentPage == 0);

    modulation.setVisible (currentPage == 1);

    resized();
    repaint();
}

void MultibandSaturationEditor::paint (juce::Graphics& g)
{
    juce::ColourGradient background (SuiteUI::Colours::panelLight, 0.0f, 0.0f,
                                     SuiteUI::Colours::background, 0.0f, (float) getHeight(), false);
    g.setGradientFill (background);
    g.fillAll();

    // Brand lockup at the left of the header (the FROTH Audio wordmark, the mark and
    // the product name, generated by the owner's tools/brand/lockups.py (not in this repo) and embedded in the bundle).
    SuiteUI::drawBrandLockup (g, getLocalBounds().removeFromTop (headerHeight).reduced (16, 0), brandLockup());

    if (currentPage == 0)
    {
        // The two group frames (plan 12: the owner's missing backgrounds, now the shared frame).
        SuiteUI::drawGroupFrame (g, splitGroupBounds, "SPLIT", lookAndFeel.getAccent());
        SuiteUI::drawGroupFrame (g, shaperGroupBounds, "SHAPER", lookAndFeel.getAccent());
    }
    else
    {
        SuiteUI::drawGroupFrame (g, shaperGroupBounds, "MODULATION", lookAndFeel.getAccent());
    }
}

void MultibandSaturationEditor::resized()
{
    auto area = getLocalBounds().reduced (16);

    // Header: menu at the far right, the preset bar flat to its left (plan 05.1).
    auto headerArea = area.removeFromTop (headerHeight);
    headerMenu.setBounds (headerArea.removeFromRight (26).withHeight (26)
                            .withY (headerArea.getCentreY() - 13));
    const int barHeight = presetBar != nullptr ? presetBar->getPreferredHeight() : 0;
    if (barHeight > 0)
        presetBar->setBounds (headerArea.removeFromRight (300).withHeight (barHeight)
                                .withY (headerArea.getCentreY() - barHeight / 2));

    // Page switch (plan 15, owner): left of the preset bar, in the header row (R8, as on
    // StereoTool). The 28 px row it occupied below the header goes back to the pages.
    pageSelector.setBounds (headerArea.removeFromRight (160).withHeight (SuiteUI::pillHeight)
                                .withY (headerArea.getCentreY() - SuiteUI::pillHeight / 2));

    if (currentPage == 0)
        layoutSoundPage (area);
    else
        layoutModPage (area);

    // The help line lives in the band the paint used to draw its footer sentence in.
    helpLine.setBounds (getLocalBounds().removeFromBottom (footerHeight));
}

void MultibandSaturationEditor::layoutSoundPage (juce::Rectangle<int> area)
{
    // The dual-band visual spans the window between the header and the groups (plan 11 4b).
    visualBounds = area.removeFromTop (visualHeight);
    visualizer.setBounds (visualBounds);

    area.removeFromTop (groupGap);

    splitGroupBounds = area.removeFromTop (splitGroupHeight);
    area.removeFromTop (groupGap);
    shaperGroupBounds = area.removeFromTop (groupHeight);

    // SPLIT: the crossover and the shared saturation mode, one row now (plan 12).
    {
        auto inner = splitGroupBounds.withTrimmedTop (groupTitleHeight + 4).reduced (10, 4);
        const int cellWidth = inner.getWidth() / 4;
        auto row = inner.removeFromTop (SuiteUI::knobRowHeight);
        crossoverCtrl.setBounds (row.removeFromLeft (cellWidth).reduced (3));
        modeCell.setBounds (row.removeFromLeft (cellWidth).reduced (3));
    }

    // SHAPER: drive and fold per band, the thermal drift, the envelope and tones, then the
    // mix family (per-band mix first, plan 17 Phase 9).
    {
        auto inner = shaperGroupBounds.withTrimmedTop (groupTitleHeight + 4).reduced (10, 4);

        auto row1 = inner.removeFromTop (SuiteUI::knobRowHeight);
        const int cellWidth1 = row1.getWidth() / 5;
        for (auto* knob : { &driveLowCtrl, &driveHighCtrl, &foldLowCtrl, &foldHighCtrl, &thermalCtrl })
            knob->setBounds (row1.removeFromLeft (cellWidth1));

        auto row2 = inner.removeFromTop (SuiteUI::knobRowHeight);
        const int cellWidth2 = row2.getWidth() / 5;
        for (auto* knob : { &sensitivityCtrl, &attackCtrl, &releaseCtrl, &toneLowCtrl, &toneHighCtrl })
            knob->setBounds (row2.removeFromLeft (cellWidth2));

        auto row3 = inner.removeFromTop (SuiteUI::knobRowHeight);
        const int cellWidth3 = row3.getWidth() / 4;
        for (auto* knob : { &mixLowCtrl, &mixHighCtrl, &mixCtrl, &outputCtrl })
            knob->setBounds (row3.removeFromLeft (cellWidth3));
    }
}

void MultibandSaturationEditor::layoutModPage (juce::Rectangle<int> area)
{
    // The visualiser stays visible above the modulation panel.
    visualBounds = area.removeFromTop (juce::jmax (150, (int) ((float) area.getHeight() * 0.30f)));
    visualizer.setBounds (visualBounds);

    area.removeFromTop (groupGap);

    // The modulation panel fills the remaining area.
    shaperGroupBounds = area;
    modulation.setBounds (area.withTrimmedTop (groupTitleHeight + 4).reduced (12, 6));
}

void MultibandSaturationEditor::timerCallback()
{
    modulation.refresh();
    controls->update();
    visualizer.repaint();
}

//==============================================================================
MultibandSaturationEditor::DualBandVisualizer::DualBandVisualizer (MultibandSaturationProcessor& p)
    : processor (p)
{
}

void MultibandSaturationEditor::DualBandVisualizer::paint (juce::Graphics& g)
{
    const auto& snapshot = processor.getBandSnapshot();
    auto full = getLocalBounds().toFloat().reduced (4.0f);
    const auto accent = dynamic_cast<SuiteUI::SuiteLookAndFeel*> (&getLookAndFeel()) != nullptr
                            ? dynamic_cast<SuiteUI::SuiteLookAndFeel&> (getLookAndFeel()).getAccent()
                            : SuiteUI::Colours::text;

    g.setColour (SuiteUI::Colours::panel);
    g.fillRoundedRectangle (full, 6.0f);
    g.setColour (SuiteUI::Colours::panelEdge);
    g.drawRoundedRectangle (full, 6.0f, 1.0f);

    // The dB scale keeps its narrow strip at the left edge (plan 13's scale request).
    auto centerArea = full;
    auto leftTicks = centerArea.removeFromLeft (10.0f);

    // The band bars fill the whole centre height (minus the Hz axis strip at the bottom).
    auto bars = centerArea.withTrimmedBottom (24.0f).reduced (14.0f, 8.0f);
    const float barWidth = bars.getWidth() * 0.28f;
    auto lowArea = bars.withX (bars.getX() + bars.getWidth() * 0.10f).withWidth (barWidth);
    auto highArea = bars.withX (bars.getX() + bars.getWidth() * 0.62f).withWidth (barWidth);

    // The grid (plan 15, owner): frequency left to right AND level bottom to top — dim
    // lines behind the bars, on the same log axis and dB law the scales use.
    g.setColour (SuiteUI::Colours::track);
    for (float hz : { 80.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 8000.0f })
    {
        const float ratio = std::log (hz / 80.0f) / std::log (8000.0f / 80.0f);
        g.drawVerticalLine ((int) (bars.getX() + ratio * bars.getWidth()), bars.getY(), bars.getBottom());
    }

    for (float db : { 0.0f, -6.0f, -12.0f, -24.0f, -60.0f })
    {
        const float y = bars.getBottom() - unitInterval (db, meterFloorDb, 0.0f) * bars.getHeight();
        g.drawHorizontalLine ((int) y, bars.getX(), bars.getRight());
    }

    // The dB dashes on the left edge, aligned with the level lines.
    g.setColour (SuiteUI::Colours::textDim);
    g.setFont (SuiteUI::uiFont (10.0f));

    for (float db : { 0.0f, -6.0f, -12.0f, -24.0f, -60.0f })
    {
        const float y = bars.getBottom() - unitInterval (db, meterFloorDb, 0.0f) * bars.getHeight();
        g.drawHorizontalLine ((int) y, leftTicks.getX(), leftTicks.getRight());
    }

    // The two band bars, low then high (plan 11 4b). The envelopes run 0..1 (the follower
    // output over a band that starts at full scale), mapped like the suite's meters.
    for (const int band : { 0, 1 })
    {
        const auto barArea = band == 0 ? lowArea : highArea;
        g.setColour (SuiteUI::Colours::track);
        g.fillRoundedRectangle (barArea, 3.0f);

        const float envelope = juce::jlimit (0.0f, 1.0f, snapshot.envelope[(size_t) band].load (std::memory_order_relaxed));
        auto filled = barArea.withTop (barArea.getBottom() - envelope * barArea.getHeight());
        g.setColour (accent);
        g.fillRoundedRectangle (filled, 3.0f);

        g.setColour (SuiteUI::Colours::textDim);
        g.setFont (SuiteUI::uiFont (11.0f));
        g.drawText (band == 0 ? "LOW" : "HIGH",
                    barArea.withTop (barArea.getBottom() + 2.0f).withHeight (14.0f),
                    juce::Justification::centred);
    }

    // The crossover marker between the bars, on a log scale across the band axis. Plan 18:
    // the marker follows the modulated value (the router's published offset for the
    // Crossover target), not the raw parameter, so a routed LFO moves the marker live.
    const float crossoverHz = juce::jlimit (80.0f, 8000.0f,
        processor.getModulatedValue (MultibandSaturationProcessor::ModTarget::Crossover));
    const float crossoverX = bars.getX() + std::log (crossoverHz / 80.0f) / std::log (8000.0f / 80.0f) * bars.getWidth();

    g.setColour (SuiteUI::Colours::text);
    g.drawVerticalLine ((int) crossoverX, bars.getY(), bars.getBottom());

    g.setColour (SuiteUI::Colours::textDim);
    g.setFont (SuiteUI::uiFont (10.0f));
    g.drawText (juce::String ((int) crossoverHz) + " Hz",
                juce::Rectangle<float> (crossoverX - 30.0f, bars.getY() - 14.0f, 60.0f, 12.0f),
                juce::Justification::centred);

    // Hz axis under the band bars (plan 13).
    auto hzAxis = centerArea.removeFromBottom (24.0f).reduced (14.0f, 0.0f);
    g.setColour (SuiteUI::Colours::textDim);
    g.setFont (SuiteUI::uiFont (10.0f));

    for (float hz : { 80.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 8000.0f })
    {
        const float ratio = std::log (hz / 80.0f) / std::log (8000.0f / 80.0f);
        const float x = hzAxis.getX() + ratio * hzAxis.getWidth();
        g.drawVerticalLine ((int) x, hzAxis.getY(), hzAxis.getCentreY());

        juce::String label = hz >= 1000.0f ? juce::String (hz / 1000.0f, 1) + "k" : juce::String ((int) hz);
        g.drawText (label,
                    juce::Rectangle<float> (x - 14.0f, hzAxis.getCentreY(), 28.0f, 12.0f),
                    juce::Justification::centred);
    }
}
