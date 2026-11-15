#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <juce_audio_utils/juce_audio_utils.h>
#include <cmath>
#include <limits>
#include <optional>

namespace
{
constexpr int stateSchemaVersion = 10;
constexpr int padCount = 16;
constexpr char stateMagic[] = { 'H', 'R', '1', '0' };
constexpr char phase9Magic[] = { 'H', 'R', 'P', '9' };
constexpr char phase8Magic[] = { 'H', 'R', 'P', '8' };
constexpr char phase7Magic[] = { 'H', 'R', 'P', '7' };
constexpr char phase6Magic[] = { 'H', 'R', 'P', '6' };
constexpr char phase5Magic[] = { 'H', 'R', 'P', '5' };
constexpr char phase4Magic[] = { 'H', 'R', 'P', '4' };
constexpr char phase3Magic[] = { 'H', 'R', 'P', '3' };
constexpr char phase2Magic[] = { 'H', 'R', 'P', '2' };
juce::AudioParameterFloatAttributes displayAs (const char* unit, int decimals)
{
    return juce::AudioParameterFloatAttributes()
        .withLabel (unit)
        .withStringFromValueFunction ([decimals] (float value, int)
        {
            return juce::String (value, decimals);
        })
        .withValueFromStringFunction ([] (const juce::String& text)
        {
            return text.getFloatValue();
        });
}
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
        padMuteParameters[pad] = parameters.getRawParameterValue (
            juce::String::formatted ("pad.%02d.mute.v1", number));
        padSoloParameters[pad] = parameters.getRawParameterValue (
            juce::String::formatted ("pad.%02d.solo.v1", number));
        padVoiceLimitParameters[pad] = parameters.getRawParameterValue (
            juce::String::formatted ("pad.%02d.voices.v1", number));
        padVelocityCurveParameters[pad] = parameters.getRawParameterValue (
            juce::String::formatted ("pad.%02d.velocity.v1", number));
        padCoarseTuneParameters[pad] = parameters.getRawParameterValue (
            juce::String::formatted ("pad.%02d.coarse.v1", number));
        padFineTuneParameters[pad] = parameters.getRawParameterValue (
            juce::String::formatted ("pad.%02d.fine.v1", number));
        constexpr const char* amplitudeNames[] { "attack", "decay", "sustain", "release" };
        for (int stage = 0; stage < 4; ++stage)
            padAmplitudeParameters[pad][stage] = parameters.getRawParameterValue (
                juce::String::formatted ("pad.%02d.amp.", number)
                + amplitudeNames[stage] + ".v1");
        constexpr const char* filterNames[] { "mode", "cutoff", "resonance", "depth",
                                              "attack", "decay", "sustain", "release" };
        for (int setting = 0; setting < 8; ++setting)
            padFilterParameters[pad][setting] = parameters.getRawParameterValue (
                juce::String::formatted ("pad.%02d.filter.", number)
                + filterNames[setting] + ".v1");
        constexpr const char* lfoNames[] { "destination", "shape", "rate", "depth", "phase" };
        for (int setting = 0; setting < 5; ++setting)
            padLfoParameters[pad][setting] = parameters.getRawParameterValue (
                juce::String::formatted ("pad.%02d.lfo.", number)
                + lfoNames[setting] + ".v1");
    }
}

void HyperrealAudioProcessor::prepareToPlay (double sampleRate, int)
{
    engine.prepare (sampleRate);
    currentSampleRate = sampleRate;
    for (int pad = 0; pad < padCount; ++pad)
    {
        padClickSeen[pad] = padClickCounters[pad].load (std::memory_order_relaxed);
        noteOffCountdown[pad] = 0;
    }
    transportWasPlaying = false;
    smoothedMasterGain.reset (sampleRate, 0.01);
    smoothedMasterGain.setCurrentAndTargetValue (parameters.getRawParameterValue ("master.gain.v1")->load());
}

