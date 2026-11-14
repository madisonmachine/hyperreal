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
        midiTriggerCounters[pad].store (0);
        playbackFrames[pad].store (-1);
        status[pad] = "empty";
        message[pad] = "Load WAV, AIFF or FLAC to begin.";
    }
    for (auto& pointer : voicePointers)
        pointer.store (nullptr);
    for (auto& pointer : tailPointers)
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
    transportStopRequested.store (false);
    for (int pad = 0; pad < padCount; ++pad)
    {
        auditionSeen[pad] = auditionCounters[pad].load();
        audiblePads[pad] = true;
        playbackFrames[pad].store (-1);
    }
    for (int voiceIndex = 0; voiceIndex < maxVoices; ++voiceIndex)
    {
        voices[voiceIndex] = {};
        voicePointers[voiceIndex].store (nullptr);
        fadeTails[voiceIndex] = {};
        tailPointers[voiceIndex].store (nullptr);
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
        pendingMemory[pad].reset();
        pendingNames[pad] = pendingFiles[pad].getFileName();
        pendingLoads[pad] = true; // Latest request wins for this pad only.
        ++loadGenerations[pad];
        status[pad] = "loading";
        message[pad] = "Decoding sample on a worker thread.";
    }
    controlChanged.notify_one();
}

void SampleEngine::requestLoadData (int pad, juce::String name, juce::MemoryBlock data)
{
    if (pad < 0 || pad >= padCount || data.isEmpty() || data.getSize() > maxInputBytes
        || ! juce::File (name).hasFileExtension (".wav;.aif;.aiff;.flac"))
        return;
    {
        std::lock_guard lock (controlMutex);
        pendingFiles[pad] = {};
        pendingNames[pad] = juce::File (name).getFileName();
        pendingMemory[pad] = std::make_unique<juce::MemoryBlock> (std::move (data));
        pendingLoads[pad] = true;
        ++loadGenerations[pad];
        status[pad] = "loading";
        message[pad] = "Decoding dropped sample on a worker thread.";
    }
    controlChanged.notify_one();
}

void SampleEngine::cancelLoad (int pad)
{
    if (pad < 0 || pad >= padCount)
        return;
    std::lock_guard lock (controlMutex);
    pendingLoads[pad] = false;
    pendingMemory[pad].reset();
    ++loadGenerations[pad]; // Invalidates any decode already in flight.
    status[pad] = current[pad] != nullptr ? "ready" : "empty";
    message[pad] = "Sample load cancelled.";
}

std::unique_ptr<SampleEngine::Sample> SampleEngine::cloneSample (const Sample* source)
{
    if (source == nullptr)
        return nullptr;
    return std::make_unique<Sample> (*source);
}

SampleEngine::Edit SampleEngine::defaultEdit (int frames, double sampleRate) noexcept
{
    return { 0, frames, 0,
             std::min (frames - 1, std::max (1, static_cast<int> (sampleRate * 0.005))), false };
}

void SampleEngine::buildWaveform (SampleData& data)
{
    constexpr std::array<int, 4> sizes { 128, 512, 2048, 8192 };
    const auto frames = data.audio.getNumSamples();
    for (int level = 0; level < static_cast<int> (sizes.size()); ++level)
    {
        auto& wave = data.waveform[level];
        wave.minimum.resize (sizes[level]);
        wave.maximum.resize (sizes[level]);
        for (int bucket = 0; bucket < sizes[level]; ++bucket)
        {
            const auto first = static_cast<int> (static_cast<juce::int64> (bucket) * frames
                                                  / sizes[level]);
            const auto last = std::max (first + 1, static_cast<int> (
                static_cast<juce::int64> (bucket + 1) * frames / sizes[level]));
            auto minimum = std::numeric_limits<float>::max();
            auto maximum = std::numeric_limits<float>::lowest();
            for (int channel = 0; channel < data.audio.getNumChannels(); ++channel)
                for (int frame = first; frame < last; ++frame)
                {
                    const auto value = data.audio.getSample (channel, frame);
                    minimum = std::min (minimum, value);
                    maximum = std::max (maximum, value);
                }
            wave.minimum[bucket] = minimum;
            wave.maximum[bucket] = maximum;
        }
    }
}

bool SampleEngine::copyPad (int pad)
{
    if (pad < 0 || pad >= padCount)
        return false;
    std::lock_guard lock (controlMutex);
    clipboard = cloneSample (current[pad].get());
    clipboardReady = true;
    message[pad] = "Pad copied. Select a destination and choose PASTE.";
    return true;
}

