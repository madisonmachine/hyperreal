#include "PluginEditor.h"
#include <BinaryData.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace
{
std::vector<std::byte> copyResource (const char* data, int size)
{
    std::vector<std::byte> bytes (static_cast<size_t> (size));
    std::memcpy (bytes.data(), data, static_cast<size_t> (size));
    return bytes;
}

std::optional<int> padFromCommand (const juce::var& command, int version = 2,
                                   const char* property = "pad")
{
    if (! command.isObject()
        || static_cast<int> (command.getProperty ("schemaVersion", 0)) != version)
        return std::nullopt;
    const auto value = command.getProperty (property, -1);
    if (! (value.isInt() || value.isInt64() || value.isDouble()))
        return std::nullopt;
    const auto number = static_cast<double> (value);
    if (! std::isfinite (number) || std::trunc (number) != number
        || number < 0.0 || number >= SampleEngine::padCount)
        return std::nullopt;
    const auto pad = static_cast<int> (number);
    return pad;
}

bool confirmedCommand (const juce::var& command)
{
    const auto confirmed = command.getProperty ("confirmed", false);
    return confirmed.isBool() && static_cast<bool> (confirmed);
}
}

bool HyperrealAudioProcessorEditor::isSupported()
{
    return juce::WebBrowserComponent::areOptionsSupported (
        juce::WebBrowserComponent::Options {}
            .withBackend (juce::WebBrowserComponent::Options::Backend::webview2)
            .withWinWebView2Options (
                juce::WebBrowserComponent::Options::WinWebView2 {}
                    .withUserDataFolder (juce::File::getSpecialLocation (
                        juce::File::SpecialLocationType::tempDirectory)
                        .getChildFile ("HYPERREAL-WebView2"))));
}

