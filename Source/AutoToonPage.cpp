#include "AutoToonPage.h"
#include "Knob.h"
#include "PresetScreen.h"

using namespace vt;

namespace ink
{
const juce::Colour black (0xff1A1410), soft (0xff4A3B2C), paper (0xffF3E8CB), paperLight (0xffFBF4E2),
                   paperDark (0xffDCC89D), wood1 (0xffD9A066), wood2 (0xffA86C3A), woodDark (0xff6E4220),
                   red (0xffB8382E), gold (0xffFFD86B), curtain (0xff6B2A22);

// smooth "hand drawn" noise, different for each seed (the boil frame)
inline float noise (float t, int seed)
{
    const float s = (float) seed * 12.9898f;
    return 0.55f * std::sin (t * 0.11f + s) + 0.3f * std::sin (t * 0.27f + s * 1.7f) + 0.15f * std::sin (t * 0.61f + s * 2.3f);
}

// redraw any path as if inked by hand: resample it and nudge every point a little
inline juce::Path wobble (const juce::Path& src, int seed, float amount = 1.6f, float step = 9.0f)
{
    juce::Path out;
    juce::PathFlatteningIterator it (src, {}, 0.4f);
    std::vector<juce::Point<float>> pts;
    float travelled = 0.0f;
    int sub = -1;
    bool closed = false;
    auto flush = [&]
    {
        if (pts.size() < 2) { pts.clear(); return; }
        out.startNewSubPath (pts[0]);
        for (size_t i = 1; i + 1 < pts.size(); ++i)
            out.quadraticTo (pts[i], (pts[i] + pts[i + 1]) * 0.5f);
        out.lineTo (pts.back());
        if (closed) out.closeSubPath();
        pts.clear();
    };
    auto add = [&] (float x, float y)
    {
        pts.push_back ({ x + amount * noise (travelled + x * 0.3f, seed), y + amount * noise (travelled + y * 0.3f + 50.0f, seed + 7) });
    };
    juce::Point<float> last { 1.0e9f, 1.0e9f };
    while (it.next())
    {
        // a new piece starts only where the outline actually jumps
        if (sub < 0 || last.getDistanceFrom ({ it.x1, it.y1 }) > 0.01f) { flush(); sub = 1; closed = false; add (it.x1, it.y1); }
        last = { it.x2, it.y2 };
        const float len = juce::Point<float> (it.x1, it.y1).getDistanceFrom ({ it.x2, it.y2 });
        const int n = std::max (1, (int) (len / step));
        for (int i = 1; i <= n; ++i)
        {
            const float f = (float) i / (float) n;
            travelled += len / (float) n;
            add (it.x1 + (it.x2 - it.x1) * f, it.y1 + (it.y2 - it.y1) * f);
        }
        if (it.closesSubPath) closed = true;
    }
    flush();
    return out;
}

inline juce::Path roundRect (juce::Rectangle<float> r, float radius)
{
    juce::Path p; p.addRoundedRectangle (r, radius); return p;
}

inline void stroke (juce::Graphics& g, const juce::Path& p, float w, juce::Colour c = black)
{
    g.setColour (c);
    g.strokePath (p, juce::PathStrokeType (w, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

// a cartoon panel: solid ink drop shadow, paper face, wobbly ink outline
inline void panel (juce::Graphics& g, juce::Rectangle<float> r, int seed, juce::Colour face = paperLight, float radius = 18.0f)
{
    auto shape = wobble (roundRect (r, radius), seed, 1.4f);
    g.setColour (black);
    g.fillPath (shape, juce::AffineTransform::translation (5.0f, 6.0f));
    g.setColour (face);
    g.fillPath (shape);
    stroke (g, shape, 3.2f);
}

// a little ink banner with a panel's name on it
inline void tag (juce::Graphics& g, juce::Point<float> at, const juce::String& text, int seed, float angle = -0.03f)
{
    g.setFont (toy::titleFont (17.0f));
    const float w = juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), text) + 24.0f;
    auto r = juce::Rectangle<float> (at.x, at.y, w, 26.0f);
    g.saveState();
    g.addTransform (juce::AffineTransform::rotation (angle, r.getCentreX(), r.getCentreY()));
    auto shape = wobble (roundRect (r, 7.0f), seed, 1.0f, 7.0f);
    g.setColour (black);
    g.fillPath (shape);
    g.setColour (paperLight);
    g.drawText (text, r.translated (0, 2.0f), juce::Justification::centred, false);
    g.restoreState();
}

inline void hatch (juce::Graphics& g, juce::Rectangle<float> r, float spacing, float alpha, float width = 1.0f)
{
    g.saveState();
    g.reduceClipRegion (r.toNearestInt());
    g.setColour (black.withAlpha (alpha));
    for (float x = r.getX() - r.getHeight(); x < r.getRight(); x += spacing)
        g.drawLine (x, r.getBottom(), x + r.getHeight(), r.getY(), width);
    g.restoreState();
}

inline juce::Path musicNote (juce::Point<float> c, float s)
{
    juce::Path p;
    p.addEllipse (c.x - s * 0.5f, c.y - s * 0.32f, s * 0.75f, s * 0.6f);
    p.addRectangle (c.x + s * 0.13f, c.y - s * 1.45f, s * 0.12f, s * 1.3f);
    p.startNewSubPath (c.x + s * 0.25f, c.y - s * 1.45f);
    p.quadraticTo (c.x + s * 0.75f, c.y - s * 1.2f, c.x + s * 0.65f, c.y - s * 0.75f);
    p.quadraticTo (c.x + s * 0.55f, c.y - s * 1.05f, c.x + s * 0.25f, c.y - s * 1.15f);
    p.closeSubPath();
    return p;
}

// visible all the way up (unlike isShowing(), this also works for snapshots)
inline bool onScreen (const juce::Component& c)
{
    for (auto* p = &c; p != nullptr; p = p->getParentComponent())
        if (! p->isVisible()) return false;
    return true;
}

class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    LookAndFeel()
    {
        setDefaultSansSerifTypeface (toy::Fonts::get().body);
        setColour (juce::PopupMenu::backgroundColourId, paperLight);
        setColour (juce::PopupMenu::textColourId, black);
        setColour (juce::PopupMenu::headerTextColourId, soft);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, black);
        setColour (juce::PopupMenu::highlightedTextColourId, paperLight);
        setColour (juce::ComboBox::textColourId, black);
        setColour (juce::ComboBox::arrowColourId, black);
        setColour (juce::AlertWindow::backgroundColourId, paperLight);
        setColour (juce::AlertWindow::textColourId, black);
        setColour (juce::AlertWindow::outlineColourId, black);
        setColour (juce::TextEditor::backgroundColourId, juce::Colours::white);
        setColour (juce::TextEditor::textColourId, black);
        setColour (juce::TextEditor::outlineColourId, black);
        setColour (juce::TextEditor::focusedOutlineColourId, black);
        setColour (juce::TextButton::buttonColourId, black);
        setColour (juce::TooltipWindow::backgroundColourId, black);
        setColour (juce::TooltipWindow::textColourId, paperLight);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float start, float end, juce::Slider& s) override
    {
        const auto b = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (2.0f);
        const float r = std::min (b.getWidth(), b.getHeight()) * 0.5f;
        const auto c = b.getCentre();
        const float angle = start + pos * (end - start);
        const bool bipolar = (bool) s.getProperties().getWithDefault ("bipolar", false);
        const float from = bipolar ? (start + end) * 0.5f : start;

        // tick marks; the ones the value has passed are inked in
        for (int i = 0; i <= 12; ++i)
        {
            const float a = start + (end - start) * (float) i / 12.0f;
            const bool on = (a >= std::min (from, angle) - 0.01f && a <= std::max (from, angle) + 0.01f) && std::abs (angle - from) > 0.02f;
            g.setColour (on ? black : black.withAlpha (0.28f));
            const float r1 = r - 1.0f, r2 = r - (on ? 8.0f : 6.0f);
            g.drawLine (c.x + std::sin (a) * r1, c.y - std::cos (a) * r1, c.x + std::sin (a) * r2, c.y - std::cos (a) * r2, on ? 2.6f : 1.6f);
        }

        const float kr = r - 11.0f;
        auto cap = juce::Rectangle<float> (kr * 2, kr * 2).withCentre (c);
        g.setColour (black);
        g.fillEllipse (cap.translated (2.5f, 3.0f));
        g.setColour (paperLight);
        g.fillEllipse (cap);
        // a little hand shading on the lower right
        juce::Path shade;
        shade.addCentredArc (c.x, c.y, kr - 4.0f, kr - 4.0f, 0, 2.0f, 3.6f, true);
        g.setColour (paperDark);
        g.strokePath (shade, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (black);
        g.drawEllipse (cap, 3.0f);

        // wedge pointer
        juce::Path wedge;
        const float tip = kr - 4.0f, base = kr * 0.2f, half = 4.0f;
        wedge.startNewSubPath (c.x + std::sin (angle) * tip, c.y - std::cos (angle) * tip);
        wedge.lineTo (c.x + std::sin (angle) * base + std::cos (angle) * half, c.y - std::cos (angle) * base + std::sin (angle) * half);
        wedge.lineTo (c.x + std::sin (angle) * base - std::cos (angle) * half, c.y - std::cos (angle) * base - std::sin (angle) * half);
        wedge.closeSubPath();
        g.fillPath (wedge);
        g.fillEllipse (juce::Rectangle<float> (8, 8).withCentre (c));
    }

    void drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box) override
    {
        auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (2.0f);
        g.setColour (black);
        g.fillRoundedRectangle (r.translated (2, 2.5f), r.getHeight() * 0.5f);
        g.setColour (box.isMouseOver (true) ? juce::Colours::white : paperLight);
        g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
        g.setColour (black);
        g.drawRoundedRectangle (r, r.getHeight() * 0.5f, 2.5f);
        juce::Path tri;
        const float cx = r.getRight() - 13.0f, cy = r.getCentreY();
        tri.addTriangle (cx - 5, cy - 3, cx + 5, cy - 3, cx, cy + 4);
        g.fillPath (tri);
    }
    juce::Font getComboBoxFont (juce::ComboBox&) override { return toy::bodyFont (14.5f); }
    void positionComboBoxText (juce::ComboBox& box, juce::Label& l) override
    {
        l.setBounds (8, 0, box.getWidth() - 26, box.getHeight());
        l.setFont (getComboBoxFont (box));
        l.setJustificationType (juce::Justification::centred);
    }
    juce::Font getPopupMenuFont() override { return toy::bodyFont (16.0f); }
    void drawPopupMenuBackground (juce::Graphics& g, int w, int h) override
    {
        g.fillAll (paperLight);
        g.setColour (black);
        g.drawRect (0, 0, w, h, 2);
    }
    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (2.0f);
        g.setColour (black);
        g.fillRoundedRectangle (r.translated (2, 2.5f), 8.0f);
        g.setColour (over ? juce::Colours::white : paperLight);
        g.fillRoundedRectangle (down ? r.translated (1, 1) : r, 8.0f);
        g.setColour (black);
        g.drawRoundedRectangle (down ? r.translated (1, 1) : r, 8.0f, 2.5f);
    }
    void drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool) override
    {
        g.setFont (toy::titleFont (16.0f));
        g.setColour (black);
        g.drawText (b.getButtonText(), b.getLocalBounds().translated (0, 2), juce::Justification::centred, false);
    }
    juce::Font getAlertWindowTitleFont() override { return toy::titleFont (24.0f); }
    juce::Font getAlertWindowMessageFont() override { return toy::bodyFont (16.0f); }
};
} // namespace ink

