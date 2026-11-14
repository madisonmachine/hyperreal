#include "PluginEditor.h"
#include <BinaryData.h>
#include <cstring>

namespace
{
std::vector<std::byte> copyResource (const char* data, int size)
{
    std::vector<std::byte> bytes (static_cast<size_t> (size));
    std::memcpy (bytes.data(), data, static_cast<size_t> (size));
    return bytes;
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
                   .withNativeFunction ("chooseSample", [this] (const auto&, auto complete)
                   {
                       openSampleChooser();
                       complete (true);
                   })
                   .withNativeFunction ("setMasterGain", [this] (const auto& arguments, auto complete)
                   {
                       complete (arguments.size() == 1 && processor.setMasterGain (arguments[0]));
                   })                   .withNativeFunction ("audition", [this] (const auto&, auto complete)
                   {
                       processor.requestAudition();
                       complete (true);
                   })
                   .withResourceProvider ([] (const auto& path) { return getResource (path); }))
{
    addAndMakeVisible (browser);
    setSize (820, 700);
    browser.goToURL (juce::WebBrowserComponent::getResourceProviderRoot());
}

void HyperrealAudioProcessorEditor::resized()
{
    browser.setBounds (getLocalBounds());
}

void HyperrealAudioProcessorEditor::openSampleChooser()
{
    if (chooserOpen)
        return;
    chooserOpen = true;
    chooser = std::make_unique<juce::FileChooser> (
        "Load a sample", juce::File {}, "*.wav;*.aif;*.aiff;*.flac");

    chooser->launchAsync (juce::FileBrowserComponent::openMode
                             | juce::FileBrowserComponent::canSelectFiles,
                         [safeEditor = juce::Component::SafePointer<HyperrealAudioProcessorEditor> (this)]
                         (const juce::FileChooser& finished)
                         {
                             if (auto* editor = safeEditor.getComponent())
                             {
                                 editor->chooserOpen = false;
                                 const auto selected = finished.getResult();
                                 if (selected.existsAsFile())
                                     editor->processor.requestSampleLoad (selected);
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