HyperrealAudioProcessorEditor::HyperrealAudioProcessorEditor (HyperrealAudioProcessor& owner)
    : juce::AudioProcessorEditor (&owner),
      processor (owner),
      browser (juce::WebBrowserComponent::Options {}
                   .withBackend (juce::WebBrowserComponent::Options::Backend::webview2)
                   .withWinWebView2Options (
                       juce::WebBrowserComponent::Options::WinWebView2 {}
                           .withUserDataFolder (juce::File::getSpecialLocation (
                               juce::File::SpecialLocationType::tempDirectory)
                               .getChildFile ("HYPERREAL-WebView2")))
                   .withNativeIntegrationEnabled()
                   .withNativeFunction ("getSnapshot", [this] (const auto&, auto complete)
                   {
                       complete (processor.getSnapshot());
                   })
                   .withNativeFunction ("getWaveform", [this] (const auto& arguments, auto complete)
                   {
                       const auto pad = arguments.size() == 1
                                          ? padFromCommand (arguments[0], 6) : std::nullopt;
                       const auto level = pad.has_value()
                                          ? arguments[0].getProperty ("level", -1) : juce::var (-1);
                       complete (pad.has_value() && level.isInt()
                                 ? processor.getWaveform (*pad, static_cast<int> (level))
                                 : juce::var {});
                   })
                   .withNativeFunction ("chooseSample", [this] (const auto& arguments, auto complete)
                   {
                       const auto pad = arguments.size() == 1
                                          ? padFromCommand (arguments[0]) : std::nullopt;
                       if (pad.has_value())
                           openSampleChooser (*pad);
                       complete (pad.has_value());
                   })
                   .withNativeFunction ("beginDrop", [this] (const auto& arguments, auto complete)
                   {
                       complete (arguments.size() == 1 ? beginDrop (arguments[0]) : 0);
                   })
                   .withNativeFunction ("appendDrop", [this] (const auto& arguments, auto complete)
                   {
                       complete (arguments.size() == 1 && appendDrop (arguments[0]));
                   })
                   .withNativeFunction ("finishDrop", [this] (const auto& arguments, auto complete)
                   {
                       complete (arguments.size() == 1 && finishDrop (arguments[0]));
                   })
                   .withNativeFunction ("cancelDrop", [this] (const auto& arguments, auto complete)
                   {
                       complete (arguments.size() == 1 && cancelDrop (arguments[0]));
                   })
                   .withNativeFunction ("setMasterGain", [this] (const auto& arguments, auto complete)
                   {
                       complete (arguments.size() == 1 && processor.setMasterGain (arguments[0]));
                   })
                   .withNativeFunction ("setPadMode", [this] (const auto& arguments, auto complete)
                   {
                       complete (arguments.size() == 1 && processor.setPadMode (arguments[0]));
                   })
                   .withNativeFunction ("setChokeGroup", [this] (const auto& arguments, auto complete)
                   {
                       complete (arguments.size() == 1 && processor.setPadChokeGroup (arguments[0]));
                   })
                   .withNativeFunction ("setPadMute", [this] (const auto& arguments, auto complete)
                   {
                       complete (arguments.size() == 1 && processor.setPadMute (arguments[0]));
                   })
                   .withNativeFunction ("setPadSolo", [this] (const auto& arguments, auto complete)
                   {
                       complete (arguments.size() == 1 && processor.setPadSolo (arguments[0]));
                   })
                   .withNativeFunction ("setVoiceLimit", [this] (const auto& arguments, auto complete)
                   {
                       complete (arguments.size() == 1 && processor.setPadVoiceLimit (arguments[0]));
                   })
                   .withNativeFunction ("setVelocityCurve", [this] (const auto& arguments, auto complete)
                   {
                       complete (arguments.size() == 1 && processor.setPadVelocityCurve (arguments[0]));
                   })
                   .withNativeFunction ("setPadEdit", [this] (const auto& arguments, auto complete)
                   {
                       complete (arguments.size() == 1 && processor.setPadEdit (arguments[0]));
                   })
                   .withNativeFunction ("setCoarseTune", [this] (const auto& arguments, auto complete)
                   {
                       complete (arguments.size() == 1 && processor.setPadCoarseTune (arguments[0]));
                   })
                   .withNativeFunction ("setFineTune", [this] (const auto& arguments, auto complete)
                   {
                       complete (arguments.size() == 1 && processor.setPadFineTune (arguments[0]));
                   })
                   .withNativeFunction ("setPadAmplitude", [this] (const auto& arguments, auto complete)
                   {
                       complete (arguments.size() == 1 && processor.setPadAmplitude (arguments[0]));
                   })
                   .withNativeFunction ("selectPad", [this] (const auto& arguments, auto complete)
                   {
                       const auto pad = arguments.size() == 1
                                          ? padFromCommand (arguments[0]) : std::nullopt;
                       complete (pad.has_value() && processor.selectPad (*pad));
                   })
                   .withNativeFunction ("cancelLoad", [this] (const auto& arguments, auto complete)
                   {
                       const auto pad = arguments.size() == 1
                                          ? padFromCommand (arguments[0]) : std::nullopt;
                       if (pad.has_value())
                           processor.cancelSampleLoad (*pad);
                       complete (pad.has_value());
                   })
                   .withNativeFunction ("audition", [this] (const auto& arguments, auto complete)
                   {
                       const auto pad = arguments.size() == 1
                                          ? padFromCommand (arguments[0]) : std::nullopt;
                       if (pad.has_value())
                           processor.requestAudition (*pad);
                       complete (pad.has_value());
                   })
                   .withNativeFunction ("copyPad", [this] (const auto& arguments, auto complete)
                   {
                       const auto pad = arguments.size() == 1
                                          ? padFromCommand (arguments[0], 4) : std::nullopt;
                       complete (pad.has_value() && processor.copyPad (*pad));
                   })
                   .withNativeFunction ("pastePad", [this] (const auto& arguments, auto complete)
                   {
                       const auto pad = arguments.size() == 1
                                          ? padFromCommand (arguments[0], 4) : std::nullopt;
                       complete (pad.has_value() && confirmedCommand (arguments[0])
                                 && processor.pastePad (*pad));
                   })
                   .withNativeFunction ("swapPads", [this] (const auto& arguments, auto complete)
                   {
                       const auto first = arguments.size() == 1
                                              ? padFromCommand (arguments[0], 4) : std::nullopt;
                       const auto second = arguments.size() == 1
                                               ? padFromCommand (arguments[0], 4, "otherPad") : std::nullopt;
                       complete (first.has_value() && second.has_value()
                                 && confirmedCommand (arguments[0])
                                 && processor.swapPads (*first, *second));
                   })
                   .withNativeFunction ("clearPad", [this] (const auto& arguments, auto complete)
                   {
                       const auto pad = arguments.size() == 1
                                          ? padFromCommand (arguments[0], 4) : std::nullopt;
                       complete (pad.has_value() && confirmedCommand (arguments[0])
                                 && processor.clearPad (*pad));
                   })
                   .withResourceProvider ([] (const auto& path) { return getResource (path); }))
{
    addAndMakeVisible (browser);
    setSize (820, 900);
    browser.goToURL (juce::WebBrowserComponent::getResourceProviderRoot());
}

