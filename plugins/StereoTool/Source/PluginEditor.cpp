#include "PluginEditor.h"

#include "SuiteBrand.h"
#include "BinaryData.h"
#include "SuiteSettings.h"

namespace
{
    // Plan 15 (owner): the visual sits at the top, the meters are gone, and the default
    // height equals the minimum height — the layout sums to the window exactly.
    constexpr int headerHeight = 44;
    constexpr int footerHeight = 26;
    constexpr int groupGap = 8;   // the suite's one-gap rule (plan 05 R7)
    constexpr int groupTitleHeight = SuiteUI::groupTitleHeight;
    constexpr int groupInnerHeight = 2 * SuiteUI::knobRowHeight + groupGap;
    constexpr int groupHeight = groupTitleHeight + 4 + groupInnerHeight + 4;
    constexpr int previewMinimumHeight = 100;
    constexpr int editorWidth = 720;
    // Plan 17 Phase 8: the IMAGE group carries four knob rows (the interest features), so
    // its group is taller than the CHARACTER/MASTER groups.
    constexpr int imageGroupHeight = groupTitleHeight + 4
                                   + 4 * SuiteUI::knobRowHeight + 3 * groupGap + 4;
    // 16 top margin + header + gap + [preview + gap + group + gap + group] + footer + gap.
    constexpr int editorHeight = 16 + headerHeight + groupGap
                               + previewMinimumHeight + groupGap + groupHeight + groupGap + imageGroupHeight
                               + footerHeight + groupGap;

    // The LEVEL group (two knobs) shares its row with IMAGE, so it only needs two cells.
    constexpr int levelGroupWidth = 196;

    // Plan 15: the mid/side bars are twice as wide as before (owner finding, plan 13).
    constexpr float midSideBarsWidth = 100.0f;

    const juce::Image& brandLockup()
    {
        static const juce::Image image = juce::ImageCache::getFromMemory (BinaryData::lockup_stereo_tool_png,
                                                                          BinaryData::lockup_stereo_tool_pngSize);
        return image;
    }
}

