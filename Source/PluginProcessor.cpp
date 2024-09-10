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
    densityEnvelope = 0.0f;

    
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

    // Early reflections setup
    const int numReflections = 8;
    float reflectionDelays[numReflections] = {0.05f, 0.10f, 0.15f, 0.20f, 0.25f, 0.30f, 0.35f, 0.40f}; // in seconds
    float reflectionGains[numReflections] = {0.6f, 0.5f, 0.4f, 0.3f, 0.2f, 0.1f, 0.05f, 0.025f};

    // Convert reflection delays to samples
    int reflectionDelaySamples[numReflections];
    for (int i = 0; i < numReflections; ++i) {
        reflectionDelaySamples[i] = static_cast<int>(reflectionDelays[i] * getSampleRate());
    }

    // Low-pass filter coefficients
    float filterCoeffs[numReflections];
    for (int i = 0; i < numReflections; ++i) {
        float cutoff = 20000.0f * std::pow(0.95f, i); // Exponentially decreasing cutoff frequency
        float w0 = 2.0f * M_PI * cutoff / getSampleRate();
        filterCoeffs[i] = std::exp(-w0);
    }

    // DC blocking filter coefficients
    const float R = 0.995f;
    static float lastInputLeft = 0.0f, lastInputRight = 0.0f, lastOutputLeft = 0.0f, lastOutputRight = 0.0f;

    // Feedback matrix
    static const float feedbackMatrix[NUM_DELAY_LINES][NUM_DELAY_LINES] = {
        {0.2f, 0.1f, 0.05f, 0.025f},
        {0.1f, 0.3f, 0.15f, 0.075f},
        {0.05f, 0.15f, 0.4f, 0.2f},
        {0.025f, 0.075f, 0.2f, 0.5f}
    };

    // Waveshaping function
    auto softClip = [](float x, float amount) {
        float absX = std::abs(x);
        if (absX <= 1.0f / 3.0f) {
            return 2.0f * x;
        } else if (absX <= 2.0f / 3.0f) {
            return x * (3.0f - powf(2.0f - 3.0f * absX, 2.0f)) / 3.0f;
        } else {
            return ((x > 0.0f) ? 1.0f : -1.0f);
        }
    };

    // Prime-based waveshaping amounts
    const float primes[NUM_DELAY_LINES] = {2.0f, 3.0f, 5.0f, 7.0f};
    const float baseWaveshapeAmount = 3.0f;

    // Density envelope
    static float densityEnvelope = 1.0f;
    const float densityAttackRate = 0.999995f; // Much slower buildup
    const float maxDensity = 0.7f; // Limit maximum density

    // Predelay for early reflections
    const int predelaySamples = static_cast<int>(0.02f * getSampleRate()); // 20ms predelay

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

        // Write to circular buffer
        mCircularBufferLeft[mCircularBufferWriteHead] = outputLeft;
        mCircularBufferRight[mCircularBufferWriteHead] = outputRight;

        // Calculate early reflections with low-pass filtering, density envelope, and predelay
        float earlyReflectionLeft = 0.0f;
        float earlyReflectionRight = 0.0f;
        for (int i = 0; i < numReflections; ++i)
        {
            int readIndex = (mCircularBufferWriteHead - reflectionDelaySamples[i] - predelaySamples + mCircularBufferLength) % mCircularBufferLength;

            // Apply low-pass filter
            mFilterStatesLeft[i] = filterCoeffs[i] * mFilterStatesLeft[i] + (1.0f - filterCoeffs[i]) * mCircularBufferLeft[readIndex];
            mFilterStatesRight[i] = filterCoeffs[i] * mFilterStatesRight[i] + (1.0f - filterCoeffs[i]) * mCircularBufferRight[readIndex];

            // Apply density envelope to early reflections (with reduced initial gain)
            float reflectionGain = reflectionGains[i] * densityEnvelope * 0.2f; // Reduced initial gain
            earlyReflectionLeft += mFilterStatesLeft[i] * reflectionGain;
            earlyReflectionRight += mFilterStatesRight[i] * reflectionGain;
        }

        // Main delay line processing
        float summedFeedbackLeft = 0.0f;
        float summedFeedbackRight = 0.0f;
        for (int i = 0; i < NUM_DELAY_LINES; ++i) {
            for (int j = 0; j < NUM_DELAY_LINES; ++j) {
                summedFeedbackLeft += mFeedbackLeft[j] * feedbackMatrix[i][j];
                summedFeedbackRight += mFeedbackRight[j] * feedbackMatrix[i][j];
            }
        }
        
        // Apply density envelope to feedback
        summedFeedbackLeft *= densityEnvelope;
        summedFeedbackRight *= densityEnvelope;
        
        // Balance feedback between channels
        float maxFeedback = std::max(std::abs(summedFeedbackLeft), std::abs(summedFeedbackRight));
        if (maxFeedback > 1.0f) {
            summedFeedbackLeft /= maxFeedback;
            summedFeedbackRight /= maxFeedback;
        }
        
        // Smooth the stereo offset
        mStereoOffsetSmooth = mStereoOffsetSmooth * smoothCoeff + stereoOffset * (1.0f - smoothCoeff);

        float combined_delay_left = 0.0f;
        float combined_delay_right = 0.0f;
        float total_weight = 0.0f;

        for (int i = 0; i < NUM_DELAY_LINES; ++i)
        {
            float delayMultiplier = 1.0f + (primes[i] / 7.0f - 1.0f) * mIrregularDelayFactor;
            float weight = 1.0f / (i + 1);  // Decreasing weight for each delay line

            // Unique LFO phase for each delay line
            float uniqueLfoPhase_left = std::fmod(mLfoPhase + (float)i / NUM_DELAY_LINES, 1.0f);
            float uniqueLfoPhase_right = std::fmod(uniqueLfoPhase_left + lfoPhaseOffset, 1.0f);
            
            float lfoOut_left = (1.0f - std::cos(2.0f * M_PI * uniqueLfoPhase_left)) * 0.0725f;
            float lfoOut_right = (1.0f - std::cos(2.0f * M_PI * uniqueLfoPhase_right)) * 0.0725f;
            
            float lfoModulation_left = lfoOut_left * (*mLfoDepthParameter / 3);
            float lfoModulation_right = lfoOut_right * (*mLfoDepthParameter / 3);
            
            float targetDelayTimeInSamples_left = baseDelayTimeInSamples * delayMultiplier * (1.0f + lfoModulation_left);
            float targetDelayTimeInSamples_right = baseDelayTimeInSamples * delayMultiplier * (1.0f + lfoModulation_right);

            // Ensure delay times are within valid range
            targetDelayTimeInSamples_left = juce::jlimit(minDelayTimeInSamples, static_cast<float>(mCircularBufferLength - 1), targetDelayTimeInSamples_left);
            targetDelayTimeInSamples_right = juce::jlimit(minDelayTimeInSamples, static_cast<float>(mCircularBufferLength - 1), targetDelayTimeInSamples_right);

            // Smooth the delay times
            mDelayTimeInSamples_left[i] = mDelayTimeInSamples_left[i] * smoothCoeff + targetDelayTimeInSamples_left * (1.0f - smoothCoeff);
            mDelayTimeInSamples_right[i] = mDelayTimeInSamples_right[i] * smoothCoeff + targetDelayTimeInSamples_right * (1.0f - smoothCoeff);

            // Calculate read heads with safe wrapping
            float mDelayReadHead_left = std::fmod(mCircularBufferWriteHead - mDelayTimeInSamples_left[i] + mCircularBufferLength, static_cast<float>(mCircularBufferLength));
            float mDelayReadHead_right = std::fmod(mCircularBufferWriteHead - mDelayTimeInSamples_right[i] - mStereoOffsetSmooth + mCircularBufferLength, static_cast<float>(mCircularBufferLength));

            // Pitch shifting logic
            if (i % 2 == 1) {  // Odd numbered delay lines (second, fourth, etc.)
                if (i % 4 == 1) {  // Second, sixth, tenth, etc. delay lines
                    mDelayReadHead_left = std::fmod(mDelayReadHead_left * 2.0f, static_cast<float>(mCircularBufferLength));
                    mDelayReadHead_right = std::fmod(mDelayReadHead_right * 2.0f, static_cast<float>(mCircularBufferLength));
                } else {  // Fourth, eighth, twelfth, etc. delay lines
                    mDelayReadHead_left = std::fmod(mDelayReadHead_left * 0.5f + mCircularBufferLength, static_cast<float>(mCircularBufferLength));
                    mDelayReadHead_right = std::fmod(mDelayReadHead_right * 0.5f + mCircularBufferLength, static_cast<float>(mCircularBufferLength));
                }
            }

            int readHead_x_left = static_cast<int>(mDelayReadHead_left);
            int readHead_x1_left = (readHead_x_left + 1) % mCircularBufferLength;
            float readHeadFloat_left = mDelayReadHead_left - readHead_x_left;

            int readHead_x_right = static_cast<int>(mDelayReadHead_right);
            int readHead_x1_right = (readHead_x_right + 1) % mCircularBufferLength;
            float readHeadFloat_right = mDelayReadHead_right - readHead_x_right;

            // Linear interpolation
            float delay_sample_left = lin_interp(mCircularBufferLeft[readHead_x_left], mCircularBufferLeft[readHead_x1_left], readHeadFloat_left);
            float delay_sample_right = lin_interp(mCircularBufferRight[readHead_x_right], mCircularBufferRight[readHead_x1_right], readHeadFloat_right);

            // Apply prime-based waveshaping
            float waveshapeAmount = baseWaveshapeAmount * primes[i] / 7.0f;
            delay_sample_left = softClip(delay_sample_left, waveshapeAmount);
            delay_sample_right = softClip(delay_sample_right, waveshapeAmount);
            
            float compensationFactor = 0.5f;
            delay_sample_left *= compensationFactor;
            delay_sample_right *= compensationFactor;

            // Apply density envelope to delay lines
            delay_sample_left *= densityEnvelope;
            delay_sample_right *= densityEnvelope;

            combined_delay_left += delay_sample_left * weight;
            combined_delay_right += delay_sample_right * weight;
            total_weight += weight;

            // Update feedback for next iteration (with reduced initial feedback)
            mFeedbackLeft[i] = delay_sample_left * (*mFeedbackParameter) * densityEnvelope;
            mFeedbackRight[i] = delay_sample_right * (*mFeedbackParameter) * densityEnvelope;
        }

        // Normalize the combined delay
        if (total_weight > 0) {
            combined_delay_left /= total_weight;
            combined_delay_right /= total_weight;
        }

        // Combine main delay output with early reflections
        outputLeft = combined_delay_left + earlyReflectionLeft;
        outputRight = combined_delay_right + earlyReflectionRight;

        // Apply soft clipping to the output
        outputLeft = std::tanh(outputLeft);
        outputRight = std::tanh(outputRight);
        
        // Mix dry and wet signals with gradual wet increase
        float dryWet = *mDryWetParameter * densityEnvelope;
        outputLeft = inputLeft * (1 - dryWet) + outputLeft * dryWet;
        outputRight = inputRight * (1 - dryWet) + outputRight * dryWet;
        
        buffer.setSample(0, sample, outputLeft);
                buffer.setSample(1, sample, outputRight);
                
                // Update circular buffer write head
                mCircularBufferWriteHead = (mCircularBufferWriteHead + 1) % mCircularBufferLength;

                // Update LFO phase
                mLfoPhase = std::fmod(mLfoPhase + *mLfoRateParameter / getSampleRate(), 1.0f);

                // Update density envelope with a maximum limit
                densityEnvelope = std::min(maxDensity, 1.0f - (1.0f - densityEnvelope) * densityAttackRate);
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

