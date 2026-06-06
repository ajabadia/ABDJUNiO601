#!/usr/bin/env python3
"""
SynthMania Preset Downloader for ABDJUNiO601

Downloads Juno-60 / Juno-106 factory preset MP3 recordings from
https://www.synthmania.com for qualitative A/B comparison.

Usage:
    python scripts/download_synthmania.py juno-60  [--output ./ref_audio/juno60]
    python scripts/download_synthmania.py juno-106 [--output ./ref_audio/juno106]
    python scripts/download_synthmania.py juno-60  --list-only   # Just list presets
    python scripts/download_synthmania.py juno-106 --preset 12   # Download single preset
"""

import argparse
import os
import re
import sys
import time
from urllib.parse import urljoin, unquote

import requests
from bs4 import BeautifulSoup


# ─── Config ──────────────────────────────────────────────────────────────────

SYNTHMANIA_URLS = {
    'juno-60': 'https://www.synthmania.com/juno-60.htm',
    'juno-106': 'https://www.synthmania.com/juno-106.htm',
}

HEADERS = {
    'User-Agent': (
        'Mozilla/5.0 (Windows NT 10.0; Win64; x64) '
        'AppleWebKit/537.36 (KHTML, like Gecko) '
        'Chrome/120.0.0.0 Safari/537.36'
    ),
    'Accept': 'text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8',
    'Accept-Language': 'en-US,en;q=0.5',
}

# Bank sizes: Juno-60 and Juno-106 both have 56 presets per bank
BANK_SIZE = 56


# ─── Page Fetching ───────────────────────────────────────────────────────────

def fetch_page(url):
    """Download HTML page content with proper headers."""
    print(f"  Fetching {url} ...")
    session = requests.Session()
    session.headers.update(HEADERS)
    resp = session.get(url, timeout=30)
    resp.raise_for_status()
    print(f"  Got {len(resp.text)} bytes, HTTP {resp.status_code}")
    return resp.text


# ─── Parsing ─────────────────────────────────────────────────────────────────

def parse_presets(html):
    """Parse preset entries from synthmania HTML page.

    The MP3 links on synthmania are in <a> tags like:
      <a href=".../11%20Brass.mp3">11 Brass</a>

    The preset name is the text inside the <a> tag (e.g. "11 Brass").
    Bank is detected from URL path ("Group A" vs "Group B" or count-based).

    Returns list of dicts: {name, description, mp3_url, bank, patch_num}
    """
    soup = BeautifulSoup(html, 'html.parser')
    presets = []
    seen_names = set()  # deduplicate

    # Find all <a> tags with .mp3 href
    for link in soup.find_all('a', href=re.compile(r'\.mp3$', re.I)):
        mp3_url = link.get('href', '').strip()
        if not mp3_url:
            continue

        # Get preset name from the <a> tag's text
        raw_name = link.get_text(strip=True)
        if not raw_name:
            # Fallback: extract from filename
            filename = unquote(os.path.basename(mp3_url))
            raw_name = re.sub(r'\.mp3$', '', filename, flags=re.I)
            # Remove leading number like "11 " or "11-"
            raw_name = re.sub(r'^\d+[\s\-_\.]+', '', raw_name).strip()

        # Skip duplicates (same name appearing multiple times)
        if raw_name.lower() in seen_names:
            continue
        seen_names.add(raw_name.lower())

        # Clean name: remove leading number prefix if present
        name = re.sub(r'^\d+[\s\-_\.]+', '', raw_name).strip()
        if not name:
            name = raw_name

        # Detect bank from URL path
        bank = _detect_bank_from_url(mp3_url, len(presets))

        # Description: look for text after the <a> tag within the parent
        description = ''
        parent = link.parent
        if parent:
            # Get all text nodes after the link
            for sibling in link.next_siblings:
                if sibling.name == 'br':
                    break
                txt = sibling.get_text(strip=True) if hasattr(sibling, 'get_text') else str(sibling).strip()
                if txt:
                    description += ' ' + txt
            description = description.strip()
            # Remove file size patterns like [123 KB]
            description = re.sub(r'\s*\[\s*\d+\s*KB?\s*\]', '', description, flags=re.I)

        patch_num = len(presets) + 1

        presets.append({
            'name': name,
            'description': description[:200],
            'mp3_url': mp3_url,
            'bank': bank,
            'patch_num': patch_num,
            '_raw_name': raw_name,
        })

    return presets


def _detect_bank_from_url(mp3_url, current_count):
    """Detect which bank a preset belongs to from URL path or count."""
    url_upper = mp3_url.upper()

    # Check URL for group/bank indicators
    if 'GROUP B' in url_upper or 'GROUP 2' in url_upper:
        return 'B'
    if 'GROUP A' in url_upper or 'GROUP 1' in url_upper:
        return 'A'
    if 'BANK B' in url_upper:
        return 'B'
    if 'BANK A' in url_upper:
        return 'A'

    # Fallback: first 56 presets = Bank A, rest = Bank B
    return 'A' if current_count < BANK_SIZE else 'B'


