/**
 * modals.js — Large modal templates extracted from index.html
 *
 * These modals are injected synchronously before other scripts (service.js,
 * browser.js) load, ensuring all DOM elements exist when referenced via
 * document.getElementById().
 */
// ========================================
// MODAL: Global Settings / Calibration / Routing / Diagnostics
// ========================================
const MODAL_SETTINGS = `
        <div class="settings-container">
            <div class="settings-header">
                <span class="settings-title">SYSTEM SETTINGS</span>
                <button class="close-btn" onclick="hideGlobalSettings()">&times;</button>
            </div>

            <div class="settings-tabs">
                <button class="tab-btn active" onclick="switchTab('general')">GENERAL</button>
                <button class="tab-btn" onclick="switchTab('calibration')">CALIBRATION</button>
                <button class="tab-btn" id="tab-routing" onclick="switchTab('routing')">ROUTING</button>
                <button class="tab-btn" onclick="switchTab('diagnostics')">DIAGNOSTICS</button>
            </div>

            <div class="settings-inner">
                <div id="tab-general" class="settings-content active single-col">
                    <div id="general-params-list" class="service-param-list styled-scroll"></div>

                    <div class="service-section" style="margin-top: 25px; border-top: 1px solid rgba(255,255,255,0.1); padding-top: 20px;">
                        <div class="service-cat-header"><span>MIDI / Voice / Pedal / Paths</span></div>
                        <div class="service-param-grid" style="grid-template-columns: 1fr;">
                            <div class="setting-row" style="padding: 10px;">
                                <label>MIDI Channel</label>
                                <select data-param="midiChannel" onchange="updatePref(this)">
                                    <option value="0">1</option>
                                    <option value="0.067">2</option>
                                    <option value="0.133">3</option>
                                    <option value="0.2">4</option>
                                    <option value="0.267">5</option>
                                    <option value="0.333">6</option>
                                    <option value="0.4">7</option>
                                    <option value="0.467">8</option>
                                    <option value="0.533">9</option>
                                    <option value="0.6">10</option>
                                    <option value="0.667">11</option>
                                    <option value="0.733">12</option>
                                    <option value="0.8">13</option>
                                    <option value="0.867">14</option>
                                    <option value="0.933">15</option>
                                    <option value="1.0">16</option>
                                </select>
                                <div class="val-lbl">CH</div>
                            </div>
                            <div class="setting-row" style="padding: 10px;" id="setting-numVoices-row">
                                <label>Voice Count</label>
                                <select data-param="numVoices" onchange="updatePref(this)">
                                    <option value="1">1 (MONO)</option>
                                    <option value="2">2</option>
                                    <option value="4">4</option>
                                    <option value="6">6 (CLASSIC)</option>
                                    <option value="8">8</option>
                                    <option value="12">12</option>
                                    <option value="16">16 (MAX)</option>
                                </select>
                                <div class="val-lbl">VOICES</div>
                            </div>
                            <div class="setting-row" style="padding: 10px;">
                                <label>Bender Range</label>
                                <select data-param="benderRange" onchange="updatePref(this)">
                                    <option value="1">1 SEMI</option>
                                    <option value="2">2 SEMIS</option>
                                    <option value="3">3 SEMIS</option>
                                    <option value="4">4 SEMIS</option>
                                    <option value="7">5th</option>
                                    <option value="12">OCTAVE</option>
                                </select>
                                <div class="val-lbl">BEND</div>
                            </div>
                            <div class="setting-row" style="padding: 10px;">
                                <label>Velocity Sensitivity</label>
                                <input type="range" class="service-slider" data-param="velocitySens" min="0" max="1" step="0.01" value="0.5" oninput="updatePref(this); document.getElementById('val-velocitySens').innerText = Math.round(parseFloat(this.value) * 100) + '%'">
                                <div class="val-lbl" id="val-velocitySens">50%</div>
                            </div>
                            <div class="setting-row" style="padding: 10px;">
                                <label>Invert Sustain Pedal</label>
                                <select data-param="sustainPedalInvert" onchange="updatePref(this)">
                                    <option value="0">OFF (Standard Polarity)</option>
                                    <option value="1">ON (Inverted Polarity)</option>
                                </select>
                                <div class="val-lbl">PEDAL</div>
                            </div>
                            <div class="setting-row" style="padding: 10px;">
                                <label>Preset Library Path</label>
                                <div style="display: flex; gap: 8px; flex: 1; align-items: center;">
                                    <input type="text" id="setting-libraryPath" style="flex: 1; background: #111; border: 1px solid #333; color: #aaa; padding: 8px; border-radius: 4px; font-family: 'Courier New', monospace; font-size: 10px;" placeholder="Documents/ABD JUNiO 601/Presets..." onchange="juce.setLibraryPath(this.value)">
                                    <button class="tuning-btn" onclick="browseLibraryPath()" style="padding: 6px 14px; white-space: nowrap; background: #347; color: #fff;">BROWSE...</button>
                                </div>
                                <div class="val-lbl">PATH</div>
                            </div>
                        </div>
                    </div>

                    <div class="service-section" style="margin-top: 25px; border-top: 1px solid rgba(255,255,255,0.1); padding-top: 20px;">
                        <div class="service-cat-header"><span>User Identity</span></div>
                        <div class="service-param-grid" style="grid-template-columns: 1fr;">
                            <div class="service-param-row" style="padding: 10px; display: flex; flex-direction: column; align-items: flex-start;">
                                <label style="font-size: 10px; color: #888; margin-bottom: 5px;">AUTHOR NAME (DEFAULT)</label>
                                <input type="text" id="setting-userName" maxlength="20" placeholder="Your Name..." style="width: 100%; background: #111; border: 1px solid #333; color: #eee; padding: 8px; border-radius: 4px; font-family: 'Inter', sans-serif;">
                            </div>
                        </div>
                    </div>

                    <div class="service-section" style="margin-top: 25px; border-top: 1px solid rgba(255,255,255,0.1); padding-top: 20px;">
                        <div class="service-cat-header"><span>Microtuning (SCL)</span></div>
                        <div class="service-param-grid" style="grid-template-columns: 1fr;">
                            <div class="service-param-row" style="padding: 10px;">
                                <div style="display: flex; gap: 10px; width: 100%;">
                                    <button class="tuning-btn" style="flex: 1;" onclick="handleTuningClick('load')">LOAD .SCL</button>
                                    <button class="tuning-btn" style="flex: 1;" onclick="handleTuningClick('reset')">RESET TUNING</button>
                                </div>
                                <div id="tuning-info" style="font-size: 11px; color: #888; text-align: center; margin-top: 8px; font-weight: bold; letter-spacing: 0.5px;">Standard Tuning</div>
                            </div>
                        </div>
                    </div>
                </div>

                <!-- TAB 2: CALIBRATION PARAMS -->
                <div id="tab-calibration" class="settings-content single-col">
                    <div style="background: rgba(255,255,255,0.02); border-bottom: 1px solid rgba(255,255,255,0.1); padding: 10px; display: flex; justify-content: space-between; align-items: center; margin-bottom: 15px;">
                        <span style="font-size: 11px; font-weight: bold; color: #aaa; text-transform: uppercase; letter-spacing: 1px;">Calibration Parameters</span>
                        <label style="font-size: 10px; display: flex; align-items: center; gap: 8px; color: var(--juno-orange); cursor: pointer; user-select: none;">
                            <input type="checkbox" id="chk-show-all-params" onchange="ServiceMode.toggleShowAllParams(this.checked)" style="cursor: pointer; accent-color: var(--juno-orange);">
                            <span id="calibration-show-all-label">SHOW ALL (SUPER SIX MODE)</span>
                        </label>
                    </div>
                    <div id="service-params-list" style="padding-right: 15px;"></div>
                    <div class="service-section" style="margin-top: 15px; border-top: 1px solid rgba(255,255,255,0.1); padding-top: 15px;">
                         <button class="tuning-btn primary" id="btn-auto-tune-cal" onclick="ServiceMode.startAutoTune()" style="background: #a50; color: white;">RUN AUTO-VCF TUNE</button>
                    </div>

                    <!-- DAC Hz Table CSV Import/Export -->
                    <div class="service-section" style="margin-top: 25px; border-top: 1px solid rgba(255,255,255,0.1); padding-top: 20px;">
                        <div class="service-cat-header"><span>DAC Hz Table (J106DACHzTable)</span></div>
                        <div class="service-param-grid" style="grid-template-columns: 1fr;">
                            <div class="service-param-row" style="padding: 10px;">
                                <div style="display: flex; gap: 10px; width: 100%;">
                                    <button class="tuning-btn" style="flex: 1;" id="btn-import-dac" onclick="handleDacTableClick('import')">IMPORT DAC TABLE (.CSV)</button>
                                    <button class="tuning-btn" style="flex: 1;" id="btn-export-dac" onclick="handleDacTableClick('export')">EXPORT DAC TABLE (.CSV)</button>
                                </div>
                                <div id="dac-table-info" style="font-size: 11px; color: #888; text-align: center; margin-top: 8px; font-weight: bold; letter-spacing: 0.5px;">Default Factory Table (4096 values)</div>
                            </div>
                        </div>
                    </div>

                    <!-- VCA Gain Table CSV Import/Export -->
                    <div class="service-section" style="margin-top: 25px; border-top: 1px solid rgba(255,255,255,0.1); padding-top: 20px;">
                        <div class="service-cat-header"><span>VCA Gain Table (J106VCAGainTable)</span></div>
                        <div class="service-param-grid" style="grid-template-columns: 1fr;">
                            <div class="service-param-row" style="padding: 10px;">
                                <div style="display: flex; gap: 10px; width: 100%;">
                                    <button class="tuning-btn" style="flex: 1;" id="btn-import-vca" onclick="handleVcaTableClick('import')">IMPORT VCA TABLE (.CSV)</button>
                                    <button class="tuning-btn" style="flex: 1;" id="btn-export-vca" onclick="handleVcaTableClick('export')">EXPORT VCA TABLE (.CSV)</button>
                                </div>
                                <div id="vca-table-info" style="font-size: 11px; color: #888; text-align: center; margin-top: 8px; font-weight: bold; letter-spacing: 0.5px;">Default Factory Table (256 values)</div>
                            </div>
                        </div>
                    </div>

                    <!-- LFO Speed Table CSV Import/Export -->
                    <div class="service-section" style="margin-top: 25px; border-top: 1px solid rgba(255,255,255,0.1); padding-top: 20px;">
                        <div class="service-cat-header"><span>LFO Speed Table</span></div>
                        <div class="service-param-grid" style="grid-template-columns: 1fr;">
                            <div class="service-param-row" style="padding: 10px;">
                                <div style="display: flex; gap: 10px; width: 100%;">
                                    <button class="tuning-btn" style="flex: 1;" id="btn-import-lfo-speed" onclick="handleLfoSpeedTableClick('import')">IMPORT LFO SPEED TABLE (.CSV)</button>
                                    <button class="tuning-btn" style="flex: 1;" id="btn-export-lfo-speed" onclick="handleLfoSpeedTableClick('export')">EXPORT LFO SPEED TABLE (.CSV)</button>
                                </div>
                                <div id="lfo-speed-table-info" style="font-size: 11px; color: #888; text-align: center; margin-top: 8px; font-weight: bold; letter-spacing: 0.5px;">Default Factory Table (128 values)</div>
                            </div>
                        </div>
                    </div>

                    <!-- LFO Ramp Table CSV Import/Export -->
                    <div class="service-section" style="margin-top: 25px; border-top: 1px solid rgba(255,255,255,0.1); padding-top: 20px;">
                        <div class="service-cat-header"><span>LFO Ramp Table</span></div>
                        <div class="service-param-grid" style="grid-template-columns: 1fr;">
                            <div class="service-param-row" style="padding: 10px;">
                                <div style="display: flex; gap: 10px; width: 100%;">
                                    <button class="tuning-btn" style="flex: 1;" id="btn-import-lfo-ramp" onclick="handleLfoRampTableClick('import')">IMPORT LFO RAMP TABLE (.CSV)</button>
                                    <button class="tuning-btn" style="flex: 1;" id="btn-export-lfo-ramp" onclick="handleLfoRampTableClick('export')">EXPORT LFO RAMP TABLE (.CSV)</button>
                                </div>
                                <div id="lfo-ramp-table-info" style="font-size: 11px; color: #888; text-align: center; margin-top: 8px; font-weight: bold; letter-spacing: 0.5px;">Default Factory Table (8 values)</div>
                            </div>
                        </div>
                    </div>

                    <!-- Sub-Osc Level Curve CSV Import/Export -->
                    <div class="service-section" style="margin-top: 25px; border-top: 1px solid rgba(255,255,255,0.1); padding-top: 20px;">
                        <div class="service-cat-header"><span>Sub-Osc Level Curve</span></div>
                        <div class="service-param-grid" style="grid-template-columns: 1fr;">
                            <div class="service-param-row" style="padding: 10px;">
                                <div style="display: flex; gap: 10px; width: 100%;">
                                    <button class="tuning-btn" style="flex: 1;" id="btn-import-sub-level" onclick="handleSubLevelTableClick('import')">IMPORT SUB-OSC LEVEL TABLE (.CSV)</button>
                                    <button class="tuning-btn" style="flex: 1;" id="btn-export-sub-level" onclick="handleSubLevelTableClick('export')">EXPORT SUB-OSC LEVEL TABLE (.CSV)</button>
                                </div>
                                <div id="sub-level-table-info" style="font-size: 11px; color: #888; text-align: center; margin-top: 8px; font-weight: bold; letter-spacing: 0.5px;">Default Factory Table (11 values)</div>
                            </div>
                        </div>
                    </div>
                </div>

                <!-- TAB 3: ROUTING -->
                <div id="tab-routing" class="settings-content single-col">
                    <div class="service-section">
                        <div class="service-section-title">Engine Section Model Routing</div>

                        <div class="setting-row">
                            <label>DCO Model</label>
                            <select data-param="modelDCO" onchange="updatePref(this)">
                                <option value="0">Juno-6 (Classic Continuous DCO)</option>
                                <option value="0.5">Juno-60 (Standard DCO)</option>
                                <option value="1">Juno-106 (Hybrid DCO / Quartz Clock)</option>
                            </select>
                            <div class="val-lbl">DCO</div>
                        </div>

                        <div class="setting-row">
                            <label>HPF Model</label>
                            <select data-param="modelHPF" onchange="updatePref(this)">
                                <option value="0">Juno-6 (Continuous Slider HPF)</option>
                                <option value="0.5">Juno-60 (Discrete 4-Position HPF)</option>
                                <option value="1">Juno-106 (Discrete HPF with Active Bass Boost)</option>
                            </select>
                            <div class="val-lbl">HPF</div>
                        </div>

                        <div class="setting-row">
                            <label>VCF Model</label>
                            <select data-param="modelVCF" onchange="updatePref(this)">
                                <option value="0">Juno-6 (IR3109 ResK & SoftClip)</option>
                                <option value="0.5">Juno-60 (Classic IR3109 Filter)</option>
                                <option value="1">Juno-106 (80017A Integrated Filter)</option>
                            </select>
                            <div class="val-lbl">VCF</div>
                        </div>

                        <div class="setting-row">
                            <label>ADSR Model</label>
                            <select data-param="modelADSR" onchange="updatePref(this)">
                                <option value="0">Juno-6 (Analog RC Curves)</option>
                                <option value="0.5">Juno-60 (Analog RC Envelope)</option>
                                <option value="1">Juno-106 (Digital uPD7811G Linear Ticks)</option>
                            </select>
                            <div class="val-lbl">ADSR</div>
                        </div>

                        <div class="setting-row">
                            <label>Chorus Model</label>
                            <select data-param="modelChorus" onchange="updatePref(this)">
                                <option value="0">Juno-6 (Classic BBD Character)</option>
                                <option value="0.5">Juno-60 (Wide Analog MN3009 BBD)</option>
                                <option value="1">Juno-106 (Standard BBD Chorus & specific hiss)</option>
                            </select>
                            <div class="val-lbl">CHORUS</div>
                        </div>

                        <div class="setting-row">
                            <label>Arpeggiator Model</label>
                            <select data-param="modelArp" onchange="updatePref(this)">
                                <option value="0">Juno-6 (Analog Rate Osc & Schmitt Trigger)</option>
                                <option value="0.5">Juno-60 (Standard Sync Clock Arpeggiator)</option>
                                <option value="1">Juno-106 (No Arpeggiator / Disabled)</option>
                            </select>
                            <div class="val-lbl">ARP</div>
                        </div>

                        <div class="setting-row">
                            <label>Voice Allocator</label>
                            <select data-param="modelPoly" onchange="updatePref(this)">
                                <option value="0">Juno-6 (Rotary voicelimit direct)</option>
                                <option value="0.5">Juno-60 (Classic rotary allocator)</option>
                                <option value="1">Juno-106 (Digital allocator)</option>
                            </select>
                            <div class="val-lbl">POLY</div>
                        </div>

                        <div class="setting-row">
                            <label>Portamento Model</label>
                            <select data-param="modelPorta" onchange="updatePref(this)">
                                <option value="0">Juno-6 (No Portamento)</option>
                                <option value="0.5">Juno-60 (No Portamento)</option>
                                <option value="1">Juno-106 (Portamento Glide)</option>
                            </select>
                            <div class="val-lbl">PORTA</div>
                        </div>

                        <div class="setting-row">
                            <label>Unison Model</label>
                            <select data-param="modelUnison" onchange="updatePref(this)">
                                <option value="0">Juno-6 (No Unison)</option>
                                <option value="0.5">Juno-60 (No Unison)</option>
                                <option value="1">Juno-106 (Unison Mode)</option>
                            </select>
                            <div class="val-lbl">UNISON</div>
                        </div>
                    </div>

                    <div class="service-section" style="margin-top: 25px; border-top: 1px solid rgba(255,255,255,0.1); padding-top: 20px;">
                        <div class="service-section-title">Arpeggiator Settings</div>

                        <div class="setting-row">
                            <label>Arpeggiator Enable</label>
                            <select data-param="arpEnabled" onchange="updatePref(this)">
                                <option value="0">OFF</option>
                                <option value="1">ON</option>
                            </select>
                            <div class="val-lbl">ARP</div>
                        </div>

                        <div class="setting-row">
                            <label>Arp Mode</label>
                            <select data-param="arpMode" onchange="updatePref(this)">
                                <option value="0">UP</option>
                                <option value="0.5">DOWN</option>
                                <option value="1">UP & DOWN</option>
                            </select>
                            <div class="val-lbl">MODE</div>
                        </div>

                        <div class="setting-row">
                            <label>Arp Range</label>
                            <select data-param="arpRange" onchange="updatePref(this)">
                                <option value="0">1 OCTAVE</option>
                                <option value="0.5">2 OCTAVES</option>
                                <option value="1">3 OCTAVES</option>
                            </select>
                            <div class="val-lbl">RANGE</div>
                        </div>

                        <div class="setting-row">
                            <label>Arp Rate (Free Run)</label>
                            <input type="range" class="service-slider" data-param="arpRate" min="0" max="1" step="0.01" value="0.5" oninput="updatePref(this)">
                            <div class="val-lbl" id="val-arpRate">0.50</div>
                        </div>

                        <div class="setting-row">
                            <label>Arp Sync to Host</label>
                            <select data-param="arpSync" onchange="updatePref(this)">
                                <option value="0">OFF (Free Run)</option>
                                <option value="1">ON (Tempo Sync)</option>
                            </select>
                            <div class="val-lbl">SYNC</div>
                        </div>
                        <div class="setting-row" id="arp-bpm-row-settings" style="display: none;">
                            <label>Sync BPM</label>
                            <div id="arp-bpm-indicator-settings" style="color: var(--juno-orange, #fa0); font-family: 'Fragment Mono', monospace; font-size: 14px; font-weight: bold; letter-spacing: 1px;">120 BPM</div>
                            <div class="val-lbl">HOST</div>
                        </div>

                        <div class="setting-row">
                            <label>Arp Time Division</label>
                            <select data-param="arpDivision" onchange="updatePref(this)">
                                <option value="0">1/1 (Whole Note)</option>
                                <option value="0.125">1/2 (Half Note)</option>
                                <option value="0.25">1/2T (Half Triplet)</option>
                                <option value="0.375">1/4 (Quarter Note)</option>
                                <option value="0.5">1/4T (Quarter Triplet)</option>
                                <option value="0.625">1/8 (Eighth Note)</option>
                                <option value="0.75">1/8T (Eighth Triplet)</option>
                                <option value="0.875">1/16 (Sixteenth Note)</option>
                                <option value="1">1/16T (Sixteenth Triplet)</option>
                            </select>
                            <div class="val-lbl">DIV</div>
                        </div>
                    </div>
                </div>

                <!-- TAB 4: DIAGNOSTICS -->
                <div id="tab-diagnostics" class="settings-content single-col">
                    <div class="service-section">
                        <div class="service-section-title">Voice Diagnostics</div>
                        <div id="voice-test-grid" class="voice-test-grid"></div>
                    </div>
                    <div class="service-section" style="margin-top: 30px; display: flex; gap: 10px; flex-wrap: wrap;">
                         <div style="display: flex; flex-direction: column; gap: 8px;">
                            <button class="tuning-btn" id="btn-test-scale" onclick="ServiceMode.playTestScale()">PLAY TEST SCALE</button>
                            <button class="tuning-btn" id="btn-record-audio" onclick="ServiceMode.toggleRecord()">
                                <span class="record-dot"></span><span id="record-text">RECORD AUDIO</span>
                            </button>
                         </div>
                          <button class="tuning-btn" onclick="ServiceMode.startSweep()">VCF SWEEP</button>
                         <button class="tuning-btn" onclick="ServiceMode.exportCalibration()">EXPORT CALIBRATION JSON</button>
                         <button class="tuning-btn" onclick="ServiceMode.importCalibration()">IMPORT CALIBRATION JSON</button>
                    </div>
                </div>
            </div>

            <div class="settings-footer" style="gap: 15px; padding: 20px; border-top: 1px solid #222; background: rgba(0,0,0,0.2);">
                <button class="tuning-btn" style="max-width: 180px; background: #600;" onclick="ServiceMode.resetCalibration()">FACTORY RESET</button>
                <button class="tuning-btn" style="max-width: 140px; background: #333; font-size: 14px;" onclick="hideGlobalSettings()">CLOSE</button>
            </div>
        </div>
`;

