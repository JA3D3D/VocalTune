#pragma once
// The sound that gets "talked" through: built-in synths, a loaded sample, or the sidechain.
#include "SampleData.h"
#include "../Params.h"
#include <cmath>
#include <array>

namespace vt
{
class CarrierGen
{
public:
    struct Settings
    {
        int type = carSaw;
        int chord = 0;
        bool follow = true;
        float sampleTune = 0.0f;   // semitones
        int sampleMode = 0;        // 0 loop, 1 retrigger on each new word
        float sampleStart = 0.0f;  // 0..1
    };

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        smoothCoef = (float) std::exp (-1.0 / (0.006 * sr));
        juce::Random rnd (1234);
        for (auto& v : phases) for (auto& p : v) p = rnd.nextFloat();
        for (auto& p : samplePos) p = 0.0;
        logHz = targetLogHz = std::log (130.0f);
        wasType = -1;
    }

    void setTargetHz (float hz) { if (hz > 10.0f) targetLogHz = std::log (hz); }
    void retrigger() { retrigPending = true; }

    float next (const Settings& s, const SampleData* smp, float sidechain)
    {
        logHz = targetLogHz + (logHz - targetLogHz) * smoothCoef;
        const float baseHz = std::exp (logHz);
        const auto& notes = chords()[(size_t) juce::jlimit (0, (int) chords().size() - 1, s.chord)].notes;
        const int nv = std::min ((int) notes.size(), 4);
        const float norm = 1.0f / std::sqrt ((float) nv);

        if (s.type != wasType) { wasType = s.type; retrigPending = true; }

        switch (s.type)
        {
            case carSidechain: return sidechain;
            case carNoise:     return rng.nextFloat() * 2.0f - 1.0f;
            case carSample:    return playSample (s, smp, baseHz, notes, nv) * norm;
            default: break;
        }

        float out = 0.0f;
        for (int v = 0; v < nv; ++v)
        {
            const float hz = baseHz * std::exp2 ((float) notes[(size_t) v] / 12.0f);
            const float dt = std::min (0.45f, hz / (float) sr);
            auto& ph = phases[(size_t) v];
            switch (s.type)
            {
                case carSaw:    out += saw (ph[0], dt); break;
                case carSquare: out += pulse (ph[0], dt, 0.5f); break;
                case carPulse:  out += pulse (ph[0], dt, 0.22f); break;
                case carSuper:
                {
                    static constexpr float detune[7] { -0.19f, -0.11f, -0.04f, 0.0f, 0.045f, 0.12f, 0.2f };
                    float sum = 0.0f;
                    for (int u = 0; u < 7; ++u)
                        sum += saw (ph[(size_t) u], std::min (0.45f, dt * std::exp2 (detune[u] / 12.0f)));
                    out += sum * 0.4f;
                    break;
                }
                default: break;
            }
        }
        return out * norm * 0.5f;
    }

private:
    static float blep (float t, float dt)
    {
        if (t < dt) { t /= dt; return t + t - t * t - 1.0f; }
        if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
        return 0.0f;
    }
    static float saw (float& ph, float dt)
    {
        float v = 2.0f * ph - 1.0f - blep (ph, dt);
        ph += dt; if (ph >= 1.0f) ph -= 1.0f;
        return v;
    }
    static float pulse (float& ph, float dt, float width)
    {
        float v = ph < width ? 1.0f : -1.0f;
        v += blep (ph, dt);
        float t2 = ph - width; if (t2 < 0.0f) t2 += 1.0f;
        v -= blep (t2, dt);
        ph += dt; if (ph >= 1.0f) ph -= 1.0f;
        return v - (2.0f * width - 1.0f);
    }

    float playSample (const Settings& s, const SampleData* smp, float baseHz, const std::vector<int>& notes, int nv)
    {
        if (smp == nullptr || smp->length() < 8) return 0.0f;
        const int len = smp->length();
        const double start = (double) juce::jlimit (0.0f, 0.95f, s.sampleStart) * (len - 1);
        const float* data = smp->audio.getReadPointer (0);

        if (retrigPending)
        {
            for (auto& p : samplePos) p = start;
            retrigPending = false;
        }

        const float root = smp->rootHz > 20.0f ? smp->rootHz : 261.63f;
        const double base = (s.follow ? (double) baseHz / root : 1.0)
                          * std::exp2 ((double) s.sampleTune / 12.0) * smp->sampleRate / sr;
        float out = 0.0f;
        for (int v = 0; v < nv; ++v)
        {
            double& pos = samplePos[(size_t) v];
            if (pos < start || pos >= (double) (len - 1)) pos = start + std::fmod (std::max (0.0, pos - start), std::max (1.0, (len - 1) - start));
            const int i = (int) pos;
            const float f = (float) (pos - i);
            auto at = [&] (int k) { k = juce::jlimit (0, len - 1, k); return data[k]; };
            const float y0 = at (i - 1), y1 = at (i), y2 = at (i + 1), y3 = at (i + 2);
            // cubic hermite
            const float c1 = 0.5f * (y2 - y0);
            const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
            const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
            out += ((c3 * f + c2) * f + c1) * f + y1;
            pos += juce::jlimit (0.01, 16.0, base * std::exp2 ((double) notes[(size_t) v] / 12.0));
        }
        return out;
    }

    double sr = 44100.0;
    float smoothCoef = 0.99f, logHz = 5.0f, targetLogHz = 5.0f;
    std::array<std::array<float, 7>, 4> phases {};
    std::array<double, 4> samplePos {};
    bool retrigPending = false;
    int wasType = -1;
    juce::Random rng;
};
}
