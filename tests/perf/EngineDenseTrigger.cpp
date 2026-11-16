#include "../../packages/engine/SampleEngine.h"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <fstream>
#include <thread>
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

bool writeImportWav (const juce::File& file)
{
    std::ofstream output (file.getFullPathName().toStdString(), std::ios::binary);
    if (! output)
        return false;
    auto write16 = [&output] (unsigned int value)
    {
        output.put (static_cast<char> (value & 0xff));
        output.put (static_cast<char> ((value >> 8) & 0xff));
    };
    auto write32 = [&write16] (unsigned int value)
    {
        write16 (value & 0xffff);
        write16 (value >> 16);
    };
    output.write ("RIFF", 4);
    write32 (36 + 4410 * 2);
    output.write ("WAVEfmt ", 8);
    write32 (16);
    write16 (1);
    write16 (1);
    write32 (44100);
    write32 (88200);
    write16 (2);
    write16 (16);
    output.write ("data", 4);
    write32 (4410 * 2);
    for (int frame = 0; frame < 4410; ++frame)
        write16 (16384);
    return output.good();
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
            output.writeInt (0);    // Trim start.
            output.writeInt (4410); // Exclusive trim end.
            output.writeInt (0);    // Fade in.
            output.writeInt (220);  // Default 5 ms fade out at 44.1 kHz.
            output.writeByte (0);   // Forward playback.
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
    for (auto& pad : settings)
    {
        pad.amplitude = { 0.003f, 0.04f, 0.7f, 0.03f };
        pad.filter = { 1, 4000.0f, 0.8f, 12.0f,
                       { 0.005f, 0.04f, 0.6f, 0.03f } };
        pad.lfo = { 2, 0, 5.0f, 12.0f, 1 };
    }
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

    constexpr int editCycles = 5000;
    auto edit = [&engine, &audio, &midi, &settings] ()
    {
        if (! engine.copyPad (0) || ! engine.pastePad (1)
            || ! engine.swapPads (1, 2) || ! engine.clearPad (2))
            return false;
        const auto snapshot = engine.getSnapshot (1);
        if (snapshot.getProperty ("pads", {}).getArray() == nullptr)
            return false;
        engine.process (audio, midi, settings);
        return true;
    };
    for (int cycle = 0; cycle < 100; ++cycle)
        if (! edit())
            return 2;
    const auto beforeEdits = privateBytes();
    for (int cycle = 0; cycle < editCycles; ++cycle)
        if (! edit())
            return 3;
    const auto afterEdits = privateBytes();
    std::cout << editCycles << " pad copy/paste/swap/clear and snapshot cycles, "
              << beforeEdits << " to " << afterEdits << " private bytes\n";

    const auto revision = static_cast<juce::int64> (
        engine.getSnapshot().getProperty ("sampleRevision", -1));
    const auto beforeLiveEdits = privateBytes();
    constexpr int liveEditCycles = 5000;
    for (int cycle = 0; cycle < liveEditCycles; ++cycle)
    {
        if (! engine.setPadEdit (0, { 0, 4410, cycle % 512,
                                       (cycle * 3) % 512, (cycle & 1) != 0 }, revision))
            return 7;
        engine.process (audio, midi, settings);
    }
    const auto afterLiveEdits = privateBytes();
    std::cout << liveEditCycles << " immediate pad edits while processing, "
              << beforeLiveEdits << " to " << afterLiveEdits << " private bytes\n";

    const auto source = juce::File::getSpecialLocation (juce::File::tempDirectory)
                            .getChildFile ("hyperreal-benchmark-import.wav");
    if (! writeImportWav (source))
        return 4;
    auto replace = [&engine, &audio, &midi, &settings, &source] (int cycle)
    {
        const auto pad = cycle % SampleEngine::padCount;
        engine.requestLoad (pad, source);
        for (int attempt = 0; attempt < 1000; ++attempt)
        {
            engine.process (audio, midi, settings);
            const auto snapshot = engine.getSnapshot (pad);
            if (snapshot.getProperty ("status", {}).toString() == "ready"
                && snapshot.getProperty ("sampleName", {}).toString() == source.getFileName())
                return true;
            std::this_thread::sleep_for (std::chrono::milliseconds (1));
        }
        return false;
    };
    for (int cycle = 0; cycle < 100; ++cycle)
        if (! replace (cycle))
            return 5;
    const auto beforeImports = privateBytes();
    constexpr int importCycles = 1000;
    for (int cycle = 0; cycle < importCycles; ++cycle)
        if (! replace (cycle))
            return 6;
    const auto afterImports = privateBytes();
    source.deleteFile();
    std::cout << importCycles << " worker sample replacements while processing, "
              << beforeImports << " to " << afterImports << " private bytes\n";
    return 0;
}
