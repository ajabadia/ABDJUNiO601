#!/usr/bin/env python3
"""
Win32 API Helpers for Native File Dialog Automation.

Provides functions to find, interact with, and dismiss native
Windows file dialogs via the Win32 API (ctypes).

Supports both classic (Edit control) and modern (DirectUI) dialogs.

Usage:
    from win32_dialog import find_file_dialog, set_file_and_open

    dlg_hwnd, edit_hwnd = find_file_dialog(title_filter="Load Tape")
    if dlg_hwnd:
        set_file_and_open(dlg_hwnd, edit_hwnd, "C:\\path\\to\\file.wav")
"""

import ctypes
import ctypes.wintypes
import time

# ── Win32 Constants ──────────────────────────────────────────────────

WM_SETTEXT = 0x000C
WM_GETTEXT = 0x000D
BM_CLICK = 0x00F5
WM_KEYDOWN = 0x0100
WM_KEYUP = 0x0101
VK_RETURN = 0x0D
VK_CONTROL = 0x11
VK_V = 0x56

# ── Win32 function wrappers ─────────────────────────────────────────

user32 = ctypes.windll.user32

SendMessageW = user32.SendMessageW
SendMessageW.argtypes = [ctypes.c_void_p, ctypes.c_uint,
                         ctypes.c_void_p, ctypes.c_void_p]
SendMessageW.restype = ctypes.c_void_p

# Callback type for EnumWindows / EnumChildWindows
WNDENUMPROC = ctypes.WINFUNCTYPE(ctypes.c_bool, ctypes.c_void_p,
                                  ctypes.c_void_p)


# ── Window enumeration helpers ──────────────────────────────────────

def _enum_windows(callback):
    """Wrap EnumWindows with a ctypes callback."""
    user32.EnumWindows(WNDENUMPROC(callback), 0)


def _enum_child_windows(parent_hwnd, callback):
    """Wrap EnumChildWindows with a ctypes callback."""
    user32.EnumChildWindows(parent_hwnd, WNDENUMPROC(callback), 0)


def get_window_text(hwnd, max_len=256):
    """Get the title text of a window."""
    buf = ctypes.create_unicode_buffer(max_len)
    user32.GetWindowTextW(hwnd, buf, max_len)
    return buf.value


def get_class_name(hwnd, max_len=64):
    """Get the class name of a window."""
    buf = ctypes.create_unicode_buffer(max_len)
    user32.GetClassNameW(hwnd, buf, max_len)
    return buf.value


# ── Find window ─────────────────────────────────────────────────────

def find_window_by_title(title_substring, class_name=None, timeout=30):
    """Find a top-level window whose title contains the given substring.

    Args:
        title_substring: Part of the window title to search for
                         (case-insensitive).
        class_name: Optional window class name to filter by (partial match).
        timeout: Maximum seconds to keep searching.

    Returns:
        HWND of the found window, or None.
    """
    deadline = time.time() + timeout
    title_lower = title_substring.lower()

    while time.time() < deadline:
        result_hwnd = [None]

        def enum_cb(hwnd, _):
            text = get_window_text(hwnd)
            if title_lower in text.lower():
                if class_name:
                    cls = get_class_name(hwnd)
                    if class_name.lower() not in cls.lower():
                        return True
                result_hwnd[0] = hwnd
                return False
            return True

        _enum_windows(enum_cb)
        if result_hwnd[0]:
            return result_hwnd[0]
        time.sleep(0.3)

    return None


# ── File dialog ─────────────────────────────────────────────────────

