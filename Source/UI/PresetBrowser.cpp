#include "PresetBrowser.h"
#include "../Core/PresetManager.h"

PresetBrowser::PresetBrowser(PresetManager& pm) : presetManager(pm),
    headerBar()
{
    setWantsKeyboardFocus(true);

    // ── Header bar ──
    addAndMakeVisible(headerBar);
    headerBar.onSortClicked = [this](PresetBrowserHeaderBar::SortColumn col)
    {
        auto typedCol = static_cast<SortColumn>(col);
        if (activeSortCol == typedCol)
            sortAscending = !sortAscending;
        else
        {
            activeSortCol = typedCol;
            sortAscending = true;
        }
        headerBar.setSortState(col, sortAscending);
        updateFilters();
        repaint();
    };

    addAndMakeVisible(searchField);
    searchField.setTextToShowWhenEmpty("Search presets...", juce::Colours::grey);
    searchField.onTextChange = [this] { updateFilters(); };

    addAndMakeVisible(librarySelector);
    librarySelector.onChange = [this] { updateFilters(); };

    addAndMakeVisible(categoryFilter);
    categoryFilter.addItem("All Categories", 1);
    for (int i = 0; i < (int)presetManager.categories_.size(); ++i)
        categoryFilter.addItem(presetManager.categories_[i], i + 2);
    categoryFilter.setSelectedId(1, juce::dontSendNotification);
    categoryFilter.onChange = [this] { updateFilters(); };

    addAndMakeVisible(favoritesToggle);
    favoritesToggle.onClick = [this] { updateFilters(); };

    addAndMakeVisible(presetList);
    presetList.setModel(this);
    presetList.setRowHeight(kListRowH);
    presetList.setColour(juce::ListBox::outlineColourId, juce::Colours::transparentBlack);
    presetList.addKeyListener(this);
    presetList.setWantsKeyboardFocus(false);

    addAndMakeVisible(saveBtn);
    saveBtn.setColour(juce::TextButton::buttonColourId, juce::Colours::darkgrey);
    saveBtn.onClick = [this] { if (onSaveClicked) onSaveClicked(); };

    addAndMakeVisible(saveAsBtn);
    saveAsBtn.setColour(juce::TextButton::buttonColourId, juce::Colours::darkblue.withAlpha(0.5f));
    saveAsBtn.onClick = [this] { if (onSaveAsClicked) onSaveAsClicked(); };

    refresh();
}



// ============================================================
// KEYBOARD
// ============================================================
bool PresetBrowser::keyPressed(const juce::KeyPress& key)
{
    const int numRows = getNumRows();
    int selIdx = getSelectedFilteredIndex();

    if (key == juce::KeyPress::upKey)
    {
        if (selIdx > 0)
            setSelectedFilteredIndex(selIdx - 1);
        else if (numRows > 0)
            setSelectedFilteredIndex(0);
        return true;
    }

    if (key == juce::KeyPress::downKey)
    {
        if (selIdx < numRows - 1)
            setSelectedFilteredIndex(selIdx + 1);
        return true;
    }

    if (key == juce::KeyPress::returnKey || key == juce::KeyPress::spaceKey)
    {
        return true; // Already loaded via selectedRowsChanged
    }

    if (key == juce::KeyPress::escapeKey)
    {
        if (onCloseRequested)
            onCloseRequested();
        return true;
    }

    if (key == juce::KeyPress::pageUpKey)
    {
        int step = juce::jmax(1, presetList.getNumRowsOnScreen() - 1);
        setSelectedFilteredIndex(juce::jmax(0, selIdx - step));
        return true;
    }
    if (key == juce::KeyPress::pageDownKey)
    {
        int step = juce::jmax(1, presetList.getNumRowsOnScreen() - 1);
        setSelectedFilteredIndex(juce::jmin(numRows - 1, selIdx + step));
        return true;
    }

    if (key == juce::KeyPress::homeKey)
    {
        if (numRows > 0) setSelectedFilteredIndex(0);
        return true;
    }
    if (key == juce::KeyPress::endKey)
    {
        if (numRows > 0) setSelectedFilteredIndex(numRows - 1);
        return true;
    }

    // Type-ahead: forward printable characters to the search field
    if (key.getTextCharacter() >= ' ' && key.getTextCharacter() <= '~')
    {
        searchField.grabKeyboardFocus();
        searchField.setText(searchField.getText() + juce::String::charToString(key.getTextCharacter()));
        return true;
    }

    return false;
}

