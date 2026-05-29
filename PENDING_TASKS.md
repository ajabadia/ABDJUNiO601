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

### 2. Menú de Opciones
- [ ] **Implementar Diálogo**: El ítem "Options..." en el menú Edit está creado pero no tiene acción asociada.
- [ ] **Configuraciones Sugeridas**:
    - Selección de Canal MIDI Global.
    - Inversión de Pedal de Sustain.
    - Rutas para la librería de presets.

### 3. Auditoría de Fidelidad de Audio
- [ ] **VCF Self-oscillation**: Corregir e integrar los parámetros dinámicos `selfOscThreshold` y `selfOscInt` en `JunoVCF::processSample` (actualmente se pasan pero se ignoran, usando solo `ResK_J106`).
- [x] **Curvas ADSR**: Verificado. Se emula la tasa de ticks de 4.23ms del uPD7811G, la atenuación del 6% en CalcDecay y curvas exponenciales realistas.
- [x] **Sub-oscillator**: Verificado. Implementado mediante PolyBLEP y filtro pasivo RC Lowpass sintonizado a 4.2 kHz para el rolloff real.

### 4. Skin Manager y Personalización
- [x] **Desarrollar Skin Manager**: Implementado dinámicamente mediante el parámetro `skinType` en `CalibrationSettings` y controlado mediante clases/atributos en la WebUI.
- [x] **Tokens de Diseño**: Sincronización de colores en caliente mediante variables CSS raíz redefinidas dinámicamente.

### 5. Manual Mode Logic
- [x] **Snapshot Inicial**: Al activar "MANUAL", el motor debe capturar inmediatamente el estado físico de TODOS los sliders de la interfaz y enviarlos al DSP de una vez (verificado y validado síncronamente en el procesador).

## Archivos de Referencia Clave
- `PluginProcessor.cpp`: Lógica de SysEx y gestión de parámetros.
- `PluginEditor.cpp`: Layout de la nueva interfaz y callbacks de menú.
- `JunoBankSection.h`: Definición de los nuevos botones momentáneos.
- `FactoryPresets.h`: Datos crudos de los 128 parches originales.

---
**Nota para Antigravity:** Al retomar, puedes empezar revisando la implementación del `UndoManager` en el `PluginProcessor` y asegurarte de que el listener en `PluginEditor` responda a las acciones de deshacer.
