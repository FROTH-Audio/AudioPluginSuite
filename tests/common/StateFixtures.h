#pragma once

// Checks that a state saved by an earlier build still loads, with the values that build recorded.
//
// The fixtures are not in the repository. Set SUITE_FIXTURE_DIR to the folder that holds
// <name>_v1.xml (the state, as written by getStateInformation) and <name>_v1.values.txt (one
// "parameterID normalisedValue" line per parameter). Without the variable the check is skipped and says so.

#include <juce_audio_processors/juce_audio_processors.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <sstream>
#include <string>

namespace StateFixtures
{
    // Returns false only when a fixture is present and does not match. A missing fixture folder is a skip, not a failure.
    template <typename Processor>
    bool loadsRecordedState (Processor& processor, const char* name)
    {
        const auto folder = juce::SystemStats::getEnvironmentVariable ("SUITE_FIXTURE_DIR", {});

        if (folder.isEmpty())
        {
            std::printf ("[SKIP] %s fixture: set SUITE_FIXTURE_DIR to check it\n", name);
            return true;
        }

        const juce::File stateFile (folder + "/" + name + "_v1.xml");
        const juce::File valuesFile (folder + "/" + name + "_v1.values.txt");

        juce::MemoryBlock blob;
        if (! stateFile.loadFileAsData (blob) || blob.getSize() == 0)
        {
            std::printf ("[FAIL] %s fixture: cannot read %s\n", name, stateFile.getFullPathName().toRawUTF8());
            return false;
        }

        processor.setStateInformation (blob.getData(), (int) blob.getSize());

        std::ifstream values (valuesFile.getFullPathName().toStdString());
        if (! values)
        {
            std::printf ("[FAIL] %s fixture: cannot read %s\n", name, valuesFile.getFullPathName().toRawUTF8());
            return false;
        }

        int checked = 0;
        bool ok = true;
        std::string line;

        while (std::getline (values, line))
        {
            std::istringstream fields (line);
            std::string id;
            double recorded = 0.0;

            if (! (fields >> id >> recorded))
                continue;

            const auto* parameter = processor.apvts.getParameter (id.c_str());
            if (parameter == nullptr)
            {
                std::printf ("[FAIL] %s fixture: no parameter %s\n", name, id.c_str());
                ok = false;
                continue;
            }

            if (std::abs ((double) parameter->getValue() - recorded) > 1.0e-5)
            {
                std::printf ("[FAIL] %s fixture: %s is %f, recorded %f\n", name, id.c_str(), parameter->getValue(), recorded);
                ok = false;
            }

            ++checked;
        }

        std::printf ("[%s] %s fixture: %d parameters checked\n", ok ? "PASS" : "FAIL", name, checked);
        return ok && checked > 40;
    }
}