// ====================================================================== the scrolling pitch graph
class PitchGraph : public juce::Component, private juce::Timer
{
public:
    explicit PitchGraph (VocalTuneProcessor& p) : proc (p) { startTimerHz (30); }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (ink::paperLight);
        g.fillRect (r);
        auto plot = r.withTrimmedLeft (44.0f).reduced (0, 6.0f);
        const float rows = 15.0f, rowH = plot.getHeight() / rows;
        auto yOf = [&] (float midi) { return plot.getCentreY() - (midi - centre) * rowH; };

        // note lanes: target notes are solid with a name, the rest are faint
        const int lo = (int) std::floor (centre - rows * 0.5f), hi = (int) std::ceil (centre + rows * 0.5f);
        for (int m = lo; m <= hi; ++m)
        {
            const float y = yOf ((float) m);
            if (y < plot.getY() - 2 || y > plot.getBottom() + 2) continue;
            const int pc = ((m % 12) + 12) % 12;
            if ((mask >> pc) & 1)
            {
                g.setColour (ink::black.withAlpha (0.08f));
                g.fillRect (plot.getX(), y - rowH * 0.5f, plot.getWidth(), rowH);
                g.setColour (ink::black.withAlpha (0.4f));
                g.drawLine (plot.getX(), y, plot.getRight(), y, 1.3f);
                g.setColour (ink::black);
                g.setFont (toy::titleFont (14.0f));
                g.drawText (noteName ((float) m), juce::Rectangle<float> (2, y - 9, 40, 18).translated (0, 1.5f), juce::Justification::centredRight, false);
            }
            else
            {
                juce::Path line; line.startNewSubPath (plot.getX(), y); line.lineTo (plot.getRight(), y);
                juce::Path dashed; const float d[] { 3.0f, 6.0f };
                juce::PathStrokeType (1.0f).createDashedStroke (dashed, line, d, 2);
                g.setColour (ink::black.withAlpha (0.18f));
                g.fillPath (dashed);
            }
        }
        g.setColour (ink::black);
        g.drawLine (plot.getX(), r.getY(), plot.getX(), r.getBottom(), 2.0f);

