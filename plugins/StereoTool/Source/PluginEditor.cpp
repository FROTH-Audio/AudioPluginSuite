#include "PluginEditor.h"

#include "SuiteBrand.h"
#include "BinaryData.h"
#include "SuiteSettings.h"

namespace
{
    // Plan 05 R4: the default window stays inside the suite budget (1000x700). The knob
    // size comes from the SuiteUI tokens (R1); never set slider bounds that exceed them.
    constexpr int editorWidth = 880;
    constexpr int editorHeight = 700;
    constexpr int headerHeight = 44;
    constexpr int footerHeight = 26;
    constexpr int groupGap = 8;   // the suite's one-gap rule (plan 05 R7)
    constexpr int groupTitleHeight = SuiteUI::groupTitleHeight;
    constexpr int groupInnerHeight = 2 * SuiteUI::knobRowHeight + groupGap;
    constexpr int groupHeight = groupTitleHeight + 4 + groupInnerHeight + 4;
    constexpr int visualHeight = 130;

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
    setResizeLimits (editorWidth - 80, editorHeight - 80, 1200, 1200);

    // The choice lists mirror the parameter choice lists. Rule #3 (plan 11): pills for
    // three, four and five options, no dropdowns left.
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

    // The pill selectors carry their own attachments (plan 11 D1).
    SuiteUI::attachSelector (characterCell, *apvts.getParameter (StereoToolProcessor::characterId), characterAttachment);
    SuiteUI::attachSelector (targetCell, *apvts.getParameter (StereoToolProcessor::transientTargetId), targetAttachment);
    SuiteUI::attachSelector (filterCell, *apvts.getParameter (StereoToolProcessor::sideFilterId), filterAttachment);

    for (auto* component : { static_cast<juce::Component*> (&driveCtrl), static_cast<juce::Component*> (&attackCtrl),
                             static_cast<juce::Component*> (&releaseCtrl), static_cast<juce::Component*> (&amountCtrl),
                             static_cast<juce::Component*> (&widthCtrl), static_cast<juce::Component*> (&mixCtrl),
                             static_cast<juce::Component*> (&midFocusCtrl), static_cast<juce::Component*> (&crossoverCtrl),
                             static_cast<juce::Component*> (&characterCell), static_cast<juce::Component*> (&targetCell),
                             static_cast<juce::Component*> (&filterCell), static_cast<juce::Component*> (&visualizer) })
        addAndMakeVisible (component);

    addAndMakeVisible (gearButton);

    // Gear menu (R13): About… and Settings…, the same two actions every suite plugin has.
    gearButton.onAbout = [this]
    {
        SuiteUI::showAboutDialog (brandLockup(), SUITE_PLUGIN_NAME, SUITE_PLUGIN_VERSION,
                                  SUITE_PLUGIN_CODE, SUITE_PLUGIN_ID, this);
    };
    gearButton.onSettings = [this]
    {
        SuiteSettings::showSettingsDialog (SuiteSettings::shared(), this);
    };

    if (presetBar->getPreferredHeight() > 0)
        addAndMakeVisible (*presetBar);
    presetBar->onPresetLoaded = [this] { repaint(); };

    startTimerHz (SuiteUI::kRefreshHz);
}

StereoToolEditor::~StereoToolEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void StereoToolEditor::paint (juce::Graphics& g)
{
    juce::ColourGradient background (SuiteUI::Colours::panelLight, 0.0f, 0.0f,
                                     SuiteUI::Colours::background, 0.0f, (float) getHeight(), false);
    g.setGradientFill (background);
    g.fillAll();

    // Brand lockup at the left of the header (the FROTH Audio wordmark, the mark and
    // the product name, generated by tools/brand/lockups.py and embedded in the bundle).
    SuiteUI::drawBrandLockup (g, getLocalBounds().removeFromTop (headerHeight).reduced (16, 0), brandLockup());

    // The two group frames and their titles (R5: 15 pt bold in the accent).
    g.setColour (lookAndFeel.getAccent());
    g.setFont (juce::Font (juce::FontOptions (15.0f, juce::Font::bold)));
    g.drawText ("CHARACTER", characterGroupBounds.withHeight (groupTitleHeight).translated (0, 4).reduced (12, 0),
                juce::Justification::centredLeft);
    g.drawText ("IMAGE", imageGroupBounds.withHeight (groupTitleHeight).translated (0, 4).reduced (12, 0),
                juce::Justification::centredLeft);

    g.setColour (SuiteUI::Colours::textDim);
    g.setFont (SuiteUI::uiFont (11.0f));
    g.drawText ("Mid focus +50 pulls the centre forward, -50 widens. Side Filter shapes the side component only.",
                getLocalBounds().removeFromBottom (footerHeight).reduced (16, 0),
                juce::Justification::centredLeft);
}

void StereoToolEditor::resized()
{
    auto area = getLocalBounds().reduced (16);

    // Header: gear at the far right, the preset bar flat to its left (plan 05.1).
    auto headerArea = area.removeFromTop (headerHeight);
    gearButton.setBounds (headerArea.removeFromRight (26).withHeight (26)
                            .withY (headerArea.getCentreY() - 13));
    const int barHeight = presetBar != nullptr ? presetBar->getPreferredHeight() : 0;
    if (barHeight > 0)
        presetBar->setBounds (headerArea.removeFromRight (300).withHeight (barHeight)
                                .withY (headerArea.getCentreY() - barHeight / 2));

    area.removeFromTop (groupGap);

    // The mid/side visual spans the window between the header and the groups (plan 11 4c).
    visualBounds = area.removeFromTop (visualHeight);
    visualizer.setBounds (visualBounds);

    area.removeFromTop (groupGap);

    characterGroupBounds = area.removeFromTop (groupHeight);
    area.removeFromTop (groupGap);
    imageGroupBounds = area.removeFromTop (groupHeight);

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

    // IMAGE: width, mix, mid focus / side filter, crossover.
    {
        auto inner = imageGroupBounds.withTrimmedTop (groupTitleHeight + 4).reduced (10, 4);
        const int cellWidth = inner.getWidth() / 4;
        auto row1 = inner.removeFromTop (SuiteUI::knobRowHeight);
        auto row2 = inner.removeFromBottom (SuiteUI::knobRowHeight);

        for (auto* cell : { static_cast<juce::Component*> (&widthCtrl), static_cast<juce::Component*> (&mixCtrl),
                            static_cast<juce::Component*> (&midFocusCtrl), static_cast<juce::Component*> (nullptr) })
        {
            if (cell == nullptr)
                break;
            cell->setBounds (row1.removeFromLeft (cellWidth).reduced (3));
        }

        for (auto* cell : { static_cast<juce::Component*> (&filterCell), static_cast<juce::Component*> (&crossoverCtrl),
                            static_cast<juce::Component*> (nullptr),    static_cast<juce::Component*> (nullptr) })
        {
            if (cell == nullptr)
                break;
            cell->setBounds (row2.removeFromLeft (cellWidth).reduced (3));
        }
    }
}

//==============================================================================
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

    // The mid and side bars (plan 11 4c), left of the envelope trace.
    auto meters = area.removeFromLeft (64.0f).reduced (10.0f, 10.0f);
    const float barWidth = 18.0f;

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
    auto trace = area.reduced (10.0f, 12.0f);

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