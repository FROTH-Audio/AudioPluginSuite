// Tests for the shared UI module. Widgets are laid out and painted into images, so the drawing
// code and the look and feel run as they would in a plugin window.

#undef NDEBUG
#include <cassert>

#include <juce_gui_basics/juce_gui_basics.h>

#include <cmath>
#include <cstdio>
#include <vector>

#include "SuiteLookAndFeel.h"
#include "SuiteHeaderMenu.h"
#include "SuiteWidgets.h"

namespace
{
    constexpr juce::uint32 testAccent = 0xff2de2ff;

    // Paints a component into an image and returns the number of non-transparent pixels.
    int paintAndCountVisiblePixels (juce::Component& component, int width, int height)
    {
        component.setBounds (0, 0, width, height);

        juce::Image image (juce::Image::ARGB, width, height, true);

        // The Graphics must be released before the pixels are read. JUCE 9 does not make the drawing visible in the image until then.
        {
            juce::Graphics g (image);
            component.paintEntireComponent (g, true);
        }

        int visible = 0;
        for (int y = 0; y < height; ++y)
            for (int x = 0; x < width; ++x)
                if (image.getPixelAt (x, y).getAlpha() > 0)
                    ++visible;

        return visible;
    }

    void testSegmentedSelector()
    {
        SuiteUI::SegmentedSelector selector { juce::Colour (testAccent) };
        selector.setOptions ({ "TUBE", "TAPE", "DIODE" });

        int callbacks = 0;
        selector.onChange = [&callbacks] (int) { ++callbacks; };

        assert (selector.getSelectedIndex() == 0);

        // Programmatic changes update the display but do not notify, so they cannot feed back into the parameter.
        selector.setSelectedIndex (2);
        assert (selector.getSelectedIndex() == 2);
        assert (callbacks == 0);

        // Out-of-range indices are clamped to the last option.
        selector.setSelectedIndex (17);
        assert (selector.getSelectedIndex() == 2);

        selector.setSelectedIndex (-4);
        assert (selector.getSelectedIndex() == 0);

        // An empty selector stays at zero.
        SuiteUI::SegmentedSelector empty { juce::Colour (testAccent) };
        empty.setSelectedIndex (3);
        assert (empty.getSelectedIndex() == 0);

        assert (paintAndCountVisiblePixels (selector, 240, 30) > 0);

        std::puts ("[PASS] testSegmentedSelector");
    }

    void testSegmentedSelectorClicksNotify()
    {
        SuiteUI::SegmentedSelector selector { juce::Colour (testAccent) };
        selector.setOptions ({ "A", "B", "C", "D" });
        selector.setBounds (0, 0, 400, 30);

        std::vector<int> reported;
        selector.onChange = [&reported] (int index) { reported.push_back (index); };

        // Clicking the middle of each segment selects that segment.
        const auto click = [&selector] (float x)
        {
            juce::MouseEvent event (juce::Desktop::getInstance().getMainMouseSource(),
                                    { x, 15.0f }, juce::ModifierKeys(), 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                    &selector, &selector, juce::Time::getCurrentTime(),
                                    { x, 15.0f }, juce::Time::getCurrentTime(), 1, false);
            selector.mouseDown (event);
        };

        click (150.0f);   // segment 1
        click (350.0f);   // segment 3
        click (350.0f);   // already selected, so no notification

        assert (reported.size() == 2);
        assert (reported[0] == 1 && reported[1] == 3);
        assert (selector.getSelectedIndex() == 3);

        std::puts ("[PASS] testSegmentedSelectorClicksNotify");
    }

    /** The note-off guarantee's widget paths (plan 13): a mouse-up releases the held key,
        a drag releases the previous key before the next one starts, the pointer leaving
        releases everything, and a destroyed strip flushes its held note. */
    void testPlayableKeyboardGuarantee()
    {
        std::vector<int> ons, offs;
        auto wire = [&ons, &offs] (SuiteUI::PlayableKeyboardStrip& strip)
        {
            strip.onNoteOn = [&ons] (int note, float) { ons.push_back (note); };
            strip.onNoteOff = [&offs] (int note) { offs.push_back (note); };
        };

        const auto keyEvent = [] (juce::Component& component, float x, float y)
        {
            return juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(),
                                     juce::Point<float> (x, y), juce::ModifierKeys(), 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                     &component, &component, juce::Time::getCurrentTime(),
                                     juce::Point<float> (x, y), juce::Time::getCurrentTime(), 1, false);
        };