bool SampleEngine::pastePad (int pad)
{
    if (pad < 0 || pad >= padCount)
        return false;
    std::lock_guard lock (controlMutex);
    if (! clipboardReady)
        return false;
    auto bytes = clipboard != nullptr ? sampleBytes (clipboard->data->audio) : 0;
    for (int other = 0; other < padCount; ++other)
        if (other != pad && current[other] != nullptr)
            bytes += sampleBytes (current[other]->data->audio);
    if (bytes > maxKitBytes)
    {
        message[pad] = "Paste exceeds the 512 MB kit sample limit.";
        return false;
    }
    auto replacement = cloneSample (clipboard.get());
    pendingLoads[pad] = false;
    pendingMemory[pad].reset();
    ++loadGenerations[pad];
    publishSample (pad, std::move (replacement));
    status[pad] = current[pad] != nullptr ? "ready" : "empty";
    message[pad] = "Pad pasted. MIDI note remains attached to this slot.";
    reapRetired();
    return true;
}

bool SampleEngine::swapPads (int first, int second)
{
    if (first < 0 || first >= padCount || second < 0 || second >= padCount || first == second)
        return false;
    std::lock_guard lock (controlMutex);
    pendingLoads[first] = pendingLoads[second] = false;
    pendingMemory[first].reset();
    pendingMemory[second].reset();
    ++loadGenerations[first];
    ++loadGenerations[second];
    std::swap (current[first], current[second]);
    activeSamples[first].store (current[first].get());
    activeSamples[second].store (current[second].get());
    ++sampleRevisions[first];
    ++sampleRevisions[second];
    status[first] = current[first] != nullptr ? "ready" : "empty";
    status[second] = current[second] != nullptr ? "ready" : "empty";
    message[first] = message[second] = "Pad content swapped; MIDI notes stay with grid slots.";
    return true;
}

bool SampleEngine::clearPad (int pad)
{
    if (pad < 0 || pad >= padCount)
        return false;
    std::lock_guard lock (controlMutex);
    pendingLoads[pad] = false;
    pendingMemory[pad].reset();
    ++loadGenerations[pad];
    publishSample (pad, nullptr);
    status[pad] = "empty";
    message[pad] = "Pad cleared. Existing voices may finish playing.";
    reapRetired();
    return true;
}

bool SampleEngine::setPadEdit (int pad, Edit edit)
{
    if (pad < 0 || pad >= padCount)
        return false;
    std::lock_guard lock (controlMutex);
    if (current[pad] == nullptr)
        return false;
    const auto frames = current[pad]->data->audio.getNumSamples();
    edit.startFrame = std::clamp (edit.startFrame, 0, frames - 1);
    edit.endFrame = std::clamp (edit.endFrame, 1, frames);
    if (edit.startFrame >= edit.endFrame)
        return false;
    const auto length = edit.endFrame - edit.startFrame;
    edit.fadeInFrames = std::clamp (edit.fadeInFrames, 0, length);
    edit.fadeOutFrames = std::clamp (edit.fadeOutFrames, 0, length);
    auto edited = cloneSample (current[pad].get());
    edited->edit = edit;
    publishSample (pad, std::move (edited));
    message[pad] = "Non-destructive playback edit applied.";
    reapRetired();
    return true;
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

void SampleEngine::noteOff (int pad, int midiChannel) noexcept
{
    Voice* oldestHeld = nullptr;
    for (auto& playing : voices)
        if (playing.sample != nullptr && playing.pad == pad
            && playing.midiChannel == midiChannel
            && playing.gated && playing.noteHeld
            && (oldestHeld == nullptr || playing.sequence < oldestHeld->sequence))
            oldestHeld = &playing;
    if (oldestHeld != nullptr)
        beginRelease (*oldestHeld);
}

void SampleEngine::releaseAll (int midiChannel) noexcept
{
    for (auto& playing : voices)
        if (midiChannel == 0 || playing.midiChannel == midiChannel)
            beginRelease (playing);
}

void SampleEngine::startVoice (int pad, float velocity, bool audition,
                               int midiChannel, const PlaybackSettings& settings) noexcept
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

    const auto limit = std::clamp (settings[pad].voiceLimit, 1, maxVoicesPerPad);
    const auto chosen = activePadVoices >= limit ? oldestPadVoice
                       : freeVoice >= 0 ? freeVoice : oldestVoice;
    if (chosen < 0)
        return;

    if (voices[chosen].sample != nullptr)
    {
        int tailSlot = -1;
        for (int index = 0; index < maxVoices; ++index)
        {
            if (fadeTails[index].sample == nullptr)
            {
                tailSlot = index;
                break;
            }
        }
        if (tailSlot < 0)
        {
            // Keep every interrupted voice on a fade. When the bounded tail pool
            // is full, release this voice in place and drop the incoming note.
            beginRelease (voices[chosen]);
            return;
        }
        fadeTails[tailSlot] = voices[chosen];
        tailPointers[tailSlot].store (fadeTails[tailSlot].sample);
        beginRelease (fadeTails[tailSlot]);
    }

    const auto gated = settings[pad].gated && ! audition;
    const auto normalizedVelocity = std::clamp (velocity, 0.0f, 1.0f);
    const auto curve = std::clamp (settings[pad].velocityCurve, 0, 2);
    const auto gain = audition ? 1.0f
                      : curve == 0 ? std::sqrt (normalizedVelocity)
                      : curve == 2 ? normalizedVelocity * normalizedVelocity
                                   : normalizedVelocity;
    voices[chosen] = { sample, 0.0, gain,
                       pad, midiChannel, nextSequence++, gated, gated, group, 0, 0 };
    voicePointers[chosen].store (sample);
}

