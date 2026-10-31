#pragma once
// Auto Toon: VocalTune's flip side. A pro auto-tune view in a 1930s inkwell cartoon style.
#include "PluginProcessor.h"

std::unique_ptr<juce::Component> createAutoToonPage (VocalTuneProcessor&, std::function<void()> onBack);

// the little "AUTO TOON" sign button that sits on the toy box page
std::unique_ptr<juce::Button> createAutoToonButton();