        // the lines: what you sang (thin, dashed) and what comes out (thick ink)
        const int count = 420;
        const float dx = plot.getWidth() / (float) count;
        auto build = [&] (const std::array<float, SpectralEngine::historySize>& h)
        {
            juce::Path p; bool pen = false; float prev = -1.0f;
            for (int i = 0; i < count; ++i)
            {
                const float v = h[(size_t) ((latest - i + SpectralEngine::historySize) % SpectralEngine::historySize)];
                const float x = plot.getRight() - (float) i * dx;
                if (v < 0.0f) { pen = false; prev = -1.0f; continue; }
                const float y = juce::jlimit (plot.getY(), plot.getBottom(), yOf (v));
                if (pen && std::abs (v - prev) > 2.5f) pen = false;   // a new phrase, not a slide
                if (! pen) { p.startNewSubPath (x, y); pen = true; } else p.lineTo (x, y);
                prev = v;
            }
            return p;
        };
        auto inPath = build (inHist), outPath = build (outHist);
        ink::stroke (g, outPath, 7.0f, ink::paperLight);
        ink::stroke (g, outPath, 3.4f);
        juce::Path dashed; const float d[] { 5.0f, 4.0f };
        juce::PathStrokeType (1.8f).createDashedStroke (dashed, inPath, d, 2);
        g.setColour (ink::red.withAlpha (0.85f));
        g.fillPath (dashed);

        // the ball riding the newest note
        const float now = outHist[(size_t) latest];
        if (now > 0.0f)
        {
            const auto c = juce::Point<float> (plot.getRight() - 8.0f, juce::jlimit (plot.getY(), plot.getBottom(), yOf (now)));
            g.setColour (ink::black);
            g.fillEllipse (juce::Rectangle<float> (16, 16).withCentre (c));
            g.setColour (ink::paperLight);
            g.fillEllipse (juce::Rectangle<float> (5, 5).withCentre (c.translated (-3, -3)));
        }
        auto legend = r.reduced (54, 6).removeFromBottom (14.0f);
        g.setFont (toy::bodyFont (12.0f));
        g.setColour (ink::red);
        g.drawText ("- - - what you sang", legend, juce::Justification::bottomLeft, false);
        g.setColour (ink::black);
        g.drawText ("------ what comes out", legend.withTrimmedLeft (130.0f), juce::Justification::bottomLeft, false);
    }

private:
    void timerCallback() override
    {
        if (! ink::onScreen (*this)) return;
        auto& e = proc.engine;
        latest = (e.historyWrite.load (std::memory_order_acquire) - 1 + SpectralEngine::historySize) % SpectralEngine::historySize;
        float sum = 0; int n = 0;
        for (int i = 0; i < SpectralEngine::historySize; ++i)
        {
            inHist[(size_t) i] = e.historyIn[(size_t) i].load (std::memory_order_relaxed);
            outHist[(size_t) i] = e.historyOut[(size_t) i].load (std::memory_order_relaxed);
        }
        for (int i = 0; i < 150; ++i)
        {
            const float v = outHist[(size_t) ((latest - i + SpectralEngine::historySize) % SpectralEngine::historySize)];
            if (v > 0) { sum += v; ++n; }
        }
        if (n > 10) centre += (sum / (float) n - centre) * 0.08f;
        mask = proc.absoluteNoteMask();
        repaint();
    }

    VocalTuneProcessor& proc;
    std::array<float, SpectralEngine::historySize> inHist {}, outHist {};
    int latest = 0, mask = 0xFFF;
    float centre = 62.0f;
};

// ====================================================================== one-octave piano for picking target notes
class NotePiano : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    explicit NotePiano (VocalTuneProcessor& p) : proc (p)
    {
        setTooltip ("Click keys to choose which notes Auto Toon snaps to");
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
        startTimerHz (15);
    }

    void paint (juce::Graphics& g) override
    {
        const int white[] { 0, 2, 4, 5, 7, 9, 11 };
        const auto r = getLocalBounds().toFloat().reduced (2.0f, 2.0f).withTrimmedBottom (2.0f);
        const float ww = r.getWidth() / 7.0f;
        for (int i = 0; i < 7; ++i)
        {
            auto k = whiteRect (i);
            const int pc = white[i];
            const bool on = (mask >> pc) & 1;
            g.setColour (pc == current ? ink::gold : ink::paperLight);
            g.fillRoundedRectangle (k, 5.0f);
            if (! on) ink::hatch (g, k.reduced (2), 6.0f, 0.3f);
            g.setColour (ink::black);
            g.drawRoundedRectangle (k, 5.0f, 2.5f);
            g.setFont (toy::titleFont (13.0f));
            g.drawText (keyNames()[pc], k.removeFromBottom (18.0f).translated (0, 1.5f), juce::Justification::centred, false);
            if (on) g.fillEllipse (juce::Rectangle<float> (9, 9).withCentre ({ k.getCentreX(), k.getBottom() - 8 }));
        }
        juce::ignoreUnused (ww);
        for (int pc : { 1, 3, 6, 8, 10 })
        {
            auto k = blackRect (pc);
            const bool on = (mask >> pc) & 1;
            g.setColour (ink::black);
            g.fillRoundedRectangle (k.translated (2, 2), 4.0f);
            g.setColour (pc == current ? ink::gold : (on ? ink::soft : ink::black));
            g.fillRoundedRectangle (k, 4.0f);
            g.setColour (ink::black);
            g.drawRoundedRectangle (k, 4.0f, 2.0f);
            if (on)
            {
                g.setColour (ink::paperLight);
                g.fillEllipse (juce::Rectangle<float> (8, 8).withCentre ({ k.getCentreX(), k.getBottom() - 9 }));
            }
        }
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        const auto pt = e.position;
        for (int pc : { 1, 3, 6, 8, 10 }) if (blackRect (pc).contains (pt)) { proc.toggleNote (pc); return; }
        const int white[] { 0, 2, 4, 5, 7, 9, 11 };
        for (int i = 0; i < 7; ++i) if (whiteRect (i).contains (pt)) { proc.toggleNote (white[i]); return; }
    }

