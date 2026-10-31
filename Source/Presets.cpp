#include "Presets.h"
#include "Params.h"

namespace vt
{
// carrier: 0 Sample 1 Saw 2 Square 3 Super 4 Pulse 5 Noise 6 Sidechain
// chord:   0 Single 1 Octaves 2 Fifth 3 Power 4 Major 5 Minor 6 Sus4 7 Maj7 8 Min7 9 Big Stack
// scale:   0 Chromatic 1 Major 2 Minor 3 HarmMinor 4 Dorian 5 Phrygian 6 PentMaj 7 PentMin 8 Blues
//          9 DoubleHarmonic 10 WholeTone 11 Power 12 Root
// bands:   0 Smooth 1 32 2 20 3 12 4 8
const std::vector<FactoryPreset>& factoryPresets()
{
    static const std::vector<FactoryPreset> p {
        // ------------------------------------------------------------ Classic
        { "Clean Slate",   "Classic", {} },
        { "Deep Voice",    "Classic", { { "pitch", -5 }, { "formant", -3 }, { "tone", -0.15f } } },
        { "Monster",       "Classic", { { "pitch", -10 }, { "formant", -6 }, { "grit", 0.35f }, { "tone", -0.3f }, { "space", 0.25f }, { "spaceSize", 0.75f } } },
        { "Giant",         "Classic", { { "pitch", -7 }, { "formant", -5 }, { "echo", 0.15f }, { "echoTime", 140 }, { "echoFeedback", 0.2f }, { "space", 0.4f }, { "spaceSize", 0.95f } } },
        { "Chipmunk",      "Classic", { { "pitch", 9 }, { "formant", 6 } } },
        { "Helium",        "Classic", { { "pitch", 4 }, { "formant", 7 } } },
        { "Baby",          "Classic", { { "pitch", 4 }, { "formant", 8 }, { "wobble", 0.25f }, { "wobbleRate", 6.5f }, { "tone", 0.2f } } },
        { "Little Kid",    "Classic", { { "pitch", 3 }, { "formant", 4.5f } } },
        { "Robot",         "Classic", { { "robot", 1 }, { "robotNote", 48 }, { "tuneSpeed", 5 }, { "metal", 0.45f }, { "metalHz", 110 }, { "crush", 0.15f }, { "buzz", 0.25f }, { "buzzHz", 55 } } },
        { "Ring Bot",      "Classic", { { "buzz", 0.9f }, { "buzzHz", 30 }, { "metal", 0.3f }, { "metalHz", 180 }, { "grit", 0.15f } } },
        { "Demon",         "Classic", { { "pitch", -12 }, { "formant", -7 }, { "mix", 0.8f }, { "grit", 0.4f }, { "choir", 0.35f }, { "space", 0.4f }, { "spaceSize", 0.85f } } },
        { "Alien",         "Classic", { { "pitch", 4 }, { "formant", 3 }, { "buzz", 0.5f }, { "buzzHz", 260 }, { "wobble", 0.4f }, { "wobbleRate", 9 }, { "choir", 0.5f }, { "echo", 0.2f } } },
        { "Ghost",         "Classic", { { "whisper", 0.7f }, { "pitch", 2 }, { "formant", 1 }, { "wobble", 0.3f }, { "wobbleRate", 3 }, { "echo", 0.35f }, { "echoTime", 380 }, { "echoFeedback", 0.5f }, { "space", 0.7f }, { "spaceSize", 0.95f } } },
        { "Witch",         "Classic", { { "pitch", 5 }, { "formant", 2 }, { "grit", 0.25f }, { "wobble", 0.2f }, { "wobbleRate", 8 }, { "tone", 0.4f } } },
        { "Whisper",       "Classic", { { "whisper", 1 }, { "tone", 0.25f } } },
        { "Troll",         "Classic", { { "pitch", -4 }, { "formant", -8 }, { "grit", 0.2f }, { "tone", -0.4f } } },
        { "Dragon",        "Classic", { { "pitch", -14 }, { "formant", -6 }, { "grit", 0.6f }, { "buzz", 0.2f }, { "buzzHz", 40 }, { "space", 0.4f }, { "spaceSize", 0.8f } } },
        { "Elf",           "Classic", { { "pitch", 7 }, { "formant", 4 }, { "choir", 0.5f }, { "space", 0.3f } } },
        { "Dark Lord",     "Classic", { { "pitch", -5 }, { "formant", -4 }, { "grit", 0.2f }, { "metal", 0.2f }, { "metalHz", 90 }, { "space", 0.3f }, { "tone", -0.3f } } },
        { "Cyborg",        "Classic", { { "pitch", -3 }, { "formant", -2 }, { "robot", 0.5f }, { "metal", 0.5f }, { "metalHz", 300 }, { "crush", 0.3f } } },
        // ------------------------------------------------------------ Talk
        { "Talkbox Funk",  "Talk", { { "talk", 1 }, { "carrier", 1 }, { "tune", 0.7f }, { "tuneSpeed", 15 }, { "grit", 0.15f }, { "sibilance", 0.5f } } },
        { "Disco Bot",     "Talk", { { "talk", 1 }, { "carrier", 3 }, { "chord", 1 }, { "tune", 1 }, { "tuneSpeed", 0 }, { "choir", 0.4f }, { "space", 0.2f } , { "vibratoKeep", 0 } } },
        { "Robot Choir",   "Talk", { { "talk", 1 }, { "carrier", 3 }, { "chord", 4 }, { "tune", 1 }, { "tuneSpeed", 10 }, { "choir", 0.6f }, { "space", 0.5f } , { "vibratoKeep", 0 } } },
        { "Minor Choir",   "Talk", { { "talk", 1 }, { "carrier", 3 }, { "chord", 5 }, { "tune", 1 }, { "tuneSpeed", 10 }, { "scale", 2 }, { "key", 9 }, { "choir", 0.6f }, { "space", 0.55f }, { "spaceSize", 0.85f } } },
        { "Power Talker",  "Talk", { { "talk", 1 }, { "carrier", 2 }, { "chord", 3 }, { "grit", 0.3f } } },
        { "Sad Robot",     "Talk", { { "talk", 1 }, { "carrier", 4 }, { "chord", 8 }, { "tune", 1 }, { "scale", 2 }, { "key", 9 }, { "echo", 0.3f }, { "space", 0.4f } } },
        { "Bass Mouth",    "Talk", { { "talk", 1 }, { "carrier", 1 }, { "talkOctave", -1 }, { "chord", 1 }, { "grit", 0.3f }, { "tone", -0.2f } } },
        { "Retro Vocoder", "Talk", { { "talk", 1 }, { "carrier", 1 }, { "bands", 2 }, { "chord", 2 }, { "sibilance", 0.6f } } },
        { "8-Bit Talker",  "Talk", { { "talk", 1 }, { "carrier", 2 }, { "bands", 4 }, { "crush", 0.5f }, { "decimate", 0.3f }, { "tune", 1 }, { "tuneSpeed", 0 } , { "vibratoKeep", 0 } } },
        { "Wind Voice",    "Talk", { { "talk", 1 }, { "carrier", 5 }, { "sibilance", 0.2f }, { "space", 0.5f }, { "spaceSize", 0.9f } } },
        { "Monster Talk",  "Talk", { { "talk", 1 }, { "carrier", 3 }, { "talkOctave", -1 }, { "formant", -5 }, { "grit", 0.4f }, { "space", 0.3f } } },
        { "Squeaky Talk",  "Talk", { { "talk", 1 }, { "carrier", 2 }, { "talkOctave", 1 }, { "formant", 5 } } },
        { "Half Human",    "Talk", { { "talk", 0.5f }, { "carrier", 1 }, { "tune", 0.5f } } },
        { "Big Stack",     "Talk", { { "talk", 1 }, { "carrier", 3 }, { "chord", 9 }, { "tune", 1 }, { "tuneSpeed", 5 }, { "space", 0.6f }, { "spaceSize", 0.9f }, { "echo", 0.2f } } },
        { "Organ Preacher","Talk", { { "talk", 1 }, { "carrier", 4 }, { "chord", 7 }, { "tune", 1 }, { "tuneSpeed", 20 }, { "choir", 0.3f }, { "space", 0.45f } } },
        // ------------------------------------------------------------ Sample (load a sample first)
        { "Talking Sample","Sample", { { "talk", 1 }, { "carrier", 0 }, { "follow", 1 }, { "character", 0.9f } } },
        { "Sample Choir",  "Sample", { { "talk", 1 }, { "carrier", 0 }, { "chord", 4 }, { "choir", 0.4f }, { "space", 0.3f } } },
        { "Sample Texture","Sample", { { "talk", 1 }, { "carrier", 0 }, { "follow", 0 }, { "character", 0.6f }, { "sibilance", 0.3f } } },
        { "Sample Stutter","Sample", { { "talk", 1 }, { "carrier", 0 }, { "sampleMode", 1 }, { "follow", 0 }, { "character", 0.8f }, { "echo", 0.25f }, { "echoTime", 180 } } },
        { "Sample Robot",  "Sample", { { "talk", 1 }, { "carrier", 0 }, { "robot", 1 }, { "tuneSpeed", 5 }, { "metal", 0.3f }, { "metalHz", 140 } } },
        { "Sample Tuned",  "Sample", { { "talk", 1 }, { "carrier", 0 }, { "tune", 1 }, { "tuneSpeed", 0 }, { "scale", 2 }, { "key", 9 } } },
        { "Sidechain Talk","Sample", { { "talk", 1 }, { "carrier", 6 }, { "character", 1 } } },
        // ------------------------------------------------------------ Tune
        { "Hard Tune",     "Tune", { { "tune", 1 }, { "tuneSpeed", 0 } , { "vibratoKeep", 0 } } },
        { "Soft Tune",     "Tune", { { "tune", 1 }, { "tuneSpeed", 90 } } },
        { "Pop Snap",      "Tune", { { "tune", 1 }, { "tuneSpeed", 12 }, { "choir", 0.3f }, { "space", 0.2f } , { "vibratoKeep", 0 } } },
        { "Minor Mood",    "Tune", { { "tune", 1 }, { "tuneSpeed", 8 }, { "scale", 2 }, { "key", 9 }, { "choir", 0.3f }, { "echo", 0.2f } } },
        { "Pentatonic Hero","Tune", { { "tune", 1 }, { "tuneSpeed", 0 }, { "scale", 7 }, { "key", 9 } , { "vibratoKeep", 0 } } },
        { "Blues Bot",     "Tune", { { "tune", 1 }, { "tuneSpeed", 0 }, { "scale", 8 }, { "buzz", 0.2f }, { "metal", 0.2f } , { "vibratoKeep", 0 } } },
        { "Monotone",      "Tune", { { "robot", 1 }, { "tuneSpeed", 20 } } },
        { "Chipmunk Tune", "Tune", { { "pitch", 12 }, { "formant", 6 }, { "tune", 1 }, { "tuneSpeed", 0 } , { "vibratoKeep", 0 } } },
        { "Deep Tune",     "Tune", { { "pitch", -12 }, { "formant", -4 }, { "tune", 1 }, { "tuneSpeed", 5 } } },
        { "Octave Up Pop", "Tune", { { "pitch", 12 }, { "formant", 0 }, { "tune", 1 }, { "tuneSpeed", 10 }, { "mix", 0.6f }, { "choir", 0.3f } } },
        // ------------------------------------------------------------ Auto Toon
        { "Hard Toon",     "Auto Toon", { { "tune", 1 }, { "tuneSpeed", 0 }, { "vibratoKeep", 0 } } },
        { "Pop Polish",    "Auto Toon", { { "tune", 1 }, { "tuneSpeed", 30 }, { "humanize", 0.5f }, { "vibratoKeep", 0.8f } } },
        { "Natural Lift",  "Auto Toon", { { "tune", 0.85f }, { "tuneSpeed", 70 }, { "humanize", 0.7f }, { "flex", 0.5f }, { "vibratoKeep", 1 } } },
        { "Robo Rapper",   "Auto Toon", { { "tune", 1 }, { "tuneSpeed", 0 }, { "vibratoKeep", 0 }, { "scale", 7 }, { "key", 9 } } },
        { "Trap Toon",     "Auto Toon", { { "tune", 1 }, { "tuneSpeed", 0 }, { "vibratoKeep", 0 }, { "scale", 2 }, { "key", 9 }, { "choir", 0.25f }, { "echo", 0.2f }, { "echoTime", 330 } } },
        { "Wobble Toon",   "Auto Toon", { { "tune", 1 }, { "tuneSpeed", 5 }, { "vibratoKeep", 2 }, { "wobble", 0.15f }, { "wobbleRate", 5.5f } } },
        { "Cartoon Duck",  "Auto Toon", { { "tune", 1 }, { "tuneSpeed", 0 }, { "vibratoKeep", 0 }, { "pitch", 7 }, { "formant", 7 }, { "crush", 0.2f } } },
        { "Old Record",    "Auto Toon", { { "tune", 1 }, { "tuneSpeed", 20 }, { "radio", 0.7f }, { "crush", 0.15f }, { "decimate", 0.1f }, { "tone", -0.3f } } },
        { "Big Band Toon", "Auto Toon", { { "tune", 1 }, { "tuneSpeed", 10 }, { "choir", 0.5f }, { "space", 0.4f } } },
        { "Basement Toon", "Auto Toon", { { "tune", 1 }, { "tuneSpeed", 0 }, { "vibratoKeep", 0 }, { "pitch", -12 }, { "formant", -3 } } },
        // ------------------------------------------------------------ Radio & Weird
        { "Megaphone",     "Weird", { { "radio", 0.9f }, { "grit", 0.5f }, { "tone", 0.3f } } },
        { "Phone Call",    "Weird", { { "radio", 1 }, { "crush", 0.25f }, { "decimate", 0.3f } } },
        { "Walkie Talkie", "Weird", { { "radio", 1 }, { "grit", 0.6f }, { "crush", 0.3f } } },
        { "Underwater",    "Weird", { { "pitch", -2 }, { "wobble", 0.6f }, { "wobbleRate", 1.5f }, { "tone", -0.8f }, { "metal", 0.3f }, { "metalHz", 90 }, { "space", 0.5f } } },
        { "Tin Can",       "Weird", { { "metal", 0.8f }, { "metalHz", 900 }, { "radio", 0.5f } } },
        { "Glitch Kid",    "Weird", { { "pitch", 4 }, { "formant", 5 }, { "crush", 0.6f }, { "decimate", 0.6f } } },
        { "Space Station", "Weird", { { "radio", 0.6f }, { "echo", 0.4f }, { "echoTime", 450 }, { "echoFeedback", 0.55f }, { "space", 0.5f }, { "buzz", 0.15f }, { "buzzHz", 400 } } },
        { "Haunted Radio", "Weird", { { "radio", 0.8f }, { "whisper", 0.4f }, { "echo", 0.3f }, { "space", 0.6f }, { "wobble", 0.3f }, { "wobbleRate", 2 } } },
        { "Insect",        "Weird", { { "pitch", 12 }, { "formant", 10 }, { "buzz", 0.6f }, { "buzzHz", 700 }, { "wobble", 0.3f }, { "wobbleRate", 11 } } },
        { "Drunk Robot",   "Weird", { { "robot", 0.7f }, { "wobble", 0.8f }, { "wobbleRate", 1.2f }, { "metal", 0.3f } } },
        { "Cave",          "Weird", { { "space", 1 }, { "spaceSize", 1 }, { "echo", 0.18f }, { "echoTime", 600 }, { "echoFeedback", 0.35f }, { "output", -3 } } },
        { "Broken Toy",    "Weird", { { "crush", 0.7f }, { "decimate", 0.5f }, { "wobble", 0.5f }, { "wobbleRate", 4.5f }, { "pitch", 3 }, { "formant", 3 }, { "grit", 0.3f } } },
        { "Shapeshifter",  "Weird", { { "pitch", -3 }, { "formant", 6 }, { "wobble", 0.35f }, { "wobbleRate", 0.7f } } },
        { "Tiny Giant",    "Weird", { { "pitch", -9 }, { "formant", 5 } } },
        { "Huge Baby",     "Weird", { { "pitch", 5 }, { "formant", -6 }, { "space", 0.3f } } },
    };
    return p;
}

juce::StringArray factoryCategories()
{
    juce::StringArray c;
    for (auto& p : factoryPresets()) c.addIfNotAlreadyThere (p.category);
    return c;
}

static void setReal (juce::RangedAudioParameter* prm, float v)
{
    prm->setValueNotifyingHost (prm->convertTo0to1 (v));
}

void applyFactory (juce::AudioProcessorValueTreeState& state, const FactoryPreset& p)
{
    for (auto* prm : state.processor.getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (prm))
            r->setValueNotifyingHost (r->getDefaultValue());
    for (auto& [pid, v] : p.values)
        if (auto* prm = state.getParameter (pid))
            setReal (prm, v);
}

void randomise (juce::AudioProcessorValueTreeState& state, juce::Random& rng)
{
    auto set = [&] (const char* pid, float v) { if (auto* p = state.getParameter (pid)) setReal (p, v); };
    auto rnd = [&] (float lo, float hi) { return lo + rng.nextFloat() * (hi - lo); };
    auto chance = [&] (float c) { return rng.nextFloat() < c; };

    for (auto* prm : state.processor.getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (prm))
            r->setValueNotifyingHost (r->getDefaultValue());

    const float pitch = chance (0.75f) ? std::round (rnd (-12, 12)) : 0.0f;
    set (id::pitch, pitch);
    set (id::formant, juce::jlimit (-12.0f, 12.0f, pitch * rnd (0.2f, 0.9f) + rnd (-4, 4)));
    if (chance (0.15f)) set (id::whisper, rnd (0.3f, 0.9f));
    if (chance (0.3f)) { set (id::wobble, rnd (0.1f, 0.7f)); set (id::wobbleRate, rnd (0.5f, 11)); }
    if (chance (0.3f))
    {
        set (id::tune, 1); set (id::tuneSpeed, chance (0.5f) ? 0.0f : rnd (5, 90));
        set (id::key, (float) rng.nextInt (12)); set (id::scale, (float) rng.nextInt (9));
    }
    if (chance (0.12f)) { set (id::robot, rnd (0.5f, 1)); set (id::robotNote, std::round (rnd (40, 60))); }
    if (chance (0.4f))
    {
        set (id::talk, rnd (0.6f, 1));
        set (id::carrier, (float) (1 + rng.nextInt (5)));
        set (id::chord, (float) rng.nextInt ((int) chords().size()));
        set (id::talkOctave, chance (0.3f) ? (float) (rng.nextInt (3) - 1) : 0.0f);
        if (chance (0.3f)) set (id::bands, (float) (1 + rng.nextInt (4)));
        set (id::sibilance, rnd (0.2f, 0.7f));
    }

    // two to four effects
    const char* fx[] { id::buzz, id::crush, id::grit, id::metal, id::radio, id::choir, id::echo, id::space };
    const int count = 2 + rng.nextInt (3);
    for (int i = 0; i < count; ++i)
    {
        const char* which = fx[rng.nextInt (8)];
        set (which, rnd (0.15f, 0.75f));
        if (which == id::buzz) set (id::buzzHz, rnd (20, 600));
        if (which == id::crush) set (id::decimate, rnd (0, 0.6f));
        if (which == id::metal) set (id::metalHz, rnd (60, 1200));
        if (which == id::echo) { set (id::echoTime, rnd (80, 700)); set (id::echoFeedback, rnd (0.1f, 0.6f)); }
        if (which == id::space) set (id::spaceSize, rnd (0.3f, 1));
    }
    if (chance (0.3f)) set (id::tone, rnd (-0.6f, 0.6f));
}

void mutate (juce::AudioProcessorValueTreeState& state, juce::Random& rng)
{
    for (auto* prm : state.processor.getParameters())
    {
        auto* r = dynamic_cast<juce::RangedAudioParameter*> (prm);
        if (r == nullptr) continue;
        const auto pid = r->getParameterID();
        if (pid == id::mix || pid == id::output) continue;
        if (dynamic_cast<juce::AudioParameterFloat*> (r) != nullptr)
        {
            if (rng.nextFloat() < 0.5f) continue;
            const float g = (rng.nextFloat() + rng.nextFloat() + rng.nextFloat() - 1.5f) * 0.12f;
            r->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, r->getValue() + g));
        }
        else if (pid == id::chord && rng.nextFloat() < 0.15f)
        {
            r->setValueNotifyingHost (rng.nextFloat());
        }
    }
}

juce::File userPresetFolder()
{
    return juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("VocalTune").getChildFile ("Presets");
}

juce::Array<juce::File> userPresetFiles()
{
    auto files = userPresetFolder().findChildFiles (juce::File::findFiles, false, "*.vtpreset");
    files.sort();
    return files;
}
}
