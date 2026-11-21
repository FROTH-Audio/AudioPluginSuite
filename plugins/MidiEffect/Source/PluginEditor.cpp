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

    // The choice lists mirror the parameter choice lists; the attachments keep them in
    // sync. Plan 12: the owner named Scale, Pattern, Vel Curve and Step Rate as dropdowns
    // (and CHORD already was one); Mode, Sync and Vel Dir stay pills.
    scaleCell.setOptions ({ "Chromatic", "Major", "Minor", "Minor Pentatonic", "Dorian", "Whole Tone" });
    arpCell.setOptions ({ "Up", "Down", "Alternate", "Random", "Scattered" });
    modeCell.setOptions ({ "Chord", "Arp" });
    rateCell.setOptions ({ "1/4", "1/4T", "1/4D", "1/8", "1/8T", "1/8D",
                           "1/16", "1/16T", "1/16D", "1/32", "1/32T", "1/32D",
                           "1/2", "1 Bar", "2 Bars", "4 Bars" });
    syncCell.setOptions ({ "Off", "On" });
    curveCell.setOptions ({ "Flat", "Linear", "Exponential", "Random" });
    dirCell.setOptions ({ "Ascending", "Descending" });
    chordCell.setOptions ({ "Major", "Minor", "Dim", "Aug", "Sus2", "Sus4", "Minor 7", "Major 7" });

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
    // The dropdowns carry their own attachments (plan 12, one shared dropdown widget).
    SuiteUI::attachDropdown (chordCell, *apvts.getParameter (MidiEffectProcessor::chordId), chordAttachment);
    SuiteUI::attachDropdown (scaleCell, *apvts.getParameter (MidiEffectProcessor::scaleId), scaleAttachment);
    SuiteUI::attachDropdown (arpCell, *apvts.getParameter (MidiEffectProcessor::arpId), arpAttachment);
    SuiteUI::attachDropdown (rateCell, *apvts.getParameter (MidiEffectProcessor::stepRateId), rateAttachment);
    SuiteUI::attachDropdown (curveCell, *apvts.getParameter (MidiEffectProcessor::velocityCurveId), curveCellAttachment);

    // The pills keep theirs (plan 11 D1).
    SuiteUI::attachSelector (modeCell, *apvts.getParameter (MidiEffectProcessor::arpModeId), modeAttachment);
    SuiteUI::attachSelector (syncCell, *apvts.getParameter (MidiEffectProcessor::hostSyncId), syncAttachment);
    SuiteUI::attachSelector (dirCell, *apvts.getParameter (MidiEffectProcessor::velocityDirectionId), dirCellAttachment);

    // Help text on every control (plan 12), through the shared describe() helper.
    SuiteUI::describe (rootCtrl, "The key centre the scale quantisation works from.");
    SuiteUI::describe (octaveCtrl, "Transposes the chord.");
    SuiteUI::describe (velocityCtrl, "Scales the note-on velocity.");
    SuiteUI::describe (spreadCtrl, "Lifts each pass through the chord tones of the 8-step arp pattern by up to an octave.");
    SuiteUI::describe (tempoCtrl, "Tempo used while Host Sync is off.");
    SuiteUI::describe (gateCtrl, "Note length as a share of one step.");
    SuiteUI::describe (modeCell, "Chord fires the whole chord at once; Arp runs the step clock.");
    SuiteUI::describe (rateCell, "The arp's step length: plain, triplet (T) or dotted (D).");
    SuiteUI::describe (syncCell, "On: the arp follows the host position and tempo. Off: it runs on the Tempo parameter.");
    SuiteUI::describe (curveCell, "The per-step velocity shape in Arp mode.");
    SuiteUI::describe (dirCell, "Which end the Linear and Exponential curves start at.");
    SuiteUI::describe (chordCell, "The chord quality built on the quantised note.");
    SuiteUI::describe (scaleCell, "The scale the incoming notes are quantised to.");
    SuiteUI::describe (arpCell, "How the chord notes are ordered into the 8-step pattern.");
    SuiteUI::describe (visualizer, "The 8-step arp strip (click to replay the chord) and the playable keyboard.");
    SuiteUI::describe (*presetBar, "Factory presets, INIT and SAVE. The menu lists user presets.");
    SuiteUI::describe (headerMenu, "Settings... and About...");

    // The help line (plan 12): the footer band, the stale-hover rule and the settings'
    // tooltips toggle, shared by every plugin in the suite.
    helpLine.setIdleText ("Chord mode: a note becomes a chord at once. Arp mode: the chord plays over an 8-step clock.");
    helpLine.setComponentID ("midieffect-help");
    helpLine.setEnablePredicate ([] { return SuiteSettings::shared().getBool (SuiteSettings::tooltipsKey, true); });
    SuiteUI::attachHelpLine (*this, helpLine, helpTracker);
    addAndMakeVisible (helpLine);

    // The playable keyboard and the chord click (plan 13): both run through the
    // processor's UI-note queue, so they take the same chord and arp logic as host notes.
    visualizer.keyboard.onNoteOn = [this] (int note, float velocity)
    {
        processor.pushUiNote (note, true, velocity);
    };
    visualizer.keyboard.onNoteOff = [this] (int note)
    {
        processor.pushUiNote (note, false);
    };
    visualizer.onChordDown = [this] (int key)
    {
        processor.pushUiNote (key, true, 0.8f);
    };
    visualizer.onChordUp = [this] (int key)
    {
        processor.pushUiNote (key, false);
    };

    // The editor takes keyboard focus, so a focus change away from it releases the
    // held UI notes (the note-off guarantee, plan 13).
    setWantsKeyboardFocus (true);

    for (auto* component : { static_cast<juce::Component*> (&rootCtrl), static_cast<juce::Component*> (&octaveCtrl),
                             static_cast<juce::Component*> (&velocityCtrl), static_cast<juce::Component*> (&spreadCtrl),
                             static_cast<juce::Component*> (&tempoCtrl), static_cast<juce::Component*> (&gateCtrl),
                             static_cast<juce::Component*> (&scaleCell), static_cast<juce::Component*> (&chordCell),
                             static_cast<juce::Component*> (&arpCell), static_cast<juce::Component*> (&modeCell),
                             static_cast<juce::Component*> (&rateCell), static_cast<juce::Component*> (&syncCell),
                             static_cast<juce::Component*> (&curveCell), static_cast<juce::Component*> (&dirCell),
                             static_cast<juce::Component*> (&visualizer) })
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

    startTimerHz (SuiteUI::kRefreshHz);
}