def find_file_dialog(title_filter=None, timeout=30):
    """Find a native file dialog and return its HWND and Edit control.

    Polls top-level windows looking for a dialog window. If title_filter
    is given, only windows whose title contains that string are considered.

    Supports both classic dialogs (with direct Edit child) and modern
    dialogs (DirectUI, no Edit child).

    Args:
        title_filter: Optional substring to match in window title
                      (e.g. "Load Tape", "Import").
        timeout: Maximum seconds to keep searching.

    Returns:
        (dialog_hwnd, edit_hwnd) tuple, or (None, None) if not found.
        edit_hwnd may be None for modern dialogs.
    """
    print(f"  [WIN] Looking for file dialog (timeout={timeout}s)...")
    deadline = time.time() + timeout

    while time.time() < deadline:
        result_hwnd = [None]
        result_edit = [None]

        def enum_cb(hwnd, _):
            text = get_window_text(hwnd)
            if not text.strip():
                return True

            # Apply title filter
            if title_filter:
                if title_filter.lower() not in text.lower():
                    return True

            # Must be a dialog class (#32770) or similar
            cls = get_class_name(hwnd)
            if cls not in ("#32770", "Dialog", "Chrome_WidgetWin_1", "Windows.UI.Core.CoreWindow"):
                return True

            # Look for an Edit control (file path input) - classic dialog
            edit = user32.FindWindowExW(hwnd, None, "Edit", None)
            if edit:
                result_hwnd[0] = hwnd
                result_edit[0] = edit
                return False

            # Fallback: ComboBox → Edit (modern-ish dialog)
            combo = user32.FindWindowExW(hwnd, None, "ComboBox", None)
            if combo:
                edit2 = user32.FindWindowExW(combo, None, "Edit", None)
                if edit2:
                    result_hwnd[0] = hwnd
                    result_edit[0] = edit2
                    return False

            # Fallback: Accept dialog without Edit control (modern dialog)
            # The edit control may be embedded in DirectUIHWND
            result_hwnd[0] = hwnd
            result_edit[0] = None  # No edit control found, use keyboard fallback
            return False

        _enum_windows(enum_cb)

        if result_hwnd[0]:
            edit_str = f"Edit=({result_edit[0]:#010x})" if result_edit[0] else "Edit=NONE [modern dialog]"
            print(f"  [WIN] Found dialog: HWND={result_hwnd[0]:#010x}, "
                  f"{edit_str}, "
                  f"title='{get_window_text(result_hwnd[0])[:60]}'")
            return result_hwnd[0], result_edit[0]

        time.sleep(0.5)

    print(f"  [WIN] No dialog found within {timeout}s!")
    return None, None


# ── Interact with dialog ────────────────────────────────────────────

def set_edit_text(edit_hwnd, text):
    """Set the text of an Edit control via WM_SETTEXT."""
    buf = ctypes.create_unicode_buffer(text)
    ptr = ctypes.cast(ctypes.pointer(buf), ctypes.c_void_p)
    SendMessageW(edit_hwnd, WM_SETTEXT, 0, ptr)


def get_edit_text(edit_hwnd, max_len=1024):
    """Read text from an Edit control via WM_GETTEXT."""
    buf = ctypes.create_unicode_buffer(max_len)
    ptr = ctypes.cast(ctypes.pointer(buf), ctypes.c_void_p)
    SendMessageW(edit_hwnd, WM_GETTEXT, max_len, ptr)
    return buf.value


def click_button_by_text(parent_hwnd, button_text):
    """Find a button by its text and simulate a click via BM_CLICK.

    Args:
        parent_hwnd: Parent window HWND.
        button_text: Button text to search for (case-insensitive).

    Returns:
        True if the button was found and clicked.
    """
    result_btn = [None]

    def enum_cb(hwnd, _):
        cls = get_class_name(hwnd)
        if "Button" in cls:
            text = get_window_text(hwnd)
            if button_text.lower() in text.lower():
                result_btn[0] = hwnd
                return False
        return True

    _enum_child_windows(parent_hwnd, enum_cb)

    if result_btn[0]:
        print(f"  [WIN] Clicking '{button_text}' (HWND={result_btn[0]:#010x})...")
        SendMessageW(result_btn[0], BM_CLICK, 0, 0)
        return True
    return False


