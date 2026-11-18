"""Select f32 for the pinned engine and its benchmark without changing topology or equations."""
from pathlib import Path
import re
import sys

root = Path(sys.argv[1])
parts = ["crates/chimera/src", "crates/cardputer-probe/src"]
for part in parts:
    for source in (root / part).rglob("*.rs"):
        text = source.read_text()
        text = re.sub(r"\bf64\b|(?<=[0-9_])f64\b", "f32", text)
        source.write_text(text)

math = root / "crates/chimera/src/math.rs"
text = math.read_text()
text, wrappers = re.subn(r"libm::([a-z0-9]+)\(", r"libm::\1f(", text)
assert wrappers == 17, "review changes to the pinned math wrappers"
math.write_text(text)

# Preserve a nonzero denominator guard in the LPC analysis helper.
lpc = root / "crates/chimera/src/analysis/lpc.rs"
text = lpc.read_text()
assert text.count("1e-300") == 1
lpc.write_text(text.replace("1e-300", "f32::MIN_POSITIVE"))

probe = root / "crates/cardputer-probe/src/lib.rs"
text = probe.read_text()
assert text.count("f32::from(rate)") == 2
probe.write_text(text.replace("f32::from(rate)", "rate as f32"))
