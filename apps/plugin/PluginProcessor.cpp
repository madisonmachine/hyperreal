#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <juce_audio_utils/juce_audio_utils.h>
#include <cmath>

namespace
{
constexpr int stateSchemaVersion = 4;
constexpr int padCount = 16;
constexpr char stateMagic[] = { 'H', 'R', 'P', '4' };
constexpr char phase3Magic[] = { 'H', 'R', 'P', '3' };
constexpr char phase2Magic[] = { 'H', 'R', 'P', '2' };
}

HyperrealAudioProcessor::HyperrealAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Main", juce::AudioChannelSet::stereo(), true)),
      parameters (*this, nullptr, "HYPERREAL_STATE", createParameterLayout())
{
    for (int pad = 0; pad < padCount; ++pad)
    {
        const auto number = pad + 1;
        padLevelParameters[pad] = parameters.getRawParameterValue (
            juce::String::formatted ("pad.%02d.level.v1", number));
        padModeParameters[pad] = parameters.getRawParameterValue (
            juce::String::formatted ("pad.%02d.mode.v1", number));
        padChokeParameters[pad] = parameters.getRawParameterValue (
            juce::String::formatted ("pad.%02d.choke.v1", number));
    }
}

void HyperrealAudioProcessor::prepareToPlay (double sampleRate, int)
{
    engine.prepare (sampleRate);
    smoothedMasterGain.reset (sampleRate, 0.01);
    smoothedMasterGain.setCurrentAndTargetValue (parameters.getRawParameterValue ("master.gain.v1")->load());
}

void HyperrealAudioProcessor::releaseResources() {}

void HyperrealAudioProcessor::processBlock (juce::AudioBuffer<float>& audio, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    SampleEngine::PlaybackSettings playback {};
    for (int pad = 0; pad < padCount; ++pad)
    {
        playback[pad].level = padLevelParameters[pad]->load();
        playback[pad].gated = padModeParameters[pad]->load() >= 0.5f;
        playback[pad].chokeGroup = static_cast<int> (padChokeParameters[pad]->load());
    }
    engine.process (audio, midi, playback);
    smoothedMasterGain.setTargetValue (parameters.getRawParameterValue ("master.gain.v1")->load());
    for (int frame = 0; frame < audio.getNumSamples(); ++frame)
    {
        const auto gain = smoothedMasterGain.getNextValue();
        for (int channel = 0; channel < audio.getNumChannels(); ++channel)
            audio.getWritePointer (channel)[frame] *= gain;
    }
}

juce::AudioProcessorEditor* HyperrealAudioProcessor::createEditor()
{
    if (HyperrealAudioProcessorEditor::isSupported())
        return new HyperrealAudioProcessorEditor (*this);
    return new juce::GenericAudioProcessorEditor (*this);
}

juce::var HyperrealAudioProcessor::getSnapshot() const
{
    auto snapshot = engine.getSnapshot (selectedPad.load());
    snapshot.getDynamicObject()->setProperty (
        "masterGain", parameters.getRawParameterValue ("master.gain.v1")->load());
    const auto pad = selectedPad.load();
    snapshot.getDynamicObject()->setProperty ("padLevel", padLevelParameters[pad]->load());
    snapshot.getDynamicObject()->setProperty ("padMode", static_cast<int> (padModeParameters[pad]->load()));
    snapshot.getDynamicObject()->setProperty ("chokeGroup", static_cast<int> (padChokeParameters[pad]->load()));
    return snapshot;
}

bool HyperrealAudioProcessor::selectPad (int pad) noexcept
{
    if (pad < 0 || pad >= padCount)
        return false;
    selectedPad.store (pad);
    return true;
}

