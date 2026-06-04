#!/usr/bin/env python3
"""
CDP (Chrome DevTools Protocol) Helpers for WebView2 Automation.

Provides a reusable CDP client class and utility functions for
interacting with WebView2 via the Chrome DevTools Protocol.

Usage:
    from cdp_helpers import CDPClient, get_cdp_ws_url

    url = get_cdp_ws_url()
    cdp = CDPClient(url)
    title = cdp.eval_js("document.title")
    cdp.click_element("#myButton")
    state = cdp.get_element_state("modal-smartImport")
    cdp.close()
"""

import json
import time
import urllib.request

try:
    import websocket
except ImportError:
    import subprocess
    import sys
    subprocess.check_call([sys.executable, "-m", "pip", "install", "websocket-client"])
    import websocket


class CDPClient:
    """Synchronous CDP client for WebView2 automation.

    Handles message id sequencing, response matching, and draining
    of pending events so eval_js() always returns the correct result.

    Usage:
        cdp = CDPClient(url)
        cdp.eval_js("document.title")        # returns string
        cdp.click_element("#btn-load-tape")   # clicks element
        state = cdp.get_element_state("si-snr")  # JSON state dict
        cdp.close()
    """

    def __init__(self, url, timeout=30):
        self.ws = websocket.create_connection(url, timeout=timeout)
        self.msg_id = 1
        self._drain()

    # ── internal ───────────────────────────────────────────────────────

    def _drain(self):
        """Drain any pending messages from the WebSocket buffer."""
        self.ws.settimeout(0.1)
        while True:
            try:
                self.ws.recv()
            except Exception:
                break
        self.ws.settimeout(10)

    def send(self, method, params=None):
        """Send a CDP command and return its message id."""
        if params is None:
            params = {}
        mid = self.msg_id
        self.ws.send(json.dumps({"id": mid, "method": method, "params": params}))
        self.msg_id += 1
        return mid

    def recv(self, expected_id, timeout=15):
        """Keep receiving until we get the response matching expected_id."""
        self.ws.settimeout(timeout)
        deadline = time.time() + timeout
        while time.time() < deadline:
            try:
                resp = json.loads(self.ws.recv())
                if resp.get("id") == expected_id:
                    return resp
            except Exception:
                pass
        return None

    def send_and_wait(self, method, params=None, timeout=15):
        """Send a CDP command and wait for its response."""
        mid = self.send(method, params)
        return self.recv(mid, timeout)

    # ── high-level helpers ─────────────────────────────────────────────

    def eval_js(self, js_expression, timeout=15):
        """Evaluate JavaScript and return the result value.

        Args:
            js_expression: JavaScript expression to evaluate.
            timeout: Max seconds to wait for result.

        Returns:
            The JS return value (string, number, list, dict, or None).
        """
        resp = self.send_and_wait("Runtime.evaluate", {
            "expression": js_expression,
            "returnByValue": True,
            "awaitPromise": True
        }, timeout=timeout)
        if resp and "result" in resp and "result" in resp["result"]:
            return resp["result"]["result"].get("value")
        return None

    def click_element(self, css_selector, timeout=10):
        """Click a DOM element by CSS selector.

        Args:
            css_selector: CSS selector string (e.g. "#myButton", "button.primary").
            timeout: Max seconds for JS evaluation.

        Returns:
            "CLICKED" on success, "NOT_FOUND" if element doesn't exist.
        """
        return self.eval_js(f"""
        (function() {{
            var el = document.querySelector({json.dumps(css_selector)});
            if (el) {{ el.click(); return 'CLICKED'; }}
            return 'NOT_FOUND';
        }})()
        """, timeout=timeout)

    def click_by_id(self, element_id, timeout=10):
        """Click an element by its HTML id attribute."""
        return self.click_element(f"#{element_id}", timeout)

    def click_by_text(self, text_contains, timeout=10):
        """Click an element whose inner text contains the given string."""
        return self.eval_js(f"""
        (function() {{
            var els = document.querySelectorAll('*');
            for (var i = 0; i < els.length; i++) {{
                if (els[i].innerText && els[i].innerText.includes({json.dumps(text_contains)})) {{
                    els[i].click();
                    return 'CLICKED';
                }}
            }}
            return 'NOT_FOUND';
        }})()
        """, timeout=timeout)

    def get_element_state(self, element_id, timeout=10):
        """Get display/disabled/text state of an element as a dict.

        Returns:
            dict with keys: display, disabled, text, innerHTML (first 300 chars).
        """
        raw = self.eval_js(f"""JSON.stringify({{
            display: (document.getElementById({json.dumps(element_id)})||{{}}).style.display||'NONE',
            disabled: (document.getElementById({json.dumps(element_id)})||{{}}).disabled,
            text: ((document.getElementById({json.dumps(element_id)})||{{}}).innerText||'').substring(0, 200),
            innerHTML: ((document.getElementById({json.dumps(element_id)})||{{}}).innerHTML||'').substring(0, 300)
        }})""", timeout=timeout)
        if raw:
            try:
                return json.loads(raw)
            except json.JSONDecodeError:
                return {"raw": raw}
        return {}

    def query_selector_all(self, css_selector, timeout=10):
        """Run document.querySelectorAll and return an array of tag+id+class strings."""
        raw = self.eval_js(f"""
        (function() {{
            var r = [];
            document.querySelectorAll({json.dumps(css_selector)}).forEach(function(e) {{
                r.push(e.tagName + (e.id ? '#' + e.id : '') + (e.className ? '.' + e.className.split(' ').join('.') : ''));
            }});
            return JSON.stringify(r);
        }})()
        """, timeout=timeout)
        if raw:
            try:
                return json.loads(raw)
            except json.JSONDecodeError:
                return []
        return []

    def navigate(self, url, timeout=15):
        """Navigate the page to a URL."""
        self.send_and_wait("Page.navigate", {"url": url}, timeout=timeout)

    def enable_console(self):
        """Enable console log capture."""
        self.send_and_wait("Console.enable")

    def wait_for_notification(self, expected_text=None, timeout=30, poll_interval=0.5):
        """Wait until a notification toast appears and optionally verify its text.

        Polls the #notification-toast element checking visibility and content.
        The toast typically fades in briefly after an import or save operation.
        Uses getComputedStyle for reliable visibility detection (handles CSS
        class-based hiding via opacity, visibility, display).

        Args:
            expected_text: Optional substring to verify in toast message.
                          If None, just waits for any toast to appear.
            timeout: Max seconds to wait.
            poll_interval: Seconds between polls.

        Returns:
            The toast message text if found and matched, or None on timeout.
        """
        print(f"  [WAIT] Waiting for notification toast (timeout={timeout}s)...")
        deadline = time.time() + timeout

        while time.time() < deadline:
            # Use getComputedStyle for reliable visibility (handles CSS class hiding)
            raw = self.eval_js(f"""JSON.stringify({{{{
                display: window.getComputedStyle(document.getElementById('notification-toast')||document.createElement('div')).display,
                opacity: parseFloat(window.getComputedStyle(document.getElementById('notification-toast')||document.createElement('div')).opacity),
                text: (document.getElementById('notification-toast')||{{}}).innerText || ''
            }}}})""", timeout=5)
            if not raw:
                time.sleep(poll_interval)
                continue

            try:
                state = json.loads(raw)
            except json.JSONDecodeError:
                state = {}

            display = state.get("display", "")
            opacity = state.get("opacity", 0.0)
            text = state.get("text", "").strip()

            # Toast is visible when NOT display:none AND opacity > 0
            if display != "none" and opacity > 0.0 and text:
                print(f"  [WAIT] Toast visible: '{text[:80]}'")
                if expected_text:
                    if expected_text.lower() in text.lower():
                        print(f"  [WAIT] Text matches expected: '{expected_text}'")
                        return text
                    else:
                        print(f"  [WAIT] Text does NOT match expected '{expected_text}' "
                              f"(got: '{text[:80]}')")
                        # Keep waiting — the toast might update
                else:
                    return text

            time.sleep(poll_interval)

        print(f"  [WAIT] Notification timeout ({timeout}s)")
        return None

    def close(self):
        """Close the WebSocket connection."""
        try:
            self.ws.close()
        except Exception:
            pass


