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
 * El guard tiene dos mitades:
 *   1. Negativa (completa): `checkBridgeDirection` barre **todo** `Source/` buscando
 *      `backend.emitEvent` en C++. Ningún fichero queda fuera de ese barrido, esté o
 *      no declarado abajo.
 *   2. Positiva (declarativa): `EMITTERS` fija qué ficheros DEBEN emitir con
 *      `emitEventIfBrowserIsVisible`; si uno deja de hacerlo, el guard falla.
 *      Para que esa lista no se quede corta, el script verifica además que todo
 *      fichero C++ que monte un `WebBrowserComponent` (host de WebView, es decir,
 *      emisor potencial) esté declarado en `EMITTERS`.
 *
 * Uso:  node scripts/check_webview_bridge_direction.mjs
 *       (lo ejecuta el job "webview-guard" de .github/workflows/ci.yml; sale
 *        con 1 si el canal va en la direccion equivocada o si hay un host de
 *        WebView sin declarar)
 */

import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import {
  checkBridgeDirection,
  formatFindings,
  listCppSources,
  stripCppComments
} from '../../ABDSharedCode/WebView2Bridge/testing/webviewBridgeDirection.js';

const here = path.dirname(fileURLToPath(import.meta.url));
const repoRoot = path.resolve(here, '..');
const sourceRoot = path.join(repoRoot, 'Source');

/** Ficheros del proyecto que emiten del C++ hacia el WebUI. */
const EMITTERS = ['UI/WebView/WebViewEditor.cpp'];

/**
 * Marcadores de "aquí se monta el componente web": un fichero que configure
 * opciones del `WebBrowserComponent` es un host de WebView y, por tanto, un
 * emisor potencial que debe figurar en `EMITTERS`.
 */
const WEB_HOST_MARKERS = ['withNativeFunction', 'withResourceProvider'];

const toPosix = p => p.split(path.sep).join('/');

const declaredEmitters = new Set(EMITTERS.map(toPosix));
const undeclaredWebHosts = listCppSources(sourceRoot)
  .map(file => ({
    file: toPosix(path.relative(sourceRoot, file)),
    source: stripCppComments(fs.readFileSync(file, 'utf8'))
  }))
  .filter(({ source }) => WEB_HOST_MARKERS.some(marker => source.includes(marker)))
  .map(({ file }) => file)
  .filter(file => !declaredEmitters.has(file));

const result = checkBridgeDirection({ sourceRoot, emitters: EMITTERS });

const problems = [
  ...result.problems,
  ...undeclaredWebHosts.map(
    file => `${file} monta un WebBrowserComponent pero no está en EMITTERS: añádelo al guard.`
  )
];
const ok = problems.length === 0;

console.log(
  `[guard webview2] ${result.scanned} fuentes C++ revisados en ` +
    `${toPosix(path.relative(repoRoot, sourceRoot))}`
);
console.log(
  ok
    ? formatFindings({ ...result, ok: true })
    : `Dirección del canal WebView2 incorrecta (${problems.length} problema(s)):\n` +
      problems.map(problem => `  - ${problem}`).join('\n')
);
console.log(`[guard webview2] emisores declarados: ${EMITTERS.join(', ')}`);

process.exit(ok ? 0 : 1);