def press_enter(dialog_hwnd):
    """Send an Enter keypress to a dialog window."""
    user32.PostMessageW(dialog_hwnd, WM_KEYDOWN, VK_RETURN, 0)
    time.sleep(0.05)
    user32.PostMessageW(dialog_hwnd, WM_KEYUP, VK_RETURN, 0)


def type_text_character_by_character(hwnd, text, delay=0.02):
    """Type text into a window by sending individual WM_CHAR messages.
    
    This works for modern file dialogs where the edit control is
    not accessible as a traditional child window.
    """
    print(f"  [WIN] Typing text character-by-character ({len(text)} chars)...")
    WM_CHAR = 0x0102
    for ch in text:
        user32.PostMessageW(hwnd, WM_CHAR, ord(ch), 0)
        time.sleep(delay)
    time.sleep(0.3)


def type_text_via_clipboard(hwnd, text):
    """Type text by placing it in clipboard and sending Ctrl+V.
    
    This is more reliable than character-by-character for modern dialogs.
    """
    import subprocess
    # Use PowerShell to set clipboard
    escaped = text.replace("'", "''")
    cmd = f'powershell -Command "Set-Clipboard -Value \\"{escaped}\\""'
    subprocess.run(cmd, shell=True, capture_output=True)
    time.sleep(0.2)
    
    # Send Ctrl+V
    user32.PostMessageW(hwnd, WM_KEYDOWN, VK_CONTROL, 0)
    time.sleep(0.05)
    user32.PostMessageW(hwnd, WM_KEYDOWN, ord('V'), 0)
    time.sleep(0.05)
    user32.PostMessageW(hwnd, WM_KEYUP, ord('V'), 0)
    time.sleep(0.05)
    user32.PostMessageW(hwnd, WM_KEYUP, VK_CONTROL, 0)
    time.sleep(0.3)
    print(f"  [WIN] Ctrl+V sent (clipboard: '{text[:80]}')")


def set_file_and_open(dialog_hwnd, edit_hwnd, file_path):
    """Set the file path in a dialog and click Open (or press Enter).

    This is the main high-level function for interacting with a
    native file dialog once it has been found via find_file_dialog().

    For classic dialogs (edit_hwnd is not None), uses WM_SETTEXT.
    For modern dialogs (edit_hwnd is None), uses keyboard simulation.

    Args:
        dialog_hwnd: Dialog window HWND.
        edit_hwnd: Edit control HWND (may be None for modern dialogs).
        file_path: Full path to the file to select.

    Returns:
        True on success.
    """
    print(f"  [WIN] Setting file path...")
    user32.SetForegroundWindow(dialog_hwnd)
    time.sleep(0.5)

    if edit_hwnd:
        # Classic dialog: use WM_SETTEXT
        set_edit_text(edit_hwnd, file_path)
        time.sleep(0.3)

        current = get_edit_text(edit_hwnd)
        print(f"  [WIN] Edit shows: '{current[:80]}'")
    else:
        # Modern dialog: use clipboard + Ctrl+V
        type_text_via_clipboard(dialog_hwnd, file_path)
        time.sleep(0.5)

    # Try the Open button first, fall back to Enter key
    if click_button_by_text(dialog_hwnd, "Open"):
        print(f"  [WIN] 'Open' button clicked!")
        time.sleep(1.0)
        return True

    if click_button_by_text(dialog_hwnd, "Abrir"):  
        print(f"  [WIN] 'Abrir' button clicked!")
        time.sleep(1.0)
        return True

    if click_button_by_text(dialog_hwnd, "Öffnen"):
        print(f"  [WIN] 'Öffnen' button clicked!")
        time.sleep(1.0)
        return True

    print(f"  [WIN] No Open button found, pressing Enter...")
    press_enter(dialog_hwnd)
    time.sleep(1.0)
    return True
