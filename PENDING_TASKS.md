# JUNiO 601 - Resumen de Estado y Tareas Pendientes

Este documento detalla el estado actual del proyecto tras la sesión de refinamiento UX/UI y enumera las tareas críticas para continuar el desarrollo.

## Estado Actual
- **Build**: Estable (Debug y Standalone). Se han corregido todos los warnings de compilación (C4189, C4100, C4457) y errores de sintaxis.
- **UX/UI**: 
    - Controles de Volumen y Tune convertidos a Knobs.
    - Pulsadores (botones 1-8, Bank/Patch, Panic, Random) ahora son momentáneos.
    - El teclado en pantalla se ha ensanchado.
    - **Display SysEx**: Ahora es multilínea y se actualiza en **tiempo real** al mover cualquier parámetro.
    - **Identidad**: Renombrado a "JUNiO 601" en cabecera y diálogos.
    - **LFO LED**: Implementado un LED táctico que parpadea al ritmo de la tasa de modulación.
- **Git**: Todo el trabajo está commiteado y pusheado a la rama `main`.

## Tareas Pendientes Detalladas

### 1. Sistema de Undo/Redo (Integración Completa)
- [x] **Verificar Tracking**: Asegurar que el `juce::UndoManager` está capturando transacciones desde `APVTS` para cada cambio de parámetro.
- [x] **Sincronización de UI**: Al hacer Undo, los sliders/botones deben actualizar su posición visual (mediante el puente de mensajería bidireccional en el WebUI).

### 2. Menú de Opciones (Completado)
- [x] **Diálogo implementado**: El menú Edit tiene "General Settings..." que abre el modal SYSTEM SETTINGS en la pestaña GENERAL.
- [x] **MIDI Channel**: Dropdown de 16 canales en GENERAL tab.
- [x] **Sustain Pedal Invert**: ON/OFF toggle en GENERAL tab.
- [x] **Library Path**: Text input + BROWSE button en GENERAL tab.
- [x] **numVoices**: Dropdown (1 MONO / 2 / 4 / 6 / 8 / 12 / 16 MAX) en GENERAL tab.
- [x] **benderRange**: Dropdown (1-4 SEMIS / 5th / OCTAVE) en GENERAL tab.
- [x] **velocitySens**: Range slider (0-100%) en GENERAL tab.
- [x] Además: User Identity (author name), Microtuning (.SCL), y los tabs CALIBRATION, ROUTING, DIAGNOSTICS con parametrización completa.

### 3. Auditoría de Fidelidad de Audio
- [x] **VCF Self-oscillation**: Corregir e integrar los parámetros dinámicos `selfOscThreshold` y `selfOscInt` en `JunoVCF::processSample` (actualmente se pasan pero se ignoran, usando solo `ResK_J106`).
- [x] **Curvas ADSR**: Verificado. Se emula la tasa de ticks de 4.23ms del uPD7811G, la atenuación del 6% en CalcDecay y curvas exponenciales realistas.
- [x] **Sub-oscillator**: Verificado. Implementado mediante PolyBLEP y filtro pasivo RC Lowpass sintonizado a 4.2 kHz para el rolloff real.

### 4. Skin Manager y Personalización
- [x] **Desarrollar Skin Manager**: Implementado dinámicamente mediante el parámetro `skinType` en `CalibrationSettings` y controlado mediante clases/atributos en la WebUI.
- [x] **Tokens de Diseño**: Sincronización de colores en caliente mediante variables CSS raíz redefinidas dinámicamente.

### 5. Manual Mode Logic
- [x] **Snapshot Inicial**: Al activar "MANUAL", el motor debe capturar inmediatamente el estado físico de TODOS los sliders de la interfaz y enviarlos al DSP de una vez (verificado y validado síncronamente en el procesador).