void HyperrealAudioProcessor::releaseResources() {}

void HyperrealAudioProcessor::processBlock (juce::AudioBuffer<float>& audio, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    if (auto* hostPlayHead = getPlayHead())
        if (const auto position = hostPlayHead->getPosition())
        {
            const auto playing = position->getIsPlaying();
            if (transportWasPlaying && ! playing)
                engine.requestTransportStop();
            transportWasPlaying = playing;
        }
    SampleEngine::PlaybackSettings playback {};
    for (int pad = 0; pad < padCount; ++pad)
    {
        playback[pad].level = padLevelParameters[pad]->load();
        playback[pad].gated = padModeParameters[pad]->load() >= 0.5f;
        playback[pad].chokeGroup = static_cast<int> (padChokeParameters[pad]->load());
        playback[pad].muted = padMuteParameters[pad]->load() >= 0.5f;
        playback[pad].solo = padSoloParameters[pad]->load() >= 0.5f;
        playback[pad].voiceLimit = static_cast<int> (padVoiceLimitParameters[pad]->load());
        playback[pad].velocityCurve = static_cast<int> (padVelocityCurveParameters[pad]->load());
        playback[pad].coarseSemitones = static_cast<int> (padCoarseTuneParameters[pad]->load());
        playback[pad].fineCents = padFineTuneParameters[pad]->load();
        playback[pad].amplitude = {
            padAmplitudeParameters[pad][0]->load(), padAmplitudeParameters[pad][1]->load(),
            padAmplitudeParameters[pad][2]->load(), padAmplitudeParameters[pad][3]->load()
        };
        playback[pad].filter = {
            static_cast<int> (padFilterParameters[pad][0]->load()),
            padFilterParameters[pad][1]->load(), padFilterParameters[pad][2]->load(),
            padFilterParameters[pad][3]->load(),
            { padFilterParameters[pad][4]->load(), padFilterParameters[pad][5]->load(),
              padFilterParameters[pad][6]->load(), padFilterParameters[pad][7]->load() }
        };
        playback[pad].lfo = {
            static_cast<int> (padLfoParameters[pad][0]->load()),
            static_cast<int> (padLfoParameters[pad][1]->load()),
            padLfoParameters[pad][2]->load(), padLfoParameters[pad][3]->load(),
            static_cast<int> (padLfoParameters[pad][4]->load())
        };
    }
    engine.process (audio, midi, playback);
    // The instrument track's input has already been consumed by the engine.
    // Only editor pad presses belong on HYPERREAL's MIDI output bus.
    midi.clear();
    const auto framesInBlock = audio.getNumSamples();
    const auto noteLength = std::max (1, static_cast<int> (currentSampleRate * 0.1));
    for (int pad = 0; pad < padCount; ++pad)
    {
        const auto note = SampleEngine::firstMidiNote + pad;
        const auto clicks = padClickCounters[pad].load (std::memory_order_relaxed);
        if (clicks != padClickSeen[pad] && framesInBlock > 0)
        {
            padClickSeen[pad] = clicks; // Match the engine's one audition per block.
            if (noteOffCountdown[pad] > 0)
                midi.addEvent (juce::MidiMessage::noteOff (1, note), 0);
            midi.addEvent (juce::MidiMessage::noteOn (1, note, static_cast<juce::uint8> (127)), 0);
            noteOffCountdown[pad] = noteLength;
        }
        if (noteOffCountdown[pad] > 0)
        {
            if (noteOffCountdown[pad] <= framesInBlock)
            {
                midi.addEvent (juce::MidiMessage::noteOff (1, note), noteOffCountdown[pad] - 1);
                noteOffCountdown[pad] = 0;
            }
            else
                noteOffCountdown[pad] -= framesInBlock;
        }
    }
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
    snapshot.getDynamicObject()->setProperty ("muted", padMuteParameters[pad]->load() >= 0.5f);
    snapshot.getDynamicObject()->setProperty ("solo", padSoloParameters[pad]->load() >= 0.5f);
    snapshot.getDynamicObject()->setProperty ("voiceLimit", static_cast<int> (padVoiceLimitParameters[pad]->load()));
    snapshot.getDynamicObject()->setProperty ("velocityCurve", static_cast<int> (padVelocityCurveParameters[pad]->load()));
    snapshot.getDynamicObject()->setProperty ("coarseTune", static_cast<int> (padCoarseTuneParameters[pad]->load()));
    snapshot.getDynamicObject()->setProperty ("fineTune", padFineTuneParameters[pad]->load());
    snapshot.getDynamicObject()->setProperty ("ampAttack", padAmplitudeParameters[pad][0]->load());
    snapshot.getDynamicObject()->setProperty ("ampDecay", padAmplitudeParameters[pad][1]->load());
    snapshot.getDynamicObject()->setProperty ("ampSustain", padAmplitudeParameters[pad][2]->load());
    snapshot.getDynamicObject()->setProperty ("ampRelease", padAmplitudeParameters[pad][3]->load());
    constexpr const char* filterSnapshotNames[] { "filterMode", "filterCutoff", "filterResonance",
                                                  "filterDepth", "filterAttack", "filterDecay",
                                                  "filterSustain", "filterRelease" };
    for (int setting = 0; setting < 8; ++setting)
        snapshot.getDynamicObject()->setProperty (
            filterSnapshotNames[setting], padFilterParameters[pad][setting]->load());
    constexpr const char* lfoSnapshotNames[] { "lfoDestination", "lfoShape", "lfoRate",
                                               "lfoDepth", "lfoPhaseMode" };
    for (int setting = 0; setting < 5; ++setting)
        snapshot.getDynamicObject()->setProperty (
            lfoSnapshotNames[setting], padLfoParameters[pad][setting]->load());
    auto pads = snapshot.getProperty ("pads", {});
    if (auto* items = pads.getArray())
        for (int index = 0; index < std::min (padCount, items->size()); ++index)
            if (auto* item = items->getReference (index).getDynamicObject())
            {
                item->setProperty ("muted", padMuteParameters[index]->load() >= 0.5f);
                item->setProperty ("solo", padSoloParameters[index]->load() >= 0.5f);
            }
    return snapshot;
}

