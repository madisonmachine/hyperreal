#include "SampleEngine.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>

namespace
{
constexpr int maxChannels = 2;
constexpr int maxVoicesPerPad = 4;
constexpr double maxDurationSeconds = 60.0;
constexpr juce::int64 maxInputBytes = 128 * 1024 * 1024;
constexpr juce::int64 maxDecodedSampleBytes = 128 * 1024 * 1024;
constexpr juce::int64 maxKitBytes = 512 * 1024 * 1024;

juce::int64 sampleBytes (const juce::AudioBuffer<float>& audio)
{
    return static_cast<juce::int64> (audio.getNumChannels())
           * audio.getNumSamples() * static_cast<int> (sizeof (float));
}
}

SampleEngine::SampleEngine()
{
    for (int pad = 0; pad < padCount; ++pad)
    {
        activeSamples[pad].store (nullptr);
        auditionCounters[pad].store (0);
        status[pad] = "empty";
        message[pad] = "Load WAV, AIFF or FLAC to begin.";
    }
    for (auto& pointer : voicePointers)
        pointer.store (nullptr);
    worker = std::thread ([this] { workerLoop(); });
}

SampleEngine::~SampleEngine()
{
    {
        std::lock_guard lock (controlMutex);
        stopping = true;
    }
    controlChanged.notify_one();
    if (worker.joinable())
        worker.join();
}

void SampleEngine::prepare (double hostSampleRate) noexcept
{
    hostRate = hostSampleRate > 0.0 ? hostSampleRate : 44100.0;
    for (int pad = 0; pad < padCount; ++pad)
    {
        auditionSeen[pad] = auditionCounters[pad].load();
        audiblePads[pad] = true;
    }
    for (int voiceIndex = 0; voiceIndex < maxVoices; ++voiceIndex)
    {
        voices[voiceIndex] = {};
        voicePointers[voiceIndex].store (nullptr);
    }
    nextSequence = 1;
}

void SampleEngine::requestLoad (int pad, juce::File file)
{
    if (pad < 0 || pad >= padCount)
        return;
    {
        std::lock_guard lock (controlMutex);
        pendingFiles[pad] = std::move (file);
        pendingLoads[pad] = true; // Latest request wins for this pad only.
        ++loadGenerations[pad];
        status[pad] = "loading";
        message[pad] = "Decoding sample on a worker thread.";
    }
    controlChanged.notify_one();
}

void SampleEngine::cancelLoad (int pad)
{
    if (pad < 0 || pad >= padCount)
        return;
    std::lock_guard lock (controlMutex);
    pendingLoads[pad] = false;
    ++loadGenerations[pad]; // Invalidates any decode already in flight.
    status[pad] = current[pad] != nullptr ? "ready" : "empty";
    message[pad] = "Sample load cancelled.";
}

void SampleEngine::requestAudition (int pad) noexcept
{
    if (pad >= 0 && pad < padCount)
        auditionCounters[pad].fetch_add (1);
}

void SampleEngine::beginRelease (Voice& playing) noexcept
{
    if (playing.sample == nullptr || playing.releaseRemaining > 0)
        return;
    playing.noteHeld = false;
    playing.releaseTotal = std::max (1, static_cast<int> (hostRate * 0.005));
    playing.releaseRemaining = playing.releaseTotal;
}

void SampleEngine::noteOff (int pad) noexcept
{
    Voice* oldestHeld = nullptr;
    for (auto& playing : voices)
        if (playing.sample != nullptr && playing.pad == pad
            && playing.gated && playing.noteHeld
            && (oldestHeld == nullptr || playing.sequence < oldestHeld->sequence))
            oldestHeld = &playing;
    if (oldestHeld != nullptr)
        beginRelease (*oldestHeld);
}

void SampleEngine::releaseAll() noexcept
{
    for (auto& playing : voices)
        beginRelease (playing);
}