private:
    juce::Rectangle<float> whiteRect (int i) const
    {
        const auto r = getLocalBounds().toFloat().reduced (2.0f).withTrimmedBottom (2.0f);
        const float ww = r.getWidth() / 7.0f;
        return { r.getX() + (float) i * ww + 1.0f, r.getY(), ww - 2.0f, r.getHeight() };
    }
    juce::Rectangle<float> blackRect (int pc) const
    {
        const auto r = getLocalBounds().toFloat().reduced (2.0f);
        const float ww = r.getWidth() / 7.0f;
        const float pos = pc == 1 ? 1.0f : pc == 3 ? 2.0f : pc == 6 ? 4.0f : pc == 8 ? 5.0f : 6.0f;
        return { r.getX() + pos * ww - ww * 0.3f, r.getY(), ww * 0.6f, r.getHeight() * 0.58f };
    }
    void timerCallback() override
    {
        if (! ink::onScreen (*this)) return;
        const int m = proc.absoluteNoteMask();
        const float t = proc.engine.targetMidi.load();
        const int c = t > 0 ? ((juce::roundToInt (t) % 12) + 12) % 12 : -1;
        if (m != mask || c != current) { mask = m; current = c; repaint(); }
    }
    VocalTuneProcessor& proc;
    int mask = 0, current = -1;
};

// ====================================================================== the singer: an original rubber-hose microphone
class Stage : public juce::Component, private juce::Timer
{
public:
    explicit Stage (VocalTuneProcessor& p) : proc (p) { setInterceptsMouseClicks (false, false); startTimerHz (30); }

    void paint (juce::Graphics& g) override
    {
        const auto r = getLocalBounds().toFloat();
        const float floorY = r.getBottom() - 30.0f;
        const float cx = r.getCentreX() + 44.0f;

        // floating music notes
        for (auto& n : notes)
        {
            g.setColour (ink::black.withAlpha (juce::jlimit (0.0f, 1.0f, 1.4f - n.age)));
            g.fillPath (ink::musicNote ({ n.x + 10.0f * std::sin (n.age * 5.0f + n.seed), n.y }, 15.0f * n.size));
        }

        g.saveState();
        const float sway = singing ? 0.09f * std::sin (t * 7.0f) : 0.025f * std::sin (t * 1.6f);
        const float bounce = 1.0f + level * 0.07f + (singing ? 0.02f * std::sin (t * 14.0f) : 0.0f);
        g.addTransform (juce::AffineTransform::scale (1.0f / bounce, bounce, cx, floorY).rotated (sway, cx, floorY));

        // stand
        g.setColour (ink::black);
        g.fillEllipse (cx - 34, floorY - 7, 68, 14);
        g.fillRect (cx - 3.0f, floorY - 70.0f, 6.0f, 66.0f);

        // body (capsule)
        auto body = juce::Rectangle<float> (cx - 40.0f, floorY - 196.0f, 80.0f, 128.0f);
        // arms: rubber hoses that go up when singing
        const float lift = singing ? 1.0f : 0.0f;
        for (int side = -1; side <= 1; side += 2)
        {
            const float sx = cx + (float) side * 38.0f, sy = body.getY() + 82.0f;
            const float wave = std::sin (t * 6.0f + (float) side) * 8.0f * lift;
            const juce::Point<float> hand (cx + (float) side * (66.0f + 6.0f * lift), sy - 24.0f - 34.0f * lift + wave);
            juce::Path arm;
            arm.startNewSubPath (sx, sy);
            arm.cubicTo (sx + (float) side * 26.0f, sy + 10.0f, hand.x - (float) side * 4.0f, hand.y + 30.0f, hand.x, hand.y);
            ink::stroke (g, arm, 6.0f);
            auto hr = juce::Rectangle<float> (22, 22).withCentre (hand);
            g.setColour (juce::Colours::white);
            g.fillEllipse (hr);
            g.setColour (ink::black);
            g.drawEllipse (hr, 2.5f);
            g.drawLine (hr.getCentreX() - 3, hr.getY() + 6, hr.getCentreX() - 3, hr.getY() + 12, 1.6f);
            g.drawLine (hr.getCentreX() + 3, hr.getY() + 6, hr.getCentreX() + 3, hr.getY() + 12, 1.6f);
        }
        g.setColour (ink::black);
        g.fillRoundedRectangle (body.translated (3, 4), 40.0f);
        g.setColour (ink::paperLight);
        g.fillRoundedRectangle (body, 40.0f);
        // grille on the top
        g.saveState();
        juce::Path cap; cap.addRoundedRectangle (body, 40.0f);
        g.reduceClipRegion (cap);
        g.setColour (ink::black.withAlpha (0.55f));
        for (float y = body.getY() + 8.0f; y < body.getY() + 34.0f; y += 6.0f) g.drawLine (body.getX(), y, body.getRight(), y, 1.6f);
        g.setColour (ink::black);
        g.fillRect (body.getX(), body.getY() + 38.0f, body.getWidth(), 8.0f);
        g.setColour (ink::paperDark);
        g.fillRect (body.getRight() - 16.0f, body.getY() + 46.0f, 16.0f, 80.0f);
        g.restoreState();
        g.setColour (ink::black);
        g.drawRoundedRectangle (body, 40.0f, 3.5f);

        // pie-cut eyes, looking up at the speech bubble
        const float blink = blinkTimer < 0.12f ? 0.12f : 1.0f;
        for (int side = -1; side <= 1; side += 2)
        {
            auto e = juce::Rectangle<float> (15.0f, 26.0f * blink).withCentre ({ cx + (float) side * 14.0f - 3.0f, body.getY() + 66.0f });
            g.setColour (ink::black);
            g.fillEllipse (e);
            if (blink > 0.5f)
            {
                juce::Path pie;
                const auto c = e.getCentre().translated (-1.0f, -3.0f);
                pie.startNewSubPath (c);
                pie.addCentredArc (c.x, c.y, 6.0f, 9.0f, 0.0f, -2.2f, -1.2f);
                pie.closeSubPath();
                g.setColour (ink::paperLight);
                g.fillPath (pie);
            }
        }
        // mouth
        const auto mc = juce::Point<float> (cx - 3.0f, body.getY() + 96.0f);
        if (level > 0.05f)
        {
            auto m = juce::Rectangle<float> (26.0f + level * 6.0f, 6.0f + level * 20.0f).withCentre (mc);
            g.setColour (ink::black);
            g.fillEllipse (m);
            g.setColour (ink::red);
            g.fillEllipse (m.withTrimmedTop (m.getHeight() * 0.55f).reduced (m.getWidth() * 0.2f, 0));
        }
        else
        {
            juce::Path smile;
            smile.addCentredArc (mc.x, mc.y - 6.0f, 13.0f, 9.0f, 0.0f, 2.0f, 4.28f, true);
            ink::stroke (g, smile, 3.0f);
        }
        g.restoreState();

        // speech bubble with the note being sung
        auto bub = juce::Rectangle<float> (10.0f, 34.0f, 124.0f, 70.0f);
        juce::Path tail;   // points down at the singer's face
        tail.startNewSubPath (bub.getRight() - 46.0f, bub.getBottom() - 8.0f);
        tail.quadraticTo (bub.getRight() - 10.0f, bub.getBottom() + 14.0f, cx - 50.0f, floorY - 128.0f);
        tail.quadraticTo (bub.getRight() - 24.0f, bub.getBottom() + 2.0f, bub.getRight() - 22.0f, bub.getBottom() - 16.0f);
        tail.closeSubPath();
        juce::Path oval; oval.addEllipse (bub);
        auto wTail = ink::wobble (tail, boil + 3, 0.8f, 6.0f), wOval = ink::wobble (oval, boil, 1.2f);
        ink::stroke (g, wTail, 3.0f);
        ink::stroke (g, wOval, 3.0f);
        g.setColour (juce::Colours::white);
        g.fillPath (wTail);
        g.fillPath (wOval);
        g.setColour (ink::black);
        g.setFont (toy::titleFont (30.0f));
        g.drawText (outMidi > 0 ? noteName (outMidi) : juce::String ("..."), bub.withTrimmedBottom (16.0f).translated (0, 6.0f), juce::Justification::centred, false);
        g.setFont (toy::bodyFont (12.0f));
        g.setColour (ink::soft);
        g.drawText (inMidi > 0 ? "you sang " + noteName (inMidi) + centsText() : juce::String ("sing to me!"),
                    bub.withTrimmedTop (44.0f), juce::Justification::centredTop, false);
    }

