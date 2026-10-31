#include "PluginEditor.h"
#include "Knob.h"
#include "PresetScreen.h"
#include "AutoToonPage.h"

using namespace vt;
using APVTS = juce::AudioProcessorValueTreeState;

// ====================================================================== sample drop zone
class SampleZone : public juce::Component, public juce::FileDragAndDropTarget, public juce::SettableTooltipClient, private juce::Timer
{
public:
    explicit SampleZone (VocalTuneProcessor& p) : proc (p)
    {
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
        setTooltip ("Drop any sound here (or click to browse). Pick SAMPLE as the talk sound and your voice will make it talk.");
        startTimerHz (10);
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (juce::Colours::black.withAlpha (0.22f));
        g.fillRoundedRectangle (r.translated (0, 2), 10.0f);
        g.setColour (dragOver ? juce::Colours::white : toy::cream);
        g.fillRoundedRectangle (r, 10.0f);

        auto s = proc.getSample();
        if (s == nullptr)
        {
            juce::Path dash, outline;
            outline.addRoundedRectangle (r.reduced (5.0f), 7.0f);
            const float lens[] { 6.0f, 5.0f };
            juce::PathStrokeType (2.0f).createDashedStroke (dash, outline, lens, 2);
            g.setColour (toy::blue.withAlpha (0.6f));
            g.fillPath (dash);
            g.setColour (toy::ink);
            g.setFont (toy::titleFont (19.0f));
            g.drawText (error.isNotEmpty() ? juce::String ("Hmm...") : juce::String ("DROP A SAMPLE"), r.withTrimmedBottom (r.getHeight() * 0.38f), juce::Justification::centredBottom, false);
            g.setFont (toy::bodyFont (12.5f));
            g.setColour (toy::brown);
            g.drawFittedText (error.isNotEmpty() ? error : juce::String ("dog barks, guitars, synths, anything!\nor click to browse"),
                              r.withTrimmedTop (r.getHeight() * 0.62f).reduced (10, 0).toNearestInt(), juce::Justification::centredTop, 2);
            return;
        }

        auto wave = r.reduced (8.0f).withTrimmedTop (16.0f).withTrimmedBottom (14.0f);
        const auto& pk = s->peaks;
        const float startFrac = proc.state.getRawParameterValue (id::sampleStart)->load();
        g.setColour (toy::blue.withAlpha (0.12f));
        g.fillRect (wave.withWidth (wave.getWidth() * startFrac));
        const int cols = (int) wave.getWidth() / 3;
        for (int i = 0; i < cols; ++i)
        {
            const float v = pk.empty() ? 0.0f : pk[(size_t) (i * (int) pk.size() / std::max (1, cols))];
            const float h = std::max (1.5f, v * wave.getHeight());
            const float x = wave.getX() + (float) i * 3.0f;
            const bool before = (float) i / (float) cols < startFrac;
            g.setColour (before ? toy::blue.withAlpha (0.3f) : toy::blue);
            g.fillRoundedRectangle (x, wave.getCentreY() - h * 0.5f, 2.0f, h, 1.0f);
        }
        g.setColour (toy::ink);
        g.setFont (toy::bodyFont (13.0f));
        g.drawText (s->name, r.reduced (10.0f, 3.0f).withHeight (16.0f).withTrimmedRight (18.0f), juce::Justification::centredLeft, true);
        g.setFont (toy::bodyFont (11.5f));
        g.setColour (toy::brown);
        g.drawText (s->rootHz > 0.0f ? "root " + noteName (69.0f + 12.0f * std::log2 (s->rootHz / 440.0f)) : juce::String ("no pitch: plays as is"),
                    r.reduced (10.0f, 3.0f).withTrimmedTop (r.getHeight() - 20.0f), juce::Justification::centredLeft, false);
        g.drawText (juce::String (s->length() / s->sampleRate, 1) + " s",
                    r.reduced (10.0f, 3.0f).withTrimmedTop (r.getHeight() - 20.0f), juce::Justification::centredRight, false);
        // clear "x"
        auto x = clearArea().toFloat();
        g.setColour (toy::red);
        g.fillEllipse (x);
        g.setColour (juce::Colours::white);
        g.drawLine (x.getX() + 5, x.getY() + 5, x.getRight() - 5, x.getBottom() - 5, 2.0f);
        g.drawLine (x.getRight() - 5, x.getY() + 5, x.getX() + 5, x.getBottom() - 5, 2.0f);
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (proc.getSample() != nullptr && clearArea().contains (e.getPosition())) { proc.clearSample(); repaint(); return; }
        chooser = std::make_unique<juce::FileChooser> ("Pick a sound to make talk",
                                                       juce::File::getSpecialLocation (juce::File::userMusicDirectory),
                                                       "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg;*.m4a");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& fc) { if (fc.getResult().existsAsFile()) load (fc.getResult()); });
    }

    bool isInterestedInFileDrag (const juce::StringArray&) override { return true; }
    void fileDragEnter (const juce::StringArray&, int, int) override { dragOver = true; repaint(); }
    void fileDragExit (const juce::StringArray&) override { dragOver = false; repaint(); }
    void filesDropped (const juce::StringArray& files, int, int) override
    {
        dragOver = false;
        if (! files.isEmpty()) load (juce::File (files[0]));
        repaint();
    }

