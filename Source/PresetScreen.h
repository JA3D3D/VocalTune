#pragma once
#include "PluginProcessor.h"
#include "ToyLookAndFeel.h"

// ====================================================================== preset screen
class PresetScreen : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    explicit PresetScreen (VocalTuneProcessor& p, bool inkStyle = false) : proc (p), ink (inkStyle)
    {
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
        setTooltip ("Click for all presets. Arrows step through them.");
        startTimerHz (8);
    }

    void paint (juce::Graphics& g) override
    {
        if (ink) { paintInk (g); return; }
        auto r = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (juce::Colours::black.withAlpha (0.25f));
        g.fillRoundedRectangle (r.translated (0, 3), 14.0f);
        g.setColour (toy::brown);
        g.fillRoundedRectangle (r, 14.0f);
        auto screen = r.reduced (5.0f);
        g.setColour (toy::cream);
        g.fillRoundedRectangle (screen, 10.0f);

        for (int side = 0; side < 2; ++side)
        {
            auto a = arrowArea (side).toFloat();
            g.setColour (hover == side ? toy::orange : toy::orange.withAlpha (0.85f));
            g.fillRoundedRectangle (a, 8.0f);
            juce::Path t;
            const float cx = a.getCentreX(), cy = a.getCentreY();
            if (side == 0) t.addTriangle (cx + 5, cy - 7, cx + 5, cy + 7, cx - 6, cy);
            else           t.addTriangle (cx - 5, cy - 7, cx - 5, cy + 7, cx + 6, cy);
            g.setColour (juce::Colours::white);
            g.fillPath (t);
        }
        auto text = screen.reduced (44.0f, 2.0f);
        g.setColour (toy::brown.withAlpha (0.75f));
        g.setFont (toy::bodyFont (12.5f));
        g.drawText (category().toUpperCase(), text.removeFromTop (17.0f), juce::Justification::centredBottom, false);
        g.setColour (toy::ink);
        g.setFont (toy::titleFont (25.0f));
        g.drawFittedText (proc.getPresetName(), text.toNearestInt(), juce::Justification::centred, 1, 0.7f);
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        const int h = arrowArea (0).contains (e.getPosition()) ? 0 : arrowArea (1).contains (e.getPosition()) ? 1 : -1;
        if (h != hover) { hover = h; repaint(); }
    }
    void mouseExit (const juce::MouseEvent&) override { hover = -1; repaint(); }
    void mouseUp (const juce::MouseEvent& e) override
    {
        if (arrowArea (0).contains (e.getPosition())) { proc.stepPreset (-1); return; }
        if (arrowArea (1).contains (e.getPosition())) { proc.stepPreset (1); return; }
        showMenu();
    }

    std::function<void()> onSaveRequested;

private:
    juce::Rectangle<int> arrowArea (int side) const
    {
        return side == 0 ? juce::Rectangle<int> (10, 10, 32, getHeight() - 20)
                         : juce::Rectangle<int> (getWidth() - 42, 10, 32, getHeight() - 20);
    }
    juce::String category() const
    {
        const auto n = proc.getPresetName();
        for (auto& e : proc.presetList()) if (e.name == n) return e.category;
        return "Custom";
    }
    void showMenu()
    {
        const auto list = proc.presetList();
        juce::PopupMenu menu;
        std::map<juce::String, juce::PopupMenu> subs;
        juce::StringArray order;
        for (int i = 0; i < list.size(); ++i)
        {
            order.addIfNotAlreadyThere (list[i].category);
            subs[list[i].category].addItem (i + 1, list[i].name, true, list[i].name == proc.getPresetName());
        }
        for (auto& cat : order)
            menu.addSubMenu (cat == "Sample" ? juce::String ("Sample (drop a sound first)") : cat, subs[cat]);
        if (! order.contains ("User")) menu.addItem (-10, "User (save one to start)", false);
        menu.addSeparator();
        menu.addItem (-1, "Save preset...");
        menu.addItem (-2, "Show presets folder");
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMinimumWidth (getWidth()),
            [this, list] (int r)
            {
                if (r > 0 && r <= list.size()) proc.applyPreset (list[r - 1]);
                else if (r == -1 && onSaveRequested) onSaveRequested();
                else if (r == -2) { auto f = vt::userPresetFolder(); f.createDirectory(); f.startAsProcess(); }
            });
    }
    void timerCallback() override
    {
        if (proc.getPresetName() != shown) { shown = proc.getPresetName(); repaint(); }
    }

    void paintInk (juce::Graphics& g)
    {
        const juce::Colour inkC (0xff1A1410), paper (0xffF6ECD2);
        auto r = getLocalBounds().toFloat().reduced (4.0f);
        g.setColour (inkC);
        g.fillRoundedRectangle (r.translated (4, 5), 16.0f);
        g.setColour (paper);
        g.fillRoundedRectangle (r, 16.0f);
        g.setColour (inkC);
        g.drawRoundedRectangle (r, 16.0f, 3.5f);
        for (int side = 0; side < 2; ++side)
        {
            auto a = arrowArea (side).toFloat().reduced (2.0f);
            const float d = std::min (a.getWidth(), a.getHeight());
            auto c = a.withSizeKeepingCentre (d, d);
            g.setColour (hover == side ? inkC : paper);
            g.fillEllipse (c);
            g.setColour (inkC);
            g.drawEllipse (c, 2.5f);
            juce::Path t;
            const float cx = c.getCentreX(), cy = c.getCentreY();
            if (side == 0) t.addTriangle (cx + 4, cy - 6, cx + 4, cy + 6, cx - 6, cy);
            else           t.addTriangle (cx - 4, cy - 6, cx - 4, cy + 6, cx + 6, cy);
            g.setColour (hover == side ? paper : inkC);
            g.fillPath (t);
        }
        auto text = r.reduced (42.0f, 2.0f);
        g.setColour (inkC.withAlpha (0.7f));
        g.setFont (toy::bodyFont (12.0f));
        g.drawText (category().toUpperCase(), text.removeFromTop (17.0f), juce::Justification::centredBottom, false);
        g.setColour (inkC);
        g.setFont (toy::titleFont (24.0f));
        g.drawFittedText (proc.getPresetName(), text.toNearestInt(), juce::Justification::centred, 1, 0.7f);
    }

    VocalTuneProcessor& proc;
    bool ink = false;
    juce::String shown;
    int hover = -1;
};

