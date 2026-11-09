#include "SuitePresetsPro.h"

#include <cstring>

namespace SuitePresets
{
    namespace
    {
        juce::uint64 hashState (juce::AudioProcessorValueTreeState& apvts)
        {
            juce::uint64 hash = 1469598103934665603ull;

            for (auto* parameter : apvts.processor.getParameters())
            {
                const auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (parameter);
                const auto id = withId != nullptr ? withId->paramID : juce::String();

                for (auto character : id)
                {
                    hash ^= (juce::uint64) (juce::juce_wchar) character;
                    hash *= 1099511628211ull;
                }

                const auto value = parameter->getValue();
                juce::uint32 bits = 0;
                std::memcpy (&bits, &value, sizeof (bits));
                hash ^= (juce::uint64) bits;
                hash *= 1099511628211ull;
            }

            return hash;
        }

        juce::StringArray categoriesOf (const std::vector<PresetInfo>& factory,
                                        const std::vector<PresetInfo>& user)
        {
            juce::StringArray categories;

            for (const auto& preset : factory)
                if (! categories.contains (preset.category))
                    categories.add (preset.category);

            for (const auto& preset : user)
                if (! categories.contains (preset.category))
                    categories.add (preset.category);

            if (categories.isEmpty())
                categories.add ("User");

            return categories;
        }
    }

    struct PresetBar::Impl : private juce::Timer
    {
        explicit Impl (PresetBar& ownerIn, StateHolder& holderIn, juce::Colour accentIn)
            : owner (ownerIn), holder (holderIn), accent (accentIn)
        {
            for (const auto& preset : PresetBank::getFactoryPresets())
                if (preset.plugin == holder.getPresetPluginId())
                    factory.push_back (preset);

            refreshUserPresets();

            auto setup = [this] (juce::TextButton& button)
            {
                button.setWantsKeyboardFocus (false);
                button.setColour (juce::TextButton::buttonColourId, accent.withAlpha (0.16f));
                button.setColour (juce::TextButton::buttonOnColourId, accent.withAlpha (0.30f));
                button.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
                button.setColour (juce::TextButton::textColourOnId, juce::Colours::white);
                owner.addAndMakeVisible (button);
            };

            setup (previous);
            setup (nameButton);
            setup (next);
            setup (initButton);
            setup (saveButton);

            previous.setTooltip ("Previous preset");
            next.setTooltip ("Next preset");
            nameButton.setTooltip ("Preset browser");
            initButton.setTooltip ("Reset every control to its default");
            saveButton.setTooltip ("Save the current sound as a user preset");

            previous.onClick = [this] { step (-1); };
            next.onClick = [this] { step (1); };
            nameButton.onClick = [this] { showMenu(); };
            initButton.onClick = [this] { applyInit(); };
            saveButton.onClick = [this] { saveAs(); };

            snapshotHash = hashState (holder.getPresetStateTree());
            updateName();
            startTimerHz (10);
        }

        ~Impl() override
        {
            stopTimer();
        }

        void paint (juce::Graphics& g)
        {
            const auto bounds = owner.getLocalBounds();

            g.setColour (accent.withAlpha (0.10f));
            g.fillRoundedRectangle (bounds.toFloat(), 4.0f);
            g.setColour (accent.withAlpha (0.45f));
            g.drawRoundedRectangle (bounds.toFloat().reduced (0.5f), 4.0f, 1.0f);
        }

        void resized()
        {
            auto bounds = owner.getLocalBounds().reduced (4, 3);
            previous.setBounds (bounds.removeFromLeft (26));
            bounds.removeFromLeft (2);
            next.setBounds (bounds.removeFromLeft (26));
            bounds.removeFromRight (2);
            saveButton.setBounds (bounds.removeFromRight (52));
            bounds.removeFromRight (2);
            initButton.setBounds (bounds.removeFromRight (52));
            bounds.removeFromRight (4);
            nameButton.setBounds (bounds);
        }