private:
    void load (const juce::File& f)
    {
        error = proc.loadSample (f);
        if (error.isEmpty())
            if (auto* c = proc.state.getParameter (id::carrier))
                c->setValueNotifyingHost (c->convertTo0to1 ((float) carSample));   // dropping a sample = use it
        errorTicks = error.isEmpty() ? 0 : 60;
        repaint();
    }
    juce::Rectangle<int> clearArea() const { return { getWidth() - 24, 5, 18, 18 }; }
    void timerCallback() override
    {
        auto s = proc.getSample().get();
        const float st = proc.state.getRawParameterValue (id::sampleStart)->load();
        if (errorTicks > 0 && --errorTicks == 0) { error.clear(); repaint(); }
        if (s != lastSeen || st != lastStart) { lastSeen = s; lastStart = st; repaint(); }
    }

    VocalTuneProcessor& proc;
    std::unique_ptr<juce::FileChooser> chooser;
    bool dragOver = false;
    juce::String error;
    int errorTicks = 0;
    const void* lastSeen = nullptr;
    float lastStart = -1.0f;
};

// ====================================================================== tuner screen
class TunerDisplay : public juce::Component, private juce::Timer
{
public:
    explicit TunerDisplay (VocalTuneProcessor& p) : proc (p) { startTimerHz (30); }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (juce::Colours::black.withAlpha (0.25f));
        g.fillRoundedRectangle (r.translated (0, 2), 10.0f);
        g.setColour (toy::ink);
        g.fillRoundedRectangle (r, 10.0f);
        g.setColour (juce::Colours::white.withAlpha (0.06f));
        g.fillRoundedRectangle (r.reduced (4).withHeight (r.getHeight() * 0.4f), 7.0f);

        auto inner = r.reduced (8.0f, 6.0f);
        auto meter = inner.removeFromBottom (8.0f);
        auto left = inner.removeFromLeft (inner.getWidth() * 0.5f);
        auto bubble = [&] (juce::Rectangle<float> a, const juce::String& cap, float midi, juce::Colour col)
        {
            g.setFont (toy::bodyFont (11.0f));
            g.setColour (toy::cream.withAlpha (0.6f));
            g.drawText (cap, a.removeFromTop (13.0f), juce::Justification::centred, false);
            g.setFont (toy::titleFont (24.0f));
            g.setColour (midi > 0.0f ? col : toy::cream.withAlpha (0.25f));
            g.drawText (midi > 0.0f ? noteName (midi) : juce::String ("--"), a, juce::Justification::centred, false);
        };
        bubble (left, "YOU SING", inMidi, toy::yellow);
        bubble (inner, "IT SINGS", outMidi, toy::green.brighter (0.3f));
        g.setColour (toy::cream.withAlpha (0.5f));
        g.setFont (toy::bodyFont (18.0f));
        g.drawText (">", r.withSizeKeepingCentre (20, 20).translated (0, 3), juce::Justification::centred, false);