### 6. Renombres de Eventos y Fixes JS Bridge (Completado)
- [x] **`onSmartTapeProgress` → `onSmartImportProgress`**: Renombrado en `WebViewEditor.cpp` (10 ocurrencias) y `script.js` (2 listeners).
- [x] **`onSmartTapeResult` → `onSmartImportResult`**: Renombrado en `WebViewEditor.cpp` (2 `dispatchToJS`) y `script.js` (1 `listenEvent`).
- [x] **`onImportResult` listener faltante**: Agregado `listenEvent("onImportResult", ...)` en `script.js` que llama a `showNotification()` con el mensaje y tipo (success/error). C++ dispatchea este evento después de `confirmImportFile()` y `confirmTapeImport()`.
- [x] **Listeners duplicados eliminados**: Se eliminaron 2 listeners duplicados en `script.js`:
    - `onSmartImportResult` duplicado (causaba doble procesamiento de `processSmartImportResult()`)
    - `onSmartImportProgress` duplicado (causaba líneas de log duplicadas y doble actualización de estado)
- [x] **`juce.confirmImportFile()` expuesto**: Faltaba `confirmImportFile: () => callNative("confirmImportFile")` en el wrapper `window.juce`, causando `TypeError` al hacer clic en Import.

### 7. Tests de Regresión CDP (CI/CD)
- [x] **`test_smart_import_direct.py`**: Test del Smart Import SYSEX vía JS bridge (6 checks: modal, badge, metadata, preset names, botón).
- [x] **`test_smart_import_sysex_e2e.py`**: Test E2E con diálogo nativo (File > Import SysEx → Smart Import).
- [x] **`test_smart_import_all_formats.py`**: Test de regresión multi-formato (Tape 12/12, SysEx 15/15, CSV 14/14, JSON 14/14 = 55 checks totales).
- [x] **`test_import_real.py`**: Test de importación real con 16 checks (fallback mode). Banco de prueba en `test_import_bank.json`.
- [x] **Pipeline CI/CD implementado** (`build_and_test.bat`):
    - `build_and_test.bat` → Build + Launch + 2 Tests (55 + 16 = 71 checks) + Cleanup
    - `build_and_test.bat --test-only` → Skip build, solo launch + test
    - `build_and_test.bat --help` → Show usage
    - **Requisito**: Variable de entorno `WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS=--remote-debugging-port=9222 --remote-allow-origins=*` para CDP en WebView2.
    - **Comando**: `python -X utf8 scripts/test_smart_import_all_formats.py && python -X utf8 scripts/test_import_real.py`
- [x] **Ampliar cobertura**:
    - [x] **Test de importación real** (`test_import_real.py`): 16 checks (UI modal JSON, badge, metadata, import click, modal close, notificación, fallback skip). Banco de prueba en `test_import_bank.json`.
    - [x] **Integrado en pipeline CI/CD**: 71 checks totales (55 regresión + 16 real import).
    - [x] Verificar que el progress-log muestre "Analysis complete!" en cada formato.

### 7b. Fixes de Tests CDP — test_import_real.py (6 Junio 2026)
- [x] **Fix: Notification toast en modo fallback** — `confirmSmartImport()` en `smart-import.js` ahora captura la promesa de `juce.confirmImportFile()` y muestra notificación vía `.then()` como JS-side fallback, para cuando C++ no dispatchea `onImportResult` (modo sin pending file).
- [x] **Fix: Library name check duplicado** — `test_import_real.py` tenía ambos lados del `or` idénticos (`"Test Bank" in csvCols or "Test Bank" in csvCols`). Segundo lado ahora verifica `"Test Bank CDP"` (nombre real del banco de prueba).
- [x] **Verificación** — `test_import_real.py`: 16/16 ✅, `test_smart_import_all_formats.py`: 55/55 ✅ (sin regresión). Pipeline total: 71/71 ✅.

