#pragma once
#include <juce_audio_basics/juce_audio_basics.h>

namespace vt
{
// A sample loaded into the talk engine. Immutable once built, shared between the
// message thread (GUI, saving) and the audio thread (playback).
struct SampleData : juce::ReferenceCountedObject
{
    using Ptr = juce::ReferenceCountedObjectPtr<SampleData>;

    juce::AudioBuffer<float> audio;   // mono
    double sampleRate = 44100.0;
    float rootHz = 0.0f;              // detected pitch, 0 = unpitched
    juce::String name;
    std::vector<float> peaks;         // 0..1 per display column, for the waveform

    int length() const { return audio.getNumSamples(); }
};
}