        // input level
        g.setColour (juce::Colours::white.withAlpha (0.1f));
        g.fillRoundedRectangle (meter, 3.0f);
        const float lv = juce::jlimit (0.0f, 1.0f, (juce::Decibels::gainToDecibels (level, -60.0f) + 60.0f) / 60.0f);
        juce::ColourGradient grad (toy::green, meter.getX(), 0, toy::red, meter.getRight(), 0, false);
        grad.addColour (0.7, toy::yellow);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (meter.withWidth (meter.getWidth() * lv), 3.0f);
    }

private:
    void timerCallback() override
    {
        const float a = proc.engine.detectedMidi.load(), b = proc.engine.targetMidi.load();
        const float l = proc.engine.inputLevel.load() * 1.41f;
        level = std::max (l, level * 0.85f);
        if (a > 0) holdTicks = 15;
        if (a > 0 || --holdTicks <= 0) { inMidi = a; outMidi = b; }
        repaint();
    }
    VocalTuneProcessor& proc;
    float inMidi = -1, outMidi = -1, level = 0;
    int holdTicks = 0;
};

// ====================================================================== dice / mutate / save buttons
class IconButton : public juce::Button
{
public:
    enum Icon { dice, sparkle, disk };
    IconButton (const juce::String& name, Icon i, juce::Colour c) : juce::Button (name), icon (i), col (c) {}

    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        auto r = getLocalBounds().toFloat().reduced (2.0f, 1.0f).withTrimmedBottom (5.0f);
        if (down) r.translate (0, 2);
        toy::drawBlock (g, r, over ? col.brighter (0.12f) : col, 12.0f, down ? 3.0f : 5.0f);
        auto ic = r.withTrimmedBottom (16.0f).reduced (8.0f, 5.0f);
        const float s = std::min (ic.getWidth(), ic.getHeight());
        ic = ic.withSizeKeepingCentre (s, s);
        g.setColour (juce::Colours::white);
        if (icon == dice)
        {
            g.fillRoundedRectangle (ic, s * 0.22f);
            g.setColour (toy::ink);
            const float d = s * 0.16f;
            for (auto [px, py] : { std::pair { 0.27f, 0.27f }, { 0.73f, 0.27f }, { 0.5f, 0.5f }, { 0.27f, 0.73f }, { 0.73f, 0.73f } })
                g.fillEllipse (ic.getX() + ic.getWidth() * px - d / 2, ic.getY() + ic.getHeight() * py - d / 2, d, d);
        }
        else if (icon == sparkle)
        {
            auto star = [&] (juce::Point<float> c, float rad)
            {
                juce::Path p;
                for (int i = 0; i < 8; ++i)
                {
                    const float a = juce::MathConstants<float>::pi * (float) i / 4.0f;
                    const float rr = (i % 2 == 0) ? rad : rad * 0.32f;
                    const juce::Point<float> pt (c.x + std::sin (a) * rr, c.y - std::cos (a) * rr);
                    if (i == 0) p.startNewSubPath (pt); else p.lineTo (pt);
                }
                p.closeSubPath();
                g.fillPath (p);
            };
            star (ic.getCentre().translated (-s * 0.1f, s * 0.08f), s * 0.42f);
            star (ic.getTopRight().translated (-s * 0.14f, s * 0.16f), s * 0.18f);
        }
        else
        {
            g.fillRoundedRectangle (ic.reduced (s * 0.06f), s * 0.1f);
            g.setColour (col.darker (0.2f));
            g.fillRect (ic.reduced (s * 0.22f, 0).withTrimmedTop (s * 0.1f).withHeight (s * 0.3f));
            g.setColour (toy::cream);
            g.fillRoundedRectangle (ic.reduced (s * 0.18f).withTrimmedTop (s * 0.36f), s * 0.05f);
        }
        g.setFont (toy::bodyFont (12.5f));
        toy::shadowText (g, getName(), r.removeFromBottom (17.0f), juce::Justification::centred);
    }

private:
    Icon icon;
    juce::Colour col;
};

