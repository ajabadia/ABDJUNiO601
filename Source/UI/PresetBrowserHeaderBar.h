#pragma once

#include <JuceHeader.h>
#include <functional>

/**
 * PresetBrowserHeaderBar — Standalone column header row.
 *
 * Owns its own:
 *  - Paint (header backgrounds, text labels, animated sort arrow)
 *  - Mouse handling (hover highlight, click-to-sort, pointing hand cursor)
 *  - Arrow animation (smooth rotation via internal Timer)
 *
 * Communicates sort changes upward via onSortClicked callback.
 * Exposes column widths via getters so PresetBrowser's list rows align perfectly.
 */
class PresetBrowserHeaderBar : public juce::Component,
                                public juce::SettableTooltipClient,
                                private juce::Timer
{
public:
    enum SortColumn { SortByName, SortByCategory, SortByLibrary, SortByFavorite };

    PresetBrowserHeaderBar();
    ~PresetBrowserHeaderBar() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseMove(const juce::MouseEvent& e) override;
    void mouseExit(const juce::MouseEvent& e) override;

    /** Update sort state (direction + column) from parent, triggers arrow animation. */
    void setSortState(SortColumn col, bool ascending);

    // ── Column width getters (for list row alignment) ──
    int getColFavW()     const { return kColFavW; }
    int getColCatW()     const { return headerCatW; }
    int getColLibW()     const { return headerLibW; }
    int getColNameW()    const { return headerNameW; }
    int getColLeftMarg() const { return kColLeftMarg; }
    int getColGap()      const { return kColGap; }

    /** Fired when user clicks a column header. */
    std::function<void(SortColumn)> onSortClicked;

private:
    void recalcWidths();
    void startArrowAnim();
    void timerCallback() override;

    // ── Column width constants (must match PresetBrowser's originals) ──
    static constexpr int kColFavW     = 24;
    static constexpr int kColCatWMin  = 80;
    static constexpr int kColLibWMin  = 70;
    static constexpr int kColGapAdj   = 8;
    static constexpr int kColLeftMarg = 4;
    static constexpr int kColGap      = 2;

    // Column proportion divisor (contentW / kColProportionDivisor = catW/libW)
    static constexpr int kColProportionDivisor = 5;

    // Visual style constants
    static constexpr float kHeaderFontSize    = 10.0f;
    static constexpr float kArrowSize         = 6.0f;
    static constexpr float kArrowOffsetX      = 4.0f;
    static constexpr float kAnimThreshold     = 0.001f;
    static constexpr float kAnimSpeed         = 0.30f;
    static constexpr float kActiveBgAlpha     = 0.30f;
    static constexpr float kActiveBgDefAlpha  = 0.15f;
    static constexpr float kHoverBgAlpha      = 0.10f;
    static constexpr float kDefaultBgAlpha    = 0.20f;
    static constexpr float kTextAlphaDim      = 0.60f;

    // Computed column widths
    int headerCatW  = kColCatWMin;
    int headerLibW  = kColLibWMin;
    int headerNameW = 0;

    // Sort state
    SortColumn activeSortCol = SortByName;
    bool       sortAscending = true;

    // Hover
    int hoveredHeaderCol = -1;

    // Arrow animation
    float arrowAnimAngle  = 0.0f;
    float arrowAnimTarget = 0.0f;

    // Clickable header rects (in this component's coordinate space)
    juce::Rectangle<int> rectFav;
    juce::Rectangle<int> rectName;
    juce::Rectangle<int> rectCategory;
    juce::Rectangle<int> rectLibrary;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PresetBrowserHeaderBar)
};