### 8. Refactorización de Archivos Grandes (División de Codebase)
- [ ] **Auditar archivos >50KB** y dividir en módulos más pequeños:
    - **114 KB** — `Source/Core/JunoUnitTests.cpp`: Separar por categoría de test (DCO, VCF, ADSR, Chorus, SysEx, Tape, CSV, etc.) en archivos individuales dentro de `Source/Core/Tests/`.
    - **110 KB** — `Source/UI/WebView/WebViewEditor.cpp`: Extraer cada `withNativeFunction` callback a su propio archivo (ej. `BridgeActions.cpp`, `BridgeImport.cpp`, `BridgeBrowser.cpp`).
    - [x] **89 KB → 63 KB** — `Source/UI/WebUI/index.html`: Separar modales grandes (browser, smart import, settings) en archivos HTML parciales e inyectarlos con JS.
      - **`Source/UI/WebUI/modals.js`** (nuevo, ~26 KB) — Contiene 3 templates modales como constantes JS string literal (`MODAL_SETTINGS`, `MODAL_BROWSER`, `MODAL_SMART_IMPORT`) + función IIFE sincrónica que inyecta HTML via `innerHTML` antes de que `service.js`/`browser.js` referencien los elementos DOM.
      - `index.html`: `modal-globalSettings` (~12 KB), `modal-browser` (~6 KB), `modal-smartImport` (~8 KB) reemplazados por contenedores vacíos con comentario. `<script src="modals.js">` añadido antes de `script.js` (inyección sincrónica garantizada).
      - BinaryData scanner detecta `modals.js` automáticamente; WebViewEditor.cpp lo sirve vía fallback `BinaryData::getNamedResource()` — sin cambios en C++ ni CMakeLists.txt.
      - Build: 0 errores. Modales pequeños (about, saveas, savePatch) mantenidos inline.
    - [x] **80 KB → 6 módulos JS** — `Source/UI/WebUI/script.js` eliminado. Dividido en:
      - **`bridge-core.js`** (~18 KB) — Variables globales, callNative, listenEvent, window.juce, updateLCD, initApp, DOMContentLoaded, copySysExToClipboard, LCD/digit/splash/sevensSegment handlers
      - **`ui-sliders.js`** (~12 KB) — setupSliders, setupButtons, setupBender, setupKeyboard, setupOctaveButtons, setupMenus, syncUI, updateSevenSegment
      - **`ui-modals.js`** (~8 KB) — Modal show/hide, switchTab, updatePref, tuning/table click handlers, save modal, initUserSettingsSync, showNotification, library path, table import listeners
      - **`smart-import.js`** (~10 KB) — processSmartImportResult, closeSmartImport, confirmSmartImport, selectSmartDecoder, renderSmartWaveform, onSmartImportProgress/Result listeners
      - **`ui-keyboard.js`** (~3 KB) — QWERTY_KEYS, allQwertyNotesOff, keydown/keyup handlers (Ctrl+Z/Y undo/redo, octave shift, piano playback)
      - **`theme-manager.js`** (~6 KB) — switchPerformanceTab, updateThemeAndSkins, updatePainterTapes, painter tape click listeners
      - **`index.html`** — `<script src="script.js">` reemplazado por los 6 módulos en orden de dependencia (bridge-core.js → ui-sliders.js → ui-modals.js → smart-import.js → ui-keyboard.js → theme-manager.js) antes de service.js/browser.js
      - BinaryData auto-detecta los nuevos .js — sin cambios en C++ ni CMakeLists.txt
      - `script.js` eliminado del disco (80 KB de código muerto)
      - Build 0 errores, tests ALL CHECKS PASSED
    - [x] **71 KB → 36 KB** — `Source/Core/PluginProcessor.cpp`: Extraer lógica de SysEx, estado, presets y miscelánea a 4 archivos separados.
      - **`Source/Core/ProcessorState.cpp`** (nuevo) — `getMirrorParameters()`, `applyPresetState()`, `updateParamsFromAPVTS()`, `applyPerformanceModulations()`
      - **`Source/Core/ProcessorPresets.cpp`** (nuevo) — `loadPreset()`, `loadLibraryPreset()`, `randomizeSound()`, `switchABSlot()`, `copyCurrentToAlternateSlot()`, `updateMetadata()`, `getPresetManager()`
      - **`Source/Core/ProcessorSysEx.cpp`** (nuevo) — `sendSysEx()`, `sendPatchDump()`, `sendManualMode()`
      - **`Source/Core/ProcessorMisc.cpp`** (nuevo) — tuning, settings, self-test, recording, UI notification, triggerPanic/LFO, enterTestMode, triggerTestProgram, getWipCount
      - `CMakeLists.txt` actualizado con 4 nuevas fuentes en target `ABDSimpleJuno106`
      - Sin includes duplicados — cada archivo incluye solo `ABDSimpleJuno106AudioProcessor.h` + headers específicos
      - Build 0 errores, tests ALL CHECKS PASSED
    - [x] **68 KB** — `Source/Synth/J106DACHzTable.h`: Migrado a binario.
      - Nueva cabecera `Source/Synth/DacHzTable.h` con `getDacHzTable()` que:
        1. Intenta cargar `kV4Hz.bin` (32 KB, raw doubles) desde el directorio del ejecutable.
        2. Si falla, cae al `constexpr` original de `J106DACHzTable.h`.
      - `scripts/generate_dac_bin.py` para regenerar el .bin desde la cabecera.
      - `CalibrationSettings.cpp` actualizado para usar `getDacHzTable()`.
      - CSV import/export intacto (usa `customDacTable` independientemente).
      - Thread-safe (C++11 static init con lambda).
