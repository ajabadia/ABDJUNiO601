#!/usr/bin/env node
/**
 * ABDJUNiO601 — Guard de dirección del canal nativo → JS de JUCE WebView2.
 *
 * Este repo no tiene runner JS, así que el job "webview-guard" de CI lanza este
 * script. La lógica NO vive aquí: se delega en el runner compartido por toda la
 * suite (`ABDSharedCode/WebView2Bridge/testing/runGuard.mjs`), que a su vez usa el
 * módulo del guard (`webviewBridgeDirection.js`). Este fichero solo aporta la
 * configuración del host, para que no haya seis implementaciones distintas.
 *
 * Qué vigila el guard:
 *   - nativo → JS: `WebBrowserComponent::emitEventIfBrowserIsVisible(eventId, obj)`
 *     (el único que dispara los `window.__JUCE__.backend.addEventListener(...)`).
 *   - JS → nativo: `window.__JUCE__.backend.emitEvent(eventId, obj)`.
 *   Escribir el segundo desde C++ (típicamente con `evaluateJavascript`) no llega a
 *   ningún listener del WebUI y **se descarta en silencio**: así se perdió todo el
 *   `cppToWebui` de ABDMS2000 (hostModel incluido) hasta el handoff §17.
 *
 * Uso:  node scripts/check_webview_bridge_direction.mjs
 *       (lo ejecuta el job "webview-guard" de .github/workflows/ci.yml; sale con 1
 *        si el canal va en la dirección equivocada o si un emisor no está declarado)
 */

import { spawnSync } from 'node:child_process';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const here = path.dirname(fileURLToPath(import.meta.url));
const repoRoot = path.resolve(here, '..');

const runnerPath = path.resolve(here, '../../ABDSharedCode/WebView2Bridge/testing/runGuard.mjs');

/** Ficheros del proyecto que emiten del C++ hacia el WebUI. */
const EMITTERS = ['UI/WebView/WebViewEditor.cpp'];

const result = spawnSync(
  process.execPath,
  [
    runnerPath,
    '--source',
    path.join(repoRoot, 'Source'),
    '--emitters',
    EMITTERS.join(','),
    '--expect-host',
    EMITTERS[0],
    '--label',
    'ABDJUNiO601'
  ],
  { stdio: 'inherit' }
);

if (result.error) {
  console.error(`[guard webview2] no pude lanzar el runner compartido: ${result.error.message}`);
  process.exit(2);
}

process.exit(result.status ?? 1);
