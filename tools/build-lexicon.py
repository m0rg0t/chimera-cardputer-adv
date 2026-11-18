#!/usr/bin/env python3
"""Pack the upstream CMUdict and CHIMERA's fallback rules for speech in flash."""
import gzip
import re
import struct
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
REVISION = "e8b95a37b64491dc93454b621c40295c7eb65b3e"


def upstream(path):
    return subprocess.check_output(["git", "-C", sys.argv[1], "show", f"{REVISION}:{path}"])


phones = [line.split("\t") for line in (ROOT / "tools/phones.tsv").read_text().splitlines()]
ipa_ids = {ipa: i for i, (_, ipa) in enumerate(phones)}
ipa_ids["ɚ"] = ipa_ids["ɝ"]
english = upstream("crates/chimera/src/phonetics/english.rs").decode()
arpabet = dict(re.findall(r'"([A-Z]+)"\s*=>\s*"([^"]+)"', english.split("const VOWELS:")[0]))
dictionary = {}
for line in gzip.decompress(upstream("public/data/cmudict.txt.gz")).decode().splitlines():
    word, _, pronunciation = line.partition("\t")
    if not re.fullmatch(r"[a-z']{1,63}", word):
        continue
    packed = bytearray()
    for phone in pronunciation.split():
        stress = int(phone[-1]) if phone[-1].isdigit() else 0
        stem = phone.rstrip("012")
        ipa = "ə" if phone == "AH0" else arpabet.get(stem)
        if ipa not in ipa_ids:
            break
        packed.append(ipa_ids[ipa] | (stress << 6))
    else:
        if packed:
            dictionary[word] = packed

blocks = []
for i, (word, pronunciation) in enumerate(sorted(dictionary.items())):
    if i % 16 == 0:
        blocks.append(bytearray())
        previous = ""
    prefix = 0
    for a, b in zip(previous, word):
        if a != b:
            break
        prefix += 1
    blocks[-1].extend(bytes([prefix]) + word[prefix:].encode() + bytes([0, len(pronunciation)]))
    blocks[-1].extend(pronunciation)
    previous = word

data = bytearray()
index = bytearray()
for block in blocks:
    index.extend(struct.pack("<I", len(data)))
    data.extend(block)
payload = struct.pack("<I", len(dictionary)) + index + data
(ROOT / "src/lexicon.generated.bin").write_bytes(payload)
print(f"{len(dictionary)} pronunciations, {len(payload):,} flash bytes")

rules = english.split("const RULES: &[Rule] = &[")[1].split("];")[0]
lines = ["// Generated from CHIMERA english.rs by tools/build-lexicon.py.", "constexpr Rule kRules[] = {"]
for pre, spelling, post, ipa in re.findall(r'\("([^"]*)", "([^"]*)", "([^"]*)", "([^"]*)"\)', rules):
    symbols = [phones[ipa_ids[p]][0] for p in ipa.split()]
    values = ", ".join(f"Ph{p}" for p in symbols)
    lines.append(f'    {{"{pre}", "{spelling}", "{post}", {{{values}}}, {len(symbols)}}},')
lines.append("};")
(ROOT / "src/fallback.generated.inc").write_text("\n".join(lines) + "\n")
(ROOT / "CMUDICT-LICENSE.txt").write_bytes(upstream("public/data/CMUDICT-LICENSE.txt"))
