#include "../../packages/engine/SampleEngine.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
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
void writeWav (const juce::File& file, int value)
{
    constexpr int frames = 4410;
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
        write16 (wav, static_cast<unsigned int> (value));
}
bool waitForReady (SampleEngine& engine, int pad)
{
    for (int attempt = 0; attempt < 300; ++attempt)
    {
        if (engine.getSnapshot (pad).getProperty ("status", {}).toString() == "ready")
            return true;
        std::this_thread::sleep_for (std::chrono::milliseconds (10));
    }
    return false;
}
bool near (float actual, float expected)
{
    return std::abs (actual - expected) < 0.02f;
}
}

int main()
{
    const auto folder = juce::File::getSpecialLocation (juce::File::tempDirectory);
    const auto first = folder.getChildFile ("hyperreal-engine-pad01.wav");
    const auto second = folder.getChildFile ("hyperreal-engine-pad02.wav");
    const auto replacement = folder.getChildFile ("hyperreal-engine-replacement.wav");
    writeWav (first, 6553);
    writeWav (second, 13107);
    writeWav (replacement, 19660);

    SampleEngine engine;
    engine.prepare (44100.0);
    engine.requestLoad (0, first);
    engine.requestLoad (1, second);
    if (! waitForReady (engine, 0) || ! waitForReady (engine, 1))
    {
        std::cerr << "Queued pad imports failed\n";
        return 2;
    }
    const auto selectedSnapshot = engine.getSnapshot (1);
    const auto padList = selectedSnapshot.getProperty ("pads", {});
    const auto pads = padList.getArray();
    if (pads == nullptr || pads->size() != SampleEngine::padCount
        || selectedSnapshot.getProperty ("sampleName", {}).toString() != second.getFileName())
    {
        std::cerr << "Pad snapshot contract failed\n";
        return 2;
    }

    juce::AudioBuffer<float> audio (2, 128);
    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::controllerEvent (1, 1, 64), 16);
    midi.addEvent (juce::MidiMessage::noteOn (1, 36, static_cast<juce::uint8> (127)), 64);
    midi.addEvent (juce::MidiMessage::noteOn (1, 37, static_cast<juce::uint8> (127)), 96);
    engine.process (audio, midi);
    if (! near (audio.getSample (0, 0), 0.0f)
        || ! near (audio.getSample (0, 64), 0.2f)
        || ! near (audio.getSample (0, 96), 0.6f))
    {
        std::cerr << "Independent pad MIDI scheduling failed\n";
        return 3;
    }

    engine.prepare (44100.0);
    midi.clear();
    midi.addEvent (juce::MidiMessage::noteOn (1, 36, static_cast<juce::uint8> (127)), 0);
    midi.addEvent (juce::MidiMessage::noteOn (1, 36, static_cast<juce::uint8> (127)), 32);
    engine.process (audio, midi);
    if (! near (audio.getSample (0, 40), 0.4f))
    {
        std::cerr << "Overlapping voices failed\n";
        return 4;
    }

    engine.requestLoad (0, replacement);
    if (! waitForReady (engine, 0))
        return 5;
    midi.clear();
    engine.process (audio, midi);
    if (! near (audio.getSample (0, 0), 0.4f))
    {
        std::cerr << "Replacement interrupted old voices\n";
        return 6;
    }

    juce::MemoryBlock state;
    {
        juce::MemoryOutputStream output (state, false);
        engine.writeSampleState (output);
    }
    juce::MemoryInputStream input (state.getData(), state.getSize(), false);
    SampleEngine restored;
    restored.prepare (44100.0);
    if (! restored.restoreSampleState (input)
        || restored.getSnapshot (0).getProperty ("status", {}).toString() != "ready"
        || restored.getSnapshot (1).getProperty ("status", {}).toString() != "ready"
        || restored.getSnapshot (2).getProperty ("status", {}).toString() != "empty")
    {
        std::cerr << "Multi-pad embedded state failed\n";
        return 7;
    }

    first.deleteFile();
    second.deleteFile();
    replacement.deleteFile();
    restored.requestLoad (1, second);
    std::this_thread::sleep_for (std::chrono::milliseconds (100));
    if (restored.getSnapshot (1).getProperty ("status", {}).toString() != "ready")
    {
        std::cerr << "Invalid import replaced active pad\n";
        return 8;
    }

    midi.addEvent (juce::MidiMessage::noteOn (1, 36, static_cast<juce::uint8> (127)), 0);
    midi.addEvent (juce::MidiMessage::noteOn (1, 37, static_cast<juce::uint8> (127)), 0);
    restored.process (audio, midi);
    if (! near (audio.getSample (0, 0), 1.0f))
    {
        std::cerr << "Restored pads did not play\n";
        return 9;
    }

    restored.prepare (44100.0);
    midi.clear();
    for (int voice = 0; voice < 5; ++voice)
        midi.addEvent (juce::MidiMessage::noteOn (1, 36, static_cast<juce::uint8> (127)), 0);
    restored.process (audio, midi);
    if (! near (audio.getSample (0, 0), 2.4f))
    {
        std::cerr << "Per-pad voice bound or deterministic stealing failed\n";
        return 10;
    }

    SampleEngine::PlaybackSettings settings {};
    settings[0].gated = true;
    restored.prepare (44100.0);
    midi.clear();
    midi.addEvent (juce::MidiMessage::noteOn (1, 36, static_cast<juce::uint8> (127)), 0);
    midi.addEvent (juce::MidiMessage::noteOff (1, 36), 64);
    restored.process (audio, midi, settings);
    if (! near (audio.getSample (0, 32), 0.6f)
        || audio.getSample (0, 127) >= 0.55f)
    {
        std::cerr << "Gated note-off did not begin release\n";
        return 11;
    }
    midi.clear();
    restored.process (audio, midi, settings);
    restored.process (audio, midi, settings);
    if (! near (audio.getSample (0, 127), 0.0f))
    {
        std::cerr << "Gated release did not finish\n";
        return 12;
    }

    settings[0].gated = false;
    restored.prepare (44100.0);
    midi.addEvent (juce::MidiMessage::noteOn (1, 36, static_cast<juce::uint8> (127)), 0);
    midi.addEvent (juce::MidiMessage::noteOff (1, 36), 32);
    restored.process (audio, midi, settings);
    if (! near (audio.getSample (0, 127), 0.6f))
    {
        std::cerr << "One-shot note-off stopped playback\n";
        return 13;
    }

    settings[0].chokeGroup = 1;
    settings[1].chokeGroup = 1;
    restored.prepare (44100.0);
    midi.clear();
    midi.addEvent (juce::MidiMessage::noteOn (1, 36, static_cast<juce::uint8> (127)), 0);
    midi.addEvent (juce::MidiMessage::noteOn (1, 37, static_cast<juce::uint8> (127)), 64);
    restored.process (audio, midi, settings);
    if (audio.getSample (0, 127) <= 0.4f || audio.getSample (0, 127) >= 0.95f)
    {
        std::cerr << "Choke did not fade the earlier one-shot\n";
        return 14;
    }
    midi.clear();
    restored.process (audio, midi, settings);
    restored.process (audio, midi, settings);
    if (! near (audio.getSample (0, 127), 0.4f))
    {
        std::cerr << "Choked voice did not end\n";
        return 15;
    }

    restored.prepare (44100.0);
    midi.addEvent (juce::MidiMessage::noteOn (1, 36, static_cast<juce::uint8> (127)), 0);
    midi.addEvent (juce::MidiMessage::controllerEvent (1, 123, 0), 32);
    restored.process (audio, midi, settings);
    midi.clear();
    restored.process (audio, midi, settings);
    restored.process (audio, midi, settings);
    if (! near (audio.getSample (0, 127), 0.0f))
    {
        std::cerr << "All-notes-off left a voice playing\n";
        return 16;
    }

    settings[0].chokeGroup = 0;
    settings[0].level = 0.5f;
    restored.prepare (44100.0);
    midi.addEvent (juce::MidiMessage::noteOn (1, 36, static_cast<juce::uint8> (127)), 0);
    restored.process (audio, midi, settings);
    if (! near (audio.getSample (0, 0), 0.3f))
    {
        std::cerr << "Per-pad level did not affect rendered audio\n";
        return 17;
    }

    settings[0].level = 1.0f;
    settings[0].muted = true;
    restored.prepare (44100.0);
    midi.clear();
    midi.addEvent (juce::MidiMessage::noteOn (1, 36, static_cast<juce::uint8> (127)), 0);
    midi.addEvent (juce::MidiMessage::noteOn (1, 37, static_cast<juce::uint8> (127)), 0);
    restored.process (audio, midi, settings);
    if (! near (audio.getSample (0, 0), 0.4f))
    {
        std::cerr << "Muted pad still triggered\n";
        return 18;
    }

    settings[0].muted = false;
    settings[1].solo = true;
    restored.prepare (44100.0);
    restored.process (audio, midi, settings);
    if (! near (audio.getSample (0, 0), 0.4f))
    {
        std::cerr << "Solo did not suppress non-solo pad\n";
        return 19;
    }

    settings[0].solo = true;
    restored.prepare (44100.0);
    restored.process (audio, midi, settings);
    if (! near (audio.getSample (0, 0), 1.0f))
    {
        std::cerr << "Multiple solo pads did not play together\n";
        return 20;
    }

    settings[0].muted = true;
    midi.clear();
    restored.process (audio, midi, settings);
    if (audio.getSample (0, 0) < 0.9f || audio.getSample (0, 127) >= 0.8f)
    {
        std::cerr << "Muting an active voice did not fade it\n";
        return 21;
    }
    restored.process (audio, midi, settings);
    if (audio.getSample (0, 127) > 0.5f)
    {
        std::cerr << "Muted voice continued after fade\n";
        return 22;
    }

    settings = {};
    restored.prepare (44100.0);
    if (! restored.copyPad (0) || ! restored.pastePad (2)
        || restored.getSnapshot (2).getProperty ("status", {}).toString() != "ready")
    {
        std::cerr << "Copy and paste did not create a third pad\n";
        return 23;
    }
    midi.clear();
    midi.addEvent (juce::MidiMessage::noteOn (1, 38, static_cast<juce::uint8> (127)), 0);
    restored.process (audio, midi, settings);
    if (! near (audio.getSample (0, 0), 0.6f))
    {
        std::cerr << "Pasted pad did not play at its grid MIDI note\n";
        return 24;
    }
    restored.prepare (44100.0);
    if (! restored.swapPads (1, 2))
        return 25;
    midi.clear();
    midi.addEvent (juce::MidiMessage::noteOn (1, 37, static_cast<juce::uint8> (127)), 0);
    restored.process (audio, midi, settings);
    if (! near (audio.getSample (0, 0), 0.6f))
    {
        std::cerr << "Swapped sample did not follow pad content\n";
        return 26;
    }
    if (! restored.clearPad (1)
        || restored.getSnapshot (1).getProperty ("status", {}).toString() != "empty")
        return 27;
    midi.clear();
    restored.process (audio, midi, settings);
    if (! near (audio.getSample (0, 0), 0.6f))
    {
        std::cerr << "Clear interrupted an already playing voice\n";
        return 28;
    }
    restored.prepare (44100.0);
    midi.addEvent (juce::MidiMessage::noteOn (1, 37, static_cast<juce::uint8> (127)), 0);
    restored.process (audio, midi, settings);
    if (! near (audio.getSample (0, 0), 0.0f))
    {
        std::cerr << "Cleared pad still accepted MIDI trigger\n";
        return 29;
    }

    std::cout << "SampleEngine multi-pad smoke passed\n";
    return 0;
}
