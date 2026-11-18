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
        static const juce::Image image = juce::ImageCache::getFromMemory (BinaryData::lockup_midi_effect_png,
                                                                         BinaryData::lockup_midi_effect_pngSize);
        return image;
    }
}

MidiEffectEditor::MidiEffectEditor (MidiEffectProcessor& p)
    : AudioProcessorEditor (&p),
      processor (p)
{
    setLookAndFeel (&lookAndFeel);

    // The bar exists before setSize(), because setSize() runs resized(), which lays the bar out.
    presetBar.reset (new SuitePresets::PresetBar (static_cast<SuitePresets::StateHolder&> (processor),
                                                  juce::Colour (0xff4ECDC4)));

    setSize (editorWidth, editorHeight);
    setResizable (true, true);
    setResizeLimits (editorWidth - 80, editorHeight - 80, 1200, 1200);

    // The combo lists mirror the parameter choice lists; the attachment keeps them in sync.
    scaleCell.box.addItemList ({ "Chromatic", "Major", "Minor", "Minor Pentatonic", "Dorian", "Whole Tone" }, 1);
    chordCell.box.addItemList ({ "Major", "Minor", "Dim", "Aug", "Sus2", "Sus4", "Minor 7", "Major 7" }, 1);
    arpCell.box.addItemList ({ "Up", "Down", "Alternate", "Random", "Scattered" }, 1);
    modeCell.box.addItemList ({ "Chord", "Arp" }, 1);
    rateCell.box.addItemList ({ "1/8", "1/8T", "1/16", "1/16T", "1/32" }, 1);
    syncCell.box.addItemList ({ "Off", "On" }, 1);
    curveCell.box.addItemList ({ "Flat", "Linear", "Exponential", "Random" }, 1);
    dirCell.box.addItemList ({ "Ascending", "Descending" }, 1);

    for (auto* cell : { &scaleCell, &chordCell, &arpCell, &modeCell, &rateCell, &syncCell, &curveCell, &dirCell })
        cell->setHelpText (cell->label.getText() + ": choose a value from the list.");

    rootCtrl.slider.setNumDecimalPlacesToDisplay (0);
    octaveCtrl.slider.setNumDecimalPlacesToDisplay (0);
    velocityCtrl.slider.setTextValueSuffix (" %");
    velocityCtrl.slider.setNumDecimalPlacesToDisplay (0);
    spreadCtrl.slider.setNumDecimalPlacesToDisplay (2);
    tempoCtrl.slider.setNumDecimalPlacesToDisplay (1);
    gateCtrl.slider.setTextValueSuffix (" %");
    gateCtrl.slider.setNumDecimalPlacesToDisplay (0);

    auto& apvts = processor.apvts;

    rootAttachment     = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, MidiEffectProcessor::rootId, rootCtrl.slider);
    octaveAttachment   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, MidiEffectProcessor::octaveId, octaveCtrl.slider);
    velocityAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, MidiEffectProcessor::velocityId, velocityCtrl.slider);
    spreadAttachment   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, MidiEffectProcessor::octaveSpreadId, spreadCtrl.slider);
    tempoAttachment    = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, MidiEffectProcessor::tempoId, tempoCtrl.slider);
    gateAttachment     = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, MidiEffectProcessor::gateLengthId, gateCtrl.slider);

    // Built after the items exist, so the first update selects the parameter's current choice.
    scaleAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, MidiEffectProcessor::scaleId, scaleCell.box);
    chordAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, MidiEffectProcessor::chordId, chordCell.box);
    arpAttachment   = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, MidiEffectProcessor::arpId, arpCell.box);
    modeAttachment  = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, MidiEffectProcessor::arpModeId, modeCell.box);
    rateAttachment  = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, MidiEffectProcessor::stepRateId, rateCell.box);
    syncAttachment  = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, MidiEffectProcessor::hostSyncId, syncCell.box);
    curveAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, MidiEffectProcessor::velocityCurveId, curveCell.box);
    dirAttachment   = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, MidiEffectProcessor::velocityDirectionId, dirCell.box);

    // Help text for every control.
    rootCtrl.setHelpText ("The key centre the scale quantisation works from.");
    rootCtrl.slider.setTooltip ("The key centre the scale quantisation works from.");
    octaveCtrl.setHelpText ("Transposes the chord.");
    octaveCtrl.slider.setTooltip ("Transposes the chord.");
    velocityCtrl.setHelpText ("Scales the note-on velocity.");
    velocityCtrl.slider.setTooltip ("Scales the note-on velocity.");
    spreadCtrl.setHelpText ("Lifts each pass through the chord tones of the 8-step arp pattern by up to an octave.");
    spreadCtrl.slider.setTooltip ("Lifts each pass through the chord tones of the 8-step arp pattern by up to an octave.");
    tempoCtrl.setHelpText ("Tempo used while Host Sync is off.");
    tempoCtrl.slider.setTooltip ("Tempo used while Host Sync is off.");
    gateCtrl.setHelpText ("Note length as a share of one step.");
    gateCtrl.slider.setTooltip ("Note length as a share of one step.");
    modeCell.setHelpText ("Chord fires the whole chord at once; Arp runs the step clock.");
    rateCell.setHelpText ("The arp's step length.");
    syncCell.setHelpText ("On: the arp follows the host position and tempo. Off: it runs on the Tempo parameter.");
    curveCell.setHelpText ("The per-step velocity shape in Arp mode.");
    dirCell.setHelpText ("Which end the Linear and Exponential curves start at.");

    for (auto* component : { static_cast<juce::Component*> (&rootCtrl), static_cast<juce::Component*> (&octaveCtrl),
                             static_cast<juce::Component*> (&velocityCtrl), static_cast<juce::Component*> (&spreadCtrl),
                             static_cast<juce::Component*> (&tempoCtrl), static_cast<juce::Component*> (&gateCtrl),
                             static_cast<juce::Component*> (&scaleCell), static_cast<juce::Component*> (&chordCell),
                             static_cast<juce::Component*> (&arpCell), static_cast<juce::Component*> (&modeCell),
                             static_cast<juce::Component*> (&rateCell), static_cast<juce::Component*> (&syncCell),
                             static_cast<juce::Component*> (&curveCell), static_cast<juce::Component*> (&dirCell) })
        addAndMakeVisible (component);

    if (presetBar->getPreferredHeight() > 0)
        addAndMakeVisible (*presetBar);
    presetBar->onPresetLoaded = [this] { repaint(); };
}

