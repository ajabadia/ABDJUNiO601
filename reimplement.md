# Plan de Evolución para JUNiO 601 (Post-Auditoría)

**Objetivo**: Alcanzar el 100% de fidelidad funcional y sónica con el hardware Roland Juno-106, partiendo de la base de código actual.

### Fase 1: Corrección del Motor de Voces (Crítico)
- [ ] **Solucionar Ataques Sordos**: Modificar `JunoVoiceManager` para forzar el reinicio de la envolvente y el DCO en cada `noteOn` que no sea un legato explícito.
- [ ] **Legato Auténtico**: Asegurar que el modo legato solo se active en Unison cuando una tecla esté físicamente pulsada.

### Fase 2: Alineación de Presets y SysEx (Fidelidad de Datos)
- [ ] **Presets de Fábrica**: Reemplazar los datos de `FactoryPresets.h` con los 128 volcados de la ROM original.
- [ ] **Mapeo SysEx**: Corregir la lógica de bits en `PresetManager` y `JunoSysExEngine` para el Chorus (bit 5 invertido) y el HPF (lógica descendente).
- [ ] **Checksum**: Eliminar el byte de checksum de los mensajes de Patch Dump (0x30) para cumplir el estándar de 23 bytes.

### Fase 3: Auditoría de Módulos (Hardware vs. Software)
- [ ] **LFO**: 
    - [ ] Cambiar la forma de onda de Senoidal a **Triangular**.
    - [ ] Mover el cálculo de fase para que sea **per-sample** y no por bloque.
    - [ ] Implementar el **Delay (fade-in) global** en `PluginProcessor`.
- [ ] **VCF**: Ajustar la curva logarítmica del Cutoff para que el rango útil coincida con el del IR3109 (5Hz-20kHz).
- [ ] **Chorus**: Calibrar el nivel de "hiss" para que el Modo II sea ligeramente más ruidoso que el Modo I.

### Fase 4: Sistema de Archivos Profesional (UX)
- [ ] **Importación Inteligente**: Implementar la lógica para gestionar bancos nuevos para archivos multi-patch (`.syx`) y añadir patches sueltos (`.jno`) a la librería "User".
- [ ] **Botón SAVE**: Asegurar que siempre guarde en el banco "User", escriba el archivo en disco y refresque la UI.
- [ ] **Botón EXPORT**: Corregir el ciclo de vida del `FileChooser` para permitir la exportación de bancos a JSON.
- [ ] **Persistencia de Rutas**: Guardar y recuperar la última carpeta utilizada para cualquier operación de archivo.

### Fase 5: LCD Interactivo (Final)
- [ ] **Feedback de Parámetros**: Implementar `parameterChanged` en el `PluginProcessor` para que envíe el nombre y valor del parámetro que se está editando a la UI.
- [ ] **Display Temporal**: Hacer que el LCD muestre la información del parámetro durante 2-3 segundos y luego vuelva a mostrar el nombre del preset.
