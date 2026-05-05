#include "PluginEditor.h"

//==============================================================================
Earthworks521Editor::Earthworks521Editor(Earthworks521Processor& p)
    : AudioProcessorEditor(&p), audioProcessor(p), apvts(p.apvts)
{
    // Create LEDs with specific colours
    phantomLed = std::make_unique<LedComponent>(juce::Colour(0xFFD4AF37), juce::Colour(0xFF3D2B1F)); // gold / dark brown
    polarityLed = std::make_unique<LedComponent>(juce::Colour(0xFF00AA00), juce::Colour(0xFF3D2B1F)); // green / dark brown
    oversampleLed = std::make_unique<LedComponent>(juce::Colour(0xFF3399FF), juce::Colour(0xFF3D2B1F)); // blue / dark brown
    clipLed = std::make_unique<LedComponent>(juce::Colours::red, juce::Colour(0xFF3D2B1F));

    // ------------------- Gain Knob -------------------
    gainKnob.setLookAndFeel(&greenKnobLNF);
    gainKnob.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    gainKnob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 18);
    gainKnob.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(60, 40, 20));
    gainKnob.setColour(juce::Slider::textBoxTextColourId, juce::Colours::lightyellow);
    gainKnob.onValueChange = [this]()
        {
            int step = (int)gainKnob.getValue();
            int gainDb = 5 + step * 5;
            gainKnob.setTextValueSuffix(" dB");
            gainKnob.setNumDecimalPlacesToDisplay(0);
            gainKnob.setTextValueSuffix(juce::String(gainDb) + " dB");
        };
    gainKnob.setValue(6, juce::dontSendNotification);
    gainKnob.onValueChange(); // initialise text
    addAndMakeVisible(gainKnob);
    gainLabel.setText("Gain", juce::dontSendNotification);
    gainLabel.setJustificationType(juce::Justification::centred);
    gainLabel.setColour(juce::Label::textColourId, juce::Colours::lightyellow);
    gainLabel.attachToComponent(&gainKnob, false);
    addAndMakeVisible(gainLabel);

    // ------------------- Output Knob -------------------
    outputKnob.setLookAndFeel(&greenKnobLNF);
    outputKnob.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    outputKnob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 18);
    outputKnob.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(60, 40, 20));
    outputKnob.setColour(juce::Slider::textBoxTextColourId, juce::Colours::lightyellow);
    outputKnob.onValueChange = [this]()
        {
            float val = (float)outputKnob.getValue();
            float dB = juce::Decibels::gainToDecibels(val, -100.0f);
            outputKnob.setTextValueSuffix(" dB");
            outputKnob.setNumDecimalPlacesToDisplay(1);
            outputKnob.setTextValueSuffix(juce::String(dB, 1) + " dB");
        };
    outputKnob.setValue(1.0f, juce::dontSendNotification);
    outputKnob.onValueChange();
    addAndMakeVisible(outputKnob);
    outputLabel.setText("Output", juce::dontSendNotification);
    outputLabel.setJustificationType(juce::Justification::centred);
    outputLabel.setColour(juce::Label::textColourId, juce::Colours::lightyellow);
    outputLabel.attachToComponent(&outputKnob, false);
    addAndMakeVisible(outputLabel);

    // ------------------- Model Label -------------------
    modelLabel.setText("AlphaZD  \xe2\x80\xa2  Model E521", juce::dontSendNotification);
    modelLabel.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    modelLabel.setColour(juce::Label::textColourId, juce::Colour(0xFFD4AF37));
    modelLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(modelLabel);

    // ------------------- Buttons -------------------
    phantomButton.setButtonText(u8"+48V");
    phantomButton.setColour(juce::TextButton::buttonColourId, juce::Colour(34, 68, 34));
    phantomButton.setColour(juce::TextButton::textColourOffId, juce::Colours::lightyellow);
    phantomButton.setClickingTogglesState(true);
    addAndMakeVisible(phantomButton);
    addAndMakeVisible(*phantomLed);

    polarityButton.setButtonText(u8"INV / NORM");
    polarityButton.setColour(juce::TextButton::buttonColourId, juce::Colour(34, 68, 34));
    polarityButton.setColour(juce::TextButton::textColourOffId, juce::Colours::lightyellow);
    polarityButton.setClickingTogglesState(true);
    addAndMakeVisible(polarityButton);
    addAndMakeVisible(*polarityLed);

    oversampleButton.setButtonText("Oversampling");
    oversampleButton.setColour(juce::TextButton::buttonColourId, juce::Colour(34, 68, 34));
    oversampleButton.setColour(juce::TextButton::textColourOffId, juce::Colours::lightyellow);
    oversampleButton.setClickingTogglesState(true);
    addAndMakeVisible(oversampleButton);
    addAndMakeVisible(*oversampleLed);

    addAndMakeVisible(*clipLed);
    clipLed->setEnabled(false); // just a display

    // ------------------- Attachments -------------------
    gainAttachment = std::make_unique<APVTS::SliderAttachment>(apvts, "gain", gainKnob);
    outputAttachment = std::make_unique<APVTS::SliderAttachment>(apvts, "output", outputKnob);
    phantomAttachment = std::make_unique<APVTS::ButtonAttachment>(apvts, "phantom", phantomButton);
    polarityAttachment = std::make_unique<APVTS::ButtonAttachment>(apvts, "polarity", polarityButton);
    oversampleAttachment = std::make_unique<APVTS::ButtonAttachment>(apvts, "oversample", oversampleButton);

    setSize(520, 280);
    startTimerHz(30);
}

