#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

namespace vt
{
struct FactoryPreset
{
    juce::String name, category;
    std::vector<std::pair<juce::String, float>> values;   // real units; anything missing = default
};

const std::vector<FactoryPreset>& factoryPresets();
juce::StringArray factoryCategories();

// apply a factory preset: every parameter back to default, then the preset's values
void applyFactory (juce::AudioProcessorValueTreeState& state, const FactoryPreset& p);

// "Dice": a fresh random voice. "Mutate": nudge the current one a little.
void randomise (juce::AudioProcessorValueTreeState& state, juce::Random& rng);
void mutate (juce::AudioProcessorValueTreeState& state, juce::Random& rng);

juce::File userPresetFolder();
juce::Array<juce::File> userPresetFiles();
}
