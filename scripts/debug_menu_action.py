"""
Debug: Check if juce.menuAction('handleImportSysex') actually triggers the file dialog.
"""
import sys, os, time, ctypes

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from cdp_helpers import CDPClient, get_cdp_ws_url

def get_window_text(hwnd):
    buf = ctypes.create_unicode_buffer(256)
    ctypes.windll.user32.GetWindowTextW(hwnd, buf, 256)
    return buf.value

def get_class_name(hwnd):
    cls = ctypes.create_unicode_buffer(64)
    ctypes.windll.user32.GetClassNameW(hwnd, cls, 64)
    return cls.value

# Connect
print("1. Connecting to CDP...")
ws_url = get_cdp_ws_url(port=9222, max_retries=10, retry_delay=2)
if not ws_url:
    print("FAIL: Cannot connect to CDP")
    sys.exit(1)

client = CDPClient(ws_url)
client.send("Page.enable")
time.sleep(1)
client._drain()

# Check UI state
print("\n2. UI state:")
synth = client.eval_js("document.getElementById('synth-app') ? 'found' : 'not-found'")
print(f"  synth-app: {synth}")

# Call menuAction directly - this is the key test
print("\n3. Calling juce.menuAction('handleImportSysex')...")
try:
    result = client.eval_js("juce.menuAction('handleImportSysex')")
    print(f"  Result: {result}")
except Exception as e:
    print(f"  Error: {e}")

# Wait and check for dialogs
print("\n4. Waiting 5 seconds for dialog...")
time.sleep(5)

user32 = ctypes.windll.user32
WNDENUMPROC = ctypes.WINFUNCTYPE(ctypes.c_bool, ctypes.c_void_p, ctypes.c_void_p)

hwnds = []
def enum_cb(hwnd, _):
    hwnds.append(hwnd)
    return True
user32.EnumWindows(WNDENUMPROC(enum_cb), 0)

# Find any dialog windows (#32770 class)
dialogs = []
for hwnd in hwnds:
    txt = get_window_text(hwnd).strip()
    cls = get_class_name(hwnd)
    if cls == "#32770" and txt:
        vis = user32.IsWindowVisible(hwnd)
        dialogs.append((hwnd, txt, vis))

if dialogs:
    print(f"  Found {len(dialogs)} dialogs:")
    for hwnd, txt, vis in dialogs:
        print(f"    HWND={hwnd:010x} vis={vis} title={txt[:80]}")
else:
    print("  NO DIALOG FOUND. The menuAction did NOT trigger the file dialog.")
    print("  Checking available windows...")
    for hwnd in hwnds:
        txt = get_window_text(hwnd).strip()
        if txt and user32.IsWindowVisible(hwnd):
            cls = get_class_name(hwnd)
            print(f"    VISIBLE: HWND={hwnd:010x} cls={cls[:20]} title={txt[:60]}")

client.close()