MidiEffectEditor::~MidiEffectEditor()
{
    // The note-off guarantee (plan 13): every UI note the editor started gets its
    // note-off before the members start to die. The keyboard strip's own destructor
    // flush is the backstop for anything the strip holds after this.
    releaseUiHeld();

    stopTimer();
    removeMouseListener (&helpTracker);
    setLookAndFeel (nullptr);
}

void MidiEffectEditor::releaseUiHeld()
{
    // The keyboard's release runs its note-off callbacks, which push into the ring.
    visualizer.keyboard.releaseHeldNotes();

    if (visualizer.heldChordKey >= 0)
    {
        visualizer.onChordUp (visualizer.heldChordKey);
        visualizer.heldChordKey = -1;
    }
}

void MidiEffectEditor::focusLost (FocusChangeType)
{
    releaseUiHeld();
}

void MidiEffectEditor::paint (juce::Graphics& g)
{
    juce::ColourGradient background (SuiteUI::Colours::panelLight, 0.0f, 0.0f,
                                     SuiteUI::Colours::background, 0.0f, (float) getHeight(), false);
    g.setGradientFill (background);
    g.fillAll();

    // Brand lockup at the left of the header (the FROTH Audio wordmark, the mark and
    // the product name, generated by the owner's tools/brand/lockups.py (not in this repo) and embedded in the bundle).
    SuiteUI::drawBrandLockup (g, getLocalBounds().removeFromTop (headerHeight).reduced (16, 0), brandLockup());

    // The two group frames (plan 12: the shared frame; the owner's missing backgrounds).
    SuiteUI::drawGroupFrame (g, arpGroupBounds, "ARP", lookAndFeel.getAccent());
    SuiteUI::drawGroupFrame (g, shapeGroupBounds, "SHAPE", lookAndFeel.getAccent());
}