        // 73 keys over 730 px: 10 px per key, so key 60 sits at x = 365, key 64 at 405.
        SuiteUI::PlayableKeyboardStrip strip;
        strip.setBounds (0, 0, 730, 40);
        wire (strip);

        strip.mouseDown (keyEvent (strip, 365.0f, 30.0f));
        assert (ons == std::vector<int> { 60 });
        assert (strip.isAnyNoteHeld());

        // Mouse-up over a different key releases only the held one (plan 13).
        strip.mouseUp (keyEvent (strip, 725.0f, 30.0f));
        assert (offs == std::vector<int> { 60 });
        assert (! strip.isAnyNoteHeld());

        // A drag releases the previous key before the next one starts.
        strip.mouseDown (keyEvent (strip, 365.0f, 30.0f));
        strip.mouseDrag (keyEvent (strip, 405.0f, 30.0f));
        assert (ons == (std::vector<int> { 60, 60, 64 }));
        assert (offs == (std::vector<int> { 60, 60 }));
        strip.mouseUp (keyEvent (strip, 405.0f, 30.0f));
        assert (offs.back() == 64);

        // The pointer leaving the strip releases everything it holds.
        strip.mouseDown (keyEvent (strip, 365.0f, 30.0f));
        strip.mouseExit (keyEvent (strip, 365.0f, 45.0f));
        assert (offs.back() == 60);
        assert (! strip.isAnyNoteHeld());

        // releaseHeldNotes is idempotent.
        strip.releaseHeldNotes();
        assert (offs.size() == 4);

        // A destroyed strip flushes its held note.
        {
            SuiteUI::PlayableKeyboardStrip doomed;
            doomed.setBounds (0, 0, 730, 40);
            wire (doomed);
            doomed.mouseDown (keyEvent (doomed, 365.0f, 30.0f));
            assert (ons.back() == 60);
        }
        assert (offs.back() == 60);
        assert (ons.size() == 5 && offs.size() == 5);   // note-ons equal note-offs

