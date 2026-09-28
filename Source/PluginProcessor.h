#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "ReverbEngine.h"
#include "Presets.h"

namespace IDs
{
    inline constexpr const char* type      = "type";
    inline constexpr const char* predelay  = "predelay";
    inline constexpr const char* decay     = "decay";
    inline constexpr const char* size      = "size";
    inline constexpr const char* damping   = "damping";
    inline constexpr const char* diffusion = "diffusion";
    inline constexpr const char* early     = "early";
    inline constexpr const char* mod       = "mod";
    inline constexpr const char* lowcut    = "lowcut";
    inline constexpr const char* highcut   = "highcut";
    inline constexpr const char* width     = "width";
    inline constexpr const char* warmth    = "warmth";
    inline constexpr const char* mix       = "mix";
    inline constexpr const char* output    = "output";
}

class EchoVaultProcessor : public juce::AudioProcessor
{
public:
    EchoVaultProcessor();
    ~EchoVaultProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 13.0; }

    int getNumPrograms() override { return ev::kNumPresets; }
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

private:
    ev::ReverbEngine engine;
    juce::AudioBuffer<float> work;
    int currentProgram = 0;

    struct Raw { std::atomic<float>* p = nullptr; float get() const { return p->load(); } };
    Raw pType, pPre, pDecay, pSize, pDamp, pDiff, pEarly, pMod, pLow, pHigh, pWidth, pWarm, pMix, pOut;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EchoVaultProcessor)
};
