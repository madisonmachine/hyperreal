#include "SampleEngine.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>

namespace
{
constexpr int maxChannels = 2;
constexpr double maxDurationSeconds = 60.0;
constexpr juce::int64 maxInputBytes = 128 * 1024 * 1024;
}

SampleEngine::SampleEngine()
{
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
    voice = nullptr;
    voicePointer.store (nullptr);
    position = 0.0;
}

void SampleEngine::requestLoad (juce::File file)
{
    {
        std::lock_guard lock (controlMutex);
        pendingFile = std::move (file);
        pendingLoad = true;
        status = "loading";
        message = "Decoding sample on a worker thread.";
    }
    controlChanged.notify_one();
}

void SampleEngine::requestAudition() noexcept
{
    auditionCounter.fetch_add (1);
}

void SampleEngine::startVoice (float velocity) noexcept
{
    voice = activeSample.load();
    voicePointer.store (voice);
    position = 0.0;
    voiceGain = std::clamp (velocity, 0.0f, 1.0f);
}

void SampleEngine::renderRange (juce::AudioBuffer<float>& output, int begin, int end) noexcept
{
    if (voice == nullptr || output.getNumChannels() < 2)
        return;

    auto* left = output.getWritePointer (0);
    auto* right = output.getWritePointer (1);
    const int frames = voice->audio.getNumSamples();
    const auto* sourceLeft = voice->audio.getReadPointer (0);
    const auto* sourceRight = voice->audio.getReadPointer (voice->audio.getNumChannels() > 1 ? 1 : 0);
    const double step = voice->sampleRate / hostRate;

    for (int frame = begin; frame < end; ++frame)
    {
        const auto index = static_cast<int> (position);
        if (index >= frames)
        {
            voice = nullptr;
            voicePointer.store (nullptr);
            break;
        }

        const auto next = std::min (index + 1, frames - 1);
        const auto fraction = static_cast<float> (position - index);
        left[frame] += (sourceLeft[index] + (sourceLeft[next] - sourceLeft[index]) * fraction) * voiceGain;
        right[frame] += (sourceRight[index] + (sourceRight[next] - sourceRight[index]) * fraction) * voiceGain;
        position += step;
    }
}

void SampleEngine::process (juce::AudioBuffer<float>& output, const juce::MidiBuffer& midi) noexcept
{
    callbackActive.store (true);
    output.clear();

    const auto auditionNow = auditionCounter.load();
    if (auditionNow != auditionSeen)
    {
        auditionSeen = auditionNow;
        startVoice (1.0f);
    }

    int cursor = 0;
    for (const auto event : midi)
    {
        const auto eventFrame = std::clamp (event.samplePosition, cursor, output.getNumSamples());
        renderRange (output, cursor, eventFrame);
        cursor = eventFrame;

        const auto& midiMessage = event.getMessage();
        if (midiMessage.isNoteOn() && midiMessage.getNoteNumber() == 36 && midiMessage.getVelocity() > 0.0f)
            startVoice (midiMessage.getFloatVelocity());
    }

    renderRange (output, cursor, output.getNumSamples());
    callbackActive.store (false);
}

juce::var SampleEngine::getSnapshot() const
{
    std::lock_guard lock (controlMutex);
    auto* object = new juce::DynamicObject();
    object->setProperty ("schemaVersion", 1);
    object->setProperty ("status", status);
    object->setProperty ("message", message);
    object->setProperty ("sampleName", current != nullptr ? current->name : juce::String());
    object->setProperty ("sampleRate", current != nullptr ? current->sampleRate : 0.0);
    object->setProperty ("durationSeconds",
                         current != nullptr ? current->audio.getNumSamples() / current->sampleRate : 0.0);
    return juce::var (object);
}

void SampleEngine::publishSample (std::unique_ptr<Sample> next)
{
    // controlMutex is held. The audio callback reads only immutable sample data.
    const auto* nextPointer = next.get();
    activeSample.store (nextPointer);
    if (current != nullptr)
        retired.push_back (std::move (current));
    current = std::move (next);
    reapRetired();
}

void SampleEngine::reapRetired()
{
    if (callbackActive.load())
        return;

    const auto* inVoice = voicePointer.load();
    std::erase_if (retired, [inVoice] (const auto& item) { return item.get() != inVoice; });
}

void SampleEngine::workerLoop()
{
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();

    for (;;)
    {
        juce::File file;
        {
            std::unique_lock lock (controlMutex);
            controlChanged.wait_for (lock, std::chrono::milliseconds (250),
                                     [this] { return stopping || pendingLoad; });
            if (stopping)
                break;
            if (! pendingLoad)
            {
                reapRetired();
                continue;
            }
            file = pendingFile;
            pendingLoad = false;
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
                || reader->sampleRate <= 0.0
                || reader->lengthInSamples < 1
                || reader->lengthInSamples > maxDurationSeconds * reader->sampleRate
                || reader->lengthInSamples > std::numeric_limits<int>::max())
            {
                error = "Sample channels, rate or duration are unsupported.";
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
            if (decoded != nullptr)
            {
                publishSample (std::move (decoded));
                status = "ready";
                message = "MIDI note 36 or AUDITION plays this sample.";
            }
            else
            {
                status = current != nullptr ? "ready" : "error";
                message = error;
            }
            reapRetired();
        }
    }
}

void SampleEngine::writeSampleState (juce::OutputStream& output) const
{
    std::lock_guard lock (controlMutex);
    output.writeByte (current != nullptr ? 1 : 0);
    if (current == nullptr)
        return;

    output.writeString (current->name);
    output.writeDouble (current->sampleRate);
    output.writeInt (current->audio.getNumChannels());
    output.writeInt (current->audio.getNumSamples());
    for (int channel = 0; channel < current->audio.getNumChannels(); ++channel)
        for (int frame = 0; frame < current->audio.getNumSamples(); ++frame)
            output.writeFloat (current->audio.getSample (channel, frame));
}

bool SampleEngine::restoreSampleState (juce::InputStream& input)
{
    if (input.getNumBytesRemaining() < 1)
        return false;
    const auto present = input.readByte();
    if (present == 0)
    {
        std::lock_guard lock (controlMutex);
        publishSample (nullptr);
        status = "empty";
        message = "Load WAV, AIFF or FLAC to begin.";
        return true;
    }
    if (present != 1)
        return false;

    auto restored = std::make_unique<Sample>();
    restored->name = input.readString();
    restored->sampleRate = input.readDouble();
    const auto channels = input.readInt();
    const auto frames = input.readInt();
    if (channels < 1 || channels > maxChannels || frames < 1
        || restored->sampleRate <= 0.0 || restored->sampleRate > 384000.0
        || frames > maxDurationSeconds * restored->sampleRate
        || input.getNumBytesRemaining() < static_cast<juce::int64> (channels) * frames * 4)
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

    {
        std::lock_guard lock (controlMutex);
        publishSample (std::move (restored));
        status = "ready";
        message = "Restored from the DAW project state.";
    }
    return true;
}