Earthworks521Editor::~Earthworks521Editor()
{
    stopTimer();
    gainKnob.setLookAndFeel(nullptr);
    outputKnob.setLookAndFeel(nullptr);
}

//==============================================================================
void Earthworks521Editor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(75, 54, 33));  // earth brown
    g.setColour(juce::Colour(50, 35, 20));
    g.drawRect(getLocalBounds(), 2);
    g.setColour(juce::Colour(0xFFD4AF37).withAlpha(0.5f));
    g.drawLine(20, 60, getWidth() - 20, 60, 1.5f);
}

void Earthworks521Editor::resized()
{
    auto area = getLocalBounds().reduced(15);
    auto topArea = area.removeFromTop(50);
    modelLabel.setBounds(topArea);

    auto mainArea = area;
    const int knobWidth = 90;
    auto knobRow = mainArea.removeFromTop(130);
    gainKnob.setBounds(knobRow.removeFromLeft(knobWidth).reduced(5));
    outputKnob.setBounds(knobRow.removeFromLeft(knobWidth).reduced(5));

    auto buttonPanel = mainArea.reduced(10);
    const int rowHeight = 40;

    // Phantom row
    auto phantomRow = buttonPanel.removeFromTop(rowHeight);
    phantomLed->setBounds(phantomRow.removeFromLeft(20));
    phantomButton.setBounds(phantomRow);

    // Polarity row
    auto polarityRow = buttonPanel.removeFromTop(rowHeight);
    polarityLed->setBounds(polarityRow.removeFromLeft(20));
    polarityButton.setBounds(polarityRow);

    // Oversampling row
    auto oversampleRow = buttonPanel.removeFromTop(rowHeight);
    oversampleLed->setBounds(oversampleRow.removeFromLeft(20));
    oversampleButton.setBounds(oversampleRow);

    // Clip LED at bottom right corner
    clipLed->setBounds(getWidth() - 35, getHeight() - 35, 20, 20);
}

void Earthworks521Editor::timerCallback()
{
    // Poll clip flag from audio thread (clear it)
    bool clipping = audioProcessor.clipActive.exchange(false);
    clipLed->setOn(clipping);

    // Update LEDs from button states
    phantomLed->setOn(phantomButton.getToggleState());
    polarityLed->setOn(polarityButton.getToggleState());
    oversampleLed->setOn(oversampleButton.getToggleState());
}