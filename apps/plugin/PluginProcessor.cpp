#include "PluginProcessor.h"
#include <juce_audio_utils/juce_audio_utils.h>

namespace
{
constexpr int stateSchemaVersion = 1;
constexpr int padCount = 16;
}

HyperrealAudioProcessor::HyperrealAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Main", juce::AudioChannelSet::stereo(), true)),
      parameters (*this, nullptr, "HYPERREAL_STATE", createParameterLayout())
{
}

void HyperrealAudioProcessor::prepareToPlay (double, int) {}
void HyperrealAudioProcessor::releaseResources() {}

void HyperrealAudioProcessor::processBlock (juce::AudioBuffer<float>& audio, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    audio.clear();
}

juce::AudioProcessorEditor* HyperrealAudioProcessor::createEditor()
{
    // Temporary Phase 1 host-parameter view. The local React WebView arrives in Phase 2.
    return new juce::GenericAudioProcessorEditor (*this);
}

bool HyperrealAudioProcessor::hasEditor() const { return true; }
const juce::String HyperrealAudioProcessor::getName() const { return JucePlugin_Name; }
bool HyperrealAudioProcessor::acceptsMidi() const { return true; }
bool HyperrealAudioProcessor::producesMidi() const { return false; }
bool HyperrealAudioProcessor::isMidiEffect() const { return false; }
double HyperrealAudioProcessor::getTailLengthSeconds() const { return 0.0; }
int HyperrealAudioProcessor::getNumPrograms() { return 1; }
int HyperrealAudioProcessor::getCurrentProgram() { return 0; }
void HyperrealAudioProcessor::setCurrentProgram (int) {}
const juce::String HyperrealAudioProcessor::getProgramName (int) { return "Default"; }
void HyperrealAudioProcessor::changeProgramName (int, const juce::String&) {}

void HyperrealAudioProcessor::getStateInformation (juce::MemoryBlock& destination)
{
    auto snapshot = parameters.copyState();
    snapshot.setProperty ("schema_version", stateSchemaVersion, nullptr);
    juce::MemoryOutputStream stream (destination, false);
    snapshot.writeToStream (stream);
}

void HyperrealAudioProcessor::setStateInformation (const void* data, int size)
{
    if (data == nullptr || size <= 0)
        return;

    const auto snapshot = juce::ValueTree::readFromData (data, static_cast<size_t> (size));
    if (! snapshot.hasType ("HYPERREAL_STATE")
        || static_cast<int> (snapshot.getProperty ("schema_version", 0)) != stateSchemaVersion)
        return;

    parameters.replaceState (snapshot);
}

bool HyperrealAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

juce::AudioProcessorValueTreeState::ParameterLayout HyperrealAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> layout;
    const auto range = juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f);

    layout.push_back (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "master.gain.v1", 1 }, "Master gain", range, 1.0f));

    for (int pad = 1; pad <= padCount; ++pad)
    {
        const auto id = juce::String::formatted ("pad.%02d.level.v1", pad);
        const auto name = juce::String::formatted ("Pad %02d level", pad);
        layout.push_back (std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { id, 1 }, name, range, 1.0f));
    }

    return { layout.begin(), layout.end() };
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new HyperrealAudioProcessor();
}
