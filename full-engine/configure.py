"""Add a configurable score reserve to the pinned snapshot; keep its default."""
from pathlib import Path
import sys

root = Path(sys.argv[1])


def replace(path, old, new):
    path = root / path
    text = path.read_text()
    if text.count(old) != 1:
        raise SystemExit(f"Expected one pinned source anchor in {path}")
    path.write_text(text.replace(old, new, 1))


replace("Cargo.toml", '"crates/chimera-cli"]', '"crates/chimera-cli", "crates/cardputer-probe"]')
score = "crates/chimera/src/control/score.rs"
replace(score, '    gestures: Vec<Gesture>,', '    gestures: Vec<Gesture>,\n    limit: usize,')
replace(score, '''        GestureScore { gestures: Vec::with_capacity(Self::CAPACITY), end: 0.0, dropped: 0 }
    }''', '''        Self::with_capacity(Self::CAPACITY)
    }

    pub fn with_capacity(limit: usize) -> Self {
        GestureScore { gestures: Vec::with_capacity(limit), limit, end: 0.0, dropped: 0 }
    }''')
replace(score, 'self.gestures.len() >= Self::CAPACITY', 'self.gestures.len() >= self.limit')
voice = "crates/chimera/src/voice/human_voice.rs"
replace(voice, '''        let m = oversample;''', '''        Self::with_score_capacity(fs_out, body, oversample, GestureScore::CAPACITY)
    }

    /// A bounded gesture queue for hosts with limited RAM.
    pub fn with_score_capacity(fs_out: f64, body: HumanBody, oversample: usize, capacity: usize) -> Self {
        let m = oversample;''')
replace(voice, 'score: GestureScore::new(),', 'score: GestureScore::with_capacity(capacity),')
replace(voice, '''        let m = oversample;
        let fs =''', '''        Self::with_options(fs_out, body, oversample, capacity, true)
    }

    /// Disable the glottal oscilloscope buffers in audio-only hosts.
    pub fn with_options(
        fs_out: f64, body: HumanBody, oversample: usize, capacity: usize, capture_scope: bool,
    ) -> Self {
        let m = oversample;
        let scope_len = if capture_scope { SCOPE_LEN } else { 0 };
        let fs =''')
for field in ("scope_area", "scope_flow"):
    replace(voice, f"{field}: vec![0.0; SCOPE_LEN]", f"{field}: vec![0.0; scope_len]")

path = root / voice
text = path.read_text()
start = text.index("                    // glottal scope at output rate\n")
end = text.index("\n                }", start)
scope = text[start:end]
replace(voice, scope, "                    if !self.scope_area.is_empty() {\n"
        + "\n".join("    " + line for line in scope.splitlines()) + "\n                    }")

text = path.read_text()
start = text.index("        // phase-locked glottal scope: two most recent complete cycles\n")
end = text.index("        let mut noise_level", start)
scope = text[start:end]
scope = scope.replace("        out.glottal_area.clear();\n        out.glottal_flow.clear();\n", "")
replace(voice, text[start:end], "        out.glottal_area.clear();\n        out.glottal_flow.clear();\n"
        + "        if !self.scope_area.is_empty() {\n"
        + "\n".join("    " + line for line in scope.splitlines()) + "\n        }\n")