// ============================================================
// PAINT & RESIZE
// ============================================================
void PresetBrowser::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black.withAlpha(kBgAlpha));
}

void PresetBrowser::resized()
{
    auto area = getLocalBounds().reduced(kOuterMargin);

    // ── Top row: search + library + fav toggle ──
    auto topArea = area.removeFromTop(kRowH);
    searchField.setBounds(topArea.removeFromLeft((int)(topArea.getWidth() * kFieldWidthRatio)).reduced(kInnerPad));
    librarySelector.setBounds(topArea.removeFromLeft((int)(topArea.getWidth() * kFieldWidthRatio)).reduced(kInnerPad));
    favoritesToggle.setBounds(topArea.reduced(kInnerPad));

    area.removeFromTop(kSectionGap);

    // ── Second row: category filter ──
    auto filterArea = area.removeFromTop(kRowH);
    categoryFilter.setBounds(filterArea.reduced(kInnerPad));

    // ── Column header row (height = kHeaderBarH, width matches list content width) ──
    area.removeFromTop(kSectionGap);
    auto headerArea = area.removeFromTop(kHeaderBarH);
    const int headerY = headerArea.getY();
    const int headerH = headerArea.getHeight();

    const int listContentW = presetList.getVisibleRowWidth();
    if (listContentW > 0)
        headerBar.setBounds(headerArea.getX(), headerY, listContentW, headerH);
    else
        headerBar.setBounds(headerArea);

    area.removeFromTop(kHeaderListGap);

    // ── Bottom row: buttons ──
    auto bottomArea = area.removeFromBottom(kRowH);
    saveBtn.setBounds(bottomArea.removeFromLeft(bottomArea.getWidth() / 2).reduced(kInnerPad));
    saveAsBtn.setBounds(bottomArea.reduced(kInnerPad));

    area.removeFromTop(kSectionGap);

    // ── Preset list takes remaining space ──
    presetList.setBounds(area);
}

// ============================================================
// LISTBOX MODEL
// ============================================================
int PresetBrowser::getNumRows()
{
    return (int)filteredItems.size();
}

void PresetBrowser::paintListBoxItem(int rowNumber, juce::Graphics& g,
                                      int /*width*/, int height, bool rowIsSelected)
{
    if (rowNumber >= (int)filteredItems.size())
        return;

    auto& item = filteredItems[rowNumber];

    // Background
    if (rowIsSelected)
        g.fillAll(juce::Colours::royalblue.withAlpha(kSelectedBgAlpha));

    // ── Column layout: use headerBar widths for perfect alignment with headers ──
    const int favW = headerBar.getColFavW();
    const int catW = headerBar.getColCatW();
    const int libW = headerBar.getColLibW();
    const int nameW = headerBar.getColNameW();

    int x = headerBar.getColLeftMarg();

    // Col 1 : Favourite star
    {
        juce::Rectangle<int> r(x, 0, favW, height);    x += favW + headerBar.getColGap();
        if (item.preset->isFavorite)
        {
            g.setColour(juce::Colours::gold);
            g.setFont(height * kStarFontScale);
            g.drawText("*", r, juce::Justification::centred);
        }
    }

    // Col 2 : Preset name
    {
        juce::Rectangle<int> r(x, 0, nameW, height);    x += nameW + headerBar.getColGap();
        g.setColour(rowIsSelected ? juce::Colours::white : juce::Colours::white.withAlpha(kNameTextAlpha));
        g.setFont(height * kNameFontScale);
        g.drawText(item.preset->name, r, juce::Justification::centredLeft, true);
    }

    // Col 3 : Category
    {
        juce::Rectangle<int> r(x, 0, catW, height);    x += catW + headerBar.getColGap();
        g.setColour(juce::Colours::lightgrey.withAlpha(kDetailTextAlpha));
        g.setFont(height * kDetailFontScale);
        g.drawText(item.preset->category, r, juce::Justification::centredLeft, true);
    }

    // Col 4 : Library name
    {
        juce::Rectangle<int> r(x, 0, libW, height);
        g.setColour(juce::Colours::orange.withAlpha(kLibTextAlpha));
        g.setFont(height * kSmallFontScale);
        g.drawText(presetManager.libraries_[item.libIdx].name,
                   r, juce::Justification::centredLeft, true);
    }
}

