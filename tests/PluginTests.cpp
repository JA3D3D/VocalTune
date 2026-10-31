// VocalTune tests + offline renderer.
//   VocalTuneTests                      run the tests
//   VocalTuneTests --render <dir>       also write every preset as a WAV, using a synthetic singer
//   VocalTuneTests --screenshot <png>   also save a picture of the plugin window
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <iostream>

using namespace vt;

static int fails = 0;
static void ok (bool c, const juce::String& m)
{
    if (! c) ++fails;
    std::cout << (c ? "ok   " : "FAIL ") << m << std::endl;
}

// ---------------------------------------------------------------- a synthetic singer
// glottal buzz -> three formant resonators, syllables with moving vowels, "s" noise and gaps
struct Reso
{
    double y1 = 0, y2 = 0;
    float run (float x, double f, double bw, double sr)
    {
        const double r = std::exp (-juce::MathConstants<double>::pi * bw / sr);
        const double a1 = -2 * r * std::cos (juce::MathConstants<double>::twoPi * f / sr), a2 = r * r;
        const double y = (1 - r) * x - a1 * y1 - a2 * y2;
        y2 = y1; y1 = y;
        return (float) y;
    }
};

static juce::AudioBuffer<float> makeVoice (double sr, std::vector<float>* f0Track = nullptr, float centsOff = 0.0f)
{
    struct Syl { float midi, dur; int vowel; bool s; };
    const std::vector<Syl> song {
        { 57, 0.35f, 0, false }, { 60, 0.30f, 1, true }, { 62, 0.45f, 2, false }, { -1, 0.18f, 0, false },
        { 64, 0.30f, 3, true }, { 62, 0.30f, 4, false }, { 60, 0.55f, 0, false }, { -1, 0.2f, 0, false },
        { 55, 0.35f, 1, false }, { 57, 0.35f, 2, true }, { 60, 0.6f, 3, false }, { -1, 0.25f, 0, false },
        { 64, 0.3f, 0, true }, { 67, 0.4f, 1, false }, { 64, 0.3f, 4, false }, { 62, 0.7f, 0, false },
    };
    const float formants[5][3] { { 730, 1090, 2440 }, { 530, 1840, 2480 }, { 270, 2290, 3010 }, { 570, 840, 2410 }, { 300, 870, 2240 } };
    double total = 0; for (auto& s : song) total += s.dur;
    juce::AudioBuffer<float> out (1, (int) (total * sr) + 1);
    out.clear();
    if (f0Track) f0Track->assign ((size_t) out.getNumSamples(), 0.0f);
    Reso r[3]; Reso hiss;
    juce::Random rnd (42);
    double phase = 0; int pos = 0;
    float lastMidi = 57;
    for (size_t si = 0; si < song.size(); ++si)
    {
        const auto& s = song[si];
        const int len = (int) (s.dur * sr);
        const auto& nf = formants[(s.vowel + 1) % 5];
        for (int i = 0; i < len && pos < out.getNumSamples(); ++i, ++pos)
        {
            const float t = (float) i / (float) len;
            float v = 0.0f;
            if (s.midi > 0)
            {
                const float glide = std::min (1.0f, (float) i / (float) (0.06 * sr));
                const float midi = lastMidi + (s.midi - lastMidi) * glide + 0.25f * std::sin ((float) pos / (float) sr * 2 * 3.14159f * 5.5f) * t + centsOff * 0.01f;
                const double f0 = 440.0 * std::exp2 ((midi - 69.0) / 12.0);
                if (f0Track) (*f0Track)[(size_t) pos] = (float) f0;
                phase += f0 / sr; if (phase >= 1) phase -= 1;
                const float buzz = (float) (phase < 0.6 ? std::sin (phase / 0.6 * 3.14159) : 0.0) - 0.38f;   // rough glottal pulse
                const float env = std::min (1.0f, t * 12.0f) * std::min (1.0f, (1.0f - t) * 8.0f);
                const float ft = std::min (1.0f, t * 1.5f);
                float sig = buzz * env;
                float sum = 0;
                for (int k = 0; k < 3; ++k)
                {
                    const float f = formants[s.vowel][k] + (nf[k] - formants[s.vowel][k]) * ft * 0.5f;
                    sum += r[k].run (sig, f, 70 + 30 * k, sr) * (k == 0 ? 1.0f : k == 1 ? 0.7f : 0.35f);
                }
                v = sum * 2.2f;
                if (s.s && t < 0.18f) v += hiss.run ((rnd.nextFloat() * 2 - 1), 6500, 3000, sr) * 0.9f * (1.0f - t / 0.18f);
            }
            out.setSample (0, pos, v);
        }
        if (s.midi > 0) lastMidi = s.midi;
    }
    out.applyGain (0.5f / std::max (1.0e-6f, out.getMagnitude (0, 0, out.getNumSamples())));
    return out;
}

