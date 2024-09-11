
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
    addParameter(mNumDelayLinesParameter = new juce::AudioParameterInt("numdelaylines", "Delay lines",  1, 25, 4));

    
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
    
    for (int i = 0; i < MAX_DELAY_LINES; ++i)
    {
        mDelayTimeInSamples_left[i] = 0.0f;
        mDelayTimeInSamples_right[i] = 0.0f;
        mFeedbackLeft[i] = 0.0f;
        mFeedbackRight[i] = 0.0f;
        mFilterStatesLeft[i] = 0.0f;
        mFilterStatesRight[i] = 0.0f;
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
    for (int i = 0; i < MAX_DELAY_LINES; ++i)
    {
        mDelayTimeInSamples_left[i] = sampleRate * mDelayTimeParameter->get() * (1.0f + 0.1f * i);
        mDelayTimeInSamples_right[i] = sampleRate * mDelayTimeParameter->get() * (1.0f + 0.1f * i);
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
    
    int num_delay_lines = mNumDelayLinesParameter->get();
    
    const float inputGainCompensation = 0.15f;  // Reduce input by 75%

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

    // All-pass filter coefficients (for diffusion)
    const float allpassCoeff = 0.7f;
    float allpassBufferLeft[MAX_DELAY_LINES][4] = {{0.0f}};
    float allpassBufferRight[MAX_DELAY_LINES][4] = {{0.0f}};

    // Density build-up parameters
    const float densityBuildupRate = 0.99f; // Adjust this value to control build-up speed
    static float densityFactor = 0.0f;

    // Feedback matrix
    const float feedbackMatrix[MAX_DELAY_LINES][MAX_DELAY_LINES] = {
        {0.50f, 0.25f, 0.125f, 0.0625f, 0.03125f, 0.015625f, 0.0078125f, 0.00390625f, 0.001953125f, 0.0009765625f, 0.00048828125f, 0.000244140625f, 0.0001220703125f, 0.00006103515625f, 0.000030517578125f, 0.0000152587890625f, 0.00000762939453125f, 0.000003814697265625f, 0.0000019073486328125f, 0.00000095367431640625f},
        {0.25f, 0.50f, 0.25f, 0.125f, 0.0625f, 0.03125f, 0.015625f, 0.0078125f, 0.00390625f, 0.001953125f, 0.0009765625f, 0.00048828125f, 0.000244140625f, 0.0001220703125f, 0.00006103515625f, 0.000030517578125f, 0.0000152587890625f, 0.00000762939453125f, 0.000003814697265625f, 0.0000019073486328125f},
        {0.125f, 0.25f, 0.50f, 0.25f, 0.125f, 0.0625f, 0.03125f, 0.015625f, 0.0078125f, 0.00390625f, 0.001953125f, 0.0009765625f, 0.00048828125f, 0.000244140625f, 0.0001220703125f, 0.00006103515625f, 0.000030517578125f, 0.0000152587890625f, 0.00000762939453125f, 0.000003814697265625f},
        {0.0625f, 0.125f, 0.25f, 0.50f, 0.25f, 0.125f, 0.0625f, 0.03125f, 0.015625f, 0.0078125f, 0.00390625f, 0.001953125f, 0.0009765625f, 0.00048828125f, 0.000244140625f, 0.0001220703125f, 0.00006103515625f, 0.000030517578125f, 0.0000152587890625f, 0.00000762939453125f},
        {0.03125f, 0.0625f, 0.125f, 0.25f, 0.50f, 0.25f, 0.125f, 0.0625f, 0.03125f, 0.015625f, 0.0078125f, 0.00390625f, 0.001953125f, 0.0009765625f, 0.00048828125f, 0.000244140625f, 0.0001220703125f, 0.00006103515625f, 0.000030517578125f, 0.0000152587890625f},
        {0.015625f, 0.03125f, 0.0625f, 0.125f, 0.25f, 0.50f, 0.25f, 0.125f, 0.0625f, 0.03125f, 0.015625f, 0.0078125f, 0.00390625f, 0.001953125f, 0.0009765625f, 0.00048828125f, 0.000244140625f, 0.0001220703125f, 0.00006103515625f, 0.000030517578125f},
        {0.0078125f, 0.015625f, 0.03125f, 0.0625f, 0.125f, 0.25f, 0.50f, 0.25f, 0.125f, 0.0625f, 0.03125f, 0.015625f, 0.0078125f, 0.00390625f, 0.001953125f, 0.0009765625f, 0.00048828125f, 0.000244140625f, 0.0001220703125f, 0.00006103515625f},
        {0.00390625f, 0.0078125f, 0.015625f, 0.03125f, 0.0625f, 0.125f, 0.25f, 0.50f, 0.25f, 0.125f, 0.0625f, 0.03125f, 0.015625f, 0.0078125f, 0.00390625f, 0.001953125f, 0.0009765625f, 0.00048828125f, 0.000244140625f, 0.0001220703125f},
        {0.001953125f, 0.00390625f, 0.0078125f, 0.015625f, 0.03125f, 0.0625f, 0.125f, 0.25f, 0.50f, 0.25f, 0.125f, 0.0625f, 0.03125f, 0.015625f, 0.0078125f, 0.00390625f, 0.001953125f, 0.0009765625f, 0.00048828125f, 0.000244140625f},
        {0.0009765625f, 0.001953125f, 0.00390625f, 0.0078125f, 0.015625f, 0.03125f, 0.0625f, 0.125f, 0.25f, 0.50f, 0.25f, 0.125f, 0.0625f, 0.03125f, 0.015625f, 0.0078125f, 0.00390625f, 0.001953125f, 0.0009765625f, 0.00048828125f},
        {0.00048828125f, 0.0009765625f, 0.001953125f, 0.00390625f, 0.0078125f, 0.015625f, 0.03125f, 0.0625f, 0.125f, 0.25f, 0.50f, 0.25f, 0.125f, 0.0625f, 0.03125f, 0.015625f, 0.0078125f, 0.00390625f, 0.001953125f, 0.0009765625f},
        {0.000244140625f, 0.00048828125f, 0.0009765625f, 0.001953125f, 0.00390625f, 0.0078125f, 0.015625f, 0.03125f, 0.0625f, 0.125f, 0.25f, 0.50f, 0.25f, 0.125f, 0.0625f, 0.03125f, 0.015625f, 0.0078125f, 0.00390625f, 0.001953125f},
        {0.0001220703125f, 0.000244140625f, 0.00048828125f, 0.0009765625f, 0.001953125f, 0.00390625f, 0.0078125f, 0.015625f, 0.03125f, 0.0625f, 0.125f, 0.25f, 0.50f, 0.25f, 0.125f, 0.0625f, 0.03125f, 0.015625f, 0.0078125f, 0.00390625f},
        {0.00006103515625f, 0.0001220703125f, 0.000244140625f, 0.00048828125f, 0.0009765625f, 0.001953125f, 0.00390625f, 0.0078125f, 0.015625f, 0.03125f, 0.0625f, 0.125f, 0.25f, 0.50f, 0.25f, 0.125f, 0.0625f, 0.03125f, 0.015625f, 0.0078125f},
        {0.000030517578125f, 0.00006103515625f, 0.0001220703125f, 0.000244140625f, 0.00048828125f, 0.0009765625f, 0.001953125f, 0.00390625f, 0.0078125f, 0.015625f, 0.03125f, 0.0625f, 0.125f, 0.25f, 0.50f, 0.25f, 0.125f, 0.0625f, 0.03125f, 0.015625f},
        {0.0000152587890625f, 0.000030517578125f, 0.00006103515625f, 0.0001220703125f, 0.000244140625f, 0.00048828125f, 0.0009765625f, 0.001953125f, 0.00390625f, 0.0078125f, 0.015625f, 0.03125f, 0.0625f, 0.125f, 0.25f, 0.50f, 0.25f, 0.125f, 0.0625f, 0.03125f},
        {0.00000762939453125f, 0.0000152587890625f, 0.000030517578125f, 0.00006103515625f, 0.0001220703125f, 0.000244140625f, 0.00048828125f, 0.0009765625f, 0.001953125f, 0.00390625f, 0.0078125f, 0.015625f, 0.03125f, 0.0625f, 0.125f, 0.25f, 0.50f, 0.25f, 0.125f, 0.0625f},
        {0.000003814697265625f, 0.00000762939453125f, 0.0000152587890625f, 0.000030517578125f, 0.00006103515625f, 0.0001220703125f, 0.000244140625f, 0.00048828125f, 0.0009765625f, 0.001953125f, 0.00390625f, 0.0078125f, 0.015625f, 0.03125f, 0.0625f, 0.125f, 0.25f, 0.50f, 0.25f, 0.125f},
        {0.0000019073486328125f, 0.000003814697265625f, 0.00000762939453125f, 0.0000152587890625f, 0.000030517578125f, 0.00006103515625f, 0.0001220703125f, 0.000244140625f, 0.00048828125f, 0.0009765625f, 0.001953125f, 0.00390625f, 0.0078125f, 0.015625f, 0.03125f, 0.0625f, 0.125f, 0.25f, 0.50f, 0.25f},
        {0.00000095367431640625f, 0.0000019073486328125f, 0.000003814697265625f, 0.00000762939453125f, 0.0000152587890625f, 0.000030517578125f, 0.00006103515625f, 0.0001220703125f, 0.000244140625f, 0.00048828125f, 0.0009765625f, 0.001953125f, 0.00390625f, 0.0078125f, 0.015625f, 0.03125f, 0.0625f, 0.125f, 0.25f, 0.50f}
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
    const float primes[MAX_DELAY_LINES] = {2.0f, 3.0f, 5.0f, 7.0f, 11.0f, 13.0f, 17.0f, 19.0f, 23.0f, 29.0f, 31.0f, 37.0f, 41.0f, 43.0f, 47.0f, 53.0f, 59.0f, 61.0f, 67.0f, 71.0f};
    const float baseWaveshapeAmount = 5.0f;

    // DC blocking filter
    float dcBlockCoeff = 0.995f;
    static float dcBlockerStateLeft = 0.0f;
    static float dcBlockerStateRight = 0.0f;

    for (int sample = 0; sample < buffer.getNumSamples(); sample++)
    {
        // Apply DC blocking filter
        float inputLeft = leftChannel[sample];
        float inputRight = rightChannel[sample];
        float outputLeft = inputLeft - lastInputLeft + R * lastOutputLeft * inputGainCompensation;
        float outputRight = inputRight - lastInputRight + R * lastOutputRight * inputGainCompensation;
        lastInputLeft = inputLeft;
        lastInputRight = inputRight;
        lastOutputLeft = outputLeft;
        lastOutputRight = outputRight;

        // Prepare feedback using the matrix
        float feedbackLeft[MAX_DELAY_LINES] = {0.0f};
        float feedbackRight[MAX_DELAY_LINES] = {0.0f};

        for (int i = 0; i < num_delay_lines; ++i) {
            for (int j = 0; j < num_delay_lines; ++j) {
                feedbackLeft[i] += mFeedbackLeft[j] * feedbackMatrix[i][j];
                feedbackRight[i] += mFeedbackRight[j] * feedbackMatrix[i][j];
            }
        }

        // Write to circular buffer with feedback
        float summedFeedbackLeft = 0.0f;
        float summedFeedbackRight = 0.0f;
        for (int i = 0; i < num_delay_lines; ++i) {
            summedFeedbackLeft += feedbackLeft[i];
            summedFeedbackRight += feedbackRight[i];
        }
        
        // Balance feedback between channels
        float maxFeedback = std::max(std::abs(summedFeedbackLeft), std::abs(summedFeedbackRight));
        if (maxFeedback > 1.0f) {
            summedFeedbackLeft /= maxFeedback;
            summedFeedbackRight /= maxFeedback;
        }
        
        mCircularBufferLeft[mCircularBufferWriteHead] = outputLeft + summedFeedbackLeft;
        mCircularBufferRight[mCircularBufferWriteHead] = outputRight + summedFeedbackRight;
        
        // Smooth the stereo offset
        mStereoOffsetSmooth = mStereoOffsetSmooth * smoothCoeff + stereoOffset * (1.0f - smoothCoeff);

        float combined_delay_left = 0.0f;
        float combined_delay_right = 0.0f;
        float total_weight = 0.0f;
        
        float earlyReflectionLeft = 0.0f;
        float earlyReflectionRight = 0.0f;

        for (int i = 0; i < num_delay_lines; ++i)
        {
            // Early reflections setup
             const int numReflections = 8;
             float reflectionDelays[numReflections] = {0.05f, 0.10f, 0.15f, 0.20f, 0.25f, 0.30f, 0.35f, 0.40f}; // in seconds
             float reflectionGains[numReflections] = {0.6f, 0.5f, 0.4f, 0.3f, 0.2f, 0.1f, 0.05f, 0.025f};
             const int predelaySamples = static_cast<int>(0.02f * getSampleRate()); // 20ms predelay

             // Convert reflection delays to samples
             int reflectionDelaySamples[numReflections];
             for (int i = 0; i < numReflections; ++i) {
                 reflectionDelaySamples[i] = static_cast<int>(reflectionDelays[i] * getSampleRate());
             }
            
            float filterCoeffs[numReflections];
            for (int i = 0; i < numReflections; ++i) {
                float cutoff = 20000.0f * std::pow(0.99f, i); // Exponentially decreasing cutoff frequency
                float w0 = 2.0f * M_PI * cutoff / getSampleRate();
                filterCoeffs[i] = std::exp(-w0);
            }

             // Calculate early reflections with low-pass filtering, density envelope, and predelay
                 for (int i = 0; i < numReflections; ++i)
                 {
                     int readIndex = (mCircularBufferWriteHead - reflectionDelaySamples[i] - predelaySamples + mCircularBufferLength) % mCircularBufferLength;
                     
                      // Apply low-pass filter
                      mFilterStatesLeft[i] = filterCoeffs[i] * mFilterStatesLeft[i] + (1.0f - filterCoeffs[i]) * mCircularBufferLeft[readIndex];
                      mFilterStatesRight[i] = filterCoeffs[i] * mFilterStatesRight[i] + (1.0f - filterCoeffs[i]) * mCircularBufferRight[readIndex];

                     // Apply density envelope to early reflections (with reduced initial gain)
                     float reflectionGain = reflectionGains[i] * 0.9f; // Reduced initial gain
                     earlyReflectionLeft += reflectionGain;
                     earlyReflectionRight += reflectionGain;
                 }
            
            // Define prime numbers for irregular delay multipliers
            const float delayPrimes[MAX_DELAY_LINES] = {2.0f, 3.0f, 5.0f, 7.0f, 11.0f, 13.0f, 17.0f, 19.0f, 23.0f, 29.0f, 31.0f, 37.0f, 41.0f, 43.0f, 47.0f, 53.0f, 59.0f, 61.0f, 67.0f, 71.0f};

            // In the class definition, add a new member variable:
            float mIrregularDelayFactor;

            // In the constructor or prepareToPlay, initialize this factor:
            mIrregularDelayFactor = 0.2f; // Adjust this value to control the irregularity

            // In the processBlock function, replace the original delay multiplier line with:
            float delayMultiplier = 1.0f + (delayPrimes[i] / 7.0f - 1.0f) * mIrregularDelayFactor;
            float weight = 1.0f / (i + 1);  // Decreasing weight for each delay line

            // Unique LFO phase for each delay line
            float uniqueLfoPhase_left = mLfoPhase + (float)i / num_delay_lines;
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

            // Implement crossfade for buffer wraparound
            float delay_sample_left, delay_sample_right;
            if (readHead_x1_left < readHead_x_left) {
                float fade = (float)readHead_x1_left / mCircularBufferLength;
                delay_sample_left = lin_interp(mCircularBufferLeft[readHead_x_left], mCircularBufferLeft[0], fade);
            } else {
                delay_sample_left = lin_interp(mCircularBufferLeft[readHead_x_left], mCircularBufferLeft[readHead_x1_left], readHeadFloat_left);
            }

            if (readHead_x1_right < readHead_x_right) {
                float fade = (float)readHead_x1_right / mCircularBufferLength;
                delay_sample_right = lin_interp(mCircularBufferRight[readHead_x_right], mCircularBufferRight[0], fade);
            } else {
                delay_sample_right = lin_interp(mCircularBufferRight[readHead_x_right], mCircularBufferRight[readHead_x1_right], readHeadFloat_right);
            }

            // Apply prime-based waveshaping with smoother transition
            float waveshapeAmount = baseWaveshapeAmount * primes[i] / 7.0f; // Normalize by the largest prime
            delay_sample_left = softClip(delay_sample_left, waveshapeAmount);
            delay_sample_right = softClip(delay_sample_right, waveshapeAmount);
            
            float compensationFactor = 0.5f; // Adjust this value to control overall volume
            delay_sample_left *= compensationFactor;
            delay_sample_right *= compensationFactor;

            // After combining delay lines, add an extra saturation stage with volume compensation
            combined_delay_left = softClip(combined_delay_left * 2.0f, 1.0f) * 0.5f * compensationFactor;
            combined_delay_right = softClip(combined_delay_right * 2.0f, 1.0f) * 0.5f * compensationFactor;

            // Apply all-pass diffusion
            for (int j = 0; j < 4; ++j) {
                float allpass_out_left = allpassCoeff * (delay_sample_left - allpassBufferLeft[i][j]) + allpassBufferLeft[i][j];
                allpassBufferLeft[i][j] = delay_sample_left;
                delay_sample_left = allpass_out_left;

                float allpass_out_right = allpassCoeff * (delay_sample_right - allpassBufferRight[i][j]) + allpassBufferRight[i][j];
                allpassBufferRight[i][j] = delay_sample_right;
                delay_sample_right = allpass_out_right;
            }

            combined_delay_left += delay_sample_left * weight;
            combined_delay_right += delay_sample_right * weight;
            total_weight += weight;
        }

        // Normalize the combined delay
        if (total_weight > 0) {
            combined_delay_left /= total_weight;
            combined_delay_right /= total_weight;
        }

        // Apply density build-up
        densityFactor = densityFactor * densityBuildupRate + (1.0f - densityBuildupRate);
        combined_delay_left *= densityFactor;
        combined_delay_right *= densityFactor;

        // Apply DC blocking filter
        float dcBlockedLeft = combined_delay_left - dcBlockerStateLeft + dcBlockCoeff * dcBlockerStateLeft;
        dcBlockerStateLeft = dcBlockedLeft;
        float dcBlockedRight = combined_delay_right - dcBlockerStateRight + dcBlockCoeff * dcBlockerStateRight;
        dcBlockerStateRight = dcBlockedRight;

        combined_delay_left = dcBlockedLeft;
        combined_delay_right = dcBlockedRight;

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

                // Scale down the feedback
                float feedback = *mFeedbackParameter * 0.5f; // Reduce feedback by half
                for (int i = 0; i < num_delay_lines; ++i) {
                    mFeedbackLeft[i] = lowPass * feedback;
                    mFeedbackRight[i] = highPass * feedback;
                }
                
                // After all processing, apply wet gain compensation and makeup gain
                float wetLeft = lowPass * 10.0f;
                float wetRight = highPass * 10.0f;

                // Final output stage
                float dryWet = *mDryWetParameter;
                outputLeft = combined_delay_left + earlyReflectionLeft;
                outputRight = combined_delay_right + earlyReflectionRight;
        
                outputLeft = inputLeft * (1 - dryWet) + wetLeft * dryWet;
                outputRight = inputRight * (1 - dryWet) + wetRight * dryWet;
                
                // Apply soft clipping to the output
//                outputLeft = std::tanh(outputLeft);
//                outputRight = std::tanh(outputRight);
                
                buffer.setSample(0, sample, outputLeft);
                buffer.setSample(1, sample, outputRight);
                
                // Smoother transition for circular buffer write head
                mCircularBufferWriteHead++;
                if (mCircularBufferWriteHead >= mCircularBufferLength) {
                    mCircularBufferWriteHead = 0;
                    // Implement a short crossfade here if needed
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