bool HyperrealAudioProcessor::selectPad (int pad) noexcept
{
    if (pad < 0 || pad >= padCount)
        return false;
    selectedPad.store (pad);
    return true;
}

void HyperrealAudioProcessor::requestAudition (int pad) noexcept
{
    if (pad < 0 || pad >= padCount)
        return;
    engine.requestAudition (pad);
    padClickCounters[pad].fetch_add (1, std::memory_order_relaxed);
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
bool HyperrealAudioProcessor::setPadMute (const juce::var& command)
{
    return setPadDiscreteParameter (command, "mute", 1);
}
bool HyperrealAudioProcessor::setPadSolo (const juce::var& command)
{
    return setPadDiscreteParameter (command, "solo", 1);
}
bool HyperrealAudioProcessor::setPadVoiceLimit (const juce::var& command)
{
    return setPadDiscreteParameter (command, "voices", 4, 1);
}
bool HyperrealAudioProcessor::setPadVelocityCurve (const juce::var& command)
{
    return setPadDiscreteParameter (command, "velocity", 2);
}
bool HyperrealAudioProcessor::setPadCoarseTune (const juce::var& command)
{
    return setPadDiscreteParameter (command, "coarse", 24, -24);
}
bool HyperrealAudioProcessor::setPadFineTune (const juce::var& command)
{
    if (! command.isObject()
        || static_cast<int> (command.getProperty ("schemaVersion", 0)) != 6)
        return false;
    const auto pad = command.getProperty ("pad", -1);
    const auto value = command.getProperty ("value", {});
    if (! (pad.isInt() || pad.isInt64() || pad.isDouble())
        || ! (value.isInt() || value.isInt64() || value.isDouble()))
        return false;
    const auto index = static_cast<double> (pad);
    const auto cents = static_cast<double> (value);
    if (! std::isfinite (index) || std::trunc (index) != index
        || index < 0.0 || index >= padCount
        || ! std::isfinite (cents) || cents < -100.0 || cents > 100.0)
        return false;
    const auto id = juce::String::formatted ("pad.%02d.fine.v1", static_cast<int> (index) + 1);
    if (auto* parameter = parameters.getParameter (id))
    {
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (static_cast<float> (cents)));
        parameter->endChangeGesture();
        return true;
    }
    return false;
}
bool HyperrealAudioProcessor::setPadAmplitude (const juce::var& command)
{
    if (! command.isObject()
        || static_cast<int> (command.getProperty ("schemaVersion", 0)) != 8)
        return false;
    const auto padValue = command.getProperty ("pad", {});
    const auto fieldValue = command.getProperty ("field", {});
    const auto parameterValue = command.getProperty ("value", {});
    const auto numeric = [] (const juce::var& value)
    {
        return value.isInt() || value.isInt64() || value.isDouble();
    };
    if (! numeric (padValue) || ! fieldValue.isString() || ! numeric (parameterValue))
        return false;
    const auto pad = static_cast<double> (padValue);
    const auto value = static_cast<double> (parameterValue);
    const auto field = fieldValue.toString();
    if (! std::isfinite (pad) || std::trunc (pad) != pad
        || pad < 0.0 || pad >= padCount || ! std::isfinite (value)
        || value < 0.0 || value > (field == "sustain" ? 1.0 : 10.0)
        || (field != "attack" && field != "decay"
            && field != "sustain" && field != "release"))
        return false;
    const auto id = juce::String::formatted ("pad.%02d.amp.", static_cast<int> (pad) + 1)
                    + field + ".v1";
    if (auto* parameter = parameters.getParameter (id))
    {
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (static_cast<float> (value)));
        parameter->endChangeGesture();
        return true;
    }
    return false;
}
bool HyperrealAudioProcessor::setPadFilter (const juce::var& command)
{
    if (! command.isObject()
        || static_cast<int> (command.getProperty ("schemaVersion", 0)) != 9)
        return false;
    const auto padValue = command.getProperty ("pad", {});
    const auto fieldValue = command.getProperty ("field", {});
    const auto parameterValue = command.getProperty ("value", {});
    const auto numeric = [] (const juce::var& value)
    {
        return value.isInt() || value.isInt64() || value.isDouble();
    };
    if (! numeric (padValue) || ! fieldValue.isString() || ! numeric (parameterValue))
        return false;
    const auto pad = static_cast<double> (padValue);
    const auto value = static_cast<double> (parameterValue);
    const auto field = fieldValue.toString();
    constexpr const char* names[] { "mode", "cutoff", "resonance", "depth",
                                    "attack", "decay", "sustain", "release" };
    constexpr double minimum[] { 0.0, 20.0, 0.5, -48.0, 0.0, 0.0, 0.0, 0.0 };
    constexpr double maximum[] { 3.0, 20000.0, 10.0, 48.0, 10.0, 10.0, 1.0, 10.0 };
    int setting = -1;
    for (int index = 0; index < 8; ++index)
        if (field == names[index])
            setting = index;
    if (! std::isfinite (pad) || std::trunc (pad) != pad
        || pad < 0.0 || pad >= padCount || ! std::isfinite (value)
        || setting < 0 || value < minimum[setting] || value > maximum[setting]
        || (setting == 0 && std::trunc (value) != value))
        return false;
    const auto id = juce::String::formatted ("pad.%02d.filter.", static_cast<int> (pad) + 1)
                    + field + ".v1";
    if (auto* parameter = parameters.getParameter (id))
    {
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (static_cast<float> (value)));
        parameter->endChangeGesture();
        return true;
    }
    return false;
}
bool HyperrealAudioProcessor::setPadLfo (const juce::var& command)
{
    if (! command.isObject()
        || static_cast<int> (command.getProperty ("schemaVersion", 0)) != 10)
        return false;
    const auto padValue = command.getProperty ("pad", {});
    const auto fieldValue = command.getProperty ("field", {});
    const auto parameterValue = command.getProperty ("value", {});
    const auto numeric = [] (const juce::var& value)
    {
        return value.isInt() || value.isInt64() || value.isDouble();
    };
    if (! numeric (padValue) || ! fieldValue.isString() || ! numeric (parameterValue))
        return false;
    const auto pad = static_cast<double> (padValue);
    const auto value = static_cast<double> (parameterValue);
    const auto field = fieldValue.toString();
    constexpr const char* names[] { "destination", "shape", "rate", "depth", "phase" };
    constexpr double minimum[] { 0.0, 0.0, 0.05, 0.0, 0.0 };
    constexpr double maximum[] { 2.0, 3.0, 20.0, 24.0, 1.0 };
    int setting = -1;
    for (int index = 0; index < 5; ++index)
        if (field == names[index])
            setting = index;
    if (! std::isfinite (pad) || std::trunc (pad) != pad
        || pad < 0.0 || pad >= padCount || ! std::isfinite (value)
        || setting < 0 || value < minimum[setting] || value > maximum[setting]
        || ((setting == 0 || setting == 1 || setting == 4) && std::trunc (value) != value))
        return false;
    const auto id = juce::String::formatted ("pad.%02d.lfo.", static_cast<int> (pad) + 1)
                    + field + ".v1";
    if (auto* parameter = parameters.getParameter (id))
    {
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (static_cast<float> (value)));
        parameter->endChangeGesture();
        return true;
    }
    return false;
}
bool HyperrealAudioProcessor::setPadEdit (const juce::var& command)
{
    if (! command.isObject()
        || static_cast<int> (command.getProperty ("schemaVersion", 0)) != 6)
        return false;
    const auto integer = [&command] (const char* key) -> std::optional<int>
    {
        const auto value = command.getProperty (key, {});
        if (! (value.isInt() || value.isInt64() || value.isDouble()))
            return std::nullopt;
        const auto number = static_cast<double> (value);
        if (! std::isfinite (number) || std::trunc (number) != number
            || number < 0.0 || number > std::numeric_limits<int>::max())
            return std::nullopt;
        return static_cast<int> (number);
    };
    const auto pad = integer ("pad");
    const auto start = integer ("startFrame");
    const auto end = integer ("endFrame");
    const auto fadeIn = integer ("fadeInFrames");
    const auto fadeOut = integer ("fadeOutFrames");
    const auto reverse = command.getProperty ("reversed", {});
    const auto revisionValue = command.getProperty ("sampleRevision", -1);
    if (! (revisionValue.isInt() || revisionValue.isInt64() || revisionValue.isDouble()))
        return false;
    const auto revisionNumber = static_cast<double> (revisionValue);
    if (! std::isfinite (revisionNumber) || std::trunc (revisionNumber) != revisionNumber
        || revisionNumber < -1.0 || revisionNumber > 9007199254740991.0)
        return false;
    if (! pad || ! start || ! end || ! fadeIn || ! fadeOut || ! reverse.isBool())
        return false;
    return engine.setPadEdit (*pad, { *start, *end, *fadeIn, *fadeOut,
                                      static_cast<bool> (reverse) },
                              static_cast<juce::int64> (revisionNumber));
}
std::array<float, 26> HyperrealAudioProcessor::readPadSettings (int pad) const
{
    return { padLevelParameters[pad]->load(), padModeParameters[pad]->load(),
             padChokeParameters[pad]->load(), padMuteParameters[pad]->load(),
             padSoloParameters[pad]->load(), padVoiceLimitParameters[pad]->load(),
             padVelocityCurveParameters[pad]->load(),
             padCoarseTuneParameters[pad]->load(), padFineTuneParameters[pad]->load(),
             padAmplitudeParameters[pad][0]->load(), padAmplitudeParameters[pad][1]->load(),
             padAmplitudeParameters[pad][2]->load(), padAmplitudeParameters[pad][3]->load(),
             padFilterParameters[pad][0]->load(), padFilterParameters[pad][1]->load(),
             padFilterParameters[pad][2]->load(), padFilterParameters[pad][3]->load(),
             padFilterParameters[pad][4]->load(), padFilterParameters[pad][5]->load(),
             padFilterParameters[pad][6]->load(), padFilterParameters[pad][7]->load(),
             padLfoParameters[pad][0]->load(), padLfoParameters[pad][1]->load(),
             padLfoParameters[pad][2]->load(), padLfoParameters[pad][3]->load(),
             padLfoParameters[pad][4]->load() };
}