static juce::AudioBuffer<float> makePluck (double sr, float hz)
{
    juce::AudioBuffer<float> b (1, (int) (sr * 1.5));
    juce::Random rnd (3);
    const int period = (int) (sr / hz);
    std::vector<float> line ((size_t) period);
    for (auto& v : line) v = rnd.nextFloat() * 2 - 1;
    int idx = 0;
    for (int i = 0; i < b.getNumSamples(); ++i)
    {
        const float a = line[(size_t) idx], c = line[(size_t) ((idx + 1) % period)];
        line[(size_t) idx] = 0.4985f * (a + c);
        b.setSample (0, i, a);
        idx = (idx + 1) % period;
    }
    return b;
}

// ---------------------------------------------------------------- helpers
static float rms (const float* x, int n) { double s = 0; for (int i = 0; i < n; ++i) s += (double) x[i] * x[i]; return (float) std::sqrt (s / std::max (1, n)); }

// median f0 of a mono signal over voiced frames
static float medianPitch (const float* x, int n, double sr)
{
    PitchDetector d; d.prepare (sr, 11); d.setRange (50, 1500);
    std::vector<float> f;
    for (int p = 0; p + 2048 <= n; p += 1024) { auto r = d.detect (x + p); if (r.hz > 0 && r.aperiodicity < 0.25f) f.push_back (r.hz); }
    if (f.empty()) return 0;
    std::nth_element (f.begin(), f.begin() + (long) f.size() / 2, f.end());
    return f[f.size() / 2];
}

// median of output pitch / input pitch, frame by frame
static float medianRatio (const float* x, const float* y, int n, double sr)
{
    PitchDetector a, b; a.prepare (sr, 11); b.prepare (sr, 11); a.setRange (50, 1500); b.setRange (50, 1500);
    std::vector<float> r;
    for (int p = 0; p + 2048 <= n; p += 1024)
    {
        auto ra = a.detect (x + p), rb = b.detect (y + p);
        if (ra.hz > 0 && rb.hz > 0 && ra.aperiodicity < 0.25f && rb.aperiodicity < 0.25f) r.push_back (rb.hz / ra.hz);
    }
    if (r.empty()) return 0;
    std::nth_element (r.begin(), r.begin() + (long) r.size() / 2, r.end());
    return r[r.size() / 2];
}

static void setP (VocalTuneProcessor& p, const char* pid, float v)
{
    auto* prm = p.state.getParameter (pid);
    prm->setValueNotifyingHost (prm->convertTo0to1 (v));
}

static void resetAll (VocalTuneProcessor& p)
{
    for (auto* prm : p.getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (prm)) r->setValueNotifyingHost (r->getDefaultValue());
}

// run mono input through the whole plugin (stereo in/out), return left output
static juce::AudioBuffer<float> runPlugin (VocalTuneProcessor& p, const juce::AudioBuffer<float>& in, double sr, int block = 480,
                                           const juce::AudioBuffer<float>* side = nullptr)
{
    const int total = in.getNumSamples() + p.getLatencySamples() + (int) (sr * 0.5);
    juce::AudioBuffer<float> out (2, total);
    const int chans = p.getTotalNumInputChannels();
    juce::AudioBuffer<float> buf (std::max (chans, 2), block);
    juce::MidiBuffer midi;
    for (int pos = 0; pos < total; pos += block)
    {
        const int n = std::min (block, total - pos);
        buf.setSize (buf.getNumChannels(), n, false, false, true);
        buf.clear();
        for (int c = 0; c < 2; ++c)
            if (pos < in.getNumSamples()) buf.copyFrom (c, 0, in, 0, pos, std::min (n, in.getNumSamples() - pos));
        if (side != nullptr && chans >= 4)
            for (int c = 2; c < 4; ++c)
                if (pos < side->getNumSamples()) buf.copyFrom (c, 0, *side, 0, pos, std::min (n, side->getNumSamples() - pos));
        p.processBlock (buf, midi);
        out.copyFrom (0, pos, buf, 0, 0, n);
        out.copyFrom (1, pos, buf, 1, 0, n);
    }
    return out;
}

static juce::AudioBuffer<float> aligned (const juce::AudioBuffer<float>& out, int latency, int len)
{
    juce::AudioBuffer<float> a (1, len);
    a.copyFrom (0, 0, out, 0, latency, std::min (len, out.getNumSamples() - latency));
    return a;
}