void MidiEffectEditor::resized()
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

    // The help line lives in the band the paint used to draw its footer sentence in.
    helpLine.setBounds (getLocalBounds().removeFromBottom (footerHeight));
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

    auto strip = area;
    auto keyboardBand = strip.removeFromBottom (strip.getHeight() * 0.42f);   // the playable strip (resized)

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

    // Further keys still sounding: a small marker (D6: markers only, not full patterns).
    const int others = snapshot.others.load (std::memory_order_relaxed);
    if (others > 0)
    {
        g.setColour (SuiteUI::Colours::textDim);
        g.setFont (SuiteUI::uiFont (11.0f));
        g.drawText ("+" + juce::String (others) + " more", strip.withTrimmedRight (8.0f).removeFromBottom (14.0f),
                    juce::Justification::centredRight);
    }
}

void MidiEffectEditor::ChordGridVisualizer::resized()
{
    // The playable keyboard fills the bottom band of the visual area (plan 13).
    auto inner = getLocalBounds().reduced (4);
    auto band = inner.removeFromBottom ((int) ((float) inner.getHeight() * 0.42f));
    keyboard.setBounds (band.reduced (8, 4));
}

void MidiEffectEditor::ChordGridVisualizer::mouseDown (const juce::MouseEvent&)
{
    // A click on the strip replays the current chord through the UI-note queue (plan 13).
    const int key = processor.getArpSnapshot().key.load (std::memory_order_relaxed);
    if (key < 0 || heldChordKey >= 0)
        return;

    heldChordKey = key;

    if (onChordDown)
        onChordDown (key);
}

void MidiEffectEditor::ChordGridVisualizer::mouseUp (const juce::MouseEvent&)
{
    if (heldChordKey < 0)
        return;

    const int key = heldChordKey;
    heldChordKey = -1;

    if (onChordUp)
        onChordUp (key);
}

void MidiEffectEditor::ChordGridVisualizer::mouseExit (const juce::MouseEvent&)
{
    // The pointer leaving the strip releases the chord click (plan 13).
    if (heldChordKey < 0)
        return;

    const int key = heldChordKey;
    heldChordKey = -1;

    if (onChordUp)
        onChordUp (key);
}

void MidiEffectEditor::timerCallback()
{
    visualizer.repaint();

    // Plan 15 (owner): the arp-only controls are inert in Chord mode (Chord fires the whole
    // chord event for event: no clock, no gate, no per-step shape). Dimmed, never hidden
    // (the plan 10 G1 rule). The chord parameters and the Arp pattern's ordering stay live.
    const bool arpMode = processor.apvts.getRawParameterValue (MidiEffectProcessor::arpModeId)->load() > 0.5f;

    rateCell.setEnabled (arpMode);      // Step Rate
    syncCell.setEnabled (arpMode);      // Host Sync
    tempoCtrl.setEnabled (arpMode);
    gateCtrl.setEnabled (arpMode);      // Gate Length
    curveCell.setEnabled (arpMode);     // Velocity Curve
    dirCell.setEnabled (arpMode);       // Velocity Direction
    spreadCtrl.setEnabled (arpMode);    // Octave Spread

    // The keyboard lights the sounding chord tones (plan 13), and the root marker
    // follows the Root parameter.
    const auto& snapshot = processor.getArpSnapshot();
    bool notes[128] = {};

    for (int i = 0; i < MidiEffectProcessor::arpSteps; ++i)
    {
        const int note = snapshot.notes[(size_t) i].load (std::memory_order_relaxed);
        if (note >= 0 && snapshot.sounding[(size_t) i].load (std::memory_order_relaxed))
            notes[note] = true;
    }

    visualizer.keyboard.setActiveNotes (notes);

    if (auto* rootParameter = processor.apvts.getRawParameterValue (MidiEffectProcessor::rootId))
        visualizer.keyboard.setRootKey ((int) (rootParameter->load() + 0.5f));
}
