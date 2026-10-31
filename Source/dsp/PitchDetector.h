#pragma once
// YIN pitch detection, computed with FFT correlation so it is cheap enough to run every hop.
#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <vector>

namespace vt
{
class PitchDetector
{
public:
    struct Result { float hz = 0.0f; float aperiodicity = 1.0f; float rms = 0.0f; };

    // frameSize must be a power of two; the integration window is half of it
    void prepare (double sampleRate, int frameOrder)
    {
        sr = sampleRate;
        order = frameOrder;
        n = 1 << order;
        w = n / 2;
        fft = std::make_unique<juce::dsp::FFT> (order);
        a.assign ((size_t) n * 2, 0.0f);
        b.assign ((size_t) n * 2, 0.0f);
        d.assign ((size_t) w, 0.0f);
        ps.assign ((size_t) n + 1, 0.0f);

        // calibrate the inverse scaling of this FFT engine
        std::vector<float> t ((size_t) n * 2, 0.0f);
        for (int i = 0; i < n; ++i) t[(size_t) i] = std::sin ((float) i * 0.37f) + 0.25f;
        std::vector<float> orig (t.begin(), t.begin() + n);
        fft->performRealOnlyForwardTransform (t.data(), true);
        fft->performRealOnlyInverseTransform (t.data());
        double num = 0, den = 0;
        for (int i = 0; i < n; ++i) { num += t[(size_t) i] * orig[(size_t) i]; den += orig[(size_t) i] * orig[(size_t) i]; }
        invScale = (float) (den / std::max (1e-20, num));
    }

    void setRange (float minHz, float maxHz) { lo = minHz; hi = maxHz; }

    Result detect (const float* x)
    {
        Result r;
        ps[0] = 0.0f;
        for (int i = 0; i < n; ++i) ps[(size_t) i + 1] = ps[(size_t) i] + x[i] * x[i];
        const float e1 = ps[(size_t) w];
        r.rms = std::sqrt (e1 / (float) w);
        if (r.rms < 2.0e-4f) return r;

        std::fill (a.begin(), a.end(), 0.0f);
        std::copy (x, x + w, a.begin());
        std::fill (b.begin(), b.end(), 0.0f);
        std::copy (x, x + n, b.begin());
        fft->performRealOnlyForwardTransform (a.data(), true);
        fft->performRealOnlyForwardTransform (b.data(), true);
        for (int k = 0; k <= n / 2; ++k)
        {
            const float ar = a[(size_t) 2 * k], ai = -a[(size_t) 2 * k + 1];   // conj(A)
            const float br = b[(size_t) 2 * k], bi = b[(size_t) 2 * k + 1];
            a[(size_t) 2 * k]     = ar * br - ai * bi;
            a[(size_t) 2 * k + 1] = ar * bi + ai * br;
        }
        fft->performRealOnlyInverseTransform (a.data());

        const int tauMin = std::max (2, (int) (sr / hi));
        const int tauMax = std::min (w - 2, (int) (sr / lo));
        float running = 0.0f;
        d[0] = 1.0f;
        for (int tau = 1; tau <= tauMax + 1; ++tau)
        {
            const float e2 = ps[(size_t) (tau + w)] - ps[(size_t) tau];
            const float diff = std::max (0.0f, e1 + e2 - 2.0f * a[(size_t) tau] * invScale);
            running += diff;
            d[(size_t) tau] = running > 0.0f ? diff * (float) tau / running : 1.0f;
        }

        int best = -1;
        for (int tau = tauMin; tau <= tauMax; ++tau)
        {
            if (d[(size_t) tau] < threshold)
            {
                while (tau + 1 <= tauMax && d[(size_t) tau + 1] < d[(size_t) tau]) ++tau;
                best = tau;
                break;
            }
        }
        if (best < 0)
        {
            float m = 2.0f;
            for (int tau = tauMin; tau <= tauMax; ++tau)
                if (d[(size_t) tau] < m) { m = d[(size_t) tau]; best = tau; }
            if (best < 0 || m > 0.35f) { r.aperiodicity = std::min (1.0f, m); return r; }
        }

        float t = (float) best;
        if (best > 1 && best < tauMax)
        {
            const float s0 = d[(size_t) best - 1], s1 = d[(size_t) best], s2 = d[(size_t) best + 1];
            const float den = s0 - 2.0f * s1 + s2;
            if (std::abs (den) > 1e-9f) t += 0.5f * (s0 - s2) / den;
        }
        r.hz = (float) (sr / t);
        r.aperiodicity = d[(size_t) best];
        return r;
    }

private:
    double sr = 44100.0;
    int order = 11, n = 2048, w = 1024;
    float lo = 60.0f, hi = 1000.0f, threshold = 0.12f, invScale = 1.0f;
    std::unique_ptr<juce::dsp::FFT> fft;
    std::vector<float> a, b, d, ps;
};
}
