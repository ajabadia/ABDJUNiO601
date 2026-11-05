#!/usr/bin/env node
/**
 * ABDJUNiO601 — Guard de dirección del canal nativo → JS de JUCE WebView2.
 *
 * En JUCE 8 hay dos canales opuestos:
 *   - nativo → JS: `WebBrowserComponent::emitEventIfBrowserIsVisible(eventId, obj)`
 *     (el único que dispara los `window.__JUCE__.backend.addEventListener(...)`).
 *   - JS → nativo: `window.__JUCE__.backend.emitEvent(eventId, obj)`
 *     (lo recoge el `withEventListener(...)` nativo).
 *
 * Escribir el segundo desde C++ (típicamente con `evaluateJavascript`) no llega a
 * ningún listener del WebUI y **se descarta en silencio**: así se perdió todo el
 * `cppToWebui` de ABDMS2000 (hostModel incluido) hasta el handoff §17.
 *
 * Este proyecto no tiene runner JS (ni `package.json`), así que el guard es este
 * script: usa la MISMA lógica compartida por la suite
 * (`ABDSharedCode/WebView2Bridge/testing/webviewBridgeDirection.js`), la que
 * ejecutan ABDMS2000 y ABDCZ101 desde vitest.
 *
 * Uso:  node scripts/check_webview_bridge_direction.mjs
 *       (lo ejecuta el job "webview-guard" de .github/workflows/ci.yml; sale
 *        con 1 si el canal va en la direccion equivocada)
 */

import path from 'node:path';
import { fileURLToPath } from 'node:url';
import {
  checkBridgeDirection,
  formatFindings
} from '../../ABDSharedCode/WebView2Bridge/testing/webviewBridgeDirection.js';

const here = path.dirname(fileURLToPath(import.meta.url));
const repoRoot = path.resolve(here, '..');
const sourceRoot = path.join(repoRoot, 'Source');

/** Ficheros del proyecto que emiten del C++ hacia el WebUI. */
const EMITTERS = ['UI/WebView/WebViewEditor.cpp'];

const result = checkBridgeDirection({ sourceRoot, emitters: EMITTERS });

console.log(
  `[guard webview2] ${result.scanned} fuentes C++ revisados en ` +
    `${path.relative(repoRoot, sourceRoot).split(path.sep).join('/')}`
);
console.log(`[guard webview2] ${formatFindings(result)}`);

process.exit(result.ok ? 0 : 1);
