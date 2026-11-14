#include "../../apps/plugin/PluginProcessor.h"
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <thread>

namespace
{
void write16 (std::ofstream& file, unsigned int value)
{
    file.put (static_cast<char> (value & 0xff));
    file.put (static_cast<char> ((value >> 8) & 0xff));
}
void write32 (std::ofstream& file, unsigned int value)
{
    write16 (file, value & 0xffff);
    write16 (file, value >> 16);
}
bool waitForReady (HyperrealAudioProcessor& processor)
{
    for (int attempt = 0; attempt < 300; ++attempt)
    {
        if (processor.getSnapshot().getProperty ("status", {}).toString() == "ready")
            return true;
        std::this_thread::sleep_for (std::chrono::milliseconds (10));
    }
    return false;
}
}

int main()
{
    constexpr int frames = 4410;
    const auto file = juce::File::getSpecialLocation (juce::File::tempDirectory)
                          .getChildFile ("hyperreal-processor-smoke.wav");
    {
        std::ofstream wav (file.getFullPathName().toStdString(), std::ios::binary);
        wav.write ("RIFF", 4);
        write32 (wav, 36 + frames * 2);
        wav.write ("WAVEfmt ", 8);
        write32 (wav, 16);
        write16 (wav, 1);
        write16 (wav, 1);
        write32 (wav, 44100);
        write32 (wav, 88200);
        write16 (wav, 2);
        write16 (wav, 16);
        wav.write ("data", 4);
        write32 (wav, frames * 2);
        for (int frame = 0; frame < frames; ++frame)
            write16 (wav, static_cast<unsigned int> (static_cast<int> (std::sin (frame * 0.1) * 16000.0)));
    }

    HyperrealAudioProcessor original;
    original.prepareToPlay (44100.0, 128);
    original.requestSampleLoad (file);
    if (! waitForReady (original))
    {
        std::cerr << "Processor import failed\n";
        return 1;
    }

    auto* command = new juce::DynamicObject();
    command->setProperty ("schemaVersion", 1);
    command->setProperty ("value", 0.5);
    if (! original.setMasterGain (juce::var (command)))
    {
        std::cerr << "Master gain command failed\n";
        return 1;
    }
    auto setting = [] (int pad, int value)
    {
        auto* item = new juce::DynamicObject();
        item->setProperty ("schemaVersion", 3);
        item->setProperty ("pad", pad);
        item->setProperty ("value", value);
        return juce::var (item);
    };
    const auto modeSet = original.setPadMode (setting (0, 1));
    const auto chokeSet = original.setPadChokeGroup (setting (0, 2));
    const auto invalidPadAccepted = original.setPadMode (setting (16, 1));
    const auto invalidChokeAccepted = original.setPadChokeGroup (setting (0, 5));
    const auto muteSet = original.setPadMute (setting (0, 1));
    const auto soloSet = original.setPadSolo (setting (1, 1));
    const auto invalidMuteAccepted = original.setPadMute (setting (0, 2));
    if (! modeSet || ! chokeSet || ! muteSet || ! soloSet
        || invalidPadAccepted || invalidChokeAccepted || invalidMuteAccepted)
    {
        std::cerr << "Pad setting command validation failed: " << modeSet << ' '
                  << chokeSet << ' ' << invalidPadAccepted << ' ' << invalidChokeAccepted << "\n";
        return 1;
    }

    juce::MemoryBlock state;
    original.getStateInformation (state);
    file.deleteFile();

    // Build a Phase 2 envelope from the saved parameter tree and the known sample.
    // The previous release stored one sample directly after the HRP2 header.
    juce::MemoryInputStream stateReader (state.getData(), state.getSize(), false);
    char stateHeader[4] {};
    stateReader.read (stateHeader, 4);
    stateReader.readInt();
    const auto parameterSize = stateReader.readInt();
    juce::MemoryBlock parameterBytes (static_cast<size_t> (parameterSize));
    stateReader.read (parameterBytes.getData(), parameterSize);
    auto phase4Parameters = juce::ValueTree::readFromData (parameterBytes.getData(),
                                                           parameterBytes.getSize());
    phase4Parameters.setProperty ("schema_version", 4, nullptr);
    for (int child = phase4Parameters.getNumChildren() - 1; child >= 0; --child)
    {
        const auto id = phase4Parameters.getChild (child).getProperty ("id").toString();
        if (id.contains (".mute.") || id.contains (".solo."))
            phase4Parameters.removeChild (child, nullptr);
    }
    auto makePreviousState = [&stateReader] (const juce::ValueTree& tree,
                                              const char* magic, int version)
    {
        juce::MemoryBlock parameterData;
        juce::MemoryOutputStream parameterOutput (parameterData, false);
        tree.writeToStream (parameterOutput);
        juce::MemoryBlock previous;
        juce::MemoryOutputStream output (previous, false);
        output.write (magic, 4);
        output.writeInt (version);
        output.writeInt (static_cast<int> (parameterData.getSize()));
        output.write (parameterData.getData(), parameterData.getSize());
        output.write (static_cast<const char*> (stateReader.getData()) + stateReader.getPosition(),
                      static_cast<size_t> (stateReader.getNumBytesRemaining()));
        return previous;
    };
    const auto phase4State = makePreviousState (phase4Parameters, "HRP4", 4);
    auto phase3Parameters = juce::ValueTree::readFromData (parameterBytes.getData(),
                                                           parameterBytes.getSize());
    phase3Parameters.setProperty ("schema_version", 3, nullptr);
    for (int child = phase3Parameters.getNumChildren() - 1; child >= 0; --child)
    {
        const auto id = phase3Parameters.getChild (child).getProperty ("id").toString();
        if (id.contains (".mode.") || id.contains (".choke.")
            || id.contains (".mute.") || id.contains (".solo."))
            phase3Parameters.removeChild (child, nullptr);
    }
    const auto phase3State = makePreviousState (phase3Parameters, "HRP3", 3);
    auto phase2Parameters = juce::ValueTree::readFromData (parameterBytes.getData(),
                                                           parameterBytes.getSize());
    phase2Parameters.setProperty ("schema_version", 2, nullptr);
    for (int child = phase2Parameters.getNumChildren() - 1; child >= 0; --child)
    {
        const auto id = phase2Parameters.getChild (child).getProperty ("id").toString();
        if (id.contains (".mode.") || id.contains (".choke.")
            || id.contains (".mute.") || id.contains (".solo."))
            phase2Parameters.removeChild (child, nullptr);
    }
    juce::MemoryBlock phase2ParameterBytes;
    {
        juce::MemoryOutputStream parametersOutput (phase2ParameterBytes, false);
        phase2Parameters.writeToStream (parametersOutput);
    }
    juce::MemoryBlock phase2State;
    {
        juce::MemoryOutputStream output (phase2State, false);
        output.write ("HRP2", 4);
        output.writeInt (2);
        output.writeInt (static_cast<int> (phase2ParameterBytes.getSize()));
        output.write (phase2ParameterBytes.getData(), phase2ParameterBytes.getSize());
        output.writeByte (1);
        output.writeString ("phase2-legacy.wav");
        output.writeDouble (44100.0);
        output.writeInt (1);
        output.writeInt (frames);
        for (int frame = 0; frame < frames; ++frame)
            output.writeFloat (static_cast<float> (
                static_cast<int> (std::sin (frame * 0.1) * 16000.0) / 32768.0));
    }

    HyperrealAudioProcessor restored;
    restored.setStateInformation (state.getData(), static_cast<int> (state.getSize()));
    restored.prepareToPlay (44100.0, 128);
    const auto snapshot = restored.getSnapshot();
    if (snapshot.getProperty ("status", {}).toString() != "ready"
        || std::abs (static_cast<double> (snapshot.getProperty ("masterGain", 0.0)) - 0.5) > 0.001
        || static_cast<int> (snapshot.getProperty ("padMode", -1)) != 1
        || static_cast<int> (snapshot.getProperty ("chokeGroup", -1)) != 2
        || ! static_cast<bool> (snapshot.getProperty ("muted", false))
        || ! restored.selectPad (1)
        || ! static_cast<bool> (restored.getSnapshot().getProperty ("solo", false)))
    {
        std::cerr << "Full project state did not restore\n";
        return 1;
    }
    restored.selectPad (0);
    restored.setStateInformation (state.getData(), static_cast<int> (state.getSize()) - 1);
    if (restored.getSnapshot().getProperty ("status", {}).toString() != "ready"
        || std::abs (static_cast<double> (
            restored.getSnapshot().getProperty ("masterGain", 0.0)) - 0.5) > 0.001)
    {
        std::cerr << "Malformed state replaced the active project\n";
        return 1;
    }

    juce::AudioBuffer<float> audio (2, 128);
    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (1, 36, static_cast<juce::uint8> (127)), 0);
    restored.setPadMute (setting (0, 0));
    restored.setPadSolo (setting (1, 0));
    restored.processBlock (audio, midi);
    const auto magnitude = audio.getMagnitude (0, 0, 128);
    if (magnitude < 0.15f || magnitude > 0.3f)
    {
        std::cerr << "Restored playback or master gain failed: " << magnitude << "\n";
        return 1;
    }

    HyperrealAudioProcessor migrated;
    migrated.setStateInformation (phase2State.getData(), static_cast<int> (phase2State.getSize()));
    migrated.prepareToPlay (44100.0, 128);
    if (migrated.getSnapshot().getProperty ("status", {}).toString() != "ready"
        || ! migrated.selectPad (1)
        || migrated.getSnapshot().getProperty ("status", {}).toString() != "empty")
    {
        std::cerr << "Phase 2 project migration failed\n";
        return 1;
    }
    migrated.selectPad (0);
    migrated.processBlock (audio, midi);
    if (audio.getMagnitude (0, 0, 128) < 0.15f)
    {
        std::cerr << "Migrated Phase 2 sample did not play\n";
        return 1;
    }
    HyperrealAudioProcessor phase3Migrated;
    phase3Migrated.setStateInformation (phase3State.getData(), static_cast<int> (phase3State.getSize()));
    if (phase3Migrated.getSnapshot().getProperty ("status", {}).toString() != "ready"
        || static_cast<int> (phase3Migrated.getSnapshot().getProperty ("padMode", -1)) != 0
        || static_cast<int> (phase3Migrated.getSnapshot().getProperty ("chokeGroup", -1)) != 0)
    {
        std::cerr << "Phase 3 project migration failed\n";
        return 1;
    }
    HyperrealAudioProcessor phase4Migrated;
    phase4Migrated.setStateInformation (phase4State.getData(), static_cast<int> (phase4State.getSize()));
    if (phase4Migrated.getSnapshot().getProperty ("status", {}).toString() != "ready"
        || static_cast<int> (phase4Migrated.getSnapshot().getProperty ("padMode", -1)) != 1
        || static_cast<int> (phase4Migrated.getSnapshot().getProperty ("chokeGroup", -1)) != 2
        || static_cast<bool> (phase4Migrated.getSnapshot().getProperty ("muted", true)))
    {
        std::cerr << "Phase 4 project migration failed\n";
        return 1;
    }

    HyperrealAudioProcessor operations;
    operations.setStateInformation (state.getData(), static_cast<int> (state.getSize()));
    if (! operations.copyPad (0) || ! operations.pastePad (2)
        || ! operations.selectPad (2)
        || operations.getSnapshot().getProperty ("status", {}).toString() != "ready"
        || static_cast<int> (operations.getSnapshot().getProperty ("padMode", -1)) != 1
        || static_cast<int> (operations.getSnapshot().getProperty ("chokeGroup", -1)) != 2
        || ! static_cast<bool> (operations.getSnapshot().getProperty ("muted", false)))
    {
        std::cerr << "Pad paste did not copy sample and settings\n";
        return 1;
    }
    if (! operations.swapPads (1, 2) || ! operations.selectPad (1)
        || operations.getSnapshot().getProperty ("status", {}).toString() != "ready"
        || ! static_cast<bool> (operations.getSnapshot().getProperty ("muted", false)))
    {
        std::cerr << "Pad swap did not move sample and settings\n";
        return 1;
    }
    if (! operations.selectPad (2)
        || operations.getSnapshot().getProperty ("status", {}).toString() != "empty"
        || ! static_cast<bool> (operations.getSnapshot().getProperty ("solo", false)))
    {
        std::cerr << "Pad swap lost destination settings\n";
        return 1;
    }
    if (! operations.clearPad (1) || ! operations.selectPad (1)
        || operations.getSnapshot().getProperty ("status", {}).toString() != "empty"
        || static_cast<bool> (operations.getSnapshot().getProperty ("muted", true))
        || static_cast<int> (operations.getSnapshot().getProperty ("padMode", -1)) != 0)
    {
        std::cerr << "Pad clear did not reset sample and settings\n";
        return 1;
    }

    std::cout << "Processor state smoke passed\n";
    return 0;
}