void SampleEngine::startVoice (int pad, float velocity, bool audition,
                               const PlaybackSettings& settings) noexcept
{
    if (! audiblePads[pad])
        return;
    const auto* sample = activeSamples[pad].load();
    if (sample == nullptr)
        return;

    const auto group = std::clamp (settings[pad].chokeGroup, 0, 4);
    if (group != 0)
        for (auto& playing : voices)
            if (playing.sample != nullptr && playing.chokeGroup == group)
                beginRelease (playing);

    int freeVoice = -1;
    int oldestPadVoice = -1;
    int oldestVoice = -1;
    int activePadVoices = 0;
    for (int index = 0; index < maxVoices; ++index)
    {
        const auto& candidate = voices[index];
        if (candidate.sample == nullptr)
        {
            if (freeVoice < 0)
                freeVoice = index;
            continue;
        }
        if (oldestVoice < 0 || candidate.sequence < voices[oldestVoice].sequence)
            oldestVoice = index;
        if (candidate.pad == pad)
        {
            ++activePadVoices;
            if (oldestPadVoice < 0 || candidate.sequence < voices[oldestPadVoice].sequence)
                oldestPadVoice = index;
        }
    }

    const auto chosen = activePadVoices >= maxVoicesPerPad ? oldestPadVoice
                       : freeVoice >= 0 ? freeVoice : oldestVoice;
    if (chosen < 0)
        return;

    const auto gated = settings[pad].gated && ! audition;
    voices[chosen] = { sample, 0.0, std::clamp (velocity, 0.0f, 1.0f),
                       pad, nextSequence++, gated, gated, group, 0, 0 };
    voicePointers[chosen].store (sample);
}

void SampleEngine::renderRange (juce::AudioBuffer<float>& output, int begin, int end,
                                const PlaybackSettings& settings) noexcept
{
    if (output.getNumChannels() < 2)
        return;
    auto* left = output.getWritePointer (0);
    auto* right = output.getWritePointer (1);

    for (int voiceIndex = 0; voiceIndex < maxVoices; ++voiceIndex)
    {
        auto& playing = voices[voiceIndex];
        if (playing.sample == nullptr)
            continue;
        const auto* source = playing.sample;
        const auto frames = source->audio.getNumSamples();
        const auto* sourceLeft = source->audio.getReadPointer (0);
        const auto* sourceRight = source->audio.getReadPointer (
            source->audio.getNumChannels() > 1 ? 1 : 0);
        const auto step = source->sampleRate / hostRate;
        const auto level = std::clamp (settings[playing.pad].level, 0.0f, 1.0f);

        for (int frame = begin; frame < end; ++frame)
        {
            const auto index = static_cast<int> (playing.position);
            if (index >= frames)
            {
                playing.sample = nullptr;
                voicePointers[voiceIndex].store (nullptr);
                break;
            }
            const auto next = std::min (index + 1, frames - 1);
            const auto fraction = static_cast<float> (playing.position - index);
            const auto releaseGain = playing.releaseRemaining > 0
                ? static_cast<float> (playing.releaseRemaining) / playing.releaseTotal : 1.0f;
            left[frame] += (sourceLeft[index] + (sourceLeft[next] - sourceLeft[index]) * fraction)
                           * playing.gain * level * releaseGain;
            right[frame] += (sourceRight[index] + (sourceRight[next] - sourceRight[index]) * fraction)
                            * playing.gain * level * releaseGain;
            playing.position += step;
            if (playing.releaseRemaining > 0 && --playing.releaseRemaining == 0)
            {
                playing.sample = nullptr;
                voicePointers[voiceIndex].store (nullptr);
                break;
            }
        }
    }
}

void SampleEngine::process (juce::AudioBuffer<float>& output, const juce::MidiBuffer& midi) noexcept
{
    static const PlaybackSettings defaults {};
    process (output, midi, defaults);
}

