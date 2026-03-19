#!/usr/bin/env python3
"""
JMdict to ESP32 Dictionary Converter

Converts JMdict XML to optimized binary format for ESP32.

Usage:
    python convert_jmdict.py JMdict_e.xml output_dir

Output files:
    - index.bin: Binary index file (sorted by reading hash)
    - entries.dat: Tab-separated entry data

Index format (10 bytes per entry):
    - hash (4 bytes): FNV-1a hash of reading
    - offset (4 bytes): File offset in entries.dat
    - length (2 bytes): Entry length

Entry format (variable length):
    word\treading\tmeanings\tpos\n

Requirements:
    pip install lxml
"""

import sys
import os
import struct
import xml.etree.ElementTree as ET
from pathlib import Path


def fnv1a_hash(s: str) -> int:
    """Calculate FNV-1a hash of string (same as ESP32 implementation)."""
    hash_value = 2166136261
    for c in s.encode('utf-8'):
        hash_value ^= c
        hash_value = (hash_value * 16777619) & 0xFFFFFFFF
    return hash_value


def is_hiragana(char: str) -> bool:
    """Check if character is hiragana."""
    code = ord(char)
    return 0x3040 <= code <= 0x309F


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


def parse_jmdict(xml_path: str) -> list:
    """Parse JMdict XML and extract entries."""
    print(f"Parsing {xml_path}...")

    entries = []

    # Use iterparse for memory efficiency
    context = ET.iterparse(xml_path, events=('end',))

    for event, elem in context:
        if elem.tag == 'entry':
            # Extract kanji forms
            kanjis = [k.find('keb').text for k in elem.findall('k_ele')
                      if k.find('keb') is not None]

            # Extract readings
            readings = []
            for r in elem.findall('r_ele'):
                reb = r.find('reb')
                if reb is not None and reb.text:
                    # Convert to hiragana for indexing
                    reading = kata_to_hira(reb.text)
                    readings.append(reading)

            # Extract senses (meanings)
            meanings = []
            pos_list = []
            for sense in elem.findall('sense'):
                # Part of speech
                for pos in sense.findall('pos'):
                    if pos.text:
                        pos_list.append(pos.text)

                # Glosses (meanings)
                for gloss in sense.findall('gloss'):
                    if gloss.text:
                        # Prefer English for now
                        lang = gloss.get('{http://www.w3.org/XML/1998/namespace}lang', 'eng')
                        if lang == 'eng':
                            meanings.append(gloss.text)

            if readings and meanings:
                word = kanjis[0] if kanjis else readings[0]
                reading = readings[0]
                meaning_str = '; '.join(meanings[:5])  # Limit to 5 meanings
                pos_str = ', '.join(set(pos_list[:3]))  # Limit to 3 POS

                entries.append({
                    'word': word,
                    'reading': reading,
                    'meanings': meaning_str,
                    'pos': pos_str
                })

            # Clear element to free memory
            elem.clear()

    print(f"Parsed {len(entries)} entries")
    return entries


def write_output(entries: list, output_dir: str):
    """Write index and data files."""
    output_path = Path(output_dir)
    output_path.mkdir(parents=True, exist_ok=True)

    index_path = output_path / 'index.bin'
    data_path = output_path / 'entries.dat'

    print(f"Writing {len(entries)} entries...")

    # Build index entries with file offsets
    index_entries = []

    with open(data_path, 'wb') as data_file:
        for entry in entries:
            offset = data_file.tell()

            # Write entry as tab-separated line
            line = f"{entry['word']}\t{entry['reading']}\t{entry['meanings']}\t{entry['pos']}\n"
            line_bytes = line.encode('utf-8')
            data_file.write(line_bytes)

            # Create index entry
            hash_value = fnv1a_hash(entry['reading'])
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
                min(idx['length'], 65535)  # Cap at 16-bit max
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
        print("Usage: python convert_jmdict.py <jmdict.xml> <output_dir>")
        print("\nExample:")
        print("  python convert_jmdict.py JMdict_e.xml ./dict")
        sys.exit(1)

    xml_path = sys.argv[1]
    output_dir = sys.argv[2]

    if not os.path.exists(xml_path):
        print(f"Error: File not found: {xml_path}")
        sys.exit(1)

    entries = parse_jmdict(xml_path)
    write_output(entries, output_dir)
    print("\nDone!")


if __name__ == '__main__':
    main()
