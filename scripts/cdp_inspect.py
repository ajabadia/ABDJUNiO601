#!/usr/bin/env python3
"""
Inspect WebView2 DOM via Chrome DevTools Protocol (CDP).

Refactored to use cdp_helpers.CDPClient for the CDP connection.

Usage:
    Ensure app is running with --remote-debugging-port=9222, then:
    python scripts/cdp_inspect.py
"""

import sys
import os

# Ensure scripts/ is in sys.path for sibling imports
_SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
if _SCRIPT_DIR not in sys.path:
    sys.path.insert(0, _SCRIPT_DIR)

sys.stdout.reconfigure(encoding='utf-8', errors='replace') if hasattr(
    sys.stdout, 'reconfigure') else None
os.environ['PYTHONIOENCODING'] = 'utf-8'

from cdp_helpers import CDPClient, get_cdp_ws_url


def print_section(title, content, indent=0):
    """Print a section header and content."""
    prefix = "  " * indent
    print(f"\n{prefix}[{title}]")
    if content:
        print(f"{prefix}{content}")
    else:
        print(f"{prefix}(empty)")


def main():
    print("=" * 60)
    print("CDP WEBVIEW INSPECTOR (using cdp_helpers)")
    print("=" * 60)

    # 1. Get WebSocket URL
    print("\n[1] Fetching CDP WebSocket URL...")
    ws_url = get_cdp_ws_url(title_filter="ABD")
    if not ws_url:
        ws_url = get_cdp_ws_url()  # fallback: any page
    if not ws_url:
        print("[1] FAILED: Could not connect to CDP on port 9222")
        return
    print(f"[1] WS URL: {ws_url}")

    # 2. Connect
    print("\n[2] Connecting CDP...")
    cdp = CDPClient(ws_url)
    cdp.eval_js("1")  # warm-up / drain
    print("[2] Connected!")

    # 3. Page info
    print("\n[3] Page Info:")
    title = cdp.eval_js("document.title")
    print(f"  Title: {title}")

    body_html = cdp.eval_js(
        "document.body ? document.body.innerHTML.substring(0, 5000) : 'NO_BODY'"
    )
    print_section("BODY HTML (first 5000 chars)", body_html, indent=1)

    # 4. Modals
    modal = cdp.eval_js(
        "(function(){var e=document.getElementById('modal-smartImport');"
        "return e ? e.outerHTML.substring(0, 2000) : 'NOT_FOUND'})()"
    )
    print_section("SMART IMPORT MODAL", modal, indent=1)

    all_modals = cdp.query_selector_all(
        ".modal-overlay, [class*=modal], dialog"
    )
    print_section("ALL MODALS", " | ".join(all_modals) if all_modals else "None", indent=1)

    # 5. Tap/import related elements
    tape_els = cdp.query_selector_all(
        "[class*=tape],[class*=Tape],[class*=import],[class*=Import],"
        "[id*=tape],[id*=Tape],[id*=import],[id*=Import]"
    )
    print_section("TAPE/IMPORT ELEMENTS",
                  " | ".join(tape_els) if tape_els else "None", indent=1)

    # 6. Buttons
    buttons = cdp.eval_js(
        "(function(){var r=[];"
        "document.querySelectorAll('button').forEach(function(b){"
        "r.push((b.textContent||'').trim().substring(0,40));"
        "});return r.join(' | ')})()"
    )
    print_section("BUTTONS", buttons, indent=1)

    # 7. Format sections presence
    print_section("FORMAT SECTIONS", "", indent=1)
    for section_id in ["si-preset-summary", "si-preset-names-section",
                        "si-tape-section", "si-sysex-section", "si-csv-section"]:
        exists = cdp.eval_js(f"document.getElementById('{section_id}') !== null")
        print(f"    {section_id}: {'✅ Present' if exists else '❌ Missing'}")

    # 8. Body classes
    body_cls = cdp.eval_js("document.body ? document.body.className : 'NO_BODY'")
    print_section("BODY CLASSES", body_cls, indent=1)

    # 9. Top-level sections
    sections = cdp.query_selector_all("body > *")
    print_section("TOP-LEVEL SECTIONS",
                  " | ".join(sections) if sections else "None", indent=1)

    cdp.close()
    print("\n[DONE]")


if __name__ == "__main__":
    main()