static void writeWav (const juce::File& f, const juce::AudioBuffer<float>& b, double sr)
{
    f.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> os = std::make_unique<juce::FileOutputStream> (f);
    auto w = wav.createWriterFor (os, juce::AudioFormatWriterOptions{}.withSampleRate (sr).withNumChannels (b.getNumChannels()).withBitsPerSample (16));
    if (w) w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    juce::File renderDir, shotFile, toonShot;
    for (int i = 1; i + 1 < argc; ++i)
    {
        if (juce::String (argv[i]) == "--render") renderDir = juce::File::getCurrentWorkingDirectory().getChildFile (argv[i + 1]);
        if (juce::String (argv[i]) == "--screenshot") shotFile = juce::File::getCurrentWorkingDirectory().getChildFile (argv[i + 1]);
        if (juce::String (argv[i]) == "--screenshot-toon") toonShot = juce::File::getCurrentWorkingDirectory().getChildFile (argv[i + 1]);
    }

    const double sr = 48000.0;
    std::vector<float> truth;
    const auto voice = makeVoice (sr, &truth);
    const float voiceRms = rms (voice.getReadPointer (0), voice.getNumSamples());
    const float voicePitch = medianPitch (voice.getReadPointer (0), voice.getNumSamples(), sr);
    ok (voicePitch > 180 && voicePitch < 340, "test singer is detected at a sensible pitch (" + juce::String (voicePitch, 1) + " Hz)");

    // ---------------------------------------------------------------- scale snapping
    ok (SpectralEngine::snapToScale (61.2f, 0, scales()[1].mask) == 62.0f, "C#+ snaps to D in C major");
    ok (SpectralEngine::snapToScale (60.4f, 0, scales()[1].mask) == 60.0f, "slightly sharp C stays C");
    ok (SpectralEngine::snapToScale (70.0f, 9, scales()[2].mask) == 69.0f || SpectralEngine::snapToScale (70.0f, 9, scales()[2].mask) == 71.0f, "A# snaps into A minor");
    ok (SpectralEngine::snapToScale (66.6f, 0, scales()[12].mask) == 60.0f || SpectralEngine::snapToScale (66.6f, 0, scales()[12].mask) == 72.0f, "root-only scale snaps to C");

    VocalTuneProcessor proc;
    proc.prepareToPlay (sr, 480);
    const int lat = proc.getLatencySamples();
    ok (lat == 2048, "latency is reported (" + juce::String (lat) + " samples)");

    // ---------------------------------------------------------------- clean pass-through
    {
        resetAll (proc); proc.prepareToPlay (sr, 480);
        auto out = runPlugin (proc, voice, sr);
        auto a = aligned (out, lat, voice.getNumSamples());
        double err = 0, sig = 0;
        for (int i = 4096; i < voice.getNumSamples() - 4096; ++i)
        {
            const double d = a.getSample (0, i) - voice.getSample (0, i);
            err += d * d; sig += (double) voice.getSample (0, i) * voice.getSample (0, i);
        }
        const double snr = 10 * std::log10 (sig / std::max (1e-20, err));
        ok (snr > 35, "with nothing turned on the voice comes out unchanged (SNR " + juce::String (snr, 1) + " dB)");
    }

    auto pitchTest = [&] (const juce::String& what, std::function<void()> setup, float expectRatio, float tolCents)
    {
        resetAll (proc); setup(); proc.prepareToPlay (sr, 480);
        auto out = runPlugin (proc, voice, sr);
        auto a = aligned (out, lat, voice.getNumSamples());
        const float ratio = medianRatio (voice.getReadPointer (0), a.getReadPointer (0), a.getNumSamples(), sr);
        const float f = voicePitch * ratio;
        const float cents = ratio > 0 ? 1200.0f * std::log2 (ratio / expectRatio) : 9999.0f;
        const float level = juce::Decibels::gainToDecibels (rms (a.getReadPointer (0), a.getNumSamples()) / voiceRms);
        if (renderDir != juce::File()) { renderDir.createDirectory(); writeWav (renderDir.getChildFile ("test " + what.replaceCharacters ("+/()", "p___") + ".wav"), a, sr); }
        ok (std::abs (cents) < tolCents, what + ": pitch " + juce::String (f, 1) + " Hz (" + juce::String (cents, 0) + " cents off), level " + juce::String (level, 1) + " dB");
        return a;
    };
    pitchTest ("pitch +12", [&] { setP (proc, id::pitch, 12); }, 2.0f, 40);
    pitchTest ("pitch -7", [&] { setP (proc, id::pitch, -7); }, std::exp2 (-7.0f / 12.0f), 40);
    pitchTest ("pitch +5 with throat +8 (baby)", [&] { setP (proc, id::pitch, 5); setP (proc, id::formant, 8); }, std::exp2 (5.0f / 12.0f), 40);
    pitchTest ("throat -6 alone keeps the pitch", [&] { setP (proc, id::formant, -6); }, 1.0f, 30);

    // formant shift should move spectral weight
    {
        auto centroid = [&] (const juce::AudioBuffer<float>& b)
        {
            juce::dsp::FFT f (12); std::vector<float> w (8192);
            double num = 0, den = 0;
            for (int p = 4096; p + 4096 < b.getNumSamples(); p += 2048)
            {
                std::fill (w.begin(), w.end(), 0.0f);
                for (int i = 0; i < 4096; ++i) w[(size_t) i] = b.getSample (0, p + i) * (0.5f - 0.5f * std::cos (6.2831853f * i / 4096));
                f.performFrequencyOnlyForwardTransform (w.data(), true);
                for (int k = 1; k < 2048; ++k) { num += k * (double) w[(size_t) k]; den += w[(size_t) k]; }
            }
            return (float) (num / std::max (1e-9, den) * sr / 4096);
        };
        resetAll (proc); setP (proc, id::formant, 6); proc.prepareToPlay (sr, 480);
        auto up = aligned (runPlugin (proc, voice, sr), lat, voice.getNumSamples());
        resetAll (proc); setP (proc, id::formant, -6); proc.prepareToPlay (sr, 480);
        auto down = aligned (runPlugin (proc, voice, sr), lat, voice.getNumSamples());
        const float c0 = centroid (voice), cu = centroid (up), cd = centroid (down);
        ok (cu > c0 * 1.15f && cd < c0 * 0.87f, "throat moves the tone: centroid " + juce::String (cd, 0) + " < " + juce::String (c0, 0) + " < " + juce::String (cu, 0) + " Hz");
    }

    // ---------------------------------------------------------------- tune + robot
    {
        // singer 40 cents flat of the notes; hard tune should pull it back
        std::vector<float> t2;
        const auto flat = makeVoice (sr, &t2, -40.0f);
        resetAll (proc); setP (proc, id::tune, 1); setP (proc, id::tuneSpeed, 0); setP (proc, id::scale, 0); proc.prepareToPlay (sr, 480);
        auto a = aligned (runPlugin (proc, flat, sr), lat, flat.getNumSamples());
        const float fin = medianPitch (flat.getReadPointer (0), flat.getNumSamples(), sr);
        const float fout = medianPitch (a.getReadPointer (0), a.getNumSamples(), sr);
        auto centsFromNote = [] (float hz) { const float m = 69 + 12 * std::log2 (hz / 440); return 100 * (m - std::round (m)); };
        ok (std::abs (centsFromNote (fout)) < 12 && std::abs (centsFromNote (fin)) > 25,
            "hard tune pulls a flat singer onto the notes (" + juce::String (centsFromNote (fin), 0) + " -> " + juce::String (centsFromNote (fout), 0) + " cents)");
    }
    {
        resetAll (proc); setP (proc, id::robot, 1); setP (proc, id::robotNote, 48); setP (proc, id::tuneSpeed, 5); proc.prepareToPlay (sr, 480);
        auto a = aligned (runPlugin (proc, voice, sr), lat, voice.getNumSamples());
        // a monotone: nearly every voiced frame should sit at C3
        PitchDetector d; d.prepare (sr, 11);
        int voicedFrames = 0, onNote = 0;
        for (int p = 0; p + 2048 <= a.getNumSamples(); p += 1024)
        {
            auto r = d.detect (a.getReadPointer (0) + p);
            if (r.hz > 0 && r.aperiodicity < 0.2f) { ++voicedFrames; if (std::abs (1200 * std::log2 (r.hz / 130.81f)) < 50) ++onNote; }
        }
        ok (voicedFrames > 20 && onNote > voicedFrames * 0.75, "robot holds one note (" + juce::String (onNote) + "/" + juce::String (voicedFrames) + " frames on C3)");
    }

    // ---------------------------------------------------------------- Auto Toon
    {
        std::vector<float> t2;
        const auto flat = makeVoice (sr, &t2, -40.0f);
        // mean distance from the nearest note, over pitched frames
        auto meanCents = [&] (const juce::AudioBuffer<float>& a)
        {
            PitchDetector d; d.prepare (sr, 11);
            double sum = 0; int n = 0;
            for (int p = 0; p + 2048 <= a.getNumSamples(); p += 512)
            {
                auto r = d.detect (a.getReadPointer (0) + p);
                if (r.hz > 0 && r.aperiodicity < 0.2f) { const float m = 69 + 12 * std::log2 (r.hz / 440); sum += std::abs (100 * (m - std::round (m))); ++n; }
            }
            return n > 0 ? (float) (sum / n) : 999.0f;
        };
        auto run = [&] (std::function<void()> setup)
        {
            resetAll (proc); setP (proc, id::tune, 1); setP (proc, id::scale, 0); setP (proc, id::tuneSpeed, 0); setup();
            proc.prepareToPlay (sr, 480);
            return aligned (runPlugin (proc, flat, sr), lat, flat.getNumSamples());
        };
        const float straight = meanCents (run ([&] { setP (proc, id::vibratoKeep, 0); }));
        const float natural = meanCents (run ([&] { setP (proc, id::vibratoKeep, 1); }));
        ok (straight < 6.0f && natural > straight * 1.5f, "VIBRATO: 0% gives dead-straight notes, 100% keeps the singer's vibrato (" + juce::String (straight, 1) + " vs " + juce::String (natural, 1) + " cents)");
        const float flexed = meanCents (run ([&] { setP (proc, id::vibratoKeep, 0); setP (proc, id::flex, 1); }));
        ok (flexed > 25.0f, "FLEX: notes the singer is clearly away from are left alone (" + juce::String (flexed, 1) + " cents)");
        const float human = meanCents (run ([&] { setP (proc, id::vibratoKeep, 0); setP (proc, id::humanize, 1); }));
        ok (human > straight + 2.0f && human < 35.0f, "HUMANIZE: held notes are corrected more gently (" + juce::String (human, 1) + " cents)");

        // piano: only A allowed -> everything lands on an A
        resetAll (proc);
        for (int pc : { 0, 2, 4, 5, 7, 11 }) proc.toggleNote (pc);   // C major minus everything but A
        ok (proc.absoluteNoteMask() == (1 << 9) && juce::roundToInt (proc.state.getRawParameterValue (id::scale)->load()) == (int) scales().size() - 1,
            "clicking piano keys makes a Custom scale (mask " + juce::String::toHexString (proc.absoluteNoteMask()) + ")");
        proc.toggleNote (9);
        ok (proc.absoluteNoteMask() == (1 << 9), "the last note can't be switched off");
        setP (proc, id::tune, 1); setP (proc, id::tuneSpeed, 0); setP (proc, id::vibratoKeep, 0);
        proc.prepareToPlay (sr, 480);
        auto a = aligned (runPlugin (proc, voice, sr), lat, voice.getNumSamples());
        PitchDetector d; d.prepare (sr, 11);
        int voicedFrames = 0, onA = 0;
        for (int p = 0; p + 2048 <= a.getNumSamples(); p += 1024)
        {
            auto r = d.detect (a.getReadPointer (0) + p);
            if (r.hz > 0 && r.aperiodicity < 0.2f) { ++voicedFrames; const int m = juce::roundToInt (69 + 12 * std::log2 (r.hz / 440)); if (((m % 12) + 12) % 12 == 9) ++onA; }
        }
        ok (voicedFrames > 20 && onA > voicedFrames * 0.85, "custom notes are obeyed (" + juce::String (onA) + "/" + juce::String (voicedFrames) + " frames on an A)");
        int filled = 0;
        for (auto& h : proc.engine.historyOut) if (h.load() > 0) ++filled;
        ok (filled > 200, "the pitch graph gets its history (" + juce::String (filled) + " points)");
    }

    // ---------------------------------------------------------------- talk box
    auto envelopeCorr = [&] (const juce::AudioBuffer<float>& a)
    {
        std::vector<float> e1, e2;
        for (int p = 0; p + 960 <= voice.getNumSamples(); p += 960)
        {
            e1.push_back (rms (voice.getReadPointer (0) + p, 960));
            e2.push_back (rms (a.getReadPointer (0) + p, 960));
        }
        double m1 = 0, m2 = 0; for (size_t i = 0; i < e1.size(); ++i) { m1 += e1[i]; m2 += e2[i]; }
        m1 /= (double) e1.size(); m2 /= (double) e2.size();
        double c = 0, v1 = 0, v2 = 0;
        for (size_t i = 0; i < e1.size(); ++i) { c += (e1[i] - m1) * (e2[i] - m2); v1 += (e1[i] - m1) * (e1[i] - m1); v2 += (e2[i] - m2) * (e2[i] - m2); }
        return (float) (c / std::sqrt (std::max (1e-20, v1 * v2)));
    };
    for (int car : { (int) carSaw, (int) carSuper, (int) carSquare, (int) carNoise })
    {
        resetAll (proc); setP (proc, id::talk, 1); setP (proc, id::carrier, (float) car); proc.prepareToPlay (sr, 480);
        auto a = aligned (runPlugin (proc, voice, sr), lat, voice.getNumSamples());
        const float corr = envelopeCorr (a);
        const float level = juce::Decibels::gainToDecibels (rms (a.getReadPointer (0), a.getNumSamples()) / voiceRms);
        ok (corr > 0.8f && std::abs (level) < 6, "talk box with " + carrierNames()[car] + " follows the words (corr " + juce::String (corr, 2) + ", level " + juce::String (level, 1) + " dB)");
        if (car == carSaw)
        {
            const float f = medianPitch (a.getReadPointer (0), a.getNumSamples(), sr);
            ok (std::abs (1200 * std::log2 (f / voicePitch)) < 60, "talk synth sings the singer's notes (" + juce::String (f, 1) + " Hz)");
        }
    }
    {
        // sample: a plucked string at 330 Hz
        const auto pluck = makePluck (sr, 330.0f);
        ok (std::abs (VocalTuneProcessor::detectRoot (pluck, sr) - 330.0f) < 10.0f, "sample root detected (" + juce::String (VocalTuneProcessor::detectRoot (pluck, sr), 1) + " Hz)");
        resetAll (proc);
        proc.loadSampleBuffer (pluck, sr, "Pluck");
        setP (proc, id::talk, 1); setP (proc, id::carrier, (float) carSample); proc.prepareToPlay (sr, 480);
        auto a = aligned (runPlugin (proc, voice, sr), lat, voice.getNumSamples());
        const float corr = envelopeCorr (a);
        const float f = medianPitch (a.getReadPointer (0), a.getNumSamples(), sr);
        ok (corr > 0.75f, "a loaded sample talks (corr " + juce::String (corr, 2) + ")");
        ok (std::abs (1200 * std::log2 (f / voicePitch)) < 80, "follow: the sample sings the singer's notes (" + juce::String (f, 1) + " Hz)");
    }
    {
        // sidechain carrier
        VocalTuneProcessor sc;
        auto layout = sc.getBusesLayout();
        layout.inputBuses.getReference (1) = juce::AudioChannelSet::stereo();
        ok (sc.setBusesLayout (layout), "sidechain input can be switched on");
        sc.prepareToPlay (sr, 480);
        setP (sc, id::talk, 1); setP (sc, id::carrier, (float) carSidechain);
        juce::AudioBuffer<float> side (1, voice.getNumSamples());
        for (int i = 0; i < side.getNumSamples(); ++i) side.setSample (0, i, 0.3f * std::sin (6.2831853f * 98.0f * (float) i / (float) sr) + 0.2f * std::sin (6.2831853f * 147.0f * (float) i / (float) sr));
        auto out = runPlugin (sc, voice, sr, 480, &side);
        auto a = aligned (out, sc.getLatencySamples(), voice.getNumSamples());
        ok (envelopeCorr (a) > 0.75f, "the sidechain sound talks (corr " + juce::String (envelopeCorr (a), 2) + ")");
    }

    // ---------------------------------------------------------------- every preset
    const auto pluck = makePluck (sr, 220.0f);
    {
        juce::StringArray loud, quiet, bad;
        for (int i = 0; i < (int) factoryPresets().size(); ++i)
        {
            VocalTuneProcessor p;
            p.prepareToPlay (sr, 512);
            p.loadSampleBuffer (pluck, sr, "Pluck");
            const auto list = p.presetList();
            p.applyPreset (list[i]);
            auto out = runPlugin (p, voice, sr, 512);
            bool finite = true; float peak = 0;
            for (int c = 0; c < 2; ++c) for (int s = 0; s < out.getNumSamples(); ++s) { const float v = out.getSample (c, s); finite &= std::isfinite (v); peak = std::max (peak, std::abs (v)); }
            const float lvl = juce::Decibels::gainToDecibels (rms (out.getReadPointer (0), out.getNumSamples()) / voiceRms * std::sqrt ((float) out.getNumSamples() / (float) voice.getNumSamples()));
            const auto& fp = factoryPresets()[(size_t) i];
            const bool sidechain = fp.name == "Sidechain Talk";
            if (! finite) bad.add (fp.name);
            if (peak > 1.5f || lvl > 6) loud.add (fp.name + " (" + juce::String (lvl, 1) + " dB, peak " + juce::String (peak, 2) + ")");
            if (! sidechain && lvl < -12) quiet.add (fp.name + " (" + juce::String (lvl, 1) + " dB)");
            if (renderDir != juce::File())
            {
                renderDir.createDirectory();
                writeWav (renderDir.getChildFile (juce::String (i).paddedLeft ('0', 2) + " " + fp.category + " - " + fp.name + ".wav"),
                          [&] { juce::AudioBuffer<float> b (2, voice.getNumSamples() + (int) sr / 2);
                                for (int c = 0; c < 2; ++c) b.copyFrom (c, 0, out, c, p.getLatencySamples(), b.getNumSamples()); return b; }(), sr);
            }
        }
        ok (bad.isEmpty(), "all " + juce::String ((int) factoryPresets().size()) + " presets stay finite " + bad.joinIntoString (", "));
        ok (loud.isEmpty(), "no preset is too loud " + loud.joinIntoString (", "));
        ok (quiet.isEmpty(), "no preset is too quiet " + quiet.joinIntoString (", "));
        if (renderDir != juce::File())
        {
            writeWav (renderDir.getChildFile ("00 input - synthetic singer.wav"), voice, sr);
            writeWav (renderDir.getChildFile ("00 sample - pluck.wav"), pluck, sr);
        }
    }

    // ---------------------------------------------------------------- dice / mutate
    {
        VocalTuneProcessor p; p.prepareToPlay (sr, 512);
        bool allFinite = true, inRange = true;
        for (int round = 0; round < 25; ++round)
        {
            if (round % 2) p.dice(); else p.mutateCurrent();
            for (auto* prm : p.getParameters()) inRange &= prm->getValue() >= 0.0f && prm->getValue() <= 1.0f;
            juce::AudioBuffer<float> chunk (2, 24000);
            for (int c = 0; c < 2; ++c) chunk.copyFrom (c, 0, voice, 0, (round * 7919) % (voice.getNumSamples() - 24000), 24000);
            juce::MidiBuffer m;
            for (int pos = 0; pos < 24000; pos += 480)
            {
                juce::AudioBuffer<float> view (chunk.getArrayOfWritePointers(), 2, pos, 480);
                p.processBlock (view, m);
            }
            for (int s = 0; s < 24000; ++s) allFinite &= std::isfinite (chunk.getSample (0, s)) && std::abs (chunk.getSample (0, s)) < 8.0f;
        }
        ok (inRange && allFinite, "25 rounds of DICE / MUTATE stay in range and sound sane");
        ok (p.getPresetName().isNotEmpty(), "dice names the new sound (" + p.getPresetName() + ")");
    }

    // ---------------------------------------------------------------- state
    {
        VocalTuneProcessor a; a.prepareToPlay (sr, 512);
        a.loadSampleBuffer (pluck, sr, "My Pluck");
        setP (a, id::pitch, -7.5f); setP (a, id::carrier, (float) carSample); setP (a, id::chord, 4); setP (a, id::echo, 0.33f);
        juce::MemoryBlock mb; a.getStateInformation (mb);
        VocalTuneProcessor b; b.prepareToPlay (sr, 512);
        b.setStateInformation (mb.getData(), (int) mb.getSize());
        auto sb = b.getSample();
        ok (std::abs (b.state.getRawParameterValue (id::pitch)->load() + 7.5f) < 0.02f
            && juce::roundToInt (b.state.getRawParameterValue (id::chord)->load()) == 4
            && std::abs (b.state.getRawParameterValue (id::echo)->load() - 0.33f) < 0.01f, "settings come back with the Live set");
        ok (sb != nullptr && sb->name == "My Pluck" && std::abs (sb->length() - pluck.getNumSamples()) < 2, "the loaded sample is saved inside the Live set (" + juce::String ((int) mb.getSize() / 1024) + " KB)");
    }
    {
        VocalTuneProcessor a; a.prepareToPlay (sr, 512);
        setP (a, id::pitch, 3.0f); setP (a, id::metal, 0.6f);
        const juce::String name = "VocalTune Test " + juce::String (juce::Random::getSystemRandom().nextInt (100000));
        ok (a.saveUserPreset (name), "a user preset can be saved");
        const auto file = userPresetFolder().getChildFile (name + ".vtpreset");
        ok (file.existsAsFile(), "it lands in " + userPresetFolder().getFullPathName());
        resetAll (a);
        VocalTuneProcessor::PresetEntry found;
        for (auto& e : a.presetList()) if (e.name == name) found = e;
        ok (found.category == "User", "and shows up under User");
        a.applyPreset (found);
        ok (std::abs (a.state.getRawParameterValue (id::metal)->load() - 0.6f) < 0.01f, "and loads back");
        a.stepPreset (1); a.stepPreset (-1);
        ok (a.getPresetName() == name, "arrows step away and back (" + a.getPresetName() + ")");
        file.deleteFile();
    }

    // ---------------------------------------------------------------- other sample rates, mono
    for (double rate : { 44100.0, 96000.0 })
    {
        VocalTuneProcessor p; p.prepareToPlay (rate, 256);
        setP (p, id::pitch, 7);
        const auto v = makeVoice (rate);
        auto a = aligned (runPlugin (p, v, rate, 256), p.getLatencySamples(), v.getNumSamples());
        const float f = medianPitch (a.getReadPointer (0), a.getNumSamples(), rate);
        const float f0 = medianPitch (v.getReadPointer (0), v.getNumSamples(), rate);
        ok (std::abs (1200 * std::log2 (f / (f0 * std::exp2 (7.0f / 12)))) < 40, "works at " + juce::String (rate / 1000, 1) + " kHz (latency " + juce::String (p.getLatencySamples()) + ")");
    }
    {
        VocalTuneProcessor p;
        auto layout = p.getBusesLayout();
        layout.inputBuses.getReference (0) = juce::AudioChannelSet::mono();
        layout.outputBuses.getReference (0) = juce::AudioChannelSet::mono();
        ok (p.setBusesLayout (layout), "mono tracks are supported");
        p.prepareToPlay (sr, 512);
        juce::AudioBuffer<float> b (1, 512); b.copyFrom (0, 0, voice, 0, 10000, 512);
        juce::MidiBuffer m; p.processBlock (b, m);
        ok (std::isfinite (b.getSample (0, 100)), "mono processing runs");
    }

    // ---------------------------------------------------------------- the window
    {
        VocalTuneProcessor p; p.prepareToPlay (sr, 512);
        p.loadSampleBuffer (makePluck (sr, 196.0f), sr, "Dog Bark");
        p.applyPreset (p.presetList()[21]);
        std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
        ed->setVisible (true);   // hosts do this when they open the window
        ok (ed != nullptr && ed->getWidth() == VocalTuneEditor::baseW, "the window opens at " + juce::String (ed->getWidth()) + " x " + juce::String (ed->getHeight()));
        // feed a little audio so the tuner shows notes
        runPlugin (p, voice, sr, 512);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (300);
        ed->setSize ((int) (VocalTuneEditor::baseW * 1.25), (int) (VocalTuneEditor::baseH * 1.25));
        ok (std::abs (p.editorScale - 1.25f) < 0.01f, "the window resizes and remembers its size");
        ed->setSize (VocalTuneEditor::baseW, VocalTuneEditor::baseH);
        if (shotFile != juce::File())
        {
            auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 2.0f);
            shotFile.deleteFile();
            juce::FileOutputStream os (shotFile);
            juce::PNGImageFormat().writeImageToStream (img, os);
            std::cout << "screenshot: " << shotFile.getFullPathName() << std::endl;
        }
        // flip to Auto Toon while a singer is going, so the graph and the singer have something to show
        auto* vte = dynamic_cast<VocalTuneEditor*> (ed.get());
        for (auto& e : p.presetList()) if (e.name == "Pop Polish") p.applyPreset (e);
        vte->showPage (1);
        ok (p.page == 1, "the AUTO TOON button flips the window over");
        {
            juce::AudioBuffer<float> part (1, (int) (sr * 2.6));
            part.copyFrom (0, 0, voice, 0, 0, part.getNumSamples());
            // feed it in real-time-ish slices so the graph scrolls like it would in Live
            juce::AudioBuffer<float> slice (1, 4800);
            for (int pos = 0; pos + 4800 <= part.getNumSamples(); pos += 4800)
            {
                slice.copyFrom (0, 0, part, 0, pos, 4800);
                juce::AudioBuffer<float> io (2, 4800);
                io.copyFrom (0, 0, slice, 0, 0, 4800); io.copyFrom (1, 0, slice, 0, 0, 4800);
                juce::MidiBuffer m;
                for (int b = 0; b < 4800; b += 480) { juce::AudioBuffer<float> v (io.getArrayOfWritePointers(), 2, b, 480); p.processBlock (v, m); }
                juce::MessageManager::getInstance()->runDispatchLoopUntil (40);
            }
        }
        if (toonShot != juce::File())
        {
            auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 2.0f);
            toonShot.deleteFile();
            juce::FileOutputStream os (toonShot);
            juce::PNGImageFormat().writeImageToStream (img, os);
            std::cout << "screenshot: " << toonShot.getFullPathName() << std::endl;
        }
        juce::MemoryBlock mb; p.getStateInformation (mb);
        VocalTuneProcessor q; q.setStateInformation (mb.getData(), (int) mb.getSize());
        ok (q.page == 1, "the Live set remembers which side was showing");
        vte->showPage (0);
    }

    // ---------------------------------------------------------------- CPU
    {
        VocalTuneProcessor p; p.prepareToPlay (sr, 256);
        p.loadSampleBuffer (pluck, sr, "Pluck");
        for (auto& e : p.presetList()) if (e.name == "Big Stack") p.applyPreset (e);
        setP (p, id::grit, 0.4f); setP (p, id::metal, 0.3f); setP (p, id::pitch, -5); setP (p, id::whisper, 0.2f);
        setP (p, id::talk, 0.7f);
        const auto start = juce::Time::getMillisecondCounterHiRes();
        auto out = runPlugin (p, voice, sr, 256);
        const double ms = juce::Time::getMillisecondCounterHiRes() - start;
        const double audioMs = 1000.0 * out.getNumSamples() / sr;
        ok (ms < audioMs * 0.25, "CPU, everything on: " + juce::String (100.0 * ms / audioMs, 1) + "% of one core");
    }

    std::cout << (fails == 0 ? "ALL TESTS PASSED" : juce::String (fails) + " TEST(S) FAILED") << std::endl;
    return fails == 0 ? 0 : 1;
}