        std::puts ("[PASS] testPlayableKeyboardGuarantee");
    }

    void testLabelledSelector()
    {
        SuiteUI::LabelledSelector cell ("MODE", juce::Colour (testAccent));
        cell.setOptions ({ "A", "B", "C" });

        assert (cell.label.getText() == "MODE");
        assert (cell.getSelectedIndex() == 0);

        // Manual updates work without a parameter and never notify.
        cell.setSelectedIndex (2);
        assert (cell.getSelectedIndex() == 2);

        // The pill paints (caption above, selector below).
        SuiteUI::SuiteLookAndFeel laf { juce::Colour (testAccent) };
        cell.setLookAndFeel (&laf);
        assert (paintAndCountVisiblePixels (cell, 160, SuiteUI::captionHeight + SuiteUI::pillHeight + 6) > 0);
        cell.setLookAndFeel (nullptr);

        std::puts ("[PASS] testLabelledSelector");
    }

    void testLabelledSliders()
    {
        SuiteUI::LabelledSlider rotary ("DRIVE", juce::Slider::RotaryVerticalDrag);
        SuiteUI::LabelledSlider linear ("CHAOS", juce::Slider::LinearHorizontal);

        assert (rotary.label.getText() == "DRIVE");
        assert (linear.label.getText() == "CHAOS");

        rotary.slider.setRange (0.0, 10.0);
        rotary.slider.setValue (4.0, juce::dontSendNotification);
        assert (std::abs (rotary.slider.getValue() - 4.0) < 1e-9);

        // The look and feel draws the knob and the linear track.
        SuiteUI::SuiteLookAndFeel laf { juce::Colour (testAccent) };
        rotary.setLookAndFeel (&laf);
        linear.setLookAndFeel (&laf);

        assert (paintAndCountVisiblePixels (rotary, 120, 150) > 0);
        assert (paintAndCountVisiblePixels (linear, 260, 60) > 0);

        rotary.setLookAndFeel (nullptr);
        linear.setLookAndFeel (nullptr);

        std::puts ("[PASS] testLabelledSliders");
    }

    void testLookAndFeelToggleButton()
    {
        SuiteUI::SuiteLookAndFeel laf { juce::Colour (testAccent) };
        assert (laf.getAccent() == juce::Colour (testAccent));

        juce::ToggleButton button ("FREEZE");
        button.setLookAndFeel (&laf);

        // Off and on, and while pressed, each take a different branch in the drawing code.
        button.setToggleState (false, juce::dontSendNotification);
        const int offPixels = paintAndCountVisiblePixels (button, 140, 40);

        button.setToggleState (true, juce::dontSendNotification);
        const int onPixels = paintAndCountVisiblePixels (button, 140, 40);

        assert (offPixels > 0 && onPixels > 0);

        button.setLookAndFeel (nullptr);

        std::puts ("[PASS] testLookAndFeelToggleButton");
    }

    void testSpectrumDisplay()
    {
        SuiteUI::SpectrumDisplay display (juce::Colour (0xffa9b7c9), juce::Colour (testAccent));

        // Before any data arrives the display shows its waiting message.
        assert (paintAndCountVisiblePixels (display, 500, 200) > 0);

        std::vector<float> input (1025), output (1025);

        for (int k = 0; k < 1025; ++k)
        {
            input[(size_t) k] = (k == 43) ? 200.0f : 0.001f;
            output[(size_t) k] = (k == 43) ? 180.0f : 0.0f;
        }

        display.setSignalPresent (true);
        display.setSpectra (input.data(), output.data(), 1025, 48000.0 / 2048.0, 2.0f / 2048.0f);

        assert (paintAndCountVisiblePixels (display, 500, 200) > 0);

        // Back to waiting, and a silent frame, both paint without problems.
        display.setSignalPresent (false);
        assert (paintAndCountVisiblePixels (display, 500, 200) > 0);

        std::fill (input.begin(), input.end(), 0.0f);
        std::fill (output.begin(), output.end(), 0.0f);
        display.setSignalPresent (true);
        display.setSpectra (input.data(), output.data(), 1025, 48000.0 / 2048.0, 2.0f / 2048.0f);
        assert (paintAndCountVisiblePixels (display, 500, 200) > 0);

        std::puts ("[PASS] testSpectrumDisplay");
    }

    // Counts painted pixels within a tolerance of a colour. Used to check that a trace is really drawn.
    int countPixelsNear (juce::Component& component, int width, int height, juce::Colour target, int tolerance)
    {
        component.setBounds (0, 0, width, height);

        juce::Image image (juce::Image::ARGB, width, height, true);

        {
            juce::Graphics g (image);
            component.paintEntireComponent (g, true);
        }

        int count = 0;

        for (int y = 0; y < height; ++y)
            for (int x = 0; x < width; ++x)
            {
                const auto pixel = image.getPixelAt (x, y);

                if (pixel.getAlpha() > 0
                    && std::abs (pixel.getRed() - target.getRed()) <= tolerance
                    && std::abs (pixel.getGreen() - target.getGreen()) <= tolerance
                    && std::abs (pixel.getBlue() - target.getBlue()) <= tolerance)
                    ++count;
            }

        return count;
    }

    void testSpectrumCurveIsDrawn()
    {
        const juce::Colour outputColour (testAccent);
        SuiteUI::SpectrumDisplay display (juce::Colour (0xffa9b7c9), outputColour);

        std::vector<float> input (1025, 1.0e-6f), output (1025, 1.0e-6f);
        input[43] = 200.0f;
        output[43] = 180.0f;

        // No signal: the trace must not be drawn, only the waiting message.
        display.setSignalPresent (false);
        display.setSpectra (input.data(), output.data(), 1025, 48000.0 / 2048.0, 2.0f / 2048.0f);
        assert (countPixelsNear (display, 500, 200, outputColour, 30) == 0);

        // With signal: the output trace must cover real pixels, not just a single point.
        display.setSignalPresent (true);
        display.setSpectra (input.data(), output.data(), 1025, 48000.0 / 2048.0, 2.0f / 2048.0f);
        assert (countPixelsNear (display, 500, 200, outputColour, 30) > 200);

        std::puts ("[PASS] testSpectrumCurveIsDrawn");
    }

    void testTrailPlot()
    {
        SuiteUI::TrailPlot plot { juce::Colour (testAccent) };

        // Fewer than two points draws only the axes.
        assert (paintAndCountVisiblePixels (plot, 200, 200) > 0);

        std::vector<juce::Point<float>> points;
        for (int i = 0; i < 300; ++i)
            points.emplace_back (std::sin (0.05f * (float) i), std::cos (0.07f * (float) i));

        plot.setTrail (points);
        assert (paintAndCountVisiblePixels (plot, 200, 200) > 0);

        std::puts ("[PASS] testTrailPlot");
    }

    void testCurveView()
    {
        SuiteUI::CurveView view { juce::Colour (testAccent) };

        // No curve set yet: only the reference line is drawn.
        assert (paintAndCountVisiblePixels (view, 200, 160) > 0);

        view.setCurve ([] (float x) { return std::tanh (3.0f * x); }, 0.5f);
        assert (paintAndCountVisiblePixels (view, 200, 160) > 0);

        // A curve that overshoots the range is clamped for display.
        view.setCurve ([] (float x) { return 5.0f * x; }, 2.0f);
        assert (paintAndCountVisiblePixels (view, 200, 160) > 0);

        std::puts ("[PASS] testCurveView");
    }

    void testLabelledDropdown()
    {
        SuiteUI::LabelledDropdown cell { "SHAPE" };
        cell.setOptions ({ "SINE", "TRIANGLE", "SAW" });

        assert (cell.getSelectedIndex() == -1);

        cell.setSelectedIndex (2);
        assert (cell.getSelectedIndex() == 2);
        assert (cell.box.getNumItems() == 3);
        assert (cell.box.getItemText (0) == "SINE");
        assert (cell.box.getItemText (2) == "SAW");

        // Out-of-range clears the selection to nothing rather than wrapping.
        cell.setSelectedIndex (-1);
        assert (cell.getSelectedIndex() == -1);

        cell.setSelectedIndex (0);
        assert (cell.getSelectedIndex() == 0);

        // The caption and the box both paint (the look and feel styles the box).
        SuiteUI::SuiteLookAndFeel laf { juce::Colour (testAccent) };
        cell.setLookAndFeel (&laf);
        assert (paintAndCountVisiblePixels (cell, 160, 54) > 0);
        cell.setLookAndFeel (nullptr);

        std::puts ("[PASS] testLabelledDropdown");
    }

    void testHelpLine()
    {
        SuiteUI::HelpLine line;
        line.setIdleText ("Idle sentence.");

        // The idle text is what the line shows before anything is hovered.
        assert (line.getText() == "Idle sentence.");

        // The owner's bug: the line kept the first hovered control's text. showFor must
        // always write something - the help text or the idle text, never the last one.
        juce::Component withHelp;
        withHelp.setHelpText ("Attack: envelope rise.");

        line.showFor (&withHelp);
        assert (line.getText() == "Attack: envelope rise.");

        juce::Component withoutHelp;
        line.showFor (&withoutHelp);
        assert (line.getText() == "Idle sentence.");

        line.showFor (nullptr);
        assert (line.getText() == "Idle sentence.");

        // A new idle text reaches the line while it is showing the idle text.
        line.setIdleText ("New idle sentence.");
        assert (line.getText() == "New idle sentence.");

        // A disabled predicate (the settings' tooltips toggle) always shows the idle text,
        // even over a control with help text.
        line.setEnablePredicate ([] { return false; });
        line.showFor (&withHelp);
        assert (line.getText() == "New idle sentence.");

        line.setEnablePredicate ([] { return true; });
        line.showFor (&withHelp);
        assert (line.getText() == "Attack: envelope rise.");
        line.setEnablePredicate ({});

        assert (paintAndCountVisiblePixels (line, 300, 26) > 0);

        line.setText (juce::String::repeatedString ("Long help text that must stay on one line. ", 12));
        assert (paintAndCountVisiblePixels (line, 300, 26) > 0);

        std::puts ("[PASS] testHelpLine");
    }

    void testAboutPanel()
    {
        SuiteUI::AboutPanel panel ({}, "MIDI Chord Arp Scale Tool", "1.0.0", "Midi", "MidiEffect");
        assert (panel.getWidth() >= 440);
        assert (panel.getHeight() >= 250);
        assert (paintAndCountVisiblePixels (panel, panel.getWidth(), panel.getHeight()) > 0);

        auto* link = dynamic_cast<juce::HyperlinkButton*> (
            panel.getChildComponent (0));
        assert (link != nullptr);
        assert (link->getComponentID() == "suite-repository-link");
        assert (link->getButtonText() == SuiteUI::kRepositoryUrl);
        assert (link->getURL().toString (false) == SuiteUI::kRepositoryUrl);
        assert (panel.getLocalBounds().contains (link->getBounds()));
        assert (juce::GlyphArrangement::getStringWidth (SuiteUI::uiFont (12.0f),
                                                        link->getButtonText()) <= link->getWidth());
        assert (juce::GlyphArrangement::getStringWidth (SuiteUI::uiFont (20.0f, true),
                                                        "MIDI Chord Arp Scale Tool")
                <= panel.getWidth() - 32);

        std::puts ("[PASS] testAboutPanel");
    }

    void testDrawGroupFrame()
    {
        juce::Image image (juce::Image::ARGB, 240, 120, true);

        {
            juce::Graphics g (image);
            SuiteUI::drawGroupFrame (g, juce::Rectangle<int> (20, 20, 200, 80), "GLOBAL",
                                     juce::Colour (testAccent));
        }

        // The fill, the border and the title all paint inside the bounds.
        int visible = 0;
        for (int y = 20; y < 100; ++y)
            for (int x = 20; x < 220; ++x)
                if (image.getPixelAt (x, y).getAlpha() > 0)
                    ++visible;

        assert (visible > 200);

        // Nothing paints outside the bounds.
        assert (image.getPixelAt (10, 10).getAlpha() == 0);
        assert (image.getPixelAt (230, 110).getAlpha() == 0);

        std::puts ("[PASS] testDrawGroupFrame");
    }

    void testLevelMeter()
    {
        SuiteUI::LevelMeter meter { juce::Colour (testAccent) };

        for (float level : { 0.0f, 0.001f, 0.5f, 1.0f, 4.0f })
        {
            meter.setLevel (level);
            assert (paintAndCountVisiblePixels (meter, 24, 160) > 0);
        }

        std::puts ("[PASS] testLevelMeter");
    }

    void testStereoLevelMeter()
    {
        const juce::Colour leftColour (0xffaaaaaa);
        const juce::Colour rightColour (testAccent);
        SuiteUI::StereoLevelMeter meter { leftColour, rightColour };

        // Idle: the track backgrounds paint, but neither bar's accent colour appears.
        meter.setLevels (0.0f, 0.0f);
        assert (countPixelsNear (meter, 32, 160, leftColour, 30) == 0);
        assert (countPixelsNear (meter, 32, 160, rightColour, 30) == 0);

        // Left-only level: left half lights, right half stays dark.
        meter.setLevels (1.0f, 0.0f);
        const int leftPixels = countPixelsNear (meter, 32, 160, leftColour, 30);
        assert (leftPixels > 100);
        assert (countPixelsNear (meter, 32, 160, rightColour, 30) == 0);

        // Right-only level: right half lights, left returns to dark.
        meter.setLevels (0.0f, 1.0f);
        const int rightPixels = countPixelsNear (meter, 32, 160, rightColour, 30);
        assert (rightPixels > 100);
        assert (countPixelsNear (meter, 32, 160, leftColour, 30) == 0);

        // Both at 0 dB: both bars light up.
        meter.setLevels (1.0f, 1.0f);
        assert (countPixelsNear (meter, 32, 160, leftColour, 30) > 100);
        assert (countPixelsNear (meter, 32, 160, rightColour, 30) > 100);

        // Saturation: 4.0 is still drawn at 0 dB (clamped).
        meter.setLevels (4.0f, 4.0f);
        assert (countPixelsNear (meter, 32, 160, leftColour, 30) > 100);
        assert (countPixelsNear (meter, 32, 160, rightColour, 30) > 100);

        std::puts ("[PASS] testStereoLevelMeter");
    }
}

int main()
{
    juce::ScopedJuceInitialiser_GUI gui;

    std::puts ("Running SuiteUI tests...");

    testSegmentedSelector();
    testSegmentedSelectorClicksNotify();
    testPlayableKeyboardGuarantee();
    testLabelledSelector();
    testLabelledSliders();
    testLabelledDropdown();
    testHelpLine();
    testAboutPanel();
    testDrawGroupFrame();
    testLookAndFeelToggleButton();
    testSpectrumDisplay();
    testSpectrumCurveIsDrawn();
    testTrailPlot();
    testCurveView();
    testLevelMeter();
    testStereoLevelMeter();

    std::puts ("All SuiteUI tests passed.");
    return 0;
}
