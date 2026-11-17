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

    SettingsDialog::SettingsDialog (Store& storeToUse, bool showSampleFolderRow)
        : store (storeToUse),
          tooltipWindow (this)
    {
        setWantsKeyboardFocus (false);
        setLookAndFeel (&lookAndFeel);

        showTooltips = store.getBool (tooltipsKey, true);
        fallbackTempo = clampFallbackBpm (store.getDouble (fallbackTempoKey, defaultFallbackBpm));
        presetPath = store.getString (presetFolderKey ("SpectralDrift"), juce::String());
        samplePath = store.getString (sgraffitoSampleFolderKey, juce::String());

        auto captionFont = SuiteUI::uiFont (13.0f, true);
        auto valueFont = SuiteUI::uiFont (14.0f);
        auto asCaption = [&captionFont] (juce::Label& label, const juce::String& text)
        {
            label.setFont (captionFont);
            label.setJustificationType (juce::Justification::centredLeft);
            label.setColour (juce::Label::textColourId, SuiteUI::Colours::text);
            label.setText (text, juce::dontSendNotification);
        };
        auto asValueText = [&valueFont] (juce::Label& label, juce::Colour colour)
        {
            label.setFont (valueFont);
            label.setJustificationType (juce::Justification::centredLeft);
            label.setColour (juce::Label::textColourId, colour);
        };

        // Tooltips row: toggle and On/Off text.
        asCaption (tooltipsCaption, "Tooltips on controls");
        tooltipsToggle.setToggleState (showTooltips, juce::dontSendNotification);
        tooltipsToggle.onStateChange = [this] { applyTooltipsSetting(); };
        asValueText (tooltipsValue, SuiteUI::Colours::text);
        addAndMakeVisible (tooltipsCaption);
        addAndMakeVisible (tooltipsToggle);
        addAndMakeVisible (tooltipsValue);

        // Fallback tempo row.
        asCaption (tempoCaption, "Fallback tempo (when the host reports none)");
        tempoSlider.setSliderStyle (juce::Slider::LinearHorizontal);
        tempoSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 70, 22);
        tempoSlider.setRange (minFallbackBpm, maxFallbackBpm, 1.0);
        tempoSlider.setTextValueSuffix (" BPM");
        tempoSlider.setValue (fallbackTempo, juce::dontSendNotification);
        tempoSlider.onValueChange = [this]
        {
            fallbackTempo = tempoSlider.getValue();
            store.setDouble (fallbackTempoKey, fallbackTempo);
            store.save();
        };
        addAndMakeVisible (tempoCaption);
        addAndMakeVisible (tempoSlider);

        // User preset folder row.
        asCaption (presetFolderCaption, "User preset folder");
        asValueText (presetFolderText, SuiteUI::Colours::textDim);
        presetFolderBrowse.onClick = [this] { browse (false); };
        presetFolderBrowse.setColour (juce::TextButton::buttonColourId, SuiteUI::Colours::panelLight);
        presetFolderBrowse.setColour (juce::TextButton::textColourOffId, SuiteUI::Colours::text);
        addAndMakeVisible (presetFolderCaption);
        addAndMakeVisible (presetFolderText);
        addAndMakeVisible (presetFolderBrowse);

        // Sgraffito's sample search folder row, only when the caller asks for it.
        if (showSampleFolderRow)
        {
            asCaption (sampleFolderCaption, "Sgraffito sample search folder");
            asValueText (sampleFolderText, SuiteUI::Colours::textDim);
            sampleFolderBrowse.onClick = [this] { browse (true); };
            sampleFolderBrowse.setColour (juce::TextButton::buttonColourId, SuiteUI::Colours::panelLight);
            sampleFolderBrowse.setColour (juce::TextButton::textColourOffId, SuiteUI::Colours::text);
            addAndMakeVisible (sampleFolderCaption);
            addAndMakeVisible (sampleFolderText);
            addAndMakeVisible (sampleFolderBrowse);
        }

        applyTooltipsSetting();
        updateFolderTexts();

        setSize (420, showSampleFolderRow ? 236 : 184);
    }

    SettingsDialog::~SettingsDialog()
    {
        setLookAndFeel (nullptr);
    }

    void SettingsDialog::paint (juce::Graphics& g)
    {
        // The suite's dark panel, with the track colour as the border (plan 10 phase 5b).
        // The suite look and feel supplies the control drawing, and every text colour is
        // set explicitly, so no host look and feel can make this dialog unreadable again.
        g.fillAll (SuiteUI::Colours::panel);
        g.setColour (SuiteUI::Colours::track);
        g.drawRect (getLocalBounds().toFloat(), 1.0f);
    }

    void SettingsDialog::resized()
    {
        auto area = getLocalBounds().reduced (16, 12);

        // Tooltips row: toggle and On/Off text under the caption.
        tooltipsCaption.setBounds (area.removeFromTop (18));
        auto toggleRow = area.removeFromTop (26);
        area.removeFromTop (8);
        tooltipsToggle.setBounds (toggleRow.removeFromLeft (46));
        toggleRow.removeFromLeft (8);
        tooltipsValue.setBounds (toggleRow.removeFromLeft (46));

        // Fallback tempo row: slider under the caption.
        tempoCaption.setBounds (area.removeFromTop (18));
        tempoSlider.setBounds (area.removeFromTop (26));
        area.removeFromTop (8);

        // User preset folder row: text and Browse under the caption.
        presetFolderCaption.setBounds (area.removeFromTop (18));
        auto presetRow = area.removeFromTop (26);
        area.removeFromTop (8);
        presetFolderBrowse.setBounds (presetRow.removeFromRight (80));
        presetRow.removeFromRight (8);
        presetFolderText.setBounds (presetRow);

        // Sgraffito's sample search folder row: same shape, only when built.
        if (sampleFolderCaption.getParentComponent() == this)
        {
            sampleFolderCaption.setBounds (area.removeFromTop (18));
            auto sampleRow = area.removeFromTop (26);
            sampleFolderBrowse.setBounds (sampleRow.removeFromRight (80));
            sampleRow.removeFromRight (8);
            sampleFolderText.setBounds (sampleRow);
        }
    }

    void SettingsDialog::updateFolderTexts()
    {
        presetFolderText.setText (presetPath.isEmpty() ? "Use default location" : "Custom: " + presetPath,
                                  juce::dontSendNotification);
        sampleFolderText.setText (samplePath.isEmpty() ? "Use default location" : "Custom: " + samplePath,
                                  juce::dontSendNotification);
    }

    void SettingsDialog::applyTooltipsSetting()
    {
        showTooltips = tooltipsToggle.getToggleState();
        store.setBool (tooltipsKey, showTooltips);
        tooltipsValue.setText (showTooltips ? "On" : "Off", juce::dontSendNotification);

        if (showTooltips)
        {
            tooltipsToggle.setTooltip ("Enable tooltip hints on all suite controls");
            tempoSlider.setTooltip ("Used when the host reports no tempo");
            presetFolderBrowse.setTooltip ("Choose the folder for user presets");
            sampleFolderBrowse.setTooltip ("Choose the folder Sgraffito searches for samples");
        }
        else
        {
            tooltipsToggle.setTooltip ({});
            tempoSlider.setTooltip ({});
            presetFolderBrowse.setTooltip ({});
            sampleFolderBrowse.setTooltip ({});
        }

        store.save();
    }

    void SettingsDialog::browse (bool forSampleFolder)
    {
        chooser = std::make_unique<juce::FileChooser> ("Select a folder");
        juce::Component::SafePointer<SettingsDialog> safeThis (this);

        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                              [safeThis, forSampleFolder] (const juce::FileChooser& fc)
        {
            if (safeThis == nullptr || fc.getResults().isEmpty())
                return;

            const auto path = fc.getResult().getFullPathName();
            if (path.isEmpty())
                return;

            auto& dialog = *safeThis;
            if (forSampleFolder)
            {
                dialog.samplePath = path;
                dialog.store.setString (sgraffitoSampleFolderKey, path);
            }
            else
            {
                dialog.presetPath = path;
                dialog.store.setString (presetFolderKey ("SpectralDrift"), path);
            }
            dialog.updateFolderTexts();
            dialog.store.save();
        });
    }

    void showSettingsDialog (Store& store, juce::Component* centreAround, bool showSampleFolderRow)
    {
        auto options = juce::DialogWindow::LaunchOptions{};
        options.dialogTitle = "Settings";
        options.dialogBackgroundColour = SuiteUI::Colours::panel;   // the title bar matches the content
        options.content = juce::OptionalScopedPointer<juce::Component> (
            new SettingsDialog (store, showSampleFolderRow), true);
        options.componentToCentreAround = centreAround;

        options.launchAsync();
    }

}  // namespace SuiteSettings
