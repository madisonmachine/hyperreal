#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_core/juce_core.h>
#include <array>
#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

class SampleEngine final
{
public:
    static constexpr int padCount = 16;
    static constexpr int firstMidiNote = 36;
    static constexpr int maxVoices = 64;
    struct PadPlaybackSettings
    {
        bool gated = false;
        int chokeGroup = 0;
        float level = 1.0f;
        bool muted = false;
        bool solo = false;
    };
    using PlaybackSettings = std::array<PadPlaybackSettings, padCount>;

    SampleEngine();
    ~SampleEngine();

    void prepare (double hostSampleRate) noexcept;
    void process (juce::AudioBuffer<float>& output, const juce::MidiBuffer& midi) noexcept;
    void process (juce::AudioBuffer<float>& output, const juce::MidiBuffer& midi,
                  const PlaybackSettings& settings) noexcept;
    void requestLoad (int pad, juce::File file);
    void requestLoad (juce::File file) { requestLoad (0, std::move (file)); }
    void cancelLoad (int pad);
    void requestAudition (int pad) noexcept;
    void requestAudition() noexcept { requestAudition (0); }
    juce::var getSnapshot (int selectedPad = 0) const;
    void writeSampleState (juce::OutputStream& output) const;
    bool restoreSampleState (juce::InputStream& input);
    bool restorePhase2SampleState (juce::InputStream& input);

private:
    struct Sample
    {
        juce::AudioBuffer<float> audio;
        double sampleRate = 0.0;
        juce::String name;
    };
    struct Voice
    {
        const Sample* sample = nullptr;
        double position = 0.0;
        float gain = 0.0f;
        int pad = 0;
        juce::uint64 sequence = 0;
        bool gated = false;
        bool noteHeld = false;
        int chokeGroup = 0;
        int releaseRemaining = 0;
        int releaseTotal = 0;
    };

    static bool readSample (juce::InputStream& input, std::unique_ptr<Sample>& sample,
                            juce::int64& decodedBytes);
    static void writeSample (juce::OutputStream& output, const Sample* sample);
    void workerLoop();
    void publishSample (int pad, std::unique_ptr<Sample> next);
    void reapRetired();
    void startVoice (int pad, float velocity, bool audition,
                     const PlaybackSettings& settings) noexcept;
    void noteOff (int pad) noexcept;
    void releaseAll() noexcept;
    void beginRelease (Voice& voice) noexcept;
    void renderRange (juce::AudioBuffer<float>& output, int begin, int end,
                      const PlaybackSettings& settings) noexcept;

    mutable std::mutex controlMutex;
    std::condition_variable controlChanged;
    std::thread worker;
    bool stopping = false;
    std::array<bool, padCount> pendingLoads {};
    std::array<juce::File, padCount> pendingFiles;
    std::array<juce::uint64, padCount> loadGenerations {};
    int lastDequeuedPad = padCount - 1;
    std::array<juce::String, padCount> status;
    std::array<juce::String, padCount> message;
    std::array<std::unique_ptr<Sample>, padCount> current;
    std::vector<std::unique_ptr<Sample>> retired;

    std::array<std::atomic<const Sample*>, padCount> activeSamples {};
    std::array<std::atomic<const Sample*>, maxVoices> voicePointers {};
    std::atomic<bool> callbackActive { false };
    std::array<std::atomic<unsigned int>, padCount> auditionCounters {};

    // The audio thread owns all voice state. Other threads only inspect voicePointers.
    std::array<Voice, maxVoices> voices {};
    std::array<unsigned int, padCount> auditionSeen {};
    std::array<bool, padCount> audiblePads {};
    juce::uint64 nextSequence = 1;
    double hostRate = 44100.0;
};
