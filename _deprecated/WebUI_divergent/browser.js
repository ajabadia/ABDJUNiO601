/* browser.js - Advanced Preset Browser Logic for JUNiO 601 */
const PresetBrowser = {
    data: { libraries: [] },
    selectedLibIdx: 0,
    selectedPresetIdx: -1,
    currentCategory: 'All',
    searchQuery: '',
    pendingSaveAsName: null,

    async init() {
        console.log("PresetBrowser: Initializing...");

        // 1. Setup listeners FIRST so buttons work even if data fetch hangs
        this.setupListeners();

        try {
            // 2. Initial load from bridge
            const response = await juce.getBrowserData();
            if (response) {
                this.data = response;
                this.render();
            }
        } catch (e) {
            console.error("PresetBrowser: Failed to load data", e);
        }
    },

    setupListeners() {
        // [New] Reactive updates from bridge
        if (window.listenEvent) {
            window.listenEvent("onPresetListUpdate", async (data) => {
                console.log("PresetBrowser: Received Reactive Update", data);

                // CASE 1: Intentional Switch (Save As / Duplicate)
                if (this.pendingUserLibrarySwitch || this.pendingSaveAsName) {
                    // Always re-fetch data to ensure we have the latest state after a save/duplicate
                    const response = await juce.getBrowserData();
                    if (response) this.data = response;

                    if (this.pendingSaveAsName) {
                        console.log("[JUNiO] Auto-selecting new preset:", this.pendingSaveAsName);
                        // Find User library
                        const userIdx = this.data.libraries.findIndex(l => l.name.toUpperCase() === "USER");
                        if (userIdx >= 0) {
                            this.selectedLibIdx = userIdx;
                            const prstIdx = this.data.libraries[userIdx].patches.findIndex(p => p.name === this.pendingSaveAsName);
                            if (prstIdx >= 0) {
                                this.selectedPresetIdx = prstIdx;
                                // Reset pending state
                                this.pendingSaveAsName = null;
                                this.pendingUserLibrarySwitch = false;

                                // Ensure User category is selected if necessary
                                this.currentCategory = "User";
                            }
                        }
                    } else if (this.pendingUserLibrarySwitch) {
                        const userIdx = this.data.libraries.findIndex(l => l.name.toUpperCase() === "USER");
                        if (userIdx >= 0) this.selectedLibIdx = userIdx;
                        this.pendingUserLibrarySwitch = false;
                    } else {
                        // Update index if name still matches to avoid "jump"
                        const currentLib = this.data.libraries[this.selectedLibIdx];
                        if (currentLib && this.selectedPresetIdx >= 0 && data.currentPresetIndex !== undefined) {
                            // Optional: Sync if engine changed slot
                            // this.selectedPresetIdx = data.currentPresetIndex;
                        }
                    }

                    this.render();

                    // Final Polish: Scroll to selection
                    setTimeout(() => {
                        const active = document.querySelector('.preset-item.active');
                        if (active) active.scrollIntoView({ behavior: 'smooth', block: 'nearest' });
                    }, 100);
                    return;
                }

                // CASE 2: Passive Background Update (Parameter change or manual engine bank switch)
                if (data.items && data.currentLibrary) {
                    // Find the library by name instead of assuming it's the current selection
                    const targetLibIdx = this.data.libraries.findIndex(l => l.name === data.currentLibrary);
                    if (targetLibIdx >= 0) {
                        const lib = this.data.libraries[targetLibIdx];
                        lib.patches = data.items;

                        // Only update visual selection if THIS library is the one being viewed
                        if (targetLibIdx === this.selectedLibIdx) {
                            if (typeof data.currentPresetIndex === 'number')
                                this.selectedPresetIdx = data.currentPresetIndex;
                            this.renderPresets();
                            this.updateButtons();
                        }
                    }
                }
            });

            window.listenEvent("onImportResult", async (data) => {
                console.log("PresetBrowser: Received Import Result", data);
                if (data && data.message) {
                    showNotification(data.message, data.success ? 'success' : 'error');
                }
                if (data && data.success) {
                    const response = await juce.getBrowserData();
                    if (response) {
                        this.data = response;
                        this.render();
                    }
                }
            });
        }
        const search = document.getElementById('browser-search');
        if (search) {
            search.oninput = (e) => {
                this.searchQuery = e.target.value.toLowerCase();
                this.renderPresets();
            };
        }

        // Setup individual list listeners
        document.getElementById('cat-list').onkeydown = (e) => this.handleListKeydown(e, 'category');
        document.getElementById('lib-list').onkeydown = (e) => this.handleListKeydown(e, 'library');
        document.getElementById('preset-list').onkeydown = (e) => this.handleListKeydown(e, 'preset');

        // CRUD Buttons
        const attach = (id, fn) => {
            const el = document.getElementById(id);
            if (el) el.onclick = fn;
        };

        attach('lib-edit-btn', () => this.editLibrary());
        attach('lib-delete-btn', () => this.deleteLibrary());
        attach('preset-add-btn', () => this.addPreset());
        attach('preset-dup-btn', () => this.duplicatePreset());
        attach('preset-move-btn', () => this.movePresetToLibrary());
        attach('preset-delete-btn', () => this.deletePreset());
        attach('preset-write-btn', () => this.saveSynthToPreset());
        attach('preset-saveas-btn', () => this.showSaveAsModal());

        const updateBtn = document.querySelector('.meta-actions button[onclick*="applyMetadata"]');
        if (updateBtn) updateBtn.onclick = () => this.applyMetadata();
    },

    // --- Persistence ---
    async saveData() {
        try {
            await juce.setBrowserData(this.data);
            console.log("PresetBrowser: Data saved successfully");
        } catch (e) {
            console.error("PresetBrowser: Failed to save data", e);
            alert("Error saving data to engine.");
        }
    },

    // --- Category CRUD ---
    addCategory() {
        const name = prompt("Enter new category name:");
        if (!name || name.trim() === "") return;
        const cat = name.trim();
        if (["All", "Factory", "User", "WIP", "Favorites"].includes(cat)) {
            alert("Invalid category name (reserved).");
            return;
        }
        // Categories are derived from library metadata in our data model
        // To "add" a category, we might need a special list if it's not dynamic
        // But the user wants to govern them. I'll add a 'categories' array to data if not present
        if (!this.data.categories) this.data.categories = [];
        if (!this.data.categories.includes(cat)) {
            this.data.categories.push(cat);
            this.saveData();
            this.render();
        }
    },

    editCategory() {
        if (["All", "Factory", "User", "WIP", "Favorites"].includes(this.currentCategory)) {
            alert("Cannot rename system categories.");
            return;
        }
        const newName = prompt("Rename category:", this.currentCategory);
        if (!newName || newName.trim() === "" || newName === this.currentCategory) return;

        const oldName = this.currentCategory;
        const updatedName = newName.trim();

        // Update in categories list
        if (this.data.categories) {
            const idx = this.data.categories.indexOf(oldName);
            if (idx >= 0) this.data.categories[idx] = updatedName;
        }

        // Update all libraries using this category
        this.data.libraries.forEach(lib => {
            if (lib.category === oldName) lib.category = updatedName;
        });

        this.currentCategory = updatedName;
        this.saveData();
        this.render();
    },

    deleteCategory() {
        if (["All", "Factory", "User", "WIP", "Favorites"].includes(this.currentCategory)) {
            alert("Cannot delete system categories.");
            return;
        }
        // Check if has libraries
        const hasLibs = this.data.libraries.some(lib => lib.category === this.currentCategory);
        if (hasLibs) {
            alert("Cannot delete category containing libraries. Move or delete libraries first.");
            return;
        }

        if (confirm(`Delete category "${this.currentCategory}"?`)) {
            if (this.data.categories) {
                this.data.categories = this.data.categories.filter(c => c !== this.currentCategory);
            }
            this.currentCategory = "All";
            this.saveData();
            this.render();
        }
    },

    // --- Library CRUD ---
    addLibrary() {
        let cat = this.currentCategory;
        if (cat === "All" || cat === "Factory" || cat === "Favorites") cat = "User";

        const name = prompt(`Enter new library name for category "${cat}":`);
        if (!name || name.trim() === "") return;

        this.data.libraries.push({
            name: name.trim(),
            category: cat,
            patches: []
        });

        this.selectedLibIdx = this.data.libraries.length - 1;
        this.selectedPresetIdx = -1;
        this.saveData();
        this.render();
    },

    editLibrary() {
        const lib = this.data.libraries[this.selectedLibIdx];
        if (!lib) return;
        if (lib.name.toUpperCase() === "FACTORY") {
            alert("Cannot edit Factory library.");
            return;
        }

        const newName = prompt("Rename library:", lib.name);
        if (!newName || newName.trim() === "" || newName === lib.name) return;

        lib.name = newName.trim();
        this.saveData();
        this.render();
    },

    deleteLibrary() {
        const lib = this.data.libraries[this.selectedLibIdx];
        if (!lib) return;
        if (lib.name.toUpperCase() === "FACTORY") {
            alert("Cannot delete Factory library.");
            return;
        }
        if (lib.patches && lib.patches.length > 0) {
            alert("Cannot delete library containing presets. Delete presets first.");
            return;
        }

        if (confirm(`Delete library "${lib.name}"?`)) {
            this.data.libraries.splice(this.selectedLibIdx, 1);
            this.selectedLibIdx = 0;
            this.selectedPresetIdx = -1;
            this.saveData();
            this.render();
        }
    },

    // --- Preset CRUD ---
    addPreset() {
        const lib = this.data.libraries[this.selectedLibIdx];
        if (!lib) return;
        if (lib.name.toUpperCase() === "FACTORY") {
            alert("Cannot add presets to Factory library. Create a User library instead.");
            return;
        }

        const name = prompt("Enter new preset name:");
        if (!name || name.trim() === "") return;

        lib.patches.push({
            name: name.trim(),
            author: "",
            category: "User",
            tags: [],
            notes: "",
            favorite: false,
            data: {} // Empty state or current synth state? User said "Duplicate" and "Save state" 
            // so maybe this is just a blank INIT preset.
        });

        this.selectedPresetIdx = lib.patches.length - 1;
        this.saveData();
        this.render();
    },

    duplicatePreset() {
        const lib = this.data.libraries[this.selectedLibIdx];
        if (this.selectedPresetIdx < 0 || !lib) return;

        const source = lib.patches[this.selectedPresetIdx];
        if (!source) return;

        // Factory protection: duplication is allowed, but must result in a user preset 
        // if the target library were to be changed. But here we duplicate in-place.
        // Wait, if we are in Factory, we should maybe duplicate to 'User' library instead?
        // User said: "los de fábrica no se pueden borrar, ni salvar sus modificaciones... si se salva tendrá que ser como un preset de usuario"

        let targetLib = lib;
        if (lib.name.toUpperCase() === "FACTORY") {
            // Find or create 'User' library
            let userLibIdx = this.data.libraries.findIndex(l => l.name.toUpperCase() === "USER");
            if (userLibIdx < 0) {
                this.data.libraries.push({ name: "User", category: "User", patches: [] });
                userLibIdx = this.data.libraries.length - 1;
            }
            targetLib = this.data.libraries[userLibIdx];
            this.selectedLibIdx = userLibIdx;
        }

        const copy = JSON.parse(JSON.stringify(source));
        copy.name += " Copy";
        targetLib.patches.push(copy);

        this.selectedPresetIdx = targetLib.patches.length - 1;
        this.saveData();
        this.render();
    },

    deletePreset() {
        const lib = this.data.libraries[this.selectedLibIdx];
        if (this.selectedPresetIdx < 0 || !lib) return;
        if (lib.name.toUpperCase() === "FACTORY") {
            alert("Cannot delete Factory presets.");
            return;
        }

        if (confirm(`Delete preset "${lib.patches[this.selectedPresetIdx].name}"?`)) {
            lib.patches.splice(this.selectedPresetIdx, 1);
            this.selectedPresetIdx = -1;
            this.saveData();
            this.render();
        }
    },

    async saveSynthToPreset() {
        const lib = this.data.libraries[this.selectedLibIdx];
        if (this.selectedPresetIdx < 0 || !lib) return;

        // Unified Logic: If we are in Factory, redirects to Save As
        if (lib.name.toUpperCase() === "FACTORY") {
            this.showSaveAsModal();
            return;
        }

        try {
            console.log("[JUNiO] Saving Current Sound (Overwrite)...");
            if (window.juce && juce.savePresetDetailed) {
                await juce.savePresetDetailed(this.selectedLibIdx, this.selectedPresetIdx);
                updateLCD("SOUND OVERWRITTEN", true);
                alert("SOUND SAVED: Synthesizer knob positions have been written into the current slot.");
            } else {
                await juce.menuAction('handleSavePreset'); // Fallback
                updateLCD("SOUND SAVED", true);
            }
        } catch (e) {
            console.error("Save failed", e);
            alert("SAVE FAILED: Could not persist sound to engine.");
        }
    },

    showSaveAsModal: async function () {
        let currentName = "New Patch";
        const nameEl = document.getElementById('meta-name');
        if (nameEl) currentName = nameEl.value || "New Patch";

        const input = document.getElementById('saveas-name-input');
        if (input) input.value = currentName + " Copy";

        const modal = document.getElementById('modal-saveas');
        if (modal) {
            console.log("[JUNiO] Opening SaveAs Modal via Style Display");
            modal.style.display = 'flex';
            modal.classList.remove('hidden'); // Coverage
            if (input) setTimeout(() => input.focus(), 50);
        } else {
            console.error("[JUNiO] SaveAs Modal NOT FOUND in DOM");
        }
    },

    closeSaveAsModal: function () {
        const modal = document.getElementById('modal-saveas');
        if (modal) {
            modal.style.display = 'none';
            modal.classList.add('hidden'); // Coverage
        }
    },

    confirmSaveAs: function () {
        const nameInput = document.getElementById('saveas-name-input');
        const name = nameInput ? nameInput.value : "";
        if (name && name.trim().length > 0) {
            const finalName = name.trim();
            console.log("[JUNiO] Saving As:", finalName);

            // Capture current metadata state for the new preset
            const author = document.getElementById('meta-author') ? document.getElementById('meta-author').value : "";
            const tags = document.getElementById('meta-tags') ? document.getElementById('meta-tags').value : "";
            const notes = document.getElementById('meta-notes') ? document.getElementById('meta-notes').value : "";
            const category = document.getElementById('meta-category') ? document.getElementById('meta-category').value : "User";

            // Set state variables BEFORE calling native
            this.pendingSaveAsName = finalName;
            this.pendingUserLibrarySwitch = true;

            if (window.juce && juce.saveAsNewPresetDetailed) {
                juce.saveAsNewPresetDetailed(finalName, category, author, tags, notes);
                updateLCD("NEW PRESET CREATED", true);
                alert("SUCCESS: New preset '" + finalName + "' added to User library.");
            } else {
                juce.menuAction('handleSavePresetAs', finalName);
            }
            this.closeSaveAsModal();
        }
    },

    // --- File Operations ---
    async exportLibrary() {
        const lib = this.data.libraries[this.selectedLibIdx];
        if (!lib) return;
        try {
            await juce.exportBank(lib); // Pass the whole lib object to C++
        } catch (e) {
            console.error("Export failed", e);
        }
    },

    async importLibrary() {
        try {
            const newLib = await juce.importBank(); // Expects C++ to return a library object
            if (newLib) {
                this.data.libraries.push(newLib);
                this.selectedLibIdx = this.data.libraries.length - 1;
                this.saveData();
                this.render();
            }
        } catch (e) {
            console.error("Import failed", e);
        }
    },

    async saveTape() {
        const lib = this.data.libraries[this.selectedLibIdx];
        if (!lib) return;
        try {
            await juce.menuAction('handleSaveTape');
            updateLCD("TAPE SAVED", true);
        } catch (e) { console.error("Save Tape failed", e); }
    },

    async loadTape() {
        const lib = this.data.libraries[this.selectedLibIdx];
        if (!lib) return;
        try {
            await juce.menuAction('handleLoadTape');
            this.init(); // Refresh all data as tape might have loaded a whole bank
        } catch (e) { console.error("Load Tape failed", e); }
    },

    // --- Advanced Preset Operations ---
    movePreset(dir) {
        const lib = this.data.libraries[this.selectedLibIdx];
        if (this.selectedPresetIdx < 0 || !lib) return;
        if (lib.name.toUpperCase() === "FACTORY") return; // Protection

        const targetIdx = this.selectedPresetIdx + dir;
        if (targetIdx < 0 || targetIdx >= lib.patches.length) return;

        // Swap
        const temp = lib.patches[this.selectedPresetIdx];
        lib.patches[this.selectedPresetIdx] = lib.patches[targetIdx];
        lib.patches[targetIdx] = temp;

        this.selectedPresetIdx = targetIdx;
        this.saveData();
        this.render();
    },

    movePresetToLibrary() {
        const lib = this.data.libraries[this.selectedLibIdx];
        if (this.selectedPresetIdx < 0 || !lib) return;

        // Factory protection: moving FROM factory is allowed (it will copy/move), 
        // but it's cleaner to see it as a move to another bank.
        // User said: "Los de fábrica no se pueden borrar". Moving away from factory 
        // would technically leave a gap? Better if moving FROM factory is a COPY.
        // But if we are in USER bank, it's a real MOVE.

        const targetLibName = prompt("Enter target library name (Exact match):");
        if (!targetLibName) return;

        const targetLibIdx = this.data.libraries.findIndex(l => l.name.toLowerCase() === targetLibName.toLowerCase());
        if (targetLibIdx < 0) {
            alert("Target library not found.");
            return;
        }
        if (this.data.libraries[targetLibIdx].name.toUpperCase() === "FACTORY") {
            alert("Cannot move presets TO Factory library.");
            return;
        }

        const preset = lib.patches[this.selectedPresetIdx];

        // If from Factory, it's a Copy. If from User, it's a Move.
        if (lib.name.toUpperCase() === "FACTORY") {
            this.data.libraries[targetLibIdx].patches.push(JSON.parse(JSON.stringify(preset)));
        } else {
            this.data.libraries[targetLibIdx].patches.push(preset);
            lib.patches.splice(this.selectedPresetIdx, 1);
            this.selectedPresetIdx = -1;
        }

        this.selectedLibIdx = targetLibIdx;
        this.saveData();
        this.render();
    },

    handleListKeydown(e, type) {
        if (e.key !== 'ArrowDown' && e.key !== 'ArrowUp') return;
        e.preventDefault();

        if (type === 'category') {
            const items = Array.from(document.querySelectorAll('#cat-list li'));
            const currentIdx = items.findIndex(li => li.classList.contains('active'));
            let nextIdx = currentIdx + (e.key === 'ArrowDown' ? 1 : -1);
            if (nextIdx >= 0 && nextIdx < items.length) {
                items[nextIdx].click();
                // Find the fresh .active element after render
                setTimeout(() => {
                    document.querySelector('#cat-list .active')?.scrollIntoView({ block: 'nearest' });
                }, 0);
            }
        } else if (type === 'library') {
            const items = Array.from(document.querySelectorAll('#lib-list li'));
            const currentIdx = items.findIndex(li => li.classList.contains('active'));
            let nextIdx = currentIdx + (e.key === 'ArrowDown' ? 1 : -1);
            if (nextIdx >= 0 && nextIdx < items.length) {
                items[nextIdx].click();
                setTimeout(() => {
                    document.querySelector('#lib-list .active')?.scrollIntoView({ block: 'nearest' });
                }, 0);
            }
        } else if (type === 'preset') {
            const items = Array.from(document.querySelectorAll('#preset-list li'));
            const currentIdx = items.findIndex(li => li.classList.contains('active'));
            let nextIdx = currentIdx + (e.key === 'ArrowDown' ? 1 : -1);
            if (nextIdx >= 0 && nextIdx < items.length) {
                items[nextIdx].click();
                setTimeout(() => {
                    document.querySelector('#preset-list .active')?.scrollIntoView({ block: 'nearest' });
                }, 0);
            }
        }
    },

    render() {
        this.renderCategories();
        this.renderLibraries();
        this.renderPresets();
        this.updateButtons(); // New method to update button visibility
    },

    renderCategories() {
        const list = document.getElementById('cat-list');
        if (!list) return;

        // System categories
        const system = ["All", "RAM", "Factory", "User", "WIP", "Favorites"];
        const custom = this.data.categories || [];
        const seen = new Set();

        let html = '';
        [...system, ...custom].forEach(cat => {
            if (seen.has(cat)) return;
            seen.add(cat);
            const active = this.currentCategory === cat ? 'active' : '';
            html += `<li class="${active}" data-cat="${cat}" onclick="PresetBrowser.selectCategory('${cat}')">${cat}</li>`;
        });
        list.innerHTML = html;
    },

    selectCategory(cat) {
        this.currentCategory = cat;
        // Reset selection to first valid library for this category
        this.selectedLibIdx = 0;
        if (this.currentCategory !== 'All') {
            const firstVisible = this.data.libraries.findIndex(lib => {
                const name = lib.name.toUpperCase();
                if (this.currentCategory === 'Factory') return name.includes('FACTORY');
                if (this.currentCategory === 'RAM') return name.includes('INTERNAL RAM') || name.includes('RAM');
                if (this.currentCategory === 'User') return name.includes('USER') || (!name.includes('FACTORY') && !name.includes('RAM'));
                if (this.currentCategory === 'WIP') return name.includes('WIP');
                if (this.currentCategory === 'Favorites') return lib.patches.some(p => p.favorite);
                return lib.category === this.currentCategory;
            });
            if (firstVisible >= 0) this.selectedLibIdx = firstVisible;
        }
        this.render();
    },

    async selectLib(idx) {
        this.selectedLibIdx = idx;
        this.selectedPresetIdx = -1;

        // Smart category handling: 
        // If we were in Factory category and picked a non-factory lib, reset to All.
        const lib = this.data.libraries[idx];
        if (lib) {
            const name = lib.name.toUpperCase();
            const isFact = name.includes('FACTORY');
            const isUser = name.includes('USER') || (!name.includes('FACTORY') && !name.includes('RAM'));
            if (this.currentCategory === 'Factory' && !isFact) this.currentCategory = 'All';
            if (this.currentCategory === 'User' && !isUser) this.currentCategory = 'All';
        }

        try {
            if (window.juce) await juce.selectLibrary(idx);
        } catch (e) { console.warn("Native selectLibrary fails", e); }
        this.render();
    },

    renderLibraries() {
        const list = document.getElementById('lib-list');
        if (!list) return;
        list.innerHTML = '';

        this.data.libraries.forEach((lib, idx) => {
            // Apply Category Filter to Libraries Column
            let shouldShow = true;
            const name = lib.name.toUpperCase();
            if (this.currentCategory === 'Factory') shouldShow = name.includes('FACTORY');
            else if (this.currentCategory === 'RAM') shouldShow = name.includes('INTERNAL RAM') || name.includes('RAM');
            else if (this.currentCategory === 'User') shouldShow = name.includes('USER') || (!name.includes('FACTORY') && !name.includes('RAM'));
            else if (this.currentCategory === 'WIP') shouldShow = name.includes('WIP');
            else if (this.currentCategory === 'Favorites') {
                shouldShow = lib.patches.some(p => p.favorite);
            } else if (this.currentCategory !== 'All') {
                shouldShow = lib.category === this.currentCategory;
            }

            if (!shouldShow) return;

            const li = document.createElement('li');
            li.innerHTML = `<span>${lib.name}</span> <small style="opacity:0.5; font-size:9px;">${lib.category || ''}</small>`;
            if (this.selectedLibIdx === idx) li.classList.add('active');
            li.onclick = () => this.selectLib(idx);
            list.appendChild(li);
        });
    },

    updateButtons() {
        const lib = this.data.libraries[this.selectedLibIdx];
        if (!lib) return;
        const isFactory = lib.name.toUpperCase() === "FACTORY";

        // Library buttons
        document.getElementById('lib-edit-btn').style.display = isFactory ? 'none' : 'block';
        document.getElementById('lib-delete-btn').style.display = isFactory ? 'none' : 'block';

        // Preset buttons
        document.getElementById('preset-add-btn').style.display = isFactory ? 'none' : 'block';
        document.getElementById('preset-delete-btn').style.display = isFactory ? 'none' : 'block';

        const writeBtn = document.getElementById('preset-write-btn');
        if (writeBtn) {
            writeBtn.style.display = isFactory ? 'none' : 'block';
            writeBtn.innerText = "SAVE CURRENT SOUND (WRITE)";
        }

        const countLabel = document.getElementById('preset-count-label');
        if (countLabel) {
            countLabel.innerText = `${lib.patches.length} Presets`;
        }

        // Lock metadata for factory
        const metaFields = ['meta-name', 'meta-author', 'meta-category', 'meta-tags', 'meta-notes'];
        metaFields.forEach(id => {
            const el = document.getElementById(id);
            if (el) el.readOnly = isFactory;
        });

        const saveAsBtn = document.getElementById('preset-saveas-btn');
        if (saveAsBtn) saveAsBtn.style.display = 'block';

        const updateBtn = document.querySelector('.meta-actions .footer-action-btn:not(.primary):not(#preset-saveas-btn)');
        if (updateBtn) updateBtn.style.display = isFactory ? 'none' : 'block';
    },

    renderPresets() {
        const list = document.getElementById('preset-list');
        if (!list) return;
        list.innerHTML = '';

        const lib = this.data.libraries[this.selectedLibIdx];
        if (!lib) return;

        lib.patches.forEach((p, idx) => {
            // Apply filtering
            const tagsStr = Array.isArray(p.tags) ? p.tags.join(', ') : (p.tags || '');
            const matchesSearch = !this.searchQuery ||
                p.name.toLowerCase().includes(this.searchQuery) ||
                (p.author && p.author.toLowerCase().includes(this.searchQuery)) ||
                tagsStr.toLowerCase().includes(this.searchQuery);

            let matchesCat = true;
            const libName = (lib.name || "").toUpperCase();

            if (this.currentCategory === 'Favorites') matchesCat = !!p.favorite;
            else if (this.currentCategory === 'WIP') matchesCat = libName.includes('WIP');
            else if (this.currentCategory === 'User') {
                matchesCat = (p.category === 'User') || (!libName.includes('FACTORY') && !libName.includes('RAM') && !libName.includes('WIP'));
            }
            else if (this.currentCategory === 'RAM') {
                matchesCat = (p.category === 'RAM') || libName.includes('RAM');
            }
            else if (this.currentCategory === 'Factory') {
                matchesCat = (p.category === 'Factory') || libName.includes('FACTORY');
            }
            else if (this.currentCategory !== 'All') {
                matchesCat = p.category === this.currentCategory;
            }

            // SPECIAL OVERRIDE: If the user manually selected a bank in the middle pane,
            // we should show everything in that bank unless they are specifically searching.
            // (Disabled for now to follow system categories, but making 'User' category smarter fixed it)

            if (!matchesSearch || !matchesCat) return;

            const li = document.createElement('li');
            li.className = 'preset-item';
            if (this.selectedPresetIdx === idx) li.classList.add('active');

            const leftSide = document.createElement('div');
            leftSide.className = 'preset-item-left';

            const nameSpan = document.createElement('span');
            nameSpan.className = 'preset-name';
            nameSpan.textContent = p.name;
            leftSide.appendChild(nameSpan);

            // [New] Origin Badge
            if (typeof p.originGroup === 'number' && p.originGroup >= 0) {
                const groupLetter = String.fromCharCode('A'.charCodeAt(0) + p.originGroup);
                const badge = document.createElement('span');
                badge.className = 'preset-origin-badge';
                badge.textContent = `${groupLetter}-${p.originBank}-${p.originPatch}`;
                leftSide.appendChild(badge);
            }

            const favBtn = document.createElement('span');
            favBtn.className = `preset-fav-btn ${p.favorite ? 'active' : ''}`;
            favBtn.textContent = '★';
            favBtn.onclick = (e) => this.toggleFavorite(e, this.selectedLibIdx, idx);

            li.appendChild(leftSide);
            li.appendChild(favBtn);

            li.onclick = (e) => {
                if (e.target.classList.contains('preset-fav-btn')) return;
                this.selectPreset(idx);
            };

            list.appendChild(li);
        });
    },

    selectPreset(idx) {
        this.selectedPresetIdx = idx;
        const lib = this.data.libraries[this.selectedLibIdx];
        const p = lib.patches[idx];

        // Load in engine using dual (Library, Preset) indices to avoid global 0-127 mismatch
        if (window.juce && juce.loadLibraryPreset) {
            juce.loadLibraryPreset(this.selectedLibIdx, idx);
        } else {
            juce.loadPreset(idx); // Fallback
        }

        // Update Info Pane
        document.getElementById('meta-name').value = p.name;
        document.getElementById('meta-author').value = p.author || '';
        document.getElementById('meta-category').value = p.category || 'Bass';
        document.getElementById('meta-tags').value = p.tags || '';
        document.getElementById('meta-notes').value = p.notes || '';
        const dateVal = p.date && p.date !== '--/--/----' ? p.date : new Date().toLocaleDateString();
        document.getElementById('meta-date').innerText = "Created: " + dateVal;

        this.renderPresets();
    },

    async toggleFavorite(event, libIdx, prstIdx) {
        event.stopPropagation();
        const p = this.data.libraries[libIdx].patches[prstIdx];
        p.favorite = !p.favorite;
        try {
            await juce.setFavorite(libIdx, prstIdx, p.favorite);
            this.renderPresets();
        } catch (e) {
            console.error("Failed to set favorite", e);
        }
    },

    async applyMetadata() {
        const lib = this.data.libraries[this.selectedLibIdx];
        if (!lib || this.selectedPresetIdx < 0 || lib.name.toUpperCase() === "FACTORY") return;

        const nameInput = document.getElementById('meta-name');
        const name = nameInput ? nameInput.value.trim() : "";
        const author = document.getElementById('meta-author') ? document.getElementById('meta-author').value : "";
        const cat = document.getElementById('meta-category') ? document.getElementById('meta-category').value : "User";
        const tags = document.getElementById('meta-tags') ? document.getElementById('meta-tags').value : "";
        const notes = document.getElementById('meta-notes') ? document.getElementById('meta-notes').value : "";

        try {
            const p = lib.patches[this.selectedPresetIdx];
            if (name) p.name = name;
            p.author = author;
            p.tags = tags;
            p.notes = notes;
            p.category = cat;

            await juce.updateMetadata(this.selectedLibIdx, this.selectedPresetIdx, name, author, tags, notes);

            // Refresh UI list to show new name immediately
            this.render();
            updateLCD("METADATA SAVED", true);
            alert("METADATA UPDATED: Only text information (Name, Author, Tags) has been updated. The sound remains unchanged.");
        } catch (e) {
            console.error("Metadata save failed", e);
            alert("METADATA FAILED: Could not update text info.");
        }
    },

    async switchAB(slot) {
        await juce.switchAB(slot);
        document.getElementById('btn-slot-a').classList.toggle('active', slot === 0);
        document.getElementById('btn-slot-b').classList.toggle('active', slot === 1);
        updateLCD("SLOT " + (slot === 0 ? "A" : "B") + " ACTIVE", true);
    },

    async copyAB() {
        await juce.copyAB();
        updateLCD("COPIED TO ALTERNATE", true);
    }
};

window.PresetBrowser = PresetBrowser;
