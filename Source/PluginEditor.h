#pragma once
#include "PluginProcessor.h"

// A interface é a mesma página HTML do protótipo, rodando dentro do plugin (WebView).
class NosferaEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit NosferaEditor (NosferaProcessor&);
    ~NosferaEditor() override = default;
    void resized() override { web.setBounds (getLocalBounds()); }
    void paint (juce::Graphics& g) override { g.fillAll (juce::Colour (0xffeeedea)); }

private:
    juce::WebBrowserComponent::Options makeOptions();
    std::optional<juce::WebBrowserComponent::Resource> getResource (const juce::String& url);
    void timerCallback() override;
    void sendState();

    NosferaProcessor& proc;
    juce::WebBrowserComponent web;
    bool ready = false;
    int seenStateVersion = -1, tick = 0;
    std::array<uint8_t, 128> lastNotes {};
    std::vector<float> lastValues;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NosferaEditor)
};
