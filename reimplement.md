# Guía de Reimplementación para JUNiO 601 - ESTADO ACTUAL

Este documento registra la evolución del proyecto tras la recuperación.

## ✅ 1. Identidad y Construcción (Completado)
- **Marca**: Plugin renombrado a **JUNiO 601**.
- **Build System**: Implementado contador automático y versión dinámica en el header.

## ✅ 2. Motor de Audio y Modulación (Completado)
- **DCO/HPF**: Reset de fase en Note-On y lógica de 4 posiciones del filtro HPF corregida.
- **LFO Global**: Implementada **onda triangular** real y delay global monofónico per-sample.
- **Chorus Hiss**: Ruido dinámico calibrado (Modo II > Modo I).
- **Voice Stealing**: Corregidos ataques "sordos" mediante re-trigger forzado de envolventes.

## ✅ 3. Conectividad y SysEx (Completado)
- **Bidireccionalidad**: Envío de Parameter Change (0x32) y Manual Mode (0x31).
- **Protocolo**: Ajustado a 23 bytes (sin Checksum) para compatibilidad 1:1 con hardware Roland.

## ✅ 4. Gestión de Memoria Pro (NUEVO)
- **Importación Inteligente**: Soporte nativo para `.syx` y `.jno`. Creación automática de bancos para archivos multi-patch.
- **Banco User Dinámico**: Los parches individuales se acumulan en la librería User manteniendo el nombre del archivo original.
- **Persistencia de Rutas**: El plugin recuerda la última carpeta de importación/exportación entre sesiones.
- **Exportación JSON**: Recuperada la función de volcado de bancos individuales o backup total de la librería.

## ✅ 5. Interfaz de Usuario (Completado)
- **Ergonomía**: Ancho ampliado a 1700px. Botones físicos con LED para ondas y chorus.
- **Layout**: Alineación perfecta de rejilla de memoria y utilidades centrales [ALL OFF][TEST][RANDOM].

## ⏳ 6. Próximo Paso: Monitor LCD (Pendiente)
- Implementar la persistencia de parámetros temporal para visualización de edición.