void SampleEngine::renderVoice (Voice& playing, std::atomic<const Sample*>& pointer,
                                juce::AudioBuffer<float>& output, int begin, int end,
                                const PlaybackSettings& settings) noexcept
{
    if (playing.sample == nullptr || output.getNumChannels() < 2)
        return;
    auto* left = output.getWritePointer (0);
    auto* right = output.getWritePointer (1);
    const auto* source = playing.sample;
    const auto& audio = source->data->audio;
    const auto& edit = source->edit;
    const auto regionFrames = edit.endFrame - edit.startFrame;
    const auto* sourceLeft = audio.getReadPointer (0);
    const auto* sourceRight = audio.getReadPointer (audio.getNumChannels() > 1 ? 1 : 0);
    const auto semitones = std::clamp (settings[playing.pad].coarseSemitones, -24, 24)
                           + std::clamp (settings[playing.pad].fineCents, -100.0f, 100.0f) / 100.0;
    const auto pitchRatio = semitones == 0.0 ? 1.0 : std::pow (2.0, semitones / 12.0);
    const auto targetStep = source->data->sampleRate / hostRate * pitchRatio;
    if (playing.step <= 0.0)
        playing.step = targetStep;
    const auto stepSmoothing = std::min (1.0, 1.0 / (hostRate * 0.01));
    const auto level = std::clamp (settings[playing.pad].level, 0.0f, 1.0f);
    const auto fadeInInverse = edit.fadeInFrames > 0 ? 1.0 / edit.fadeInFrames : 0.0;
    const auto fadeOutStart = regionFrames - 1 - edit.fadeOutFrames;
    const auto fadeOutInverse = edit.fadeOutFrames > 0 ? 1.0 / edit.fadeOutFrames : 0.0;
    for (int frame = begin; frame < end; ++frame)
    {
        if (playing.position >= regionFrames)
        {
            playing.sample = nullptr;
            pointer.store (nullptr);
            break;
        }
        const auto sourcePosition = edit.reversed
            ? static_cast<double> (edit.endFrame - 1) - playing.position
            : static_cast<double> (edit.startFrame) + playing.position;
        const auto index = std::clamp (static_cast<int> (sourcePosition),
                                       edit.startFrame, edit.endFrame - 1);
        const auto next = std::min (index + 1, edit.endFrame - 1);
        const auto fraction = static_cast<float> (std::max (0.0, sourcePosition - index));
        const auto releaseGain = playing.releaseRemaining > 0
            ? static_cast<float> (playing.releaseRemaining) / playing.releaseTotal : 1.0f;
        const auto inGain = edit.fadeInFrames == 0 ? 1.0f
            : static_cast<float> (std::min (1.0, playing.position * fadeInInverse));
        const auto outGain = edit.fadeOutFrames == 0 || playing.position < fadeOutStart ? 1.0f
            : static_cast<float> (std::max (0.0,
                (static_cast<double> (regionFrames - 1) - playing.position) * fadeOutInverse));
        left[frame] += (sourceLeft[index] + (sourceLeft[next] - sourceLeft[index]) * fraction)
                       * playing.gain * level * releaseGain * inGain * outGain;
        right[frame] += (sourceRight[index] + (sourceRight[next] - sourceRight[index]) * fraction)
                        * playing.gain * level * releaseGain * inGain * outGain;
        if (std::abs (playing.step - targetStep) > 1.0e-7)
            playing.step += (targetStep - playing.step) * stepSmoothing;
        else
            playing.step = targetStep;
        playing.position += playing.step;
        if (playing.position >= regionFrames)
        {
            playing.sample = nullptr;
            pointer.store (nullptr);
            break;
        }
        if (playing.releaseRemaining > 0 && --playing.releaseRemaining == 0)
        {
            playing.sample = nullptr;
            pointer.store (nullptr);
            break;
        }
    }
}

