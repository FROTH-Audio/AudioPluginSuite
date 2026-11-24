#include "PluginEditor.h"

#include "SuiteSettings.h"

namespace
{
    // Plan 05 R4: the default window stays inside the suite budget (1000x700). The knob
    // size comes from the SuiteUI tokens (R1); never set slider bounds that exceed them.
    constexpr int editorWidth = 640;
    constexpr int editorHeight = 420;
    constexpr int headerHeight = 44;
    constexpr int footerHeight = 26;

    // Preset bar and menu share the header with the title (R12/R13).
    constexpr int titleWidth = 200;
    constexpr int presetBarWidth = 260;
    constexpr int menuSize = 26;
}

__NAME__Editor::__NAME__Editor (__NAME__Processor& p)
    : AudioProcessorEditor (&p),
      processor (p),
      gainAttachment (p.apvts, __NAME__Processor::gainId, gainSlider.slider)
{
    setLookAndFeel (&lookAndFeel);

    // The bar exists before setSize(), because setSize() runs resized(), which lays it out.
    presetBar.reset (new SuitePresets::PresetBar (processor, juce::Colour (0xff9aa0a6)));

    setSize (editorWidth, editorHeight);
    setResizable (true, true);
    setResizeLimits (editorWidth - 60, editorHeight - 60, 1200, 1200);

    addAndMakeVisible (gainSlider);

    // R12: the bar ships in every build; without the factory bank it lists user presets only.
    if (presetBar->getPreferredHeight() > 0)
        addAndMakeVisible (*presetBar);
    presetBar->onPresetLoaded = [this] { repaint(); };

    // R13: one menu with Settings... and About..., no separate buttons (plan 08 Q3).
    addAndMakeVisible (headerMenu);
    headerMenu.onAbout = [this]
    {
        // Pass the lockup image here once the plugin has one (LOCKUP in CMake).
        SuiteUI::showAboutDialog ({}, SUITE_PLUGIN_NAME, SUITE_PLUGIN_VERSION,
                                  SUITE_PLUGIN_CODE, SUITE_PLUGIN_ID, this);
    };
    headerMenu.onSettings = [this]
    {
        SuiteSettings::showSettingsDialog (SuiteSettings::shared(), this);
    };
}

__NAME__Editor::~__NAME__Editor()
{
    setLookAndFeel (nullptr);
}

void __NAME__Editor::paint (juce::Graphics& g)
{
    g.fillAll (SuiteUI::Colours::background);

    // Header: the brand lockup goes here once the plugin has one (LOCKUP in CMake, see
    // the other plugins and SuiteUI::drawBrandLockup). The product name uses the suite font
    // and stays left of the preset bar.
    g.setColour (lookAndFeel.getAccent());
    g.setFont (SuiteUI::uiFont (15.0f, true));
    auto titleArea = getLocalBounds().removeFromTop (headerHeight).reduced (16, 0);
    titleArea.setWidth (juce::jmin (titleWidth, titleArea.getWidth()));
    g.drawText ("__PRODUCT__", titleArea, juce::Justification::centredLeft);

    g.setColour (SuiteUI::Colours::textDim);
    g.setFont (SuiteUI::uiFont (11.0f));
    g.drawText ("Hover a control to see what it does.",
                getLocalBounds().removeFromBottom (footerHeight).reduced (16, 0),
                juce::Justification::centredLeft);
}

void __NAME__Editor::resized()
{
    auto area = getLocalBounds().reduced (16);

    auto header = area.removeFromTop (headerHeight);

    // R13: the menu button sits at the far right of the header, right of the preset bar.
    headerMenu.setBounds (header.removeFromRight (menuSize).withSizeKeepingCentre (menuSize, menuSize));

    // R12: the bar sits flat in the header, right of the title.
    header.removeFromLeft (juce::jmin (titleWidth, header.getWidth()));
    auto barArea = header.removeFromRight (juce::jmin (presetBarWidth, header.getWidth()));
    presetBar->setBounds (barArea.withSizeKeepingCentre (barArea.getWidth(),
                                                         juce::jmin (30, barArea.getHeight())));

    area.removeFromTop (8);
    area.removeFromBottom (footerHeight);

    // One knob row at the suite size (SuiteUI::knobRowHeight); the widget caps the knob.
    gainSlider.setBounds (area.removeFromTop (SuiteUI::knobRowHeight));
}