        int getTotal() const noexcept
        {
            return (int) (factory.size() + user.size());
        }

        const PresetInfo& at (int flatIndex) const
        {
            return flatIndex < (int) factory.size() ? factory[(size_t) flatIndex]
                                                     : user[(size_t) (flatIndex - (int) factory.size())];
        }

        void applyFlat (int flatIndex)
        {
            if (flatIndex < 0 || flatIndex >= getTotal())
                return;

            const auto& preset = at (flatIndex);
            holder.applyPresetState (preset.state);

            currentFlat = flatIndex;
            currentName = preset.name;
            snapshotHash = hashState (holder.getPresetStateTree());
            dirty = false;
            updateName();

            if (owner.onPresetLoaded)
                owner.onPresetLoaded();
        }

        void applyInit()
        {
            // An empty <PARAMETERS> tree: applyPresetState() resets every absent parameter to its
            // default, which is exactly what Init means.
            holder.applyPresetState (juce::ValueTree ("PARAMETERS"));

            currentFlat = -1;
            currentName = "Init";
            snapshotHash = hashState (holder.getPresetStateTree());
            dirty = false;
            updateName();

            if (owner.onPresetLoaded)
                owner.onPresetLoaded();
        }

        void step (int direction)
        {
            const int total = getTotal();

            if (total == 0)
                return;

            const int nextIndex = currentFlat < 0
                                      ? (direction > 0 ? 0 : total - 1)
                                      : (currentFlat + direction + total) % total;

            applyFlat (nextIndex);
        }

        void updateName()
        {
            nameButton.setButtonText (dirty ? currentName + " *" : currentName);
        }

        void timerCallback() override
        {
            const bool nowDirty = hashState (holder.getPresetStateTree()) != snapshotHash;

            if (nowDirty != dirty)
            {
                dirty = nowDirty;
                updateName();
            }
        }

        void refreshUserPresets()
        {
            user = PresetFile::scanUserPresets (holder.getPresetPluginId());
        }

