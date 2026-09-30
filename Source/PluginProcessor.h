#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include "DSP.h"

class SmadderProcessor : public juce::AudioProcessor
{
public:
    SmadderProcessor();
    ~SmadderProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Smadder"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}
    void loadPreset (int index); // used by the editor; always applies

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    static juce::StringArray delayDivisionNames();

    juce::AudioProcessorValueTreeState apvts;

private:
    struct Params
    {
        std::atomic<float>* drive; std::atomic<float>* bits; std::atomic<float>* downsample;
        std::atomic<float>* filterType; std::atomic<float>* cutoff; std::atomic<float>* reso;
        std::atomic<float>* lfoRate; std::atomic<float>* lfoDepth;
        std::atomic<float>* wow; std::atomic<float>* noise;
        std::atomic<float>* delaySync; std::atomic<float>* delayMs; std::atomic<float>* delayDiv;
        std::atomic<float>* feedback; std::atomic<float>* delayTone; std::atomic<float>* delayMix;
        std::atomic<float>* pingPong;
        std::atomic<float>* mix; std::atomic<float>* output;
    } p;

    double sr = 44100.0;
    int currentProgram = 0;

    smadder::Engine engine;
    juce::AudioBuffer<float> dryBuffer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SmadderProcessor)
};