# ─── Download ────────────────────────────────────────────────────────────────

def download_mp3(url, output_path):
    """Download an MP3 file. Handles relative URLs."""
    full_url = urljoin('https://www.synthmania.com', url)

    print(f"  -> Downloading ...", end=' ', flush=True)
    session = requests.Session()
    session.headers.update(HEADERS)
    resp = session.get(full_url, timeout=120)
    resp.raise_for_status()

    with open(output_path, 'wb') as f:
        f.write(resp.content)

    size_kb = len(resp.content) / 1024
    print(f"OK ({size_kb:.0f} KB)")
    return output_path


# ─── Main ────────────────────────────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser(
        description="Download Juno-60/106 factory preset MP3s from SynthMania",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s juno-60
  %(prog)s juno-106 --output ./ref_audio/juno106
  %(prog)s juno-60 --list-only
  %(prog)s juno-60 --preset 12
  %(prog)s juno-106 --preset "Brass 1"
        """
    )
    parser.add_argument('synth', choices=['juno-60', 'juno-106'],
                        help='Synth type to download presets for')
    parser.add_argument('--output', '-o', default=None,
                        help='Output directory (default: ref_audio/<synth>)')
    parser.add_argument('--list-only', '-l', action='store_true',
                        help='Only list available presets, do not download')
    parser.add_argument('--preset', '-p', type=str, default=None,
                        help='Download a single preset by number or name substring')
    parser.add_argument('--delay', type=float, default=1.5,
                        help='Delay between downloads in seconds (default: 1.5)')
    parser.add_argument('--max', type=int, default=0,
                        help='Maximum presets to download (0 = all)')

    args = parser.parse_args()

    # Determine output directory
    if args.output is None:
        args.output = os.path.join('ref_audio', args.synth.replace('-', '_'))

    print("=" * 60)
    print(f"  SynthMania Downloader - {args.synth.upper()}")
    print("=" * 60)

    # Fetch and parse
    base_url = SYNTHMANIA_URLS[args.synth]
    html = fetch_page(base_url)
    presets = parse_presets(html)

    print(f"\n  Found {len(presets)} preset recordings:")
    print(f"  {'-' * 50}")

    if args.list_only:
        for i, p in enumerate(presets, 1):
            desc = p['description'][:60].replace('\n', ' ').strip()
            print(f"  {i:3d}. [{p['bank']}] {p['name']:<35s} {desc}")
        return

    # Filter single preset
    if args.preset:
        if args.preset.isdigit():
            idx = int(args.preset) - 1
            if 0 <= idx < len(presets):
                presets = [presets[idx]]
            else:
                print(f"  Error: preset #{args.preset} not found (1-{len(presets)})")
                sys.exit(1)
        else:
            matches = [p for p in presets if args.preset.lower() in p['name'].lower()]
            if not matches:
                print(f"  Error: no presets matching '{args.preset}'")
                sys.exit(1)
            presets = matches

    # Apply max limit
    if args.max > 0:
        presets = presets[:args.max]

    # Create output directory
    os.makedirs(args.output, exist_ok=True)
    print(f"\n  Output directory: {os.path.abspath(args.output)}")

    # Download each preset
    downloaded = 0
    for i, p in enumerate(presets, 1):
        # Sanitize filename: keep alphanumeric, spaces, hyphens, then collapse spaces to underscores
        safe_name = re.sub(r'[^a-zA-Z0-9 _-]', '', p['name']).strip()
        safe_name = re.sub(r'[ _-]+', '_', safe_name)
        filename = f"{p['bank']}{p['patch_num']:02d}_{safe_name}.mp3"
        filepath = os.path.join(args.output, filename)

        print(f"\n  [{i}/{len(presets)}] {p['name']}")
        print(f"         -> {filename}")

        if os.path.exists(filepath):
            size_kb = os.path.getsize(filepath) / 1024
            print(f"         Already exists ({size_kb:.0f} KB), skipping.")
            downloaded += 1
        else:
            try:
                download_mp3(p['mp3_url'], filepath)
                downloaded += 1
            except Exception as e:
                print(f"FAILED: {e}")

        if i < len(presets):
            time.sleep(args.delay)

    print(f"\n{'-' * 50}")
    print(f"  Downloaded {downloaded}/{len(presets)} files to:")
    print(f"  {os.path.abspath(args.output)}")
    print(f"{'-' * 50}")
    print()
    print(f"  Next: convert MP3s to WAV for comparison.")
    print(f"  Since ffmpeg is not installed, you can use an online converter")
    print(f"  or install ffmpeg from https://ffmpeg.org/download.html")
    print()
    print(f"  Then compare with:")
    print(f"    python scripts/compare_audio.py \\")
    print(f"      \"{args.output}/A01_{safe_name}.wav\" \\")
    print(f"      \"path/to/engine_output.wav\"")
    print()


if __name__ == '__main__':
    main()