// ========================================
// MODAL: Preset Browser
// ========================================
const MODAL_BROWSER = `
        <div class="browser-container">
            <div class="browser-header">
                <div class="browser-title-zone">
                    <span class="browser-title">PRESET BROWSER</span>
                    <div id="browser-status-badges">
                        <div class="badge-unit"><div class="badge" id="badge-lc">LC</div><div class="badge-lbl">MIDI</div></div>
                        <div class="badge-unit"><div class="badge" id="badge-ab">A</div><div class="badge-lbl">SLOT</div></div>
                        <div class="badge-unit"><div class="badge" id="badge-wip">0</div><div class="badge-lbl">WIP</div></div>
                    </div>
                </div>
                <div class="browser-search-zone">
                    <input type="text" id="browser-search" placeholder="Search presets, authors, tags...">
                    <button class="close-btn" onclick="hideBrowser()">&times;</button>
                </div>
            </div>

            <div class="browser-main">
                <!-- PANE 1: CATEGORIES -->
                <div class="browser-pane" id="pane-categories">
                    <div class="pane-header">
                        CATEGORIES
                        <div class="pane-actions">
                            <button class="icon-btn" onclick="PresetBrowser.addCategory()" title="Add Category">+</button>
                            <button class="icon-btn" onclick="PresetBrowser.editCategory()" title="Edit Selected">✎</button>
                            <button class="icon-btn" onclick="PresetBrowser.deleteCategory()" title="Delete Selected">×</button>
                        </div>
                    </div>
                    <ul class="pane-list styled-scroll" id="cat-list" tabindex="0">
                        <li class="active" data-cat="All">All</li>
                        <li data-cat="Factory">Factory</li>
                        <li data-cat="User">User</li>
                        <li data-cat="WIP">WIP (Work in Progress)</li>
                        <li data-cat="Favorites">Favorites</li>
                    </ul>
                </div>

                <!-- PANE 2: LIBRARIES / BANKS -->
                <div class="browser-pane" id="pane-libraries">
                    <div class="pane-header">
                        LIBRARIES / BANKS
                        <div class="pane-actions">
                            <button class="icon-btn" onclick="PresetBrowser.addLibrary()" title="Add Library">+</button>
                            <button class="icon-btn" id="lib-edit-btn" onclick="PresetBrowser.editLibrary()" title="Rename Library">✎</button>
                            <button class="icon-btn" id="lib-delete-btn" onclick="PresetBrowser.deleteLibrary()" title="Delete Library">×</button>
                        </div>
                    </div>
                    <ul class="pane-list styled-scroll" id="lib-list" tabindex="0"></ul>
                    <div class="pane-footer-actions bank-ops-grid">
                        <button class="footer-action-btn" onclick="juce.menuAction('handleImportSysex')">IMPORT PRESET/S</button>
                        <button class="footer-action-btn" onclick="PresetBrowser.importLibrary()">IMPORT .JSON</button>
                        <button class="footer-action-btn" onclick="PresetBrowser.exportLibrary()">EXPORT .JSON</button>
                        <button class="footer-action-btn" id="btn-save-tape" onclick="PresetBrowser.saveTape()">SAVE TAPE (.WAV)</button>
                        <button class="footer-action-btn" id="btn-load-tape" onclick="PresetBrowser.loadTape()">LOAD TAPE (.WAV)</button>
                    </div>
                </div>

                <!-- PANE 3: PRESETS -->
                <div class="browser-pane" id="pane-presets">
                    <div class="pane-header">
                        PRESETS
                        <div class="pane-actions">
                            <button class="icon-btn" id="preset-add-btn" onclick="PresetBrowser.addPreset()" title="New Preset">+</button>
                            <button class="icon-btn" id="preset-dup-btn" onclick="PresetBrowser.duplicatePreset()" title="Duplicate Selected">⧉</button>
                            <button class="icon-btn" id="preset-move-btn" onclick="PresetBrowser.movePresetToLibrary()" title="Move to another Bank">➔</button>
                            <button class="icon-btn" onclick="PresetBrowser.movePreset(-1)" title="Move Up">▲</button>
                            <button class="icon-btn" onclick="PresetBrowser.movePreset(1)" title="Move Down">▼</button>
                            <button class="icon-btn" id="preset-delete-btn" onclick="PresetBrowser.deletePreset()" title="Delete Selected">×</button>
                        </div>
                    </div>
                    <div class="preset-col-headers">
                        <span class="col-sort active asc" data-sort="name" onclick="PresetBrowser.sortBy('name')">NAME <span class="sort-arrow">▼</span></span>
                        <span class="col-sort" data-sort="category" onclick="PresetBrowser.sortBy('category')">CAT <span class="sort-arrow"></span></span>
                        <span class="col-sort" data-sort="library" onclick="PresetBrowser.sortBy('library')">LIB <span class="sort-arrow"></span></span>
                        <span class="col-sort col-fav" data-sort="favorite" onclick="PresetBrowser.sortBy('favorite')">★ <span class="sort-arrow"></span></span>
                    </div>
                    <ul class="pane-list styled-scroll" id="preset-list" tabindex="0"></ul>
                    <div class="pane-footer-actions">
                        <div class="preset-count" id="preset-count-label">0 Presets</div>
                    </div>
                </div>

                <!-- PANE 4: METADATA & PREVIEW -->
                <div id="pane-info">
                    <div class="info-header">METADATA</div>
                    <div class="info-content">
                        <div class="meta-row">
                            <label>Name</label>
                            <input type="text" id="meta-name" readonly>
                        </div>
                        <div class="meta-row">
                            <label>Author</label>
                            <input type="text" id="meta-author">
                        </div>
                        <div class="meta-row">
                            <label>Category</label>
                            <select id="meta-category">
                                <option>Bass</option>
                                <option>Lead</option>
                                <option>Pad</option>
                                <option>Strings</option>
                                <option>Brass</option>
                                <option>FX</option>
                                <option>Keys</option>
                                <option>Organ</option>
                                <option>Arp</option>
                                <option>User</option>
                            </select>
                        </div>
                        <div class="meta-row">
                            <label>Tags</label>
                            <input type="text" id="meta-tags" placeholder="80s, analog, warm...">
                        </div>
                        <div class="meta-row stack">
                            <label>Notes</label>
                            <textarea id="meta-notes" class="styled-scroll"></textarea>
                        </div>
                        <div class="meta-footer-info">
                            <span id="meta-date">Created: --/--/----</span>
                        </div>
                        <div class="meta-actions">
                            <button class="footer-action-btn primary" onclick="PresetBrowser.applyMetadata()">UPDATE METADATA</button>
                            <button class="footer-action-btn" id="preset-saveas-btn" onclick="PresetBrowser.showSaveAsModal()">SAVE AS...</button>
                            <button class="footer-action-btn" id="preset-write-btn" onclick="PresetBrowser.saveSynthToPreset()">SAVE CURRENT SOUND (WRITE)</button>
                        </div>
                    </div>

                    <div class="ab-compare-zone">
                        <div class="ab-header">A/B COMPARE</div>
                        <div class="ab-buttons">
                            <button id="btn-slot-a" class="ab-btn active" onclick="PresetBrowser.switchAB(0)">SLOT A</button>
                            <button id="btn-slot-b" class="ab-btn" onclick="PresetBrowser.switchAB(1)">SLOT B</button>
                            <button id="btn-copy-ab" class="ab-btn" onclick="PresetBrowser.copyAB()">COPY A➔B</button>
                        </div>
                    </div>

                </div>
            </div>

            <div class="browser-footer">
                <div class="footer-right" style="width: 100%; display: flex; justify-content: flex-end; padding: 10px 20px;">
                    <button class="footer-btn cancel" onclick="hideBrowser()">CLOSE</button>
                </div>
            </div>
        </div>
`;