StereoToolEditor::StereoToolEditor (StereoToolProcessor& p)
    : AudioProcessorEditor (&p),
      processor (p)
{
    setLookAndFeel (&lookAndFeel);

    // The bar exists before setSize(), because setSize() runs resized(), which lays the bar out.
    presetBar.reset (new SuitePresets::PresetBar (static_cast<SuitePresets::StateHolder&> (processor),
                                                  juce::Colour (0xffB18CFF)));

    setSize (editorWidth, editorHeight);
    setResizable (true, true);
    // Plan 15 (owner): the default height IS the minimum height.
    setResizeLimits (editorWidth - 80, editorHeight, 1200, 1200);

    // The choice lists mirror the parameter choice lists. Plan 13 (owner finding): the
    // character and side-filter lists have long text, so they are dropdowns; the three-way
    // target stays a pill.
    characterCell.setOptions ({ "Tube", "Tape", "Diode", "Console", "Clip" });
    targetCell.setOptions ({ "Width", "Drive", "Both" });
    filterCell.setOptions ({ "Off", "High Pass", "Low Pass", "Band Pass" });

    driveCtrl.slider.setNumDecimalPlacesToDisplay (1);
    attackCtrl.slider.setTextValueSuffix (" ms");
    releaseCtrl.slider.setTextValueSuffix (" ms");
    amountCtrl.slider.setTextValueSuffix (" %");
    widthCtrl.slider.setTextValueSuffix (" %");
    mixCtrl.slider.setTextValueSuffix (" %");

    auto& apvts = processor.apvts;

    driveAttachment   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, StereoToolProcessor::driveId, driveCtrl.slider);
    attackAttachment  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, StereoToolProcessor::transientAttackId, attackCtrl.slider);
    releaseAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, StereoToolProcessor::transientReleaseId, releaseCtrl.slider);
    amountAttachment  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, StereoToolProcessor::transientAmountId, amountCtrl.slider);
    widthAttachment   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, StereoToolProcessor::widthId, widthCtrl.slider);
    mixAttachment     = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, StereoToolProcessor::mixId, mixCtrl.slider);
    midFocusAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, StereoToolProcessor::midFocusId, midFocusCtrl.slider);
    crossoverAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, StereoToolProcessor::sideCrossoverId, crossoverCtrl.slider);

    // The choice widgets carry their own attachments (character and side filter are dropdowns).
    SuiteUI::attachDropdown (characterCell, *apvts.getParameter (StereoToolProcessor::characterId), characterAttachment);
    SuiteUI::attachSelector (targetCell, *apvts.getParameter (StereoToolProcessor::transientTargetId), targetAttachment);
    SuiteUI::attachDropdown (filterCell, *apvts.getParameter (StereoToolProcessor::sideFilterId), filterAttachment);

    // Page switch (plan 13): SOUND / MOD, following the SD/DS pattern.
    pageSelector.setOptions ({ "SOUND", "MOD" });
    pageSelector.onChange = [this] (int index) { setPage (index); };

    // The level stage (plan 12), appended so no saved parameter index moves.
    inputCtrl.slider.setNumDecimalPlacesToDisplay (1);
    outputCtrl.slider.setNumDecimalPlacesToDisplay (1);
    inputCtrl.slider.setTextValueSuffix (" dB");
    outputCtrl.slider.setTextValueSuffix (" dB");
    inputAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, StereoToolProcessor::inputLevelId, inputCtrl.slider);
    outputAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, StereoToolProcessor::outputLevelId, outputCtrl.slider);

    // The pan stage (plan 17 Phase 4): a balance knob, appended so no index moves.
    panCtrl.slider.setNumDecimalPlacesToDisplay (1);
    panCtrl.slider.setTextValueSuffix (" %");
    panAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, StereoToolProcessor::panId, panCtrl.slider);

    // The Phase-8 interest features (plan 17 Phase 8) and the master Phase toggle (plan 18).
    monoMakerCtrl.slider.setNumDecimalPlacesToDisplay (1);
    msToneMidCtrl.slider.setNumDecimalPlacesToDisplay (2);
    msToneSideCtrl.slider.setNumDecimalPlacesToDisplay (2);
    for (auto* ctrl : { &mbWidthLowCtrl, &mbWidthMidCtrl, &mbWidthHighCtrl })
        ctrl->slider.setTextValueSuffix (" %");
    rotRateCtrl.slider.setNumDecimalPlacesToDisplay (2);
    rotRateCtrl.slider.setTextValueSuffix (" Hz");
    rotDepthCtrl.slider.setNumDecimalPlacesToDisplay (1);
    rotDepthCtrl.slider.setTextValueSuffix (" %");
    phaseCtrl.slider.setNumDecimalPlacesToDisplay (1);
    phaseCtrl.slider.setTextValueSuffix (juce::String::fromUTF8 (" \xc2\xb0"));   // " °" as explicit UTF-8

    monoMakerAttachment  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, StereoToolProcessor::monoMakerCrossoverId, monoMakerCtrl.slider);
    msToneMidAttachment  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, StereoToolProcessor::msToneMidId, msToneMidCtrl.slider);
    msToneSideAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, StereoToolProcessor::msToneSideId, msToneSideCtrl.slider);
    mbWidthLowAttachment  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, StereoToolProcessor::multibandWidthLowId, mbWidthLowCtrl.slider);
    mbWidthMidAttachment  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, StereoToolProcessor::multibandWidthMidId, mbWidthMidCtrl.slider);
    mbWidthHighAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, StereoToolProcessor::multibandWidthHighId, mbWidthHighCtrl.slider);
    rotRateAttachment  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, StereoToolProcessor::imageRotationRateId, rotRateCtrl.slider);
    rotDepthAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, StereoToolProcessor::imageRotationDepthId, rotDepthCtrl.slider);
    phaseAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, StereoToolProcessor::phaseId, phaseCtrl.slider);

    // Help text on every control (plan 12); the footer sentence is the help line's idle text.
    SuiteUI::describe (driveCtrl, "Gain before the character stage. 0 dB is transparent.");
    SuiteUI::describe (attackCtrl, "How fast the transient follower rises. Inert at Amount 0.");
    SuiteUI::describe (releaseCtrl, "How fast the transient follower falls back. Inert at Amount 0.");
    SuiteUI::describe (amountCtrl, "How hard transients push the Width and/or Drive. 0 bypasses the follower.");
    SuiteUI::describe (characterCell, "Tube is the classic tanh curve; Tape, Diode, Console and Clip share its blend structure.");
    SuiteUI::describe (targetCell, "What the transient follower moves: the Width, the Drive, or both.");
    SuiteUI::describe (widthCtrl, "The side gain. 0 keeps the image, -100 folds to mono, +100 doubles the side.");
    SuiteUI::describe (mixCtrl, "0 is the untouched input, 100 the fully processed signal.");
    SuiteUI::describe (midFocusCtrl, "+50 pulls the centre forward, -50 widens the image.");
    SuiteUI::describe (crossoverCtrl, "The side filter's corner frequency. Only active while Side Filter is not Off.");
    SuiteUI::describe (filterCell, "Filters the side component only. Off is a true bypass.");
    SuiteUI::describe (inputCtrl, "Gain before the chain. 0 dB leaves the level unchanged.");
    SuiteUI::describe (outputCtrl, "The final gain stage. 0 dB leaves the level unchanged.");
    SuiteUI::describe (panCtrl, "The balance after the mix. Left and right shift the kept channel; the centre is untouched.");
    SuiteUI::describe (phaseCtrl, "Rotates the right channel's phase by up to 360 degrees, calibrated at 1 kHz. 0 is transparent.");
    SuiteUI::describe (monoMakerCtrl, "Folds everything below this crossover to mono. 0 turns the mono-maker off.");
    SuiteUI::describe (msToneMidCtrl, "Tilts the mid channel around 1 kHz. 0 is transparent.");
    SuiteUI::describe (msToneSideCtrl, "Tilts the side channel around 1 kHz. 0 is transparent.");
    SuiteUI::describe (mbWidthLowCtrl, "Narrows or widens the side below 200 Hz. 0 leaves the band unchanged.");
    SuiteUI::describe (mbWidthMidCtrl, "Narrows or widens the side between 200 Hz and 2 kHz. 0 leaves the band unchanged.");
    SuiteUI::describe (mbWidthHighCtrl, "Narrows or widens the side above 2 kHz. 0 leaves the band unchanged.");
    SuiteUI::describe (rotRateCtrl, "How fast the image rotation swings. Inert while Depth is 0.");
    SuiteUI::describe (rotDepthCtrl, "How far the image rotation swings, up to 90 degrees. 0 is off.");
    SuiteUI::describe (visualizer, "Mid and side levels, and the transient follower's envelope.");
    visualizer.setComponentID ("stereotool-preview");
    SuiteUI::describe (pageSelector, "SOUND: the stereo controls. MOD: the two LFOs and the six routing slots.");

    addAndMakeVisible (inputCtrl);
    addAndMakeVisible (outputCtrl);
    addAndMakeVisible (panCtrl);
    addAndMakeVisible (phaseCtrl);
    addAndMakeVisible (monoMakerCtrl);
    addAndMakeVisible (msToneMidCtrl);
    addAndMakeVisible (msToneSideCtrl);
    addAndMakeVisible (mbWidthLowCtrl);
    addAndMakeVisible (mbWidthMidCtrl);
    addAndMakeVisible (mbWidthHighCtrl);
    addAndMakeVisible (rotRateCtrl);
    addAndMakeVisible (rotDepthCtrl);

    for (auto* component : { static_cast<juce::Component*> (&driveCtrl), static_cast<juce::Component*> (&attackCtrl),
                             static_cast<juce::Component*> (&releaseCtrl), static_cast<juce::Component*> (&amountCtrl),
                             static_cast<juce::Component*> (&widthCtrl), static_cast<juce::Component*> (&mixCtrl),
                             static_cast<juce::Component*> (&midFocusCtrl), static_cast<juce::Component*> (&crossoverCtrl),
                             static_cast<juce::Component*> (&monoMakerCtrl), static_cast<juce::Component*> (&msToneMidCtrl),
                             static_cast<juce::Component*> (&msToneSideCtrl), static_cast<juce::Component*> (&mbWidthLowCtrl),
                             static_cast<juce::Component*> (&mbWidthMidCtrl), static_cast<juce::Component*> (&mbWidthHighCtrl),
                             static_cast<juce::Component*> (&rotRateCtrl), static_cast<juce::Component*> (&rotDepthCtrl),
                             static_cast<juce::Component*> (&phaseCtrl),
                             static_cast<juce::Component*> (&characterCell), static_cast<juce::Component*> (&targetCell),
                             static_cast<juce::Component*> (&filterCell), static_cast<juce::Component*> (&visualizer),
                             static_cast<juce::Component*> (&pageSelector), static_cast<juce::Component*> (&modulation) })
        addAndMakeVisible (component);

    // Each control that the routing can move, and the target it shows (plan 13).
    using Target = StereoToolProcessor::ModTarget;
    controls = std::make_unique<SuiteModulation::ControlDisplay> (processor.getRouter(),
        std::vector<SuiteModulation::ControlDisplay::Entry> {
            { widthCtrl.slider,       (int) Target::Width },
            { driveCtrl.slider,       (int) Target::Drive },
            { mixCtrl.slider,         (int) Target::Mix },
            { attackCtrl.slider,      (int) Target::TransientAttack },
            { releaseCtrl.slider,     (int) Target::TransientRelease },
            { amountCtrl.slider,      (int) Target::TransientAmount },
            { midFocusCtrl.slider,    (int) Target::MidFocus },
            { crossoverCtrl.slider,   (int) Target::SideCrossover },
            { inputCtrl.slider,       (int) Target::Input },
            { outputCtrl.slider,      (int) Target::Output },
            { panCtrl.slider,         (int) Target::Pan },
            { monoMakerCtrl.slider,   (int) Target::MonoMakerCrossover },
            { msToneMidCtrl.slider,   (int) Target::MSToneMid },
            { msToneSideCtrl.slider,  (int) Target::MSToneSide },
            { mbWidthLowCtrl.slider,  (int) Target::MultibandWidthLow },
            { mbWidthMidCtrl.slider,  (int) Target::MultibandWidthMid },
            { mbWidthHighCtrl.slider, (int) Target::MultibandWidthHigh },
            { rotRateCtrl.slider,     (int) Target::ImageRotationRate },
            { rotDepthCtrl.slider,    (int) Target::ImageRotationDepth },
        });

    addAndMakeVisible (headerMenu);

    // The help line (plan 12): the footer band, the stale-hover rule and the settings'
    // tooltips toggle, shared by every plugin in the suite.
    helpLine.setIdleText ("Mid focus +50 pulls the centre forward, -50 widens. Side Filter shapes the side component only.");
    helpLine.setComponentID ("stereotool-help");
    helpLine.setEnablePredicate ([] { return SuiteSettings::shared().getBool (SuiteSettings::tooltipsKey, true); });
    SuiteUI::attachHelpLine (*this, helpLine, helpTracker);
    addAndMakeVisible (helpLine);

    SuiteUI::describe (*presetBar, "Factory presets, INIT and SAVE. The menu lists user presets.");
    SuiteUI::describe (headerMenu, "Settings... and About...");

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