// ====================================================================== everything, at base size
class VocalTuneContent : public juce::Component
{
public:
    explicit VocalTuneContent (VocalTuneProcessor& p)
        : proc (p), zone (p), tuner (p), presets (p),
          diceBtn ("DICE", IconButton::dice, toy::yellow.darker (0.05f)),
          mutateBtn ("MUTATE", IconButton::sparkle, toy::pink),
          saveBtn ("SAVE", IconButton::disk, toy::green)
    {
        auto& st = proc.state;
        auto knob = [&] (const char* pid, const char* label, juce::Colour accent, bool bip = false) -> ToyKnob&
        {
            knobs.push_back (std::make_unique<ToyKnob> (st, pid, label, accent, bip));
            addAndMakeVisible (*knobs.back());
            knobById[pid] = knobs.back().get();
            return *knobs.back();
        };
        auto light = [] (juce::Colour c) { return c.brighter (0.35f); };

        // master
        knob (id::mix, "MIX", light (toy::navy));
        knob (id::output, "OUT", light (toy::navy), true);
        // voice
        knob (id::pitch, "PITCH", light (toy::red), true);
        knob (id::fine, "FINE", light (toy::red), true);
        knob (id::formant, "THROAT", light (toy::red), true);
        knob (id::whisper, "WHISPER", light (toy::red));
        knob (id::wobble, "WOBBLE", light (toy::red));
        knob (id::wobbleRate, "SPEED", light (toy::red));
        // talk
        knob (id::talk, "TALK", toy::yellow);
        knob (id::character, "FLATTEN", light (toy::blue));
        knob (id::sibilance, "S-SOUNDS", light (toy::blue));
        knob (id::talkOctave, "OCTAVE", light (toy::blue), true);
        knob (id::sampleTune, "SMP TUNE", light (toy::blue), true);
        knob (id::sampleStart, "START", light (toy::blue));
        // tune
        knob (id::tune, "TUNE", light (toy::green));
        knob (id::tuneSpeed, "SPEED", light (toy::green));
        knob (id::robot, "ROBOT", light (toy::green));
        knob (id::robotNote, "NOTE", light (toy::green));
        // fx
        knob (id::buzz, "BUZZ", light (toy::orange));      knob (id::buzzHz, "HZ", light (toy::orange));
        knob (id::crush, "BITS", light (toy::yellow).darker (0.1f)); knob (id::decimate, "RATE", light (toy::yellow).darker (0.1f));
        knob (id::grit, "GRIT", light (toy::purple));      knob (id::tone, "TONE", light (toy::purple), true);
        knob (id::metal, "METAL", light (toy::teal));      knob (id::metalHz, "HZ", light (toy::teal));
        knob (id::radio, "RADIO", light (toy::navy));
        knob (id::choir, "CHOIR", light (toy::pink));
        knob (id::echo, "ECHO", light (toy::blue));        knob (id::echoTime, "TIME", light (toy::blue)); knob (id::echoFeedback, "REPEAT", light (toy::blue));
        knob (id::space, "SPACE", light (toy::green));     knob (id::spaceSize, "SIZE", light (toy::green));

        knobFor (id::talk).slider.setTooltip ("How much the talk sound replaces your voice");
        knobFor (id::character).slider.setTooltip ("How much the talk sound's own tone is flattened so your mouth shapes it");
        knobFor (id::formant).slider.setTooltip ("Throat size (formants): down = bigger creature, up = tinier");

        // talk sound selector
        for (int i = 0; i < carrierNames().size(); ++i)
        {
            auto* b = sourceBtns.add (new juce::TextButton (carrierNames()[i].toUpperCase()));
            b->setColour (juce::TextButton::buttonColourId, toy::blue);
            b->setClickingTogglesState (false);
            b->onClick = [this, i] { if (auto* c = proc.state.getParameter (id::carrier)) c->setValueNotifyingHost (c->convertTo0to1 ((float) i)); };
            addAndMakeVisible (b);
        }
        sourceBtns[carSidechain]->setTooltip ("Uses the track routed into VocalTune's sidechain input");
        sourceBtns[carSample]->setTooltip ("The sample in the drop zone");
        carrierAttach = std::make_unique<juce::ParameterAttachment> (*st.getParameter (id::carrier),
            [this] (float v) { for (int i = 0; i < sourceBtns.size(); ++i) sourceBtns[i]->setToggleState (i == juce::roundToInt (v), juce::dontSendNotification); });
        carrierAttach->sendInitialUpdate();

        auto combo = [&] (juce::ComboBox& c, const char* pid, const juce::StringArray& items, const juce::String& tip)
        {
            c.addItemList (items, 1);
            c.setTooltip (tip);
            addAndMakeVisible (c);
            comboAttach.push_back (std::make_unique<APVTS::ComboBoxAttachment> (st, pid, c));
        };
        { juce::StringArray chordItems; for (auto& c : chords()) chordItems.add (c.name); combo (chordBox, id::chord, chordItems, "Play the talk sound as a chord"); }
        combo (bandsBox, id::bands, bandNames(), "Smooth = talkbox, fewer bands = old-school vocoder");
        combo (modeBox, id::sampleMode, sampleModeNames(), "Loop the sample, or restart it on every new word");
        combo (keyBox, id::key, keyNames(), "Key for Tune");
        { juce::StringArray sc; for (auto& s : scales()) sc.add (s.name); combo (scaleBox, id::scale, sc, "Scale for Tune"); }

        followBtn.setButtonText ("FOLLOW");
        followBtn.setColour (juce::ToggleButton::tickColourId, toy::blue);
        followBtn.setTooltip ("On: the sample sings the notes you sing. Off: it keeps its own pitch.");
        addAndMakeVisible (followBtn);
        followAttach = std::make_unique<APVTS::ButtonAttachment> (st, id::follow, followBtn);

        addAndMakeVisible (zone);
        addAndMakeVisible (tuner);
        addAndMakeVisible (presets);
        presets.onSaveRequested = [this] { askToSave(); };

        diceBtn.setTooltip ("Roll a brand new random voice");
        mutateBtn.setTooltip ("Nudge the current sound a little");
        saveBtn.setTooltip ("Save this sound as your own preset");
        diceBtn.onClick = [this] { proc.dice(); };
        mutateBtn.onClick = [this] { proc.mutateCurrent(); };
        saveBtn.onClick = [this] { askToSave(); };
        for (auto* b : { &diceBtn, &mutateBtn, &saveBtn }) addAndMakeVisible (b);
        toonBtn = createAutoToonButton();
        toonBtn->onClick = [this] { if (onAutoToon) onAutoToon(); };
        addAndMakeVisible (*toonBtn);

        layout();
    }