- [x] **Estandarizar includes**: Includes innecesarios/duplicados removidos de archivos divididos:
    - **`BridgeActions.cpp`**: Eliminados `BridgeImport.h`, `ServiceModeManager.h`, `JunoModelConfig.h` (no usados)
    - **`BridgeActions.h`**: Eliminado forward declaration de `JunoTapeDecoder` (no referenciado en signatures)
    - **`WebViewEditor.cpp`**: Eliminados 5 includes heredados de la refactorización (`CalibrationSettings.h`, `ServiceModeManager.h`, `JunoTapeEncoder.h`, `JunoSysexImporter.h`, `JunoCsvImporter.h`)
    - **`ProcessorMisc.cpp`**: Eliminado `<JuceHeader.h>` redundante (incluido vía `ABDSimpleJuno106AudioProcessor.h`)
    - **`BridgeImport.cpp`**: Eliminado `<JuceHeader.h>` redundante (incluido vía `BridgeImport.h`)
    - Code review: confirmó que todas las remociones son correctas (ningún símbolo perdido)
- [x] **Verificar build**: Después de cada división, compilar y ejecutar tests para asegurar que no hay regresiones.

### 9. Revisión y Mejora del Preset Browser (UX/UI) — Primera Pasada ✓
- [x] **Auditar lógica actual** — `onPresetSelected` se dispara desde `selectedRowsChanged` -> `loadPresetAt()`, `updateFilters()` filtra por library/categoría/favoritos/búsqueda, `refresh()` mantiene selección.
- [x] **Mejorar usabilidad:**
    - **Doble clic** para cargar y cerrar (`listBoxItemDoubleClicked` -> `onCloseRequested()`).
    - **Vista columnar**: Favorito (★), Nombre, Categoría, Librería.
    - **Ordenamiento**: Click en cabeceras alterna ascendente/descendente con indicador visual (> / <).
    - **Atajos de teclado**: ↑↓, Page Up/Down, Home/End, Enter, Escape, type-ahead search.
    - **KeyListener en presetList**: Atajos funcionan incluso cuando la lista tiene foco.
    - **Audition mode**: `selectedRowsChanged` carga el preset al navegar sin cerrar.
- [x] **Evaluar migración a WebUI**: WebUI mejorado con columnas ordenables, teclado, persistencia de selección y doble clic. Nativo C++ se mantiene para el BROWSER de la barra lateral (hardware emulation) — ambos coexisten, cada uno optimizado para su contexto.

### 10. Segunda Pasada — Fixes de Robustez del PresetBrowser Nativo (Completado)
- [x] **Conectar callbacks huérfanos** en `PluginEditor.cpp`:
    - `onCloseRequested` → `setVisible(false)` + sincronizar `browserToggle` + LCD feedback.
    - `onSaveClicked` → `handleSave()` (flujo write-arm).
    - `onSaveAsClicked` → AlertWindow modal para nombre → `saveAsNewPresetFromState()`. Sin double-delete (`deleteWhenDismissed=true` en `enterModalState`, el callback captura `[this, alert]`).
