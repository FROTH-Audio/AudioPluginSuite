#include "SuiteSettings.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace SuiteSettings
{
    namespace
    {
        juce::File defaultSettingsFile()
        {
            return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                    .getChildFile ("FROTH Audio").getChildFile ("SuiteSettings.xml");
        }
    }

    Store::Store() : Store (defaultSettingsFile()) {}

    Store::Store (const juce::File& fileToUse)
        : file (fileToUse)
    {
        rebuildProperties();
    }

    void Store::rebuildProperties()
    {
        props = std::make_unique<juce::PropertiesFile> (file, makeOptions());
    }

    juce::PropertiesFile::Options Store::makeOptions() const
    {
        juce::PropertiesFile::Options options;
        options.millisecondsBeforeSaving = 0;
        return options;
    }

    bool Store::getBool (const juce::String& key, bool defaultValue) const
    {
        return props->getBoolValue (key, defaultValue);
    }

    double Store::getDouble (const juce::String& key, double defaultValue) const
    {
        return props->getDoubleValue (key, defaultValue);
    }

    juce::String Store::getString (const juce::String& key, const juce::String& defaultValue) const
    {
        return props->getValue (key, defaultValue);
    }

    void Store::setBool (const juce::String& key, bool value)
    {
        props->setValue (key, value ? "1" : "0");
    }

    void Store::setDouble (const juce::String& key, double value)
    {
        props->setValue (key, juce::String (value, 3));
    }

    void Store::setString (const juce::String& key, const juce::String& value)
    {
        props->setValue (key, value);
    }

    void Store::reset (const juce::String& key)
    {
        props->removeValue (key);
    }

    bool Store::save()
    {
        file.getParentDirectory().createDirectory();
        return props->saveIfNeeded();
    }

    void Store::setFile (const juce::File& newFile)
    {
        file = newFile;
        rebuildProperties();
    }

    Store& shared()
    {
        static juce::SharedResourcePointer<Store> store;
        return *store;
    }

    SettingsDialog::SettingsDialog (Store& storeToOwn, juce::Component* centreAround)
        : Component(),
          store (storeToOwn),
          showTooltips (true),
          fallbackTempo (120.0),
          tooltipWindow (this)
    {
        setSize (420, 360);
        setWantsKeyboardFocus (false);

        buildTooltipsRow();
        buildTempoRow();
        buildPresetFolderRow();
        if (shouldBuildSampleFolderRow)
            buildSampleFolderRow();

        showTooltips = store.getBool (tooltipsKey, true);
        fallbackTempo = clampFallbackBpm (store.getDouble (fallbackTempoKey, defaultFallbackBpm));
        presetPath = store.getString (presetFolderKey ("SpectralDrift"), juce::String());
        samplePath = store.getString (sgraffitoSampleFolderKey, juce::String());

        updateTooltipsRow();
        updateTempoRow();
        updatePresetFolderRow();
        if (sampleFolderRow.label != nullptr)
            updateSampleFolderRow();

        auto centre = getScreenBounds().getCentre();
        centre.x -= getWidth() / 2;
        centre.y -= getHeight() / 2;
        setTopLeftPosition (static_cast<int>(centre.x), static_cast<int>(centre.y));
    }

    SettingsDialog::~SettingsDialog()
    {
    }

    void SettingsDialog::paint (juce::Graphics& g)
    {
        g.fillAll (juce::Colours::white);
        g.setColour (juce::Colours::black);
        g.drawRect (getLocalBounds().toFloat(), 1.0f);
    }

    void SettingsDialog::updateTooltipsRow()
    {
        if (SuiteSettings::tooltipsRow.label != nullptr)
            SuiteSettings::tooltipsRow.label->setText ("Show tooltips", juce::dontSendNotification);
        
        auto& toggle = *SuiteSettings::tooltipsRow.toggle;
        if (toggle.getToggleState() != showTooltips)
            toggle.setToggleState(showTooltips, juce::dontSendNotification);
        if (showTooltips)
            toggle.setTooltip ("Enable tooltip hints on controls");
        else
            toggle.setTooltip ("Disable tooltip hints on controls");
    }

    void SettingsDialog::updateTempoRow()
    {
        if (SuiteSettings::tempoRow.label != nullptr)
            SuiteSettings::tempoRow.label->setText ("Fallback tempo (when no host tempo)", juce::dontSendNotification);
        
        auto& slider = *SuiteSettings::tempoRow.slider;
        slider.setValue (fallbackTempo);
        if (! showTooltips)
            slider.setTooltip ("Used when the host reports no playhead position");
    }

    void SettingsDialog::updatePresetFolderRow()
    {
        if (SuiteSettings::presetFolderRow.label != nullptr)
            SuiteSettings::presetFolderRow.label->setText ("User preset folder", juce::dontSendNotification);
        
        auto& text = *SuiteSettings::presetFolderRow.text;
        if (presetPath.isEmpty())
            text.setText ("Use default location", juce::dontSendNotification);
        else
            text.setText ("Custom: " + presetPath, juce::dontSendNotification);
    }

    void SettingsDialog::updateSampleFolderRow()
    {
        if (SuiteSettings::sampleFolderRow.label == nullptr)
            return;

        SuiteSettings::sampleFolderRow.label->setText ("Sample search folder", juce::dontSendNotification);
        
        auto& text = *SuiteSettings::sampleFolderRow.text;
        if (samplePath.isEmpty())
            text.setText ("Use default location", juce::dontSendNotification);
        else
            text.setText ("Custom: " + samplePath, juce::dontSendNotification);
    }

    void SettingsDialog::applyTooltipsSetting()
    {
        showTooltips = SuiteSettings::tooltipsRow.toggle->getToggleState();
        store.setBool (tooltipsKey, showTooltips);
        updateTooltipsRow();
    }

    void SettingsDialog::applyTempoSetting()
    {
        fallbackTempo = (*SuiteSettings::tempoRow.slider).getValue();
        store.setDouble (fallbackTempoKey, fallbackTempo);
        updateTempoRow();
    }

    void SettingsDialog::applyPresetFolderSetting()
    {
        presetPath = juce::String((*SuiteSettings::presetFolderRow.text).getText().trim());
        if (! presetPath.isEmpty())
            store.setString (presetFolderKey ("SpectralDrift"), presetPath);
        updatePresetFolderRow();
    }

    void SettingsDialog::applySampleFolderSetting()
    {
        if (SuiteSettings::sampleFolderRow.label == nullptr)
            return;

        auto& text = *SuiteSettings::sampleFolderRow.text;
        samplePath = juce::String(text.getText().trim());
        if (! samplePath.isEmpty())
            store.setString (sgraffitoSampleFolderKey, samplePath);
        updateSampleFolderRow();
    }

    void SettingsDialog::browsePresetFolder()
    {
        auto chooser = new juce::FileChooser ("Select preset folder", presetPath.isEmpty() ? "" : presetPath, "*.zip;*.vst3");
        chooser->launchAsync (0, [this, chooser] (const juce::FileChooser& fc)
        {
            juce::File result = fc.getResult();
            if (! result.isRoot())
            {
                presetPath = result.getFullPathName();
                (*SuiteSettings::presetFolderRow.text).setText (presetPath, juce::sendNotification);
                store.setString (presetFolderKey ("SpectralDrift"), presetPath);
            }
        });
    }

    void SettingsDialog::browseSampleFolder()
    {
        if (SuiteSettings::sampleFolderRow.label == nullptr)
            return;

        auto chooser = new juce::FileChooser ("Select sample folder", samplePath.isEmpty() ? "" : samplePath, "*.wav;*.aiff");
        chooser->launchAsync (0, [this, chooser] (const juce::FileChooser& fc)
        {
            juce::File result = fc.getResult();
            if (! result.isRoot())
            {
                samplePath = result.getFullPathName();
                auto& text = *SuiteSettings::sampleFolderRow.text;
                text.setText (samplePath, juce::sendNotification);
                store.setString (sgraffitoSampleFolderKey, samplePath);
            }
        });
    }

    void SettingsDialog::buttonPressed (juce::Button* buttonThatWasPressed)
    {
        if (buttonThatWasPressed == SuiteSettings::tooltipsRow.toggle)
            applyTooltipsSetting();
        else if (buttonThatWasPressed == SuiteSettings::presetFolderRow.browse)
            browsePresetFolder();
        else if (SuiteSettings::sampleFolderRow.browse != nullptr && buttonThatWasPressed == SuiteSettings::sampleFolderRow.browse)
            browseSampleFolder();

        store.save();
    }

    void SettingsDialog::buildTooltipsRow()
    {
        auto& row = SuiteSettings::tooltipsRow;
        auto label = new juce::Label ("Tooltips", "Tooltips");
        label->setFont (juce::Font (13.0f, juce::Font::bold));
        label->setJustificationType (juce::Justification::centredLeft);
        label->setText ("Show tooltips on controls", juce::dontSendNotification);
        addAndMakeVisible (label);

        auto bounds = getLocalBounds().reduced (12, 8).removeFromTop (16);
        row.toggle = new juce::ToggleButton ("");
        row.toggle->setToggleState (true, juce::sendNotification);
        row.toggle->setBounds (bounds.removeFromLeft (50));
        addAndMakeVisible (row.toggle);

        (*row.valueBox).setText ("ON", juce::dontSendNotification);
        (*row.valueBox).setFont (juce::Font(14.0f));
        (*row.valueBox).setJustificationType (juce::Justification::centred);
        row.toggle->setTooltip ("Enable tooltip hints on controls");
        addAndMakeVisible ((Component*)row.valueBox);

        row.area = bounds;
    }

    void SettingsDialog::buildTempoRow()
    {
        auto& row = SuiteSettings::tempoRow;
        auto label = new juce::Label ("Fallback tempo", "Fallback tempo");
        label->setFont (juce::Font (13.0f, juce::Font::bold));
        label->setJustificationType (juce::Justification::centredLeft);
        label->setText ("Used when the host reports no playhead position", juce::dontSendNotification);
        addAndMakeVisible (label);

        auto bounds = getLocalBounds().reduced (12, 8).removeFromTop (16);
        row.slider = new juce::Slider ("");
        row.slider->setSliderStyle (juce::Slider::LinearVertical);
        row.slider->setBounds (bounds);
        addAndMakeVisible (row.slider);

        row.area = bounds;
    }

    void SettingsDialog::buildPresetFolderRow()
    {
        auto& row = SuiteSettings::presetFolderRow;
        auto label = new juce::Label ("User preset folder", "User preset folder");
        label->setFont (juce::Font (13.0f, juce::Font::bold));
        label->setJustificationType (juce::Justification::centredLeft);
        label->setText ("Browse for a custom location or use the default", juce::dontSendNotification);
        addAndMakeVisible (label);

        auto bounds = getLocalBounds().reduced (12, 8).removeFromTop (16);
        row.text = new juce::Label ("", "");
        row.text->setFont (juce::Font(14.0f));
        row.text->setJustificationType (juce::Justification::centredLeft);
        row.text->setText ("Use default location", juce::dontSendNotification);
        row.text->setBounds (bounds.removeFromLeft (bounds.getWidth() - 40));
        addAndMakeVisible (row.text);

        auto browseBtn = new juce::TextButton ("Browse...");
        browseBtn->setTooltip ("Browse for folder");
        browseBtn->setBounds (bounds);
        row.browse = browseBtn;
        addAndMakeVisible (row.browse);

        row.area = bounds;
    }

    void SettingsDialog::buildSampleFolderRow()
    {
        auto& row = SuiteSettings::sampleFolderRow;
        auto label = new juce::Label ("Sample search folder", "Sample search folder");
        label->setFont (juce::Font (13.0f, juce::Font::bold));
        label->setJustificationType (juce::Justification::centredLeft);
        label->setText ("Browse for a custom location or use the default", juce::dontSendNotification);
        addAndMakeVisible (label);

        auto bounds = getLocalBounds().reduced (12, 8).removeFromTop (16);
        row.text = new juce::Label ("", "");
        row.text->setFont (juce::Font(14.0f));
        row.text->setJustificationType (juce::Justification::centredLeft);
        row.text->setText ("Use default location", juce::dontSendNotification);
        row.text->setBounds (bounds.removeFromLeft (bounds.getWidth() - 40));
        addAndMakeVisible (row.text);

        auto browseBtn = new juce::TextButton ("Browse...");
        browseBtn->setTooltip ("Browse for folder");
        browseBtn->setBounds (bounds);
        row.browse = browseBtn;
        addAndMakeVisible (row.browse);

        row.area = bounds;
    }

    void showSettingsDialog (Store& store, juce::Component* centreAround)
    {
        auto options = juce::DialogWindow::LaunchOptions{};
        options.dialogTitle = "Settings";
        options.content = juce::OptionalScopedPointer<juce::Component>(new SettingsDialog (store, centreAround), true);
        options.componentToCentreAround = centreAround;
        
        options.launchAsync();
    }

}  // namespace SuiteSettings

// Row builder static instance definitions
namespace SuiteSettings {
    TooltipsRowBuilder tooltipsRow = { nullptr, nullptr, nullptr, juce::Rectangle<int>() };
    TempoRowBuilder tempoRow = { nullptr, nullptr, juce::Rectangle<int>() };
    PresetFolderRowBuilder presetFolderRow = { nullptr, nullptr, nullptr, juce::Rectangle<int>() };
    SampleFolderRowBuilder sampleFolderRow = { nullptr, nullptr, nullptr, juce::Rectangle<int>() };
}
