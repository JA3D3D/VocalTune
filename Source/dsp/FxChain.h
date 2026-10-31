#pragma once
// Character effects after the voice engine: Buzz (ring mod), Crush, Grit, Tone, Metal (comb),
// Radio, Choir (stereo doubler), Echo (ping-pong) and Space (reverb).
#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>
#include <vector>

namespace vt
{
struct FxSettings
{
    float buzz = 0, buzzHz = 60, crush = 0, decimate = 0, grit = 0, tone = 0;
    float metal = 0, metalHz = 220, radio = 0, choir = 0;
    float echo = 0, echoTime = 280, echoFeedback = 0.35f, space = 0, spaceSize = 0.6f;
};

struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    float process (float x) { float y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; }
    void reset() { z1 = z2 = 0; }
    void set (double sr, int type, double f, double q, double dB = 0)   // 0 LP, 1 HP, 2 low shelf, 3 high shelf
    {
        const double w = juce::MathConstants<double>::twoPi * juce::jlimit (10.0, sr * 0.45, f) / sr;
        const double cw = std::cos (w), sw = std::sin (w), A = std::pow (10.0, dB / 40.0);
        double B0, B1, B2, A0, A1, A2;
        if (type <= 1)
        {
            const double al = sw / (2 * q);
            if (type == 0) { B0 = (1 - cw) / 2; B1 = 1 - cw; B2 = (1 - cw) / 2; }
            else           { B0 = (1 + cw) / 2; B1 = -(1 + cw); B2 = (1 + cw) / 2; }
            A0 = 1 + al; A1 = -2 * cw; A2 = 1 - al;
        }
        else
        {
            const double al = sw / 2 * std::sqrt (2.0), sA = 2 * std::sqrt (A) * al / std::sqrt (2.0);
            if (type == 2)
            {
                B0 = A * ((A + 1) - (A - 1) * cw + sA); B1 = 2 * A * ((A - 1) - (A + 1) * cw); B2 = A * ((A + 1) - (A - 1) * cw - sA);
                A0 = (A + 1) + (A - 1) * cw + sA; A1 = -2 * ((A - 1) + (A + 1) * cw); A2 = (A + 1) + (A - 1) * cw - sA;
            }
            else
            {
                B0 = A * ((A + 1) + (A - 1) * cw + sA); B1 = -2 * A * ((A - 1) + (A + 1) * cw); B2 = A * ((A + 1) + (A - 1) * cw - sA);
                A0 = (A + 1) - (A - 1) * cw + sA; A1 = 2 * ((A - 1) - (A + 1) * cw); A2 = (A + 1) - (A - 1) * cw - sA;
            }
        }
        b0 = (float) (B0 / A0); b1 = (float) (B1 / A0); b2 = (float) (B2 / A0); a1 = (float) (A1 / A0); a2 = (float) (A2 / A0);
    }
};