void SampleEngine::renderRange (juce::AudioBuffer<float>& output, int begin, int end,
                                const PlaybackSettings& settings) noexcept
{
    for (int index = 0; index < maxVoices; ++index)
    {
        renderVoice (voices[index], voicePointers[index], output, begin, end, settings);
        renderVoice (fadeTails[index], tailPointers[index], output, begin, end, settings);
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
    if (transportStopRequested.exchange (false))
        releaseAll();

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
            startVoice (pad, 1.0f, true, 0, settings);
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
            releaseAll (midiMessage.getChannel());
            continue;
        }
        if (midiMessage.isNoteOff (true))
        {
            const auto pad = midiMessage.getNoteNumber() - firstMidiNote;
            if (pad >= 0 && pad < padCount)
                noteOff (pad, midiMessage.getChannel());
            continue;
        }
        if (midiMessage.isNoteOn() && midiMessage.getVelocity() > 0.0f)
        {
            const auto pad = midiMessage.getNoteNumber() - firstMidiNote;
            if (pad >= 0 && pad < padCount)
            {
                midiTriggerCounters[pad].fetch_add (1, std::memory_order_relaxed);
                startVoice (pad, midiMessage.getFloatVelocity(), false,
                            midiMessage.getChannel(), settings);
            }
        }
    }

    renderRange (output, cursor, output.getNumSamples(), settings);
    std::array<const Voice*, padCount> latestVoices {};
    for (const auto& voice : voices)
        if (voice.sample != nullptr)
        {
            auto& latest = latestVoices[voice.pad];
            if (latest == nullptr || voice.sequence > latest->sequence)
                latest = &voice;
        }
    for (int pad = 0; pad < padCount; ++pad)
    {
        const auto* latest = latestVoices[pad];
        const auto frame = latest == nullptr ? -1
            : static_cast<int> (latest->sample->edit.reversed
                ? latest->sample->edit.endFrame - 1 - latest->position
                : latest->sample->edit.startFrame + latest->position);
        playbackFrames[pad].store (frame, std::memory_order_relaxed);
    }
    callbackActive.store (false);
}

juce::var SampleEngine::getSnapshot (int selectedPad) const
{
    selectedPad = std::clamp (selectedPad, 0, padCount - 1);
    std::lock_guard lock (controlMutex);
    auto* object = new juce::DynamicObject();
    object->setProperty ("schemaVersion", 2);
    object->setProperty ("selectedPad", selectedPad);
    object->setProperty ("clipboardReady", clipboardReady);
    object->setProperty ("status", status[selectedPad]);
    object->setProperty ("message", message[selectedPad]);
    object->setProperty ("sampleName", current[selectedPad] != nullptr
                                           ? current[selectedPad]->name : juce::String());
    object->setProperty ("sampleRate", current[selectedPad] != nullptr
                                           ? current[selectedPad]->data->sampleRate : 0.0);
    object->setProperty ("durationSeconds", current[selectedPad] != nullptr
        ? current[selectedPad]->data->audio.getNumSamples() / current[selectedPad]->data->sampleRate : 0.0);
    object->setProperty ("sourceFrames", current[selectedPad] != nullptr
        ? current[selectedPad]->data->audio.getNumSamples() : 0);
    object->setProperty ("sampleRevision", static_cast<juce::int64> (
        sampleRevisions[selectedPad]));
    const auto edit = current[selectedPad] != nullptr ? current[selectedPad]->edit : Edit {};
    object->setProperty ("trimStart", edit.startFrame);
    object->setProperty ("trimEnd", edit.endFrame);
    object->setProperty ("fadeInFrames", edit.fadeInFrames);
    object->setProperty ("fadeOutFrames", edit.fadeOutFrames);
    object->setProperty ("reverse", edit.reversed);
    object->setProperty ("playbackFrame", playbackFrames[selectedPad].load (
        std::memory_order_relaxed));
    juce::Array<juce::var> pads;
    for (int pad = 0; pad < padCount; ++pad)
    {
        auto* item = new juce::DynamicObject();
        item->setProperty ("index", pad);
        item->setProperty ("midiNote", firstMidiNote + pad);
        item->setProperty ("midiTriggerCount", static_cast<juce::int64> (
            midiTriggerCounters[pad].load (std::memory_order_relaxed)));
        item->setProperty ("status", status[pad]);
        item->setProperty ("sampleName", current[pad] != nullptr ? current[pad]->name : juce::String());
        pads.add (juce::var (item));
    }
    object->setProperty ("pads", juce::var (pads));
    return juce::var (object);
}

