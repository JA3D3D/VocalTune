#pragma once
// The heart of VocalTune. Each FFT frame of the voice is split into
//   * an envelope (the shape of the throat and mouth: formants), and
//   * the excitation (the buzz of the vocal cords: pitch).
// The voice changer moves the pitch and the formants independently (phase vocoder with
// peak-locked phase, after Laroche & Dolson). The talk engine keeps the voice's envelope but
// swaps the excitation for any other sound, so a synth, a sample or the sidechain "talks".
#include "PitchDetector.h"
#include "Carrier.h"
#include <atomic>
#include <complex>

namespace vt
{
struct EngineSettings
{
    float pitchSemis = 0, formantSemis = 0, whisper = 0, wobble = 0, wobbleRate = 5;
    float tune = 0, tuneSpeedMs = 25; int key = 0; int scaleMask = 0xFFF;
    float robot = 0, robotNote = 48;
    float talk = 0; int talkOctave = 0; int bands = 0; float character = 0.9f, sibilance = 0.4f;
    CarrierGen::Settings carrier;
};

class SpectralEngine
{
public:
    void prepare (double sampleRate);
    void reset();
    int getLatency() const { return n; }

    // in: mono voice, side: mono sidechain (may be null), out: mono result
    void process (const float* in, const float* side, float* out, int numSamples,
                  const EngineSettings& s, const SampleData* sample);

    // read by the GUI
    std::atomic<float> detectedMidi { -1.0f }, targetMidi { -1.0f }, inputLevel { 0.0f };

    // scale snapping, exposed for the tests
    static float snapToScale (float midi, int key, int mask);

private:
    void processFrame (const EngineSettings& s, const SampleData* sample);
    void envelope (const float* mag, float* env, int iterations);
    void fwd (float* d) const { fft->performRealOnlyForwardTransform (d, true); }
    void inv (float* d) const;

    double sr = 44100.0;
    int order = 11, n = 2048, hop = 512, half = 1024, lifter = 60;
    float olaNorm = 1.0f, invScale = 1.0f, hopSeconds = 0.01f;
    std::unique_ptr<juce::dsp::FFT> fft;
    PitchDetector yin;
    CarrierGen carrier;

    std::vector<float> window, inBuf, carBuf, outAccum, outHop, work, work2, cwork;
    std::vector<float> mag, prevPhase, rot, newRot, env, envWarp, envTalk, cmag, cenv;
    std::vector<std::complex<float>> X, Y, T;
    std::vector<int> peaks;
    int counter = 0;

    float corr = 0.0f, lfoPhase = 0.0f, carrierMidi = 48.0f, talkGain = 0.0f, voiceNorm = 1.0f;
    int quietFrames = 100;
    juce::Random rng;
};
}