        void showMenu()
        {
            auto model = PresetMenuModel::build (factory, user, currentFlat);
            auto menu = model.createMenu();
            menu.addSeparator();
            menu.addItem (900001, "Init");
            menu.addItem (900002, "Show preset folder");

            menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&nameButton),
                                [this] (int result)
                                {
                                    if (result == 0)
                                        return;

                                    if (result == 900001)
                                    {
                                        applyInit();
                                        return;
                                    }

                                    if (result == 900002)
                                    {
                                        auto folder = PresetFile::getUserPresetFolder (holder.getPresetPluginId());
                                        folder.createDirectory();
                                        folder.revealToUser();
                                        return;
                                    }

                                    applyFlat (result - 1);
                                });
        }

        void saveAs()
        {
            auto* window = new juce::AlertWindow ("Save preset",
                                                  "Choose a name and a category for this preset.",
                                                  juce::MessageBoxIconType::NoIcon);

            const auto initialName = (currentFlat >= 0 && currentFlat < getTotal()) ? currentName : juce::String ("My Preset");
            const auto initialCategory = (currentFlat >= 0 && currentFlat < getTotal()) ? at (currentFlat).category
                                                                                       : juce::String ("User");

            window->addTextEditor ("name", initialName, "Name");
            window->addComboBox ("category", categoriesOf (factory, user), "Category");
            window->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
            window->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

            if (auto* combo = window->getComboBoxComponent ("category"))
            {
                combo->setEditableText (true);
                combo->setText (initialCategory, juce::dontSendNotification);
            }

            window->enterModalState (true,
                                     juce::ModalCallbackFunction::create ([this, window] (int result)
                                     {
                                         if (result != 1)
                                             return;

                                         const auto name = window->getTextEditorContents ("name").trim();
                                         auto* combo = window->getComboBoxComponent ("category");
                                         const auto category = combo != nullptr ? combo->getText().trim()
                                                                                : juce::String ("User");

                                         if (name.isEmpty())
                                         {
                                             showError ("The preset needs a name.");
                                             return;
                                         }

                                         writePreset (name, category.isEmpty() ? juce::String ("User") : category);
                                     }),
                                     true);
        }

        void writePreset (const juce::String& name, const juce::String& category)
        {
            const auto folder = PresetFile::getUserPresetFolder (holder.getPresetPluginId());
            const auto file = folder.getChildFile (PresetFile::sanitiseFileName (name) + ".ausp");

            if (file.existsAsFile())
            {
                messageBox = juce::AlertWindow::showScopedAsync (
                    juce::MessageBoxOptions()
                        .withIconType (juce::MessageBoxIconType::QuestionIcon)
                        .withTitle ("Overwrite preset?")
                        .withMessage ("A preset called \"" + name + "\" already exists. Replace it?")
                        .withButton ("Replace", 1)
                        .withButton ("Cancel", 0)
                        .withAssociatedComponent (&owner),
                    [this, file, name, category] (int result)
                    {
                        if (result == 1)
                            writePresetFile (file, name, category);
                    });

                return;
            }

            writePresetFile (file, name, category);
        }

        void writePresetFile (const juce::File& file, const juce::String& name, const juce::String& category)
        {
            PresetInfo preset;
            preset.name = name;
            preset.category = category;
            preset.description = category;
            preset.plugin = holder.getPresetPluginId();
            preset.isFactory = false;

            const auto result = PresetFile::write (file, preset,
                                                   holder.getPresetStateTree().copyState(),
                                                   holder.getPresetStateVersion());

            if (! result.ok)
            {
                showError (result.error);
                return;
            }

            refreshUserPresets();

            for (int i = 0; i < (int) user.size(); ++i)
            {
                if (user[(size_t) i].file == file)
                {
                    applyFlat ((int) factory.size() + i);
                    break;
                }
            }
        }

        void showError (const juce::String& message)
        {
            messageBox = juce::AlertWindow::showScopedAsync (
                juce::MessageBoxOptions()
                    .withIconType (juce::MessageBoxIconType::WarningIcon)
                    .withTitle ("Preset")
                    .withMessage (message)
                    .withButton ("OK", 1)
                    .withAssociatedComponent (&owner),
                [] (int) {});
        }

        PresetBar& owner;
        StateHolder& holder;
        juce::Colour accent;

        std::vector<PresetInfo> factory;
        std::vector<PresetInfo> user;

        int currentFlat = -1;
        juce::String currentName { "Init" };
        bool dirty = false;
        juce::uint64 snapshotHash = 0;

        juce::TextButton previous { "<" };
        juce::TextButton nameButton { "Init" };
        juce::TextButton next { ">" };
        juce::TextButton initButton { "INIT" };
        juce::TextButton saveButton { "SAVE" };

        juce::ScopedMessageBox messageBox;
    };

    PresetBar::PresetBar (StateHolder& holder, juce::Colour accent)
        : impl (std::make_unique<Impl> (*this, holder, accent))
    {
        setWantsKeyboardFocus (false);
    }

    PresetBar::~PresetBar() = default;

    int PresetBar::getPreferredHeight() const noexcept
    {
        return isAvailable() ? 30 : 0;
    }

    void PresetBar::paint (juce::Graphics& g)
    {
        if (impl != nullptr)
            impl->paint (g);
    }

    void PresetBar::resized()
    {
        if (impl != nullptr)
            impl->resized();
    }

    void PresetBar::selectPreset (int factoryIndex)
    {
        if (impl != nullptr)
            impl->applyFlat (factoryIndex);
    }

    int PresetBar::getNumFactoryPresets() const noexcept
    {
        return impl != nullptr ? (int) impl->factory.size() : 0;
    }
}
