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
void writeRampWav (const juce::File& file, int rate = 44100)
{
    constexpr int frames = 256;
    std::ofstream wav (file.getFullPathName().toStdString(), std::ios::binary);
    wav.write ("RIFF", 4);
    write32 (wav, 36 + frames * 2);
    wav.write ("WAVEfmt ", 8);
    write32 (wav, 16);
    write16 (wav, 1);
    write16 (wav, 1);
    write32 (wav, static_cast<unsigned int> (rate));
    write32 (wav, static_cast<unsigned int> (rate * 2));
    write16 (wav, 2);
    write16 (wav, 16);
    wav.write ("data", 4);
    write32 (wav, frames * 2);
    for (int frame = 0; frame < frames; ++frame)
        write16 (wav, static_cast<unsigned int> ((frame + 1) * 100));
}
void writeToneWav (const juce::File& file)
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
        write16 (wav, static_cast<unsigned int> (static_cast<int> (
            std::sin (2.0 * 3.141592653589793 * 250.0 * frame / 44100.0) * 12000.0)));
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
    juce::MemoryBlock droppedBytes;
    juce::MemoryBlock replacementBytes;
    if (! first.loadFileAsData (droppedBytes)
        || ! replacement.loadFileAsData (replacementBytes))
        return 1;

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
    const auto midiSnapshot = engine.getSnapshot (0);
    const auto midiPadList = midiSnapshot.getProperty ("pads", {});
    const auto* midiPads = midiPadList.getArray();
    if (midiPads == nullptr
        || static_cast<int> (midiPads->getReference (0).getProperty ("midiTriggerCount", 0)) != 1
        || static_cast<int> (midiPads->getReference (1).getProperty ("midiTriggerCount", 0)) != 1
        || static_cast<int> (midiPads->getReference (2).getProperty ("midiTriggerCount", 0)) != 0)
    {
        std::cerr << "Per-pad MIDI activity counters failed\n";
        return 41;
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
    if (! near (audio.getSample (0, 0), 3.0f))
    {
        std::cerr << "Stolen voice did not enter its release tail\n";
        return 10;
    }
    midi.clear();
    restored.process (audio, midi);
    restored.process (audio, midi);
    if (! near (audio.getSample (0, 127), 2.4f))
    {
        std::cerr << "Stolen voice did not finish its release tail\n";
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

    restored.prepare (44100.0);
    midi.clear();
    midi.addEvent (juce::MidiMessage::noteOn (1, 36, static_cast<juce::uint8> (127)), 0);
    midi.addEvent (juce::MidiMessage::noteOn (2, 36, static_cast<juce::uint8> (64)), 0);
    midi.addEvent (juce::MidiMessage::noteOff (2, 36), 64);
    restored.process (audio, midi, settings);
    if (! near (audio.getSample (0, 32), 0.902f)
        || ! near (audio.getSample (0, 127), 0.815f))
    {
        std::cerr << "Note-off released a voice from another MIDI channel\n";
        return 30;
    }

    settings[0].gated = false;
    restored.prepare (44100.0);
    midi.clear();
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

    settings = {};
    settings[0].voiceLimit = 1;
    restored.prepare (44100.0);
    midi.clear();
    midi.addEvent (juce::MidiMessage::noteOn (1, 36, static_cast<juce::uint8> (127)), 0);
    midi.addEvent (juce::MidiMessage::noteOn (1, 36, static_cast<juce::uint8> (127)), 64);
    restored.process (audio, midi, settings);
    if (! near (audio.getSample (0, 32), 0.6f)
        || ! near (audio.getSample (0, 64), 1.2f))
    {
        std::cerr << "Mono retrigger did not fade the replaced voice\n";
        return 31;
    }
    midi.clear();
    restored.process (audio, midi, settings);
    restored.process (audio, midi, settings);
    if (! near (audio.getSample (0, 127), 0.6f))
    {
        std::cerr << "Mono retrigger exceeded its sustained voice limit\n";
        return 32;
    }

    settings[0].voiceLimit = 4;
    settings[0].velocityCurve = 0;
    restored.prepare (44100.0);
    midi.addEvent (juce::MidiMessage::noteOn (1, 36, static_cast<juce::uint8> (64)), 0);
    restored.process (audio, midi, settings);
    if (! near (audio.getSample (0, 0), 0.6f * std::sqrt (64.0f / 127.0f)))
    {
        std::cerr << "Soft velocity curve failed\n";
        return 33;
    }
    settings[0].velocityCurve = 2;
    restored.prepare (44100.0);
    restored.process (audio, midi, settings);
    if (! near (audio.getSample (0, 0), 0.6f * (64.0f / 127.0f) * (64.0f / 127.0f)))
    {
        std::cerr << "Hard velocity curve failed\n";
        return 34;
    }

    settings = {};
    settings[0].gated = true;
    restored.prepare (44100.0);
    midi.clear();
    midi.addEvent (juce::MidiMessage::noteOn (1, 36, static_cast<juce::uint8> (127)), 0);
    midi.addEvent (juce::MidiMessage::noteOff (1, 36), 0);
    restored.process (audio, midi, settings);
    midi.clear();
    restored.process (audio, midi, settings);
    restored.process (audio, midi, settings);
    if (! near (audio.getSample (0, 127), 0.0f))
    {
        std::cerr << "Equal-time note-on/note-off ordering failed\n";
        return 35;
    }

    settings = {};
    settings[0].chokeGroup = 1;
    settings[2].chokeGroup = 1;
    restored.prepare (44100.0);
    midi.addEvent (juce::MidiMessage::noteOn (1, 36, static_cast<juce::uint8> (127)), 0);
    midi.addEvent (juce::MidiMessage::noteOn (1, 38, static_cast<juce::uint8> (127)), 0);
    restored.process (audio, midi, settings);
    midi.clear();
    restored.process (audio, midi, settings);
    restored.process (audio, midi, settings);
    if (! near (audio.getSample (0, 127), 0.4f))
    {
        std::cerr << "Equal-time choke ordering failed\n";
        return 36;
    }

    settings = {};
    settings[0].gated = true;
    restored.prepare (44100.0);
    midi.addEvent (juce::MidiMessage::noteOn (1, 36, static_cast<juce::uint8> (127)), 0);
    restored.process (audio, midi, settings);
    restored.requestTransportStop();
    midi.clear();
    restored.process (audio, midi, settings);
    restored.process (audio, midi, settings);
    restored.process (audio, midi, settings);
    if (! near (audio.getSample (0, 127), 0.0f))
    {
        std::cerr << "Transport-stop release left a gated voice playing\n";
        return 37;
    }

    restored.requestLoadData (3, "dragged.wav", std::move (droppedBytes));
    if (! waitForReady (restored, 3)
        || restored.getSnapshot (3).getProperty ("sampleName", {}).toString() != "dragged.wav")
    {
        std::cerr << "Memory-backed dropped sample import failed\n";
        return 38;
    }
    restored.prepare (44100.0);
    midi.clear();
    midi.addEvent (juce::MidiMessage::noteOn (1, 39, static_cast<juce::uint8> (127)), 0);
    restored.process (audio, midi);
    if (! near (audio.getSample (0, 0), 0.2f))
    {
        std::cerr << "Memory-backed dropped sample did not play\n";
        return 39;
    }
    juce::MemoryBlock invalidDrop;
    invalidDrop.append ("bad", 3);
    restored.requestLoadData (3, "invalid.wav", std::move (invalidDrop));
    if (! waitForReady (restored, 3)
        || restored.getSnapshot (3).getProperty ("sampleName", {}).toString() != "dragged.wav")
    {
        std::cerr << "Invalid dropped sample replaced the playable pad\n";
        return 40;
    }

    restored.pauseNextDecodeForTest();
    restored.requestLoadData (3, "cancelled.wav", std::move (replacementBytes));
    bool decodeStarted = false;
    for (int attempt = 0; attempt < 200; ++attempt)
    {
        if (restored.decodeStartedForTest())
        {
            decodeStarted = true;
            break;
        }
        std::this_thread::sleep_for (std::chrono::milliseconds (10));
    }
    if (! decodeStarted)
    {
        restored.resumeDecodeForTest();
        std::cerr << "Cancellation test did not reach the decode boundary\n";
        return 42;
    }
    restored.cancelLoad (3);
    restored.resumeDecodeForTest();
    std::this_thread::sleep_for (std::chrono::milliseconds (50));
    if (restored.getSnapshot (3).getProperty ("status", {}).toString() != "ready"
        || restored.getSnapshot (3).getProperty ("sampleName", {}).toString() != "dragged.wav")
    {
        std::cerr << "Cancelled import replaced the playable pad\n";
        return 43;
    }

    settings = {};
    restored.prepare (44100.0);
    midi.clear();
    midi.addEvent (juce::MidiMessage::noteOn (1, 36, static_cast<juce::uint8> (127)), 0);
    restored.process (audio, midi, settings);
    midi.clear();
    for (int block = 1; block < 34; ++block)
        restored.process (audio, midi, settings);
    restored.process (audio, midi, settings);
    if (audio.getSample (0, 0) < 0.1f || audio.getSample (0, 0) > 0.2f
        || audio.getSample (0, 56) > 0.01f
        || ! near (audio.getSample (0, 100), 0.0f))
    {
        std::cerr << "One-shot sample end did not fade to silence\n";
        return 44;
    }

    settings[0].voiceLimit = 1;
    restored.prepare (44100.0);
    midi.clear();
    for (int trigger = 0; trigger < 65; ++trigger)
        midi.addEvent (juce::MidiMessage::noteOn (1, 36, static_cast<juce::uint8> (127)), 0);
    midi.addEvent (juce::MidiMessage::noteOn (1, 36, static_cast<juce::uint8> (64)), 0);
    restored.process (audio, midi, settings);
    if (! near (audio.getSample (0, 0), 39.0f))
    {
        std::cerr << "Full fade-tail pool cut off an active voice\n";
        return 45;
    }
    midi.clear();
    for (int block = 0; block < 3; ++block)
        restored.process (audio, midi, settings);
    if (! near (audio.getSample (0, 127), 0.0f))
    {
        std::cerr << "Tail-pool overflow did not release the old voice\n";
        return 46;
    }

    const auto ramp = folder.getChildFile ("hyperreal-phase4-ramp.wav");
    writeRampWav (ramp);
    SampleEngine edited;
    edited.prepare (44100.0);
    edited.requestLoad (0, ramp);
    if (! waitForReady (edited, 0)
        || ! edited.setPadEdit (0, { 10, 20, 2, 2, false })
        || edited.setPadEdit (0, { 20, 10, 0, 0, false }))
        return 47;
    midi.clear();
    midi.addEvent (juce::MidiMessage::noteOn (1, 36, static_cast<juce::uint8> (127)), 0);
    edited.process (audio, midi);
    if (! near (audio.getSample (0, 0), 0.0f)
        || std::abs (audio.getSample (0, 1) - 1200.0f / 32768.0f * 0.5f) > 0.002f
        || std::abs (audio.getSample (0, 8) - 1900.0f / 32768.0f * 0.5f) > 0.002f
        || ! near (audio.getSample (0, 9), 0.0f)
        || ! near (audio.getSample (0, 10), 0.0f))
    {
        std::cerr << "Trimmed playback fades or boundary failed\n";
        return 48;
    }
    if (! edited.setPadEdit (0, { 10, 20, 0, 0, true }))
        return 49;
    edited.prepare (44100.0);
    edited.process (audio, midi);
    if (std::abs (audio.getSample (0, 0) - 2000.0f / 32768.0f) > 0.002f
        || std::abs (audio.getSample (0, 9) - 1100.0f / 32768.0f) > 0.002f
        || ! near (audio.getSample (0, 10), 0.0f))
    {
        std::cerr << "Reverse playback escaped selected region\n";
        return 50;
    }
    const auto waveform = edited.getWaveform (0, 0);
    if (! waveform.isObject()
        || waveform.getProperty ("minimum", {}).getArray() == nullptr
        || waveform.getProperty ("maximum", {}).getArray() == nullptr
        || waveform.getProperty ("minimum", {}).getArray()->size() != 128)
    {
        std::cerr << "Waveform summary was not generated\n";
        return 52;
    }
    if (! edited.setPadEdit (0, { 10, 20, 0, 0, false }))
        return 53;
    SampleEngine::PlaybackSettings tuning {};
    tuning[0].coarseSemitones = 12;
    edited.prepare (44100.0);
    edited.process (audio, midi, tuning);
    if (std::abs (audio.getSample (0, 1) - 1300.0f / 32768.0f) > 0.002f
        || ! near (audio.getSample (0, 5), 0.0f))
    {
        std::cerr << "Octave-up resampling or duration failed\n";
        return 54;
    }
    tuning[0].coarseSemitones = -12;
    edited.prepare (44100.0);
    edited.process (audio, midi, tuning);
    if (std::abs (audio.getSample (0, 2) - 1200.0f / 32768.0f) > 0.002f
        || ! near (audio.getSample (0, 20), 0.0f))
    {
        std::cerr << "Octave-down resampling or duration failed\n";
        return 55;
    }
    tuning[0].coarseSemitones = 0;
    edited.prepare (48000.0);
    edited.process (audio, midi, tuning);
    if (std::abs (audio.getSample (0, 1) - 1191.875f / 32768.0f) > 0.002f)
    {
        std::cerr << "Sample-rate resampling failed\n";
        return 56;
    }
    if (! edited.setPadEdit (0, { 10, 20, 0, 0, true }))
        return 57;
    const auto revision = static_cast<juce::int64> (
        edited.getSnapshot().getProperty ("sampleRevision", -1));
    if (revision < 0
        || ! edited.setPadEdit (0, { 10, 20, 0, 0, true }, revision)
        || edited.setPadEdit (0, { 10, 15, 0, 0, false }, revision + 1))
    {
        std::cerr << "Live edit revision guard failed\n";
        return 67;
    }
    juce::AudioBuffer<float> cursorAudio (2, 1);
    edited.prepare (44100.0);
    edited.process (cursorAudio, midi);
    const auto cursorState = edited.getSnapshot();
    if (static_cast<int> (cursorState.getProperty ("playbackFrame", -1)) != 18
        || std::abs (static_cast<float> (cursorState.getProperty ("playbackSpeed", 0.0f))
                     + 44100.0f) > 1.0f)
    {
        std::cerr << "Reverse cursor speed or position failed\n";
        return 68;
    }
    juce::MemoryBlock editedState;
    {
        juce::MemoryOutputStream output (editedState, false);
        edited.writeSampleState (output);
    }
    juce::MemoryInputStream editedInput (editedState.getData(), editedState.getSize(), false);
    SampleEngine recalledEdit;
    recalledEdit.prepare (44100.0);
    if (! recalledEdit.restoreSampleState (editedInput)
        || static_cast<int> (recalledEdit.getSnapshot().getProperty ("trimStart", -1)) != 10
        || static_cast<int> (recalledEdit.getSnapshot().getProperty ("trimEnd", -1)) != 20
        || ! static_cast<bool> (recalledEdit.getSnapshot().getProperty ("reverse", false)))
    {
        std::cerr << "Phase 4 edit state did not round-trip\n";
        return 51;
    }
    const auto excessiveRate = folder.getChildFile ("hyperreal-phase4-rate-500k.wav");
    writeRampWav (excessiveRate, 500000);
    edited.requestLoad (0, excessiveRate);
    bool rejectedRate = false;
    for (int attempt = 0; attempt < 300; ++attempt)
    {
        const auto snapshot = edited.getSnapshot();
        if (snapshot.getProperty ("message", {}).toString().contains ("unsupported"))
        {
            rejectedRate = snapshot.getProperty ("status", {}).toString() == "ready"
                && static_cast<int> (snapshot.getProperty ("sourceFrames", -1)) == 256;
            break;
        }
        std::this_thread::sleep_for (std::chrono::milliseconds (10));
    }
    excessiveRate.deleteFile();
    if (! rejectedRate)
    {
        std::cerr << "Unsupported source rate replaced an active sample\n";
        return 58;
    }
    const auto fastSource = folder.getChildFile ("hyperreal-phase4-rate-96k.wav");
    writeRampWav (fastSource, 96000);
    SampleEngine rateEngine;
    rateEngine.requestLoad (0, fastSource);
    if (! waitForReady (rateEngine, 0))
        return 59;
    juce::AudioBuffer<float> edgeAudio (2, 128);
    juce::MidiBuffer edgeMidi;
    edgeMidi.addEvent (juce::MidiMessage::noteOn (1, 36,
                       static_cast<juce::uint8> (127)), 0);
    SampleEngine::PlaybackSettings extremeTune {};
    for (const double hostRate : { 44100.0, 48000.0, 96000.0 })
        for (const int coarse : { -24, 24 })
            for (const float fine : { -100.0f, 100.0f })
                for (const bool reverse : { false, true })
                {
                    rateEngine.prepare (hostRate);
                    extremeTune[0].coarseSemitones = coarse;
                    extremeTune[0].fineCents = fine;
                    if (! rateEngine.setPadEdit (0, { 10, 20, 0, 0, reverse }))
                        return 60;
                    rateEngine.process (edgeAudio, edgeMidi, extremeTune);
                    const auto ratio = std::pow (2.0, (coarse + fine / 100.0) / 12.0);
                    const auto expectedFrames = static_cast<int> (std::ceil (
                        10.0 / (96000.0 / hostRate * ratio)));
                    const auto firstValue = (reverse ? 2000.0f : 1100.0f) / 32768.0f;
                    if (expectedFrames < 1 || expectedFrames >= edgeAudio.getNumSamples()
                        || std::abs (edgeAudio.getSample (0, 0) - firstValue) > 0.002f
                        || edgeAudio.getSample (0, expectedFrames - 1) <= 0.0f)
                    {
                        std::cerr << "Extreme tuning start or duration failed\n";
                        return 61;
                    }
                    for (int frame = expectedFrames; frame < edgeAudio.getNumSamples(); ++frame)
                        if (! std::isfinite (edgeAudio.getSample (0, frame))
                            || std::abs (edgeAudio.getSample (0, frame)) > 1.0e-6f)
                        {
                            std::cerr << "Extreme tuning escaped selected region\n";
                            return 62;
                        }
                }
    rateEngine.prepare (44100.0);
    if (! rateEngine.setPadEdit (0, { 10, 11, 0, 0, true }))
        return 63;
    extremeTune[0] = {};
    rateEngine.process (edgeAudio, edgeMidi, extremeTune);
    if (std::abs (edgeAudio.getSample (0, 0) - 1100.0f / 32768.0f) > 0.002f
        || ! near (edgeAudio.getSample (0, 1), 0.0f))
    {
        std::cerr << "One-frame reverse region failed\n";
        return 64;
    }
    if (! rateEngine.setPadEdit (0, { 10, 11, 100, 100, false })
        || static_cast<int> (rateEngine.getSnapshot().getProperty ("fadeInFrames", -1)) != 1
        || static_cast<int> (rateEngine.getSnapshot().getProperty ("fadeOutFrames", -1)) != 1)
    {
        std::cerr << "Overlong fades were not clamped to a short region\n";
        return 65;
    }
    rateEngine.prepare (44100.0);
    rateEngine.process (edgeAudio, edgeMidi, extremeTune);
    if (std::abs (edgeAudio.getSample (0, 0)) > 1.0e-6f
        || std::abs (edgeAudio.getSample (0, 1)) > 1.0e-6f)
    {
        std::cerr << "One-frame fades did not end at silence\n";
        return 66;
    }
    fastSource.deleteFile();
    ramp.deleteFile();

    SampleEngine envelopeEngine;
    envelopeEngine.prepare (1000.0);
    const auto envelopeSource = folder.getChildFile ("hyperreal-phase5-envelope.wav");
    writeWav (envelopeSource, 6553);
    envelopeEngine.requestLoad (0, envelopeSource);
    if (! waitForReady (envelopeEngine, 0)
        || ! envelopeEngine.setPadEdit (0, { 0, 4410, 0, 0, false }))
        return 67;
    envelopeSource.deleteFile();
    SampleEngine::PlaybackSettings envelopeSettings {};
    envelopeSettings[0].amplitude = { 0.01f, 0.01f, 0.5f, 0.02f };
    juce::AudioBuffer<float> envelopeAudio (2, 128);
    juce::MidiBuffer envelopeMidi;
    envelopeMidi.addEvent (juce::MidiMessage::noteOn (1, 36,
                           static_cast<juce::uint8> (127)), 0);
    envelopeEngine.process (envelopeAudio, envelopeMidi, envelopeSettings);
    if (! near (envelopeAudio.getSample (0, 0), 0.02f)
        || ! near (envelopeAudio.getSample (0, 9), 0.2f)
        || ! near (envelopeAudio.getSample (0, 19), 0.1f)
        || ! near (envelopeAudio.getSample (0, 40), 0.1f)
        || envelopeAudio.getSample (0, 90) >= 0.08f
        || ! near (envelopeAudio.getSample (0, 110), 0.0f))
    {
        std::cerr << "One-shot amplitude ADSR stages failed\n";
        return 68;
    }
    envelopeEngine.prepare (1000.0);
    envelopeSettings[0].amplitude.releaseSeconds = 0.2f;
    envelopeEngine.process (envelopeAudio, envelopeMidi, envelopeSettings);
    if (envelopeAudio.getSample (0, 95) >= 0.02f
        || ! near (envelopeAudio.getSample (0, 110), 0.0f))
    {
        std::cerr << "Long release exceeded a short one-shot sample\n";
        return 70;
    }
    envelopeEngine.prepare (1000.0);
    envelopeSettings[0].gated = true;
    envelopeSettings[0].amplitude.releaseSeconds = 0.02f;
    envelopeMidi.addEvent (juce::MidiMessage::noteOff (1, 36), 40);
    envelopeEngine.process (envelopeAudio, envelopeMidi, envelopeSettings);
    if (! near (envelopeAudio.getSample (0, 39), 0.1f)
        || envelopeAudio.getSample (0, 50) >= 0.08f
        || ! near (envelopeAudio.getSample (0, 70), 0.0f))
    {
        std::cerr << "Gated amplitude ADSR release failed\n";
        return 69;
    }

    SampleEngine filterEngine;
    filterEngine.prepare (1000.0);
    const auto filterSource = folder.getChildFile ("hyperreal-phase5-filter.wav");
    writeWav (filterSource, 6553);
    filterEngine.requestLoad (0, filterSource);
    if (! waitForReady (filterEngine, 0)
        || ! filterEngine.setPadEdit (0, { 0, 4410, 0, 0, false }))
        return 71;
    filterSource.deleteFile();
    SampleEngine::PlaybackSettings filterSettings {};
    filterSettings[0].filter.cutoffHz = 100.0f;
    filterSettings[0].filter.resonance = 0.707f;
    juce::AudioBuffer<float> filterAudio (2, 128);
    juce::MidiBuffer filterMidi;
    filterMidi.addEvent (juce::MidiMessage::noteOn (1, 36,
                         static_cast<juce::uint8> (127)), 0);
    filterSettings[0].filter.mode = 1;
    filterEngine.process (filterAudio, filterMidi, filterSettings);
    const auto lowPassDc = filterAudio.getSample (0, 60);
    filterEngine.prepare (1000.0);
    filterSettings[0].filter.mode = 2;
    filterEngine.process (filterAudio, filterMidi, filterSettings);
    const auto highPassDc = filterAudio.getSample (0, 60);
    filterEngine.prepare (1000.0);
    filterSettings[0].filter.mode = 3;
    filterEngine.process (filterAudio, filterMidi, filterSettings);
    const auto bandPassDc = filterAudio.getSample (0, 60);
    if (! near (lowPassDc, 0.2f) || std::abs (highPassDc) > 0.02f
        || std::abs (bandPassDc) > 0.02f)
    {
        std::cerr << "Filter modes did not produce expected DC response: "
                  << lowPassDc << ' ' << highPassDc << ' ' << bandPassDc << "\n";
        return 72;
    }
    const auto toneSource = folder.getChildFile ("hyperreal-phase5-filter-tone.wav");
    writeToneWav (toneSource);
    filterEngine.requestLoad (0, toneSource);
    if (! waitForReady (filterEngine, 0)
        || ! filterEngine.setPadEdit (0, { 0, 4410, 0, 0, false }))
        return 73;
    toneSource.deleteFile();
    filterSettings[0].filter.mode = 1;
    filterSettings[0].filter.cutoffHz = 50.0f;
    filterSettings[0].filter.envelopeDepthSemitones = 0.0f;
    filterEngine.prepare (1000.0);
    filterEngine.process (filterAudio, filterMidi, filterSettings);
    const auto quietRms = filterAudio.getRMSLevel (0, 30, 50);
    filterSettings[0].filter.envelopeDepthSemitones = 24.0f;
    filterEngine.prepare (1000.0);
    filterEngine.process (filterAudio, filterMidi, filterSettings);
    const auto openRms = filterAudio.getRMSLevel (0, 30, 50);
    if (openRms < quietRms * 2.0f)
    {
        std::cerr << "Bipolar filter-envelope depth did not open the filter: "
                  << quietRms << ' ' << openRms << "\n";
        return 74;
    }
    filterSettings[0].filter.envelopeDepthSemitones = -24.0f;
    filterEngine.prepare (1000.0);
    filterEngine.process (filterAudio, filterMidi, filterSettings);
    const auto closedRms = filterAudio.getRMSLevel (0, 30, 50);
    if (closedRms >= quietRms * 0.8f)
    {
        std::cerr << "Negative filter-envelope depth did not close the filter: "
                  << quietRms << ' ' << closedRms << "\n";
        return 77;
    }
    filterSettings[0].filter.envelopeDepthSemitones = 24.0f;
    filterSettings[0].filter.envelope.attackSeconds = 0.05f;
    filterEngine.prepare (1000.0);
    filterEngine.process (filterAudio, filterMidi, filterSettings);
    const auto earlyRms = filterAudio.getRMSLevel (0, 15, 15);
    const auto lateRms = filterAudio.getRMSLevel (0, 65, 15);
    if (lateRms <= earlyRms * 1.4f)
    {
        std::cerr << "Filter-envelope attack did not move cutoff: "
                  << earlyRms << ' ' << lateRms << "\n";
        return 75;
    }
    filterSettings[0].filter.envelope.attackSeconds = 0.0f;
    filterSettings[0].filter.envelopeDepthSemitones = 0.0f;
    filterEngine.prepare (1000.0);
    filterEngine.process (filterAudio, filterMidi, filterSettings);
    const auto singleVoice = filterAudio.getSample (0, 20);
    filterEngine.prepare (1000.0);
    filterMidi.addEvent (juce::MidiMessage::noteOn (1, 36,
                         static_cast<juce::uint8> (127)), 0);
    filterEngine.process (filterAudio, filterMidi, filterSettings);
    if (std::abs (filterAudio.getSample (0, 20) - singleVoice * 2.0f) > 0.01f)
    {
        std::cerr << "Overlapping voices shared filter state\n";
        return 78;
    }
    filterMidi.clear();
    filterMidi.addEvent (juce::MidiMessage::noteOn (1, 36,
                         static_cast<juce::uint8> (127)), 0);
    filterSettings[0].filter.resonance = 10.0f;
    filterSettings[0].filter.cutoffHz = 450.0f;
    filterSettings[0].filter.envelopeDepthSemitones = 48.0f;
    filterEngine.prepare (1000.0);
    filterEngine.process (filterAudio, filterMidi, filterSettings);
    for (int frame = 0; frame < filterAudio.getNumSamples(); ++frame)
        if (! std::isfinite (filterAudio.getSample (0, frame))
            || std::abs (filterAudio.getSample (0, frame)) > 8.0f)
        {
            std::cerr << "Maximum-resonance filter produced unstable output\n";
            return 76;
        }

    std::cout << "SampleEngine multi-pad smoke passed\n";
    return 0;
}
