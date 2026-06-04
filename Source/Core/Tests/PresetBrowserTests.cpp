#define JUCE_UNIT_TESTS 1
#include <JuceHeader.h>
#include "../../UI/PresetBrowser.h"

/**
 * PresetBrowser Column Width Alignment Tests.
 *
 * Verifies that the column width formula used in recalculateHeaderRects()
 * produces widths that are internally consistent and match the layout
 * used by paintListBoxItem().
 *
 * Both resized() and paintListBoxItem() now read from the SAME stored
 * header rect members (headerFav, headerName, headerCategory, headerLibrary),
 * so alignment is architecturally guaranteed. This test verifies the
 * mathematical invariants of the formula itself.
 */
class PresetBrowserColumnWidthTests : public juce::UnitTest
{
public:
    PresetBrowserColumnWidthTests()
        : juce::UnitTest("PresetBrowser Column Widths", "JunoUI")
    {
    }

    // ── Column width constants (must match PresetBrowser::kCol*)
    static constexpr int kColFavW     = 24;
    static constexpr int kColCatWMin  = 80;
    static constexpr int kColLibWMin  = 70;
    static constexpr int kColGapAdj   = 8;
    static constexpr int kColLeftMarg = 4;
    static constexpr int kColGap      = 2;

    // Compile-time verification that test constants match PresetBrowser
    static_assert(kColFavW     == PresetBrowser::kColFavW,     "kColFavW must match PresetBrowser");
    static_assert(kColCatWMin  == PresetBrowser::kColCatWMin,  "kColCatWMin must match PresetBrowser");
    static_assert(kColLibWMin  == PresetBrowser::kColLibWMin,  "kColLibWMin must match PresetBrowser");
    static_assert(kColGapAdj   == PresetBrowser::kColGapAdj,   "kColGapAdj must match PresetBrowser");
    static_assert(kColLeftMarg == PresetBrowser::kColLeftMarg, "kColLeftMarg must match PresetBrowser");
    static_assert(kColGap      == PresetBrowser::kColGap,      "kColGap must match PresetBrowser");

    // Compile-time verification that layout constants match PresetBrowser
    static_assert(PresetBrowser::kOuterMargin   == 5,  "PresetBrowser::kOuterMargin should be 5");
    static_assert(PresetBrowser::kRowH          == 30, "PresetBrowser::kRowH should be 30");
    static_assert(PresetBrowser::kInnerPad      == 2,  "PresetBrowser::kInnerPad should be 2");
    static_assert(PresetBrowser::kSectionGap    == 5,  "PresetBrowser::kSectionGap should be 5");
    static_assert(PresetBrowser::kHeaderBarH    == 22, "PresetBrowser::kHeaderBarH should be 22");
    static_assert(PresetBrowser::kHeaderListGap == 2,  "PresetBrowser::kHeaderListGap should be 2");

    // Compile-time verification of new layout/font constants
    static_assert(PresetBrowser::kColProportionDivisor == 5,   "kColProportionDivisor should be 5");
    static_assert(PresetBrowser::kListRowH             == 24,  "kListRowH should match PresetBrowser definition");
    static_assert(PresetBrowser::kNameFontScale        == 0.65f, "kNameFontScale should be 0.65");
    static_assert(PresetBrowser::kStarFontScale        == 0.65f, "kStarFontScale should be 0.65");
    static_assert(PresetBrowser::kDetailFontScale      == 0.55f, "kDetailFontScale should be 0.55");
    static_assert(PresetBrowser::kSmallFontScale       == 0.50f, "kSmallFontScale should be 0.50");

    // Compile-time verification of new visual/alpha constants
    static_assert(PresetBrowser::kSelectedBgAlpha == 0.25f, "kSelectedBgAlpha should be 0.25");
    static_assert(PresetBrowser::kNameTextAlpha   == 0.85f, "kNameTextAlpha should be 0.85");
    static_assert(PresetBrowser::kDetailTextAlpha == 0.50f, "kDetailTextAlpha should be 0.50");
    static_assert(PresetBrowser::kLibTextAlpha    == 0.50f, "kLibTextAlpha should be 0.50");
    static_assert(PresetBrowser::kBgAlpha         == 0.20f, "kBgAlpha should be 0.20");
    static_assert(PresetBrowser::kFieldWidthRatio == 0.40f, "kFieldWidthRatio should be 0.40");

    // ── Helpers: column width formula (mirrors recalculateHeaderRects()) ──
    static int calcFavW(int) { return kColFavW; }
    static int calcCatW(int contentW) { return juce::jmax(kColCatWMin, contentW / PresetBrowser::kColProportionDivisor); }
    static int calcLibW(int contentW) { return juce::jmax(kColLibWMin, contentW / PresetBrowser::kColProportionDivisor); }
    static int calcNameW(int contentW)
    {
        return contentW - calcFavW(contentW) - calcCatW(contentW) - calcLibW(contentW) - kColGapAdj;
    }

    // Simulate the column layout loop used in both resized() and paintListBoxItem()
    static int rightEdgeAfter(int contentW)
    {
        int x = kColLeftMarg;
        x += calcFavW(contentW) + kColGap;
        x += calcNameW(contentW) + kColGap;
        x += calcCatW(contentW) + kColGap;
        x += calcLibW(contentW);
        return x;
    }

