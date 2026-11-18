# CHIMERA for Cardputer-Adv

A standalone voice synthesizer for the M5Stack Cardputer-Adv. Type English on the built-in keyboard and press **Enter** to speak. The LCD shows a color-coded cross-section of CHIMERA’s head. The tongue, lips, jaw, and soft palate move with the active phoneme. Say wraps text and scrolls as you type. Piano highlights the key and vowel in use. Speech is created on the ESP32-S3 at 24 kHz and played through the ES8311 codec. No computer, Wi-Fi, or SD card is needed after installation.

## Controls

| Mode | Keys |
| --- | --- |
| Say | Type up to 96 characters. **Enter** speaks the line, **Backspace** edits it, and **Fn + backtick** stops playback. The line stays available for editing or replay. |
| Both | **Tab** switches between Say and Piano. |
| Piano | **A W S E D F T G Y H U J K** play a chromatic scale. **1–5** select /a e i o u/. **Z/X** move an octave. **-/=** change the shared volume. **Enter** toggles the demo. |

Say supports English, common punctuation, and numbers read digit by digit. Unlisted words use spelling rules. Cyrillic input and Russian pronunciation are outside this version.

## Build and install

```sh
pio run
tools/test-host.sh
```

The `dist` folder contains a standalone flash image, an application image for the matching PlatformIO partition layout, desktop WAV previews from the firmware DSP, and a screen preview rendered from the firmware’s drawing code.

**Flashing the standalone image replaces the active firmware and partition table.** Back up the device if you want to restore its current setup. Enter download mode by turning the Cardputer-Adv off, holding **G0**, and connecting it via USB. See the [official Cardputer-Adv guide](https://docs.m5stack.com/en/core/Cardputer-Adv).

```sh
pio run -t upload --upload-port /dev/cu.usbmodemXXXX
# Or use the prebuilt image with esptool 4.x
esptool.py --chip esp32s3 --port /dev/cu.usbmodemXXXX write_flash 0x0 dist/chimera-cardputer-adv-standalone.bin
```

The accompanying `chimera-cardputer-adv-app.bin` goes at `0x10000` with the matching 8 MB partition layout. Third-party launchers have not been tested.

When assembling a merged image, use `--flash_mode dio --flash_freq 80m --flash_size 8MB`. The Cardputer-Adv’s ROM uses the DIO header to load the software bootloader. [Espressif boot mode documentation](https://docs.espressif.com/projects/esptool/en/latest/esp32s3/advanced-topics/boot-mode-selection.html).

## Simulation

The [upstream CHIMERA engine](https://github.com/jamesondh/chimera-vocal-synth/tree/e8b95a37b64491dc93454b621c40295c7eb65b3e) uses a double-precision nonlinear finite-difference solver. This firmware implements a smaller single-precision simulation with a 12-section oral tract, an eight-section nasal branch, and a glottal pulse derived from CHIMERA. The pressure waves, stop releases, and frication are synthesized on the ESP32-S3. The firmware stores anatomy and pronunciation data, with no recorded speech.

The build-time exporter derives the acoustic profiles and 2D outlines from the same CHIMERA male anatomy and phoneme inventory. The LCD interpolates tongue, lips, jaw and soft palate among those profiles. It shows the sagittal anatomy view. The timing and articulation model are simplified, and the resulting speech differs from the browser version.

The firmware keeps 124,926 CMUdict pronunciations (1.53 MB) in flash. It uses CHIMERA’s English fallback rules for missing words. Regenerate the tables from a checkout containing the pinned upstream revision.

```sh
tools/regenerate-profiles.sh ../chimera-russian
python3 tools/build-lexicon.py ../chimera-russian
tools/test-host.sh dist/chimera-voice-preview.wav dist/chimera-speech-preview.wav
```

## Desktop screen preview

With SDL2 installed, build the host renderer and export 240 × 135 frames.

```sh
pio run -e screen-preview
mkdir -p /tmp/chimera-screens
.pio/build/screen-preview/program /tmp/chimera-screens
```

## Verification

- The same M5GFX drawing code rendered desktop previews of the empty editor, active speech, long text, the 96-character limit, and an active piano note.
- The ESP32-S3 build uses about 2.07 MB of the application partition and 24.8 KB of static RAM. The display sprite and task stacks allocate additional RAM at run time.
- Desktop tests exercise sustained vowels, pitch, release, speech planning, out-of-dictionary words, long inputs, cancellation, and block-size independence. The speech WAV is synthesized by the same C++ code that runs on the board.
- A local Whisper check recognized “The sun is shining today.” It misheard several words in a longer line with “Chimera” and “Cardputer,” so speech intelligibility is still experimental.
- An instrumented Say build ran “Hello, the sun is shining today” on a physical 240 MHz Cardputer-Adv at 24 kHz. It rendered 200 speech blocks with a mean of 0.862 ms and a maximum render time of 1.006 ms against a 10.667 ms deadline. All 200 blocks reached the speaker queue with zero enqueue failures or empty queue observations. The head update loop ran 61 times during speech and observed 24 phoneme changes. The test submitted text programmatically through the same Say handler as Enter. Acoustic output, LCD appearance, and physical key entry still need a hands-on check.

To repeat the Say benchmark, apply [the instrumentation patch](full-engine/say-benchmark.patch) with `patch -p1 < full-engine/say-benchmark.patch` in a copy of this project. Build and flash that copy, then capture the report at 115200 baud. The test submits the phrase automatically after boot.

CHIMERA is by Jameson Hodge. See [LICENSE](LICENSE) and [CMUDICT-LICENSE.txt](CMUDICT-LICENSE.txt). The framework and M5 library versions are pinned in [platformio.ini](platformio.ini).

The [full-engine probe](full-engine/README.md) runs CHIMERA’s original Rust human voice on the ESP32-S3. At 24 kHz on the Cardputer-Adv, it averaged 1,109 ms per block in float64 and 161 ms in float32 against a 10.667 ms deadline.
