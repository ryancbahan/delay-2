/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

#define MAX_DELAY_TIME 2

//==============================================================================
/**
*/
class DelaytutorialAudioProcessor  : public juce::AudioProcessor
{
public:
    //==============================================================================
    DelaytutorialAudioProcessor();
    ~DelaytutorialAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;
    
    float lin_interp(float sample_x, float sample_x1, float inPhase);

private:
    
    juce::AudioParameterFloat* mDryWetParameter;
    juce::AudioParameterFloat* mFeedbackParameter;
    juce::AudioParameterFloat* mDelayTimeParameter;
    juce::AudioParameterFloat* mLfoRateParameter;
    juce::AudioParameterFloat* mLfoDepthParameter;
    juce::AudioParameterFloat* mLfoPhaseParameter;
    
    float mLfoPhase;
    
    float mDelayTimeSmooth;
    float mLfoDepthSmooth;
    float mStereoOffsetSmooth;
    
    float* mCircularBufferLeft;
    float* mCircularBufferRight;
    
    int mCircularBufferWriteHead;
    int mCircularBufferLength;
    
    float mDelayReadHead_left;
    float mDelayReadHead_right;

    
    static const int NUM_DELAY_LINES = 4;  // Number of delay lines
    float mDelayFraction = 0.66f;  // Each delay line will be this fraction of the previous
    
    float mDelayTimeInSamples_left[NUM_DELAY_LINES];
    float mDelayTimeInSamples_right[NUM_DELAY_LINES];
    
    float mFeedbackLeft[NUM_DELAY_LINES];
    float mFeedbackRight[NUM_DELAY_LINES];
    float mIrregularDelayFactor = 0.2f;
    
    float mFilterStatesLeft[8] = {0};
    float mFilterStatesRight[8] = {0};
    
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DelaytutorialAudioProcessor)
};

