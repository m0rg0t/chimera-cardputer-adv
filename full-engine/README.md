# Full human voice on ESP32-S3

This diagnostic firmware compiles CHIMERA’s original Rust human voice for `xtensa-esp32s3-espidf`. It runs CHIMERA’s nonlinear airway, pressure-driven glottis, articulatory geometry, and gesture planner in either float64 or float32. Its input is a fixed phoneme sequence for “Hello, the sun is shining today.” Say and Piano in the main Cardputer application use their own firmware image.

The [pinned upstream snapshot](https://github.com/jamesondh/chimera-vocal-synth/tree/e8b95a37b64491dc93454b621c40295c7eb65b3e) is prepared in a fresh directory. The probe uses a 64-gesture queue, builds the 53-gesture test plan before allocating the voice, and disables capture of the two glottal oscilloscope channels. The device task has a 32 KiB stack. These changes preserve the numerical solver and glottal equations. Desktop defaults remain available in the prepared snapshot.

## What was verified

- Espressif Rust 1.97.0.0 cross-compiled the engine. PlatformIO linked it into a 897,325-byte application using 20,176 bytes of static RAM.
- Desktop tests matched the reduced-memory settings to the stock-capacity renderer bit for bit across 214,016 samples at 24 kHz 1×, 24 kHz 2×, and 48 kHz 2×. Voice, geometry, and planning golden tests also passed.
- In Espressif’s ESP32-S3 emulator, the 24 kHz 1× voice initialized, left 69,200 bytes of free heap, and produced 2,048 finite, nonzero-audio samples (peak 0.289). The bounded emulator run reached that checkpoint; it did not finish all three timing cases. The emulator check exercises the solver without LCD drawing, I²S playback, or arbitrary text input. See [esp-emulator](https://github.com/espressif/esp-emulator).

## Physical Cardputer-Adv result

On 2026-11-18, both precisions ran on the same 240 MHz ESP32-S3 Cardputer-Adv at 24 kHz without oversampling. Each run measured the first 32 blocks of 256 samples from the same 53-gesture plan. The measurements cover PCM generation only. Both runs produced 8,192 finite samples with a peak amplitude of approximately 0.289.

| Metric | Float64 | Float32 |
| --- | ---: | ---: |
| Playback budget per block | 10.667 ms | 10.667 ms |
| Mean render time per block | 1,109.152 ms | 160.989 ms |
| Worst render time per block | 1,637.427 ms | 228.791 ms |
| Mean time relative to playback budget | 104× | 15.1× |
| Free heap after voice initialization | 69,200 bytes | 199,864 bytes |
| Minimum free heap during the run | 55,836 bytes | 192,748 bytes |

Float32 reduced mean render time by 6.9×. The full solver still misses the real-time deadline. The prepared float32 source retains the upstream grid, airway branches, governing equations, and planner. It uses single-precision arithmetic and math functions across the engine. The near-zero guard in the LPC analysis helper uses the smallest positive normal float32 value.

A desktop comparison of 72,192 samples at 24 kHz and 1× found a 0.003 dB RMS level difference, 0.99976 waveform correlation, and 33.24 dB waveform SNR relative to float64. This is a comparison of one phrase. The tolerances and scope of the original float64 golden suite are unchanged.

The float32 build uses Espressif Rust 1.98.1.0, which fixes an Xtensa compiler crash encountered with 1.97.0.0. [Espressif Rust issue #277](https://github.com/esp-rs/rust/issues/277).

The [reduced waveguide in Say mode](../README.md#verification) met the deadline on the same board with speaker output and head updates active.

Double precision on the ESP32-S3 is implemented in software. [Espressif FreeRTOS guide](https://docs.espressif.com/projects/esp-idf/en/v4.4.5/esp32s3/api-guides/freertos-smp.html).

When assembling a merged image, use `--flash_mode dio --flash_freq 80m --flash_size 8MB`. The Cardputer-Adv’s ROM uses the DIO header to load the software bootloader. [Espressif boot mode documentation](https://docs.espressif.com/projects/esptool/en/latest/esp32s3/advanced-topics/boot-mode-selection.html).

## Rebuild

From this directory, with Python 3, PlatformIO, and the [Espressif Rust toolchain with rust-src](https://docs.espressif.com/projects/rust/book/getting-started/toolchain.html):

```sh
./prepare.sh ../../chimera-russian /tmp/chimera-full-engine
(cd /tmp/chimera-full-engine && cargo +esp build --release -Zbuild-std=std,panic_abort \
  --target xtensa-esp32s3-espidf -p chimera-cardputer-probe)
export CHIMERA_PROBE_LIB=/tmp/chimera-full-engine/target/xtensa-esp32s3-espidf/release/libchimera_cardputer_probe.a
pio run -d firmware
```

For the short float32 benchmark, use a separate prepared directory and the matching probe firmware.

```sh
./prepare.sh ../../chimera-russian /tmp/chimera-float32 float32
(cd /tmp/chimera-float32 && cargo +esp build --release -Zbuild-std=std,panic_abort \
  --target xtensa-esp32s3-espidf -p chimera-cardputer-probe)
export CHIMERA_PROBE_LIB=/tmp/chimera-float32/target/xtensa-esp32s3-espidf/release/libchimera_cardputer_probe.a
pio run -d firmware-float32
```

Desktop comparison and goldens use stock Cargo.

```sh
cd /tmp/chimera-full-engine
cargo run --release -p chimera-cardputer-probe --example host
cargo test -p chimera --test golden_voice --test golden_geometry --test golden_planning
```

Render the same phrase from each freshly prepared desktop checkout with `cargo run --release -q -p chimera-cardputer-probe --example capture -- 24000 1 282 > phrase.pcm`. The output is mono little-endian float32 PCM in both cases. Use a separate Cargo target directory per checkout.

## Run the probe on a Cardputer-Adv

`chimera-full-engine-probe.bin` is the three-rate float64 diagnostic. `chimera-float32-probe.bin` runs the 32-block float32 benchmark. Flashing either image at offset `0x0` replaces the active firmware and partition table. Use the [main firmware image](../dist/chimera-cardputer-adv-standalone.bin) to return to Say and Piano. See [M5Stack’s download-mode instructions](https://docs.m5stack.com/en/core/Cardputer-Adv).

Open the USB serial monitor at 115200 baud and reset the device to capture startup. The probe reports allocation and stack headroom, average and worst block duration, and whether rendered samples are finite. Each test runs once per reset. The float32 image repeats its final summary on the serial console.
