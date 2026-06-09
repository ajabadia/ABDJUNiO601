#!/usr/bin/env python3
"""Audit all CSS classes across WebUI: find orphaned + dead CSS classes."""

import re
import os
from collections import defaultdict

PROJECT = r"D:\desarrollos\ABDSynths\ABDJUNiO601"
WEBUI = os.path.join(PROJECT, "Source", "UI", "WebUI")

CSS_FILES = [
    "css/vars.css", "css/base.css", "css/header.css", "css/sidebar.css",
    "css/engine.css", "css/controls.css", "css/performance.css",
    "css/keyboard.css", "css/about.css", "css/settings.css",
    "style.css", "service.css", "browser.css"
]

HTML_JS_FILES = [
    "index.html", "bridge-core.js", "ui-sliders.js", "ui-modals.js",
    "smart-import.js", "ui-keyboard.js", "theme-manager.js",
    "modals.js", "service.js", "browser.js"
]

# CSS selectors to IGNORE (pseudo-classes, pseudo-elements, attribute selectors, combinators, @media, etc.)
SKIP_CSS_PATTERNS = re.compile(
    r'^:|\b(hover|active|focus|visited|before|after|first-child|last-child|nth-child|'
    r'checked|disabled|enabled|not\(|has\(|where\(|is\(|data-|aria-|@)'
)

def extract_css_classes(text):
    """Extract unique class names from CSS."""
    classes = set()
    # Match .class-name { or .class-name, 
    # Also match .class-name.other-class (compound)
    # Match .class-name::pseudo, .class-name:pseudo
    # Match .class-name > .other-class
    lines = text.split('\n')
    for line in lines:
        # Remove comments
        line = re.sub(r'/\*.*?\*/', '', line)
        # Find all class selectors: .className (but not .className( or .className:)
        matches = re.findall(r'\.([a-zA-Z_][a-zA-Z0-9_-]*)\b', line)
        for m in matches:
            if not SKIP_CSS_PATTERNS.match(m):
                classes.add(m)
    return classes


def extract_html_js_classes(text):
    """Extract class references from HTML/JS."""
    classes = set()
    
    # HTML: class="foo bar" or class='foo bar'
    for m in re.finditer(r'class\s*=\s*"([^"]*)"', text):
        for c in m.group(1).split():
            classes.add(c.strip())
    for m in re.finditer(r"class\s*=\s*'([^']*)'", text):
        for c in m.group(1).split():
            classes.add(c.strip())
    
    # JS template literals with class names in backticks
    # classList.add('foo'), classList.remove('foo'), classList.toggle('foo')
    # classList.contains('foo')
    for m in re.finditer(r"""classList\.(?:add|remove|toggle|contains)\s*\(\s*['"]([^'"]+)['"]""", text):
        classes.add(m.group(1))
    
    # className = 'foo'
    for m in re.finditer(r"""className\s*=\s*['"]([^'"]+)['"]""", text):
        for c in m.group(1).split():
            classes.add(c.strip())
    
    # .className and #id.className patterns in CSS selectors used in JS
    # e.g., querySelector('.className')
    for m in re.finditer(r"""querySelector(?:All)?\s*\(\s*['"]([^'"]+)['"]""", text):
        sel = m.group(1)
        classes_in_sel = re.findall(r'\.([a-zA-Z_][a-zA-Z0-9_-]*)', sel)
        for c in classes_in_sel:
            classes.add(c)
    
    # Template literal: `foo ${...} bar` — check for class-like patterns
    # This is hard to do perfectly, so we'll skip template literals
    
    # classList.toggle('className', condition) — covered above
    
    # Regular string concatenation: 'className'
    for m in re.finditer(r"""['"]\.(class-[a-zA-Z0-9_-]+)['"]""", text):
        classes.add(m.group(1))
    
    return classes