- [x] **Desalineación de columnas por scrollbar**:
    - `resized()` ahora posiciona `presetList` **antes** de calcular rectángulos de cabecera.
    - Usa `presetList.getRowWidth()` como fuente única de ancho (incluye/excluye scrollbar según corresponda).
    - `paintListBoxItem()` usa los anchos almacenados en `headerFav/Name/Category/Library` en lugar de recalcular desde `width`.
- [x] **Preservar selección y scroll en `updateFilters()`**:
    - Guarda `(libIdx, presetIdx)` del preset seleccionado antes de rebuildear `filteredItems`.
    - Guarda scroll position del scrollbar vertical.
    - Guard `mgrSelectionChanged`: si el `PresetManager` cambió externamente (import, bank nav), salta Priority 1 y usa Priority 2 (manager actual).
    - `onPresetSelected = nullptr` temporal para evitar recargar audio durante el restore.
    - Restaura scroll si no se restauró selección.
- [x] **Scrollbar visibility detection**: `updateFilters()` compara `getRowWidth()` contra `lastContentWidth` después de rebuild. Si cambió (scrollbar aparece/desaparece), recalcula header rects sin esperar `resized()`.
- [x] **Teclas ↑/↓ consistentes**: Cuando `selIdx == -1`, ambas teclas seleccionan fila 0 (antes solo ↓ lo hacía).
- [x] **Browser Toggle Button** en `JunoBankSection`:
    - `JunoButton browserToggle { "BROWSER" }` con toggle state.
    - Estado inicial ON (sincronizado con browser visible).
    - `onClick` → `presetBrowser.setVisible(toggleState)`. 
    - `onCloseRequested` sincroniza `browserToggle.setToggleState(false, dontSendNotification)`.
    - `resized()` reordenada: sección 2 = BROWSER TOGGLE, sección 3 = PRESET BROWSER, 4 = NAVIGATION, 5 = BANK SELECTOR, 6 = UTILITY BUTTONS.
- [x] **Extraer fórmula de anchos a método privado**:
    - Nuevo método `recalculateHeaderRects(int contentWidth, int headerY, int headerH)`.
    - Contiene la fórmula única: `favW=24`, `catW=jmax(80, w/5)`, `libW=jmax(70, w/5)`, `nameW=w-favW-catW-libW-8`.
    - Actualiza los 4 rectángulos de cabecera + `lastContentWidth`.
    - Llamado desde `resized()` y `updateFilters()`, eliminando la duplicación.

### 11. Tercera Pasada — Extracción Header Bar, Hover, Tooltips, Layout Constants (Completado)
- [x] **Extraer column headers a `PresetBrowserHeaderBar`** — Nuevo componente standalone (`.h` ~100 líneas, `.cpp` ~200 líneas) que gestiona autónomamente:
    - **Paint**: `paintCell()` con 3 niveles de color (activo, inactive+hover, default).
    - **Hover state**: `mouseMove()` detecta header bajo cursor mediante `contains()` con Y-range guard.
    - **Cursor pointing hand**: `setMouseCursor(PointingHandCursor/PointingHandCursor/PointingHandCursor/PointingHandCursor)` cuando sobre header, `NormalCursor` al salir.
    - **Arrow animation**: `Timer` interno a 60fps, interpola `arrowAnimAngle` al 30% restante/frame (~300ms). Triángulo rotado via `AffineTransform::rotation()`.
    - **Tooltips**: Herencia `SettableTooltipClient`, `setTooltip("Click to sort by X")` en mouseMove, limpieza en mouseExit y early-out.
    - **Click-to-sort**: `mouseDown()` dispara callback `onSortClicked(col)` que PresetBrowser maneja.
    - **Column width formula**: `recalcWidths()` replicada desde `recalculateHeaderRects()`.