MidiEffectEditor::~MidiEffectEditor()
{
    setLookAndFeel (nullptr);
}

void MidiEffectEditor::paint (juce::Graphics& g)
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
    g.drawText ("ARP", arpGroupBounds.withHeight (groupTitleHeight).translated (0, 4).reduced (12, 0),
                juce::Justification::centredLeft);
    g.drawText ("SHAPE", shapeGroupBounds.withHeight (groupTitleHeight).translated (0, 4).reduced (12, 0),
                juce::Justification::centredLeft);

    g.setColour (SuiteUI::Colours::textDim);
    g.setFont (SuiteUI::uiFont (11.0f));
    g.drawText ("Chord mode: a note becomes a chord at once. Arp mode: the chord plays over an 8-step clock.",
                getLocalBounds().removeFromBottom (footerHeight).reduced (16, 0),
                juce::Justification::centredLeft);
}

void MidiEffectEditor::resized()
{
    auto area = getLocalBounds().reduced (16);

    // Reserve space for the preset bar (300x30), right-aligned in the header band.
    auto headerArea = area.removeFromTop (headerHeight);
    const int barHeight = presetBar != nullptr ? presetBar->getPreferredHeight() : 0;

    if (barHeight > 0)
        presetBar->setBounds (headerArea.removeFromRight (300).withHeight (barHeight)
                                .withY (headerArea.getCentreY() - barHeight / 2));

    area.removeFromTop (groupGap);

    arpGroupBounds = area.removeFromTop (groupHeight);
    area.removeFromTop (groupGap);
    shapeGroupBounds = area.removeFromTop (groupHeight);

    // ARP: mode, step rate, sync, tempo / gate, velocity curve, velocity direction.
    {
        auto inner = arpGroupBounds.withTrimmedTop (groupTitleHeight + 4).reduced (10, 4);
        const int cellWidth = inner.getWidth() / 4;
        auto row1 = inner.removeFromTop (SuiteUI::knobRowHeight);
        auto row2 = inner.removeFromBottom (SuiteUI::knobRowHeight);

        std::initializer_list<juce::Component*> row1Cells { &modeCell, &rateCell, &syncCell, &tempoCtrl };
        std::initializer_list<juce::Component*> row2Cells { &gateCtrl, &curveCell, &dirCell, nullptr };

        for (auto* cells : { &row1Cells, &row2Cells })
        {
            auto row = cells == &row1Cells ? row1 : row2;
            for (auto* cell : *cells)
            {
                if (cell == nullptr)
                    break;
                cell->setBounds (row.removeFromLeft (cellWidth).reduced (3));
            }
        }
    }

    // SHAPE: root, scale, chord, pattern / octave, spread, velocity.
    {
        auto inner = shapeGroupBounds.withTrimmedTop (groupTitleHeight + 4).reduced (10, 4);
        const int cellWidth = inner.getWidth() / 4;
        auto row1 = inner.removeFromTop (SuiteUI::knobRowHeight);
        auto row2 = inner.removeFromBottom (SuiteUI::knobRowHeight);

        for (auto* cell : { static_cast<juce::Component*> (&rootCtrl), static_cast<juce::Component*> (&scaleCell),
                            static_cast<juce::Component*> (&chordCell), static_cast<juce::Component*> (&arpCell) })
            cell->setBounds (row1.removeFromLeft (cellWidth).reduced (3));

        for (auto* cell : { static_cast<juce::Component*> (&octaveCtrl), static_cast<juce::Component*> (&spreadCtrl),
                            static_cast<juce::Component*> (&velocityCtrl), static_cast<juce::Component*> (nullptr) })
        {
            if (cell == nullptr)
                break;
            cell->setBounds (row2.removeFromLeft (cellWidth).reduced (3));
        }
    }
}
