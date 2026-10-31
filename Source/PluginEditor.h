#pragma once
#include "PluginProcessor.h"
#include "ToyLookAndFeel.h"

class VocalTuneContent;

class VocalTuneEditor : public juce::AudioProcessorEditor
{
public:
    static constexpr int baseW = 1040, baseH = 574;

    explicit VocalTuneEditor (VocalTuneProcessor&);
    ~VocalTuneEditor() override;
    void resized() override;
    void paint (juce::Graphics&) override;

    VocalTuneContent& getContent() { return *content; }
    void showPage (int page);   // 0 toy box, 1 Auto Toon

private:
    VocalTuneProcessor& proc;
    toy::LookAndFeel lnf;
    std::unique_ptr<VocalTuneContent> content;
    std::unique_ptr<juce::Component> toon;
    juce::TooltipWindow tooltips { this, 600 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VocalTuneEditor)
};
