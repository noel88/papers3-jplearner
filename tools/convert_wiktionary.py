#!/usr/bin/env python3
"""
Wiktionary JSONL to ESP32 Dictionary Converter

Converts kaikki.org Wiktionary JSONL to optimized binary format for ESP32.

Usage:
    python convert_wiktionary.py kaikki.org-dictionary-Japanese.jsonl output_dir

Output files:
    - wikt_index.bin: Binary index file (sorted by word hash)
    - wikt_entries.dat: Tab-separated entry data

Index format (10 bytes per entry):
    - hash (4 bytes): FNV-1a hash of word
    - offset (4 bytes): File offset in entries.dat
    - length (2 bytes): Entry length

Entry format (variable length):
    word\treading\tmeanings\tpos\n
"""

import sys
import os
import json
import struct
from pathlib import Path
from collections import defaultdict


def fnv1a_hash(s: str) -> int:
    """Calculate FNV-1a hash of string (same as ESP32 implementation)."""
    hash_value = 2166136261
    for c in s.encode('utf-8'):
        hash_value ^= c
        hash_value = (hash_value * 16777619) & 0xFFFFFFFF
    return hash_value


def kata_to_hira(s: str) -> str:
    """Convert katakana to hiragana."""
    result = []
    for c in s:
        code = ord(c)
        if 0x30A0 <= code <= 0x30FF:  # Katakana range
            result.append(chr(code - 0x60))
        else:
            result.append(c)
    return ''.join(result)


def extract_reading(entry: dict) -> str:
    """Extract reading (hiragana) from entry."""
    # Try forms first
    forms = entry.get('forms', [])
    for form in forms:
        tags = form.get('tags', [])
        if 'hiragana' in tags:
            return form.get('form', '')
        # Check ruby for readings
        ruby = form.get('ruby', [])
        if ruby:
            readings = [r[1] for r in ruby if len(r) > 1]
            if readings:
                return kata_to_hira(''.join(readings))

    # Try sounds
    sounds = entry.get('sounds', [])
    for sound in sounds:
        other = sound.get('other', '')
        if other:
            return kata_to_hira(other)

    # Fallback to word itself if it's hiragana/katakana
    word = entry.get('word', '')
    return kata_to_hira(word)


def extract_meanings(entry: dict, max_meanings: int = 5) -> str:
    """Extract meanings from senses."""
    meanings = []
    senses = entry.get('senses', [])

    for sense in senses:
        glosses = sense.get('glosses', [])
        for gloss in glosses:
            # Skip romanization entries
            if 'Rōmaji transcription' in gloss:
                continue
            # Clean up gloss
            gloss = gloss.strip()
            if gloss and gloss not in meanings:
                meanings.append(gloss)
                if len(meanings) >= max_meanings:
                    break
        if len(meanings) >= max_meanings:
            break

    return '; '.join(meanings)


def parse_wiktionary(jsonl_path: str) -> list:
    """Parse Wiktionary JSONL and extract entries."""
    print(f"Parsing {jsonl_path}...")

    entries = defaultdict(list)  # Group by word
    skipped = 0
    total = 0

    with open(jsonl_path, 'r', encoding='utf-8') as f:
        for line in f:
            total += 1
            if total % 50000 == 0:
                print(f"  Processed {total} lines...")

            try:
                entry = json.loads(line)
            except json.JSONDecodeError:
                skipped += 1
                continue

            # Skip romanization entries
            pos = entry.get('pos', '')
            if pos == 'romanization':
                skipped += 1
                continue

            # Skip entries without valid senses
            senses = entry.get('senses', [])
            if not senses:
                skipped += 1
                continue

            word = entry.get('word', '')
            if not word:
                skipped += 1
                continue

            reading = extract_reading(entry)
            meanings = extract_meanings(entry)

            if not meanings or 'Rōmaji transcription' in meanings:
                skipped += 1
                continue

            entries[word].append({
                'word': word,
                'reading': reading,
                'meanings': meanings,
                'pos': pos
            })

    # Flatten and deduplicate
    result = []
    for word, word_entries in entries.items():
        # Combine entries for same word
        combined_meanings = []
        combined_pos = set()
        reading = ''

        for e in word_entries:
            if not reading and e['reading']:
                reading = e['reading']
            combined_meanings.append(e['meanings'])
            if e['pos']:
                combined_pos.add(e['pos'])

        result.append({
            'word': word,
            'reading': reading if reading else word,
            'meanings': '; '.join(combined_meanings[:5]),  # Limit combined meanings
            'pos': ', '.join(sorted(combined_pos)[:3])
        })

    print(f"Parsed {total} lines, skipped {skipped}, got {len(result)} unique entries")
    return result


def write_output(entries: list, output_dir: str, prefix: str = 'wikt'):
    """Write index and data files."""
    output_path = Path(output_dir)
    output_path.mkdir(parents=True, exist_ok=True)

    index_path = output_path / f'{prefix}_index.bin'
    data_path = output_path / f'{prefix}_entries.dat'

    print(f"Writing {len(entries)} entries...")

    # Build index entries with file offsets
    index_entries = []

    with open(data_path, 'wb') as data_file:
        for entry in entries:
            offset = data_file.tell()

            # Write entry as tab-separated line
            line = f"{entry['word']}\t{entry['reading']}\t{entry['meanings']}\t{entry['pos']}\n"
            line_bytes = line.encode('utf-8')

            # Skip entries that are too long
            if len(line_bytes) > 65535:
                continue

            data_file.write(line_bytes)

            # Create index entry (hash by word, not reading)
            hash_value = fnv1a_hash(entry['word'])
            index_entries.append({
                'hash': hash_value,
                'offset': offset,
                'length': len(line_bytes)
            })

    # Sort index by hash for binary search
    index_entries.sort(key=lambda x: x['hash'])

    # Write index file
    with open(index_path, 'wb') as index_file:
        # Write entry count (4 bytes)
        index_file.write(struct.pack('<I', len(index_entries)))

        # Write index entries (10 bytes each)
        for idx in index_entries:
            index_file.write(struct.pack('<IIH',
                idx['hash'],
                idx['offset'],
                min(idx['length'], 65535)
            ))

    # Print statistics
    data_size = data_path.stat().st_size
    index_size = index_path.stat().st_size

    print(f"\nOutput files:")
    print(f"  {index_path}: {index_size:,} bytes ({len(index_entries)} entries)")
    print(f"  {data_path}: {data_size:,} bytes")
    print(f"  Total: {(index_size + data_size):,} bytes")
    print(f"\nCopy these files to SD card /dict/ folder")


def main():
    if len(sys.argv) < 3:
        print("Usage: python convert_wiktionary.py <wiktionary.jsonl> <output_dir>")
        print("\nExample:")
        print("  python convert_wiktionary.py kaikki.org-dictionary-Japanese.jsonl ./dict")
        sys.exit(1)

    jsonl_path = sys.argv[1]
    output_dir = sys.argv[2]

    if not os.path.exists(jsonl_path):
        print(f"Error: File not found: {jsonl_path}")
        sys.exit(1)

    entries = parse_wiktionary(jsonl_path)
    write_output(entries, output_dir)
    print("\nDone!")


if __name__ == '__main__':
    main()