# ── Module-level helpers ─────────────────────────────────────────────

def get_cdp_ws_url(port=9222, max_retries=20, retry_delay=1.5,
                   title_filter=None, url_filter=None):
    """Fetch the WebSocket debugger URL from the CDP endpoint.

    Polls http://localhost:{port}/json until a WebView2 target is found.

    Args:
        port: CDP debug port (default 9222).
        max_retries: Number of connection attempts.
        retry_delay: Seconds between retries.
        title_filter: Optional string to match page title (e.g. "ABD").
        url_filter: Optional string to match page URL (e.g. "juce.backend").

    Returns:
        WebSocket URL string (e.g. "ws://localhost:9222/..."), or None.
    """
    for i in range(max_retries):
        try:
            resp = urllib.request.urlopen(f"http://localhost:{port}/json", timeout=3)
            data = json.loads(resp.read().decode())

            # Prefer pages matching filters
            for page in data:
                ws = page.get("webSocketDebuggerUrl", "")
                if not ws:
                    continue
                if url_filter and url_filter in page.get("url", ""):
                    return ws
                if title_filter and title_filter in page.get("title", ""):
                    return ws

            # Fallback: first non-blank page
            for page in data:
                ws = page.get("webSocketDebuggerUrl", "")
                if ws and page.get("url") != "about:blank":
                    return ws

            # Last resort: first page
            if data:
                ws = data[0].get("webSocketDebuggerUrl")
                if ws:
                    return ws

        except Exception as e:
            print(f"  [CDP] get_ws_url attempt {i+1}: {e}")
        time.sleep(retry_delay)

    return None


