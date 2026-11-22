#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "Params.h"
#include "Presets.h"

class NosferaProcessor : public juce::AudioProcessor
{
public:
    NosferaProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Nosfera"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }

    // Os 150 presets aparecem como "programas" no FL Studio.
    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Usado pela interface.
    void setParameterFromUi (const juce::String& id, const juce::var& value);
    juce::var getParameterForUi (const juce::String& id) const;
    void copyScope (float* dest, int n);
    float getLevel() const { return level.load(); }

    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboard;           // notas tocadas no teclado da interface
    std::atomic<float> uiBend { 0 }, uiMod { 0 };
    std::atomic<bool> uiPanic { false };
    std::array<std::atomic<uint8_t>, 128> noteState {};
    std::atomic<int> stateVersion { 0 };        // muda quando o host troca preset ou estado
    juce::String presetName;

private:
    void handleMidi (const juce::MidiMessage&);

    nosfera::Synth synth;
    std::vector<float> tmpL, tmpR;
    float hostBend = 0, hostMod = 0;
    int currentProgram = 0;
    std::atomic<float> level { 0 };
    juce::SpinLock scopeLock;
    std::array<float, 1024> scope {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NosferaProcessor)
};