void SampleEngine::process (juce::AudioBuffer<float>& output, const juce::MidiBuffer& midi,
                            const PlaybackSettings& settings) noexcept
{
    callbackActive.store (true);
    output.clear();

    const auto anySolo = std::any_of (settings.begin(), settings.end(),
                                     [] (const auto& pad) { return pad.solo; });
    for (int pad = 0; pad < padCount; ++pad)
    {
        const auto audible = ! settings[pad].muted && (! anySolo || settings[pad].solo);
        if (! audible && audiblePads[pad])
            for (auto& playing : voices)
                if (playing.sample != nullptr && playing.pad == pad)
                    beginRelease (playing);
        audiblePads[pad] = audible;
    }

    for (int pad = 0; pad < padCount; ++pad)
    {
        const auto now = auditionCounters[pad].load();
        if (now != auditionSeen[pad])
        {
            auditionSeen[pad] = now; // Coalesce multiple UI presses within one block.
            startVoice (pad, 1.0f, true, settings);
        }
    }

    int cursor = 0;
    for (const auto event : midi)
    {
        const auto eventFrame = std::clamp (event.samplePosition, cursor, output.getNumSamples());
        renderRange (output, cursor, eventFrame, settings);
        cursor = eventFrame;

        const auto& midiMessage = event.getMessage();
        if (midiMessage.isAllNotesOff() || midiMessage.isAllSoundOff())
        {
            releaseAll();
            continue;
        }
        if (midiMessage.isNoteOff (true))
        {
            const auto pad = midiMessage.getNoteNumber() - firstMidiNote;
            if (pad >= 0 && pad < padCount)
                noteOff (pad);
            continue;
        }
        if (midiMessage.isNoteOn() && midiMessage.getVelocity() > 0.0f)
        {
            const auto pad = midiMessage.getNoteNumber() - firstMidiNote;
            if (pad >= 0 && pad < padCount)
                startVoice (pad, midiMessage.getFloatVelocity(), false, settings);
        }
    }

    renderRange (output, cursor, output.getNumSamples(), settings);
    callbackActive.store (false);
}

juce::var SampleEngine::getSnapshot (int selectedPad) const
{
    selectedPad = std::clamp (selectedPad, 0, padCount - 1);
    std::lock_guard lock (controlMutex);
    auto* object = new juce::DynamicObject();
    object->setProperty ("schemaVersion", 2);
    object->setProperty ("selectedPad", selectedPad);
    object->setProperty ("status", status[selectedPad]);
    object->setProperty ("message", message[selectedPad]);
    object->setProperty ("sampleName", current[selectedPad] != nullptr
                                           ? current[selectedPad]->name : juce::String());
    object->setProperty ("sampleRate", current[selectedPad] != nullptr
                                           ? current[selectedPad]->sampleRate : 0.0);
    object->setProperty ("durationSeconds", current[selectedPad] != nullptr
        ? current[selectedPad]->audio.getNumSamples() / current[selectedPad]->sampleRate : 0.0);
    juce::Array<juce::var> pads;
    for (int pad = 0; pad < padCount; ++pad)
    {
        auto* item = new juce::DynamicObject();
        item->setProperty ("index", pad);
        item->setProperty ("midiNote", firstMidiNote + pad);
        item->setProperty ("status", status[pad]);
        item->setProperty ("sampleName", current[pad] != nullptr ? current[pad]->name : juce::String());
        pads.add (juce::var (item));
    }
    object->setProperty ("pads", juce::var (pads));
    return juce::var (object);
}

void SampleEngine::publishSample (int pad, std::unique_ptr<Sample> next)
{
    // controlMutex is held. Audio reads immutable samples through atomics.
    activeSamples[pad].store (next.get());
    if (current[pad] != nullptr)
        retired.push_back (std::move (current[pad]));
    current[pad] = std::move (next);
}

