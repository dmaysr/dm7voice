#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "Voicing.h"

namespace ParamID
{
    inline constexpr const char* input     = "input";
    inline constexpr const char* proximity = "proximity";
    inline constexpr const char* presence  = "presence";
    inline constexpr const char* bassCut   = "basscut";
    inline constexpr const char* character = "character";
    inline constexpr const char* mix       = "mix";
    inline constexpr const char* output    = "output";
}

class DM7VoiceProcessor : public juce::AudioProcessor
{
public:
    DM7VoiceProcessor();
    ~DM7VoiceProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    using AudioProcessor::processBlock;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Current response for the editor's graph (uses target values, not smoothed)
    dm7::Chain getDisplayChain (double& fsOut, double& mixOut) const;

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void processSaturation (juce::dsp::AudioBlock<float>& block, float drive);

    std::atomic<float>* pInput = nullptr;
    std::atomic<float>* pProximity = nullptr;
    std::atomic<float>* pPresence = nullptr;
    std::atomic<float>* pBassCut = nullptr;
    std::atomic<float>* pCharacter = nullptr;
    std::atomic<float>* pMix = nullptr;
    std::atomic<float>* pOutput = nullptr;

    double fs = 44100.0;
    static constexpr int maxChannels = 2;
    static constexpr int subBlock = 32;

    std::array<std::array<dm7::Biquad, dm7::numStages>, maxChannels> filters;
    dm7::Chain chain;
    juce::SmoothedValue<double> smProximity, smPresence, smBassCut;
    juce::SmoothedValue<float> smDrive;
    bool needsDesign = true;

    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
    juce::dsp::DryWetMixer<float> dryWet { 64 };
    juce::dsp::Gain<float> inputGain, outputGain;
    std::array<double, maxChannels> dcX {}, dcY {};
    double dcR = 0.999;

    int currentProgram = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DM7VoiceProcessor)
};
