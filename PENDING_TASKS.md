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
- [ ] **Verificar Tracking**: Asegurar que el `juce::UndoManager` está capturando transacciones desde `APVTS` para cada cambio de parámetro.
- [ ] **Sincronización de UI**: Al hacer Undo, los sliders/botones deben actualizar su posición visual (puede requerir llamadas a `component.sendLookAndFeelChange()` o listeners específicos).

### 2. Menú de Opciones
- [ ] **Implementar Diálogo**: El ítem "Options..." en el menú Edit está creado pero no tiene acción asociada.
- [ ] **Configuraciones Sugeridas**:
    - Selección de Canal MIDI Global.
    - Inversión de Pedal de Sustain.
    - Rutas para la librería de presets.

### 3. Auditoría de Fidelidad de Audio
- [ ] **VCF Self-oscillation**: Calibrar el filtro para que comience a oscilar exactamente donde lo hace el hardware original (Resonancia cerca del máximo).
- [ ] **Curvas ADSR**: Comparar los tiempos de ataque y decaimiento con mediciones de un Juno-106 real (curvas exponenciales vs lineales).
- [ ] **Sub-oscillator**: Verificar que la fase y el peso armónico de la onda cuadrada del sub-osc sean idénticos al original.

### 4. Skin Manager y Personalización
- [ ] **Desarrollar `SkinManager.h`**: Se ha creado un esqueleto para alternar entre el look "Classic Blue/Grey" y el "Juno-106S" (negro). 
- [ ] **Tokens de Diseño**: Conectar los colores de `DesignTokens.h` con el `SkinManager` para cambios en caliente.

### 5. Manual Mode Logic
- [ ] **Snapshot Inicial**: Al activar "MANUAL", el motor debe capturar inmediatamente el estado físico de TODOS los sliders de la interfaz y enviarlos al DSP de una vez (actualmente se hace vía APVTS pero falta verificar la totalidad de parámetros).

## Archivos de Referencia Clave
- `PluginProcessor.cpp`: Lógica de SysEx y gestión de parámetros.
- `PluginEditor.cpp`: Layout de la nueva interfaz y callbacks de menú.
- `JunoBankSection.h`: Definición de los nuevos botones momentáneos.
- `FactoryPresets.h`: Datos crudos de los 128 parches originales.

---
**Nota para Antigravity:** Al retomar, puedes empezar revisando la implementación del `UndoManager` en el `PluginProcessor` y asegurarte de que el listener en `PluginEditor` responda a las acciones de deshacer.
