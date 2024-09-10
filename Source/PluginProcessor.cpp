/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
DelaytutorialAudioProcessor::DelaytutorialAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       )
#endif
{
    addParameter(mDryWetParameter = new juce::AudioParameterFloat("drywet", "Dry/wet", 0.01, 1, 0.5));
    addParameter(mFeedbackParameter = new juce::AudioParameterFloat("feedback", "Feedback", 0, 0.98, 0.5));
    addParameter(mDelayTimeParameter = new juce::AudioParameterFloat("delaytime", "Delay time",  0.01, MAX_DELAY_TIME, 0.5));
    addParameter(mLfoRateParameter = new juce::AudioParameterFloat("lforate", "LFO rate",  0.1f, 20.f, 01.f));
    addParameter(mLfoDepthParameter = new juce::AudioParameterFloat("lfodepth", "LFO depth",  0.0f, 0.1f, 0.05f));
    addParameter(mLfoPhaseParameter = new juce::AudioParameterFloat("lfophase", "LFO phase",  0.0f, 1.f, 0.f));
    
    mCircularBufferLeft = nullptr;
    mCircularBufferRight = nullptr;
    mCircularBufferWriteHead = 0;
    mCircularBufferLength = 0;
    mDelayReadHead_left = 0;
    mDelayReadHead_right = 0;
    mFeedbackLeft = 0;
    mFeedbackRight = 0;
    mDelayTimeSmooth = 0;
    mLfoPhase = 0;
    mLfoDepthSmooth = 0;
    mStereoOffsetSmooth = 0;
    mDelayFraction = 0.66f;
    
    for (int i = 0; i < NUM_DELAY_LINES; ++i)
    {
        mDelayTimeInSamples_left[i] = 0.0f;
        mDelayTimeInSamples_right[i] = 0.0f;
    }

}

DelaytutorialAudioProcessor::~DelaytutorialAudioProcessor()
{
    if (mCircularBufferLeft != nullptr) {
        delete [] mCircularBufferLeft;
        mCircularBufferLeft = nullptr;
    }
    
    if (mCircularBufferRight != nullptr) {
        delete [] mCircularBufferRight;
        mCircularBufferRight = nullptr;
    }
}

//==============================================================================
const juce::String DelaytutorialAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool DelaytutorialAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool DelaytutorialAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool DelaytutorialAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double DelaytutorialAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int DelaytutorialAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int DelaytutorialAudioProcessor::getCurrentProgram()
{
    return 0;
}

void DelaytutorialAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String DelaytutorialAudioProcessor::getProgramName (int index)
{
    return {};
}

void DelaytutorialAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void DelaytutorialAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    for (int i = 0; i < NUM_DELAY_LINES; ++i)
    {
        mDelayTimeInSamples_left[i] = 0.0f;
        mDelayTimeInSamples_right[i] = 0.0f;
    }
    mDelayReadHead_left = 0;
    mDelayReadHead_right = 0;
    mLfoPhase = 0;
    mLfoDepthSmooth = 0;
    mStereoOffsetSmooth = 0;
    mDelayFraction = 0.66f;

    
    mCircularBufferWriteHead = 0;
    mCircularBufferLength = sampleRate * MAX_DELAY_TIME;

    if (mCircularBufferLeft == nullptr) {
        mCircularBufferLeft = new float[mCircularBufferLength];
    }
    
    juce::zeromem(mCircularBufferLeft, mCircularBufferLength * sizeof(float));
    
    if (mCircularBufferRight == nullptr) {
        mCircularBufferRight = new float[mCircularBufferLength];
    }
    
    juce::zeromem(mCircularBufferRight, mCircularBufferLength * sizeof(float));

    
    mDelayTimeSmooth = *mDelayTimeParameter;
    
    std::fill(mCircularBufferLeft, mCircularBufferLeft + mCircularBufferLength, 0.0f);
    std::fill(mCircularBufferRight, mCircularBufferRight + mCircularBufferLength, 0.0f);
}

float DelaytutorialAudioProcessor::lin_interp(float sample_x, float sample_x1, float inPhase) {
    return (1 - inPhase) * sample_x + inPhase * sample_x1;
}

void DelaytutorialAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool DelaytutorialAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void DelaytutorialAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    float* leftChannel = buffer.getWritePointer(0);
    float* rightChannel = buffer.getWritePointer(1);
    
    float delayTimeInSeconds = *mDelayTimeParameter;
    float baseDelayTimeInSamples = getSampleRate() * delayTimeInSeconds;
    float lfoPhaseOffset = *mLfoPhaseParameter;
    float stereoOffsetInMs = lfoPhaseOffset * 50.0f;
    float stereoOffset = stereoOffsetInMs * 0.001f * getSampleRate();
    
    float smoothCoeff = std::exp(-2.0f * M_PI * 20.0f / getSampleRate());

    const float minDelayTimeInSamples = 0.025f * getSampleRate(); // 25ms in samples

    // DC blocking filter coefficients
    const float R = 0.995f;
    static float lastInputLeft = 0.0f, lastInputRight = 0.0f, lastOutputLeft = 0.0f, lastOutputRight = 0.0f;

    for (int sample = 0; sample < buffer.getNumSamples(); sample++)
    {
        // Apply DC blocking filter
        float inputLeft = leftChannel[sample];
        float inputRight = rightChannel[sample];
        float outputLeft = inputLeft - lastInputLeft + R * lastOutputLeft;
        float outputRight = inputRight - lastInputRight + R * lastOutputRight;
        lastInputLeft = inputLeft;
        lastInputRight = inputRight;
        lastOutputLeft = outputLeft;
        lastOutputRight = outputRight;

        mCircularBufferLeft[mCircularBufferWriteHead] = outputLeft + mFeedbackLeft;
        mCircularBufferRight[mCircularBufferWriteHead] = outputRight + mFeedbackRight;
        
        // Smooth the stereo offset
        mStereoOffsetSmooth = mStereoOffsetSmooth * smoothCoeff + stereoOffset * (1.0f - smoothCoeff);

        float combined_delay_left = 0.0f;
        float combined_delay_right = 0.0f;
        float total_weight = 0.0f;

        for (int i = 0; i < NUM_DELAY_LINES; ++i)
        {
            float delayMultiplier = std::pow(mDelayFraction, i);
            float weight = 1.0f / (i + 1);  // Decreasing weight for each delay line

            // Unique LFO phase for each delay line
            float uniqueLfoPhase_left = mLfoPhase + (float)i / NUM_DELAY_LINES;
            float uniqueLfoPhase_right = uniqueLfoPhase_left + lfoPhaseOffset;
            
            // Wrap phases between 0 and 1
            uniqueLfoPhase_left = std::fmod(uniqueLfoPhase_left, 1.0f);
            uniqueLfoPhase_right = std::fmod(uniqueLfoPhase_right, 1.0f);
            
            float lfoOut_left = (1.0f - std::cos(2.0f * M_PI * uniqueLfoPhase_left)) * 0.0725f;
            float lfoOut_right = (1.0f - std::cos(2.0f * M_PI * uniqueLfoPhase_right)) * 0.0725f;
            
            float lfoModulation_left = lfoOut_left * (*mLfoDepthParameter / 3);
            float lfoModulation_right = lfoOut_right * (*mLfoDepthParameter / 3);
            
            float targetDelayTimeInSamples_left = baseDelayTimeInSamples * delayMultiplier * (1.0f + lfoModulation_left);
            float targetDelayTimeInSamples_right = baseDelayTimeInSamples * delayMultiplier * (1.0f + lfoModulation_right);

            // Check if delay time is below 25ms and triple it if so
            if (targetDelayTimeInSamples_left < minDelayTimeInSamples)
                targetDelayTimeInSamples_left *= 3.0f;
            if (targetDelayTimeInSamples_right < minDelayTimeInSamples)
                targetDelayTimeInSamples_right *= 3.0f;

            // Smooth the delay times
            mDelayTimeInSamples_left[i] = mDelayTimeInSamples_left[i] * smoothCoeff + targetDelayTimeInSamples_left * (1.0f - smoothCoeff);
            mDelayTimeInSamples_right[i] = mDelayTimeInSamples_right[i] * smoothCoeff + targetDelayTimeInSamples_right * (1.0f - smoothCoeff);

            float mDelayReadHead_left = mCircularBufferWriteHead - mDelayTimeInSamples_left[i];
            float mDelayReadHead_right = mCircularBufferWriteHead - mDelayTimeInSamples_right[i] - mStereoOffsetSmooth;

            // Pitch shifting logic
            if (i % 2 == 1) {  // Odd numbered delay lines (second, fourth, etc.)
                if (i % 4 == 1) {  // Second, sixth, tenth, etc. delay lines
                    mDelayReadHead_left *= 2.0f;  // Octave up
                    mDelayReadHead_right *= 2.0f;
                } else {  // Fourth, eighth, twelfth, etc. delay lines
                    mDelayReadHead_left *= 0.5f;  // Octave down
                    mDelayReadHead_right *= 0.5f;
                }
            }

            // Ensure we never go below -1 octave
            float minReadSpeed = 0.25f;  // -1 octave
            float maxDelayTime_left = mCircularBufferWriteHead - mDelayReadHead_left;
            float maxDelayTime_right = mCircularBufferWriteHead - mDelayReadHead_right;

            if (maxDelayTime_left > mDelayTimeInSamples_left[i] / minReadSpeed) {
                mDelayReadHead_left = mCircularBufferWriteHead - (mDelayTimeInSamples_left[i] / minReadSpeed);
            }
            if (maxDelayTime_right > mDelayTimeInSamples_right[i] / minReadSpeed) {
                mDelayReadHead_right = mCircularBufferWriteHead - (mDelayTimeInSamples_right[i] / minReadSpeed);
            }

            // Ensure read heads are within buffer bounds
            mDelayReadHead_left = std::fmod(mDelayReadHead_left, static_cast<float>(mCircularBufferLength));
            mDelayReadHead_right = std::fmod(mDelayReadHead_right, static_cast<float>(mCircularBufferLength));
            if (mDelayReadHead_left < 0) mDelayReadHead_left += mCircularBufferLength;
            if (mDelayReadHead_right < 0) mDelayReadHead_right += mCircularBufferLength;

            int readHead_x_left = static_cast<int>(mDelayReadHead_left);
            int readHead_x1_left = (readHead_x_left + 1) % mCircularBufferLength;
            float readHeadFloat_left = mDelayReadHead_left - readHead_x_left;

            int readHead_x_right = static_cast<int>(mDelayReadHead_right);
            int readHead_x1_right = (readHead_x_right + 1) % mCircularBufferLength;
            float readHeadFloat_right = mDelayReadHead_right - readHead_x_right;

            float delay_sample_left = lin_interp(mCircularBufferLeft[readHead_x_left], mCircularBufferLeft[readHead_x1_left], readHeadFloat_left);
            float delay_sample_right = lin_interp(mCircularBufferRight[readHead_x_right], mCircularBufferRight[readHead_x1_right], readHeadFloat_right);

            combined_delay_left += delay_sample_left * weight;
            combined_delay_right += delay_sample_right * weight;
            total_weight += weight;
        }

        // Normalize the combined delay
        if (total_weight > 0) {
            combined_delay_left /= total_weight;
            combined_delay_right /= total_weight;
        }

        // Soft clipping to prevent overloads
        combined_delay_left = std::tanh(combined_delay_left);
        combined_delay_right = std::tanh(combined_delay_right);
        
        const float tremRate = 2.0f; // 2 Hz
        const float tremDepth = 0.5f; // 50% depth
        static float tremPhase = 0.0f;
        const float tremPhaseInc = tremRate / getSampleRate();
        
        // Apply Harmonic Tremolo
        float tremLfo = 0.5f + 0.5f * sinf(2.0f * M_PI * tremPhase);
        float lowPass = combined_delay_left * (1.0f - (tremDepth / mDelayFraction) * (tremLfo * 3)) + combined_delay_right * (tremDepth * tremLfo);
        float highPass = combined_delay_left * (tremDepth * tremLfo) + combined_delay_right * (1.0f - tremDepth * tremLfo);

        float feedback = *mFeedbackParameter;
        mFeedbackLeft = lowPass * feedback;
        mFeedbackRight = highPass * feedback;
        
        float dryWet = *mDryWetParameter;
        buffer.setSample(0, sample, inputLeft * (1 - dryWet) + lowPass * dryWet);
        buffer.setSample(1, sample, inputRight * (1 - dryWet) + highPass * dryWet);
        
        mCircularBufferWriteHead++;
        if (mCircularBufferWriteHead >= mCircularBufferLength) {
            mCircularBufferWriteHead = 0;
        }
    


        // Update tremolo phase
        tremPhase += tremPhaseInc;
        if (tremPhase >= 1.0f) tremPhase -= 1.0f;

        // Update the main LFO phase
        mLfoPhase += *mLfoRateParameter / getSampleRate();
        mLfoPhase = std::fmod(mLfoPhase, 1.0f);
    }
}