void PresetBrowser::listBoxItemClicked(int row, const juce::MouseEvent&)
{
    // Single click: selection is handled by selectedRowsChanged.
    // Do NOT load here to avoid double-triggering.
    juce::ignoreUnused(row);
}

void PresetBrowser::listBoxItemDoubleClicked(int row, const juce::MouseEvent&)
{
    if (row < 0 || row >= (int)filteredItems.size())
        return;

    // Double-click: close the browser (preset already loaded by selectedRowsChanged)
    if (onCloseRequested)
        onCloseRequested();
}

void PresetBrowser::selectedRowsChanged(int lastRowSelected)
{
    if (lastRowSelected >= 0 && lastRowSelected < (int)filteredItems.size())
        loadPresetAt(lastRowSelected);
}

// ============================================================
// REFRESH & FILTERS
// ============================================================
void PresetBrowser::refresh()
{
    auto lastSelectedId = librarySelector.getSelectedId();
    librarySelector.clear(juce::dontSendNotification);
    librarySelector.addItem("All Libraries", 1);

    for (int i = 0; i < (int)presetManager.libraries_.size(); ++i)
        librarySelector.addItem(presetManager.libraries_[i].name, i + 2);

    if (lastSelectedId > 0)
        librarySelector.setSelectedId(lastSelectedId, juce::dontSendNotification);
    else
        librarySelector.setSelectedId(1, juce::dontSendNotification);

    updateFilters();
}

