/*  SuiteSettingsTests: the shared plugin settings store.

    Run with a directory argument (CTest passes the build's test-settings folder); without one
    a sibling of the working directory is used. The tests redirect every Store to files under
    that folder and must never touch the real user settings file.
*/

#undef NDEBUG
#include <cassert>
#include <cstdio>
#include <juce_gui_basics/juce_gui_basics.h>

#include "../include/SuiteSettings.h"

using namespace SuiteSettings;

namespace
{
    /** argv[1], when CTest passes it, names the folder under build/ the tests may use. */
    juce::File testRoot (int argc, char** argv)
    {
        if (argc > 1 && juce::String (juce::CharPointer_UTF8 (argv[1])).isNotEmpty())
            return juce::File::getCurrentWorkingDirectory().getChildFile (juce::CharPointer_UTF8 (argv[1]));

        return juce::File::getCurrentWorkingDirectory().getChildFile ("test-settings");
    }
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    const auto root = testRoot (argc, argv);
    root.deleteRecursively();
    root.createDirectory();

    // 1. Defaults: a store on a file that does not exist falls back to every default.
    const auto fileA = root.getChildFile ("store_a.xml");
    {
        Store store (fileA);
        assert (store.getBool (tooltipsKey, true));
        assert (store.getDouble (fallbackTempoKey, defaultFallbackBpm) == defaultFallbackBpm);
        assert (store.getString (presetFolderKey ("SpectralDrift"), juce::String()).isEmpty());
        assert (! fileA.existsAsFile());                       // reading never writes
        assert (store.getFile() == fileA);
    }

    // 2. Round trip: set, save, and a fresh store reads the values back.
    {
        Store store (fileA);
        store.setBool (tooltipsKey, false);
        store.setDouble (fallbackTempoKey, 172.5);
        store.setString (presetFolderKey ("SpectralDrift"), "C:/presets/drift");
        assert (store.save());
        assert (fileA.existsAsFile());
    }
    {
        Store store (fileA);
        assert (! store.getBool (tooltipsKey, true));
        assert (store.getDouble (fallbackTempoKey, defaultFallbackBpm) == 172.5);
        assert (store.getString (presetFolderKey ("SpectralDrift"), juce::String())
                 == "C:/presets/drift");
    }

    // 3. reset(): the key is gone, the default applies again, and the change persists.
    {
        Store store (fileA);
        store.reset (tooltipsKey);
        store.save();
    }
    {
        Store store (fileA);
        assert (store.getBool (tooltipsKey, true));
        assert (store.getDouble (fallbackTempoKey, defaultFallbackBpm) == 172.5);  // untouched
    }

    // 4. The clamp helper keeps the fallback tempo inside the legal band.
    assert (clampFallbackBpm (500.0) == maxFallbackBpm);
    assert (clampFallbackBpm (5.0) == minFallbackBpm);
    assert (clampFallbackBpm (132.0) == 132.0);

    // 5. A corrupt settings file is ignored: defaults come back, and saving recovers the file.
    const auto fileB = root.getChildFile ("store_b.xml");
    fileB.replaceWithText ("this is not XML at all <");
    {
        Store store (fileB);
        assert (store.getBool (tooltipsKey, true));
        assert (store.getDouble (fallbackTempoKey, defaultFallbackBpm) == defaultFallbackBpm);
        store.setBool (tooltipsKey, false);
        assert (store.save());
    }
    {
        Store store (fileB);
        assert (! store.getBool (tooltipsKey, true));
    }

    // 6. setFile(): the store switches files and reloads, in both directions.
    {
        Store store (fileA);
        assert (store.getDouble (fallbackTempoKey, defaultFallbackBpm) == 172.5);
        store.setFile (fileB);
        assert (! store.getBool (tooltipsKey, true));          // fileB was saved with false above
        store.setFile (fileA);
        assert (store.getDouble (fallbackTempoKey, defaultFallbackBpm) == 172.5);
    }

    // 7. Every plugin id forms its own folder key; they do not collide.
    {
        Store store (fileA);
        store.setString (presetFolderKey ("SpectralDrift"), "A");
        store.setString (presetFolderKey ("Sgraffito"), "B");
        assert (store.getString (presetFolderKey ("SpectralDrift"), juce::String()) == "A");
        assert (store.getString (presetFolderKey ("Sgraffito"), juce::String()) == "B");
    }

