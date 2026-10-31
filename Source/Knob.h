#pragma once
// A knob with its name above and its value below. "Toy" style: white text with a shadow;
// "ink" style: black cartoon lettering on paper.
#include "ToyLookAndFeel.h"

class ToyKnob : public juce::Component
{
public:
    ToyKnob (juce::AudioProcessorValueTreeState& st, const char* pid, const juce::String& label, juce::Colour accent,
             bool bipolar = false, bool inkStyle = false)
        : text (label), param (st.getParameter (pid)), ink (inkStyle)
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        slider.setColour (juce::Slider::rotarySliderFillColourId, accent);
        slider.getProperties().set ("bipolar", bipolar);
        slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.22f, juce::MathConstants<float>::pi * 2.78f, true);
        slider.setTooltip (param->getName (64) + " - drag, or double-click to reset");
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (st, pid, slider);
        slider.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));
        slider.onValueChange = [this] { repaint(); };
        addAndMakeVisible (slider);
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        if (ink)
        {
            g.setColour (juce::Colour (0xff1A1410));
            g.setFont (toy::titleFont (15.5f));
            g.drawText (text, r.removeFromTop (19.0f).translated (0, 2), juce::Justification::centred, false);
            g.setFont (toy::bodyFont (12.5f));
            g.setColour (juce::Colour (0xff4A3B2C));
            g.drawText (param->getCurrentValueAsText(), r.removeFromBottom (15.0f), juce::Justification::centred, false);
            return;
        }
        g.setFont (toy::bodyFont (13.5f));
        toy::shadowText (g, text, r.removeFromTop (17.0f), juce::Justification::centred);
        g.setFont (toy::bodyFont (12.0f));
        toy::shadowText (g, param->getCurrentValueAsText(), r.removeFromBottom (15.0f), juce::Justification::centred, toy::cream, 1.0f);
    }
    void resized() override
    {
        auto r = getLocalBounds().withTrimmedTop (ink ? 19 : 17).withTrimmedBottom (15);
        const int s = std::min (r.getWidth(), r.getHeight());
        slider.setBounds (r.withSizeKeepingCentre (s, s));
    }

    juce::Slider slider;

private:
    juce::String text;
    juce::RangedAudioParameter* param;
    bool ink;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};
