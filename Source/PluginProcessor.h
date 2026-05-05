#pragma once

#include <JuceHeader.h>

//==============================================================================
class Earthworks521Processor : public juce::AudioProcessor,
    public juce::AudioProcessorValueTreeState::Listener
{
public:
    Earthworks521Processor();
    ~Earthworks521Processor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int) override;
    const juce::String getProgramName(int) override;
    void changeProgramName(int, const juce::String&) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    void parameterChanged(const juce::String& parameterID, float newValue) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    juce::AudioProcessorValueTreeState apvts;

    // Atomic flag for UI (audio thread writes, UI thread reads & clears)
    std::atomic<bool> clipActive{ false };

private:
    // Atomic mirrors of parameters (safe for audio thread)
    std::atomic<int>   gainStepIndex{ 6 };      // 0..11 → 5..60 dB
    std::atomic<bool>  phantomPower{ false };
    std::atomic<bool>  polarityInvert{ false };
    std::atomic<bool>  oversampleEnable{ false };
    std::atomic<float> outputLevel{ 1.0f };     // 0..1 → -inf..0 dB

    double currentSampleRate = 44100.0;
    int    currentBlockSize = 512;

    // Per‑channel state
    std::vector<float> lastSample;   // slew limiter memory
    std::vector<float> dcState;      // DC servo integrator

    // Pre‑computed coefficients
    float dcAlphaBase = 0.0f;
    float dcAlphaOS = 0.0f;
    float slewBase = 0.0f;
    float slewOS = 0.0f;

    static constexpr int kOversamplingOrder = 1;   // 2× oversampling
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;

    static float gainLinearFromStep(int step) noexcept;
    void resetChannelState();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Earthworks521Processor)
};