void SampleEngine::reapRetired()
{
    if (callbackActive.load())
        return;
    std::erase_if (retired, [this] (const auto& item)
    {
        for (const auto& pointer : voicePointers)
            if (pointer.load() == item.get())
                return false;
        return true;
    });
}

void SampleEngine::workerLoop()
{
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();

    for (;;)
    {
        juce::File file;
        int pad = -1;
        juce::uint64 generation = 0;
        {
            std::unique_lock lock (controlMutex);
            controlChanged.wait_for (lock, std::chrono::milliseconds (250),
                                     [this]
                                     {
                                         return stopping || std::any_of (pendingLoads.begin(),
                                                                         pendingLoads.end(),
                                                                         [] (bool pending) { return pending; });
                                     });
            if (stopping)
                break;
            for (int offset = 1; offset <= padCount; ++offset)
            {
                const auto candidate = (lastDequeuedPad + offset) % padCount;
                if (pendingLoads[candidate])
                {
                    pad = candidate;
                    lastDequeuedPad = pad;
                    break;
                }
            }
            if (pad < 0)
            {
                reapRetired();
                continue;
            }
            file = pendingFiles[pad];
            generation = loadGenerations[pad];
            pendingLoads[pad] = false;
        }

        juce::String error;
        std::unique_ptr<Sample> decoded;
        if (! file.existsAsFile() || file.getSize() > maxInputBytes
            || ! file.hasFileExtension (".wav;.aif;.aiff;.flac"))
        {
            error = "Choose a WAV, AIFF or FLAC file under 128 MB.";
        }
        else if (auto reader = std::unique_ptr<juce::AudioFormatReader> (formats.createReaderFor (file)))
        {
            if (reader->numChannels < 1 || reader->numChannels > maxChannels
                || ! std::isfinite (reader->sampleRate) || reader->sampleRate <= 0.0
                || reader->lengthInSamples < 1
                || reader->lengthInSamples > maxDurationSeconds * reader->sampleRate
                || reader->lengthInSamples > std::numeric_limits<int>::max()
                || reader->lengthInSamples * static_cast<juce::int64> (reader->numChannels) * 4
                    > maxDecodedSampleBytes)
            {
                error = "Sample channels, rate, duration or decoded size are unsupported.";
            }
            else
            {
                decoded = std::make_unique<Sample>();
                const auto frames = static_cast<int> (reader->lengthInSamples);
                decoded->audio.setSize (static_cast<int> (reader->numChannels), frames);
                decoded->sampleRate = reader->sampleRate;
                decoded->name = file.getFileName();
                if (! reader->read (&decoded->audio, 0, frames, 0, true, true))
                {
                    decoded.reset();
                    error = "Could not decode the selected sample.";
                }
            }
        }
        else
        {
            error = "Could not open the selected sample.";
        }

        {
            std::lock_guard lock (controlMutex);
            if (generation != loadGenerations[pad])
            {
                reapRetired();
                continue;
            }
            if (decoded != nullptr)
            {
                auto bytes = sampleBytes (decoded->audio);
                for (int other = 0; other < padCount; ++other)
                    if (other != pad && current[other] != nullptr)
                        bytes += sampleBytes (current[other]->audio);
                if (bytes > maxKitBytes)
                {
                    decoded.reset();
                    error = "The 16-pad sample memory limit is 512 MB.";
                }
            }
            if (decoded != nullptr)
            {
                publishSample (pad, std::move (decoded));
                status[pad] = "ready";
                message[pad] = "MIDI note " + juce::String (firstMidiNote + pad)
                               + " or AUDITION plays this sample.";
            }
            else
            {
                status[pad] = current[pad] != nullptr ? "ready" : "error";
                message[pad] = error;
            }
            reapRetired();
        }
    }
}