    void setBoil (int b) { boil = b; }

private:
    juce::String centsText() const
    {
        const float c = 100.0f * (inMidi - std::round (inMidi));
        const int ci = juce::roundToInt (c);
        return ci == 0 ? juce::String() : (ci > 0 ? " +" : " ") + juce::String (ci);
    }
    void timerCallback() override
    {
        if (! ink::onScreen (*this)) return;
        const float dt = 1.0f / 30.0f;
        t += dt;
        const float in = proc.engine.detectedMidi.load(), out = proc.engine.targetMidi.load();
        const float lv = juce::jlimit (0.0f, 1.0f, (juce::Decibels::gainToDecibels (proc.engine.inputLevel.load() * 1.41f, -60.0f) + 50.0f) / 40.0f);
        level = std::max (lv, level * 0.8f);
        singing = in > 0.0f;
        if (singing) { inMidi = in; outMidi = out; hold = 0.6f; }
        else if ((hold -= dt) <= 0.0f) { inMidi = outMidi = -1.0f; }
        blinkTimer -= dt;
        if (blinkTimer < -0.0f) blinkTimer = 2.5f + rng.nextFloat() * 3.0f;
        spawn -= dt;
        if (singing && spawn <= 0.0f)
        {
            notes.push_back ({ getWidth() * 0.5f + 40.0f + rng.nextFloat() * 30.0f, (float) getHeight() - 150.0f, 0.0f, rng.nextFloat() * 6.0f, 0.8f + rng.nextFloat() * 0.5f });
            spawn = 0.35f;
        }
        for (auto& n : notes) { n.age += dt; n.y -= 38.0f * dt; n.x += 14.0f * dt; }
        notes.erase (std::remove_if (notes.begin(), notes.end(), [] (const Note& n) { return n.age > 1.4f; }), notes.end());
        repaint();
    }

    struct Note { float x, y, age, seed, size; };
    VocalTuneProcessor& proc;
    std::vector<Note> notes;
    float t = 0, level = 0, inMidi = -1, outMidi = -1, hold = 0, blinkTimer = 3, spawn = 0;
    bool singing = false;
    int boil = 0;
    juce::Random rng;
};

// ====================================================================== old film: scratches, dust and flicker
class FilmOverlay : public juce::Component, private juce::Timer
{
public:
    FilmOverlay() { setInterceptsMouseClicks (false, false); startTimerHz (12); }
    void paint (juce::Graphics& g) override
    {
        juce::Random r (seed);
        g.setColour (ink::black.withAlpha (r.nextFloat() * 0.035f));
        g.fillAll();
        const int scratches = r.nextInt (3);
        for (int i = 0; i < scratches; ++i)
        {
            const float x = r.nextFloat() * (float) getWidth();
            const float y0 = r.nextFloat() * (float) getHeight() * 0.5f;
            g.setColour (ink::black.withAlpha (0.12f + r.nextFloat() * 0.15f));
            g.drawLine (x, y0, x + r.nextFloat() * 4.0f - 2.0f, y0 + 80.0f + r.nextFloat() * (float) getHeight() * 0.5f, 0.8f);
        }
        const int dust = 2 + r.nextInt (5);
        for (int i = 0; i < dust; ++i)
        {
            g.setColour (ink::black.withAlpha (0.25f + r.nextFloat() * 0.3f));
            const float s = 1.5f + r.nextFloat() * 3.0f;
            g.fillEllipse (r.nextFloat() * (float) getWidth(), r.nextFloat() * (float) getHeight(), s, s * (0.6f + r.nextFloat()));
        }
    }
private:
    void timerCallback() override { if (ink::onScreen (*this)) { seed = (int) juce::Random::getSystemRandom().nextInt(); repaint(); } }
    int seed = 1;
};

