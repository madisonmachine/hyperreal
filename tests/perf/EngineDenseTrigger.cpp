#include "../../packages/engine/SampleEngine.h"
#include <algorithm>
#include <chrono>
#include <iostream>
#if JUCE_WINDOWS
#define NOMINMAX
#include <windows.h>
#include <psapi.h>
#endif

namespace
{
size_t privateBytes()
{
#if JUCE_WINDOWS
    PROCESS_MEMORY_COUNTERS_EX counters {};
    if (GetProcessMemoryInfo (GetCurrentProcess(),
                              reinterpret_cast<PROCESS_MEMORY_COUNTERS*> (&counters),
                              sizeof (counters)))
        return static_cast<size_t> (counters.PrivateUsage);
#endif
    return 0;
}
}

int main()
{
    juce::MemoryBlock kit;
    {
        juce::MemoryOutputStream output (kit, false);
        for (int pad = 0; pad < SampleEngine::padCount; ++pad)
        {
            output.writeByte (1);
            output.writeString ("benchmark.wav");
            output.writeDouble (44100.0);
            output.writeInt (1);
            output.writeInt (4410);
            for (int frame = 0; frame < 4410; ++frame)
                output.writeFloat (0.2f);
        }
    }

    SampleEngine engine;
    engine.prepare (44100.0);
    juce::MemoryInputStream input (kit.getData(), kit.getSize(), false);
    if (! engine.restoreSampleState (input))
        return 1;

    juce::AudioBuffer<float> audio (2, 128);
    juce::MidiBuffer midi;
    for (int pad = 0; pad < SampleEngine::padCount; ++pad)
        midi.addEvent (juce::MidiMessage::noteOn (
            1, SampleEngine::firstMidiNote + pad, static_cast<juce::uint8> (100)), pad * 8);
    SampleEngine::PlaybackSettings settings {};
    engine.process (audio, midi, settings);
    const auto initialPrivateBytes = privateBytes();
    constexpr int blocks = 100000;
    std::chrono::nanoseconds longest {};
    const auto start = std::chrono::steady_clock::now();
    for (int block = 0; block < blocks; ++block)
    {
        const auto before = std::chrono::steady_clock::now();
        engine.process (audio, midi, settings);
        longest = std::max (longest, std::chrono::duration_cast<std::chrono::nanoseconds> (
            std::chrono::steady_clock::now() - before));
    }
    const auto elapsed = std::chrono::steady_clock::now() - start;
    const auto elapsedMs = std::chrono::duration<double, std::milli> (elapsed).count();
    const auto worstMs = std::chrono::duration<double, std::milli> (longest).count();
    const auto finalPrivateBytes = privateBytes();
    std::cout << blocks << " blocks, " << blocks * SampleEngine::padCount
              << " triggers, " << elapsedMs << " ms total, "
              << elapsedMs / blocks << " ms/block average, "
              << worstMs << " ms worst observed block, "
              << initialPrivateBytes << " to " << finalPrivateBytes
              << " private bytes\n";
    return 0;
}
