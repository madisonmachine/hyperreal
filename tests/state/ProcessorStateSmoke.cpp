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
    const auto voicesSet = original.setPadVoiceLimit (setting (0, 1));
    const auto velocitySet = original.setPadVelocityCurve (setting (0, 2));
    const auto coarseSet = original.setPadCoarseTune (setting (0, 7));
    auto* fineCommand = new juce::DynamicObject();
    fineCommand->setProperty ("schemaVersion", 6);
    fineCommand->setProperty ("pad", 0);
    fineCommand->setProperty ("value", 37.5);
    const auto fineSet = original.setPadFineTune (juce::var (fineCommand));
    const auto amplitudeSetting = [] (int pad, const char* field, double value)
    {
        auto* item = new juce::DynamicObject();
        item->setProperty ("schemaVersion", 8);
        item->setProperty ("pad", pad);
        item->setProperty ("field", field);
        item->setProperty ("value", value);
        return juce::var (item);
    };
    const auto amplitudeSet = original.setPadAmplitude (amplitudeSetting (0, "attack", 0.001))
                              && original.setPadAmplitude (amplitudeSetting (0, "decay", 0.02))
                              && original.setPadAmplitude (amplitudeSetting (0, "sustain", 0.6))
                              && original.setPadAmplitude (amplitudeSetting (0, "release", 0.03));
    const auto filterSetting = [] (int pad, const char* field, double value)
    {
        auto* item = new juce::DynamicObject();
        item->setProperty ("schemaVersion", 9);
        item->setProperty ("pad", pad);
        item->setProperty ("field", field);
        item->setProperty ("value", value);
        return juce::var (item);
    };
    const auto filterSet = original.setPadFilter (filterSetting (0, "mode", 1))
                           && original.setPadFilter (filterSetting (0, "cutoff", 5000))
                           && original.setPadFilter (filterSetting (0, "resonance", 0.8))
                           && original.setPadFilter (filterSetting (0, "depth", 12))
                           && original.setPadFilter (filterSetting (0, "attack", 0.01))
                           && original.setPadFilter (filterSetting (0, "decay", 0.02))
                           && original.setPadFilter (filterSetting (0, "sustain", 0.7))
                           && original.setPadFilter (filterSetting (0, "release", 0.03));
    const auto lfoSetting = [] (int pad, const char* field, double value)
    {
        auto* item = new juce::DynamicObject();
        item->setProperty ("schemaVersion", 10);
        item->setProperty ("pad", pad);
        item->setProperty ("field", field);
        item->setProperty ("value", value);
        return juce::var (item);
    };
    const auto lfoSet = original.setPadLfo (lfoSetting (0, "destination", 1))
                        && original.setPadLfo (lfoSetting (0, "shape", 2))
                        && original.setPadLfo (lfoSetting (0, "rate", 5))
                        && original.setPadLfo (lfoSetting (0, "depth", 12))
                        && original.setPadLfo (lfoSetting (0, "phase", 1));
    const auto invalidVoicesAccepted = original.setPadVoiceLimit (setting (0, 0));
    if (! modeSet || ! chokeSet || ! muteSet || ! soloSet || ! voicesSet || ! velocitySet
        || ! coarseSet || ! fineSet || ! amplitudeSet || ! filterSet || ! lfoSet
        || original.setPadLfo (lfoSetting (0, "destination", 3))
        || original.setPadLfo (lfoSetting (0, "shape", 1.5))
        || original.setPadLfo (lfoSetting (0, "rate", 0))
        || original.setPadLfo (lfoSetting (0, "depth", 25))
        || original.setPadLfo (lfoSetting (0, "unknown", 1))
        || original.setPadFilter (filterSetting (0, "mode", 1.5))
        || original.setPadFilter (filterSetting (0, "cutoff", 20001))
        || original.setPadFilter (filterSetting (0, "resonance", 0.1))
        || original.setPadFilter (filterSetting (0, "depth", 49))
        || original.setPadFilter (filterSetting (0, "unknown", 1))
        || original.setPadAmplitude (amplitudeSetting (0, "attack", -1.0))
        || original.setPadAmplitude (amplitudeSetting (0, "sustain", 1.1))
        || original.setPadAmplitude (amplitudeSetting (0, "unknown", 0.5))
        || original.setPadCoarseTune (setting (0, 25))
        || invalidPadAccepted || invalidChokeAccepted || invalidMuteAccepted
        || invalidVoicesAccepted)
    {
        std::cerr << "Pad setting command validation failed: " << modeSet << ' '
                  << chokeSet << ' ' << invalidPadAccepted << ' ' << invalidChokeAccepted << "\n";
        return 1;
    }
    const auto hostTextMatches = [&original] (const juce::String& name,
                                               const juce::String& text,
                                               const juce::String& unit)
    {
        for (auto* parameter : original.getParameters())
            if (parameter->getName (128) == name)
                return parameter->getCurrentValueAsText() == text
                    && parameter->getLabel() == unit;
        return false;
    };
    if (! hostTextMatches ("Pad 01 amplitude attack seconds", "0.001", "s")
        || ! hostTextMatches ("Pad 01 filter cutoff Hz", "5000", "Hz")
        || ! hostTextMatches ("Pad 01 filter envelope depth semitones", "12.0", "st")
        || ! hostTextMatches ("Pad 01 LFO rate Hz", "5.00", "Hz")
        || ! hostTextMatches ("Pad 01 LFO depth semitones", "12.0", "st"))
    {
        std::cerr << "Host parameter text or unit mismatch\n";
        return 1;
    }
    HyperrealAudioProcessor hostAutomation;
    auto* automatedCutoff = static_cast<juce::AudioProcessorParameter*> (nullptr);
    auto* automatedRate = static_cast<juce::AudioProcessorParameter*> (nullptr);
    for (auto* parameter : hostAutomation.getParameters())
    {
        if (parameter->getName (128) == "Pad 01 filter cutoff Hz")
            automatedCutoff = parameter;
        if (parameter->getName (128) == "Pad 01 LFO rate Hz")
            automatedRate = parameter;
    }
    if (automatedCutoff == nullptr || automatedRate == nullptr)
        return 1;
    automatedCutoff->setValueNotifyingHost (0.25f);
    automatedRate->setValueNotifyingHost (0.7f);
    const auto automatedSnapshot = hostAutomation.getSnapshot();
    if (static_cast<double> (automatedSnapshot.getProperty ("filterCutoff", 0.0)) <= 20.0
        || static_cast<double> (automatedSnapshot.getProperty ("lfoRate", 0.0)) <= 1.0)
    {
        std::cerr << "Host-originated automation did not reach the editor snapshot\n";
        return 1;
    }
    auto* editCommand = new juce::DynamicObject();
    editCommand->setProperty ("schemaVersion", 6);
    editCommand->setProperty ("pad", 0);
    editCommand->setProperty ("startFrame", 100);
    editCommand->setProperty ("endFrame", 4000);
    editCommand->setProperty ("fadeInFrames", 40);
    editCommand->setProperty ("fadeOutFrames", 60);
    editCommand->setProperty ("reversed", true);
    juce::var editCommandValue (editCommand);
    if (! original.setPadEdit (editCommandValue))
        return 1;
    const auto sampleRevision = static_cast<juce::int64> (
        original.getSnapshot().getProperty ("sampleRevision", -1));
    editCommand->setProperty ("sampleRevision", sampleRevision + 1);
    if (original.setPadEdit (editCommandValue))
    {
        std::cerr << "Stale edit command was accepted\n";
        return 1;
    }
    editCommand->setProperty ("sampleRevision", sampleRevision);
    if (! original.setPadEdit (editCommandValue))
        return 1;

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
    const auto phase7SampleOffset = static_cast<size_t> (stateReader.getPosition());
    juce::MemoryBlock phase7Samples (
        static_cast<const char*> (state.getData()) + phase7SampleOffset,
        state.getSize() - phase7SampleOffset);
    juce::MemoryBlock legacySamples;
    {
        juce::MemoryOutputStream output (legacySamples, false);
        for (int pad = 0; pad < SampleEngine::padCount; ++pad)
        {
            const auto present = stateReader.readByte();
            output.writeByte (present);
            if (present == 0)
                continue;
            const auto name = stateReader.readString();
            const auto rate = stateReader.readDouble();
            const auto channels = stateReader.readInt();
            const auto count = stateReader.readInt();
            output.writeString (name);
            output.writeDouble (rate);
            output.writeInt (channels);
            output.writeInt (count);
            for (int channel = 0; channel < channels; ++channel)
                for (int frame = 0; frame < count; ++frame)
                    output.writeFloat (stateReader.readFloat());
            for (int field = 0; field < 4; ++field)
                stateReader.readInt();
            stateReader.readByte();
        }
    }
    if (stateReader.getNumBytesRemaining() != 0)
        return 1;
    auto phase6Parameters = juce::ValueTree::readFromData (parameterBytes.getData(),
                                                           parameterBytes.getSize());
    phase6Parameters.setProperty ("schema_version", 6, nullptr);
    for (int child = phase6Parameters.getNumChildren() - 1; child >= 0; --child)
    {
        const auto id = phase6Parameters.getChild (child).getProperty ("id").toString();
        if (id.contains (".coarse.") || id.contains (".fine.")
            || id.contains (".amp.") || id.contains (".filter.") || id.contains (".lfo."))
            phase6Parameters.removeChild (child, nullptr);
    }
    auto phase5Parameters = juce::ValueTree::readFromData (parameterBytes.getData(),
                                                           parameterBytes.getSize());
    phase5Parameters.setProperty ("schema_version", 5, nullptr);
    for (int child = phase5Parameters.getNumChildren() - 1; child >= 0; --child)
    {
        const auto id = phase5Parameters.getChild (child).getProperty ("id").toString();
        if (id.contains (".voices.") || id.contains (".velocity.")
            || id.contains (".coarse.") || id.contains (".fine.")
            || id.contains (".amp.") || id.contains (".filter.") || id.contains (".lfo."))
            phase5Parameters.removeChild (child, nullptr);
    }
    auto phase4Parameters = juce::ValueTree::readFromData (parameterBytes.getData(),
                                                           parameterBytes.getSize());
    phase4Parameters.setProperty ("schema_version", 4, nullptr);
    for (int child = phase4Parameters.getNumChildren() - 1; child >= 0; --child)
    {
        const auto id = phase4Parameters.getChild (child).getProperty ("id").toString();
        if (id.contains (".mute.") || id.contains (".solo.")
            || id.contains (".voices.") || id.contains (".velocity.")
            || id.contains (".coarse.") || id.contains (".fine.")
            || id.contains (".amp.") || id.contains (".filter.") || id.contains (".lfo."))
            phase4Parameters.removeChild (child, nullptr);
    }
    auto makePreviousState = [&legacySamples] (const juce::ValueTree& tree,
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
        output.write (legacySamples.getData(), legacySamples.getSize());
        return previous;
    };
    auto phase7Parameters = juce::ValueTree::readFromData (parameterBytes.getData(),
                                                           parameterBytes.getSize());
    phase7Parameters.setProperty ("schema_version", 7, nullptr);
    for (int child = phase7Parameters.getNumChildren() - 1; child >= 0; --child)
        if (phase7Parameters.getChild (child).getProperty ("id").toString().contains (".amp.")
            || phase7Parameters.getChild (child).getProperty ("id").toString().contains (".filter.")
            || phase7Parameters.getChild (child).getProperty ("id").toString().contains (".lfo."))
            phase7Parameters.removeChild (child, nullptr);
    juce::MemoryBlock phase7State;
    {
        juce::MemoryBlock phase7ParameterBytes;
        juce::MemoryOutputStream parametersOutput (phase7ParameterBytes, false);
        phase7Parameters.writeToStream (parametersOutput);
        juce::MemoryOutputStream output (phase7State, false);
        output.write ("HRP7", 4);
        output.writeInt (7);
        output.writeInt (static_cast<int> (phase7ParameterBytes.getSize()));
        output.write (phase7ParameterBytes.getData(), phase7ParameterBytes.getSize());
        output.write (phase7Samples.getData(), phase7Samples.getSize());
    }
    auto phase8Parameters = juce::ValueTree::readFromData (parameterBytes.getData(),
                                                           parameterBytes.getSize());
    phase8Parameters.setProperty ("schema_version", 8, nullptr);
    for (int child = phase8Parameters.getNumChildren() - 1; child >= 0; --child)
        if (phase8Parameters.getChild (child).getProperty ("id").toString().contains (".filter.")
            || phase8Parameters.getChild (child).getProperty ("id").toString().contains (".lfo."))
            phase8Parameters.removeChild (child, nullptr);
    juce::MemoryBlock phase8State;
    {
        juce::MemoryBlock phase8ParameterBytes;
        juce::MemoryOutputStream parametersOutput (phase8ParameterBytes, false);
        phase8Parameters.writeToStream (parametersOutput);
        juce::MemoryOutputStream output (phase8State, false);
        output.write ("HRP8", 4);
        output.writeInt (8);
        output.writeInt (static_cast<int> (phase8ParameterBytes.getSize()));
        output.write (phase8ParameterBytes.getData(), phase8ParameterBytes.getSize());
        output.write (phase7Samples.getData(), phase7Samples.getSize());
    }
    auto phase9Parameters = juce::ValueTree::readFromData (parameterBytes.getData(),
                                                           parameterBytes.getSize());
    phase9Parameters.setProperty ("schema_version", 9, nullptr);
    for (int child = phase9Parameters.getNumChildren() - 1; child >= 0; --child)
        if (phase9Parameters.getChild (child).getProperty ("id").toString().contains (".lfo."))
            phase9Parameters.removeChild (child, nullptr);
    juce::MemoryBlock phase9State;
    {
        juce::MemoryBlock phase9ParameterBytes;
        juce::MemoryOutputStream parametersOutput (phase9ParameterBytes, false);
        phase9Parameters.writeToStream (parametersOutput);
        juce::MemoryOutputStream output (phase9State, false);
        output.write ("HRP9", 4);
        output.writeInt (9);
        output.writeInt (static_cast<int> (phase9ParameterBytes.getSize()));
        output.write (phase9ParameterBytes.getData(), phase9ParameterBytes.getSize());
        output.write (phase7Samples.getData(), phase7Samples.getSize());
    }
    const auto phase6State = makePreviousState (phase6Parameters, "HRP6", 6);
    const auto phase5State = makePreviousState (phase5Parameters, "HRP5", 5);
    const auto phase4State = makePreviousState (phase4Parameters, "HRP4", 4);
    auto phase3Parameters = juce::ValueTree::readFromData (parameterBytes.getData(),
                                                           parameterBytes.getSize());
    phase3Parameters.setProperty ("schema_version", 3, nullptr);
    for (int child = phase3Parameters.getNumChildren() - 1; child >= 0; --child)
    {
        const auto id = phase3Parameters.getChild (child).getProperty ("id").toString();
        if (id.contains (".mode.") || id.contains (".choke.")
            || id.contains (".mute.") || id.contains (".solo.")
            || id.contains (".voices.") || id.contains (".velocity.")
            || id.contains (".coarse.") || id.contains (".fine.")
            || id.contains (".amp.") || id.contains (".filter.") || id.contains (".lfo."))
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
            || id.contains (".mute.") || id.contains (".solo.")
            || id.contains (".voices.") || id.contains (".velocity.")
            || id.contains (".coarse.") || id.contains (".fine.")
            || id.contains (".amp.") || id.contains (".filter.") || id.contains (".lfo."))
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
        || static_cast<int> (snapshot.getProperty ("voiceLimit", -1)) != 1
        || static_cast<int> (snapshot.getProperty ("velocityCurve", -1)) != 2
        || static_cast<int> (snapshot.getProperty ("coarseTune", -1)) != 7
        || std::abs (static_cast<double> (snapshot.getProperty ("fineTune", 0.0)) - 37.5) > 0.11
        || std::abs (static_cast<double> (snapshot.getProperty ("ampAttack", -1.0)) - 0.001) > 0.001
        || std::abs (static_cast<double> (snapshot.getProperty ("ampDecay", -1.0)) - 0.02) > 0.001
        || std::abs (static_cast<double> (snapshot.getProperty ("ampSustain", -1.0)) - 0.6) > 0.001
        || std::abs (static_cast<double> (snapshot.getProperty ("ampRelease", -1.0)) - 0.03) > 0.001
        || static_cast<int> (snapshot.getProperty ("filterMode", -1)) != 1
        || std::abs (static_cast<double> (snapshot.getProperty ("filterCutoff", -1.0)) - 5000.0) > 1.0
        || std::abs (static_cast<double> (snapshot.getProperty ("filterResonance", -1.0)) - 0.8) > 0.02
        || std::abs (static_cast<double> (snapshot.getProperty ("filterDepth", -100.0)) - 12.0) > 0.11
        || std::abs (static_cast<double> (snapshot.getProperty ("filterSustain", -1.0)) - 0.7) > 0.001
        || static_cast<int> (snapshot.getProperty ("lfoDestination", -1)) != 1
        || static_cast<int> (snapshot.getProperty ("lfoShape", -1)) != 2
        || std::abs (static_cast<double> (snapshot.getProperty ("lfoRate", -1.0)) - 5.0) > 0.1
        || std::abs (static_cast<double> (snapshot.getProperty ("lfoDepth", -1.0)) - 12.0) > 0.1
        || static_cast<int> (snapshot.getProperty ("lfoPhaseMode", -1)) != 1
        || static_cast<int> (snapshot.getProperty ("trimStart", -1)) != 100
        || static_cast<int> (snapshot.getProperty ("trimEnd", -1)) != 4000
        || ! static_cast<bool> (snapshot.getProperty ("reverse", false))
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
    midi.addEvent (juce::MidiMessage::noteOn (1, 36, static_cast<juce::uint8> (127)), 0);
    migrated.processBlock (audio, midi);
    if (audio.getMagnitude (0, 0, 128) < 0.15f)
    {
        std::cerr << "Migrated Phase 2 sample did not play\n";
        return 1;
    }
    migrated.prepareToPlay (44100.0, 128);
    auto* fullGain = new juce::DynamicObject();
    fullGain->setProperty ("schemaVersion", 1);
    fullGain->setProperty ("value", 1.0);
    if (! migrated.setMasterGain (juce::var (fullGain)))
        return 1;
    midi.clear();
    for (int voice = 0; voice < 4; ++voice)
        midi.addEvent (juce::MidiMessage::noteOn (
            1, 36, static_cast<juce::uint8> (127)), 0);
    migrated.processBlock (audio, midi);
    const auto clipSnapshot = migrated.getSnapshot();
    if (static_cast<double> (clipSnapshot.getProperty ("masterPeak", 0.0)) < 1.0
        || ! static_cast<bool> (clipSnapshot.getProperty ("masterClip", false)))
    {
        std::cerr << "Master meter missed a clipped output block\n";
        return 1;
    }
    midi.clear();
    for (int block = 0; block < 350; ++block)
        migrated.processBlock (audio, midi);
    const auto quietSnapshot = migrated.getSnapshot();
    if (static_cast<bool> (quietSnapshot.getProperty ("masterClip", true))
        || static_cast<double> (quietSnapshot.getProperty ("masterPeak", 1.0)) >= 0.5)
    {
        std::cerr << "Master meter did not release after silence\n";
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
    HyperrealAudioProcessor phase5Migrated;
    phase5Migrated.setStateInformation (phase5State.getData(), static_cast<int> (phase5State.getSize()));
    if (phase5Migrated.getSnapshot().getProperty ("status", {}).toString() != "ready"
        || static_cast<int> (phase5Migrated.getSnapshot().getProperty ("voiceLimit", -1)) != 4
        || static_cast<int> (phase5Migrated.getSnapshot().getProperty ("velocityCurve", -1)) != 1)
    {
        std::cerr << "Phase 5 project migration failed\n";
        return 1;
    }
    HyperrealAudioProcessor phase6Migrated;
    phase6Migrated.setStateInformation (phase6State.getData(), static_cast<int> (phase6State.getSize()));
    if (phase6Migrated.getSnapshot().getProperty ("status", {}).toString() != "ready"
        || static_cast<int> (phase6Migrated.getSnapshot().getProperty ("trimStart", -1)) != 0
        || static_cast<int> (phase6Migrated.getSnapshot().getProperty ("trimEnd", -1)) != frames
        || static_cast<int> (phase6Migrated.getSnapshot().getProperty ("coarseTune", -1)) != 0)
    {
        std::cerr << "Phase 6 project migration failed\n";
        return 1;
    }
    HyperrealAudioProcessor phase7Migrated;
    phase7Migrated.setStateInformation (phase7State.getData(), static_cast<int> (phase7State.getSize()));
    const auto phase7Snapshot = phase7Migrated.getSnapshot();
    if (phase7Snapshot.getProperty ("status", {}).toString() != "ready"
        || static_cast<int> (phase7Snapshot.getProperty ("trimStart", -1)) != 100
        || std::abs (static_cast<double> (phase7Snapshot.getProperty ("ampAttack", -1.0))) > 0.001
        || static_cast<int> (phase7Snapshot.getProperty ("filterMode", -1)) != 0
        || std::abs (static_cast<double> (phase7Snapshot.getProperty ("ampSustain", -1.0)) - 1.0) > 0.001)
    {
        std::cerr << "Phase 7 project migration failed\n";
        return 1;
    }
    HyperrealAudioProcessor phase8Migrated;
    phase8Migrated.setStateInformation (phase8State.getData(), static_cast<int> (phase8State.getSize()));
    const auto phase8Snapshot = phase8Migrated.getSnapshot();
    if (phase8Snapshot.getProperty ("status", {}).toString() != "ready"
        || static_cast<int> (phase8Snapshot.getProperty ("trimStart", -1)) != 100
        || std::abs (static_cast<double> (phase8Snapshot.getProperty ("ampSustain", -1.0)) - 0.6) > 0.001
        || static_cast<int> (phase8Snapshot.getProperty ("filterMode", -1)) != 0)
    {
        std::cerr << "Phase 8 project migration failed\n";
        return 1;
    }
    HyperrealAudioProcessor phase9Migrated;
    phase9Migrated.setStateInformation (phase9State.getData(), static_cast<int> (phase9State.getSize()));
    const auto phase9Snapshot = phase9Migrated.getSnapshot();
    if (phase9Snapshot.getProperty ("status", {}).toString() != "ready"
        || static_cast<int> (phase9Snapshot.getProperty ("filterMode", -1)) != 1
        || static_cast<int> (phase9Snapshot.getProperty ("lfoDestination", -1)) != 0
        || std::abs (static_cast<double> (phase9Snapshot.getProperty ("lfoRate", -1.0)) - 1.0) > 0.1)
    {
        std::cerr << "Phase 9 project migration failed\n";
        return 1;
    }

    HyperrealAudioProcessor operations;
    operations.setStateInformation (state.getData(), static_cast<int> (state.getSize()));
    if (! operations.copyPad (0) || ! operations.pastePad (2)
        || ! operations.selectPad (2)
        || operations.getSnapshot().getProperty ("status", {}).toString() != "ready"
        || static_cast<int> (operations.getSnapshot().getProperty ("padMode", -1)) != 1
        || static_cast<int> (operations.getSnapshot().getProperty ("chokeGroup", -1)) != 2
        || static_cast<int> (operations.getSnapshot().getProperty ("voiceLimit", -1)) != 1
        || static_cast<int> (operations.getSnapshot().getProperty ("velocityCurve", -1)) != 2
        || static_cast<int> (operations.getSnapshot().getProperty ("trimStart", -1)) != 100
        || static_cast<int> (operations.getSnapshot().getProperty ("trimEnd", -1)) != 4000
        || std::abs (static_cast<double> (
            operations.getSnapshot().getProperty ("ampSustain", -1.0)) - 0.6) > 0.001
        || std::abs (static_cast<double> (
            operations.getSnapshot().getProperty ("filterDepth", -100.0)) - 12.0) > 0.11
        || static_cast<int> (operations.getSnapshot().getProperty ("lfoDestination", -1)) != 1
        || static_cast<int> (operations.getSnapshot().getProperty ("lfoShape", -1)) != 2
        || static_cast<int> (operations.getSnapshot().getProperty ("lfoPhaseMode", -1)) != 1
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
        || static_cast<int> (operations.getSnapshot().getProperty ("padMode", -1)) != 0
        || static_cast<int> (operations.getSnapshot().getProperty ("voiceLimit", -1)) != 4
        || static_cast<int> (operations.getSnapshot().getProperty ("velocityCurve", -1)) != 1
        || std::abs (static_cast<double> (
            operations.getSnapshot().getProperty ("ampSustain", -1.0)) - 1.0) > 0.001
        || static_cast<int> (operations.getSnapshot().getProperty ("filterMode", -1)) != 0
        || static_cast<int> (operations.getSnapshot().getProperty ("lfoDestination", -1)) != 0)
    {
        std::cerr << "Pad clear did not reset sample and settings\n";
        return 1;
    }

    HyperrealAudioProcessor clicked;
    clicked.setStateInformation (state.getData(), static_cast<int> (state.getSize()));
    clicked.prepareToPlay (44100.0, 128);
    clicked.setPadMute (setting (0, 0));
    clicked.setPadSolo (setting (1, 0));
    if (! clicked.producesMidi())
    {
        std::cerr << "Pad-click MIDI output was not advertised\n";
        return 1;
    }
    clicked.requestAudition (0);
    juce::MidiBuffer clickMidi;
    clickMidi.addEvent (juce::MidiMessage::noteOn (1, 37, static_cast<juce::uint8> (100)), 0);
    clicked.processBlock (audio, clickMidi);
    int outputNotes = 0;
    for (const auto event : clickMidi)
    {
        if (! event.getMessage().isNoteOn() || event.getMessage().getNoteNumber() != 36)
        {
            std::cerr << "Pad-click output contained an unexpected MIDI event\n";
            return 1;
        }
        ++outputNotes;
    }
    if (outputNotes != 1 || audio.getMagnitude (0, 0, 128) < 0.1f)
    {
        std::cerr << "Pad click did not audition and emit its MIDI note\n";
        return 1;
    }
    bool noteOffReceived = false;
    for (int block = 0; block < 40; ++block)
    {
        clickMidi.clear();
        clicked.processBlock (audio, clickMidi);
        for (const auto event : clickMidi)
            if (event.getMessage().isNoteOff() && event.getMessage().getNoteNumber() == 36)
                noteOffReceived = true;
    }
    if (! noteOffReceived)
    {
        std::cerr << "Pad-click MIDI note-off was not emitted\n";
        return 1;
    }

    std::cout << "Processor state smoke passed\n";
    return 0;
}
