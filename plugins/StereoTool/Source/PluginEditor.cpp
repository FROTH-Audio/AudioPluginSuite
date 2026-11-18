#include "PluginEditor.h"

#include "SuiteBrand.h"
#include "BinaryData.h"

namespace
{
    // Plan 05 R4: the default window stays inside the suite budget (1000x700). The knob
    // size comes from the SuiteUI tokens (R1); never set slider bounds that exceed them.
    constexpr int editorWidth = 880;
    constexpr int editorHeight = 560;
    constexpr int headerHeight = 44;
    constexpr int footerHeight = 26;
    constexpr int groupGap = 8;   // the suite's one-gap rule (plan 05 R7)
    constexpr int groupTitleHeight = SuiteUI::groupTitleHeight;
    constexpr int groupInnerHeight = 2 * SuiteUI::knobRowHeight + groupGap;
    constexpr int groupHeight = groupTitleHeight + 4 + groupInnerHeight + 4;

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

    // The combo lists mirror the parameter choice lists; the attachment keeps them in sync.
    characterCell.box.addItemList ({ "Tube", "Tape", "Diode", "Console", "Clip" }, 1);
    targetCell.box.addItemList ({ "Width", "Drive", "Both" }, 1);
    filterCell.box.addItemList ({ "Off", "High Pass", "Low Pass", "Band Pass" }, 1);

    driveCtrl.slider.setTextValueSuffix (" dB");
    driveCtrl.slider.setNumDecimalPlacesToDisplay (1);
    attackCtrl.slider.setTextValueSuffix (" ms");
    attackCtrl.slider.setNumDecimalPlacesToDisplay (1);
    releaseCtrl.slider.setTextValueSuffix (" ms");
    releaseCtrl.slider.setNumDecimalPlacesToDisplay (1);
    amountCtrl.slider.setTextValueSuffix (" %");
    amountCtrl.slider.setNumDecimalPlacesToDisplay (0);
    widthCtrl.slider.setTextValueSuffix (" %");
    widthCtrl.slider.setNumDecimalPlacesToDisplay (1);
    mixCtrl.slider.setTextValueSuffix (" %");
    mixCtrl.slider.setNumDecimalPlacesToDisplay (1);
    midFocusCtrl.slider.setTextValueSuffix (" %");
    midFocusCtrl.slider.setNumDecimalPlacesToDisplay (1);
    crossoverCtrl.slider.setNumDecimalPlacesToDisplay (1);

    auto& apvts = processor.apvts;

    driveAttachment   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, StereoToolProcessor::driveId, driveCtrl.slider);
    attackAttachment  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, StereoToolProcessor::transientAttackId, attackCtrl.slider);
    releaseAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, StereoToolProcessor::transientReleaseId, releaseCtrl.slider);
    amountAttachment  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, StereoToolProcessor::transientAmountId, amountCtrl.slider);
    widthAttachment   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, StereoToolProcessor::widthId, widthCtrl.slider);
    mixAttachment     = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, StereoToolProcessor::mixId, mixCtrl.slider);
    midFocusAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, StereoToolProcessor::midFocusId, midFocusCtrl.slider);
    crossoverAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, StereoToolProcessor::sideCrossoverId, crossoverCtrl.slider);

    // Built after the items exist, so the first update selects the parameter's current choice.
    characterAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, StereoToolProcessor::characterId, characterCell.box);
    targetAttachment    = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, StereoToolProcessor::transientTargetId, targetCell.box);
    filterAttachment    = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, StereoToolProcessor::sideFilterId, filterCell.box);

    // Help text for every control.
    driveCtrl.setHelpText ("Pre-gain into the shaper. 0 dB is transparent; below 0 is a clean gain reduction.");
    driveCtrl.slider.setTooltip (driveCtrl.getHelpText());
    attackCtrl.setHelpText ("How fast the follower rises on a transient.");
    attackCtrl.slider.setTooltip (attackCtrl.getHelpText());
    releaseCtrl.setHelpText ("How fast the follower falls after a transient.");
    releaseCtrl.slider.setTooltip (releaseCtrl.getHelpText());
    amountCtrl.setHelpText ("How hard the follower moves its target. 0 bypasses the follower.");
    amountCtrl.slider.setTooltip (amountCtrl.getHelpText());
    widthCtrl.setHelpText ("Scales the side component: -100 folds to mono, 0 is unchanged, +100 doubles the side.");
    widthCtrl.slider.setTooltip (widthCtrl.getHelpText());
    mixCtrl.setHelpText ("Dry/wet balance.");
    mixCtrl.slider.setTooltip (mixCtrl.getHelpText());
    midFocusCtrl.setHelpText ("Pulls the centre forward (+) or widens the image (-): mid = 1 + f, side = 1 - f.");
    midFocusCtrl.slider.setTooltip (midFocusCtrl.getHelpText());
    crossoverCtrl.setHelpText ("The side filter's corner frequency.");
    crossoverCtrl.slider.setTooltip (crossoverCtrl.getHelpText());
    characterCell.setHelpText ("The saturation curve on the mid and side.");
    targetCell.setHelpText ("What the transient follower moves.");
    filterCell.setHelpText ("A filter on the side component only, so the lows can stay mono. Off is a true bypass.");

    for (auto* component : { static_cast<juce::Component*> (&driveCtrl), static_cast<juce::Component*> (&attackCtrl),
                             static_cast<juce::Component*> (&releaseCtrl), static_cast<juce::Component*> (&amountCtrl),
                             static_cast<juce::Component*> (&widthCtrl), static_cast<juce::Component*> (&mixCtrl),
                             static_cast<juce::Component*> (&midFocusCtrl), static_cast<juce::Component*> (&crossoverCtrl),
                             static_cast<juce::Component*> (&characterCell), static_cast<juce::Component*> (&targetCell),
                             static_cast<juce::Component*> (&filterCell) })
        addAndMakeVisible (component);

    if (presetBar->getPreferredHeight() > 0)
        addAndMakeVisible (*presetBar);
    presetBar->onPresetLoaded = [this] { repaint(); };
}

StereoToolEditor::~StereoToolEditor()
{
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

    // The two group frames and their titles.
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

    // Reserve space for the preset bar (300x30), right-aligned in the header band.
    auto headerArea = area.removeFromTop (headerHeight);
    const int barHeight = presetBar != nullptr ? presetBar->getPreferredHeight() : 0;

    if (barHeight > 0)
        presetBar->setBounds (headerArea.removeFromRight (300).withHeight (barHeight)
                                .withY (headerArea.getCentreY() - barHeight / 2));

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
                            static_cast<juce::Component*> (&attackCtrl), static_cast<juce::Component*> (&releaseCtrl) })
            cell->setBounds (row1.removeFromLeft (cellWidth).reduced (3));

        for (auto* cell : { static_cast<juce::Component*> (&amountCtrl), static_cast<juce::Component*> (&targetCell),
                            static_cast<juce::Component*> (nullptr), static_cast<juce::Component*> (nullptr) })
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
                            static_cast<juce::Component*> (nullptr), static_cast<juce::Component*> (nullptr) })
        {
            if (cell == nullptr)
                break;
            cell->setBounds (row2.removeFromLeft (cellWidth).reduced (3));
        }
    }
}
