#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"

class HyperrealAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                             public juce::FileDragAndDropTarget
{
public:
    explicit HyperrealAudioProcessorEditor (HyperrealAudioProcessor& processor);
    static bool isSupported();
    void resized() override;
    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;

private:
    static std::optional<juce::WebBrowserComponent::Resource> getResource (const juce::String& path);
    void openSampleChooser();

    HyperrealAudioProcessor& processor;
    std::unique_ptr<juce::FileChooser> chooser;
    bool chooserOpen = false;
    juce::WebBrowserComponent browser;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HyperrealAudioProcessorEditor)
};
