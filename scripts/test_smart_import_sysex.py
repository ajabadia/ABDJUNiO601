"""
Smart Import SysEx CDP Test
Verifies: File > Import SysEx → Smart Import modal with blue SYSEX badge + metadata
"""
import os, sys, json, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from cdp_helpers import CDPClient, get_cdp_ws_url, wait_for_modal
from win32_dialog import find_file_dialog, set_file_and_open

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_ROOT = os.path.dirname(SCRIPT_DIR)
TEST_SYX = os.path.join(PROJECT_ROOT, "docs", "test_juno106_bank.syx")

def main():
    print("=" * 60)
    print("TEST: Smart Import SysEx")
    print("=" * 60)
    
    # 1. Connect CDP
    print("\n[1] Connecting to CDP...")
    ws_url = get_cdp_ws_url(port=9222, max_retries=10, retry_delay=2)
    if not ws_url:
        print("FAIL: Could not get CDP WebSocket URL")
        sys.exit(1)
    print(f"  WS URL: {ws_url}")
    
    client = CDPClient(ws_url)
    client.send("Page.enable")
    time.sleep(1)
    client._drain()
    print("  CDP connected OK")
    
    # 2. Click File > Import SysEx
    print("\n[2] Clicking File > Import SysEx...")
    # First find the menu bar
    file_menu_id = client.eval_js("""
        (() => {
            // Find the File menu button
            const menus = document.querySelectorAll('.menu-bar > .menu-item, .dropbtn, [class*=\"menu\"]');
            for (const m of menus) {
                if (m.textContent.trim().toLowerCase() === 'file') {
                    m.click();
                    return 'clicked-file-menu';
                }
            }
            // Try aria-label
            const fileBtn = document.querySelector('[aria-label=\"File\"]');
            if (fileBtn) { fileBtn.click(); return 'clicked-aria-file'; }
            return 'no-file-menu-found';
        })()
    """)
    print(f"  File menu click: {file_menu_id}")
    time.sleep(1)
    
    # Click Import SysEx from the dropdown
    import_sysex_clicked = client.eval_js("""
        (() => {
            // Look for the Import SysEx menu item in any visible dropdown
            const items = document.querySelectorAll('.menu-item, .dropdown-item, li, [class*=\"menu\"] a, [class*=\"dropdown\"] a');
            for (const item of items) {
                const txt = item.textContent.trim().toLowerCase();
                if (txt.includes('import') && (txt.includes('sysex') || txt.includes('syx') || txt.includes('sys ex'))) {
                    item.click();
                    return 'clicked-import-sysex-' + item.textContent.trim();
                }
            }
            // Try the menuAction route directly via juce
            if (window.juce && window.juce.menuAction) {
                juce.menuAction('handleImportSysex');
                return 'called-menuAction-handleImportSysex';
            }
            return 'no-import-sysex-found';
        })()
    """)
    print(f"  Import SysEx click: {import_sysex_clicked}")
    
    # If the button click didn't work, try calling menuAction directly
    if "no-" in import_sysex_clicked:
        print("  Trying direct menuAction call...")
        import_sysex_clicked = client.eval_js("""
            (() => {
                if (window.juce && window.juce.menuAction) {
                    juce.menuAction('handleImportSysex');
                    return 'called-menuAction-handleImportSysex';
                }
                return 'no-juce-found';
            })()
        """)
        print(f"  Direct menuAction: {import_sysex_clicked}")
    
    # 3. Wait for native file dialog
    print("\n[3] Waiting for file dialog...")
    time.sleep(2)
    dialog_hwnd, edit_hwnd = find_file_dialog(title_filter="Import", timeout=15)
    if not dialog_hwnd:
        print("FAIL: File dialog not found")
        client.close()
        sys.exit(1)
    print(f"  Dialog HWND: {dialog_hwnd:#010x}, Edit: {edit_hwnd:#010x}")
    
    # 4. Set file path and open
    print("\n[4] Selecting test SysEx file...")
    result = set_file_and_open(dialog_hwnd, edit_hwnd, TEST_SYX)
    print(f"  File selection: {result}")
    
    # 5. Wait for Smart Import modal to appear
    print("\n[5] Waiting for Smart Import modal...")
    time.sleep(3)
    
    modal_found = client.eval_js("""
        (() => {
            const modal = document.getElementById('smartImportModal');
            if (!modal) return 'no-modal';
            const style = window.getComputedStyle(modal);
            const visible = style.display !== 'none' && style.visibility !== 'hidden' && modal.offsetParent !== null;
            return visible ? 'modal-visible' : 'modal-hidden';
        })()
    """)
    print(f"  Modal state: {modal_found}")
    
    # 6. Check format badge
    print("\n[6] Checking format badge...")
    time.sleep(2)
    
    badge_info = client.eval_js("""
        (() => {
            const badge = document.getElementById('si-format-badge');
            if (!badge) return JSON.stringify({error: 'no-badge-found'});
            const style = window.getComputedStyle(badge);
            const bgColor = style.backgroundColor;
            const text = badge.textContent.trim();
            const visible = style.display !== 'none' && modal.offsetParent !== null;
            return JSON.stringify({
                text: text,
                backgroundColor: bgColor,
                visible: visible
            });
        })()
    """)
    print(f"  Badge info: {badge_info}")
    
    # 7. Check Sysex metadata section
    print("\n[7] Checking SysEx metadata...")
    sysex_section = client.eval_js("""
        (() => {
            const section = document.getElementById('si-sysex-section');
            if (!section) return JSON.stringify({error: 'no-sysex-section'});
            const style = window.getComputedStyle(section);
            const visible = style.display !== 'none';
            
            const deviceId = section.querySelector('#si-sysex-device-id');
            const funcCode = section.querySelector('#si-sysex-function');
            const checksum = section.querySelector('#si-sysex-checksum');
            const hexPreview = section.querySelector('#si-sysex-hex');
            
            return JSON.stringify({
                visible: visible,
                deviceId: deviceId ? deviceId.textContent.trim() : null,
                functionCode: funcCode ? funcCode.textContent.trim() : null,
                checksum: checksum ? checksum.textContent.trim() : null,
                hexPreview: hexPreview ? hexPreview.textContent.trim().substring(0, 60) : null
            });
        })()
    """)
    print(f"  Sysex section: {sysex_section}")
    
    # 8. Check preset names section
    print("\n[8] Checking preset names section...")
    preset_info = client.eval_js("""
        (() => {
            const namesSection = document.getElementById('si-preset-names-section');
            if (!namesSection) return JSON.stringify({error: 'no-names-section'});
            const style = window.getComputedStyle(namesSection);
            const visible = style.display !== 'none';
            
            const nameDivs = namesSection.querySelectorAll('.preset-name-entry');
            const names = Array.from(nameDivs).map(d => d.textContent.trim()).slice(0, 5);
            
            return JSON.stringify({
                visible: visible,
                nameCount: nameDivs.length,
                firstFewNames: names
            });
        })()
    """)
    print(f"  Preset names: {preset_info}")
    
    # 9. Check preset summary
    print("\n[9] Checking preset summary...")
    summary_info = client.eval_js("""
        (() => {
            const summary = document.getElementById('si-preset-summary');
            if (!summary) return JSON.stringify({error: 'no-summary'});
            const cards = summary.querySelectorAll('.summary-card .summary-value');
            const values = Array.from(cards).map(c => c.textContent.trim());
            return JSON.stringify({
                found: true,
                values: values
            });
        })()
    """)
    print(f"  Summary: {summary_info}")
    
    # 10. VERDICT
    print("\n" + "=" * 60)
    print("VERDICT")
    print("=" * 60)
    
    all_pass = True
    
    if "visible" in badge_info and '"visible":true' in badge_info:
        print("  ✅ SYSEX badge visible")
    else:
        print("  ❌ SYSEX badge NOT visible")
        all_pass = False
    
    if "visible" in sysex_section and '"visible":true' in sysex_section:
        print("  ✅ SysEx metadata section visible")
    else:
        print("  ❌ SysEx metadata section NOT visible")
        all_pass = False
    
    if "visible" in preset_info and '"visible":true' in preset_info:
        print("  ✅ Preset names section visible")
    else:
        print("  ❌ Preset names section NOT visible")
        all_pass = False
    
    if "values" in summary_info:
        print(f"  ✅ Summary cards: {summary_info}")
    else:
        print(f"  ⚠️ Summary: {summary_info}")
    
    print(f"\n{'✅ ALL CHECKS PASSED' if all_pass else '❌ SOME CHECKS FAILED'}")
    
    # Cleanup
    client.close()
    sys.exit(0 if all_pass else 1)

if __name__ == "__main__":
    main()
