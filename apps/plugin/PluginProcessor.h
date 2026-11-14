#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "../../packages/engine/SampleEngine.h"
#include <array>

class HyperrealAudioProcessor final : public juce::AudioProcessor
{
public:
    HyperrealAudioProcessor();

    void prepareToPlay (double sampleRate, int maximumExpectedSamplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>& audio, juce::MidiBuffer& midi) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;
    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& name) override;
    void getStateInformation (juce::MemoryBlock& destination) override;
    void setStateInformation (const void* data, int size) override;

    void requestSampleLoad (int pad, juce::File file) { engine.requestLoad (pad, std::move (file)); }
    void requestSampleLoadData (int pad, juce::String name, juce::MemoryBlock data)
    {
        engine.requestLoadData (pad, std::move (name), std::move (data));
    }
    void requestSampleLoad (juce::File file) { requestSampleLoad (0, std::move (file)); }
    void cancelSampleLoad (int pad) { engine.cancelLoad (pad); }
    void requestAudition (int pad) noexcept { engine.requestAudition (pad); }
    void requestAudition() noexcept { requestAudition (selectedPad.load()); }
    bool selectPad (int pad) noexcept;
    int getSelectedPad() const noexcept { return selectedPad.load(); }
    juce::var getSnapshot() const;
    juce::var getWaveform (int pad, int level) const { return engine.getWaveform (pad, level); }
    bool setMasterGain (const juce::var& command);
    bool setPadMode (const juce::var& command);
    bool setPadChokeGroup (const juce::var& command);
    bool setPadMute (const juce::var& command);
    bool setPadSolo (const juce::var& command);
    bool setPadVoiceLimit (const juce::var& command);
    bool setPadVelocityCurve (const juce::var& command);
    bool setPadEdit (const juce::var& command);
    bool setPadCoarseTune (const juce::var& command);
    bool setPadFineTune (const juce::var& command);
    bool copyPad (int pad);
    bool pastePad (int pad);
    bool swapPads (int first, int second);
    bool clearPad (int pad);

private:
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    bool setPadDiscreteParameter (const juce::var& command, const char* name,
                                  int maximum, int minimum = 0);
    std::array<float, 9> readPadSettings (int pad) const;
    void applyPadSettings (int pad, const std::array<float, 9>& values);

    juce::AudioProcessorValueTreeState parameters;
    SampleEngine engine;
    std::array<std::atomic<float>*, SampleEngine::padCount> padLevelParameters {};
    std::array<std::atomic<float>*, SampleEngine::padCount> padModeParameters {};
    std::array<std::atomic<float>*, SampleEngine::padCount> padChokeParameters {};
    std::array<std::atomic<float>*, SampleEngine::padCount> padMuteParameters {};
    std::array<std::atomic<float>*, SampleEngine::padCount> padSoloParameters {};
    std::array<std::atomic<float>*, SampleEngine::padCount> padVoiceLimitParameters {};
    std::array<std::atomic<float>*, SampleEngine::padCount> padVelocityCurveParameters {};
    std::array<std::atomic<float>*, SampleEngine::padCount> padCoarseTuneParameters {};
    std::array<std::atomic<float>*, SampleEngine::padCount> padFineTuneParameters {};
    std::array<float, 9> clipboardSettings { 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 4.0f, 1.0f, 0.0f, 0.0f };
    bool clipboardSettingsReady = false;
    std::atomic<int> selectedPad { 0 };
    bool transportWasPlaying = false;
    juce::SmoothedValue<float> smoothedMasterGain;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HyperrealAudioProcessor)
};