StereoToolEditor::~StereoToolEditor()
{
    stopTimer();
    removeMouseListener (&helpTracker);
    setLookAndFeel (nullptr);
}

SuiteModulation::ModulationPanel::Style StereoToolEditor::modulationStyle()
{
    return { { juce::Colour (0xffB18CFF), juce::Colour (0xffB18CFF) },
             SuiteUI::Colours::text, SuiteUI::Colours::textDim,
             SuiteUI::Colours::panel, SuiteUI::Colours::track };
}

void StereoToolEditor::setPage (int newPage)
{
    currentPage = juce::jlimit (0, 1, newPage);
    pageSelector.setSelectedIndex (currentPage);

    for (auto* component : { static_cast<juce::Component*> (&driveCtrl), static_cast<juce::Component*> (&attackCtrl),
                             static_cast<juce::Component*> (&releaseCtrl), static_cast<juce::Component*> (&amountCtrl),
                             static_cast<juce::Component*> (&widthCtrl), static_cast<juce::Component*> (&mixCtrl),
                             static_cast<juce::Component*> (&midFocusCtrl), static_cast<juce::Component*> (&crossoverCtrl),
                             static_cast<juce::Component*> (&monoMakerCtrl), static_cast<juce::Component*> (&msToneMidCtrl),
                             static_cast<juce::Component*> (&msToneSideCtrl), static_cast<juce::Component*> (&mbWidthLowCtrl),
                             static_cast<juce::Component*> (&mbWidthMidCtrl), static_cast<juce::Component*> (&mbWidthHighCtrl),
                             static_cast<juce::Component*> (&rotRateCtrl), static_cast<juce::Component*> (&rotDepthCtrl),
                             static_cast<juce::Component*> (&phaseCtrl),
                             static_cast<juce::Component*> (&characterCell), static_cast<juce::Component*> (&targetCell),
                             static_cast<juce::Component*> (&filterCell), static_cast<juce::Component*> (&inputCtrl),
                             static_cast<juce::Component*> (&outputCtrl), static_cast<juce::Component*> (&panCtrl) })
        component->setVisible (currentPage == 0);

    // The preview carries the input/output meters, so it stays on both pages.
    visualizer.setVisible (true);
    modulation.setVisible (currentPage == 1);

    resized();
    repaint();
}

