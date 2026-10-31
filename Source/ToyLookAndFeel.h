#pragma once
// The toy-chest skin: wooden case, rainbow toy blocks, chunky candy knobs.
#include <juce_gui_basics/juce_gui_basics.h>
#include "BinaryData.h"

namespace toy
{
inline juce::Colour c (juce::uint32 argb) { return juce::Colour (argb); }
inline const juce::Colour red    = c (0xffE63946), orange = c (0xffF77F00), yellow = c (0xffFFC300),
                          green  = c (0xff2BB673), teal   = c (0xff00A6A6), blue   = c (0xff3A86FF),
                          purple = c (0xff8338EC), pink   = c (0xffFF4F9A), navy   = c (0xff26408B),
                          wood1  = c (0xffE9B872), wood2  = c (0xffC4874A), brown  = c (0xff5A2F17),
                          cream  = c (0xffFFF3D6), ink    = c (0xff3B2414);

struct Fonts
{
    juce::Typeface::Ptr title, body;
    Fonts()
    {
        title = juce::Typeface::createSystemTypefaceFor (BinaryData::LuckiestGuyRegular_ttf, (size_t) BinaryData::LuckiestGuyRegular_ttfSize);
        body  = juce::Typeface::createSystemTypefaceFor (BinaryData::FredokaSemiBold_ttf, (size_t) BinaryData::FredokaSemiBold_ttfSize);
    }
    static Fonts& get() { static Fonts f; return f; }
};
inline juce::Font titleFont (float h) { return juce::Font (juce::FontOptions (Fonts::get().title).withHeight (h)); }
inline juce::Font bodyFont (float h)  { return juce::Font (juce::FontOptions (Fonts::get().body).withHeight (h)); }

// text with a little dark drop shadow, the way letters are printed on toys
inline void shadowText (juce::Graphics& g, const juce::String& t, juce::Rectangle<float> r, juce::Justification j,
                        juce::Colour col = juce::Colours::white, float shadow = 1.5f)
{
    g.setColour (juce::Colours::black.withAlpha (0.3f));
    g.drawText (t, r.translated (0, shadow), j, false);
    g.setColour (col);
    g.drawText (t, r, j, false);
}

// a chunky plastic block: dark "side" underneath, glossy face on top
inline void drawBlock (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour col, float radius = 14.0f, float depth = 5.0f)
{
    g.setColour (juce::Colours::black.withAlpha (0.18f));
    g.fillRoundedRectangle (r.translated (0, depth + 2), radius);
    g.setColour (col.darker (0.45f));
    g.fillRoundedRectangle (r.translated (0, depth), radius);
    juce::ColourGradient grad (col.brighter (0.18f), r.getX(), r.getY(), col.darker (0.06f), r.getX(), r.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (r, radius);
    g.setColour (juce::Colours::white.withAlpha (0.22f));
    g.drawRoundedRectangle (r.reduced (1.5f), radius - 1.5f, 1.5f);
    // gloss strip
    g.setColour (juce::Colours::white.withAlpha (0.13f));
    g.fillRoundedRectangle (r.reduced (6, 4).withHeight (std::min (18.0f, r.getHeight() * 0.25f)), radius * 0.6f);
}

class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    LookAndFeel()
    {
        setDefaultSansSerifTypeface (Fonts::get().body);
        setColour (juce::PopupMenu::backgroundColourId, cream);
        setColour (juce::PopupMenu::textColourId, ink);
        setColour (juce::PopupMenu::headerTextColourId, brown);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, blue);
        setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
        setColour (juce::ComboBox::textColourId, ink);
        setColour (juce::ComboBox::arrowColourId, ink);
        setColour (juce::AlertWindow::backgroundColourId, cream);
        setColour (juce::AlertWindow::textColourId, ink);
        setColour (juce::AlertWindow::outlineColourId, brown);
        setColour (juce::TextEditor::backgroundColourId, juce::Colours::white);
        setColour (juce::TextEditor::textColourId, ink);
        setColour (juce::TextEditor::outlineColourId, brown.withAlpha (0.5f));
        setColour (juce::TextEditor::focusedOutlineColourId, blue);
        setColour (juce::TextButton::buttonColourId, blue);
        setColour (juce::TextButton::textColourOffId, juce::Colours::white);
        setColour (juce::TooltipWindow::backgroundColourId, ink);
        setColour (juce::TooltipWindow::textColourId, cream);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                           float start, float end, juce::Slider& s) override
    {
        const auto accent = s.findColour (juce::Slider::rotarySliderFillColourId);
        const auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (3.0f);
        const float r = std::min (bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto ctr = bounds.getCentre();
        const float angle = start + pos * (end - start);
        const float track = std::max (3.5f, r * 0.13f);

        juce::Path bg;
        bg.addCentredArc (ctr.x, ctr.y, r - track * 0.5f, r - track * 0.5f, 0, start, end, true);
        g.setColour (juce::Colours::black.withAlpha (0.25f));
        g.strokePath (bg, juce::PathStrokeType (track, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        const bool bipolar = (bool) s.getProperties().getWithDefault ("bipolar", false);
        const float from = bipolar ? (start + end) * 0.5f : start;
        if (std::abs (angle - from) > 0.01f)
        {
            juce::Path v;
            v.addCentredArc (ctr.x, ctr.y, r - track * 0.5f, r - track * 0.5f, 0, std::min (from, angle), std::max (from, angle), true);
            g.setColour (juce::Colours::white);
            g.strokePath (v, juce::PathStrokeType (track, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // the knob cap
        const float kr = r - track * 1.9f;
        auto cap = juce::Rectangle<float> (kr * 2, kr * 2).withCentre (ctr);
        g.setColour (juce::Colours::black.withAlpha (0.25f));
        g.fillEllipse (cap.translated (0, kr * 0.12f + 1.5f));
        g.setColour (accent.darker (0.5f));
        g.fillEllipse (cap.translated (0, kr * 0.1f));
        juce::ColourGradient grad (accent.brighter (0.55f), cap.getX(), cap.getY(), accent.darker (0.1f), cap.getRight(), cap.getBottom(), false);
        g.setGradientFill (grad);
        g.fillEllipse (cap);
        // grip ridges
        g.setColour (accent.darker (0.3f).withAlpha (0.5f));
        for (int i = 0; i < 12; ++i)
        {
            const float a = juce::MathConstants<float>::twoPi * (float) i / 12.0f + angle;
            g.drawLine (ctr.x + std::sin (a) * kr * 0.82f, ctr.y - std::cos (a) * kr * 0.82f,
                        ctr.x + std::sin (a) * kr * 0.97f, ctr.y - std::cos (a) * kr * 0.97f, 1.4f);
        }
        g.setColour (juce::Colours::white.withAlpha (0.38f));
        g.fillEllipse (cap.reduced (kr * 0.38f).translated (-kr * 0.22f, -kr * 0.3f).withHeight (kr * 0.42f));
        // pointer dot
        const float pr = kr * 0.6f;
        const auto dot = juce::Point<float> (ctr.x + std::sin (angle) * pr, ctr.y - std::cos (angle) * pr);
        g.setColour (juce::Colours::white);
        g.fillEllipse (juce::Rectangle<float> (kr * 0.32f, kr * 0.32f).withCentre (dot));
        g.setColour (accent.darker (0.6f));
        g.drawEllipse (juce::Rectangle<float> (kr * 0.32f, kr * 0.32f).withCentre (dot), 1.0f);
    }

    void drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box) override
    {
        auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (1.0f);
        g.setColour (juce::Colours::black.withAlpha (0.2f));
        g.fillRoundedRectangle (r.translated (0, 2), r.getHeight() * 0.5f);
        g.setColour (box.isMouseOver (true) ? juce::Colours::white : cream);
        g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
        juce::Path tri;
        const float cx = r.getRight() - 13.0f, cy = r.getCentreY();
        tri.addTriangle (cx - 4.5f, cy - 2.5f, cx + 4.5f, cy - 2.5f, cx, cy + 3.5f);
        g.setColour (box.findColour (juce::ComboBox::arrowColourId));
        g.fillPath (tri);
    }
    juce::Font getComboBoxFont (juce::ComboBox&) override { return bodyFont (14.0f); }
    void positionComboBoxText (juce::ComboBox& box, juce::Label& l) override
    {
        l.setBounds (8, 0, box.getWidth() - 26, box.getHeight());
        l.setFont (getComboBoxFont (box));
        l.setJustificationType (juce::Justification::centred);
    }
    juce::Font getPopupMenuFont() override { return bodyFont (16.0f); }
    void drawPopupMenuBackground (juce::Graphics& g, int w, int h) override
    {
        g.fillAll (cream);
        g.setColour (brown.withAlpha (0.4f));
        g.drawRect (0, 0, w, h, 2);
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (1.0f);
        const auto accent = b.findColour (juce::TextButton::buttonColourId);
        const bool on = b.getToggleState();
        if (isSolid (b))
        {
            g.setColour (accent.darker (0.4f));
            g.fillRoundedRectangle (r.translated (0, 2), 8.0f);
            g.setColour (over ? accent.brighter (0.15f) : accent);
            g.fillRoundedRectangle (down ? r.translated (0, 1) : r, 8.0f);
            return;
        }
        g.setColour (juce::Colours::black.withAlpha (0.18f));
        g.fillRoundedRectangle (r.translated (0, 2), r.getHeight() * 0.5f);
        g.setColour (on ? cream : juce::Colours::white.withAlpha (over ? 0.32f : 0.2f));
        g.fillRoundedRectangle (down ? r.translated (0, 1) : r, r.getHeight() * 0.5f);
        if (on)
        {
            g.setColour (accent.darker (0.3f));
            g.drawRoundedRectangle (r.reduced (1), r.getHeight() * 0.5f, 2.0f);
        }
    }
    void drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool) override
    {
        const auto accent = b.findColour (juce::TextButton::buttonColourId);
        g.setFont (bodyFont (std::min (15.0f, b.getHeight() * 0.58f)));
        g.setColour (b.getToggleState() && ! isSolid (b) ? accent.darker (0.35f) : juce::Colours::white);
        g.drawText (b.getButtonText(), b.getLocalBounds(), juce::Justification::centred, false);
    }

    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool over, bool) override
    {
        // a little plastic slide switch
        auto r = b.getLocalBounds().toFloat();
        auto sw = r.removeFromLeft (40.0f).withSizeKeepingCentre (38.0f, 22.0f);
        const bool on = b.getToggleState();
        const auto accent = b.findColour (juce::ToggleButton::tickColourId);
        g.setColour (juce::Colours::black.withAlpha (0.28f));
        g.fillRoundedRectangle (sw, 11.0f);
        g.setColour (on ? cream : juce::Colours::white.withAlpha (over ? 0.35f : 0.22f));
        g.fillRoundedRectangle (sw.reduced (2.0f), 9.0f);
        auto knob = juce::Rectangle<float> (18.0f, 18.0f).withCentre ({ on ? sw.getRight() - 11.0f : sw.getX() + 11.0f, sw.getCentreY() });
        g.setColour (on ? accent.darker (0.2f) : juce::Colours::white);
        g.fillEllipse (knob);
        g.setFont (bodyFont (14.0f));
        shadowText (g, b.getButtonText(), r.withTrimmedLeft (4.0f), juce::Justification::centredLeft);
    }

    void drawCornerResizer (juce::Graphics& g, int w, int h, bool, bool) override
    {
        g.setColour (brown.withAlpha (0.6f));
        for (int i = 0; i < 3; ++i)
            g.drawLine ((float) w - 4.0f - (float) i * 5.0f, (float) h - 2.0f, (float) w - 2.0f, (float) h - 4.0f - (float) i * 5.0f, 2.0f);
    }

    static bool isSolid (juce::Button& b)
    {
        return dynamic_cast<juce::AlertWindow*> (b.getParentComponent()) != nullptr
            || (bool) b.getProperties().getWithDefault ("solid", false);
    }

    juce::Font getAlertWindowTitleFont() override { return titleFont (22.0f); }
    juce::Font getAlertWindowMessageFont() override { return bodyFont (16.0f); }
    juce::Font getTextButtonFont (juce::TextButton&, int h) override { return bodyFont (std::min (15.0f, h * 0.58f)); }
};
}
