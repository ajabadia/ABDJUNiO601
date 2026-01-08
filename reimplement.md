# Guía de Reimplementación para JUNiO 601 - ESTADO ACTUAL

Este documento registra la evolución del proyecto tras la recuperación. **Meta actual: Fidelidad de Hardware y Estabilidad Absoluta.**

## ✅ 1. Identidad y Construcción (Completado)
- **Marca**: Plugin renombrado a **JUNiO 601**.
- **Build System**: Implementado contador automático en `build_standalone.bat`.
- **Header**: Visualización dinámica de Build # y Timestamp en la UI.

## ✅ 2. Motor de Audio y Fidelidad (Completado)
- **DCO Phase Sync**: `dco.reset()` en Note-On (notas no-legato) para ataques consistentes.
- **Ganancia**: Aumento de `kVoiceOutputGain` a **0.75f** para corregir el volumen bajo.
- **HPF Auténtico**: Implementación de 4 posiciones (Bass Boost, Flat, 225Hz, 450Hz) con lógica de filtros IIR precisa.
- **LFO Global Monofónico**: Fase gestionada centralmente en `PluginProcessor`. Cada voz aplica su propia rampa de **Delay (fade-in)** de hasta 5 segundos.
- **Chorus Hiss**: Emulación de ruido BBD calibrada. El Modo II produce un 50% más de hiss que el Modo I.

## ✅ 3. Conectividad y SysEx (Completado)
- **Bidireccionalidad 0x32**: El plugin envía mensajes SysEx de "Parameter Change" en tiempo real al mover controles.
- **Manual Mode 0x31**: Envío de comando Manual al pulsar el botón físico en la UI.
- **Mapeo de 18 Bytes**: Sincronización total del orden de parámetros para compatibilidad con dumps reales.
- **Tape Decoder Pro**: Añadida **Auto-Normalización** y detección por **Ventana de Energía** para soportar grabaciones ruidosas.

## ✅ 4. Interfaz de Usuario (Completado)
- **Estabilidad**: `resized()` restaurado con coordenadas fijas (750px de altura) para evitar solapamientos.
- **Nuevos Controles**:
    - **GROUP A/B**: Gestión de bancos de 64 patches.
    - **MANUAL**: Sincronización instantánea con el panel.
    - **RANDOM**: Generador de patches basado en reglas de síntesis musical.
    - **EXPORT**: Exportación de librerías completas a JSON.
    - **PANIC**: Botón "ALL OFF" para resetear voces zombis.

## ✅ 5. Estabilidad de Código (Completado)
- **Zero Warnings**: Eliminados todos los avisos C4100, C4996 y C4244.
- **JUCE 8 Ready**: Migración de APIs de fuentes (`FontOptions`) y gestión de memoria.

## ⏳ 6. Próximo Paso: Monitor LCD (Pendiente)
- Implementar la **Persistencia de Parámetros**: Mostrar temporalmente el nombre y valor (0-127) del control editado antes de volver al nombre del patch.
- Implementar `juce::AsyncUpdater` para el paso seguro de textos del hilo de audio a la UI.