void StereoToolEditor::paint (juce::Graphics& g)
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
        // The three group frames (R5: 15 pt bold titles in the accent; plan 12's shared frame).
        // Plan 18: the level group is named MASTER (the owner's rename finding).
        SuiteUI::drawGroupFrame (g, characterGroupBounds, "CHARACTER", lookAndFeel.getAccent());
        SuiteUI::drawGroupFrame (g, imageGroupBounds, "IMAGE", lookAndFeel.getAccent());
        SuiteUI::drawGroupFrame (g, levelGroupBounds, "MASTER", lookAndFeel.getAccent());
    }
    else
    {
        SuiteUI::drawGroupFrame (g, modulationGroupBounds, "MODULATION", lookAndFeel.getAccent());
    }
}

void StereoToolEditor::resized()
{
    auto area = getLocalBounds().reduced (16);

    // Header: menu at the far right, the preset bar flat to its left (plan 05.1), then the
    // page switch (R8: top right of the header, plan 13).
    auto headerArea = area.removeFromTop (headerHeight);
    headerMenu.setBounds (headerArea.removeFromRight (26).withHeight (26)
                            .withY (headerArea.getCentreY() - 13));
    const int barHeight = presetBar != nullptr ? presetBar->getPreferredHeight() : 0;
    if (barHeight > 0)
        presetBar->setBounds (headerArea.removeFromRight (300).withHeight (barHeight)
                                .withY (headerArea.getCentreY() - barHeight / 2));
    pageSelector.setBounds (headerArea.removeFromRight (140).withHeight (SuiteUI::pillHeight)
                                .withY (headerArea.getCentreY() - SuiteUI::pillHeight / 2));

    // The help line sits in the footer band. Reserve it (plus the standard gap) before the
    // content, so no group can overlap it.
    helpLine.setBounds (getLocalBounds().removeFromBottom (footerHeight));
    area.setBottom (getLocalBounds().getHeight() - footerHeight - groupGap);

    area.removeFromTop (groupGap);

    if (currentPage == 0)
        layoutSoundPage (area);
    else
        layoutModPage (area);
}

