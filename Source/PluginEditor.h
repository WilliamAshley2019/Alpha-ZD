#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
// Custom LED indicator (round, any colour)
class LedComponent : public juce::Component
{
public:
    LedComponent(juce::Colour onColour, juce::Colour offColour)
        : onColour(onColour), offColour(offColour)
    {
        setSize(16, 16);
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced(2.0f);
        g.setColour(isOn ? onColour : offColour);
        g.fillEllipse(bounds);
        g.setColour(juce::Colours::black);
        g.drawEllipse(bounds, 1.0f);
    }

    void setOn(bool on)
    {
        if (isOn != on)
        {
            isOn = on;
            repaint();
        }
    }

private:
    juce::Colour onColour, offColour;
    bool isOn = false;
};

//==============================================================================
// Custom rotary knob LookAndFeel (green / earth tone)
class GreenMetalKnobLNF : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider(juce::Graphics& g,
        int x, int y, int w, int h,
        float sliderPos,
        float rotaryStartAngle,
        float rotaryEndAngle,
        juce::Slider&) override
    {
        auto bounds = juce::Rectangle<float>(
            static_cast<float>(x),
            static_cast<float>(y),
            static_cast<float>(w),
            static_cast<float>(h)).reduced(4.0f);

        const float radius = bounds.getWidth() * 0.5f;
        const float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

        // Outer ring (dark green)
        g.setColour(juce::Colour(34, 68, 34));
        g.fillEllipse(bounds);

        // Inner gradient
        juce::ColourGradient grad(
            juce::Colour(85, 128, 85),  // light green
            bounds.getCentreX(), bounds.getY(),
            juce::Colour(34, 68, 34),   // dark green
            bounds.getCentreX(), bounds.getBottom(),
            false);
        g.setGradientFill(grad);
        g.fillEllipse(bounds.reduced(radius * 0.15f));

        // Indicator (gold)
        juce::Path p;
        p.addRectangle(-1.5f, -radius * 0.6f, 3.0f, radius * 0.4f);
        g.setColour(juce::Colour(0xFFD4AF37));
        g.fillPath(p, juce::AffineTransform::rotation(angle).translated(bounds.getCentre()));
    }
};

//==============================================================================
class Earthworks521Editor : public juce::AudioProcessorEditor,
    private juce::Timer
{
public:
    Earthworks521Editor(Earthworks521Processor&);
    ~Earthworks521Editor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    Earthworks521Processor& audioProcessor;

    GreenMetalKnobLNF greenKnobLNF;

    // Knobs
    juce::Slider gainKnob;
    juce::Slider outputKnob;

    // LEDs (colours defined in constructor)
    std::unique_ptr<LedComponent> phantomLed;
    std::unique_ptr<LedComponent> polarityLed;
    std::unique_ptr<LedComponent> oversampleLed;
    std::unique_ptr<LedComponent> clipLed;

    // Toggle buttons
    juce::ToggleButton phantomButton;
    juce::ToggleButton polarityButton;
    juce::ToggleButton oversampleButton;

    juce::Label gainLabel, outputLabel, modelLabel;

    using APVTS = juce::AudioProcessorValueTreeState;
    APVTS& apvts;

    std::unique_ptr<APVTS::SliderAttachment> gainAttachment;
    std::unique_ptr<APVTS::SliderAttachment> outputAttachment;
    std::unique_ptr<APVTS::ButtonAttachment> phantomAttachment;
    std::unique_ptr<APVTS::ButtonAttachment> polarityAttachment;
    std::unique_ptr<APVTS::ButtonAttachment> oversampleAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Earthworks521Editor)
};