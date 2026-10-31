#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "Params.h"
#include "Presets.h"
#include "dsp/SpectralEngine.h"
#include "dsp/FxChain.h"

class VocalTuneProcessor : public juce::AudioProcessor, private juce::Timer
{
public:
    VocalTuneProcessor();
    ~VocalTuneProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 6.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return presetName; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // ---- samples (message thread)
    juce::String loadSample (const juce::File& file);        // returns an error message, or empty on success
    void loadSampleBuffer (const juce::AudioBuffer<float>& audio, double rate, const juce::String& name);
    void clearSample();
    vt::SampleData::Ptr getSample() const;
    static float detectRoot (const juce::AudioBuffer<float>& mono, double rate);

    // ---- presets (message thread)
    struct PresetEntry { juce::String name, category; int factoryIndex = -1; juce::File file; };
    juce::Array<PresetEntry> presetList() const;
    void applyPreset (const PresetEntry&);
    void stepPreset (int delta);
    bool saveUserPreset (const juce::String& name);
    void dice();
    void mutateCurrent();
    juce::String getPresetName() const { return presetName; }

    // ---- Auto Toon piano (message thread)
    int absoluteNoteMask() const;       // bit n = pitch class n (C = 0) is a target note
    void toggleNote (int pitchClass);   // switches the scale to Custom on first use
    std::function<void()> onPresetChanged;    // set by the editor

    juce::AudioProcessorValueTreeState state;
    vt::SpectralEngine engine;
    float editorScale = 1.0f;
    int page = 0;   // 0 toy box, 1 Auto Toon

private:
    void timerCallback() override;
    std::unique_ptr<juce::XmlElement> makeStateXml (bool includeSample);
    void restoreFromXml (const juce::XmlElement&, bool keepSampleIfMissing);
    void setPresetName (const juce::String& n, int idx);
    void readSettings (vt::EngineSettings&, vt::FxSettings&) const;

    vt::FxChain fx;
    juce::AudioFormatManager formats;
    mutable juce::SpinLock sampleLock;
    vt::SampleData::Ptr pendingSample, audioSample;
    juce::Array<vt::SampleData::Ptr> retired;
    std::atomic<bool> sampleChanged { false };

    juce::AudioBuffer<float> monoIn, monoSide, monoOut, wet, dryDelay;
    int dryPos = 0, latency = 0;
    float outGain = 1.0f, mixSm = 1.0f;
    juce::String presetName { "Clean Slate" };
    int presetIndex = 0;
    juce::Random rng;

    std::unordered_map<std::string, std::atomic<float>*> raw;
    float p (const char* pid) const { return raw.at (pid)->load(); }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VocalTuneProcessor)
};