void StereoToolEditor::layoutSoundPage (juce::Rectangle<int> area)
{
    modulationGroupBounds = {};
    modulation.setBounds ({});

    // Row 1 (plan 15, owner): the mid/side + follower preview across the full width at the
    // top of the window. It takes the spare height above its 100 px floor, so an enlarged
    // window grows the preview and the default window has none to spare.
    characterGroupBounds = area.removeFromBottom (groupHeight);
    area.removeFromBottom (groupGap);

    // The IMAGE group is taller than the others (plan 17 Phase 8: four knob rows).
    auto row = area.removeFromBottom (imageGroupHeight);
    levelGroupBounds = row.removeFromLeft (levelGroupWidth);
    row.removeFromLeft (groupGap);
    imageGroupBounds = row;
    area.removeFromBottom (groupGap);

    visualBounds = area;
    visualizer.setBounds (visualBounds);

    // LEVEL is MASTER (plan 18): the level stage plus pan and the Phase toggle.
    {
        auto inner = levelGroupBounds.withTrimmedTop (groupTitleHeight + 4).reduced (10, 4);
        auto row1 = inner.removeFromTop (SuiteUI::knobRowHeight);
        const int cellWidth = row1.getWidth() / 2;
        inputCtrl.setBounds (row1.removeFromLeft (cellWidth).reduced (3));
        outputCtrl.setBounds (row1.reduced (3));

        inner.removeFromTop (groupGap);
        auto row2 = inner.removeFromTop (SuiteUI::knobRowHeight);
        panCtrl.setBounds (row2.removeFromLeft (cellWidth).reduced (3));
        phaseCtrl.setBounds (row2.reduced (3));
    }

    // CHARACTER: character, drive, attack, release / amount, target.
    {
        auto inner = characterGroupBounds.withTrimmedTop (groupTitleHeight + 4).reduced (10, 4);
        const int cellWidth = inner.getWidth() / 4;
        auto row1 = inner.removeFromTop (SuiteUI::knobRowHeight);
        auto row2 = inner.removeFromBottom (SuiteUI::knobRowHeight);

        for (auto* cell : { static_cast<juce::Component*> (&characterCell), static_cast<juce::Component*> (&driveCtrl),
                            static_cast<juce::Component*> (&attackCtrl),     static_cast<juce::Component*> (&releaseCtrl) })
            cell->setBounds (row1.removeFromLeft (cellWidth).reduced (3));

        for (auto* cell : { static_cast<juce::Component*> (&amountCtrl), static_cast<juce::Component*> (&targetCell),
                            static_cast<juce::Component*> (nullptr),    static_cast<juce::Component*> (nullptr) })
        {
            if (cell == nullptr)
                break;
            cell->setBounds (row2.removeFromLeft (cellWidth).reduced (3));
        }
    }

    // IMAGE: width, mix, mid focus, crossover / side filter; then the plan-17 interest
    // features and the rotation (four rows, plan 18's taller group).
    {
        auto inner = imageGroupBounds.withTrimmedTop (groupTitleHeight + 4).reduced (10, 4);
        const int cellWidth = inner.getWidth() / 4;
        auto row1 = inner.removeFromTop (SuiteUI::knobRowHeight);
        auto row2 = inner.removeFromTop (SuiteUI::knobRowHeight);
        inner.removeFromTop (groupGap);
        auto row3 = inner.removeFromTop (SuiteUI::knobRowHeight);
        inner.removeFromTop (groupGap);
        auto row4 = inner.removeFromTop (SuiteUI::knobRowHeight);

        for (auto* cell : { static_cast<juce::Component*> (&widthCtrl), static_cast<juce::Component*> (&mixCtrl),
                            static_cast<juce::Component*> (&midFocusCtrl), static_cast<juce::Component*> (&crossoverCtrl) })
            cell->setBounds (row1.removeFromLeft (cellWidth).reduced (3));

        for (auto* cell : { static_cast<juce::Component*> (&filterCell), static_cast<juce::Component*> (&monoMakerCtrl),
                            static_cast<juce::Component*> (&msToneMidCtrl), static_cast<juce::Component*> (&msToneSideCtrl) })
            cell->setBounds (row2.removeFromLeft (cellWidth).reduced (3));

        for (auto* cell : { static_cast<juce::Component*> (&mbWidthLowCtrl), static_cast<juce::Component*> (&mbWidthMidCtrl),
                            static_cast<juce::Component*> (&mbWidthHighCtrl), static_cast<juce::Component*> (&rotRateCtrl) })
            cell->setBounds (row3.removeFromLeft (cellWidth).reduced (3));

        rotDepthCtrl.setBounds (row4.removeFromLeft (cellWidth).reduced (3));
    }
}

