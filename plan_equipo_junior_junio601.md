# 🚀 Plan de Trabajo para Equipo Junior: Calidad y Producción VST3 / JUCE (Proyecto ABDJUNiO601)

> **Propósito:** Diagnóstico y hoja de ruta de ejecución para elevar el sintetizador **ABDJUNiO601** al mismo estándar de calidad y estabilidad VST3 alcanzado en **ABDEep**, basándose en el [Checklist de Calidad para Plugins JUCE](../ABDEep/plugin_quality_checklist.md).
>
> **Nota de Control:** Ninguna línea de código ha sido modificada durante esta auditoría. Este documento sirve de especificación técnica para que el equipo Junior lo ejecute paso a paso.

---

## 📊 1. Diagnóstico de la Auditoría Actual (ABDJUNiO601)

### ✅ Puntos Fuertes Detectados
1. **Suavizado de Parámetros:** Usa `juce::SmoothedValue` en volumen (`smoothedSagGain`).
2. **Anti-denormals:** Incluye `juce::ScopedNoDenormals` al inicio de `processBlock()`.
3. **Persistencia XML (Session v2):** `getStateInformation()` y `setStateInformation()` serializan correctamente un nodo `<Session version="2">` con `patchName`, `author`, `category`, `currentBank`, `currentPreset`, etc.
4. **Resiliencia de Calibración:** Los archivos de calibración usan `CalibrationSettings` con valores por defecto de fábrica.
5. **Frecuencia de Muestreo:** Soporte de `prepareToPlay()` con inicialización de DSP para BBD Chorus, arpegiador y LFO.

---

### ⚠️ Deficiencias Detectadas (Hallazgos Críticos y Medios)

| ID | Área del Checklist | Deficiencia Detectada en Código | Impacto / Riesgo | Prioridad |
|---|---|---|---|---|
| **J-01** | 4. Persistencia DAW | **[CRÍTICO] Reseteo forzado de preset en `prepareToPlay()`**: En `PluginProcessor.cpp` (líneas 250-255), `if (firstPrepare_) loadPreset(0);` sobrescribe cualquier patch guardado en la sesión del DAW al abrir el proyecto. | **Pérdida de datos del usuario**: Al reabrir un proyecto en Cubase/Ableton, el plugin olvida los retoques hechos por el usuario y carga el Preset 0 de fábrica. | 🔴 Crítico |
| **J-02** | 19. VST3 Bypass | **Falta de `processBlockBypassed()`**: No está implementado en `PluginProcessor.cpp`. Al presionar el botón de Bypass en el DAW, JUCE procesa silencio o no responde adecuadamente. | El DAW no puede desactivar el plugin de forma limpia; posible congelamiento de notas o audio procesado innecesariamente. | 🔴 Alta |
| **J-03** | 18. Sample Rate / Buffer | **Falta de guarda de buffer cero**: `processBlock()` en `PluginProcessor.cpp` (línea 268) no valida `if (buffer.getNumSamples() == 0) return;`. | Crashes en FL Studio, Logic Pro o Bitwig al enviar bloques de 0 muestras. | 🔴 Alta |
| **J-04** | 6. Latencia & Tail | **Tail Length hardcoded a `0.0s`**: `getTailLengthSeconds()` devuelve `0.0s`, pero el plugin contiene Chorus BBD y Tape Echo (Super Six). | El DAW corta la cola de reverberación/eco abruptamente al pausar la reproducción. | 🟡 Media |
| **J-05** | 22. Foco de Teclado | **Captura no deseada de atajos**: En `WebViewEditor.cpp`, no se invoca `setWantsKeyboardFocus(false);` en el contenedor de la interfaz nativa. | Al pulsar la Barra Espaciadora en el DAW (Play/Stop), el evento es atrapado por WebView2. | 🟡 Media |
| **J-06** | 17. Validación | **Pluginval nunca ejecutado**: No existe validación automatizada en el pipeline de build para `JUNiO_601.vst3`. | Posibles fallos intermitentes no detectados en hosts específicos. | 🔴 Alta |
| **J-07** | 21. CI/CD & Build | **Falta de script de verificación**: No existe un `verify_release.ps1` que enlace compilar → unit tests → Vitest → Pluginval. | Proceso de entrega propenso a errores humanos manuales. | 🟢 Baja |

---

## 🛠️ 2. Plan de Acción por Sprints para el Equipo Junior

### 📦 Sprint 1: Correcciones Críticas de Estado y Seguridad Real-Time
**Objetivo:** Evitar la pérdida de datos de los usuarios al reabrir proyectos en DAWs y prevenir crashes por buffers vacíos.