    void paint (juce::Graphics& g) override
    {
        if (background.isNull()) renderBackground();
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.drawImageTransformed (background, juce::AffineTransform::scale (0.5f));
    }

    std::function<void()> onAutoToon;

private:
    struct Block { juce::Rectangle<float> r; juce::Colour col; juce::String title, sub; };

    ToyKnob& knobFor (const char* pid) { return *knobById.at (pid); }

    void layout()
    {
        blocks.clear();
        auto place = [&] (const char* pid, int x, int y, int w, int h) { knobFor (pid).setBounds (x, y, w, h); };

        // header
        presets.setBounds (338, 14, 352, 66);
        diceBtn.setBounds (702, 14, 58, 70);
        mutateBtn.setBounds (764, 14, 58, 70);
        saveBtn.setBounds (826, 14, 58, 70);
        blocks.push_back ({ { 896, 14, 130, 68 }, toy::navy, "", "" });
        place (id::mix, 902, 14, 58, 70);
        place (id::output, 962, 14, 58, 70);

        // voice
        const int top = 100, rowH = 290;
        blocks.push_back ({ { 14, (float) top, 236, (float) rowH }, toy::red, "VOICE", "pitch & throat" });
        const char* voice[] { id::pitch, id::fine, id::formant, id::whisper, id::wobble, id::wobbleRate };
        for (int i = 0; i < 6; ++i) place (voice[i], 24 + (i % 3) * 72, top + 46 + (i / 3) * 118, 72, 108);

        // talk box
        const int tx = 262, tw = 516;
        blocks.push_back ({ { (float) tx, (float) top, (float) tw, (float) rowH }, toy::blue, "TALK BOX", "make any sound talk" });
        const int pillW = (tw - 24 - 6 * 5) / 7;
        for (int i = 0; i < sourceBtns.size(); ++i) sourceBtns[i]->setBounds (tx + 12 + i * (pillW + 5), top + 42, pillW, 26);
        zone.setBounds (tx + 12, top + 78, 224, 108);
        place (id::talk, tx + 244, top + 74, 96, 116);
        place (id::character, tx + 344, top + 80, 80, 104);
        place (id::sibilance, tx + 424, top + 80, 80, 104);
        place (id::talkOctave, tx + 12, top + 196, 66, 86);
        place (id::sampleTune, tx + 80, top + 196, 66, 86);
        place (id::sampleStart, tx + 148, top + 196, 66, 86);
        const int cx = tx + 230;
        captions.clear();
        captions.push_back ({ { (float) cx, (float) top + 202, 132, 14 }, "CHORD" });
        chordBox.setBounds (cx, top + 218, 132, 26);
        captions.push_back ({ { (float) cx + 142, (float) top + 202, 132, 14 }, "VOCODER BANDS" });
        bandsBox.setBounds (cx + 142, top + 218, 132, 26);
        modeBox.setBounds (cx, top + 252, 132, 26);
        followBtn.setBounds (cx + 146, top + 252, 128, 26);

        // tune & robot
        const int ux = 790, uw = 236;
        blocks.push_back ({ { (float) ux, (float) top, (float) uw, (float) rowH }, toy::green, "TUNE", "" });
        toonBtn->setBounds (ux + uw - 108, top + 8, 100, 30);
        tuner.setBounds (ux + 12, top + 42, uw - 24, 74);
        captions.push_back ({ { (float) ux + 12, (float) top + 124, 70, 14 }, "KEY" });
        captions.push_back ({ { (float) ux + 88, (float) top + 124, 136, 14 }, "SCALE" });
        keyBox.setBounds (ux + 12, top + 140, 70, 26);
        scaleBox.setBounds (ux + 88, top + 140, uw - 100, 26);
        const char* tune[] { id::tune, id::tuneSpeed, id::robot, id::robotNote };
        for (int i = 0; i < 4; ++i) place (tune[i], ux + 10 + i * 54, top + 178, 54, 100);

        // effects row
        struct Fx { const char* title; juce::Colour col; std::vector<const char*> ids; };
        const std::vector<Fx> fx {
            { "BUZZ", toy::orange, { id::buzz, id::buzzHz } },
            { "CRUSH", toy::yellow.darker (0.08f), { id::crush, id::decimate } },
            { "GRIT", toy::purple, { id::grit, id::tone } },
            { "METAL", toy::teal, { id::metal, id::metalHz } },
            { "RADIO", toy::navy, { id::radio } },
            { "CHOIR", toy::pink, { id::choir } },
            { "ECHO", toy::blue, { id::echo, id::echoTime, id::echoFeedback } },
            { "SPACE", toy::green, { id::space, id::spaceSize } },
        };
        const int fy = 404, fh = 154, cell = 55, pad = 9, gap = 6;
        int x = 14;
        for (auto& f : fx)
        {
            const int w = (int) f.ids.size() * cell + pad * 2;
            blocks.push_back ({ { (float) x, (float) fy, (float) w, (float) fh }, f.col, f.title, "" });
            for (size_t i = 0; i < f.ids.size(); ++i) place (f.ids[i], x + pad + (int) i * cell, fy + 38, cell, 104);
            x += w + gap;
        }
        fxRight = x - gap;
    }

