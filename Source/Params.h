#pragma once
// Every knob, switch and menu in VocalTune. Parameter IDs are saved in Live sets and presets,
// so never rename one once it has shipped.
#include <juce_audio_processors/juce_audio_processors.h>

namespace vt
{
namespace id
{
    // voice changer
    inline constexpr const char* pitch       = "pitch";
    inline constexpr const char* fine        = "fine";
    inline constexpr const char* formant     = "formant";
    inline constexpr const char* whisper     = "whisper";
    inline constexpr const char* wobble      = "wobble";
    inline constexpr const char* wobbleRate  = "wobbleRate";
    // tune / robot
    inline constexpr const char* tune        = "tune";
    inline constexpr const char* tuneSpeed   = "tuneSpeed";
    inline constexpr const char* key         = "key";
    inline constexpr const char* scale       = "scale";
    inline constexpr const char* robot       = "robot";
    inline constexpr const char* robotNote   = "robotNote";
    // talk engine
    inline constexpr const char* talk        = "talk";
    inline constexpr const char* carrier     = "carrier";
    inline constexpr const char* chord       = "chord";
    inline constexpr const char* talkOctave  = "talkOctave";
    inline constexpr const char* bands       = "bands";
    inline constexpr const char* character   = "character";
    inline constexpr const char* sibilance   = "sibilance";
    inline constexpr const char* follow      = "follow";
    inline constexpr const char* sampleTune  = "sampleTune";
    inline constexpr const char* sampleMode  = "sampleMode";
    inline constexpr const char* sampleStart = "sampleStart";
    // effects
    inline constexpr const char* buzz        = "buzz";
    inline constexpr const char* buzzHz      = "buzzHz";
    inline constexpr const char* crush       = "crush";
    inline constexpr const char* decimate    = "decimate";
    inline constexpr const char* grit        = "grit";
    inline constexpr const char* tone        = "tone";
    inline constexpr const char* metal       = "metal";
    inline constexpr const char* metalHz     = "metalHz";
    inline constexpr const char* radio       = "radio";
    inline constexpr const char* choir       = "choir";
    inline constexpr const char* echo        = "echo";
    inline constexpr const char* echoTime    = "echoTime";
    inline constexpr const char* echoFeedback= "echoFeedback";
    inline constexpr const char* space       = "space";
    inline constexpr const char* spaceSize   = "spaceSize";
    // master
    inline constexpr const char* mix         = "mix";
    inline constexpr const char* output      = "output";
}

inline const juce::StringArray& keyNames()
{
    static const juce::StringArray k { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return k;
}

struct ScaleDef { const char* name; int mask; };   // bit n set = semitone n above the key is allowed
inline const std::vector<ScaleDef>& scales()
{
    auto m = [] (std::initializer_list<int> notes) { int v = 0; for (int n : notes) v |= 1 << n; return v; };
    static const std::vector<ScaleDef> s {
        { "Chromatic",      0xFFF },
        { "Major",          m ({ 0, 2, 4, 5, 7, 9, 11 }) },
        { "Minor",          m ({ 0, 2, 3, 5, 7, 8, 10 }) },
        { "Harmonic Minor", m ({ 0, 2, 3, 5, 7, 8, 11 }) },
        { "Dorian",         m ({ 0, 2, 3, 5, 7, 9, 10 }) },
        { "Phrygian",       m ({ 0, 1, 3, 5, 7, 8, 10 }) },
        { "Pentatonic Maj", m ({ 0, 2, 4, 7, 9 }) },
        { "Pentatonic Min", m ({ 0, 3, 5, 7, 10 }) },
        { "Blues",          m ({ 0, 3, 5, 6, 7, 10 }) },
        { "Double Harmonic",m ({ 0, 1, 4, 5, 7, 8, 11 }) },
        { "Whole Tone",     m ({ 0, 2, 4, 6, 8, 10 }) },
        { "Power (1+5)",    m ({ 0, 7 }) },
        { "Root Only",      m ({ 0 }) },
    };
    return s;
}

inline const juce::StringArray& carrierNames()
{
    static const juce::StringArray c { "Sample", "Saw", "Square", "Super", "Pulse", "Noise", "Sidechain" };
    return c;
}
enum Carrier { carSample, carSaw, carSquare, carSuper, carPulse, carNoise, carSidechain };

struct ChordDef { const char* name; std::vector<int> notes; };
inline const std::vector<ChordDef>& chords()
{
    static const std::vector<ChordDef> c {
        { "Single",   { 0 } },
        { "Octaves",  { -12, 0, 12 } },
        { "Fifth",    { 0, 7 } },
        { "Power",    { -12, 0, 7 } },
        { "Major",    { 0, 4, 7 } },
        { "Minor",    { 0, 3, 7 } },
        { "Sus4",     { 0, 5, 7 } },
        { "Major 7",  { 0, 4, 7, 11 } },
        { "Minor 7",  { 0, 3, 7, 10 } },
        { "Big Stack",{ -12, 0, 7, 12 } },
    };
    return c;
}

inline const juce::StringArray& bandNames()
{
    static const juce::StringArray b { "Smooth", "32 Bands", "20 Bands", "12 Bands", "8 Bands" };
    return b;
}
inline int bandCount (int index) { static const int n[] { 0, 32, 20, 12, 8 }; return n[juce::jlimit (0, 4, index)]; }

inline const juce::StringArray& sampleModeNames()
{
    static const juce::StringArray m { "Loop", "Retrigger" };
    return m;
}

inline juce::String noteName (float midi)
{
    const int n = juce::roundToInt (midi);
    return keyNames()[((n % 12) + 12) % 12] + juce::String (n / 12 - 1);
}

inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;
    int v = 1;

    auto pct = [] (float x, int) { return String (roundToInt (x * 100.0f)) + "%"; };
    auto add = [&] (const char* pid, const char* name, NormalisableRange<float> r, float def,
                    std::function<String (float, int)> toText, const char* label = "")
    {
        p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { pid, v }, name, r, def,
            AudioParameterFloatAttributes().withLabel (label).withStringFromValueFunction (std::move (toText))));
    };
    auto skewed = [] (float lo, float hi, float centre) { NormalisableRange<float> r (lo, hi); r.setSkewForCentre (centre); return r; };
    auto amount = [&] (const char* pid, const char* name, float def = 0.0f) { add (pid, name, { 0.0f, 1.0f }, def, pct); };
    auto choice = [&] (const char* pid, const char* name, const StringArray& items, int def)
    {
        p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { pid, v }, name, items, def));
    };

    auto semis = [] (float x, int)
    {
        if (std::abs (x) < 0.05f) return String ("0.0 st");
        return (x > 0.0f ? "+" : "") + String (x, std::abs (x) < 10 ? 1 : 0) + " st";
    };

    // ---- voice
    add (id::pitch,   "Pitch",   { -24.0f, 24.0f, 0.01f }, 0.0f, semis);
    add (id::fine,    "Fine",    { -100.0f, 100.0f, 0.1f }, 0.0f, [] (float x, int) { return String (roundToInt (x)) + " ct"; });
    add (id::formant, "Formant", { -12.0f, 12.0f, 0.01f }, 0.0f, semis);
    amount (id::whisper, "Whisper");
    amount (id::wobble,  "Wobble");
    add (id::wobbleRate, "Wobble Rate", skewed (0.3f, 14.0f, 4.0f), 5.5f, [] (float x, int) { return String (x, 1) + " Hz"; });

    // ---- tune / robot
    amount (id::tune, "Tune");
    add (id::tuneSpeed, "Tune Speed", skewed (0.0f, 400.0f, 40.0f), 25.0f, [] (float x, int) { return String (roundToInt (x)) + " ms"; });
    choice (id::key, "Key", vt::keyNames(), 0);
    { StringArray s; for (auto& d : scales()) s.add (d.name); choice (id::scale, "Scale", s, 1); }
    amount (id::robot, "Robot");
    add (id::robotNote, "Robot Note", { 24.0f, 84.0f, 1.0f }, 48.0f, [] (float x, int) { return noteName (x); });

    // ---- talk
    amount (id::talk, "Talk");
    choice (id::carrier, "Talk Sound", carrierNames(), carSaw);
    { StringArray s; for (auto& c : chords()) s.add (c.name); choice (id::chord, "Chord", s, 0); }
    add (id::talkOctave, "Talk Octave", { -2.0f, 2.0f, 1.0f }, 0.0f, [] (float x, int) { int o = roundToInt (x); return (o > 0 ? "+" : "") + String (o) + " oct"; });
    choice (id::bands, "Bands", bandNames(), 0);
    amount (id::character, "Character", 0.9f);
    amount (id::sibilance, "Sibilance", 0.45f);
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { id::follow, v }, "Follow Pitch", true));
    add (id::sampleTune, "Sample Tune", { -24.0f, 24.0f, 0.01f }, 0.0f, semis);
    choice (id::sampleMode, "Sample Mode", sampleModeNames(), 0);
    amount (id::sampleStart, "Sample Start");

    // ---- effects
    amount (id::buzz, "Buzz");
    add (id::buzzHz, "Buzz Hz", skewed (10.0f, 1200.0f, 120.0f), 60.0f, [] (float x, int) { return String (roundToInt (x)) + " Hz"; });
    amount (id::crush, "Crush");
    amount (id::decimate, "Decimate");
    amount (id::grit, "Grit");
    add (id::tone, "Tone", { -1.0f, 1.0f }, 0.0f, [] (float x, int) { int t = roundToInt (x * 100); return t == 0 ? String ("Flat") : (t < 0 ? "Dark " + String (-t) : "Bright " + String (t)); });
    amount (id::metal, "Metal");
    add (id::metalHz, "Metal Hz", skewed (40.0f, 2500.0f, 250.0f), 220.0f, [] (float x, int) { return String (roundToInt (x)) + " Hz"; });
    amount (id::radio, "Radio");
    amount (id::choir, "Choir");
    amount (id::echo, "Echo");
    add (id::echoTime, "Echo Time", skewed (20.0f, 1500.0f, 300.0f), 280.0f, [] (float x, int) { return String (roundToInt (x)) + " ms"; });
    add (id::echoFeedback, "Echo Feedback", { 0.0f, 0.95f }, 0.35f, pct);
    amount (id::space, "Space");
    amount (id::spaceSize, "Space Size", 0.6f);

    // ---- master
    amount (id::mix, "Mix", 1.0f);
    add (id::output, "Output", { -24.0f, 12.0f, 0.1f }, 0.0f, [] (float x, int) { return (x > 0.04f ? "+" : "") + String (x, 1) + " dB"; });

    return { p.begin(), p.end() };
}
} // namespace vt