- [x] **Refactor PresetBrowser para delegar en header bar**:
    - Eliminados ~150 líneas de lógica de headers de `PresetBrowser.cpp` (paint de cabeceras, mouseDown/mouseMove/mouseExit, timerCallback, startArrowAnim, recalculateHeaderRects).
    - Constructor: elimina `addMouseListener(this, true)`, agrega `addAndMakeVisible(headerBar)` + callback `onSortClicked`.
    - `resized()` usa `headerBar.setBounds()` en lugar de `recalculateHeaderRects()`.
    - `paintListBoxItem()` usa `headerBar.getColFavW()/getColCatW()/getColLibW()/getColNameW()` en lugar de `headerFav.getWidth()` etc.
    - `updateFilters()` usa `headerBar.setSize()` para sincronizar ancho cuando scrollbar cambia.
- [x] **Eliminada herencia `Timer` de `PresetBrowser`** — Ahora solo en `PresetBrowserHeaderBar`.
- [x] **Agregados a `CMakeLists.txt`** — `PresetBrowserHeaderBar.h/.cpp` en target principal `ABDSimpleJuno106`.
- [x] **Layout constants con nombre** — 6 nuevas constantes `public static constexpr int` en `PresetBrowser.h`:
    - `kOuterMargin=5`, `kRowH=30`, `kInnerPad=2`, `kSectionGap=5`, `kHeaderBarH=22`, `kHeaderListGap=2`.
    - 15 literales mágicas reemplazadas en `PresetBrowser::resized()`.
- [x] **Compile-time verification** — 12 `static_assert` en `PresetBrowserTests.cpp`:
    - 6 para column width constants (kColFavW, kColCatWMin, kColLibWMin, kColGapAdj, kColLeftMarg, kColGap).
    - 6 para layout constants (kOuterMargin, kRowH, kInnerPad, kSectionGap, kHeaderBarH, kHeaderListGap).
- [x] **Update `chat_log.md`** — Nueva sección 23 documentando toda la refactorización.

### 12. Cuarta Pasada — Layout Constants Remanentes + Font Scales (Completado)
- [x] **Proportion divisor**: `kColProportionDivisor = 5` añadido en `PresetBrowser.h` y `PresetBrowserHeaderBar.h`. `contentW / 5` → `contentW / kColProportionDivisor` en `recalcWidths()`.
- [x] **List row height**: `kListRowH = 24` en `PresetBrowser.h`. `setRowHeight(24)` → `setRowHeight(kListRowH)` en constructor.
- [x] **Font scales con nombre**: 4 constantes `static constexpr float` en `PresetBrowser.h`:
    - `kNameFontScale = 0.65f` — nombre de preset
    - `kStarFontScale = 0.65f` — estrella de favorito
    - `kDetailFontScale = 0.55f` — categoría
    - `kSmallFontScale = 0.50f` — nombre de librería
- [x] 4 literales de escala reemplazadas en `PresetBrowser::paintListBoxItem()` + 1 en constructor.
- [x] Build 0 errores, tests ALL CHECKS PASSED.
- [x] **Update `chat_log.md`** — Sección 24 documentando layout constants + tooltips.

### 13. Quinta Pasada — Visual Constants: Header Bar Paint, List Alphas, Proporciones, Test (Completado)
- [x] **10 constantes visuales** en `PresetBrowserHeaderBar.h` (privadas): kHeaderFontSize=10.0, kArrowSize=6.0, kArrowOffsetX=4.0, kAnimThreshold=0.001, kAnimSpeed=0.30, kActiveBgAlpha=0.30, kActiveBgDefAlpha=0.15, kHoverBgAlpha=0.10, kDefaultBgAlpha=0.20, kTextAlphaDim=0.60.
- [x] **7 constantes visuales** en `PresetBrowser.h` (públicas): kSelectedBgAlpha=0.25, kNameTextAlpha=0.85, kDetailTextAlpha=0.50, kLibTextAlpha=0.50, kBgAlpha=0.20, kFieldWidthRatio=0.40.
- [x] **19 literales reemplazadas**: 10 en `PresetBrowserHeaderBar::paint()`, 2 en `timerCallback()`, 6 en `PresetBrowser` (paint, paintListBoxItem, resized), 1 en constructor (`saveAsBtn`).
- [x] **12 static_assert nuevos** en `PresetBrowserTests.cpp` (6 font/row + 6 visual/ratio). Helpers actualizados a `PresetBrowser::kColProportionDivisor`.
- [x] Build 0 errores, tests ALL CHECKS PASSED (30 static_assert totales).
- [x] **Update `chat_log.md`** — Sección 25 documentando visual constants.

