#include "SuiteState.h"

#include <juce_core/juce_core.h>
#include <juce_filesystem/juce_filesystem.h>

//==============================================================================
bool SuiteState::writeAuspFile (const juce::File& file, juce::AudioProcessorValueTreeState& apvts, int version)
{
    // Write header: "FROH" magic + version byte
    juce::OutputStream& out = *file.getOutputStream();
    
    // Magic "FROH"
    out.write ("FROH", 4);
    // Version
    out.writeInt (version, 1);
    
    // Binary XML state
    const auto xmlBytes = write (apvts, version);
    const auto* data = static_cast<const juce::uint8*> (xmlBytes.getData());
    const auto size = xmlBytes.getSize();
    
    // Write the XML bytes
    for (int i = 0; i < size; ++i)
        out.writeByte (data[i]);
    
    // Optional: write a trailing zero byte for alignment
    out.writeByte (0);
    
    return file.getOutputStream()->close() == juce::FileOutputStream::stream_no_error;
}

SuiteState::LoadResult SuiteState::readAuspFile (const juce::File& file, juce::AudioProcessorValueTreeState& apvts,
                                                 int currentVersion)
{
    // Read the file
    juce::InputStream* in = file.createInputStream();
    if (! in)
        return LoadResult::notXml;
    
    // Read magic "FROH"
    char magic[5] = { 0 };
    if (in->read (magic, 4) != 4)
    {
        delete in;
        return LoadResult::notXml;
    }
    
    if (juce::String (magic, 4) != "FROH")
    {
        delete in;
        return LoadResult::notXml;
    }
    
    // Read version byte
    int fileVersion = 0;
    if (in->read (&fileVersion, 1) != 1)
    {
        delete in;
        return LoadResult::notXml;
    }
    
    // Read binary XML state
    // Get remaining size
    auto remainingSize = in->getRemainingSizeToEnd();
    if (remainingSize < 1)  // at least one byte for the trailing zero
    {
        delete in;
        return LoadResult::notXml;
    }
    
    // Read the XML bytes into a MemoryBlock
    juce::MemoryBlock xmlData;
    xmlData.setSize (remainingSize);
    if (in->read (xmlData.getData(), remainingSize) != remainingSize)
    {
        delete in;
        return LoadResult::notXml;
    }
    
    delete in;
    
    // Parse the binary XML using the same detail::decodeXml as the plain read()
    const auto xml = detail::decodeXml (xmlData.getData(), (int) xmlData.getSize());
    
    if (xml == nullptr)
        return LoadResult::notXml;
    
    // Check root tag name matches the APVTS state type
    if (! xml->hasTagName (apvts.state.getType()))
        return LoadResult::wrongRoot;
    
    // Check version
    const int stored = xml->getIntAttribute (SuiteState::versionProperty, 1);
    
    if (currentVersion >= 0 && stored > currentVersion)
        return LoadResult::newerThanPlugin;
    
    // Apply the state
    auto tree = juce::ValueTree::fromXml (*xml);
    
    for (int version = stored; version < currentVersion; ++version)
    {
        // Migrations would go here - for now just skip
    }
    
    tree.setProperty (SuiteState::versionProperty, currentVersion >= 0 ? currentVersion : stored, nullptr);
    apvts.replaceState (tree);
    
    return LoadResult::loaded;
}

juce::File SuiteState::getUserPresetFolder (const juce::String& pluginName)
{
    // Get the application's application support directory
    // On Windows: %APPDATA%\[Manufacturer]\[Plugin]
    // On macOS: ~/Library/Application Support/[Manufacturer]/[Plugin]
    // On Linux: ~/.local/share/[Manufacturer]/[Plugin]
    
    juce::String manufacturer = "FROTH Audio";  // could be configured per plugin
    juce::File folder = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory);
    
    folder = folder.getChildFile (manufacturer);
    folder = folder.getChildFile (pluginName);
    
    // Create the folder if it doesn't exist
    if (! folder_exists (folder))
        folder.createDirectory();
    
    return folder;
}