    // 8. shared(): one process-wide instance, used for read-only probing here. Its default file
    //    lives in the user's application data folder; no test writes through it.
    {
        assert (&shared() == &shared());
        assert (shared().getBool (tooltipsKey, true)
                || ! shared().getBool (tooltipsKey, true));    // reads either way, no crash
    }

    // 9. The dialog: constructs twice in one process (the old global-row version crashed right
    //    here), paints with its children, and the Sgraffito variant carries the extra
    //    sample-folder row.
    {
        Store store (fileA);
        // Paints the dialog and its visible children (the plain paint() skips children).
        auto render = [] (SettingsDialog& dialog)
        {
            juce::Image image (juce::Image::ARGB, dialog.getWidth(), dialog.getHeight(), true);

            {
                juce::Graphics g (image);   // scoped: JUCE 8+ shows the drawing only after release
                dialog.paintEntireComponent (g, true);
            }

            return image;
        };
        // The dialog paints the suite's dark panel and readable text (plan 10 phase 5b):
        // the caption rows must contain pixels that differ from the panel background.
        auto textPixelsInCaptionRows = [&render] (SettingsDialog& dialog)
        {
            const auto image = render (dialog);
            const auto background = SuiteUI::Colours::panel;
            int textPixels = 0;

            for (int y = 12; y < 70; ++y)
                for (int x = 16; x < 300; ++x)
                    if (image.getPixelAt (x, y) != background)
                        ++textPixels;

            return textPixels;
        };

        int childrenWithoutSampleRow = 0;
        {
            SettingsDialog dialog (store, false);
            childrenWithoutSampleRow = dialog.getNumChildComponents();

            assert (render (dialog).getPixelAt (8, 8) == SuiteUI::Colours::panel);   // the suite paint runs
            assert (textPixelsInCaptionRows (dialog) > 20);   // captions are actually drawn
        }
        int childrenWithSampleRow = 0;
        {
            SettingsDialog dialog (store, true);
            childrenWithSampleRow = dialog.getNumChildComponents();

            assert (render (dialog).getPixelAt (8, 8) == SuiteUI::Colours::panel);
            assert (textPixelsInCaptionRows (dialog) > 20);

            // The Browse buttons carry their captions: full-intensity text pixels inside
            // the button row (the phase 5b legibility fix, verified per pixel).
            const auto dialogImage = render (dialog);
            int textPixelsInBrowseRow = 0;

            for (int x = 326; x < 402; ++x)
                if (dialogImage.getPixelAt (x, 147) == SuiteUI::Colours::text)
                    ++textPixelsInBrowseRow;

            assert (textPixelsInBrowseRow > 0);

            // The recorded render (suite rule R9). FileOutputStream appends to an existing
            // file, so the stale render must go first - or every run archives another PNG
            // in front of the new one and decoders read the oldest one (found here the
            // hard way: the file kept showing the pre-caption dialog).
            const auto renderDir = juce::SystemStats::getEnvironmentVariable ("SUITE_RENDER_DIR", {});
            if (renderDir.isNotEmpty())
            {
                juce::File written (juce::File (renderDir).getChildFile ("settings-dialog.png"));
                written.deleteFile();
                juce::FileOutputStream stream (written, 200000);

                if (stream.openedOk())
                {
                    juce::PNGImageFormat png;
                    png.writeImageToStream (dialogImage, stream);
                    stream.flush();
                }

                // The file must decode to the same image that was written.
                auto reloaded = juce::ImageFileFormat::loadFrom (written);
                assert (reloaded.isValid());
                assert (reloaded.getPixelAt (8, 8) == SuiteUI::Colours::panel);
                assert (reloaded.getPixelAt (380, 147) == dialogImage.getPixelAt (380, 147));
            }
        }
        {
            // A third construction on the same store: the repeat is the regression guard.
            SettingsDialog dialog (store, true);
        }
        assert (childrenWithSampleRow > childrenWithoutSampleRow);
    }

    std::printf ("All SuiteSettings tests passed.\n");
    return 0;
}