// ====================================================================== "TOY BOX" arrow sign (goes back)
class ArrowSign : public juce::Button
{
public:
    ArrowSign() : juce::Button ("TOY BOX") { setMouseCursor (juce::MouseCursor::PointingHandCursor); setTooltip ("Back to the toy box"); }
    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        auto r = getLocalBounds().toFloat().reduced (6.0f, 8.0f);
        g.saveState();
        g.addTransform (juce::AffineTransform::rotation (over ? 0.02f : 0.07f, r.getCentreX(), r.getCentreY()));
        if (down) g.addTransform (juce::AffineTransform::translation (1, 1));
        juce::Path arrow;
        arrow.startNewSubPath (r.getX(), r.getCentreY());
        arrow.lineTo (r.getX() + 22.0f, r.getY());
        arrow.lineTo (r.getRight(), r.getY() + 3.0f);
        arrow.lineTo (r.getRight() - 4.0f, r.getBottom());
        arrow.lineTo (r.getX() + 22.0f, r.getBottom() - 2.0f);
        arrow.closeSubPath();
        auto w = ink::wobble (arrow, 3, 1.0f);
        g.setColour (ink::black);
        g.fillPath (w, juce::AffineTransform::translation (3, 4));
        g.setGradientFill (juce::ColourGradient (ink::wood1, 0, r.getY(), ink::wood2, 0, r.getBottom(), false));
        g.fillPath (w);
        g.setColour (ink::woodDark.withAlpha (0.5f));
        for (int i = 1; i < 3; ++i) g.drawLine (r.getX() + 26.0f, r.getY() + r.getHeight() * (float) i / 3.0f, r.getRight() - 6.0f, r.getY() + r.getHeight() * (float) i / 3.0f + 1.5f, 1.0f);
        ink::stroke (g, w, 3.0f);
        g.setColour (ink::black);
        g.setFont (toy::signFont (17.0f));
        g.drawText ("TOY BOX", r.withTrimmedLeft (18.0f).translated (0, 1), juce::Justification::centred, false);
        g.restoreState();
    }
};

// ====================================================================== the page
class AutoToonContent : public juce::Component, private juce::Timer
{
public:
    static constexpr int W = 1040, H = 574;

    AutoToonContent (VocalTuneProcessor& p, std::function<void()> back)
        : proc (p), graph (p), piano (p), stage (p), presets (p, true)
    {
        setLookAndFeel (&lnf);
        auto& st = proc.state;
        auto knob = [&] (const char* pid, const char* label, bool bip = false) -> ToyKnob&
        {
            knobs.push_back (std::make_unique<ToyKnob> (st, pid, label, ink::black, bip, true));
            addAndMakeVisible (*knobs.back());
            byId[pid] = knobs.back().get();
            return *knobs.back();
        };
        knob (id::tune, "STRENGTH").slider.setTooltip ("How hard Auto Toon pulls you onto the notes (0 = off)");
        knob (id::tuneSpeed, "SPEED").slider.setTooltip ("Retune speed: 0 ms = the hard robotic snap, higher = smooth and natural");
        knob (id::humanize, "HUMANIZE").slider.setTooltip ("Long held notes get a gentler retune, so they sound human");
        knob (id::flex, "FLEX").slider.setTooltip ("Only correct notes you're close to; slides and scoops stay yours");
        knob (id::vibratoKeep, "VIBRATO").slider.setTooltip ("0% = dead straight notes, 100% = your vibrato, 200% = extra wobbly");
        knob (id::pitch, "PITCH", true);
        knob (id::formant, "THROAT", true);
        knob (id::mix, "MIX");
        knob (id::output, "OUT", true);

        keyBox.addItemList (keyNames(), 1);
        { juce::StringArray sc; for (auto& s : scales()) sc.add (s.name); scaleBox.addItemList (sc, 1); }
        keyAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (st, id::key, keyBox);
        scaleAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (st, id::scale, scaleBox);
        for (auto* c : { &keyBox, &scaleBox }) addAndMakeVisible (c);

        addAndMakeVisible (graph);
        addAndMakeVisible (piano);
        addAndMakeVisible (stage);
        addAndMakeVisible (presets);
        presets.onSaveRequested = [this] { askToSave(); };
        backBtn.onClick = std::move (back);
        addAndMakeVisible (backBtn);
        addAndMakeVisible (film);

        // layout
        presets.setBounds (430, 20, 330, 70);
        backBtn.setBounds (770, 24, 120, 62);
        place (id::mix, 904, 14, 58, 84);
        place (id::output, 962, 14, 58, 84);
        graph.setBounds (34, 160, 674, 232);
        stage.setBounds (748, 146, 260, 246);
        keyBox.setBounds (36, 448, 72, 28);
        scaleBox.setBounds (116, 448, 168, 28);
        piano.setBounds (34, 482, 384, 70);
        const char* row[] { id::tune, id::tuneSpeed, id::humanize, id::flex, id::vibratoKeep, id::pitch, id::formant };
        for (int i = 0; i < 7; ++i) place (row[i], 456 + i * 80, 436, 80, 116);
        film.setBounds (0, 0, W, H);
        startTimerHz (7);
    }
    ~AutoToonContent() override { setLookAndFeel (nullptr); }

    void paint (juce::Graphics& g) override
    {
        auto& img = frames[(size_t) boil];
        if (img.isNull()) img = render (boil);
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.drawImageTransformed (img, juce::AffineTransform::scale (0.5f));
    }

private:
    void place (const char* pid, int x, int y, int w, int h) { byId.at (pid)->setBounds (x, y, w, h); }

    void timerCallback() override
    {
        if (! ink::onScreen (*this)) return;
        boil = (boil + 1) % 3;
        stage.setBoil (boil);
        repaint();
    }