def main():
    # 1. Extract from CSS
    css_classes = set()
    for fname in CSS_FILES:
        fpath = os.path.join(WEBUI, fname)
        if not os.path.exists(fpath):
            print(f"[SKIP] CSS file not found: {fname}")
            continue
        with open(fpath, 'r', encoding='utf-8') as f:
            classes = extract_css_classes(f.read())
            count_before = len(css_classes)
            css_classes |= classes
            count_after = len(css_classes)
            print(f"[CSS] {fname}: +{len(classes)} classes ({count_after - count_before} new)")

    # 2. Extract from HTML/JS
    html_js_classes = set()
    for fname in HTML_JS_FILES:
        fpath = os.path.join(WEBUI, fname)
        if not os.path.exists(fpath):
            print(f"[SKIP] HTML/JS file not found: {fname}")
            continue
        with open(fpath, 'r', encoding='utf-8') as f:
            classes = extract_html_js_classes(f.read())
            count_before = len(html_js_classes)
            html_js_classes |= classes
            count_after = len(html_js_classes)
            print(f"[HTML/JS] {fname}: +{len(classes)} classes ({count_after - count_before} new)")

    # 3. Exclude common noise
    # Remove CSS framework classes, pseudo-class noise, etc.
    noise = {'hover', 'active', 'focus', 'visited', 'before', 'after',
             'first-child', 'last-child', 'nth-child', 'disabled', 'checked',
             'not', 'has', 'where', 'is', 'root', 'host', 'slotted',
             'part', 'cue', 'cue-region', 'grammar-error', 'spelling-error',
             'target', 'link', 'any-link', 'scope', 'focus-visible',
             'focus-within', 'user-invalid', 'user-valid', 'indeterminate',
             'placeholder-shown', 'read-only', 'read-write', 'required',
             'optional', 'valid', 'invalid', 'in-range', 'out-of-range',
             'default', 'only-child', 'only-of-type', 'first-of-type',
             'last-of-type', 'nth-last-child', 'nth-last-of-type',
             'empty', 'fullscreen', 'picture-in-picture',
             'modal', 'scroll', 'center', 'open', 'hidden', 'left', 'right',
             'top', 'bottom', 'middle', 'start', 'end', 'small', 'large',
             'auto', 'none', 'block', 'inline', 'flex', 'grid',
             'column', 'row', 'wrap', 'nowrap', 'static', 'relative',
             'absolute', 'fixed', 'sticky', 'border-box', 'content-box',
             'inherit', 'initial', 'unset', 'revert', 'revert-layer',
             'style', 'class', 'id', 'data', 'aria', 'role'}

    css_classes_clean = css_classes - noise
    html_js_classes_clean = html_js_classes - noise

    # 4. Cross-reference
    only_in_css = css_classes_clean - html_js_classes_clean
    only_in_html_js = html_js_classes_clean - css_classes_clean

    print(f"\n{'='*60}")
    print(f"RESUMEN")
    print(f"{'='*60}")
    print(f"Total CSS classes: {len(css_classes_clean)}")
    print(f"Total HTML/JS class refs: {len(html_js_classes_clean)}")
    print(f"Classes in CSS only (potential dead CSS): {len(only_in_css)}")
    print(f"Classes in HTML/JS only (potential orphaned): {len(only_in_html_js)}")
    print(f"Classes in both (used): {len(css_classes_clean & html_js_classes_clean)}")

    print(f"\n{'='*60}")
    print(f"POTENTIAL DEAD CSS (in CSS but never referenced in HTML/JS)")
    print(f"{'='*60}")
    if only_in_css:
        for c in sorted(only_in_css):
            print(f"  .{c}")
    else:
        print("  (none)")

    print(f"\n{'='*60}")
    print(f"POTENTIAL ORPHANED CLASSES (in HTML/JS but no CSS definition)")
    print(f"{'='*60}")
    if only_in_html_js:
        for c in sorted(only_in_html_js):
            print(f"  .{c}")
    else:
        print("  (none)")

    # 5. Show which CSS file(s) each unused class comes from
    if only_in_css:
        print(f"\n{'='*60}")
        print(f"DETAIL: Dead CSS classes by file")
        print(f"{'='*60}")
        for fname in CSS_FILES:
            fpath = os.path.join(WEBUI, fname)
            if not os.path.exists(fpath):
                continue
            with open(fpath, 'r', encoding='utf-8') as f:
                file_classes = extract_css_classes(f.read())
            dead_in_file = file_classes & only_in_css
            if dead_in_file:
                print(f"\n  {fname}:")
                for c in sorted(dead_in_file):
                    print(f"    .{c}")


if __name__ == '__main__':
    main()
