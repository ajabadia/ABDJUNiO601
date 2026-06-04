#!/usr/bin/env python3
"""
Test: Open Smart Tape Import dialog via UI navigation.

Flow:
  Launch app → Click [BROWSER] → Click LOAD TAPE (.WAV) →
  Handle native file dialog → Verify Smart Tape Import modal.

Refactored to use cdp_helpers and win32_dialog modules.

Usage:
    Ensure app is running with --remote-debugging-port=9222, then:
    python scripts/test_menu_tape_import.py
"""

import sys
import os
import time

# Ensure scripts/ is in sys.path for sibling imports
_SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
if _SCRIPT_DIR not in sys.path:
    sys.path.insert(0, _SCRIPT_DIR)

from cdp_helpers import CDPClient, get_cdp_ws_url, wait_for_modal
from win32_dialog import find_file_dialog, set_file_and_open

# Resolve WAV file relative to project root
_PROJECT_ROOT = os.path.dirname(_SCRIPT_DIR)
WAV_FILE = os.path.join(_PROJECT_ROOT, "docs", "JUNO-106",
                        "Roland Juno-60 factory programs group 1.wav")
if not os.path.exists(WAV_FILE):
    WAV_FILE = os.path.join(_PROJECT_ROOT, "docs", "JUNO-106",
                            "JUNO106 Bank A.wav")


def main():
    print("=" * 60)
    print("TEST: Smart Tape Import via UI Navigation (refactored)")
    print("=" * 60)
    print(f"WAV: {WAV_FILE}")
    print()

    # ── 1. CDP ──────────────────────────────────────────────────────
    print("[1] Connecting CDP...")
    ws_url = get_cdp_ws_url(title_filter="ABD")
    if not ws_url:
        ws_url = get_cdp_ws_url()
    if not ws_url:
        print("[1] FAILED")
        return False
    print(f"[1] OK: {ws_url}")

    cdp = CDPClient(ws_url)
    cdp.eval_js("1")  # warm-up
    print("[1] CDP ready")
    print()

    # ── 2. Wait for app ─────────────────────────────────────────────
    print("[2] Waiting for app load...")
    time.sleep(5)
    title = cdp.eval_js("document.title")
    splash = cdp.eval_js(
        "document.getElementById('splash-screen').style.display"
    )
    print(f"[2] Title: {title}")
    print(f"[2] Splash: {splash}")
    print()

    # ── 3. Click [BROWSER] ──────────────────────────────────────────
    print("[3] Opening Preset Browser...")
    result = cdp.click_by_text("BROWSER")
    print(f"[3] Browser: {result}")
    time.sleep(1.5)

    browser_display = cdp.eval_js(
        "document.getElementById('modal-browser').style.display"
    )
    print(f"[3] Browser modal: {browser_display}")
    print()

    # ── 4. Click LOAD TAPE ──────────────────────────────────────────
    print("[4] Clicking LOAD TAPE (.WAV)...")
    result = cdp.click_by_id("btn-load-tape")
    print(f"[4] Load Tape: {result}")
    print()

    # ── 5. File dialog ──────────────────────────────────────────────
    print("[5] Handling native file dialog...")
    time.sleep(2)
    dlg_hwnd, edit_hwnd = find_file_dialog(title_filter="Load Tape",
                                            timeout=25)
    if not dlg_hwnd:
        print("[5] FAILED: No file dialog")
        cdp.close()
        return False

    ok = set_file_and_open(dlg_hwnd, edit_hwnd, WAV_FILE)
    print(f"[5] Dialog: {ok}")
    print()

    # ── 6. Wait for modal ──────────────────────────────────────────
    print("[6] Waiting for Smart Import modal...")
    _, btn_enabled = wait_for_modal(
        cdp, modal_id="modal-smartImport",
        enable_on="btn-si-import",
        timeout=60, poll_interval=1
    )
    print()

    # ── 7. Report ──────────────────────────────────────────────────
    print("[7] FINAL REPORT:")

    if btn_enabled:
        metrics = cdp.eval_js("""JSON.stringify({
            snr: (document.getElementById('si-snr')||{}).innerText,
            jit: (document.getElementById('si-jitter')||{}).innerText,
            dro: (document.getElementById('si-dropouts')||{}).innerText,
            dur: (document.getElementById('si-duration')||{}).innerText,
            qlt: (document.getElementById('si-quality-badge')||{}).innerText
        })""")
        print(f"  Metrics: {metrics}")

        dec = cdp.eval_js(
            "(document.getElementById('si-decoder-list')||{}).children.length || 0"
        )
        print(f"  Decoder entries: {dec}")

        canvas = cdp.eval_js(
            "var c=document.getElementById('si-waveform-canvas'); "
            "c ? c.width+'x'+c.height : 'NONE'"
        )
        print(f"  Waveform: {canvas}")

        # Format badge
        badge = cdp.eval_js(
            "(document.getElementById('si-format-badge')||{}).innerText"
        )
        print(f"  Format badge: {badge}")

        print(f"\n{'=' * 60}")
        print("** TEST PASSED ** — Dialog opened via UI navigation!")
        print(f"{'=' * 60}")
    else:
        debug = cdp.eval_js("""JSON.stringify({
            modal: (document.getElementById('modal-smartImport')||{}).style.display,
            btn: (document.getElementById('btn-si-import')||{}).disabled,
            log: (document.getElementById('si-progress-log')||{}).innerText || ''
        })""")
        print(f"  Debug: {debug}")
        print(f"\n{'=' * 60}")
        print("** TEST FAILED **")
        print(f"{'=' * 60}")

    cdp.close()
    return btn_enabled


if __name__ == "__main__":
    ok = main()
    sys.exit(0 if ok else 1)
