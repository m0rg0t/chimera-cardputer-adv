#!/usr/bin/env bash
set -euo pipefail

here=$(cd "$(dirname "$0")" && pwd)
source_repo=${1:?"usage: $0 /path/to/chimera-vocal-synth /new/output/directory [float64|float32]"}
output=${2:?"pass a new output directory"}
revision=e8b95a37b64491dc93454b621c40295c7eb65b3e
precision=${3:-float64}
case "$precision" in
    float64|float32) ;;
    *) printf '%s\n' "Precision must be float32 or float64" >&2; exit 2 ;;
esac

mkdir "$output"
git -C "$source_repo" archive "$revision" Cargo.toml Cargo.lock crates LICENSE | tar -xf - -C "$output"
cp -R "$here/probe" "$output/crates/cardputer-probe"
python3 "$here/configure.py" "$output"

if [ "$precision" = float32 ]; then
    python3 "$here/precision32.py" "$output"
fi