juce::var SampleEngine::getWaveform (int pad, int level) const
{
    if (pad < 0 || pad >= padCount || level < 0 || level >= 4)
        return {};
    std::lock_guard lock (controlMutex);
    if (current[pad] == nullptr)
        return {};
    const auto& wave = current[pad]->data->waveform[level];
    juce::Array<juce::var> minimum, maximum;
    minimum.ensureStorageAllocated (static_cast<int> (wave.minimum.size()));
    maximum.ensureStorageAllocated (static_cast<int> (wave.maximum.size()));
    for (const auto value : wave.minimum)
        minimum.add (value);
    for (const auto value : wave.maximum)
        maximum.add (value);
    auto* result = new juce::DynamicObject();
    result->setProperty ("schemaVersion", 6);
    result->setProperty ("pad", pad);
    result->setProperty ("level", level);
    result->setProperty ("sampleRevision", static_cast<juce::int64> (sampleRevisions[pad]));
    result->setProperty ("minimum", juce::var (minimum));
    result->setProperty ("maximum", juce::var (maximum));
    return juce::var (result);
}

void SampleEngine::publishSample (int pad, std::unique_ptr<Sample> next)
{
    // controlMutex is held. Audio reads immutable samples through atomics.
    const auto contentChanged = current[pad] == nullptr || next == nullptr
                                || current[pad]->data != next->data;
    activeSamples[pad].store (next.get());
    if (current[pad] != nullptr)
        retired.push_back (std::move (current[pad]));
    current[pad] = std::move (next);
    if (contentChanged)
        ++sampleRevisions[pad];
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
        for (const auto& pointer : tailPointers)
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
        std::unique_ptr<juce::MemoryBlock> memory;
        juce::String sourceName;
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
            memory = std::move (pendingMemory[pad]);
            sourceName = pendingNames[pad];
            generation = loadGenerations[pad];
            pendingLoads[pad] = false;
        }

#ifdef HYPERREAL_ENGINE_TEST_HOOKS
        testDecodeStarted.store (true);
        while (testPauseDecode.load())
            std::this_thread::sleep_for (std::chrono::milliseconds (1));