void HyperrealAudioProcessorEditor::resized()
{
    browser.setBounds (getLocalBounds());
}

bool HyperrealAudioProcessorEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    return files.size() == 1
           && juce::File (files[0]).hasFileExtension (".wav;.aif;.aiff;.flac");
}

void HyperrealAudioProcessorEditor::filesDropped (const juce::StringArray& files, int x, int y)
{
    if (! isInterestedInFileDrag (files))
        return;

    const auto file = juce::File (files[0]);
    const auto fallbackPad = processor.getSelectedPad();
    const auto width = std::max (1, browser.getWidth());
    const auto height = std::max (1, browser.getHeight());
    const auto script = "(() => { const element = document.elementFromPoint("
        + juce::String (x) + " * window.innerWidth / " + juce::String (width) + ", "
        + juce::String (y) + " * window.innerHeight / " + juce::String (height)
        + "); const pad = element?.closest('[data-pad-index]'); return pad ? Number(pad.dataset.padIndex) : -1; })()";
    browser.evaluateJavascript (script,
        [safeEditor = juce::Component::SafePointer<HyperrealAudioProcessorEditor> (this),
         file, fallbackPad] (const auto& result)
        {
            if (auto* editor = safeEditor.getComponent())
            {
                auto target = fallbackPad;
                if (const auto* value = result.getResult())
                    if (value->isInt() || value->isInt64() || value->isDouble())
                    {
                        const auto candidate = static_cast<int> (*value);
                        if (candidate >= 0 && candidate < SampleEngine::padCount)
                            target = candidate;
                    }
                editor->processor.requestSampleLoad (target, file);
            }
        });
}

int HyperrealAudioProcessorEditor::beginDrop (const juce::var& command)
{
    const auto pad = padFromCommand (command, 5);
    const auto rawName = command.getProperty ("name", {});
    const auto rawSize = command.getProperty ("size", 0);
    if (! pad.has_value() || ! rawName.isString()
        || ! (rawSize.isInt() || rawSize.isInt64() || rawSize.isDouble()))
        return 0;
    const auto size = static_cast<double> (rawSize);
    const auto name = juce::File (rawName.toString()).getFileName();
    if (! std::isfinite (size) || std::trunc (size) != size
        || size < 1.0 || size > 128.0 * 1024.0 * 1024.0
        || name.isEmpty() || name.length() > 255
        || ! juce::File (name).hasFileExtension (".wav;.aif;.aiff;.flac"))
        return 0;
    auto transfer = std::make_unique<DropTransfer>();
    transfer->token = nextDropToken;
    nextDropToken = nextDropToken == std::numeric_limits<int>::max() ? 1 : nextDropToken + 1;
    transfer->pad = *pad;
    transfer->name = name;
    transfer->expected = static_cast<size_t> (size);
    transfer->data.setSize (transfer->expected, false);
    dropTransfer = std::move (transfer);
    return dropTransfer->token;
}

