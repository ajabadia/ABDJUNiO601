#include "PresetBrowserHeaderBar.h"

PresetBrowserHeaderBar::PresetBrowserHeaderBar()
{
    setWantsKeyboardFocus(true);
}

// ============================================================
// PAINT
// ============================================================
void PresetBrowserHeaderBar::paint(juce::Graphics& g)
{
    auto paintCell = [&](const juce::Rectangle<int>& rect, const juce::String& label,
                          SortColumn col)
    {
        bool isActive  = (activeSortCol == col);
        bool isHovered = (hoveredHeaderCol == (int)col);

        // Background fill
        if (isActive)
            g.setColour(juce::Colours::royalblue.withAlpha(isHovered ? kActiveBgAlpha : kActiveBgDefAlpha));
        else if (isHovered)
            g.setColour(juce::Colours::royalblue.withAlpha(kHoverBgAlpha));
        else
            g.setColour(juce::Colours::black.withAlpha(kDefaultBgAlpha));
        g.fillRect(rect);

        // Text
        g.setFont(kHeaderFontSize);
        if (isActive)
            g.setColour(juce::Colours::orange);
        else if (isHovered)
            g.setColour(juce::Colours::lightgrey);
        else
            g.setColour(juce::Colours::lightgrey.withAlpha(kTextAlphaDim));

        juce::String display = label;
        if (activeSortCol == col)
        {
            // Draw animated sort arrow (rotated triangle) left of label
            const float cx = (float)rect.getX() + kArrowOffsetX;
            const float cy = (float)rect.getCentreY();

            g.setColour(juce::Colours::orange);
            juce::Path arrowPath;
            arrowPath.addTriangle(0.0f, -kArrowSize * 0.5f,
                                  -kArrowSize * 0.5f, kArrowSize * 0.5f,
                                  kArrowSize * 0.5f, kArrowSize * 0.5f);
            arrowPath.applyTransform(juce::AffineTransform::rotation(arrowAnimAngle, 0.0f, 0.0f));
            arrowPath.applyTransform(juce::AffineTransform::translation(cx, cy));
            g.fillPath(arrowPath);

            // Indent label to leave room for arrow
            display = " " + label;
            g.drawText(display, rect, juce::Justification::centredLeft, true);
        }
        else
        {
            g.drawText(display, rect, juce::Justification::centredLeft, true);
        }
    };

    paintCell(rectFav,      "*",        SortByFavorite);
    paintCell(rectName,     "Name",     SortByName);
    paintCell(rectCategory, "Category", SortByCategory);
    paintCell(rectLibrary,  "Library",  SortByLibrary);
}

// ============================================================
// RESIZE → RECALCULATE COLUMN WIDTHS
// ============================================================
void PresetBrowserHeaderBar::resized()
{
    recalcWidths();
}

void PresetBrowserHeaderBar::recalcWidths()
{
    const int contentW = getWidth();
    if (contentW <= 0)
        return;

    const int favW = kColFavW;
    const int catW = juce::jmax(kColCatWMin, contentW / kColProportionDivisor);
    const int libW = juce::jmax(kColLibWMin, contentW / kColProportionDivisor);
    const int nameW = contentW - favW - catW - libW - kColGapAdj;

    headerCatW  = catW;
    headerLibW  = libW;
    headerNameW = nameW;

    int x = kColLeftMarg;
    const int h = getHeight();
    rectFav      = { x, 0, favW, h }; x += favW + kColGap;
    rectName     = { x, 0, nameW, h }; x += nameW + kColGap;
    rectCategory = { x, 0, catW,  h }; x += catW + kColGap;
    rectLibrary  = { x, 0, libW,  h };
}

// ============================================================
// HEADER CLICK → SORT
// ============================================================
void PresetBrowserHeaderBar::mouseDown(const juce::MouseEvent& e)
{
    auto pos = e.getPosition();

    auto check = [&](const juce::Rectangle<int>& rect, SortColumn col) -> bool
    {
        if (! rect.contains(pos))
            return false;
        if (onSortClicked)
            onSortClicked(col);
        return true;
    };

    if (check(rectFav,      SortByFavorite)) return;
    if (check(rectName,     SortByName))     return;
    if (check(rectCategory, SortByCategory)) return;
    if (check(rectLibrary,  SortByLibrary))  return;
}

// ============================================================
// HOVER & CURSOR
// ============================================================
void PresetBrowserHeaderBar::mouseMove(const juce::MouseEvent& e)
{
    auto pos = e.getPosition();

    // Early-out: only care about the header row
    if (pos.y < 0 || pos.y >= getHeight())
    {
        if (hoveredHeaderCol != -1)
        {
            hoveredHeaderCol = -1;
            setTooltip({});
            setMouseCursor(juce::MouseCursor::NormalCursor);
            repaint();
        }
        return;
    }

    int newHover = -1;
    if      (rectFav.contains(pos))      newHover = SortByFavorite;
    else if (rectName.contains(pos))     newHover = SortByName;
    else if (rectCategory.contains(pos)) newHover = SortByCategory;
    else if (rectLibrary.contains(pos))  newHover = SortByLibrary;

    if (newHover != hoveredHeaderCol)
    {
        hoveredHeaderCol = newHover;

        // Update tooltip
        switch (newHover)
        {
            case SortByName:     setTooltip("Click to sort by Name");     break;
            case SortByCategory: setTooltip("Click to sort by Category"); break;
            case SortByLibrary:  setTooltip("Click to sort by Library");  break;
            case SortByFavorite: setTooltip("Click to sort by Favorites"); break;
            default:             setTooltip({});                           break;
        }

        setMouseCursor(newHover != -1
                       ? juce::MouseCursor::PointingHandCursor
                       : juce::MouseCursor::NormalCursor);
        repaint();
    }
}

void PresetBrowserHeaderBar::mouseExit(const juce::MouseEvent&)
{
    if (hoveredHeaderCol != -1)
    {
        hoveredHeaderCol = -1;
        setTooltip({});
        setMouseCursor(juce::MouseCursor::NormalCursor);
        repaint();
    }
}

// ============================================================
// SORT STATE (called by parent)
// ============================================================
void PresetBrowserHeaderBar::setSortState(SortColumn col, bool ascending)
{
    activeSortCol  = col;
    sortAscending  = ascending;
    startArrowAnim();
    repaint();
}

// ============================================================
// ARROW ANIMATION
// ============================================================
void PresetBrowserHeaderBar::startArrowAnim()
{
    arrowAnimTarget = sortAscending ? 0.0f : juce::MathConstants<float>::pi;
    startTimerHz(60);
}

void PresetBrowserHeaderBar::timerCallback()
{
    const float diff = arrowAnimTarget - arrowAnimAngle;
    if (std::abs(diff) < kAnimThreshold)
    {
        arrowAnimAngle = arrowAnimTarget;
        stopTimer();
        repaint();
        return;
    }
    arrowAnimAngle += diff * kAnimSpeed;
    repaint();
}