//==============================================================================
bool DelaytutorialAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* DelaytutorialAudioProcessor::createEditor()
{
    return new DelaytutorialAudioProcessorEditor (*this);
}

//==============================================================================
void DelaytutorialAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    std::unique_ptr<juce::XmlElement> xml(new juce::XmlElement("delay"));
    
    xml->setAttribute("Dry/Wet", *mDryWetParameter);
    xml->setAttribute("Feedback", *mFeedbackParameter);
    xml->setAttribute("Delay time", *mDelayTimeParameter);
    xml->setAttribute("LFO rate", *mLfoRateParameter);
    xml->setAttribute("LFO depth", *mLfoDepthParameter);
    xml->setAttribute("LFO phase", *mLfoPhaseParameter);
    
    copyXmlToBinary(*xml, destData);
}

void DelaytutorialAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    
    if (xml.get() != nullptr && xml->hasTagName("delay")) {
        *mDryWetParameter = xml->getDoubleAttribute("Dry/wet");
        *mFeedbackParameter = xml->getDoubleAttribute("Feedback");
        *mDelayTimeParameter = xml->getDoubleAttribute("Delay time");
        *mLfoRateParameter = xml->getDoubleAttribute("LFO rate");
        *mLfoDepthParameter = xml->getDoubleAttribute("LFO depth");
        *mLfoPhaseParameter = xml->getDoubleAttribute("LFO phase");
    }
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DelaytutorialAudioProcessor();
}