    void renderBackground()
    {
        const int W = VocalTuneEditor::baseW, H = VocalTuneEditor::baseH;
        background = juce::Image (juce::Image::ARGB, W * 2, H * 2, true);
        juce::Graphics g (background);
        g.addTransform (juce::AffineTransform::scale (2.0f));
        auto all = juce::Rectangle<float> (0, 0, (float) W, (float) H);

        // the wooden toy chest
        g.setColour (toy::brown);
        g.fillRoundedRectangle (all, 18.0f);
        auto wood = all.reduced (5.0f);
        juce::ColourGradient wg (toy::wood1, 0, 0, toy::wood2, 0, (float) H, false);
        g.setGradientFill (wg);
        g.fillRoundedRectangle (wood, 14.0f);
        juce::Random rnd (7);
        g.saveState();
        g.reduceClipRegion (wood.toNearestInt());
        for (int plank = 1; plank < 6; ++plank)
        {
            const float y = (float) plank * (float) H / 6.0f + rnd.nextFloat() * 6.0f;
            g.setColour (toy::brown.withAlpha (0.16f));
            g.fillRect (0.0f, y, (float) W, 2.0f);
            g.setColour (juce::Colours::white.withAlpha (0.08f));
            g.fillRect (0.0f, y + 2.0f, (float) W, 1.0f);
        }
        for (int i = 0; i < 70; ++i)
        {
            juce::Path grain;
            const float y = rnd.nextFloat() * (float) H, x0 = rnd.nextFloat() * (float) W - 100.0f, len = 80.0f + rnd.nextFloat() * 260.0f;
            grain.startNewSubPath (x0, y);
            grain.cubicTo (x0 + len * 0.3f, y + rnd.nextFloat() * 6 - 3, x0 + len * 0.7f, y + rnd.nextFloat() * 6 - 3, x0 + len, y + rnd.nextFloat() * 4 - 2);
            g.setColour (toy::brown.withAlpha (0.05f + rnd.nextFloat() * 0.06f));
            g.strokePath (grain, juce::PathStrokeType (0.8f + rnd.nextFloat()));
        }
        g.restoreState();
        // brass screws in the corners
        for (auto pt : { juce::Point<float> (16, 16), { (float) W - 16, 16 }, { 16, (float) H - 16 }, { (float) W - 16, (float) H - 16 } })
        {
            auto s = juce::Rectangle<float> (11, 11).withCentre (pt);
            g.setColour (toy::c (0xffB8862B));
            g.fillEllipse (s);
            g.setColour (toy::c (0xff7A5518));
            g.drawEllipse (s, 1.2f);
            g.drawLine (s.getX() + 3, s.getCentreY(), s.getRight() - 3, s.getCentreY(), 1.4f);
        }

        // logo: alphabet blocks
        const juce::String word ("VOCALTUNE");
        const juce::Colour cols[] { toy::red, toy::orange, toy::yellow, toy::green, toy::blue, toy::purple, toy::pink, toy::teal, toy::navy };
        float lx = 24.0f;
        for (int i = 0; i < word.length(); ++i)
        {
            if (i == 5) lx += 8.0f;
            auto b = juce::Rectangle<float> (lx, 18.0f + ((i % 2) ? 3.0f : 0.0f), 31.0f, 31.0f);
            g.saveState();
            g.addTransform (juce::AffineTransform::rotation ((i % 2 ? 0.07f : -0.06f), b.getCentreX(), b.getCentreY()));
            toy::drawBlock (g, b, cols[i], 6.0f, 4.0f);
            g.setFont (toy::titleFont (25.0f));
            toy::shadowText (g, word.substring (i, i + 1), b.translated (0, 3.0f), juce::Justification::centred);
            g.restoreState();
            lx += 33.0f;
        }
        g.setFont (toy::bodyFont (13.5f));
        g.setColour (toy::brown);
        g.drawText ("toy box voice lab  *  drop it, talk it, tune it", juce::Rectangle<float> (24, 60, 320, 18), juce::Justification::centredLeft, false);

        // the toy blocks
        for (auto& b : blocks)
        {
            toy::drawBlock (g, b.r, b.col, 16.0f, 6.0f);
            if (b.title.isNotEmpty())
            {
                g.setFont (toy::titleFont (b.r.getWidth() < 100 ? 17.0f : 21.0f));
                const auto tr = b.r.reduced (b.r.getWidth() < 100 ? 8.0f : 14.0f, 0).withTrimmedTop (10.0f).withHeight (24.0f);
                toy::shadowText (g, b.title, tr.translated (0, 3.0f), juce::Justification::centredLeft, juce::Colours::white, 2.0f);
                if (b.sub.isNotEmpty())
                {
                    g.setFont (toy::bodyFont (12.5f));
                    g.setColour (juce::Colours::white.withAlpha (0.75f));
                    g.drawText (b.sub, tr.translated (0, 2.0f), juce::Justification::centredRight, false);
                }
            }
        }
        g.setFont (toy::bodyFont (11.0f));
        for (auto& c : captions)
        {
            g.setColour (juce::Colours::white.withAlpha (0.8f));
            g.drawText (c.second, c.first, juce::Justification::centredLeft, false);
        }
        g.setColour (toy::brown.withAlpha (0.7f));
        g.drawText ("v" JucePlugin_VersionString, juce::Rectangle<float> ((float) fxRight - 60, (float) H - 15, 60, 12), juce::Justification::centredRight, false);
    }

