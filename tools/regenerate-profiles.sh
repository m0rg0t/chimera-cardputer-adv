#!/usr/bin/env bash
set -euo pipefail

root=$(cd "$(dirname "$0")/.." && pwd)
source_repo=${1:?"usage: $0 /path/to/chimera-vocal-synth [--bench]"}
shift
revision=e8b95a37b64491dc93454b621c40295c7eb65b3e
scratch=$(mktemp -d)
trap 'rm -rf "$scratch"' EXIT

git -C "$source_repo" archive "$revision" Cargo.toml Cargo.lock crates | tar -xf - -C "$scratch"
cp "$root/tools/export_profiles.rs" "$scratch/crates/chimera/examples/cardputer_profiles.rs"
(cd "$scratch" && cargo run --locked --release -p chimera --example cardputer_profiles -- "$root/tools/phones.tsv" "$@") \
    > "$scratch/profiles.generated.hpp"
cp "$scratch/profiles.generated.hpp" "$root/src/profiles.generated.hpp"