void StereoToolEditor::layoutModPage (juce::Rectangle<int> area)
{
    characterGroupBounds = {};
    imageGroupBounds = {};
    levelGroupBounds = {};

    // The preview stays visible above the modulation panel (plan 15: no meters now).
    visualBounds = area.removeFromTop (juce::jmax (previewMinimumHeight, (int) ((float) area.getHeight() * 0.30f)));
    visualizer.setBounds (visualBounds);

    area.removeFromTop (groupGap);

    // The modulation panel fills the remaining area.
    modulationGroupBounds = area;
    modulation.setBounds (area.withTrimmedTop (groupTitleHeight + 4).reduced (12, 6));
}

void StereoToolEditor::timerCallback()
{
    modulation.refresh();
    controls->update();
    visualizer.tick();

    // Plan 15 (owner): controls that do not apply right now are dimmed, never hidden
    // (the plan 10 G1 rule). Side Crossover is inert while the Side Filter is Off, and
    // the transient follower's Attack/Release/Target are inert while Amount is 0.
    const int filterIndex = juce::roundToInt (
        processor.apvts.getRawParameterValue (StereoToolProcessor::sideFilterId)->load());
    const float transientAmount = processor.apvts.getRawParameterValue (StereoToolProcessor::transientAmountId)->load();

    crossoverCtrl.setEnabled (filterIndex != 0);
    attackCtrl.setEnabled (transientAmount > 0.0f);
    releaseCtrl.setEnabled (transientAmount > 0.0f);
    targetCell.setEnabled (transientAmount > 0.0f);

    // Plan 17 Phase 8 affinity: the rotation rate is inert while the depth is 0.
    const float rotationDepth = processor.apvts.getRawParameterValue (StereoToolProcessor::imageRotationDepthId)->load();
    rotRateCtrl.setEnabled (rotationDepth > 0.0f);
}

