#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "../../packages/engine/SampleEngine.h"

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
    void requestSampleLoad (juce::File file) { requestSampleLoad (0, std::move (file)); }
    void cancelSampleLoad (int pad) { engine.cancelLoad (pad); }
    void requestAudition (int pad) noexcept { engine.requestAudition (pad); }
    void requestAudition() noexcept { requestAudition (selectedPad.load()); }
    bool selectPad (int pad) noexcept;
    int getSelectedPad() const noexcept { return selectedPad.load(); }
    juce::var getSnapshot() const;
    bool setMasterGain (const juce::var& command);

private:
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    juce::AudioProcessorValueTreeState parameters;
    SampleEngine engine;
    std::atomic<int> selectedPad { 0 };
    juce::SmoothedValue<float> smoothedMasterGain;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HyperrealAudioProcessor)
};