void HyperrealAudioProcessor::applyPadSettings (int pad, const std::array<float, 26>& values)
{
    constexpr const char* names[] { "level", "mode", "choke", "mute", "solo", "voices", "velocity", "coarse", "fine",
                                    "amp.attack", "amp.decay", "amp.sustain", "amp.release",
                                    "filter.mode", "filter.cutoff", "filter.resonance", "filter.depth",
                                    "filter.attack", "filter.decay", "filter.sustain", "filter.release",
                                    "lfo.destination", "lfo.shape", "lfo.rate", "lfo.depth", "lfo.phase" };
    for (int index = 0; index < 26; ++index)
    {
        const auto id = juce::String::formatted ("pad.%02d.", pad + 1) + names[index] + ".v1";
        if (auto* parameter = parameters.getParameter (id))
        {
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (values[index]));
            parameter->endChangeGesture();
        }
    }
}

bool HyperrealAudioProcessor::copyPad (int pad)
{
    if (pad < 0 || pad >= padCount || ! engine.copyPad (pad))
        return false;
    clipboardSettings = readPadSettings (pad);
    clipboardSettingsReady = true;
    return true;
}

bool HyperrealAudioProcessor::pastePad (int pad)
{
    if (pad < 0 || pad >= padCount || ! clipboardSettingsReady || ! engine.pastePad (pad))
        return false;
    applyPadSettings (pad, clipboardSettings);
    return true;
}