    juce::Image render (int seed)
    {
        juce::Image img (juce::Image::ARGB, W * 2, H * 2, true);
        juce::Graphics g (img);
        g.addTransform (juce::AffineTransform::scale (2.0f));
        const auto all = juce::Rectangle<float> (0, 0, (float) W, (float) H);

        // old paper
        g.setGradientFill (juce::ColourGradient (ink::paper, all.getCentreX(), all.getCentreY(), ink::paperDark, 0, 0, true));
        g.fillRect (all);
        juce::Random rnd (99);
        for (int i = 0; i < 2600; ++i)
        {
            g.setColour (ink::woodDark.withAlpha (0.03f + rnd.nextFloat() * 0.08f));
            const float s = 0.6f + rnd.nextFloat() * 1.6f;
            g.fillEllipse (rnd.nextFloat() * W, rnd.nextFloat() * H, s, s);
        }
        for (int i = 0; i < 90; ++i)
        {
            juce::Path fibre;
            const float x = rnd.nextFloat() * W, y = rnd.nextFloat() * H;
            fibre.startNewSubPath (x, y);
            fibre.quadraticTo (x + rnd.nextFloat() * 14 - 7, y + rnd.nextFloat() * 14 - 7, x + rnd.nextFloat() * 26 - 13, y + rnd.nextFloat() * 26 - 13);
            g.setColour (ink::woodDark.withAlpha (0.08f));
            g.strokePath (fibre, juce::PathStrokeType (0.6f));
        }

        // panels
        ink::panel (g, { 20, 136, 700, 268 }, seed + 1);
        ink::panel (g, { 736, 136, 284, 268 }, seed + 2, ink::paper);
        ink::panel (g, { 20, 420, 412, 142 }, seed + 3);
        ink::panel (g, { 446, 420, 574, 142 }, seed + 4);
        ink::panel (g, { 898, 10, 126, 92 }, seed + 5);

        // the stage: floorboards and curtains
        {
            auto st = juce::Rectangle<float> (742, 142, 272, 256);
            g.saveState();
            g.reduceClipRegion (ink::roundRect (st, 14.0f));
            auto floor = st.withTrimmedTop (st.getHeight() - 44.0f);
            g.setColour (ink::wood2);
            g.fillRect (floor);
            g.setColour (ink::black.withAlpha (0.6f));
            for (float x = floor.getX() - 40; x < floor.getRight() + 40; x += 34)
                g.drawLine (x, floor.getY(), x + (x - st.getCentreX()) * 0.35f, floor.getBottom(), 1.5f);
            g.drawLine (floor.getX(), floor.getY(), floor.getRight(), floor.getY(), 3.0f);
            for (int side = 0; side < 2; ++side)
            {
                juce::Path cur;
                const float x0 = side == 0 ? st.getX() : st.getRight();
                const float dir = side == 0 ? 1.0f : -1.0f;
                cur.startNewSubPath (x0, st.getY());
                cur.lineTo (x0 + dir * 70.0f, st.getY());
                cur.quadraticTo (x0 + dir * 30.0f, st.getY() + 90.0f, x0 + dir * 46.0f, st.getBottom() - 44.0f);
                cur.lineTo (x0, st.getBottom() - 44.0f);
                cur.closeSubPath();
                auto w = ink::wobble (cur, seed + 20 + side, 1.2f);
                g.setColour (ink::curtain);
                g.fillPath (w);
                g.setColour (ink::black.withAlpha (0.45f));
                for (int f = 1; f < 4; ++f)
                {
                    juce::Path fold;
                    fold.startNewSubPath (x0 + dir * 16.0f * (float) f, st.getY());
                    fold.quadraticTo (x0 + dir * 8.0f * (float) f, st.getY() + 100.0f, x0 + dir * 11.0f * (float) f, st.getBottom() - 46.0f);
                    g.strokePath (ink::wobble (fold, seed + f, 0.8f), juce::PathStrokeType (1.5f));
                }
                ink::stroke (g, w, 2.5f);
            }
            // valance
            juce::Path val;
            val.startNewSubPath (st.getX(), st.getY());
            val.lineTo (st.getRight(), st.getY());
            val.lineTo (st.getRight(), st.getY() + 18.0f);
            for (float x = st.getRight(); x > st.getX(); x -= 34.0f) val.quadraticTo (x - 17.0f, st.getY() + 34.0f, x - 34.0f, st.getY() + 18.0f);
            val.closeSubPath();
            auto vw = ink::wobble (val, seed + 30, 1.0f);
            g.setColour (ink::curtain.darker (0.2f));
            g.fillPath (vw);
            ink::stroke (g, vw, 2.5f);
            g.restoreState();
        }

        ink::tag (g, { 34, 123 }, "PITCH", seed + 6);
        ink::tag (g, { 750, 123 }, "THE SINGER", seed + 7, 0.03f);
        ink::tag (g, { 34, 407 }, "NOTES", seed + 8, 0.02f);
        ink::tag (g, { 460, 407 }, "TUNING", seed + 9);
        g.setFont (toy::bodyFont (12.5f));
        g.setColour (ink::soft);
        g.drawText ("click the keys to pick notes", juce::Rectangle<float> (292, 448, 132, 28), juce::Justification::centred, false);

        drawSign (g, seed);

        // vignette, like an old cartoon reel
        juce::ColourGradient vig (juce::Colours::transparentBlack, all.getCentreX(), all.getCentreY(), ink::black.withAlpha (0.38f), 0, 0, true);
        vig.addColour (0.6, juce::Colours::transparentBlack);
        g.setGradientFill (vig);
        g.fillRect (all);
        return img;
    }

