/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
/**
*/
class DelaytutorialAudioProcessorEditor  : public juce::AudioProcessorEditor
                                        
{
public:
    DelaytutorialAudioProcessorEditor (DelaytutorialAudioProcessor&);
    ~DelaytutorialAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    DelaytutorialAudioProcessor& audioProcessor;
    
    juce::Slider mDryWetSlider;
    juce::Slider mFeedbackSlider;
    juce::Slider mDelaySlider;
    juce::Slider mLfoRateSlider;
    juce::Slider mLfoDepthSlider;
    juce::Slider mLfoPhaseSlider;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DelaytutorialAudioProcessorEditor)
};