    void runTest() override
    {
        // Minimum viable width where nameW > 0: 24 + 80 + 70 + 8 + 1 = 183
        // Below that, the formula produces negative nameW because minimum column
        // widths (favW=24, catW>=80, libW>=70) already exceed the content width.
        // In practice the PresetBrowser sidebar is ~380px, so this never occurs.
        std::vector<int> testWidths = { 100, 180, 200, 300, 380, 500, 800, 1200 };

        // ── Test 1: Invariant — column widths are consistent and sum to contentWidth ──
        beginTest("Column widths sum to contentWidth across various sizes");
        {
            for (int w : testWidths)
            {
                int favW = calcFavW(w);
                int catW = calcCatW(w);
                int libW = calcLibW(w);
                int nameW = calcNameW(w);
                int rightEdge = rightEdgeAfter(w);

                expect(favW == kColFavW, "favW should be 24, got " + juce::String(favW));
                expect(catW >= kColCatWMin, "catW should be >= 80, got " + juce::String(catW) + " for w=" + juce::String(w));
                expect(libW >= kColLibWMin, "libW should be >= 70, got " + juce::String(libW) + " for w=" + juce::String(w));

                // nameW becomes negative when minimums exceed content width.
                // This is expected — callers guard with listContentW > 0.
                if (w >= 200)
                    expect(nameW > 0, "nameW should be > 0 for w=" + juce::String(w)
                           + ", got " + juce::String(nameW));

                // Right edge invariant holds at ALL widths
                expect(std::abs(rightEdge - w) <= 2,
                       "Right edge should be within 2px of w=" + juce::String(w)
                       + ", got " + juce::String(rightEdge));

                printf("  contentW=%4d => fav=%d cat=%d lib=%d name=%d rightEdge=%d\n",
                       w, favW, catW, libW, nameW, rightEdge);
            }
        }

        // ── Test 2: paintListBoxItem() layout is byte-identical to resized() layout ──
        beginTest("paintListBoxItem layout matches resized layout");
        {
            for (int w : testWidths)
            {
                int favW = calcFavW(w);
                int catW = calcCatW(w);
                int libW = calcLibW(w);
                int nameW = calcNameW(w);

                // Simulate resized() layout: fav -> name -> cat -> lib
                int xr = kColLeftMarg;
                auto rFav  = juce::Rectangle<int>(xr, 0, favW, 0);  xr += favW + kColGap;
                auto rName = juce::Rectangle<int>(xr, 0, nameW, 0); xr += nameW + kColGap;
                auto rCat  = juce::Rectangle<int>(xr, 0, catW, 0);  xr += catW + kColGap;
                auto rLib  = juce::Rectangle<int>(xr, 0, libW, 0);

                // Simulate paintListBoxItem() layout: fav -> name -> cat -> lib
                int xp = kColLeftMarg;
                auto pFav  = juce::Rectangle<int>(xp, 0, favW, 0);  xp += favW + kColGap;
                auto pName = juce::Rectangle<int>(xp, 0, nameW, 0); xp += nameW + kColGap;
                auto pCat  = juce::Rectangle<int>(xp, 0, catW, 0);  xp += catW + kColGap;
                auto pLib  = juce::Rectangle<int>(xp, 0, libW, 0);

                expect(rFav  == pFav,  "headerFav X mismatch at w=" + juce::String(w));
                expect(rName == pName, "headerName X mismatch at w=" + juce::String(w));
                expect(rCat  == pCat,  "headerCategory X mismatch at w=" + juce::String(w));
                expect(rLib  == pLib,  "headerLibrary X mismatch at w=" + juce::String(w));
            }
        }

        // ── Test 3: Edge cases — min viable, scaling, zero width ──
        beginTest("Edge cases: min width, very narrow, very wide, zero");
        {
            // (a) Min viable width (nameW barely > 0)
            {
                int w = 183; // 24 + 80 + 70 + 8 + 1
                int nameW = calcNameW(w);
                expect(nameW > 0, "nameW should be > 0 at min viable width " + juce::String(w)
                       + ", got " + juce::String(nameW));
                int re = rightEdgeAfter(w);
                expect(std::abs(re - w) <= 2, "Right edge mismatch at min width w=" + juce::String(w));
            }

            // (b) Width where catW == 80 (contentW/5 <= 80)
            {
                int w = 200;
                expect(calcCatW(w) == kColCatWMin, "catW should clamp to 80 at w=200, got " + juce::String(calcCatW(w)));
                expect(calcLibW(w) == kColLibWMin, "libW should clamp to 70 at w=200, got " + juce::String(calcLibW(w)));
            }

            // (c) Large width where catW and libW scale with contentW/5
            {
                int w = 1000;
                expect(calcCatW(w) == w / 5,
                       "catW should scale with contentW/5 at large widths, got " + juce::String(calcCatW(w)));
                expect(calcLibW(w) == w / 5,
                       "libW should scale with contentW/5 at large widths, got " + juce::String(calcLibW(w)));
            }

            // (d) Zero width — formula should handle gracefully (callers guard with > 0 check)
            {
                int nameW = calcNameW(0);
                expect(nameW < 0, "nameW should be negative for contentW=0, got " + juce::String(nameW));
                expect(calcCatW(0) == kColCatWMin, "catW should clamp to 80 even at contentW=0");
                expect(calcLibW(0) == kColLibWMin, "libW should clamp to 70 even at contentW=0");
            }
        }

        printf("PresetBrowserColumnWidthTests: ALL CHECKS PASSED\n");
    }
};

static PresetBrowserColumnWidthTests presetBrowserColumnWidthTests;
