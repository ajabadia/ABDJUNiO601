// ABD-SynthEngine/Protocol/Presets/PresetManagerBase.h
#pragma once
#include <JuceHeader.h>
#include <vector>
#include <algorithm>

namespace ABD {

/**
 *  Base genérica para gestión de presets.
 *  Tu PresetManager del Juno hereda de aquí.
 *  CZPresetManager y ProphecyPresetManager también.
 *
 *  Cada sintetizador implementa:
 *  - stateToBytes() / bytesToState() para su formato de patch
 */
class PresetManagerBase
{
public:
    struct Preset
    {
        juce::String    name;
        juce::String    category;
        juce::ValueTree state;

        Preset() = default;
        Preset(const juce::String& n,
               const juce::String& cat,
               const juce::ValueTree& s)
            : name(n), category(cat), state(s) {}
    };

    struct Library
    {
        juce::String         name;
        std::vector<Preset>  patches;
    };

    virtual ~PresetManagerBase() = default;

    //── Librería ──────────────────────────────────────────────────────────────
    void addLibrary(const juce::String& name)
    {
        libraries_.push_back({ name, {} });
    }

    void selectLibrary(int index)
    {
        currentLibIdx_ = juce::jlimit(0, (int)libraries_.size() - 1, index);
    }

    int getNumLibraries()       const noexcept { return (int)libraries_.size(); }
    int getActiveLibraryIndex() const noexcept { return currentLibIdx_; }

    const Library& getLibrary(int idx) const
    {
        return libraries_[juce::jlimit(0, (int)libraries_.size()-1, idx)];
    }

    //── Preset activo ─────────────────────────────────────────────────────────
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
        if (currentLibIdx_ >= (int)libraries_.size()) return nullptr;
        auto& lib = libraries_[currentLibIdx_];
        if (index < 0 || index >= (int)lib.patches.size()) return nullptr;
        return &lib.patches[index];
    }

    juce::StringArray getPresetNames() const
    {
        juce::StringArray names;
        if (currentLibIdx_ < (int)libraries_.size())
            for (auto& p : libraries_[currentLibIdx_].patches)
                names.add(p.name);
        return names;
    }

    //── User presets ──────────────────────────────────────────────────────────
    void saveUserPreset(const juce::String& name,
                        const juce::ValueTree& state)
    {
        // Buscar librería "User"
        for (auto& lib : libraries_)
        {
            if (lib.name == "User")
            {
                // Actualizar si existe, añadir si no
                for (auto& p : lib.patches)
                    if (p.name == name)
                    { p.state = state; writeUserPresets(); return; }

                lib.patches.push_back({ name, "User", state });
                writeUserPresets();
                return;
            }
        }
        // Crear librería User si no existe
        Library user { "User", {} };
        user.patches.push_back({ name, "User", state });
        libraries_.push_back(user);
        writeUserPresets();
    }

    void deleteUserPreset(const juce::String& name)
    {
        for (auto& lib : libraries_)
        {
            if (lib.name == "User")
            {
                lib.patches.erase(
                    std::remove_if(lib.patches.begin(), lib.patches.end(),
                        [&](const Preset& p){ return p.name == name; }),
                    lib.patches.end());
                writeUserPresets();
                return;
            }
        }
    }

    //── Exportar / Importar JSON ──────────────────────────────────────────────
    void exportLibraryToJson(const juce::File& file) const
    {
        if (currentLibIdx_ >= (int)libraries_.size()) return;

        juce::Array<juce::var> jsonPresets;
        for (auto& p : libraries_[currentLibIdx_].patches)
        {
            juce::DynamicObject::Ptr o = new juce::DynamicObject();
            o->setProperty("name",  p.name);
            o->setProperty("state", p.state.toXmlString());
            jsonPresets.add(juce::var(o.get()));
        }

        juce::DynamicObject::Ptr root = new juce::DynamicObject();
        root->setProperty("synthType",   getSynthType());
        root->setProperty("libraryName", libraries_[currentLibIdx_].name);
        root->setProperty("presets",     jsonPresets);

        file.replaceWithText(juce::JSON::toString(juce::var(root.get())));
    }

    juce::Result importFromJson(const juce::File& file)
    {
        juce::var json;
        auto result = juce::JSON::parse(file.loadFileAsString(), json);
        if (result.failed()) return result;

        auto* obj = json.getDynamicObject();
        if (!obj) return juce::Result::fail("JSON inválido");

        // Verificar que el JSON es de este sintetizador
        juce::String type = obj->getProperty("synthType").toString();
        if (type.isNotEmpty() && type != getSynthType())
            return juce::Result::fail("Preset de otro sintetizador: " + type);

        juce::String libName = obj->getProperty("libraryName").toString();
        addLibrary(libName);
        int libIdx = getNumLibraries() - 1;

        auto presetsVar = obj->getProperty("presets");
        if (auto* arr = presetsVar.getArray())
        {
            for (auto& pVar : *arr)
            {
                if (auto* pObj = pVar.getDynamicObject())
                {
                    juce::String name  = pObj->getProperty("name").toString();
                    juce::String stateXml = pObj->getProperty("state").toString();
                    auto stateTree = juce::ValueTree::fromXml(stateXml);
                    libraries_[libIdx].patches.push_back({name, libName, stateTree});
                }
            }
        }
        return juce::Result::ok();
    }

    //── Interfaz virtual — cada sintetizador implementa ──────────────────────
    virtual juce::String    getSynthType()   const = 0;   // "Juno106", "CZ101", "Prophecy"
    virtual void            loadFactoryPresets()   = 0;
    virtual juce::ValueTree bytesToState(const uint8_t* data, int size) const = 0;
    virtual std::vector<uint8_t> stateToBytes(const juce::ValueTree&)   const = 0;
    virtual juce::File      getUserPresetsDirectory() const = 0;

protected:
    std::vector<Library> libraries_;
    int currentLibIdx_   { 0 };
    int currentPresetIdx_{ 0 };

    void loadUserPresets()
    {
        auto dir = getUserPresetsDirectory();
        if (!dir.exists()) return;

        addLibrary("User");
        int libIdx = getNumLibraries() - 1;

        for (auto& f : dir.findChildFiles(juce::File::findFiles, false, "*.json"))
        {
            juce::var json;
            if (juce::JSON::parse(f.loadFileAsString(), json).wasOk())
            {
                if (auto* o = json.getDynamicObject())
                {
                    juce::String name = o->getProperty("name").toString();
                    auto stateTree = juce::ValueTree::fromXml(
                        o->getProperty("state").toString());
                    libraries_[libIdx].patches.push_back({name, "User", stateTree});
                }
            }
        }
    }

private:
    void writeUserPresets()
    {
        auto dir = getUserPresetsDirectory();
        dir.createDirectory();

        for (auto& lib : libraries_)
        {
            if (lib.name != "User") continue;
            for (auto& p : lib.patches)
            {
                juce::DynamicObject::Ptr o = new juce::DynamicObject();
                o->setProperty("name",  p.name);
                o->setProperty("state", p.state.toXmlString());
                dir.getChildFile(p.name + ".json")
                   .replaceWithText(juce::JSON::toString(juce::var(o.get())));
            }
        }
    }
};

} // namespace ABD