// ========================================
// MODAL: Smart Import
// ========================================
const MODAL_SMART_IMPORT = `
        <div class="smart-import-container">
            <!-- Header -->
            <div class="smart-import-header">
                <div style="display: flex; align-items: center; gap: 12px;">
                    <span style="font-size: 14px; font-weight: bold; color: #fa0; letter-spacing: 1px;">SMART IMPORT</span>
                    <span id="si-format-badge" style="font-size: 10px; font-weight: bold; color: #000; background: #fa0; padding: 2px 8px; border-radius: 3px; letter-spacing: 1px; text-transform: uppercase;">TAPE</span>
                    <span id="si-fileName" style="font-size: 11px; color: #888; font-family: monospace;"></span>
                </div>
                <button class="close-btn" onclick="closeSmartImport()" style="color: #666; font-size: 22px;">&times;</button>
            </div>

            <div class="smart-import-body">
                <!-- Progress Log Area -->
                <div id="si-progress-section" style="border-bottom: 1px solid #1a1a1a;">
                    <div class="si-section-header" style="padding: 8px 20px; font-size: 9px; color: #666; text-transform: uppercase; letter-spacing: 1px; background: rgba(0,0,0,0.2);">
                        <span>PROGRESS LOG</span>
                        <span id="si-progress-status" style="float: right; color: #fa0; font-weight: bold;">ANALYZING...</span>
                    </div>
                    <div id="si-progress-log" class="styled-scroll" style="padding: 8px 20px; max-height: 120px; overflow-y: auto; font-family: 'Courier New', monospace; font-size: 10px; line-height: 1.6; background: rgba(0,0,0,0.15);">
                        <div style="color: #555;">Waiting for analysis...</div>
                    </div>
                </div>

                <!-- Results Area (hidden until analysis completes) -->
                <div id="si-results-section" style="display: none; flex: 1; overflow-y: auto; padding: 15px 20px;">
                    <!-- Row 1: Preset Summary (all formats) -->
                    <div id="si-preset-summary" style="margin-bottom: 15px; display: none;">
                        <div class="si-section-header" style="font-size: 9px; color: #666; text-transform: uppercase; letter-spacing: 1px; margin-bottom: 8px;">IMPORT SUMMARY</div>
                        <div id="si-summary-grid" style="display: grid; grid-template-columns: 1fr 1fr 1fr 1fr; gap: 8px;">
                            <div class="si-metric-card">
                                <div class="si-metric-label">PRESETS</div>
                                <div class="si-metric-value" id="si-preset-count">-</div>
                            </div>
                            <div class="si-metric-card">
                                <div class="si-metric-label">BANKS</div>
                                <div class="si-metric-value" id="si-bank-count">-</div>
                            </div>
                            <div class="si-metric-card">
                                <div class="si-metric-label">TYPE</div>
                                <div class="si-metric-value" id="si-import-type" style="font-size: 10px;">-</div>
                            </div>
                            <div class="si-metric-card">
                                <div class="si-metric-label">FORMAT</div>
                                <div class="si-metric-value" id="si-import-format" style="font-size: 10px;">-</div>
                            </div>
                        </div>
                    </div>

                    <!-- Row 2: Preset Names List (all formats) -->
                    <div id="si-preset-names-section" style="margin-bottom: 15px; display: none;">
                        <div class="si-section-header" style="font-size: 9px; color: #666; text-transform: uppercase; letter-spacing: 1px; margin-bottom: 8px;">
                            PRESET NAMES
                            <span id="si-names-count" style="float: right; color: #888;"></span>
                        </div>
                        <div id="si-preset-names" class="styled-scroll" style="max-height: 120px; overflow-y: auto; background: rgba(0,0,0,0.15); border: 1px solid #1a1a1a; border-radius: 3px; padding: 6px 10px; font-family: 'Courier New', monospace; font-size: 10px; line-height: 1.8;"></div>
                    </div>

                    <!-- Row 3: Tape-specific Sections -->
                    <div id="si-tape-section" style="display: none;">
                        <!-- Quality Metrics -->
                        <div style="margin-bottom: 15px;">
                            <div class="si-section-header" style="font-size: 9px; color: #666; text-transform: uppercase; letter-spacing: 1px; margin-bottom: 8px;">SIGNAL QUALITY</div>
                            <div id="si-metrics-grid" style="display: grid; grid-template-columns: 1fr 1fr 1fr 1fr; gap: 8px;">
                                <div class="si-metric-card">
                                    <div class="si-metric-label">SNR</div>
                                    <div class="si-metric-value" id="si-snr">- dB</div>
                                    <div class="si-metric-badge" id="si-snr-badge"></div>
                                </div>
                                <div class="si-metric-card">
                                    <div class="si-metric-label">JITTER</div>
                                    <div class="si-metric-value" id="si-jitter">- %</div>
                                    <div class="si-metric-badge" id="si-jitter-badge"></div>
                                </div>
                                <div class="si-metric-card">
                                    <div class="si-metric-label">DROPOUTS</div>
                                    <div class="si-metric-value" id="si-dropouts">- %</div>
                                    <div class="si-metric-badge" id="si-dropouts-badge"></div>
                                </div>
                                <div class="si-metric-card">
                                    <div class="si-metric-label">DURATION</div>
                                    <div class="si-metric-value" id="si-duration">- s</div>
                                    <div class="si-metric-badge" id="si-quality-badge" style="background: #555;">-</div>
                                </div>
                            </div>
                        </div>

                        <!-- Decoder Results Selection -->
                        <div style="margin-bottom: 15px;">
                            <div class="si-section-header" style="font-size: 9px; color: #666; text-transform: uppercase; letter-spacing: 1px; margin-bottom: 8px;">DECODER RESULTS</div>
                            <div id="si-decoder-list"></div>
                        </div>

                        <!-- Waveform Preview -->
                        <div>
                            <div class="si-section-header" style="font-size: 9px; color: #666; text-transform: uppercase; letter-spacing: 1px; margin-bottom: 5px;">WAVEFORM</div>
                            <div style="position: relative; background: #000; border: 1px solid #222; border-radius: 4px; overflow: hidden;">
                                <canvas id="si-waveform-canvas" width="740" height="80" style="width: 100%; height: 80px; display: block;"></canvas>
                            </div>
                        </div>
                    </div>

                    <!-- Row 4: SysEx-specific Section -->
                    <div id="si-sysex-section" style="display: none; margin-bottom: 15px;">
                        <div class="si-section-header" style="font-size: 9px; color: #666; text-transform: uppercase; letter-spacing: 1px; margin-bottom: 8px;">SYSEX DETAILS</div>
                        <div id="si-sysex-grid" style="display: grid; grid-template-columns: 1fr 1fr 1fr; gap: 8px;">
                            <div class="si-metric-card">
                                <div class="si-metric-label">DEVICE ID</div>
                                <div class="si-metric-value" id="si-sysex-device" style="font-size: 12px;">-</div>
                            </div>
                            <div class="si-metric-card">
                                <div class="si-metric-label">FUNCTION</div>
                                <div class="si-metric-value" id="si-sysex-function" style="font-size: 10px;">-</div>
                            </div>
                            <div class="si-metric-card">
                                <div class="si-metric-label">CHECKSUM</div>
                                <div class="si-metric-value" id="si-sysex-checksum" style="font-size: 11px;">-</div>
                            </div>
                        </div>
                        <div id="si-sysex-hex" style="margin-top: 8px; background: rgba(0,0,0,0.15); border: 1px solid #1a1a1a; border-radius: 3px; padding: 8px; font-family: 'Courier New', monospace; font-size: 9px; color: #888; word-break: break-all; max-height: 60px; overflow-y: auto;"></div>
                    </div>

                    <!-- Row 5: CSV-specific Section -->
                    <div id="si-csv-section" style="display: none; margin-bottom: 15px;">
                        <div class="si-section-header" style="font-size: 9px; color: #666; text-transform: uppercase; letter-spacing: 1px; margin-bottom: 8px;">CSV PARAMETERS</div>
                        <div id="si-csv-grid" style="display: grid; grid-template-columns: 1fr 1fr; gap: 8px;">
                            <div class="si-metric-card">
                                <div class="si-metric-label">COLUMNS</div>
                                <div class="si-metric-value" id="si-csv-columns" style="font-size: 12px;">-</div>
                            </div>
                            <div class="si-metric-card">
                                <div class="si-metric-label">PARAMETERS</div>
                                <div class="si-metric-value" id="si-csv-params" style="font-size: 10px;">-</div>
                            </div>
                        </div>
                        <div id="si-csv-column-list" style="margin-top: 8px; background: rgba(0,0,0,0.15); border: 1px solid #1a1a1a; border-radius: 3px; padding: 8px; font-family: 'Courier New', monospace; font-size: 9px; color: #aaa; max-height: 60px; overflow-y: auto;"></div>
                    </div>
                </div>
            </div>

            <!-- Footer -->
            <div class="smart-import-footer">
                <button class="footer-btn" onclick="closeSmartImport()" style="padding: 8px 20px;">CANCEL</button>
                <button class="footer-btn primary" onclick="confirmSmartImport()" id="btn-si-import" disabled style="padding: 8px 20px; background: #0a4; border-color: #0a4;">IMPORT SELECTED</button>
            </div>
        </div>
`;

// ========================================
// Synchronous injection — runs immediately
// to ensure DOM elements exist before
// service.js and browser.js load.
// ========================================
(function() {
    var settings = document.getElementById('modal-globalSettings');
    if (settings) settings.innerHTML = MODAL_SETTINGS;

    var browser = document.getElementById('modal-browser');
    if (browser) browser.innerHTML = MODAL_BROWSER;

    var smartImport = document.getElementById('modal-smartImport');
    if (smartImport) smartImport.innerHTML = MODAL_SMART_IMPORT;
})();
