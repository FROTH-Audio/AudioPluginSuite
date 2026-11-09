#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <functional>
#include <memory>
#include <vector>

/**
 * Versioned saved state for the plugins. Header-only, like CoreDSP.
 *
 * The blob is the APVTS state in JUCE's binary XML format, the same bytes the plugins have always written,
 * with a `suiteStateVersion` attribute on the root element. A blob with no attribute is version 1, which is
 * every session saved before this module existed. A blob from a newer plugin is refused and nothing changes,
 * so a newer session never loads partly and changes the sound.
 */
namespace SuiteState
{
    inline constexpr const char* versionProperty = "suiteStateVersion";

    /** Turns a state of version fromVersion into fromVersion + 1. Migrations run in version order. */
    struct Migration
    {
        int fromVersion;
        std::function<void (juce::ValueTree&)> apply;
    };

    enum class LoadResult
    {
        loaded,
        notXml,
        wrongRoot,
        newerThanPlugin
    };

    namespace detail
    {
        // JUCE's AudioProcessor::copyXmlToBinary and getXmlFromBinary are protected, so the same format is written here.
        // Keep these in step with JUCE: magic 0x21324356, a length word, the UTF-8 XML, and a trailing zero byte.
        inline constexpr juce::uint32 xmlMagic = 0x21324356u;

        inline juce::MemoryBlock encodeXml (const juce::XmlElement& xml)
        {
            juce::MemoryBlock block;
            {
                juce::MemoryOutputStream out (block, false);
                out.writeInt ((int) xmlMagic);
                out.writeInt (0);
                xml.writeTo (out, juce::XmlElement::TextFormat().singleLine());
                out.writeByte (0);
            }

            // Go back and write the string length.
            static_cast<juce::uint32*> (block.getData())[1]
                = juce::ByteOrder::swapIfBigEndian ((juce::uint32) block.getSize() - 9);
            return block;
        }

        inline std::unique_ptr<juce::XmlElement> decodeXml (const void* data, int sizeInBytes)
        {
            if (sizeInBytes > 8 && juce::ByteOrder::littleEndianInt (data) == (int) xmlMagic)
            {
                const auto length = (int) juce::ByteOrder::littleEndianInt (static_cast<const char*> (data) + 4);

                if (length > 0)
                    return juce::parseXML (juce::String::fromUTF8 (static_cast<const char*> (data) + 8,
                                                                    juce::jmin (sizeInBytes - 8, length)));
            }

            return {};
        }

        inline bool treeHasParameter (const juce::ValueTree& tree, const juce::String& id)
        {
            for (int i = 0; i < tree.getNumChildren(); ++i)
                if (tree.getChild (i).getProperty ("id").toString() == id)
                    return true;

            return false;
        }
    }

    /**
     * Writes the APVTS state with the version attribute set. extra, if given, edits the state before it is
     * written, which is how a plugin stores state that is not a parameter (for example the Micro-Timing Chaos
     * Lerp curve).
     *
     * copyState() is what pushes the live parameter values into the tree, so apvts is not const. Reading
     * apvts.state directly would write the values from the last sync, not the current ones.
     */
    inline juce::MemoryBlock write (juce::AudioProcessorValueTreeState& apvts, int currentVersion,
                                    const std::function<void (juce::ValueTree&)>& extra = {})
    {
        auto state = apvts.copyState();
        state.setProperty (versionProperty, currentVersion, nullptr);

        if (extra)
            extra (state);

        const auto xml = state.createXml();
        return detail::encodeXml (*xml);
    }

    /**
     * Reads a blob into the APVTS. Nothing changes unless the result is `loaded`.
     *
     * - A blob with no version attribute is version 1. Migrations run from its version up to currentVersion.
     * - afterLoad runs once the state has been replaced, and only on success.
     * - resetMissing sets each parameter that the blob does not contain to its default. In JUCE 9.0.5,
     *   replaceState already does this (measured in the tests), so the option is an explicit guarantee rather
     *   than a fix. Presets use it anyway, so they do not depend on that JUCE behaviour.
     */
    inline LoadResult read (const void* data, int sizeInBytes, juce::AudioProcessorValueTreeState& apvts,
                            int currentVersion, const std::vector<Migration>& migrations,
                            const std::function<void (const juce::ValueTree&)>& afterLoad = {},
                            bool resetMissing = false)
    {
        const auto xml = detail::decodeXml (data, sizeInBytes);

        if (xml == nullptr)
            return LoadResult::notXml;

        if (! xml->hasTagName (apvts.state.getType()))
            return LoadResult::wrongRoot;

        const int stored = xml->getIntAttribute (versionProperty, 1);

        if (stored > currentVersion)
            return LoadResult::newerThanPlugin;

        auto tree = juce::ValueTree::fromXml (*xml);

        for (int version = stored; version < currentVersion; ++version)
            for (const auto& migration : migrations)
                if (migration.fromVersion == version && migration.apply)
                    migration.apply (tree);

        tree.setProperty (versionProperty, currentVersion, nullptr);
        apvts.replaceState (tree);

        if (resetMissing)
            for (auto* parameter : apvts.processor.getParameters())
                if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (parameter))
                    if (! detail::treeHasParameter (tree, withId->paramID))
                        parameter->setValueNotifyingHost (parameter->getDefaultValue());

        if (afterLoad)
            afterLoad (apvts.state);

        return LoadResult::loaded;
    }
}