void PresetBrowser::updateFilters()
{
    // ── Save previous selection identity (libIdx + presetIdx) ──
    int prevLib = -1, prevPreset = -1;
    int prevFilteredIdx = getSelectedFilteredIndex();
    if (prevFilteredIdx >= 0 && prevFilteredIdx < (int)filteredItems.size())
    {
        prevLib = filteredItems[prevFilteredIdx].libIdx;
        prevPreset = filteredItems[prevFilteredIdx].presetIdx;
    }
    // Detect if the PresetManager's selection changed externally (e.g. after import)
    bool mgrSelectionChanged = (prevLib != presetManager.getCurrentLibraryIndex() ||
                                prevPreset != presetManager.getCurrentPresetIndex());

    // ── Save scroll position ──
    double scrollPos = 0.0;
    auto& scrollbar = presetList.getVerticalScrollBar();
    scrollPos = scrollbar.getCurrentRangeStart();

    // ── Rebuild filtered list ──
    filteredItems.clear();

    juce::String search = searchField.getText();
    juce::String category = categoryFilter.getText();
    if (category == "All Categories") category.clear();

    juce::String libName = librarySelector.getText();
    if (libName == "All Libraries") libName.clear();

    bool favOnly = favoritesToggle.getToggleState();

    for (int l = 0; l < (int)presetManager.libraries_.size(); ++l)
    {
        auto& lib = presetManager.libraries_[l];
        if (libName.isNotEmpty() && lib.name != libName) continue;

        for (int p = 0; p < (int)lib.patches.size(); ++p)
        {
            auto& pr = lib.patches[p];
            if (favOnly && !pr.isFavorite) continue;
            if (category.isNotEmpty() && pr.category != category) continue;
            if (search.isNotEmpty() && !pr.name.containsIgnoreCase(search)) continue;

            filteredItems.push_back({ l, p, &pr });
        }
    }

    // ── Sort ──
    std::sort(filteredItems.begin(), filteredItems.end(),
        [this](const PresetRef& a, const PresetRef& b) -> bool
        {
            bool less = false;
            switch (activeSortCol)
            {
                case SortByName:
                    less = a.preset->name.compareNatural(b.preset->name) < 0;
                    break;
                case SortByCategory:
                    less = a.preset->category.compare(b.preset->category) < 0;
                    if (!less && a.preset->category == b.preset->category)
                        less = a.preset->name.compareNatural(b.preset->name) < 0;
                    break;
                case SortByLibrary:
                    less = presetManager.libraries_[a.libIdx].name
                            .compare(presetManager.libraries_[b.libIdx].name) < 0;
                    if (!less && presetManager.libraries_[a.libIdx].name
                                    == presetManager.libraries_[b.libIdx].name)
                        less = a.preset->name.compareNatural(b.preset->name) < 0;
                    break;
                case SortByFavorite:
                    less = a.preset->isFavorite && !b.preset->isFavorite;
                    if (a.preset->isFavorite == b.preset->isFavorite)
                        less = a.preset->name.compareNatural(b.preset->name) < 0;
                    break;
            }
            return sortAscending ? less : !less;
        });

    presetList.updateContent();
    presetList.repaint();

    // ── If scrollbar visibility changed (content width differs), update header bar width ──
    const int newContentW = presetList.getVisibleRowWidth();
    if (newContentW != lastContentWidth && newContentW > 0)
    {
        headerBar.setSize(newContentW, headerBar.getHeight());
        lastContentWidth = newContentW;
    }

    // ── Restore selection ──
    // Temporarily disconnect the audio callback to avoid reloading the same preset
    auto savedOnPresetSelected = onPresetSelected;
    onPresetSelected = nullptr;

    bool selectionRestored = false;

    // Priority 1: Restore to the same preset (only if manager hasn't changed externally)
    if (!mgrSelectionChanged && prevLib >= 0)
    {
        for (int i = 0; i < (int)filteredItems.size(); ++i)
        {
            if (filteredItems[i].libIdx == prevLib && filteredItems[i].presetIdx == prevPreset)
            {
                presetList.selectRow(i);
                presetList.scrollToEnsureRowIsOnscreen(i);
                selectionRestored = true;
                break;
            }
        }
    }

    // Priority 2: If previous not found, try PresetManager's current selection
    if (!selectionRestored)
    {
        int currentLib = presetManager.getCurrentLibraryIndex();
        int currentPreset = presetManager.getCurrentPresetIndex();
        for (int i = 0; i < (int)filteredItems.size(); ++i)
        {
            if (filteredItems[i].libIdx == currentLib && filteredItems[i].presetIdx == currentPreset)
            {
                presetList.selectRow(i);
                presetList.scrollToEnsureRowIsOnscreen(i);
                selectionRestored = true;
                break;
            }
        }
    }

    onPresetSelected = savedOnPresetSelected;

    // ── Restore scroll position if no selection was restored ──
    if (!selectionRestored && scrollPos > 0.0)
    {
        auto& scrollbar = presetList.getVerticalScrollBar();
        scrollbar.setCurrentRangeStart(scrollPos);
    }
}

PresetManager& PresetBrowser::getPresetManager() { return presetManager; }


// ============================================================
// HELPERS
// ============================================================
int PresetBrowser::getSelectedFilteredIndex() const
{
    auto selected = presetList.getSelectedRow();
    if (selected >= 0 && selected < (int)filteredItems.size())
        return selected;
    return -1;
}

void PresetBrowser::setSelectedFilteredIndex(int idx)
{
    if (idx >= 0 && idx < (int)filteredItems.size())
    {
        presetList.selectRow(idx);
        presetList.scrollToEnsureRowIsOnscreen(idx);
    }
}

void PresetBrowser::loadPresetAt(int filteredIndex)
{
    if (filteredIndex < 0 || filteredIndex >= (int)filteredItems.size())
        return;

    auto& item = filteredItems[filteredIndex];
    presetManager.selectPreset(item.libIdx, item.presetIdx);
    if (onPresetSelected)
        onPresetSelected(item.libIdx, item.presetIdx);
}
