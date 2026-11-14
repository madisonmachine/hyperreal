#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_core/juce_core.h>
#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

class SampleEngine final
{
public:
    SampleEngine();
    ~SampleEngine();

    void prepare (double hostSampleRate) noexcept;
    void process (juce::AudioBuffer<float>& output, const juce::MidiBuffer& midi) noexcept;
    void requestLoad (juce::File file);
    void requestAudition() noexcept;
    juce::var getSnapshot() const;
    void writeSampleState (juce::OutputStream& output) const;
    bool restoreSampleState (juce::InputStream& input);

private:
    struct Sample
    {
        juce::AudioBuffer<float> audio;
        double sampleRate = 0.0;
        juce::String name;
    };

    void workerLoop();
    void publishSample (std::unique_ptr<Sample> next);
    void reapRetired();
    void startVoice (float velocity) noexcept;
    void renderRange (juce::AudioBuffer<float>& output, int begin, int end) noexcept;

    mutable std::mutex controlMutex;
    std::condition_variable controlChanged;
    std::thread worker;
    bool stopping = false;
    bool pendingLoad = false;
    juce::File pendingFile;
    juce::String status { "empty" };
    juce::String message { "Load WAV, AIFF or FLAC to begin." };
    std::unique_ptr<Sample> current;
    std::vector<std::unique_ptr<Sample>> retired;

    std::atomic<const Sample*> activeSample { nullptr };
    std::atomic<const Sample*> voicePointer { nullptr };
    std::atomic<bool> callbackActive { false };
    std::atomic<unsigned int> auditionCounter { 0 };

    // Audio-thread-only voice data.
    const Sample* voice = nullptr;
    double position = 0.0;
    double hostRate = 44100.0;
    float voiceGain = 0.0f;
    unsigned int auditionSeen = 0;
};
