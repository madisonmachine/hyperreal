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
    void openSampleChooser (int targetPad);
    int beginDrop (const juce::var& command);
    bool appendDrop (const juce::var& command);
    bool finishDrop (const juce::var& command);
    bool cancelDrop (const juce::var& command);

    struct DropTransfer
    {
        int token = 0;
        int pad = 0;
        size_t expected = 0;
        size_t received = 0;
        juce::String name;
        juce::MemoryBlock data;
    };

    HyperrealAudioProcessor& processor;
    std::unique_ptr<juce::FileChooser> chooser;
    bool chooserOpen = false;
    int nextDropToken = 1;
    std::unique_ptr<DropTransfer> dropTransfer;
    juce::WebBrowserComponent browser;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HyperrealAudioProcessorEditor)
};