- [ ] **Tarea 1.1: Eliminar la sobrescritura forzada de preset en `prepareToPlay()`**
  - **Archivo:** `Source/Core/PluginProcessor.cpp` (líneas 250-255)
  - **Instrucción:** Eliminar el bloque `if (firstPrepare_) { loadPreset(0); }`. El estado del preset debe gestionarse exclusivamente mediante `setStateInformation()` (para proyectos guardados) o inicializarse en el constructor.

- [ ] **Tarea 1.2: Añadir guarda de buffer cero en `processBlock()`**
  - **Archivo:** `Source/Core/PluginProcessor.cpp` (línea 268)
  - **Instrucción:** Al inicio de `processBlock()`, agregar:
    ```cpp
    if (buffer.getNumSamples() == 0 || getTotalNumOutputChannels() == 0)
        return;
    ```

- [ ] **Tarea 1.3: Corregir `getTailLengthSeconds()`**
  - **Archivo:** `Source/Core/PluginProcessor.cpp`
  - **Instrucción:** Cambiar la devolución de `0.0` a `3.0` segundos (cubre la cola del BBD Chorus y Tape Echo del modelo Super Six).

---

### 🎛️ Sprint 2: Compatibilidad Nativa VST3 y Control de Teclado
**Objetivo:** Cumplir al 100% con la especificación Steinberg VST3 y mejorar la experiencia en DAWs.

- [ ] **Tarea 2.1: Implementar Bypass VST3 Nativo**
  - **Archivos:** `Source/Core/ABDSimpleJuno106AudioProcessor.h`, `Source/Core/PluginProcessor.cpp`
  - **Instrucción:**
    1. Declarar `void processBlockBypassed(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override;` en el `.h`.
    2. Implementar `processBlockBypassed()` en `.cpp` garantizando:
       - Guarda de buffer cero.
       - Silenciado/Reset de voces (`synthEngine.panic()` o `voiceManager.resetAllVoices()`).
       - Limpieza de buffers MIDI.
       - Pass-through directo (copiado de canales si hay entradas o silenciado limpio).

- [ ] **Tarea 2.2: Configurar Foco de Teclado en la WebUI**
  - **Archivo:** `Source/UI/WebView/WebViewEditor.cpp`
  - **Instrucción:** En el constructor de `WebViewEditor`, añadir `setWantsKeyboardFocus(false);` para permitir el correcto flujo de atajos de teclado hacia el DAW host.

---

### 🧪 Sprint 3: Automatización de Calidad (Pluginval + CI/CD)
**Objetivo:** Asegurar la certificación de calidad e integración en el flujo de trabajo continuo.

- [ ] **Tarea 3.1: Ejecutar y validar con Pluginval**
  - **Instrucción:** Compilar la versión Release VST3 y ejecutar la validación automatizada:
    ```bash
    pluginval.exe --strictness-level 5 --seed 42 --validate "build_j106/Release/VST3/JUNiO_601.vst3"
    ```
  - **Criterio de Entrega:** El reporte debe finalizar con `ALL TESTS PASSED`.

- [ ] **Tarea 3.2: Crear el script automatizado `verify_release.ps1`**
  - **Archivo a crear:** `scripts/verify_release.ps1`
  - **Instrucción:** Portar la estructura del script de ABDEep para ABDJUNiO601, ejecutando en secuencia:
    1. Build de producción (`build_all_platforms_and_formats.bat`).
    2. Unit Tests C++ (`JunoUnitTests.cpp`).
    3. Tests de la WebUI (Vitest en `WebUI`).
    4. Test de conformidad Pluginval.

---

## 📋 3. Criterios de Aceptación para la Entrega (Definition of Done)

Para dar por aprobada la actualización del proyecto **ABDJUNiO601**:

1. **Restauración de Sesión Probada:** Al guardar una pista con retoques de sonido en Cubase/Reaper y reabrir el proyecto, el sintetizador debe sonar exactamente con los retoques guardados (sin volver al Preset 0).
2. **Cero Fallos en Pluginval:** La validación con `--strictness-level 5 --seed 42` debe terminar en `ALL TESTS PASSED`.
3. **Paso de Tests Existentes:** Todos los tests unitarios C++ (`JunoUnitTests`) y de la WebUI deben dar 100% OK.
4. **Bypass Funcional:** Al presionar el botón de Bypass en el DAW, las notas dejan de sonar inmediatamente y no se producen clics ni cuelgues.

---

> **Referencia transversal:** [Checklist de Calidad JUCE/VST3](../ABDEep/plugin_quality_checklist.md)
