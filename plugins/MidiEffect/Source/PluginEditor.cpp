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

    // The choice lists mirror the parameter choice lists; the attachment keeps them in sync.
    // Rule #3 (plan 11): a pill for six or fewer options, a dropdown only for CHORD (eight).
    scaleCell.setOptions ({ "Chromatic", "Major", "Minor", "Minor Pentatonic", "Dorian", "Whole Tone" });
    arpCell.setOptions ({ "Up", "Down", "Alternate", "Random", "Scattered" });
    modeCell.setOptions ({ "Chord", "Arp" });
    rateCell.setOptions ({ "1/8", "1/8T", "1/16", "1/16T", "1/32" });
    syncCell.setOptions ({ "Off", "On" });
    curveCell.setOptions ({ "Flat", "Linear", "Exponential", "Random" });
    dirCell.setOptions ({ "Ascending", "Descending" });
    chordCell.box.addItemList ({ "Major", "Minor", "Dim", "Aug", "Sus2", "Sus4", "Minor 7", "Major 7" }, 1);

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
    chordAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, MidiEffectProcessor::chordId, chordCell.box);

    // The pill selectors carry their own attachments (plan 11 D1).
    SuiteUI::attachSelector (scaleCell, *apvts.getParameter (MidiEffectProcessor::scaleId), scaleAttachment);
    SuiteUI::attachSelector (arpCell, *apvts.getParameter (MidiEffectProcessor::arpId), arpAttachment);
    SuiteUI::attachSelector (modeCell, *apvts.getParameter (MidiEffectProcessor::arpModeId), modeAttachment);
    SuiteUI::attachSelector (rateCell, *apvts.getParameter (MidiEffectProcessor::stepRateId), rateAttachment);
    SuiteUI::attachSelector (syncCell, *apvts.getParameter (MidiEffectProcessor::hostSyncId), syncAttachment);
    SuiteUI::attachSelector (curveCell, *apvts.getParameter (MidiEffectProcessor::velocityCurveId), curveCellAttachment);
    SuiteUI::attachSelector (dirCell, *apvts.getParameter (MidiEffectProcessor::velocityDirectionId), dirCellAttachment);

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
    chordCell.setHelpText ("CHORD: choose a value from the list.");
    scaleCell.setHelpText ("The scale the incoming notes are quantised to.");
    arpCell.setHelpText ("How the chord notes are ordered into the 8-step pattern.");
    syncCell.setHelpText ("On: the arp follows the host position and tempo.");

    for (auto* component : { static_cast<juce::Component*> (&rootCtrl), static_cast<juce::Component*> (&octaveCtrl),
                             static_cast<juce::Component*> (&velocityCtrl), static_cast<juce::Component*> (&spreadCtrl),
                             static_cast<juce::Component*> (&tempoCtrl), static_cast<juce::Component*> (&gateCtrl),
                             static_cast<juce::Component*> (&scaleCell), static_cast<juce::Component*> (&chordCell),
                             static_cast<juce::Component*> (&arpCell), static_cast<juce::Component*> (&modeCell),
                             static_cast<juce::Component*> (&rateCell), static_cast<juce::Component*> (&syncCell),
                             static_cast<juce::Component*> (&curveCell), static_cast<juce::Component*> (&dirCell),
                             static_cast<juce::Component*> (&visualizer) })
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

MidiEffectEditor::~MidiEffectEditor()
{
    stopTimer();
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

    // The two group frames and their titles (R5: 15 pt bold in the accent).
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

    // Header: gear at the far right, the preset bar flat to its left (plan 05.1).
    auto headerArea = area.removeFromTop (headerHeight);
    gearButton.setBounds (headerArea.removeFromRight (26).withHeight (26)
                            .withY (headerArea.getCentreY() - 13));
    const int barHeight = presetBar != nullptr ? presetBar->getPreferredHeight() : 0;
    if (barHeight > 0)
        presetBar->setBounds (headerArea.removeFromRight (300).withHeight (barHeight)
                                .withY (headerArea.getCentreY() - barHeight / 2));

    area.removeFromTop (groupGap);

    // The chord/arp visual spans the window between the header and the groups (plan 11 4a).
    visualBounds = area.removeFromTop (visualHeight);
    visualizer.setBounds (visualBounds);

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

        for (auto* cell : { static_cast<juce::Component*> (&modeCell), static_cast<juce::Component*> (&rateCell),
                            static_cast<juce::Component*> (&syncCell),  static_cast<juce::Component*> (&tempoCtrl) })
            cell->setBounds (row1.removeFromLeft (cellWidth).reduced (3));

        for (auto* cell : { static_cast<juce::Component*> (&gateCtrl),  static_cast<juce::Component*> (&curveCell),
                            static_cast<juce::Component*> (&dirCell),   static_cast<juce::Component*> (nullptr) })
        {
            if (cell == nullptr)
                break;
            cell->setBounds (row2.removeFromLeft (cellWidth).reduced (3));
        }
    }

    // SHAPE: root, scale, chord, pattern / octave, spread, velocity.
    {
        auto inner = shapeGroupBounds.withTrimmedTop (groupTitleHeight + 4).reduced (10, 4);
        const int cellWidth = inner.getWidth() / 4;
        auto row1 = inner.removeFromTop (SuiteUI::knobRowHeight);
        auto row2 = inner.removeFromBottom (SuiteUI::knobRowHeight);

        for (auto* cell : { static_cast<juce::Component*> (&rootCtrl),   static_cast<juce::Component*> (&scaleCell),
                            static_cast<juce::Component*> (&chordCell), static_cast<juce::Component*> (&arpCell) })
            cell->setBounds (row1.removeFromLeft (cellWidth).reduced (3));

        for (auto* cell : { static_cast<juce::Component*> (&octaveCtrl),   static_cast<juce::Component*> (&spreadCtrl),
                            static_cast<juce::Component*> (&velocityCtrl), static_cast<juce::Component*> (nullptr) })
        {
            if (cell == nullptr)
                break;
            cell->setBounds (row2.removeFromLeft (cellWidth).reduced (3));
        }
    }
}