#endif

        juce::String error;
        std::unique_ptr<Sample> decoded;
        const auto validInput = memory != nullptr
            ? ! memory->isEmpty() && memory->getSize() <= maxInputBytes
            : file.existsAsFile() && file.getSize() <= maxInputBytes;
        if (! validInput || ! juce::File (sourceName).hasFileExtension (".wav;.aif;.aiff;.flac"))
        {
            error = "Choose a WAV, AIFF or FLAC file under 128 MB.";
        }
        else if (auto reader = std::unique_ptr<juce::AudioFormatReader> (
                     memory != nullptr
                         ? formats.createReaderFor (std::make_unique<juce::MemoryInputStream> (
                               std::move (*memory)))
                         : formats.createReaderFor (file)))
        {
            if (reader->numChannels < 1 || reader->numChannels > maxChannels
                || ! std::isfinite (reader->sampleRate)
                || reader->sampleRate <= 0.0 || reader->sampleRate > 384000.0
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
                auto data = std::make_shared<SampleData>();
                data->audio.setSize (static_cast<int> (reader->numChannels), frames);
                data->sampleRate = reader->sampleRate;
                decoded->data = data;
                decoded->name = sourceName;
                decoded->edit = defaultEdit (frames, reader->sampleRate);
                if (! reader->read (&data->audio, 0, frames, 0, true, true))
                {
                    decoded.reset();
                    error = "Could not decode the selected sample.";
                }
                else
                    buildWaveform (*data);
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
                auto bytes = sampleBytes (decoded->data->audio);
                for (int other = 0; other < padCount; ++other)
                    if (other != pad && current[other] != nullptr)
                        bytes += sampleBytes (current[other]->data->audio);
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

void SampleEngine::writeSample (juce::OutputStream& output, const Sample* sample, bool includeEdits)
{
    output.writeByte (sample != nullptr ? 1 : 0);
    if (sample == nullptr)
        return;
    output.writeString (sample->name);
    output.writeDouble (sample->data->sampleRate);
    output.writeInt (sample->data->audio.getNumChannels());
    output.writeInt (sample->data->audio.getNumSamples());
    for (int channel = 0; channel < sample->data->audio.getNumChannels(); ++channel)
        for (int frame = 0; frame < sample->data->audio.getNumSamples(); ++frame)
            output.writeFloat (sample->data->audio.getSample (channel, frame));
    if (includeEdits)
    {
        output.writeInt (sample->edit.startFrame);
        output.writeInt (sample->edit.endFrame);
        output.writeInt (sample->edit.fadeInFrames);
        output.writeInt (sample->edit.fadeOutFrames);
        output.writeByte (sample->edit.reversed ? 1 : 0);
    }
}

void SampleEngine::writeSampleState (juce::OutputStream& output, bool includeEdits) const
{
    std::lock_guard lock (controlMutex);
    for (const auto& sample : current)
        writeSample (output, sample.get(), includeEdits);
}

bool SampleEngine::readSample (juce::InputStream& input, std::unique_ptr<Sample>& sample,
                               juce::int64& decodedBytes, bool hasEdits)
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
    const auto sampleRate = input.readDouble();
    const auto channels = input.readInt();
    const auto frames = input.readInt();
    const auto bytes = static_cast<juce::int64> (channels) * frames * 4;
    if (restored->name.length() > 255
        || channels < 1 || channels > maxChannels || frames < 1
        || ! std::isfinite (sampleRate)
        || sampleRate <= 0.0 || sampleRate > 384000.0
        || frames > maxDurationSeconds * sampleRate
        || bytes > maxDecodedSampleBytes
        || input.getNumBytesRemaining() < bytes)
        return false;

    auto data = std::make_shared<SampleData>();
    data->audio.setSize (channels, frames);
    data->sampleRate = sampleRate;
    for (int channel = 0; channel < channels; ++channel)
        for (int frame = 0; frame < frames; ++frame)
        {
            const auto value = input.readFloat();
            if (! std::isfinite (value) || std::abs (value) > 100.0f)
                return false;
            data->audio.setSample (channel, frame, value);
        }
    restored->data = data;
    restored->edit = defaultEdit (frames, sampleRate);
    if (hasEdits)
    {
        if (input.getNumBytesRemaining() < 17)
            return false;
        Edit edit;
        edit.startFrame = input.readInt();
        edit.endFrame = input.readInt();
        edit.fadeInFrames = input.readInt();
        edit.fadeOutFrames = input.readInt();
        const auto reverse = input.readByte();
        if (edit.startFrame < 0 || edit.endFrame > frames
            || edit.startFrame >= edit.endFrame
            || edit.fadeInFrames < 0 || edit.fadeOutFrames < 0
            || edit.fadeInFrames > edit.endFrame - edit.startFrame
            || edit.fadeOutFrames > edit.endFrame - edit.startFrame
            || (reverse != 0 && reverse != 1))
            return false;
        edit.reversed = reverse == 1;
        restored->edit = edit;
    }
    buildWaveform (*data);
    sample = std::move (restored);
    decodedBytes = bytes;
    return true;
}

bool SampleEngine::restoreSampleState (juce::InputStream& input, bool hasEdits)
{
    std::array<std::unique_ptr<Sample>, padCount> restored;
    juce::int64 totalBytes = 0;
    for (auto& sample : restored)
    {
        juce::int64 bytes = 0;
        if (! readSample (input, sample, bytes, hasEdits))
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
    if (! readSample (input, restored, bytes, false) || input.getNumBytesRemaining() != 0)
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