### 14. README — Tabla de Layout Constants (Completado)
- [x] **Sección "PresetBrowser Layout Constants"** agregada a `README.md` con 4 tablas:
    - **Column Width Constants** (7): kColFavW=24, kColCatWMin=80, kColLibWMin=70, kColGapAdj=8, kColLeftMarg=4, kColGap=2, kColProportionDivisor=5.
    - **Layout Constants** (7): kOuterMargin=5, kRowH=30, kInnerPad=2, kSectionGap=5, kHeaderBarH=22, kHeaderListGap=2, kListRowH=24.
    - **Font Scale Constants** (4): kNameFontScale=0.65, kStarFontScale=0.65, kDetailFontScale=0.55, kSmallFontScale=0.50.
    - **Visual / Alpha Constants** (16): kSelectedBgAlpha=0.25, kNameTextAlpha=0.85, kDetailTextAlpha=0.50, kLibTextAlpha=0.50, kBgAlpha=0.20, kFieldWidthRatio=0.40, kHeaderFontSize=10.0, kArrowSize=6.0, kArrowOffsetX=4.0, kActiveBgAlpha=0.30, kActiveBgDefAlpha=0.15, kHoverBgAlpha=0.10, kDefaultBgAlpha=0.20, kTextAlphaDim=0.60, kAnimThreshold=0.001, kAnimSpeed=0.30.
- [x] **Update `chat_log.md`** — Secciones 26 y 27 documentando README constants.

## Archivos de Referencia Clave
- `PluginProcessor.cpp`: Lógica de SysEx y gestión de parámetros.
- `PluginEditor.cpp`: Layout de la nueva interfaz y callbacks de menú.
- `JunoBankSection.h`: Definición de los nuevos botones momentáneos.
- `FactoryPresets.h`: Datos crudos de los 128 parches originales.
- `scripts/cdp_helpers.py`: Cliente CDP reutilizable para tests automatizados.
- `scripts/test_smart_import_all_formats.py`: Test de regresión multi-formato (Tape, SysEx, CSV).

### 15. Model Visibility & Overlap Fixes (Completado)
- [x] **5 fixes de solapamiento** en `bridge-core.js` `updateModelVisibility()`:
    - **VCF**: Ocultar wrapper `.ctrl-group.center` cuando `vcf-polarity` está oculto (J-60, J-6).
    - **VCA**: Centrar LEVEL slider con `justifyContent: center` cuando `vca-mode` está oculto (J-60, J-6).
    - **DCO separadores (J-6)**: Ocultar todos los `.separator` dentro de `#dco` cuando J-6 (LFO/PWM y SUB/NOISE ocultos).
    - **Header grid (J-6)**: Cambiar grid de `230px 60px 1fr 400px` a `230px 60px 1fr` + LCD expandido al 100% cuando `sysex-zone` oculto.
    - **Engine row 2**: Quitar `border-right` de `#env` cuando CHORUS oculto (env es el último elemento visible).

### 16. calibrationProfile Oculto en Modelos Individuales (Completado)
- [x] **Filtro en `service.js`**: `if (p.id === "calibrationProfile" && getCompiledTargetModel() !== 0) return false;` — oculta el selector de perfil en modelos individuales.
- [x] **Valor forzado en `bridge-core.js`**: J-106 → 2, J-60 → 1, J-6 → 0 mediante `juce.setCalibrationParam()`.
- [x] El filtrado de parámetros `_J6`/`_J60`/`_J106` sigue funcionando porque `paramValuesCache['calibrationProfile']` mantiene el valor correcto.