bool HyperrealAudioProcessorEditor::appendDrop (const juce::var& command)
{
    if (! command.isObject() || dropTransfer == nullptr
        || static_cast<int> (command.getProperty ("schemaVersion", 0)) != 5
        || static_cast<int> (command.getProperty ("token", 0)) != dropTransfer->token)
        return false;
    const auto chunk = command.getProperty ("chunk", {});
    if (! chunk.isString() || chunk.toString().length() > 87384)
        return false;
    juce::MemoryOutputStream decoded;
    if (! juce::Base64::convertFromBase64 (decoded, chunk.toString())
        || decoded.getDataSize() < 1 || decoded.getDataSize() > 65536
        || decoded.getDataSize() > dropTransfer->expected - dropTransfer->received)
        return false;
    std::memcpy (static_cast<char*> (dropTransfer->data.getData()) + dropTransfer->received,
                 decoded.getData(), decoded.getDataSize());
    dropTransfer->received += decoded.getDataSize();
    return true;
}

bool HyperrealAudioProcessorEditor::finishDrop (const juce::var& command)
{
    if (! command.isObject() || dropTransfer == nullptr
        || static_cast<int> (command.getProperty ("schemaVersion", 0)) != 5
        || static_cast<int> (command.getProperty ("token", 0)) != dropTransfer->token
        || dropTransfer->received != dropTransfer->expected)
        return false;
    auto transfer = std::move (dropTransfer);
    processor.requestSampleLoadData (transfer->pad, transfer->name, std::move (transfer->data));
    return true;
}

bool HyperrealAudioProcessorEditor::cancelDrop (const juce::var& command)
{
    if (! command.isObject() || dropTransfer == nullptr
        || static_cast<int> (command.getProperty ("schemaVersion", 0)) != 5
        || static_cast<int> (command.getProperty ("token", 0)) != dropTransfer->token)
        return false;
    dropTransfer.reset();
    return true;
}

void HyperrealAudioProcessorEditor::openSampleChooser (int targetPad)
{
    if (chooserOpen)
        return;
    chooserOpen = true;
    chooser = std::make_unique<juce::FileChooser> (
        "Load a sample", juce::File {}, "*.wav;*.aif;*.aiff;*.flac");

    chooser->launchAsync (juce::FileBrowserComponent::openMode
                             | juce::FileBrowserComponent::canSelectFiles,
                         [safeEditor = juce::Component::SafePointer<HyperrealAudioProcessorEditor> (this),
                          targetPad]
                         (const juce::FileChooser& finished)
                         {
                             if (auto* editor = safeEditor.getComponent())
                             {
                                 editor->chooserOpen = false;
                                 const auto selected = finished.getResult();
                                 if (selected.existsAsFile())
                                     editor->processor.requestSampleLoad (targetPad, selected);
                             }
                         });
}

std::optional<juce::WebBrowserComponent::Resource>
HyperrealAudioProcessorEditor::getResource (const juce::String& path)
{
    if (path == "/" || path == "/index.html")
        return juce::WebBrowserComponent::Resource {
            copyResource (BinaryData::index_html, BinaryData::index_htmlSize), "text/html" };

    if (path == "/assets/app.js")
        return juce::WebBrowserComponent::Resource {
            copyResource (BinaryData::app_js, BinaryData::app_jsSize), "text/javascript" };

    if (path == "/assets/style.css")
        return juce::WebBrowserComponent::Resource {
            copyResource (BinaryData::style_css, BinaryData::style_cssSize), "text/css" };

    return std::nullopt;
}
