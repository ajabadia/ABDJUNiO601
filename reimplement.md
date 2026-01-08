# Guía de Reimplementación para JUNiO 601

Esta guía resume los cambios exitosos realizados durante la sesión para ser aplicados sobre la copia de seguridad. **Evitar reescrituras parciales que causen duplicidad de funciones.**

## 1. Identidad y Marca
- Cambiar el nombre visible del sintetizador a **JUNiO 601**.
- Actualizar `PluginEditor.cpp` (método `paint`) para mostrar el nombre a la izquierda y la info de compilación a la derecha.

## 2. Sistema de Compilación (build_standalone.bat)
- Implementar el contador automático en el `.bat`:
  - Incremento de `build_no.txt`.
  - Generación de `Source/Core/BuildVersion.h`.

## 3. Motor de Audio y Voces
### DCO
- En `Voice::noteOn`, llamar a `dco.reset()` para notas no legatas (sincronización de fase).
- Aumentar `JunoConstants::kVoiceOutputGain` a **0.75f** para corregir el volumen bajo.

### HPF (Lógica Inversa)
- El fader del HPF en el Juno-106 tiene 4 posiciones:
  - **0 (Bits 11)**: Bass Boost (Low Shelf @ 80Hz, +6dB).
  - **1 (Bits 10)**: Plano / Off (High Pass @ 10Hz).
  - **2 (Bits 01)**: High Pass @ 225Hz.
  - **3 (Bits 00)**: High Pass @ 450Hz.

### LFO Global (Monofónico)
- Mover la fase del LFO a `PluginProcessor.cpp`.
- Las voces deben recibir el valor actual del LFO en `renderNextBlock(..., float masterLfoValue)`.
- `JunoLFO` debe gestionar solo la rampa de **fade-in (delay)** de forma individual por voz.

## 4. Mapeo de Parámetros (18 Bytes)
Sincronizar `createPresetFromJunoBytes` en `PresetManager.cpp` con este orden:
- 0: LFO Rate, 1: LFO Delay, 2: LFO to DCO, 3: DCO PWM, 4: DCO Noise... (ver implementación en PresetManager.cpp).

## 5. Mejoras en Carga de Archivos
- **JunoTapeDecoder.h**: Añadir normalización y detección de bits por "ventana de energía".
- **PresetManager.cpp**: `addLibraryFromSysEx` debe saltar el byte 4 en mensajes `0x30`.

## 6. Interfaz de Usuario (JunoControlSection)
- Restaurar el método `resized()` con coordenadas fijas.
- Botón **SAVE**: Guardar preset actual.
- Botón **EXPORT**: Exportar banco completo a JSON.
- **Botón RANDOM**: Añadir funcionalidad para generar patches aleatorios musicales.

## 7. Próximo paso: Monitor LCD
- Implementar captura de parámetros y visualización temporal 0-127.