### 17. Skin Selector Filtrado por Modelo Compilado (Completado)
- [x] **Nueva función `getCompiledTargetModel()`** en `service.js` — lee `targetModel` de `initData`.
- [x] **Skin filtering**: Reemplazada la lógica basada en `calibrationProfile` por `getCompiledTargetModel()`:
    - Super Six (0): 9 skins
    - J-106 (1): 6 skins (JUNO-106 CLASSIC, JUNO-106S DARK + universales)
    - J-60 (2): 5 skins (JUNO-60 CLASSIC + universales)
    - J-6 (3): 5 skins (JUNO-6 ANALOG + universales)

### 18. Voice Count Forzado a 6 (Completado)
- [x] Selector `numVoices` oculto en modelos individuales (ya existía).
- [x] Valor forzado a 6.0 (CLASSIC) mediante `juce.setCalibrationParam('numVoices', 6.0)`.

### 19. Test Visual de Visibilidad (Completado)
- [x] `test_model_ui.html` actualizado a v5 con 28 checks por modelo (+calibrationProfile check).
- [x] Verificación browser-use: 112 checks totales, 0 fallos.

### 20. Tests Unitarios — 4 Modelos, 0 Fallos (Completado)
- [x] **Super Six (0)**: ~5,502 tests, 0 fallos.
- [x] **JUNO_TARGET_MODEL=1 (J-106)**: 5,595 tests, 0 fallos.
- [x] **JUNO_TARGET_MODEL=2 (J-60)**: 5,160 tests, 0 fallos.
- [x] **JUNO_TARGET_MODEL=3 (J-6)**: ~4,413 tests, 0 fallos.

### 21. Herramientas de Análisis Cualitativo de Audio (Nuevo)
- [x] **`scripts/compare_audio.py`** — Herramienta de comparación espectral WAV vs WAV:
  - Carga WAV (scipy) o MP3 (ffmpeg subprocess)
  - Alineación temporal por cross-correlación
  - Polyphase resampling con ratio reducido por GCD
  - 4 paneles: waveform overlay, average spectrum, spectrogram difference, metrics table
  - Métricas: RMS diff, correlación, similitud espectral (cosine), centroide espectral, spectral rolloff
  - Interpretación por colores (verde/amarillo/rojo)
  - Segment selector interactivo via SpanSelector
- [x] **`scripts/download_synthmania.py`** — Descargador de preset MP3s desde synthmania.com:
  - Scraping con BeautifulSoup de `<a>` tags con href `.mp3`
  - Soporte Juno-60 y Juno-106
  - Detección de bancos A/B por regex + fallback por conteo
  - Rate limiting configurable
  - Modos: list-only, single-preset, batch download
  - Nombres sanitizados para Windows
  - Issues fixeados: Unicode cp1252, HTTP 406 ModSecurity, scraper `<b>` vs `<a>`, nombres truncados
- [x] **Descarga de presets de referencia** (18 MP3s):
  - 9 presets Juno-60 en `ref_audio/juno60/`
  - 9 presets Juno-106 en `ref_audio/juno106/`- [x] **Instalar ffmpeg** (6 Jun 2026) — Descargado e instalado ffmpeg 8.1.1 essentials desde gyan.dev
- [x] **Ejecutar primera comparación cualitativa** (6 Jun 2026) — 3 comparaciones: self-test (✅), diff patches (✅ detecta diferencia), engine A11 vs ref A01 (❌ patches diferentes)
- [ ] **Generar audio de referencia del engine** — PENDIENTE: el engine WAV existente (A11 "Brass Set 1") no coincide con ninguna referencia SynthMania (A01-A10). Se necesita grabar el engine tocando un preset que SÍ tenga referencia (ej. A12 Brass Swell → SynthMania #2 Brass Swell).
- [ ] **Ejecutar comparación cualitativa real** — PENDIENTE: requiere engine WAV + referencia matching

---
**Nota:** Para generar engine WAV, abrir la app standalone, cargar preset A12 (Brass Swell) o el preset que tenga matching con SynMania, activar REC en el menú, tocar notas C3-E3-G3 durante ~3s c/u, detener grabación y guardar en `ref_audio/engine/`.