def wait_for_modal(cdp, modal_id="modal-smartImport", timeout=60, poll_interval=1,
                   enable_on="btn-si-import"):
    """Poll CDP until the import modal appears and the confirm button enables.

    Args:
        cdp: CDPClient instance.
        modal_id: HTML id of the modal overlay.
        timeout: Max seconds to wait.
        poll_interval: Seconds between polls.
        enable_on: Element id to check for enabled state.

    Returns:
        (modal_found, button_enabled) tuple of booleans.
    """
    modal_found = False
    btn_enabled = False

    for i in range(int(timeout / poll_interval)):
        time.sleep(poll_interval)

        state = cdp.get_element_state(modal_id)
        if not state:
            continue

        modal_display = state.get("display", "NONE")
        btn_disabled = state.get("disabled")

        if modal_display != "NONE" and not modal_found:
            modal_found = True
            log_text = cdp.eval_js(
                "(document.getElementById('si-progress-log')||{}).innerText||''"
            ) or ""
            status_text = cdp.eval_js(
                "(document.getElementById('si-progress-status')||{}).innerText||''"
            ) or ""
            print(f"  [WAIT] t={(i+1)*poll_interval}s | MODAL VISIBLE | "
                  f"status={status_text[:40]}")
            if log_text:
                print(f"         log: {log_text[:150]}")

        if btn_disabled is not None and btn_disabled is False:
            btn_enabled = True
            print(f"  [WAIT] t={(i+1)*poll_interval}s | *** {enable_on} ENABLED! ***")
            break

        if i % 3 == 0 and i > 0:
            print(f"  [WAIT] t={(i+1)*poll_interval}s | still waiting...")

    return modal_found, btn_enabled