bool HyperrealAudioProcessor::swapPads (int first, int second)
{
    if (first < 0 || first >= padCount || second < 0 || second >= padCount || first == second)
        return false;
    const auto firstSettings = readPadSettings (first);
    const auto secondSettings = readPadSettings (second);
    if (! engine.swapPads (first, second))
        return false;
    applyPadSettings (first, secondSettings);
    applyPadSettings (second, firstSettings);
    return true;
}

bool HyperrealAudioProcessor::clearPad (int pad)
{
    if (pad < 0 || pad >= padCount || ! engine.clearPad (pad))
        return false;
    applyPadSettings (pad, { 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 4.0f, 1.0f, 0.0f, 0.0f,
                             0.0f, 0.0f, 1.0f, 0.0f,
                             0.0f, 20000.0f, 0.707f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f,
                             0.0f, 0.0f, 1.0f, 0.0f, 0.0f });
    return true;
}
bool HyperrealAudioProcessor::setPadDiscreteParameter (const juce::var& command,
                                                        const char* name, int maximum, int minimum)
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
        || pad < 0.0 || pad >= padCount || value < minimum || value > maximum)
        return false;
    const auto id = juce::String::formatted ("pad.%02d.", static_cast<int> (pad) + 1)
                    + name + ".v1";
    if (auto* parameter = parameters.getParameter (id))
    {
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (static_cast<float> (value)));
        parameter->endChangeGesture();
        return true;
    }
    return false;
}
bool HyperrealAudioProcessor::hasEditor() const { return true; }
const juce::String HyperrealAudioProcessor::getName() const { return JucePlugin_Name; }
bool HyperrealAudioProcessor::acceptsMidi() const { return true; }
bool HyperrealAudioProcessor::producesMidi() const { return true; }
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
    const auto phase10 = std::memcmp (magic, stateMagic, sizeof (magic)) == 0
                        && version == stateSchemaVersion;
    const auto phase9 = std::memcmp (magic, phase9Magic, sizeof (magic)) == 0
                        && version == 9;
    const auto phase8 = std::memcmp (magic, phase8Magic, sizeof (magic)) == 0
                        && version == 8;
    const auto phase7 = std::memcmp (magic, phase7Magic, sizeof (magic)) == 0
                        && version == 7;
    const auto phase6 = std::memcmp (magic, phase6Magic, sizeof (magic)) == 0
                        && version == 6;
    const auto phase5 = std::memcmp (magic, phase5Magic, sizeof (magic)) == 0
                        && version == 5;
    const auto phase4 = std::memcmp (magic, phase4Magic, sizeof (magic)) == 0
                        && version == 4;
    const auto phase3 = std::memcmp (magic, phase3Magic, sizeof (magic)) == 0
                        && version == 3;
    const auto phase2 = std::memcmp (magic, phase2Magic, sizeof (magic)) == 0
                        && version == 2;
    if (! phase10 && ! phase9 && ! phase8 && ! phase7 && ! phase6 && ! phase5 && ! phase4 && ! phase3 && ! phase2)
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
        || ! ((phase10 || phase9 || phase8 || phase7 || phase6 || phase5 || phase4 || phase3)
                  ? engine.restoreSampleState (input, phase10 || phase9 || phase8 || phase7)
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

    for (int pad = 1; pad <= padCount; ++pad)
    {
        const auto muteId = juce::String::formatted ("pad.%02d.mute.v1", pad);
        const auto soloId = juce::String::formatted ("pad.%02d.solo.v1", pad);
        layout.push_back (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID { muteId, 1 },
            juce::String::formatted ("Pad %02d mute", pad), false));
        layout.push_back (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID { soloId, 1 },
            juce::String::formatted ("Pad %02d solo", pad), false));
    }

    for (int pad = 1; pad <= padCount; ++pad)
    {
        const auto voicesId = juce::String::formatted ("pad.%02d.voices.v1", pad);
        const auto velocityId = juce::String::formatted ("pad.%02d.velocity.v1", pad);
        layout.push_back (std::make_unique<juce::AudioParameterInt> (
            juce::ParameterID { voicesId, 1 },
            juce::String::formatted ("Pad %02d voices", pad), 1, 4, 4));
        layout.push_back (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { velocityId, 1 },
            juce::String::formatted ("Pad %02d velocity curve", pad),
            juce::StringArray { "Soft", "Linear", "Hard" }, 1));
    }

    for (int pad = 1; pad <= padCount; ++pad)
    {
        const auto coarseId = juce::String::formatted ("pad.%02d.coarse.v1", pad);
        const auto fineId = juce::String::formatted ("pad.%02d.fine.v1", pad);
        layout.push_back (std::make_unique<juce::AudioParameterInt> (
            juce::ParameterID { coarseId, 1 },
            juce::String::formatted ("Pad %02d coarse tune", pad), -24, 24, 0));
        layout.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { fineId, 1 },
            juce::String::formatted ("Pad %02d fine tune", pad),
            juce::NormalisableRange<float> (-100.0f, 100.0f, 0.1f), 0.0f));
    }

    for (int pad = 1; pad <= padCount; ++pad)
    {
        constexpr const char* names[] { "attack", "decay", "sustain", "release" };
        constexpr float defaults[] { 0.0f, 0.0f, 1.0f, 0.0f };
        for (int stage = 0; stage < 4; ++stage)
        {
            const auto id = juce::String::formatted ("pad.%02d.amp.", pad)
                            + names[stage] + ".v1";
            const auto name = juce::String::formatted ("Pad %02d amplitude ", pad)
                              + names[stage] + (stage == 2 ? " level" : " seconds");
            layout.push_back (std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { id, 1 }, name,
                juce::NormalisableRange<float> (0.0f, stage == 2 ? 1.0f : 10.0f,
                                                 0.001f), defaults[stage],
                displayAs (stage == 2 ? "" : "s", 3)));
        }
    }

    auto cutoffRange = juce::NormalisableRange<float> (20.0f, 20000.0f, 1.0f);
    cutoffRange.setSkewForCentre (1000.0f);
    for (int pad = 1; pad <= padCount; ++pad)
    {
        const auto prefix = juce::String::formatted ("pad.%02d.filter.", pad);
        const auto label = juce::String::formatted ("Pad %02d filter ", pad);
        layout.push_back (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { prefix + "mode.v1", 1 }, label + "mode",
            juce::StringArray { "Off", "Low-pass", "High-pass", "Band-pass" }, 0));
        layout.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { prefix + "cutoff.v1", 1 }, label + "cutoff Hz",
            cutoffRange, 20000.0f, displayAs ("Hz", 0)));
        layout.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { prefix + "resonance.v1", 1 }, label + "resonance Q",
            juce::NormalisableRange<float> (0.5f, 10.0f, 0.01f), 0.707f,
            displayAs ("", 2)));
        layout.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { prefix + "depth.v1", 1 }, label + "envelope depth semitones",
            juce::NormalisableRange<float> (-48.0f, 48.0f, 0.1f), 0.0f,
            displayAs ("st", 1)));
        constexpr const char* envelopeNames[] { "attack", "decay", "sustain", "release" };
        constexpr float defaults[] { 0.0f, 0.0f, 1.0f, 0.0f };
        for (int stage = 0; stage < 4; ++stage)
            layout.push_back (std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { prefix + envelopeNames[stage] + ".v1", 1 },
                label + "envelope " + envelopeNames[stage]
                    + (stage == 2 ? " level" : " seconds"),
                juce::NormalisableRange<float> (0.0f, stage == 2 ? 1.0f : 10.0f,
                                                 0.001f), defaults[stage],
                displayAs (stage == 2 ? "" : "s", 3)));
    }

    auto lfoRateRange = juce::NormalisableRange<float> (0.05f, 20.0f, 0.001f);
    lfoRateRange.setSkewForCentre (2.0f);
    for (int pad = 1; pad <= padCount; ++pad)
    {
        const auto prefix = juce::String::formatted ("pad.%02d.lfo.", pad);
        const auto label = juce::String::formatted ("Pad %02d LFO ", pad);
        layout.push_back (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { prefix + "destination.v1", 1 }, label + "destination",
            juce::StringArray { "Off", "Pitch", "Cutoff" }, 0));
        layout.push_back (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { prefix + "shape.v1", 1 }, label + "shape",
            juce::StringArray { "Sine", "Triangle", "Square", "Saw" }, 0));
        layout.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { prefix + "rate.v1", 1 }, label + "rate Hz",
            lfoRateRange, 1.0f, displayAs ("Hz", 2)));
        layout.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { prefix + "depth.v1", 1 }, label + "depth semitones",
            juce::NormalisableRange<float> (0.0f, 24.0f, 0.1f), 0.0f,
            displayAs ("st", 1)));
        layout.push_back (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { prefix + "phase.v1", 1 }, label + "phase",
            juce::StringArray { "Reset", "Free-running" }, 0));
    }

    return { layout.begin(), layout.end() };
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new HyperrealAudioProcessor();
}