    // the crooked, nailed-up wooden cut-out sign
    void drawSign (juce::Graphics& g, int seed)
    {
        const auto c = juce::Point<float> (210.0f, 66.0f);
        const float w = 372.0f, h = 96.0f;
        g.saveState();
        g.addTransform (juce::AffineTransform::rotation (-0.075f, c.x, c.y));

        // a rough cut board: wavy top and bottom, chopped ends
        juce::Random r (5);
        juce::Path board;
        const float x0 = c.x - w * 0.5f, y0 = c.y - h * 0.5f;
        board.startNewSubPath (x0 + 10.0f, y0 + 4.0f);
        for (float x = x0 + 40.0f; x < x0 + w - 20.0f; x += 32.0f) board.lineTo (x, y0 + r.nextFloat() * 6.0f - 2.0f);
        board.lineTo (x0 + w - 6.0f, y0 + 8.0f);
        board.lineTo (x0 + w + 4.0f, c.y - 6.0f);
        board.lineTo (x0 + w - 10.0f, c.y + 4.0f);   // a chopped notch
        board.lineTo (x0 + w - 2.0f, y0 + h - 6.0f);
        for (float x = x0 + w - 30.0f; x > x0 + 20.0f; x -= 30.0f) board.lineTo (x, y0 + h + r.nextFloat() * 6.0f - 3.0f);
        board.lineTo (x0 + 4.0f, y0 + h - 4.0f);
        board.lineTo (x0 - 6.0f, c.y + 10.0f);
        board.lineTo (x0 + 2.0f, c.y - 18.0f);
        board.closeSubPath();
        auto shape = ink::wobble (board, seed + 40, 1.3f, 8.0f);

        g.setColour (ink::black);
        g.fillPath (shape, juce::AffineTransform::translation (7.0f, 8.0f));
        g.setGradientFill (juce::ColourGradient (ink::wood1, 0, y0, ink::wood2, 0, y0 + h, false));
        g.fillPath (shape);
        g.saveState();
        g.reduceClipRegion (shape);
        // wood grain + two knots + a crack
        for (int i = 0; i < 9; ++i)
        {
            juce::Path grain;
            const float y = y0 + 8.0f + (float) i * 10.5f;
            grain.startNewSubPath (x0 - 10, y);
            for (float x = x0; x < x0 + w + 20; x += 40)
                grain.quadraticTo (x + 20.0f, y + std::sin ((x + (float) i * 37.0f) * 0.03f) * 4.0f, x + 40.0f, y + std::sin ((x + 40.0f + (float) i * 37.0f) * 0.02f) * 2.0f);
            g.setColour (ink::woodDark.withAlpha (0.35f));
            g.strokePath (ink::wobble (grain, seed + i, 0.6f, 12.0f), juce::PathStrokeType (1.1f));
        }
        for (auto k : { juce::Point<float> (x0 + 70.0f, y0 + 70.0f), juce::Point<float> (x0 + w - 80.0f, y0 + 22.0f) })
            for (int ring = 0; ring < 3; ++ring)
            {
                g.setColour (ink::woodDark.withAlpha (0.55f - (float) ring * 0.12f));
                g.drawEllipse (juce::Rectangle<float> (10.0f + (float) ring * 9.0f, 5.0f + (float) ring * 5.0f).withCentre (k), 1.3f);
            }
        juce::Path crack;
        crack.startNewSubPath (x0 + w - 2.0f, y0 + h - 30.0f);
        crack.lineTo (x0 + w - 26.0f, y0 + h - 24.0f);
        crack.lineTo (x0 + w - 40.0f, y0 + h - 30.0f);
        crack.lineTo (x0 + w - 58.0f, y0 + h - 26.0f);
        ink::stroke (g, crack, 1.6f);
        g.restoreState();
        ink::stroke (g, shape, 4.0f);

        // "AUTO TOON", painted on with bouncy letters
        g.setFont (toy::signFont (50.0f));
        juce::GlyphArrangement ga;
        ga.addLineOfText (g.getCurrentFont(), "AUTO TOON", 0, 0);
        const auto bounds = ga.getBoundingBox (0, -1, true);
        const float ox = c.x - bounds.getCentreX(), oy = c.y - bounds.getCentreY() + 2.0f;
        for (int i = 0; i < ga.getNumGlyphs(); ++i)
        {
            auto& gl = ga.getGlyph (i);
            if (gl.isWhitespace()) continue;
            juce::Path p;
            gl.createPath (p);
            const auto gb = p.getBounds();
            const float rot = (i % 2 == 0 ? -0.07f : 0.06f) * (i == 3 ? 1.6f : 1.0f);
            const float lift = (i % 3 == 0) ? -3.0f : (i % 3 == 1 ? 2.0f : 0.0f);
            p.applyTransform (juce::AffineTransform::rotation (rot, gb.getCentreX(), gb.getCentreY()).translated (ox, oy + lift));
            g.setColour (ink::black);
            g.fillPath (p, juce::AffineTransform::translation (3.0f, 3.5f));
            g.strokePath (p, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour (juce::Colour (0xffFFF3D6));
            g.fillPath (p);
        }

        // nails, with a little rust running down
        for (auto n : { juce::Point<float> (x0 + 22.0f, y0 + 18.0f), juce::Point<float> (x0 + w - 26.0f, y0 + 16.0f), juce::Point<float> (x0 + w * 0.5f + 6.0f, y0 + h - 12.0f) })
        {
            g.setColour (juce::Colour (0xff7A4A22).withAlpha (0.55f));
            g.fillRoundedRectangle (n.x - 1.5f, n.y + 3.0f, 3.0f, 12.0f, 1.5f);
            auto head = juce::Rectangle<float> (14, 14).withCentre (n);
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xffD8D2C4), head.getX(), head.getY(), juce::Colour (0xff6E6A62), head.getRight(), head.getBottom(), false));
            g.fillEllipse (head);
            g.setColour (ink::black);
            g.drawEllipse (head, 2.2f);
            g.setColour (juce::Colours::white.withAlpha (0.85f));
            g.fillEllipse (head.getX() + 3.0f, head.getY() + 3.0f, 4.0f, 3.0f);
        }
        g.restoreState();
    }

    void askToSave()
    {
        auto* w = new juce::AlertWindow ("Save your sound", "Give it a name:", juce::MessageBoxIconType::NoIcon, this);
        w->addTextEditor ("name", proc.getPresetName().trimCharactersAtEnd ("*"));
        w->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
        w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        w->enterModalState (true, juce::ModalCallbackFunction::create ([this, w] (int result)
        {
            if (result == 1) proc.saveUserPreset (w->getTextEditorContents ("name"));
        }), true);
    }

    VocalTuneProcessor& proc;
    ink::LookAndFeel lnf;
    std::vector<std::unique_ptr<ToyKnob>> knobs;
    std::map<juce::String, ToyKnob*> byId;
    juce::ComboBox keyBox, scaleBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> keyAttach, scaleAttach;
    PitchGraph graph;
    NotePiano piano;
    Stage stage;
    PresetScreen presets;
    ArrowSign backBtn;
    FilmOverlay film;
    std::array<juce::Image, 3> frames;
    int boil = 0;
};

std::unique_ptr<juce::Component> createAutoToonPage (VocalTuneProcessor& p, std::function<void()> onBack)
{
    return std::make_unique<AutoToonContent> (p, std::move (onBack));
}

// ====================================================================== the button on the toy box page
class AutoToonButton : public juce::Button
{
public:
    AutoToonButton() : juce::Button ("AUTO TOON")
    {
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
        setTooltip ("Flip over to Auto Toon: the full auto-tune");
    }
    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        auto r = getLocalBounds().toFloat().reduced (4.0f, 3.0f);
        g.saveState();
        g.addTransform (juce::AffineTransform::rotation (over ? -0.01f : -0.05f, r.getCentreX(), r.getCentreY()));
        if (down) g.addTransform (juce::AffineTransform::translation (1, 1));
        auto shape = ink::wobble (ink::roundRect (r, 6.0f), 2, 0.8f, 7.0f);
        g.setColour (ink::black);
        g.fillPath (shape, juce::AffineTransform::translation (2.5f, 3.0f));
        g.setColour (ink::paperLight);
        g.fillPath (shape);
        ink::stroke (g, shape, 2.5f);
        g.setColour (ink::black);
        g.setFont (toy::signFont (14.5f));
        g.drawText ("AUTO TOON", r.translated (0, 1.0f), juce::Justification::centred, false);
        g.restoreState();
    }
};

std::unique_ptr<juce::Button> createAutoToonButton() { return std::make_unique<AutoToonButton>(); }

