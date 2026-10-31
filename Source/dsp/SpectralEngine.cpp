#include "SpectralEngine.h"

namespace vt
{
static constexpr float twoPi = juce::MathConstants<float>::twoPi;
static inline float wrapPi (float x) { return x - twoPi * std::floor ((x + juce::MathConstants<float>::pi) / twoPi); }

void SpectralEngine::prepare (double sampleRate)
{
    sr = sampleRate;
    order = sr > 60000.0 ? 12 : 11;
    n = 1 << order;
    hop = n / 4;
    half = n / 2;
    hopSeconds = (float) (hop / sr);
    lifter = std::max (16, (int) std::round (sr * 0.0014));
    fft = std::make_unique<juce::dsp::FFT> (order);
    yin.prepare (sr, order);
    yin.setRange (60.0f, 1000.0f);
    carrier.prepare (sr);

    window.resize ((size_t) n);
    for (int i = 0; i < n; ++i) window[(size_t) i] = 0.5f - 0.5f * std::cos (twoPi * (float) i / (float) n);
    double ola = 0.0;
    for (int i = 0; i < n; i += hop) ola += (double) window[(size_t) i] * window[(size_t) i];
    // sum of window^2 across overlapping frames is constant; average it over one hop
    ola = 0.0;
    for (int j = 0; j < hop; ++j)
        for (int m = 0; m < n / hop; ++m) ola += (double) window[(size_t) (j + m * hop)] * window[(size_t) (j + m * hop)];
    olaNorm = (float) (hop / ola);

    auto sz = [] (std::vector<float>& v, int s) { v.assign ((size_t) s, 0.0f); };
    sz (inBuf, n); sz (carBuf, n); sz (outAccum, n); sz (outHop, hop);
    sz (work, 2 * n); sz (work2, 2 * n); sz (cwork, 2 * n);
    for (auto* v : { &mag, &prevPhase, &rot, &newRot, &env, &envWarp, &envTalk, &cmag, &cenv }) sz (*v, half + 1);
    X.assign ((size_t) half + 1, {}); Y.assign ((size_t) half + 1, {}); T.assign ((size_t) half + 1, {});
    peaks.reserve ((size_t) half);

    // measure the inverse scaling of this FFT engine (it differs between platforms)
    for (int i = 0; i < n; ++i) work[(size_t) i] = std::sin ((float) i * 0.29f) + 0.1f;
    std::vector<float> orig (work.begin(), work.begin() + n);
    invScale = 1.0f;
    fwd (work.data());
    fft->performRealOnlyInverseTransform (work.data());
    double num = 0, den = 0;
    for (int i = 0; i < n; ++i) { num += work[(size_t) i] * orig[(size_t) i]; den += orig[(size_t) i] * orig[(size_t) i]; }
    invScale = (float) (den / std::max (1e-20, num));
    reset();
}

void SpectralEngine::reset()
{
    for (auto* v : { &inBuf, &carBuf, &outAccum, &outHop, &prevPhase, &rot, &newRot })
        std::fill (v->begin(), v->end(), 0.0f);
    counter = 0;
    corr = 0.0f;
    talkGain = 0.0f;
    voiceNorm = 1.0f;
    quietFrames = 100;
    detectedMidi = -1.0f;
    targetMidi = -1.0f;
    currentNote = -1.0f;
    for (auto& h : historyIn) h.store (-1.0f);
    for (auto& h : historyOut) h.store (-1.0f);
}

void SpectralEngine::inv (float* d) const
{
    fft->performRealOnlyInverseTransform (d);
    juce::FloatVectorOperations::multiply (d, invScale, n);
}

float SpectralEngine::snapToScale (float midi, int key, int mask)
{
    if (mask == 0) mask = 1;
    const int base = (int) std::floor (midi);
    float best = midi, bestDist = 1e9f;
    for (int cand = base - 12; cand <= base + 12; ++cand)
    {
        const int degree = (((cand - key) % 12) + 12) % 12;
        if ((mask >> degree) & 1)
        {
            const float dist = std::abs ((float) cand - midi);
            if (dist < bestDist) { bestDist = dist; best = (float) cand; }
        }
    }
    return best;
}

void SpectralEngine::envelope (const float* m, float* e, int iterations)
{
    // cepstral smoothing, iterated towards the "true envelope" so it rides over the harmonic peaks
    std::vector<float>& target = work2;   // log magnitude we are fitting (first half+1 used)
    for (int k = 0; k <= half; ++k) target[(size_t) k] = std::log (std::max (m[k], 1.0e-9f));

    std::copy (target.begin(), target.begin() + half + 1, e);
    for (int it = 0; it < iterations; ++it)
    {
        for (int k = 0; k <= half; ++k)
        {
            const float v = it == 0 ? target[(size_t) k] : std::max (target[(size_t) k], e[k]);
            work[(size_t) 2 * k] = v;
            work[(size_t) 2 * k + 1] = 0.0f;
        }
        inv (work.data());                          // -> real cepstrum
        for (int q = lifter; q <= n - lifter; ++q) work[(size_t) q] = 0.0f;
        work[(size_t) lifter - 1] *= 0.5f;
        work[(size_t) (n - lifter + 1)] *= 0.5f;
        for (int q = n; q < 2 * n; ++q) work[(size_t) q] = 0.0f;
        fwd (work.data());
        for (int k = 0; k <= half; ++k) e[k] = work[(size_t) 2 * k];
    }
    for (int k = 0; k <= half; ++k) e[k] = std::exp (juce::jlimit (-30.0f, 10.0f, e[k]));
}

void SpectralEngine::process (const float* in, const float* side, float* out, int numSamples,
                              const EngineSettings& s, const SampleData* sample)
{
    const bool needCarrier = s.talk > 0.0001f;
    for (int i = 0; i < numSamples; ++i)
    {
        inBuf[(size_t) (n - hop + counter)] = in[i];
        if (needCarrier)
            carBuf[(size_t) (n - hop + counter)] = carrier.next (s.carrier, sample, side != nullptr ? side[i] : 0.0f);
        out[i] = outHop[(size_t) counter];
        if (++counter == hop)
        {
            processFrame (s, sample);
            std::memmove (inBuf.data(), inBuf.data() + hop, sizeof (float) * (size_t) (n - hop));
            std::memmove (carBuf.data(), carBuf.data() + hop, sizeof (float) * (size_t) (n - hop));
            counter = 0;
        }
    }
}

void SpectralEngine::processFrame (const EngineSettings& s, const SampleData* sample)
{
    // ---------------------------------------------------------------- pitch
    const auto p = yin.detect (inBuf.data());
    inputLevel = p.rms;
    const bool voiced = p.hz > 0.0f;
    const float naturalMidi = voiced ? 69.0f + 12.0f * std::log2 (p.hz / 440.0f) : carrierMidi;

    if (voiced)
    {
        float want = 0.0f;
        float speedMs = std::max (s.tuneSpeedMs, 0.0f);
        if (s.tune > 0.0f)
        {
            // pick the target note, sticking with the current one until another is clearly closer
            const float cand = snapToScale (naturalMidi, s.key, s.scaleMask);
            if (currentNote < 0.0f)
            {
                currentNote = cand; heldSeconds = 0.0f; slowMidi = naturalMidi;
            }
            else if (cand != currentNote && std::abs (naturalMidi - cand) < std::abs (naturalMidi - currentNote) - 0.18f)
            {
                currentNote = cand; heldSeconds = 0.0f; slowMidi = naturalMidi;
            }
            heldSeconds += hopSeconds;

            // split the sung pitch into the note being held and its vibrato
            slowMidi += (naturalMidi - slowMidi) * (1.0f - std::exp (-hopSeconds / 0.09f));
            const float vibGate = juce::jlimit (0.0f, 1.0f, (heldSeconds - 0.05f) / 0.12f);
            const float vib = juce::jlimit (-1.0f, 1.0f, naturalMidi - slowMidi) * vibGate;
            const float desired = currentNote + s.vibratoKeep * vib;

            // flex: leave notes alone when the singer is clearly sliding away on purpose
            float flexW = 1.0f;
            if (s.flex > 0.0f)
            {
                const float window = 1.0f - 0.9f * s.flex;
                const float dist = std::abs (slowMidi - currentNote);
                if (dist > window) flexW = juce::jlimit (0.0f, 1.0f, 1.0f - (dist - window) / 0.15f);
            }
            want = s.tune * flexW * (desired - naturalMidi);
            // humanize: long held notes get a gentler retune than the moves between them
            speedMs += s.humanize * 350.0f * juce::jlimit (0.0f, 1.0f, heldSeconds / 0.3f);
        }
        if (s.robot > 0.0f) want = want * (1.0f - s.robot) + s.robot * (s.robotNote - naturalMidi);
        const float speed = speedMs * 0.001f;
        const float a = speed <= 0.001f ? 1.0f : 1.0f - std::exp (-hopSeconds / speed);
        corr += (want - corr) * a;
    }
    else
    {
        currentNote = -1.0f;
        corr *= std::exp (-hopSeconds / 0.25f);   // let go slowly between words
    }

    lfoPhase = std::fmod (lfoPhase + twoPi * s.wobbleRate * hopSeconds, twoPi);
    const float wob = s.wobble * 1.5f * std::sin (lfoPhase);
    const float shiftSemis = juce::jlimit (-36.0f, 36.0f, s.pitchSemis + corr + wob);
    const float ratio = std::exp2 (shiftSemis / 12.0f);
    const float fr = std::exp2 (s.formantSemis / 12.0f);

    if (voiced) carrierMidi = naturalMidi;
    carrier.setTargetHz (440.0f * std::exp2 ((carrierMidi + corr + s.pitchSemis + wob + 12.0f * (float) s.talkOctave - 69.0f) / 12.0f));
    detectedMidi = voiced ? naturalMidi : -1.0f;
    targetMidi = voiced ? naturalMidi + shiftSemis : -1.0f;
    {
        const int w = historyWrite.load (std::memory_order_relaxed);
        historyIn[(size_t) w].store (voiced ? naturalMidi : -1.0f, std::memory_order_relaxed);
        historyOut[(size_t) w].store (voiced ? naturalMidi + shiftSemis : -1.0f, std::memory_order_relaxed);
        historyWrite.store ((w + 1) % historySize, std::memory_order_release);
    }

    // new word? (for the sample's retrigger mode)
    if (p.rms < 0.003f) ++quietFrames;
    else
    {
        if (p.rms > 0.01f && quietFrames >= 4 && s.carrier.sampleMode == 1) carrier.retrigger();
        quietFrames = 0;
    }

    // ---------------------------------------------------------------- analysis
    for (int i = 0; i < n; ++i) work[(size_t) i] = inBuf[(size_t) i] * window[(size_t) i];
    fwd (work.data());
    float maxMag = 0.0f, voiceEnergy = 0.0f;
    const float hopPhase = twoPi * (float) hop / (float) n;
    for (int k = 0; k <= half; ++k)
    {
        X[(size_t) k] = { work[(size_t) 2 * k], work[(size_t) 2 * k + 1] };
        mag[(size_t) k] = std::abs (X[(size_t) k]);
        maxMag = std::max (maxMag, mag[(size_t) k]);
        voiceEnergy += mag[(size_t) k] * mag[(size_t) k];
    }

    std::fill (Y.begin(), Y.end(), std::complex<float> {});
    const bool silent = maxMag < 1.0e-7f;
    if (! silent)
    {
        envelope (mag.data(), env.data(), 3);
        for (int k = 0; k <= half; ++k)
        {
            const float src = (float) k / fr;
            const int i0 = (int) src;
            if (i0 >= half) { envWarp[(size_t) k] = env[(size_t) half] * 0.01f; continue; }
            const float f = src - (float) i0;
            envWarp[(size_t) k] = env[(size_t) i0] * (1.0f - f) + env[(size_t) i0 + 1] * f;
        }
    }

    // ---------------------------------------------------------------- voice changer
    const float talkAmt = juce::jlimit (0.0f, 1.0f, s.talk);
    const float voiceGain = std::cos (talkAmt * juce::MathConstants<float>::halfPi);
    const float talkMix = std::sin (talkAmt * juce::MathConstants<float>::halfPi);

    if (! silent && voiceGain > 0.0001f)
    {
        peaks.clear();
        const float floor = maxMag * 1.0e-5f;
        for (int k = 2; k <= half - 2; ++k)
        {
            const float m = mag[(size_t) k];
            if (m > floor && m > mag[(size_t) k - 1] && m >= mag[(size_t) k + 1] && m > mag[(size_t) k - 2] && m >= mag[(size_t) k + 2])
                peaks.push_back (k);
        }

        const bool identity = std::abs (ratio - 1.0f) < 1.0e-4f && std::abs (fr - 1.0f) < 1.0e-4f;
        if (identity || peaks.empty())
        {
            for (int k = 0; k <= half; ++k) Y[(size_t) k] = X[(size_t) k];
        }
        else
        {
            int regionStart = 0;
            for (size_t pi = 0; pi < peaks.size(); ++pi)
            {
                const int pk = peaks[pi];
                int regionEnd = half + 1;
                if (pi + 1 < peaks.size())
                {
                    const int nxt = peaks[pi + 1];
                    regionEnd = pk + 1;
                    for (int k = pk + 1; k < nxt; ++k) if (mag[(size_t) k] < mag[(size_t) regionEnd]) regionEnd = k;
                }

                const float phase = std::arg (X[(size_t) pk]);
                const float dphi = wrapPi (phase - prevPhase[(size_t) pk] - hopPhase * (float) pk);
                const float trueBin = (float) pk + dphi / hopPhase;
                const float delta = trueBin * (ratio - 1.0f);
                const int shift = (int) std::lround (delta);
                const float r = wrapPi (rot[(size_t) pk] + hopPhase * delta);
                const std::complex<float> rotor (std::cos (r), std::sin (r));

                for (int k = regionStart; k < regionEnd; ++k)
                {
                    newRot[(size_t) k] = r;
                    const int dest = k + shift;
                    if (dest < 0 || dest > half) continue;
                    const float g = std::min (20.0f, envWarp[(size_t) dest] / std::max (env[(size_t) k], 1.0e-9f));
                    Y[(size_t) dest] += X[(size_t) k] * rotor * g;
                }
                regionStart = regionEnd;
            }
            std::swap (rot, newRot);

            // moving pitch and throat shouldn't change how loud it is
            float ey = 0.0f;
            for (int k = 0; k <= half; ++k) ey += std::norm (Y[(size_t) k]);
            const float target = ey > 1.0e-20f ? juce::jlimit (0.2f, 5.0f, std::sqrt (voiceEnergy / ey)) : 1.0f;
            voiceNorm += (target - voiceNorm) * 0.5f;
            for (int k = 0; k <= half; ++k) Y[(size_t) k] *= voiceNorm;
        }

        if (s.whisper > 0.001f)
        {
            float ey = 0.0f, ew = 0.0f;
            for (int k = 0; k <= half; ++k)
            {
                const float a = rng.nextFloat() * twoPi;
                T[(size_t) k] = std::polar (envWarp[(size_t) k], a);
                ey += std::norm (Y[(size_t) k]);
                ew += std::norm (T[(size_t) k]);
            }
            const float gW = ew > 1.0e-20f ? std::sqrt (ey / ew) : 0.0f;
            const float w = s.whisper;
            for (int k = 0; k <= half; ++k)
                Y[(size_t) k] = Y[(size_t) k] * (1.0f - w) + T[(size_t) k] * (gW * w);
        }
        for (int k = 0; k <= half; ++k) Y[(size_t) k] *= voiceGain;
    }
    for (int k = 0; k <= half; ++k) prevPhase[(size_t) k] = std::arg (X[(size_t) k]);

    // ---------------------------------------------------------------- talk engine
    if (! silent && talkMix > 0.0001f)
    {
        for (int i = 0; i < n; ++i) cwork[(size_t) i] = carBuf[(size_t) i] * window[(size_t) i];
        std::fill (cwork.begin() + n, cwork.end(), 0.0f);
        fft->performRealOnlyForwardTransform (cwork.data(), true);
        float cmax = 0.0f;
        for (int k = 0; k <= half; ++k)
        {
            cmag[(size_t) k] = std::hypot (cwork[(size_t) 2 * k], cwork[(size_t) 2 * k + 1]);
            cmax = std::max (cmax, cmag[(size_t) k]);
        }

        // the voice's (formant-shifted) envelope, optionally chunked into retro vocoder bands
        std::copy (envWarp.begin(), envWarp.end(), envTalk.begin());
        if (const int nb = bandCount (s.bands); nb > 0)
        {
            const float lo = 90.0f, hi = (float) std::min (12000.0, sr * 0.45);
            int startBin = 0;
            for (int b = 0; b < nb; ++b)
            {
                const float edgeHz = lo * std::pow (hi / lo, (float) (b + 1) / (float) nb);
                int endBin = b == nb - 1 ? half + 1 : std::min (half + 1, std::max (startBin + 1, (int) (edgeHz * (float) n / (float) sr)));
                float pw = 0.0f;
                for (int k = startBin; k < endBin; ++k) pw += envWarp[(size_t) k] * envWarp[(size_t) k];
                const float a = std::sqrt (pw / (float) std::max (1, endBin - startBin));
                for (int k = startBin; k < endBin; ++k) envTalk[(size_t) k] = a;
                startBin = endBin;
            }
        }

        float et = 0.0f;
        if (cmax > 1.0e-7f)
        {
            envelope (cmag.data(), cenv.data(), 2);
            const float ch = s.character;
            for (int k = 0; k <= half; ++k)
            {
                const std::complex<float> c (cwork[(size_t) 2 * k], cwork[(size_t) 2 * k + 1]);
                const float flat = ch > 0.999f ? 1.0f / std::max (cenv[(size_t) k], 1.0e-9f)
                                               : std::pow (std::max (cenv[(size_t) k], 1.0e-9f), -ch);
                T[(size_t) k] = c * (flat * envTalk[(size_t) k]);
                et += std::norm (T[(size_t) k]);
            }
        }
        else
        {
            std::fill (T.begin(), T.end(), std::complex<float> {});
        }

        // match the voice's loudness frame by frame, so the talk follows the words
        const float target = et > 1.0e-20f ? std::sqrt (voiceEnergy / et) : 0.0f;
        talkGain = target < talkGain ? target : talkGain + (target - talkGain) * 0.7f;
        const float g = std::min (talkGain, 1.0e4f) * talkMix;

        // breath and "s" sounds: shaped noise, louder when the voice isn't pitched
        const float unvoiced = voiced ? juce::jlimit (0.0f, 1.0f, (p.aperiodicity - 0.05f) * 4.0f) : 1.0f;
        const float sib = s.sibilance * (0.2f + 0.8f * unvoiced) * 0.9f * talkMix;
        const float binHz = (float) sr / (float) n;
        for (int k = 0; k <= half; ++k)
        {
            std::complex<float> v = T[(size_t) k] * g;
            if (sib > 0.0001f)
            {
                const float hz = (float) k * binHz;
                const float hf = juce::jlimit (0.0f, 1.0f, (hz - 1500.0f) / 2500.0f);
                if (hf > 0.0f) v += std::polar (envWarp[(size_t) k] * hf * sib, rng.nextFloat() * twoPi);
            }
            Y[(size_t) k] += v;
        }
    }

    // ---------------------------------------------------------------- synthesis
    for (int k = 0; k <= half; ++k)
    {
        work[(size_t) 2 * k] = Y[(size_t) k].real();
        work[(size_t) 2 * k + 1] = Y[(size_t) k].imag();
    }
    work[1] = 0.0f;
    work[(size_t) 2 * half + 1] = 0.0f;
    for (int i = 2 * half + 2; i < 2 * n; ++i) work[(size_t) i] = 0.0f;
    inv (work.data());

    for (int i = 0; i < n; ++i)
    {
        float v = work[(size_t) i] * window[(size_t) i] * olaNorm;
        if (! std::isfinite (v)) v = 0.0f;
        outAccum[(size_t) i] += v;
    }
    std::copy (outAccum.begin(), outAccum.begin() + hop, outHop.begin());
    std::memmove (outAccum.data(), outAccum.data() + hop, sizeof (float) * (size_t) (n - hop));
    std::fill (outAccum.begin() + (n - hop), outAccum.end(), 0.0f);
}
}