class FxChain
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        metalLine.assign ((size_t) (sr / 30.0) + 4, 0.0f);
        for (auto& l : choirLine) l.assign ((size_t) (sr * 0.06) + 4, 0.0f);
        for (auto& l : echoLine) l.assign ((size_t) (sr * 1.6) + 4, 0.0f);
        radioHp.set (sr, 1, 450, 0.8); radioLp.set (sr, 0, 3000, 0.9);
        radioHp2.set (sr, 1, 450, 0.8); radioLp2.set (sr, 0, 3000, 0.9);
        for (auto& f : echoDamp) f.set (sr, 0, 4500, 0.7);
        reverb.setSampleRate (sr);
        smoothK = 1.0f - (float) std::exp (-1.0 / (0.02 * sr));
        levelK = 1.0f - (float) std::exp (-1.0 / (0.12 * sr));
        lastTone = 99.0f;
        reset();
    }

    void reset()
    {
        std::fill (metalLine.begin(), metalLine.end(), 0.0f);
        for (auto& l : choirLine) std::fill (l.begin(), l.end(), 0.0f);
        for (auto& l : echoLine) std::fill (l.begin(), l.end(), 0.0f);
        for (auto* b : { &radioHp, &radioLp, &radioHp2, &radioLp2, &lowShelf, &highShelf }) b->reset();
        for (auto& f : echoDamp) f.reset();
        reverb.reset();
        holdCount = 0; held = 0; pIn = pOut = 0; autoGain = 1;
    }

    // mono in -> stereo out
    void process (const float* in, float* outL, float* outR, int num, const FxSettings& s)
    {
        if (std::abs (s.tone - lastTone) > 0.001f)
        {
            lastTone = s.tone;
            // dark = less treble (plus a little warmth), bright = less mud (plus a little air)
            lowShelf.set (sr, 2, 280, 0.7, s.tone < 0 ? -s.tone * 3.0 : -s.tone * 9.0);
            highShelf.set (sr, 3, 3200, 0.7, s.tone < 0 ? s.tone * 15.0 : s.tone * 6.0);
        }
        const float bits = 16.0f - s.crush * 13.0f;
        const float q = std::exp2 (bits - 1.0f);
        const int holdN = 1 + (int) std::round (s.decimate * s.decimate * 40.0f);
        const float drive = 1.0f + s.grit * s.grit * 40.0f;
        const float driveComp = 1.0f / std::sqrt (drive);
        const float buzzInc = juce::MathConstants<float>::twoPi * s.buzzHz / (float) sr;
        const float metalFb = 0.55f + 0.4f * s.metal;
        const int mlen = (int) metalLine.size();
        const int clen = (int) choirLine[0].size();
        const int elen = (int) echoLine[0].size();
        const bool colour = s.buzz > 0.0001f || s.crush > 0.0001f || s.decimate > 0.0001f || s.grit > 0.0001f
                         || s.metal > 0.0001f || s.radio > 0.0001f || std::abs (s.tone) > 0.001f;

        for (int i = 0; i < num; ++i)
        {
            sm (sBuzz, s.buzz); sm (sMetal, s.metal); sm (sRadio, s.radio); sm (sChoir, s.choir); sm (sEcho, s.echo);
            sm (sMetalD, (float) sr / s.metalHz); sm (sEchoT, s.echoTime * 0.001f * (float) sr);

            float x = in[i];
            const float x0 = x;
            // buzz
            if (sBuzz > 0.0001f)
            {
                buzzPhase += buzzInc; if (buzzPhase > juce::MathConstants<float>::twoPi) buzzPhase -= juce::MathConstants<float>::twoPi;
                x *= (1.0f - sBuzz) + sBuzz * std::sin (buzzPhase) * 1.3f;
            }
            // crush + decimate
            if (s.crush > 0.0001f) x = std::round (x * q) / q;
            if (holdN > 1) { if (holdCount++ % holdN == 0) held = x; x = held; }
            // grit
            if (s.grit > 0.0001f)
            {
                const float d = std::tanh (x * drive + 0.08f * s.grit) - std::tanh (0.08f * s.grit);
                x = x + (d * driveComp * 1.4f - x) * std::min (1.0f, s.grit * 2.5f);
            }
            // tone
            if (lastTone != 0.0f) x = highShelf.process (lowShelf.process (x));
            // metal
            {
                const float d = juce::jlimit (2.0f, (float) mlen - 3.0f, sMetalD);
                float rp = (float) metalPos - d; if (rp < 0) rp += (float) mlen;
                const int i0 = (int) rp; const float f = rp - (float) i0;
                const float del = metalLine[(size_t) i0] * (1 - f) + metalLine[(size_t) ((i0 + 1) % mlen)] * f;
                const float y = x + del * metalFb;
                metalLine[(size_t) metalPos] = std::tanh (y);
                metalPos = (metalPos + 1) % mlen;
                x = x + (y * 0.45f - x) * sMetal;
            }
            // radio
            if (sRadio > 0.0001f)
            {
                float r = radioLp2.process (radioLp.process (radioHp2.process (radioHp.process (x))));
                r = std::tanh (r * 2.5f) * 0.75f;
                x = x + (r - x) * sRadio;
            }
            // keep the colour effects at roughly the level that went in, so presets are easy to compare
            pIn += (x0 * x0 - pIn) * levelK;
            pOut += (x * x - pOut) * levelK;
            const float want = colour ? juce::jlimit (0.25f, 3.0f, std::sqrt ((pIn + 1.0e-9f) / (pOut + 1.0e-9f))) : 1.0f;
            autoGain += (want - autoGain) * smoothK;
            x *= autoGain;
            // choir: three wobbling copies per side
            float L = x, R = x;
            choirLine[0][(size_t) choirPos] = x;
            if (sChoir > 0.0001f)
            {
                static constexpr float base[3] { 0.011f, 0.017f, 0.025f };
                static constexpr float rate[3] { 0.31f, 0.47f, 0.73f };
                float cl = 0, cr = 0;
                for (int v = 0; v < 3; ++v)
                {
                    choirLfo[v] += rate[v] / (float) sr; if (choirLfo[v] > 1) choirLfo[v] -= 1;
                    const float ph = choirLfo[v] * juce::MathConstants<float>::twoPi;
                    cl += tap (choirLine[0], choirPos, (base[v] + 0.0035f * std::sin (ph)) * (float) sr, clen);
                    cr += tap (choirLine[0], choirPos, (base[v] + 0.004f + 0.0035f * std::cos (ph)) * (float) sr, clen);
                }
                L = x * (1.0f - 0.35f * sChoir) + cl * 0.45f * sChoir;
                R = x * (1.0f - 0.35f * sChoir) + cr * 0.45f * sChoir;
            }
            choirPos = (choirPos + 1) % clen;
            // echo (ping-pong)
            {
                const float d = juce::jlimit (4.0f, (float) elen - 3.0f, sEchoT);
                const float el = tap (echoLine[0], echoPos, d, elen);
                const float er = tap (echoLine[1], echoPos, d, elen);
                echoLine[0][(size_t) echoPos] = echoDamp[0].process ((L + R) * 0.5f * sEcho + er * s.echoFeedback);
                echoLine[1][(size_t) echoPos] = echoDamp[1].process (el * s.echoFeedback);
                echoPos = (echoPos + 1) % elen;
                L += el; R += er;
            }
            outL[i] = L; outR[i] = R;
        }

        if (s.space > 0.0001f || reverbTail > 0)
        {
            juce::Reverb::Parameters rp;
            rp.roomSize = 0.3f + 0.69f * s.spaceSize;
            rp.damping = 0.55f - 0.3f * s.spaceSize;
            rp.width = 1.0f;
            rp.wetLevel = 0.3f * s.space;
            rp.dryLevel = 1.0f - 0.4f * s.space;
            reverb.setParameters (rp);
            reverb.processStereo (outL, outR, num);
            reverbTail = s.space > 0.0001f ? (int) (sr * 8) : std::max (0, reverbTail - num);
        }
    }

private:
    void sm (float& v, float target) { v += (target - v) * smoothK; }
    static float tap (const std::vector<float>& line, int writePos, float delay, int len)
    {
        float rp = (float) writePos - delay;
        while (rp < 0) rp += (float) len;
        const int i0 = (int) rp % len; const float f = rp - std::floor (rp);
        return line[(size_t) i0] * (1 - f) + line[(size_t) ((i0 + 1) % len)] * f;
    }

    double sr = 44100;
    float levelK = 0.001f, pIn = 0, pOut = 0, autoGain = 1;
    float smoothK = 0.001f, lastTone = 99.0f, buzzPhase = 0, held = 0;
    float sBuzz = 0, sMetal = 0, sRadio = 0, sChoir = 0, sEcho = 0, sMetalD = 200, sEchoT = 10000;
    int holdCount = 0, metalPos = 0, choirPos = 0, echoPos = 0, reverbTail = 0;
    float choirLfo[3] { 0.0f, 0.33f, 0.66f };
    std::vector<float> metalLine;
    std::vector<float> choirLine[1];
    std::vector<float> echoLine[2];
    Biquad radioHp, radioLp, radioHp2, radioLp2, lowShelf, highShelf, echoDamp[2];
    juce::Reverb reverb;
};
}