//==============================================================================
void MidiEffectEditor::ChordGridVisualizer::paint (juce::Graphics& g)
{
    const auto& snapshot = processor.getArpSnapshot();
    auto area = getLocalBounds().toFloat().reduced (4.0f);
    const auto accent = dynamic_cast<SuiteUI::SuiteLookAndFeel*> (&getLookAndFeel()) != nullptr
                            ? dynamic_cast<SuiteUI::SuiteLookAndFeel&> (getLookAndFeel()).getAccent()
                            : SuiteUI::Colours::text;

    g.setColour (SuiteUI::Colours::panel);
    g.fillRoundedRectangle (area, 6.0f);
    g.setColour (SuiteUI::Colours::panelEdge);
    g.drawRoundedRectangle (area, 6.0f, 1.0f);

    const int key = snapshot.key.load (std::memory_order_relaxed);

    if (key < 0)
    {
        g.setColour (SuiteUI::Colours::textDim);
        g.setFont (SuiteUI::uiFont (13.0f));
        g.drawText ("Play a note to see its chord and arp pattern.", area, juce::Justification::centred);
        return;
    }

    auto strip = area.removeFromTop (area.getHeight() * 0.5f).reduced (8.0f, 6.0f);
    auto keyboard = area.withTrimmedTop (2.0f).reduced (8.0f, 6.0f);

    // The 8-step strip: one cell per step, note name inside, filled while sounding.
    const float cellWidth = strip.getWidth() / (float) MidiEffectProcessor::arpSteps;
    const int currentStep = snapshot.step.load (std::memory_order_relaxed);

    for (int i = 0; i < MidiEffectProcessor::arpSteps; ++i)
    {
        auto cell = juce::Rectangle<float> (strip.getX() + cellWidth * (float) i, strip.getY(),
                                            cellWidth, strip.getHeight());
        cell.reduce (2.0f, 2.0f);

        const int note = snapshot.notes[(size_t) i].load (std::memory_order_relaxed);
        const bool sounding = snapshot.sounding[(size_t) i].load (std::memory_order_relaxed);

        if (sounding)
        {
            g.setColour (accent.withAlpha (0.25f));
            g.fillRoundedRectangle (cell, 4.0f);
        }

        if (i == currentStep)
        {
            g.setColour (accent);
            g.drawRoundedRectangle (cell, 4.0f, 1.6f);
        }
        else
        {
            g.setColour (SuiteUI::Colours::track);
            g.drawRoundedRectangle (cell, 4.0f, 1.0f);
        }

        if (note >= 0)
        {
            g.setColour (sounding ? accent : SuiteUI::Colours::text);
            g.setFont (SuiteUI::uiFont (11.0f, sounding));
            g.drawText (MidiEffectProcessor::noteName (note), cell, juce::Justification::centred);
        }
    }

    // The keyboard: MIDI 48-72, chord tones outlined, sounding notes filled.
    constexpr int firstKey = 48;
    constexpr int numKeys = 25;
    const float keyWidth = keyboard.getWidth() / (float) numKeys;

    g.setColour (SuiteUI::Colours::track);
    g.drawRect (keyboard, 1.0f);

    for (int i = 0; i < numKeys; ++i)
    {
        const int note = firstKey + i;
        const auto keyRect = juce::Rectangle<float> (keyboard.getX() + keyWidth * (float) i, keyboard.getY(),
                                                     keyWidth, keyboard.getHeight());
        const bool black = juce::MidiMessage::isMidiNoteBlack (note);

        if (! black)
        {
            g.setColour (SuiteUI::Colours::panelLight.withAlpha (0.5f));
            g.fillRect (keyRect.reduced (0.5f, 0.0f));
        }
    }

    for (int pass = 0; pass < 2; ++pass)
    {
        for (int i = 0; i < numKeys; ++i)
        {
            const int note = firstKey + i;
            const bool black = juce::MidiMessage::isMidiNoteBlack (note);

            if (black != (pass == 1))
                continue;

            auto keyRect = juce::Rectangle<float> (keyboard.getX() + keyWidth * (float) i, keyboard.getY(),
                                                   keyWidth, keyboard.getHeight());

            bool inPattern = false, sounding = false;
            for (int s = 0; s < MidiEffectProcessor::arpSteps; ++s)
            {
                if (snapshot.notes[(size_t) s].load (std::memory_order_relaxed) == note)
                {
                    inPattern = true;
                    sounding = snapshot.sounding[(size_t) s].load (std::memory_order_relaxed);
                }
            }

            if (! inPattern)
                continue;

            if (black)
            {
                g.setColour (sounding ? accent : SuiteUI::Colours::track);
                g.fillRect (keyRect.reduced (1.0f, 0.0f));
            }
            else
            {
                g.setColour (sounding ? accent : SuiteUI::Colours::track);
                g.fillRect (keyRect.removeFromTop (keyRect.getHeight() * 0.35f).reduced (1.0f, 0.0f));
            }
        }
    }

    // Further keys still sounding: a small marker (D6: markers only, not full patterns).
    const int others = snapshot.others.load (std::memory_order_relaxed);
    if (others > 0)
    {
        g.setColour (SuiteUI::Colours::textDim);
        g.setFont (SuiteUI::uiFont (11.0f));
        g.drawText ("+" + juce::String (others) + " more", area.withTrimmedLeft (8.0f).removeFromBottom (14.0f),
                    juce::Justification::centredLeft);
    }
}
