#pragma once
#include <JuceHeader.h>
#include <vector>
#include <algorithm>

namespace ABD {

struct Preset
{
    juce::String    name;
    juce::String    category;
    juce::String    author;
    juce::String    tags;
    juce::String    notes;
    juce::String    creationDate;
    bool            isFavorite = false;
    juce::ValueTree state;

    // Heritage metadata
    int originGroup = -1; // 0=A, 1=B, etc.
    int originBank  = -1; // 1-8
    int originPatch = -1; // 1-8

    Preset() : isFavorite(false), originGroup(-1), originBank(-1), originPatch(-1) {}
    
    Preset(const juce::String& n, const juce::String& cat, const juce::ValueTree& s)
        : name(n), category(cat), isFavorite(false), state(s), originGroup(-1), originBank(-1), originPatch(-1) {}

    Preset(const juce::String& n, const juce::String& cat, const juce::String& auth,
           const juce::String& t, const juce::String& nots, const juce::String& date,
           bool fav, const juce::ValueTree& s)
        : name(n), category(cat), author(auth), tags(t), notes(nots), creationDate(date),
          isFavorite(fav), state(s) {}

    juce::ValueTree toValueTree() const
    {
        juce::ValueTree vt ("Preset");
        vt.setProperty ("name",     name,     nullptr);
        vt.setProperty ("category", category, nullptr);
        vt.setProperty ("author",   author,   nullptr);
        vt.setProperty ("tags",     tags,     nullptr);
        vt.setProperty ("notes",    notes,    nullptr);
        vt.setProperty ("favorite", isFavorite, nullptr);
        vt.setProperty ("date",     creationDate, nullptr);
        
        if (originGroup >= 0) vt.setProperty ("originGroup", originGroup, nullptr);
        if (originBank >= 0)  vt.setProperty ("originBank",  originBank,  nullptr);
        if (originPatch >= 0) vt.setProperty ("originPatch", originPatch, nullptr);

        if (state.isValid())
            vt.addChild (state.createCopy(), -1, nullptr);
        return vt;
    }

    void fromValueTree (const juce::ValueTree& vt)
    {
        name         = vt.getProperty ("name", "").toString();
        category     = vt.getProperty ("category", "").toString();
        author       = vt.getProperty ("author", "").toString();
        tags         = vt.getProperty ("tags", "").toString();
        notes        = vt.getProperty ("notes", "").toString();
        isFavorite   = (bool)vt.getProperty ("favorite", false);
        creationDate = vt.getProperty ("date", "").toString();
        
        originGroup  = (int)vt.getProperty ("originGroup", -1);
        originBank   = (int)vt.getProperty ("originBank",  -1);
        originPatch  = (int)vt.getProperty ("originPatch", -1);

        auto paramsTree = vt.getChildWithName ("Parameters");
        if (paramsTree.isValid())
            state = paramsTree.createCopy();
        else if (vt.hasType("Parameters"))
            state = vt.createCopy();
        else if (vt.getNumChildren() > 0)
            state = vt.getChild(0).createCopy();
    }
};

struct Library
{
    juce::String         name;
    std::vector<Preset>  patches;
};

class PresetManagerBase
{
public:
    virtual ~PresetManagerBase() = default;

    void addLibrary(const juce::String& name)
    {
        libraries_.push_back({ name, {} });
    }

    void selectLibrary(int index)
    {
        currentLibIdx_ = juce::jlimit(0, (int)libraries_.size() - 1, index);
    }

    int getNumLibraries() const noexcept { return (int)libraries_.size(); }
    
    const Library& getCurrentLibrary() const
    {
        if (currentLibIdx_ >= 0 && currentLibIdx_ < (int)libraries_.size())
            return libraries_[currentLibIdx_];
        static Library empty;
        return empty;
    }

    Library& getCurrentLibrary()
    {
        if (currentLibIdx_ >= 0 && currentLibIdx_ < (int)libraries_.size())
            return libraries_[currentLibIdx_];
        static Library empty;
        return empty;
    }

    int getActiveLibraryIndex() const noexcept { return currentLibIdx_; }

    const Library& getLibrary(int idx) const
    {
        return libraries_[juce::jlimit(0, (int)std::max(0, (int)libraries_.size()-1), idx)];
    }

    Library& getLibrary(int idx)
    {
        return libraries_[juce::jlimit(0, (int)std::max(0, (int)libraries_.size()-1), idx)];
    }

    int getLibraryIndex(const juce::String& name) const
    {
        for (int i = 0; i < (int)libraries_.size(); ++i)
            if (libraries_[i].name == name) return i;
        return -1;
    }

    void setCurrentPreset(int index) noexcept { currentPresetIdx_ = index; }
    int getCurrentPresetIndex() const noexcept { return currentPresetIdx_; }

    juce::String getCurrentPresetName() const
    {
        if (auto* p = getPreset(currentPresetIdx_))
            return p->name;
        return {};
    }

    juce::ValueTree getCurrentPresetState() const
    {
        if (auto* p = getPreset(currentPresetIdx_))
            return p->state;
        return {};
    }

    const Preset* getPreset(int index) const
    {
        if (currentLibIdx_ < 0 || currentLibIdx_ >= (int)libraries_.size()) return nullptr;
        const auto& lib = libraries_[currentLibIdx_];
        if (index < 0 || index >= (int)lib.patches.size()) return nullptr;
        return &lib.patches[index];
    }

    juce::StringArray getPresetNames() const
    {
        juce::StringArray names;
        if (currentLibIdx_ >= 0 && currentLibIdx_ < (int)libraries_.size())
            for (auto& p : libraries_[currentLibIdx_].patches)
                names.add(p.name);
        return names;
    }

    virtual juce::String getSynthType() const = 0;
    virtual void loadFactoryPresets() = 0;
    virtual juce::ValueTree bytesToState(const uint8_t* data, int size) const = 0;
    virtual std::vector<uint8_t> stateToBytes(const juce::ValueTree& state) const = 0;
    virtual juce::File getUserPresetsDirectory() const = 0;

    void saveUserPresets()
    {
        auto dir = getUserPresetsDirectory();
        if (!dir.exists()) dir.createDirectory();
        
        for (const auto& lib : libraries_)
        {
            if (lib.name == "User")
            {
                for (const auto& p : lib.patches)
                {
                    auto vt = p.toValueTree();
                    auto file = dir.getChildFile(p.name + ".json");
                    file.replaceWithText(vt.toXmlString());
                }
            }
        }
    }

    void writeUserPresets() { saveUserPresets(); }

    void loadBrowserData()
    {
        auto dir = getUserPresetsDirectory();
        if (!dir.exists()) return;

        auto userLibIdx = getLibraryIndex("User");
        if (userLibIdx == -1)
        {
            addLibrary("User");
            userLibIdx = (int)libraries_.size() - 1;
        }

        auto files = dir.findChildFiles(juce::File::findFiles, false, "*.json");
        for (auto& f : files)
        {
            juce::ValueTree vt = juce::ValueTree::fromXml(f.loadFileAsString());
            if (vt.isValid())
            {
                Preset p;
                p.fromValueTree(vt);
                libraries_[userLibIdx].patches.push_back(p);
            }
        }
    }

    void saveBrowserData() {}
    void setLastPath(const juce::String& path) { lastPath_ = path; }
    juce::String getLastPath() const { return lastPath_; }

public:
    std::vector<Library> libraries_;
    int currentLibIdx_ = -1;
    int currentPresetIdx_ = 0;
    juce::String lastPath_;
};

} // namespace ABD
