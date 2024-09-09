/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
DelaytutorialAudioProcessorEditor::DelaytutorialAudioProcessorEditor (DelaytutorialAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    // Make sure that before the constructor has finished, you've set the
    // editor's size to whatever you need it to be.
    setSize (400, 300);
    
    auto& params = processor.getParameters();
    
    juce::AudioParameterFloat* dryWetParameter = (juce::AudioParameterFloat*)params.getUnchecked(0);
    
    mDryWetSlider.setBounds(0,0,100,100);
    mDryWetSlider.setSliderStyle(juce::Slider::SliderStyle::RotaryVerticalDrag);
    mDryWetSlider.setTextBoxStyle(juce::Slider::TextEntryBoxPosition::NoTextBox, true, 0, 0);
    mDryWetSlider.setRange(dryWetParameter->range.start, dryWetParameter->range.end);
    mDryWetSlider.setValue(*dryWetParameter);
    addAndMakeVisible(mDryWetSlider);
    
    mDryWetSlider.onValueChange = [this, dryWetParameter] {
        *dryWetParameter = mDryWetSlider.getValue();
    };
    
    mDryWetSlider.onDragStart = [dryWetParameter] {
        dryWetParameter->beginChangeGesture();
    };
    
    mDryWetSlider.onDragEnd = [dryWetParameter] {
        dryWetParameter->endChangeGesture();
    };
    
    juce::AudioParameterFloat* feedbackParameter = (juce::AudioParameterFloat*)params.getUnchecked(1);
    
    mFeedbackSlider.setBounds(100, 0, 100, 100);
    mFeedbackSlider.setSliderStyle(juce::Slider::SliderStyle::RotaryVerticalDrag);
    mFeedbackSlider.setTextBoxStyle(juce::Slider::TextEntryBoxPosition::NoTextBox, true, 0, 0);
    mFeedbackSlider.setRange(feedbackParameter->range.start, feedbackParameter->range.end);
    mFeedbackSlider.setValue(*feedbackParameter);
    addAndMakeVisible(mFeedbackSlider);
    
    mFeedbackSlider.onValueChange = [this, feedbackParameter] {
        *feedbackParameter = mFeedbackSlider.getValue();
    };
    
    mFeedbackSlider.onDragStart = [feedbackParameter] {
        feedbackParameter->beginChangeGesture();
    };
    
    mFeedbackSlider.onDragEnd = [feedbackParameter] {
        feedbackParameter->endChangeGesture();
    };
    
    juce::AudioParameterFloat* delayParameter = (juce::AudioParameterFloat*)params.getUnchecked(2);

    mDelaySlider.setBounds(200, 0, 100, 100);
    mDelaySlider.setSliderStyle(juce::Slider::SliderStyle::RotaryVerticalDrag);
    mDelaySlider.setTextBoxStyle(juce::Slider::TextEntryBoxPosition::NoTextBox, true, 0, 0);
    mDelaySlider.setRange(delayParameter->range.start, delayParameter->range.end);
    mDelaySlider.setValue(*delayParameter);
    addAndMakeVisible(mDelaySlider);
    
    mDelaySlider.onValueChange = [this, delayParameter] {
        *delayParameter = mDelaySlider.getValue();
    };
    
    mDelaySlider.onDragStart = [delayParameter] {
        delayParameter->beginChangeGesture();
    };
    
    mDelaySlider.onDragEnd = [delayParameter] {
        delayParameter->endChangeGesture();
    };
    
    juce::AudioParameterFloat* lfoRateParameter = (juce::AudioParameterFloat*)params.getUnchecked(3);

    mLfoRateSlider.setBounds(0, 100, 100, 100);
    mLfoRateSlider.setSliderStyle(juce::Slider::SliderStyle::RotaryVerticalDrag);
    mLfoRateSlider.setTextBoxStyle(juce::Slider::TextEntryBoxPosition::NoTextBox, true, 0, 0);
    mLfoRateSlider.setRange(lfoRateParameter->range.start, lfoRateParameter->range.end);
    mLfoRateSlider.setValue(*lfoRateParameter);
    addAndMakeVisible(mLfoRateSlider);
    
    mLfoRateSlider.onValueChange = [this, lfoRateParameter] {
        *lfoRateParameter = mLfoRateSlider.getValue();
    };
    
    mLfoRateSlider.onDragStart = [lfoRateParameter] {
        lfoRateParameter->beginChangeGesture();
    };
    
    mLfoRateSlider.onDragEnd = [lfoRateParameter] {
        lfoRateParameter->endChangeGesture();
    };
    
    juce::AudioParameterFloat* lfoDepthParameter = (juce::AudioParameterFloat*)params.getUnchecked(4);

    mLfoDepthSlider.setBounds(100, 100, 100, 100);
    mLfoDepthSlider.setSliderStyle(juce::Slider::SliderStyle::RotaryVerticalDrag);
    mLfoDepthSlider.setTextBoxStyle(juce::Slider::TextEntryBoxPosition::NoTextBox, true, 0, 0);
    mLfoDepthSlider.setRange(lfoDepthParameter->range.start, lfoDepthParameter->range.end);
    mLfoDepthSlider.setValue(*lfoDepthParameter);
    addAndMakeVisible(mLfoDepthSlider);
    
    mLfoDepthSlider.onValueChange = [this, lfoDepthParameter] {
        *lfoDepthParameter = mLfoDepthSlider.getValue();
    };
    
    mLfoDepthSlider.onDragStart = [lfoDepthParameter] {
        lfoDepthParameter->beginChangeGesture();
    };
    
    mLfoDepthSlider.onDragEnd = [lfoDepthParameter] {
        lfoDepthParameter->endChangeGesture();
    };
    
    juce::AudioParameterFloat* lfoPhaseParameter = (juce::AudioParameterFloat*)params.getUnchecked(5);

    mLfoPhaseSlider.setBounds(200, 100, 100, 100);
    mLfoPhaseSlider.setSliderStyle(juce::Slider::SliderStyle::RotaryVerticalDrag);
    mLfoPhaseSlider.setTextBoxStyle(juce::Slider::TextEntryBoxPosition::NoTextBox, true, 0, 0);
    mLfoPhaseSlider.setRange(lfoPhaseParameter->range.start, lfoPhaseParameter->range.end);
    mLfoPhaseSlider.setValue(*lfoPhaseParameter);
    addAndMakeVisible(mLfoPhaseSlider);
    
    mLfoPhaseSlider.onValueChange = [this, lfoPhaseParameter] {
        *lfoPhaseParameter = mLfoPhaseSlider.getValue();
    };
    
    mLfoPhaseSlider.onDragStart = [lfoPhaseParameter] {
        lfoPhaseParameter->beginChangeGesture();
    };
    
    mLfoPhaseSlider.onDragEnd = [lfoPhaseParameter] {
        lfoPhaseParameter->endChangeGesture();
    };
}

DelaytutorialAudioProcessorEditor::~DelaytutorialAudioProcessorEditor()
{
}

//==============================================================================
void DelaytutorialAudioProcessorEditor::paint (juce::Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    g.setColour (juce::Colours::white);
    g.setFont (15.0f);
}

void DelaytutorialAudioProcessorEditor::resized()
{
    // This is generally where you'll want to lay out the positions of any
    // subcomponents in your editor..
}

