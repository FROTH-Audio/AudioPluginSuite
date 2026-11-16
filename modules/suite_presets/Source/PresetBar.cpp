#include "SuitePresets.h"
#include "PresetBank.h"

#include <algorithm>
#include <cstring>

namespace SuitePresets
{
    namespace
    {
        constexpr juce::uint64 fnvOffset = 1469598103934665603ull;
        constexpr juce::uint64 fnvPrime = 1099511628211ull;

        void mixParameter (juce::uint64& hash, const juce::String& id, float value)
        {
            for (auto character : id)
            {
                hash ^= (juce::uint64) (juce::juce_wchar) character;
                hash *= fnvPrime;
            }

            juce::uint32 bits = 0;
            std::memcpy (&bits, &value, sizeof (bits));
            hash ^= (juce::uint64) bits;
            hash *= fnvPrime;
        }

        juce::uint64 hashState (juce::AudioProcessorValueTreeState& apvts)
        {
            juce::uint64 hash = fnvOffset;

            for (auto* parameter : apvts.processor.getParameters())
            {
                const auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (parameter);
                mixParameter (hash, withId != nullptr ? withId->paramID : juce::String(),
                              parameter->getValue());
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
        /**
            A header button with no fill (plan 08 Q1 "ghost buttons"): a hairline rounded outline,
            dim text, accent on hover. `emphasis` lifts the preset name above the arrow and
            INIT/SAVE buttons: 0 = the quiet controls, 1 = the name.
        */
        struct GhostButton : juce::TextButton
        {
            using TextButton::TextButton;

            float emphasis = 0.0f;
            juce::Colour accent { 0xff888888 };

            void paintButton (juce::Graphics& g, bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown) override
            {
                const bool active = shouldDrawButtonAsHighlighted || shouldDrawButtonAsDown;
                auto bounds = getLocalBounds().toFloat().reduced (0.5f, 0.5f);
                constexpr float corner = 6.0f;

                if (active)
                {
                    g.setColour (accent.withAlpha (shouldDrawButtonAsDown ? 0.14f : 0.07f));
                    g.fillRoundedRectangle (bounds, corner);
                }

                g.setColour (accent.withAlpha (juce::jlimit (0.0f, 0.8f,
                                (0.22f + 0.18f * emphasis) * (active ? 1.8f : 1.0f))));
                g.drawRoundedRectangle (bounds, corner, 1.0f);

                // The suite look and feel never re-colours button text, so draw it here: dim
                // when idle, full white on hover/press.
                g.setFont (getLookAndFeel().getTextButtonFont (*this, getHeight()));
                g.setColour (active ? juce::Colours::white
                                    : juce::Colours::white.withAlpha (0.55f + 0.25f * emphasis));
                g.drawText (getButtonText(), getLocalBounds(), juce::Justification::centred);
            }
        };

        /** The name button: left click opens the browser menu, right click the user-file menu. */
        struct NameButton : GhostButton
        {
            using GhostButton::GhostButton;

            std::function<void()> onContextMenu;

            void mouseDown (const juce::MouseEvent& e) override
            {
                if (e.mods.isPopupMenu())
                {
                    if (onContextMenu)
                        onContextMenu();
                }
                else
                    juce::TextButton::mouseDown (e);
            }
        };

        explicit Impl (PresetBar& ownerIn, StateHolder& holderIn, juce::Colour accentIn)
            : owner (ownerIn), holder (holderIn), accent (accentIn)
        {
            for (const auto& preset : PresetBank::getFactoryPresets())
                if (preset.plugin == holder.getPresetPluginId())
                    factory.push_back (preset);

            refreshUserPresets();

            // Restore the selection the processor remembers (plan 09 1.4): the bar is
            // rebuilt with every editor; the processor is not. An unresolved user file
            // falls back to Init — the audio state is whatever the session left.
            holderSelectionSeen = holder.getPresetSelection();
            restoreSelectionFromHolder (holderSelectionSeen);

            auto setup = [this] (GhostButton& button)
            {
                button.setWantsKeyboardFocus (false);
                button.accent = accent;
                button.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
                button.setColour (juce::TextButton::textColourOnId, juce::Colours::white);
                owner.addAndMakeVisible (button);
            };

            setup (previous);
            setup (nameButton);
            setup (next);
            setup (initButton);
            setup (saveButton);

            // The preset name is the only control that wants a second look; the arrows and
            // INIT/SAVE stay quiet (plan 08 Q1).
            nameButton.emphasis = 1.0f;

            previous.setTooltip ("Previous preset");
            next.setTooltip ("Next preset");
            nameButton.setTooltip ("Preset browser");
            initButton.setTooltip ("Reset every control to its default");
            saveButton.setTooltip ("Save the current sound as a user preset");

            previous.onClick = [this] { step (-1); };
            next.onClick = [this] { step (1); };
            nameButton.onClick = [this] { showMenu(); };
            nameButton.onContextMenu = [this] { showNameContextMenu(); };
            initButton.onClick = [this] { applyInit(); };
            saveButton.onClick = [this] { saveAs(); };

            snapshotHash = currentFlat >= 0 ? hashOfAppliedPreset (at (currentFlat))
                                            : hashState (holder.getPresetStateTree());
            updateName();
            startTimerHz (10);
        }

        ~Impl() override
        {
            stopTimer();
        }

        void paint (juce::Graphics& g)
        {
            // Flat on the header: no fill, no border (plan 05.1). The buttons carry the look.
            juce::ignoreUnused (g);
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

        /** The selection the bar would record for a flat index: factory presets by bank
            index (append-only, stable), user presets by file path. -1 or out of range is none. */
        PresetSelection selectionForFlat (int flatIndex) const
        {
            PresetSelection selection;

            if (flatIndex < 0 || flatIndex >= getTotal())
                return selection;

            if (flatIndex < (int) factory.size())
            {
                selection.kind = PresetSelection::Kind::factory;
                selection.factoryIndex = flatIndex;
            }
            else
            {
                selection.kind = PresetSelection::Kind::userFile;
                selection.userFilePath = at (flatIndex).file.getFullPathName();
            }

            return selection;
        }

        /** Points the bar at the holder's selection: factory by index, user by path in the
            freshly scanned list, anything unresolvable at Init. Never touches the audio
            state: restoring the name re-applies nothing. */
        void restoreSelectionFromHolder (const PresetSelection& selection)
        {
            currentFlat = -1;
            currentName = "Init";
            currentCategory = {};

            if (selection.kind == PresetSelection::Kind::factory
                && selection.factoryIndex >= 0 && selection.factoryIndex < (int) factory.size())
            {
                currentFlat = selection.factoryIndex;
            }
            else if (selection.kind == PresetSelection::Kind::userFile
                     && selection.userFilePath.isNotEmpty())
            {
                for (int i = 0; i < (int) user.size(); ++i)
                {
                    if (user[(size_t) i].file.getFullPathName() == selection.userFilePath)
                    {
                        currentFlat = (int) factory.size() + i;
                        break;
                    }
                }
            }

            if (currentFlat >= 0)
            {
                currentName = at (currentFlat).name;
                currentCategory = at (currentFlat).category;
            }
        }

        /** Tells the processor what the bar just applied, and remembers it so the timer
            does not echo our own change back at us (plan 09 1.4). */
        void recordSelection()
        {
            const auto selection = selectionForFlat (currentFlat);
            holder.setPresetSelection (selection);
            holderSelectionSeen = selection;
        }

        /** The hash of the state a preset's delta would produce: every parameter at the
            delta's value, at its default when the delta does not name it — the same
            resetMissing semantics as applyPresetState, computed without touching the
            live state. It is the clean baseline a restored bar needs, so a
            tweaked-then-reopened session still shows the " *" (plan 09 1.4). */
        juce::uint64 hashOfAppliedPreset (const PresetInfo& preset) const
        {
            auto& apvts = holder.getPresetStateTree();
            juce::uint64 hash = fnvOffset;

            for (auto* parameter : apvts.processor.getParameters())
            {
                const auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (parameter);
                const auto id = withId != nullptr ? withId->paramID : juce::String();

                float denormalised = 0.0f;
                bool found = false;

                for (int c = 0; c < preset.state.getNumChildren(); ++c)
                {
                    if (preset.state.getChild (c).getProperty ("id").toString() != id)
                        continue;

                    const auto value = preset.state.getChild (c).getProperty ("value");
                    denormalised = value.isString() ? value.toString().getFloatValue()
                                                    : (float) (double) value;
                    found = true;
                    break;
                }

                float normalised = parameter->getDefaultValue();

                if (found)
                    if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (parameter))
                        normalised = ranged->convertTo0to1 (denormalised);

                mixParameter (hash, id, normalised);
            }

            return hash;
        }

        /** The clean baseline after a restore: the state the restored preset would
            produce, or the current state when nothing resolved (the pre-plan-09
            behaviour). */
        void refreshSnapshot()
        {
            snapshotHash = currentFlat >= 0 ? hashOfAppliedPreset (at (currentFlat))
                                            : hashState (holder.getPresetStateTree());
            dirty = false;
        }

        void applyFlat (int flatIndex)
        {
            if (flatIndex < 0 || flatIndex >= getTotal())
                return;

            const auto& preset = at (flatIndex);
            holder.applyPresetState (preset.state);

            currentFlat = flatIndex;
            currentName = preset.name;
            currentCategory = preset.category;
            recordSelection();   // the processor remembers, so a rebuilt bar shows it (plan 09 1.4)
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
            currentCategory = {};
            recordSelection();   // Init is a selection too: the processor forgets the preset (plan 09 1.4)
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

            // From Init there is no category yet: step to an end of the full list.
            if (currentFlat < 0)
            {
                const int nextIndex = direction > 0 ? 0 : total - 1;
                applyFlat (nextIndex);
                return;
            }

            // Step within the current group (factory or user) and the current category,
            // wrapping around within that slice.
            const bool inFactory = currentFlat < (int) factory.size();
            const int groupStart = inFactory ? 0 : (int) factory.size();
            const int groupEnd = inFactory ? (int) factory.size() : total;
            const juce::String category = currentCategory;

            std::vector<int> matches;
            matches.reserve (groupEnd - groupStart);

            for (int i = groupStart; i < groupEnd; ++i)
                if (at (i).category == category)
                    matches.push_back (i);

            if (matches.empty())
                return;

            auto pos = std::find (matches.begin(), matches.end(), currentFlat);
            if (pos == matches.end())
            {
                // Current preset is not in its own category slice (should not happen); fall back.
                applyFlat (matches.front());
                return;
            }

            const int index = (int) (pos - matches.begin());
            const int nextIndex = (index + direction + (int) matches.size()) % (int) matches.size();
            applyFlat (matches[nextIndex]);
        }

        void updateName()
        {
            nameButton.setButtonText (dirty ? currentName + " *" : currentName);
        }

        void timerCallback() override
        {
            // Another editor of the same processor may have applied a preset: follow the
            // holder's selection when it moves (multi-editor sync, plan 09 1.4). A
            // selection that cannot resolve (a user file that vanished between scans)
            // stays at what it resolved to; holderSelectionSeen stops the tick from
            // re-processing it forever.
            const auto holderSelection = holder.getPresetSelection();

            if (! (holderSelection == holderSelectionSeen))
            {
                holderSelectionSeen = holderSelection;
                restoreSelectionFromHolder (holderSelection);
                refreshSnapshot();
                updateName();
                return;
            }

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

        /** Right click on the name: Save As, then the user-file actions (factory ones are greyed out). */
        void showNameContextMenu()
        {
            const bool userFile = currentUserIsUserFile();

            juce::PopupMenu menu;
            menu.addItem (910001, "Save As...", true, false);
            menu.addSeparator();
            menu.addItem (910002, "Rename...", userFile, false);
            menu.addItem (910003, "Delete", userFile, false);
            menu.addItem (910004, "Reveal in Explorer/Finder", userFile, false);

            menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&nameButton),
                                [this] (int result)
                                {
                                    switch (result)
                                    {
                                        case 910001: saveAs(); return;
                                        case 910002: renameCurrent(); return;
                                        case 910003: deleteCurrent(); return;
                                        case 910004:
                                            if (currentUserIsUserFile())
                                                currentFile().revealToUser();
                                            return;
                                        default: return;
                                    }
                                });
        }

        bool currentUserIsUserFile() const
        {
            return currentFlat >= (int) factory.size() && currentFlat < getTotal();
        }

        juce::File currentFile() const
        {
            return currentUserIsUserFile() ? at (currentFlat).file : juce::File();
        }

        void renameCurrent()
        {
            if (! currentUserIsUserFile())
                return;

            const auto oldFile = currentFile();

            auto* window = new juce::AlertWindow ("Rename preset",
                                                  "Choose a new name for this preset.",
                                                  juce::MessageBoxIconType::NoIcon);
            window->addTextEditor ("name", currentName, "Name");
            window->addButton ("Rename", 1, juce::KeyPress (juce::KeyPress::returnKey));
            window->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

            window->enterModalState (true,
                                     juce::ModalCallbackFunction::create ([this, oldFile, window] (int result)
                                     {
                                         if (result != 1)
                                             return;

                                         const auto newName = window->getTextEditorContents ("name").trim();

                                         if (newName.isEmpty())
                                         {
                                             showError ("The preset needs a name.");
                                             return;
                                         }

                                         const auto folder = PresetFile::getUserPresetFolder (holder.getPresetPluginId());
                                         const auto newFile = folder.getChildFile (PresetFile::sanitiseFileName (newName) + ".ausp");

                                         if (newFile != oldFile && newFile.existsAsFile())
                                         {
                                             showError ("A preset called \"" + newName + "\" already exists.");
                                             return;
                                         }

                                         const auto read = PresetFile::read (oldFile, holder.getPresetPluginId());

                                         if (! read.ok)
                                         {
                                             showError (read.error);
                                             return;
                                         }

                                         auto renamed = read;
                                         renamed.preset.name = newName;

                                         const auto written = PresetFile::write (newFile, renamed.preset,
                                                                                 read.preset.state,
                                                                                 holder.getPresetStateVersion());

                                         if (! written.ok)
                                         {
                                             showError (written.error);
                                             return;
                                         }

                                         if (newFile != oldFile)
                                             oldFile.deleteFile();

                                         refreshUserPresets();
                                         updateNameAndIndex (newFile, newName);
                                     }),
                                     true);
        }

        void deleteCurrent()
        {
            if (! currentUserIsUserFile())
                return;

            const auto file = currentFile();
            const auto name = currentName;

            messageBox = juce::AlertWindow::showScopedAsync (
                juce::MessageBoxOptions()
                    .withIconType (juce::MessageBoxIconType::QuestionIcon)
                    .withTitle ("Delete preset")
                    .withMessage ("Delete \"" + name + "\"? This cannot be undone.")
                    .withButton ("Delete", 1)
                    .withButton ("Cancel", 0)
                    .withAssociatedComponent (&owner),
                [this, file, name] (int result)
                {
                    if (result != 1)
                        return;

                    if (! file.deleteFile() && file.existsAsFile())
                    {
                        showError ("Could not delete \"" + name + "\".");
                        return;
                    }

                    refreshUserPresets();
                    applyInit();
                });
        }

        /** Re-points the bar at the user preset that lives in this file. */
        void updateNameAndIndex (const juce::File& file, const juce::String& newName)
        {
            currentName = newName;

            for (int i = 0; i < (int) user.size(); ++i)
            {
                if (user[(size_t) i].file == file)
                {
                    currentFlat = (int) factory.size() + i;
                    break;
                }
            }

            currentFlat = (currentFlat >= 0 && currentFlat < getTotal()
                           && at (currentFlat).file == file) ? currentFlat : -1;

            if (currentFlat < 0)
            {
                currentName = "Init";
                currentCategory = {};
            }
            else
            {
                currentCategory = at (currentFlat).category;
            }

            recordSelection();   // the rename moved the file: the identity follows it (plan 09 1.4)

            snapshotHash = hashState (holder.getPresetStateTree());
            dirty = false;
            updateName();
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
        juce::String currentCategory;
        bool dirty = false;
        juce::uint64 snapshotHash = 0;

        // The holder selection this bar last restored or recorded (plan 09 1.4). Message
        // thread only, like everything in the bar.
        PresetSelection holderSelectionSeen;

        GhostButton previous { "<" };
        NameButton nameButton { "Init" };
        GhostButton next { ">" };
        GhostButton initButton { "INIT" };
        GhostButton saveButton { "SAVE" };

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
        return 30;
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

    juce::String PresetBar::getCurrentPresetName() const
    {
        return impl != nullptr ? impl->currentName : juce::String ("Init");
    }

    bool PresetBar::isPresetDirty() const
    {
        return impl != nullptr && impl->dirty;
    }

    int PresetBar::getNumFactoryPresets() const noexcept
    {
        return impl != nullptr ? (int) impl->factory.size() : 0;
    }
}