    void askToSave()
    {
        auto* w = new juce::AlertWindow ("Save your sound", "Give it a name:", juce::MessageBoxIconType::NoIcon, this);
        auto suggested = proc.getPresetName().trimCharactersAtEnd ("*");
        w->addTextEditor ("name", suggested);
        w->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
        w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        w->enterModalState (true, juce::ModalCallbackFunction::create ([this, w] (int result)
        {
            if (result == 1) proc.saveUserPreset (w->getTextEditorContents ("name"));
        }), true);
    }

    VocalTuneProcessor& proc;
    std::vector<std::unique_ptr<ToyKnob>> knobs;
    std::map<juce::String, ToyKnob*> knobById;
    juce::OwnedArray<juce::TextButton> sourceBtns;
    std::unique_ptr<juce::ParameterAttachment> carrierAttach;
    juce::ComboBox chordBox, bandsBox, modeBox, keyBox, scaleBox;
    std::vector<std::unique_ptr<APVTS::ComboBoxAttachment>> comboAttach;
    juce::ToggleButton followBtn;
    std::unique_ptr<APVTS::ButtonAttachment> followAttach;
    SampleZone zone;
    TunerDisplay tuner;
    PresetScreen presets;
    IconButton diceBtn, mutateBtn, saveBtn;
    std::unique_ptr<juce::Button> toonBtn;
    std::vector<Block> blocks;
    std::vector<std::pair<juce::Rectangle<float>, juce::String>> captions;
    juce::Image background;
    int fxRight = 0;
};

