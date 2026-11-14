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

    juce::MemoryBlock state;
    original.getStateInformation (state);
    file.deleteFile();

    HyperrealAudioProcessor restored;
    restored.setStateInformation (state.getData(), static_cast<int> (state.getSize()));
    restored.prepareToPlay (44100.0, 128);
    const auto snapshot = restored.getSnapshot();
    if (snapshot.getProperty ("status", {}).toString() != "ready"
        || std::abs (static_cast<double> (snapshot.getProperty ("masterGain", 0.0)) - 0.5) > 0.001)
    {
        std::cerr << "Full project state did not restore\n";
        return 1;
    }

    juce::AudioBuffer<float> audio (2, 128);
    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (1, 36, static_cast<juce::uint8> (127)), 0);
    restored.processBlock (audio, midi);
    const auto magnitude = audio.getMagnitude (0, 0, 128);
    if (magnitude < 0.15f || magnitude > 0.3f)
    {
        std::cerr << "Restored playback or master gain failed: " << magnitude << "\n";
        return 1;
    }

    std::cout << "Processor state smoke passed\n";
    return 0;
}
