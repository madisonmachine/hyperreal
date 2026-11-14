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

bool waitForReady (SampleEngine& engine)
{
    for (int attempt = 0; attempt < 300; ++attempt)
    {
        if (engine.getSnapshot().getProperty ("status", {}).toString() == "ready")
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
                          .getChildFile ("hyperreal-engine-smoke.wav");
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
        {
            const auto value = static_cast<int> (std::sin (frame * 0.1) * 16000.0);
            write16 (wav, static_cast<unsigned int> (value));
        }
    }

    SampleEngine engine;
    engine.prepare (44100.0);
    engine.requestLoad (file);
    if (! waitForReady (engine))
    {
        std::cerr << "Sample import failed\n";
        return 1;
    }

    juce::AudioBuffer<float> audio (2, 128);
    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (1, 36, static_cast<juce::uint8> (100)), 64);
    engine.process (audio, midi);
    if (audio.getMagnitude (0, 0, 64) != 0.0f || audio.getMagnitude (0, 64, 64) < 0.1f)
    {
        std::cerr << "MIDI sample offset or playback failed\n";
        return 1;
    }

    juce::MemoryBlock state;
    juce::MemoryOutputStream output (state, false);
    engine.writeSampleState (output);
    juce::MemoryInputStream input (state.getData(), state.getSize(), false);
    SampleEngine restored;
    restored.prepare (44100.0);
    if (! restored.restoreSampleState (input)
        || restored.getSnapshot().getProperty ("status", {}).toString() != "ready")
    {
        std::cerr << "Embedded sample state failed\n";
        return 1;
    }

    restored.requestLoad (file.getSiblingFile ("missing.txt"));
    std::this_thread::sleep_for (std::chrono::milliseconds (100));
    if (restored.getSnapshot().getProperty ("status", {}).toString() != "ready")
    {
        std::cerr << "Invalid import replaced active sample\n";
        return 1;
    }

    file.deleteFile();
    std::cout << "SampleEngine smoke passed\n";
    return 0;
}