// ====================================================================== editor
VocalTuneEditor::VocalTuneEditor (VocalTuneProcessor& p) : AudioProcessorEditor (p), proc (p)
{
    const float s = juce::jlimit (0.7f, 1.8f, proc.editorScale);   // read before resizing callbacks overwrite it
    setLookAndFeel (&lnf);
    content = std::make_unique<VocalTuneContent> (p);
    content->setLookAndFeel (&lnf);
    addChildComponent (*content);
    toon = createAutoToonPage (p, [this] { showPage (0); });
    addChildComponent (*toon);
    content->onAutoToon = [this] { showPage (1); };
    showPage (proc.page);
    setResizable (true, true);
    setResizeLimits ((int) (baseW * 0.7), (int) (baseH * 0.7), (int) (baseW * 1.8), (int) (baseH * 1.8));
    getConstrainer()->setFixedAspectRatio ((double) baseW / baseH);
    setSize (juce::roundToInt (baseW * s), juce::roundToInt (baseH * s));
}

VocalTuneEditor::~VocalTuneEditor()
{
    content->setLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

void VocalTuneEditor::showPage (int page)
{
    proc.page = juce::jlimit (0, 1, page);
    content->setVisible (proc.page == 0);
    toon->setVisible (proc.page == 1);
}

void VocalTuneEditor::paint (juce::Graphics& g) { g.fillAll (toy::brown); }

void VocalTuneEditor::resized()
{
    const float s = (float) getWidth() / (float) baseW;
    for (auto* c : { (juce::Component*) content.get(), toon.get() })
    {
        c->setBounds (0, 0, baseW, baseH);
        c->setTransform (juce::AffineTransform::scale (s));
    }
    proc.editorScale = s;
}
