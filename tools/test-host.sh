#!/usr/bin/env bash
set -euo pipefail

root=$(cd "$(dirname "$0")/.." && pwd)
scratch=$(mktemp -d)
trap 'rm -rf "$scratch"' EXIT
cd "$root"
"${CXX:-c++}" -O2 -std=c++17 -Wall -Wextra -Werror -I"$root/src" \
    src/voice.cpp src/speech.cpp src/lexicon.S tests/voice_test.cpp -o "$scratch/voice-test"
"$scratch/voice-test" "$@"