bool HyperrealAudioProcessor::setMasterGain (const juce::var& command)
{
    if (! command.isObject()
        || static_cast<int> (command.getProperty ("schemaVersion", 0)) != 1)
        return false;

    const auto rawValue = command.getProperty ("value", -1.0);
    if (! (rawValue.isDouble() || rawValue.isInt() || rawValue.isInt64()))
        return false;
    const auto value = static_cast<double> (rawValue);
    if (! std::isfinite (value) || value < 0.0 || value > 1.0)
        return false;

    if (auto* parameter = parameters.getParameter ("master.gain.v1"))
    {
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost (static_cast<float> (value));
        parameter->endChangeGesture();
        return true;
    }
    return false;
}
bool HyperrealAudioProcessor::setPadMode (const juce::var& command)
{
    return setPadDiscreteParameter (command, "mode", 1);
}
bool HyperrealAudioProcessor::setPadChokeGroup (const juce::var& command)
{
    return setPadDiscreteParameter (command, "choke", 4);
}
bool HyperrealAudioProcessor::setPadDiscreteParameter (const juce::var& command,
                                                        const char* name, int maximum)
{
    if (! command.isObject()
        || static_cast<int> (command.getProperty ("schemaVersion", 0)) != 3)
        return false;
    const auto rawPad = command.getProperty ("pad", -1);
    const auto rawValue = command.getProperty ("value", -1);
    const auto numeric = [] (const juce::var& item)
    {
        return item.isInt() || item.isInt64() || item.isDouble();
    };
    if (! numeric (rawPad) || ! numeric (rawValue))
        return false;
    const auto pad = static_cast<double> (rawPad);
    const auto value = static_cast<double> (rawValue);
    if (! std::isfinite (pad) || ! std::isfinite (value)
        || std::trunc (pad) != pad || std::trunc (value) != value
        || pad < 0.0 || pad >= padCount || value < 0.0 || value > maximum)
        return false;
    const auto id = juce::String::formatted ("pad.%02d.", static_cast<int> (pad) + 1)
                    + name + ".v1";
    if (auto* parameter = parameters.getParameter (id))
    {
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost (static_cast<float> (value / maximum));
        parameter->endChangeGesture();
        return true;
    }
    return false;
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

    juce::MemoryBlock parametersData;
    juce::MemoryOutputStream parameterStream (parametersData, false);
    snapshot.writeToStream (parameterStream);

    juce::MemoryOutputStream output (destination, false);
    output.write (stateMagic, sizeof (stateMagic));
    output.writeInt (stateSchemaVersion);
    output.writeInt (static_cast<int> (parametersData.getSize()));
    output.write (parametersData.getData(), parametersData.getSize());
    engine.writeSampleState (output);
}

void HyperrealAudioProcessor::setStateInformation (const void* data, int size)
{
    if (data == nullptr || size <= 0)
        return;

    // Accept the Phase 1 parameter-only state so saved shell sessions still open.
    const auto legacy = juce::ValueTree::readFromData (data, static_cast<size_t> (size));
    if (legacy.hasType ("HYPERREAL_STATE")
        && static_cast<int> (legacy.getProperty ("schema_version", 0)) == 1)
    {
        parameters.replaceState (legacy);
        return;
    }

    juce::MemoryInputStream input (data, static_cast<size_t> (size), false);
    char magic[4] {};
    if (input.read (magic, sizeof (magic)) != sizeof (magic))
        return;

    const auto version = input.readInt();
    const auto phase4 = std::memcmp (magic, stateMagic, sizeof (magic)) == 0
                        && version == stateSchemaVersion;
    const auto phase3 = std::memcmp (magic, phase3Magic, sizeof (magic)) == 0
                        && version == 3;
    const auto phase2 = std::memcmp (magic, phase2Magic, sizeof (magic)) == 0
                        && version == 2;
    if (! phase4 && ! phase3 && ! phase2)
        return;

    const auto parameterSize = input.readInt();
    if (parameterSize <= 0 || parameterSize > input.getNumBytesRemaining())
        return;

    juce::MemoryBlock parameterBytes;
    parameterBytes.setSize (static_cast<size_t> (parameterSize));
    if (input.read (parameterBytes.getData(), parameterSize) != parameterSize)
        return;

    const auto snapshot = juce::ValueTree::readFromData (parameterBytes.getData(), parameterBytes.getSize());
    if (! snapshot.hasType ("HYPERREAL_STATE")
        || static_cast<int> (snapshot.getProperty ("schema_version", 0)) != version
        || ! (phase4 || phase3 ? engine.restoreSampleState (input)
                               : engine.restorePhase2SampleState (input)))
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

    for (int pad = 1; pad <= padCount; ++pad)
    {
        const auto modeId = juce::String::formatted ("pad.%02d.mode.v1", pad);
        const auto chokeId = juce::String::formatted ("pad.%02d.choke.v1", pad);
        layout.push_back (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { modeId, 1 },
            juce::String::formatted ("Pad %02d mode", pad),
            juce::StringArray { "One-shot", "Gate" }, 0));
        layout.push_back (std::make_unique<juce::AudioParameterInt> (
            juce::ParameterID { chokeId, 1 },
            juce::String::formatted ("Pad %02d choke group", pad), 0, 4, 0));
    }

    return { layout.begin(), layout.end() };
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new HyperrealAudioProcessor();
}