void SampleEngine::writeSample (juce::OutputStream& output, const Sample* sample)
{
    output.writeByte (sample != nullptr ? 1 : 0);
    if (sample == nullptr)
        return;
    output.writeString (sample->name);
    output.writeDouble (sample->sampleRate);
    output.writeInt (sample->audio.getNumChannels());
    output.writeInt (sample->audio.getNumSamples());
    for (int channel = 0; channel < sample->audio.getNumChannels(); ++channel)
        for (int frame = 0; frame < sample->audio.getNumSamples(); ++frame)
            output.writeFloat (sample->audio.getSample (channel, frame));
}

void SampleEngine::writeSampleState (juce::OutputStream& output) const
{
    std::lock_guard lock (controlMutex);
    for (const auto& sample : current)
        writeSample (output, sample.get());
}

bool SampleEngine::readSample (juce::InputStream& input, std::unique_ptr<Sample>& sample,
                               juce::int64& decodedBytes)
{
    if (input.getNumBytesRemaining() < 1)
        return false;
    const auto present = input.readByte();
    if (present == 0)
    {
        sample.reset();
        decodedBytes = 0;
        return true;
    }
    if (present != 1)
        return false;

    auto restored = std::make_unique<Sample>();
    restored->name = input.readString();
    restored->sampleRate = input.readDouble();
    const auto channels = input.readInt();
    const auto frames = input.readInt();
    const auto bytes = static_cast<juce::int64> (channels) * frames * 4;
    if (restored->name.length() > 255
        || channels < 1 || channels > maxChannels || frames < 1
        || ! std::isfinite (restored->sampleRate)
        || restored->sampleRate <= 0.0 || restored->sampleRate > 384000.0
        || frames > maxDurationSeconds * restored->sampleRate
        || bytes > maxDecodedSampleBytes
        || input.getNumBytesRemaining() < bytes)
        return false;

    restored->audio.setSize (channels, frames);
    for (int channel = 0; channel < channels; ++channel)
        for (int frame = 0; frame < frames; ++frame)
        {
            const auto value = input.readFloat();
            if (! std::isfinite (value) || std::abs (value) > 100.0f)
                return false;
            restored->audio.setSample (channel, frame, value);
        }
    sample = std::move (restored);
    decodedBytes = bytes;
    return true;
}

bool SampleEngine::restoreSampleState (juce::InputStream& input)
{
    std::array<std::unique_ptr<Sample>, padCount> restored;
    juce::int64 totalBytes = 0;
    for (auto& sample : restored)
    {
        juce::int64 bytes = 0;
        if (! readSample (input, sample, bytes))
            return false;
        totalBytes += bytes;
        if (totalBytes > maxKitBytes)
            return false;
    }
    if (input.getNumBytesRemaining() != 0)
        return false;

    std::lock_guard lock (controlMutex);
    for (int pad = 0; pad < padCount; ++pad)
    {
        publishSample (pad, std::move (restored[pad]));
        status[pad] = current[pad] != nullptr ? "ready" : "empty";
        message[pad] = current[pad] != nullptr ? "Restored from the DAW project state."
                                              : "Load WAV, AIFF or FLAC to begin.";
    }
    reapRetired();
    return true;
}

bool SampleEngine::restorePhase2SampleState (juce::InputStream& input)
{
    std::unique_ptr<Sample> restored;
    juce::int64 bytes = 0;
    if (! readSample (input, restored, bytes) || input.getNumBytesRemaining() != 0)
        return false;
    std::lock_guard lock (controlMutex);
    publishSample (0, std::move (restored));
    status[0] = current[0] != nullptr ? "ready" : "empty";
    message[0] = current[0] != nullptr ? "Restored from a Phase 2 project."
                                      : "Load WAV, AIFF or FLAC to begin.";
    for (int pad = 1; pad < padCount; ++pad)
    {
        publishSample (pad, nullptr);
        status[pad] = "empty";
        message[pad] = "Load WAV, AIFF or FLAC to begin.";
    }
    reapRetired();
    return true;
}
