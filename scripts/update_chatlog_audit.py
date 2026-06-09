#!/usr/bin/env python3
"""Append CSS audit results to chat_log.md."""

NEW_SECTION = """

### 38.11 Auditoría Completa de Clases CSS (245 clases analizadas)

Se realizó una auditoría automatizada (`scripts/audit_css_classes.py`) que cruza todas las clases definidas en 13 archivos CSS contra las referencias en 10 archivos HTML/JS.

| Métrica | Cantidad |
|:--------|:--------:|
| Clases CSS totales | **245** |
| Referencias en HTML/JS | **291** |
| Clases en uso (ambos) | **230** |
| CSS muerto (solo en CSS) | **15** (8 reales, 7 falsos positivos) |
| Huérfanas (solo en HTML/JS) | **61** |

#### CSS Muerto Verificado (8 clases)

| Clase | Archivo | Descripción |
|:------|:--------|:------------|
| `.blink` | performance.css | Animación oct-led (0.5s) — JS nunca la togglea |
| `.blink-fast` | performance.css | Animación oct-led rápida (0.25s) — JS nunca la togglea |
| `.fade-out` | base.css | `#splash-screen.fade-out` — posiblemente no usada |
| `.error` | style.css | `.notification-toast.error` — estilo de notificación de error |
| `.success` | style.css | `.notification-toast.success` — estilo de notificación de éxito |
| `.ab-util-btn` | browser.css | Botón utilidad browser |
| `.browser-action-btn` | browser.css | Botón acción browser |
| `.info-actions` | browser.css | Contenedor acciones browser |

#### Clases Huérfanas Más Significativas (61 total)

| Grupo | Clases | Estado |
|:------|:-------|:------:|
| **Panel Delay (13)** | `.delay-ec-led`, `.delay-knob-lbl`, `.delay-knob-ring`, `.delay-knob-unit`, `.delay-knobs-group`, `.delay-nav`, `.delay-nav-lbl`, `.delay-nav-unit`, `.delay-preset-knob`, `.delay-subpage`, `.delay-subpage-btn`, `.delay-sync-row-inline`, `.delay-tab-led` | Sin CSS |
| **Performance Tabs (6)** | `.perf-tab-btn`, `.perf-tab-content`, `.perf-tab-nav-btn`, `.perf-tabs-carousel`, `.perf-tabs-scroll`, `.performance-tabs` | Sin CSS |
| **Smart Import (4)** | `.smart-import-body`, `.smart-import-container`, `.smart-import-footer`, `.smart-import-header` | Sin CSS |
| **Preset Knobs (3)** | `.preset-knob-body`, `.preset-knob-img`, `.preset-knob-pointer` | Sin CSS |
| **Knob layout (2)** | `.knob-column`, `.knob-unit` | Sin CSS |
| **Varios (20+)** | `.record-dot`, `.recording`, `.save-field`, `.poly-btn`, `.poly-mode-btn`, `.patch-btn`, `.patch-grid`, `.changed`, `.hex-byte`, `.v-slider-mini`, `.h-slider-handle`, `.h-slider-input`, `.modal-content`, `.nav-lbl`, `.model-selector-row`, `.heads-bezel`, `.echo-cancel-fo`, `.sw-col`, `.tab-led`, `.tab-nav-arrow` | Sin CSS |

### 38.12 Auditoría style.css vs service.css vs browser.css

Se verificó que no hay reglas redundantes entre los 3 archivos CSS que se cargan via `@import` desde `style.css`:

| Archivo | Responsabilidad | Clases exclusivas |
|:--------|:----------------|:-----------------:|
| `style.css` | Hub @import + `.notification-toast` | ~5 |
| `service.css` | Service mode + Calibration + Smart Import | ~50 |
| `browser.css` | Preset browser + Search + A/B compare | ~80 |

**Resultado: 0 reglas redundantes** ✅

- Ningún nombre de clase aparece en más de un archivo
- `.primary` y `.active` se usan en múltiples archivos pero siempre como selectores compuestos (`.footer-action-btn.primary`, `.badge.active`) — sin conflictos
- `:root` variables en browser.css usan prefijo `--browser-*` y `--pane-*` — no chocan con `css/vars.css`
- Único `@keyframes`: `pulse-status` en service.css
- Único `@font-face`: `SevenSegment` en browser.css

### 38.13 Herramienta de Auditoría Creada

| Archivo | Descripción |
|:--------|:------------|
| `scripts/audit_css_classes.py` | Script Python que extrae clases CSS de 13 archivos, referencias de 10 HTML/JS, y cruza ambos sets para identificar CSS muerto y clases huérfanas |

---

"""

def main():
    with open('chat_log.md', 'r', encoding='utf-8') as f:
        content = f.read()
    
    marker = '*Fin del registro'
    idx = content.find(marker)
    if idx < 0:
        print('ERROR: marker not found')
        return
    
    # Insert new sections before the marker
    insert = NEW_SECTION
    new_content = content[:idx] + insert + content[idx:]
    
    with open('chat_log.md', 'w', encoding='utf-8', newline='') as f:
        f.write(new_content)
    
    print(f'SUCCESS: Inserted {len(insert)} chars before "{marker}"')
    # Verify the end of the file
    print('--- Last 100 chars of updated file:')
    with open('chat_log.md', 'r', encoding='utf-8') as f:
        f.seek(-150, 2)
        print(f.read())


if __name__ == '__main__':
    main()
