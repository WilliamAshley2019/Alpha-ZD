#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
Earthworks521Processor::Earthworks521Processor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
    apvts(*this, nullptr, "Parameters", createParameterLayout())
{
    apvts.addParameterListener("gain", this);
    apvts.addParameterListener("phantom", this);
    apvts.addParameterListener("polarity", this);
    apvts.addParameterListener("oversample", this);
    apvts.addParameterListener("output", this);
}

Earthworks521Processor::~Earthworks521Processor()
{
    apvts.removeParameterListener("gain", this);
    apvts.removeParameterListener("phantom", this);
    apvts.removeParameterListener("polarity", this);
    apvts.removeParameterListener("oversample", this);
    apvts.removeParameterListener("output", this);
}

//==============================================================================
void Earthworks521Processor::parameterChanged(const juce::String& paramID, float newValue)
{
    if (paramID == "gain")      gainStepIndex = (int)newValue;
    else if (paramID == "phantom")   phantomPower = (newValue > 0.5f);
    else if (paramID == "polarity")  polarityInvert = (newValue > 0.5f);
    else if (paramID == "oversample")oversampleEnable = (newValue > 0.5f);
    else if (paramID == "output")    outputLevel = newValue;
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout
Earthworks521Processor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // Stepped gain: 0 = 5 dB, 11 = 60 dB
    layout.add(std::make_unique<juce::AudioParameterInt>("gain", "Gain", 0, 11, 6));

    layout.add(std::make_unique<juce::AudioParameterBool>("phantom", "+48V", false));
    layout.add(std::make_unique<juce::AudioParameterBool>("polarity", "Polarity", false));
    layout.add(std::make_unique<juce::AudioParameterBool>("oversample", "Oversample", false));

    // Output attenuation: 0..1 (dB display handled in UI)
    layout.add(std::make_unique<juce::AudioParameterFloat>("output", "Output", 0.0f, 1.0f, 1.0f));

    return layout;
}

//==============================================================================
float Earthworks521Processor::gainLinearFromStep(int step) noexcept
{
    const int gainDb = 5 + step * 5;   // 5,10,...,60
    return std::pow(10.0f, gainDb / 20.0f);
}

void Earthworks521Processor::resetChannelState()
{
    int numChannels = getTotalNumInputChannels();
    lastSample.assign(numChannels, 0.0f);
    dcState.assign(numChannels, 0.0f);
    clipActive = false;
}

//==============================================================================
void Earthworks521Processor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    currentBlockSize = samplesPerBlock;

    // Coefficient calculation
    const double twoPi = juce::MathConstants<double>::twoPi;
    const double desiredCutoff = 1.0;  // 1 Hz high‑pass

    // Base rate
    double alphaBase = 1.0 - std::exp(-twoPi * desiredCutoff / sampleRate);
    dcAlphaBase = static_cast<float>(alphaBase);
    double maxChangeBase = 22.0e6 / sampleRate;
    slewBase = static_cast<float>(maxChangeBase);

    // Oversampled (2×)
    double alphaOS = 1.0 - std::exp(-twoPi * desiredCutoff / (sampleRate * 2.0));
    dcAlphaOS = static_cast<float>(alphaOS);
    double maxChangeOS = 22.0e6 / (sampleRate * 2.0);
    slewOS = static_cast<float>(maxChangeOS);

    // Always allocate oversampler (enabled/disabled via atomic flag)
    oversampler.reset(new juce::dsp::Oversampling<float>(2, kOversamplingOrder,
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
        true));  // linear phase
    oversampler->initProcessing(samplesPerBlock);

    resetChannelState();
}

void Earthworks521Processor::releaseResources()
{
    oversampler.reset();
    resetChannelState();
}

//==============================================================================
static void applyDSPToBlock(juce::dsp::AudioBlock<float>& block,
    int numChannels,
    bool polarityInvert,
    float gainLin,
    float slewLimit,
    float dcAlpha,
    std::vector<float>& lastSample,
    std::vector<float>& dcState,
    std::atomic<bool>& clipActive)
{
    const int numSamples = (int)block.getNumSamples();
    for (int ch = 0; ch < numChannels; ++ch)
    {
        float* data = block.getChannelPointer(ch);
        float& last = lastSample[ch];
        float& dc = dcState[ch];

        for (int i = 0; i < numSamples; ++i)
        {
            float x = data[i];

            // Polarity invert
            if (polarityInvert)
                x = -x;

            // Linear gain
            x *= gainLin;

            // Slew limiting (22 V/µs)
            float diff = x - last;
            if (diff > slewLimit) diff = slewLimit;
            else if (diff < -slewLimit) diff = -slewLimit;
            x = last + diff;
            last = x;

            // DC servo (1 Hz high‑pass)
            dc = dc + dcAlpha * (x - dc);
            x -= dc;

            // Hard clip at +29 dBu rail (~21.5 V, assuming 0 dBFS = 0 dBu reference)
            constexpr float maxVoltage = 21.5f;
            if (x > maxVoltage)
            {
                x = maxVoltage;
                clipActive = true;
            }
            else if (x < -maxVoltage)
            {
                x = -maxVoltage;
                clipActive = true;
            }

            data[i] = x;
        }
    }
}

void Earthworks521Processor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numChannels = getTotalNumInputChannels();
    const int numSamples = buffer.getNumSamples();

    // Read atomic mirrors
    const int  gainIdx = gainStepIndex.load();
    const bool inv = polarityInvert.load();
    const bool useOS = oversampleEnable.load();
    const float outGain = outputLevel.load();

    const float gainLinear = gainLinearFromStep(gainIdx);
    clipActive = false;   // reset at start of block

    // Create AudioBlock view
    juce::dsp::AudioBlock<float> block(buffer);

    if (useOS && oversampler != nullptr)
    {
        // 2× oversampling processing
        auto upBlock = oversampler->processSamplesUp(block);
        applyDSPToBlock(upBlock, numChannels, inv, gainLinear,
            slewOS, dcAlphaOS, lastSample, dcState, clipActive);
        oversampler->processSamplesDown(block);
    }
    else
    {
        // Base rate
        applyDSPToBlock(block, numChannels, inv, gainLinear,
            slewBase, dcAlphaBase, lastSample, dcState, clipActive);
    }

    // Apply output attenuation (after all processing)
    if (outGain < 1.0f)
    {
        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* data = buffer.getWritePointer(ch);
            for (int i = 0; i < numSamples; ++i)
                data[i] *= outGain;
        }
    }

    // Clear extra output channels
    for (int ch = numChannels; ch < buffer.getNumChannels(); ++ch)
        buffer.clear(ch, 0, numSamples);
}

//==============================================================================
juce::AudioProcessorEditor* Earthworks521Processor::createEditor()
{
    return new Earthworks521Editor(*this);
}

bool Earthworks521Processor::hasEditor() const { return true; }
const juce::String Earthworks521Processor::getName() const { return JucePlugin_Name; }
bool Earthworks521Processor::acceptsMidi() const { return false; }
bool Earthworks521Processor::producesMidi() const { return false; }
bool Earthworks521Processor::isMidiEffect() const { return false; }
double Earthworks521Processor::getTailLengthSeconds() const { return 0.0; }

int Earthworks521Processor::getNumPrograms() { return 1; }
int Earthworks521Processor::getCurrentProgram() { return 0; }
void Earthworks521Processor::setCurrentProgram(int) {}
const juce::String Earthworks521Processor::getProgramName(int) { return {}; }
void Earthworks521Processor::changeProgramName(int, const juce::String&) {}

void Earthworks521Processor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void Earthworks521Processor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml && xml->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new Earthworks521Processor();
}