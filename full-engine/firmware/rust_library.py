"""Link the prebuilt ESP32-S3 Rust static library into the Arduino probe."""
import os
from pathlib import Path

Import("env")  # injects the active PlatformIO environment

archive = os.environ.get("CHIMERA_PROBE_LIB")
if not archive or not Path(archive).is_file():
    raise SystemExit("Set CHIMERA_PROBE_LIB to the ESP32-S3 libchimera_cardputer_probe.a")
env.Append(LIBS=[env.File(str(Path(archive).resolve()))])