//==============================================================================
StereoToolEditor::MidSideMeter::MidSideMeter (StereoToolProcessor& p)
    : processor (p)
{
}

void StereoToolEditor::MidSideMeter::tick()
{
    const auto& snapshot = processor.getMeterSnapshot();
    envelopeHistory[(size_t) historyHead] = snapshot.followerEnvelope.load (std::memory_order_relaxed);
    historyHead = (historyHead + 1) % (int) envelopeHistory.size();

    repaint();
}

void StereoToolEditor::MidSideMeter::paint (juce::Graphics& g)
{
    const auto& snapshot = processor.getMeterSnapshot();
    auto area = getLocalBounds().toFloat().reduced (4.0f);
    const auto accent = dynamic_cast<SuiteUI::SuiteLookAndFeel*> (&getLookAndFeel()) != nullptr
                            ? dynamic_cast<SuiteUI::SuiteLookAndFeel&> (getLookAndFeel()).getAccent()
                            : SuiteUI::Colours::text;

    g.setColour (SuiteUI::Colours::panel);
    g.fillRoundedRectangle (area, 6.0f);
    g.setColour (SuiteUI::Colours::panelEdge);
    g.drawRoundedRectangle (area, 6.0f, 1.0f);

    // The bars and trace fill the preview (plan 15: the meter columns are gone).
    auto inner = area.reduced (4.0f);

    // The mid and side bars, each twice as wide as before (owner finding, plan 13).
    auto meters = inner.removeFromLeft (midSideBarsWidth).reduced (10.0f, 10.0f);
    const float barWidth = 36.0f;

    for (const int band : { 0, 1 })
    {
        const float level = juce::jlimit (0.0f, 1.0f,
            band == 0 ? snapshot.midLevel.load (std::memory_order_relaxed)
                      : snapshot.sideLevel.load (std::memory_order_relaxed));

        auto barArea = meters.withX (meters.getX() + (float) band * (barWidth + 6.0f)).withWidth (barWidth);
        g.setColour (SuiteUI::Colours::track);
        g.fillRoundedRectangle (barArea, 3.0f);

        auto filled = barArea.withTop (barArea.getBottom() - juce::jlimit (0.0f, 1.0f, level * 2.0f) * barArea.getHeight());
        g.setColour (accent);
        g.fillRoundedRectangle (filled, 3.0f);

        g.setColour (SuiteUI::Colours::textDim);
        g.setFont (SuiteUI::uiFont (11.0f));
        g.drawText (band == 0 ? "MID" : "SIDE",
                    barArea.withTop (barArea.getBottom() + 2.0f).withHeight (14.0f),
                    juce::Justification::centred);
    }

    // The transient follower's envelope, scrolled: the oldest point on the left.
    auto trace = inner.reduced (10.0f, 12.0f);

    g.setColour (SuiteUI::Colours::track);
    g.drawHorizontalLine ((int) trace.getCentreY(), trace.getX(), trace.getRight());

    juce::Path path;
    const int count = (int) envelopeHistory.size();
    bool started = false;

    for (int i = 0; i < count; ++i)
    {
        // Oldest first: the read position walks backwards from the head.
        const float value = envelopeHistory[(size_t) ((historyHead + i) % count)];
        const float x = trace.getX() + (float) i / (float) (count - 1) * trace.getWidth();
        const float y = trace.getBottom() - juce::jlimit (0.0f, 1.0f, value) * trace.getHeight();

        if (! started)
        {
            path.startNewSubPath (x, y);
            started = true;
        }
        else
        {
            path.lineTo (x, y);
        }
    }

    g.setColour (accent.withAlpha (0.25f));
    g.strokePath (path, juce::PathStrokeType (5.0f));
    g.setColour (accent);
    g.strokePath (path, juce::PathStrokeType (1.5f));

    g.setColour (SuiteUI::Colours::textDim);
    g.setFont (SuiteUI::uiFont (11.0f));
    g.drawText ("FOLLOWER", trace.getX() + 4.0f, trace.getY() - 1.0f, 80.0f, 12.0f, juce::Justification::centredLeft);
}