# CHIMERA for Cardputer-Adv

[Project site](https://m0rg0t.github.io/chimera-cardputer-adv/) · [Download the current build](dist/chimera-cardputer-adv-standalone.bin) · [Releases](https://github.com/m0rg0t/chimera-cardputer-adv/releases) · [Original CHIMERA](https://github.com/jamesondh/chimera-vocal-synth)

A standalone voice synthesizer for the M5Stack Cardputer-Adv. Type English on the built-in keyboard and press **Enter** to speak. The LCD shows a color-coded cross-section of CHIMERA’s head. The tongue, lips, jaw, and soft palate move with the active phoneme. Say wraps text and scrolls as you type. Piano highlights the key and vowel in use. Songs plays three public-domain melodies with the live voice engine and animated head. Speech is created on the ESP32-S3 at 24 kHz and played through the ES8311 codec. No computer, Wi-Fi, or SD card is needed after installation.

## Controls

| Mode | Keys |
| --- | --- |
| Say | Type up to 96 characters. **Enter** speaks the line, **Backspace** edits it, and **Fn + backtick** stops playback. Use **Fn + -** and **Fn + =** to adjust volume. The line stays available for editing or replay. |
| All modes | **Tab** cycles through Say, Piano, and Songs. |
| Piano | **A W S E D F T G Y H U J K** play a chromatic scale. **1–5** select /a e i o u/. **Z/X** move an octave. **-/=** change the shared volume. **Enter** toggles the demo. |
| Songs | **1–3** start a melody. **Enter** starts or stops the selected melody. **Fn + backtick** stops playback. Use **Fn + -** and **Fn + =** to adjust volume. |

Volume starts at 69%. All modes share the setting. The maximum speaker mixer gain is 6 dB higher than in the initial build. A peak limiter keeps loud notes and bursts within the digital output range.

Say supports English, common punctuation, and numbers read digit by digit. Unlisted words use spelling rules. Cyrillic input and Russian pronunciation are outside this version.

## Songs

The ESP32-S3 renders these single-voice arrangements live at 24 kHz. The head follows the vowel used for each note.

| Key | Melody | Historical source |
| --- | --- | --- |
| **1** | **Ode to Joy**, theme excerpt | Beethoven’s Ninth Symphony, finale. [Berlin State Library autograph](https://staatsbibliothek-berlin.de/die-staatsbibliothek/abteilungen/musik/sammlungen/bestaende/l-van-beethoven-9-sinfonie/willkommen) |
| **2** | **Frère Jacques** | Traditional French round documented in an eighteenth-century manuscript. [BnF manuscript MS-1843](https://catalogue.bnf.fr/ark:/12148/cb43892737w) |
| **3** | **Twinkle, Twinkle, Little Star** | Traditional French tune *Ah! vous dirai-je, maman*, also used in Mozart’s K. 265. [Mozarteum score](https://dme.mozarteum.at/DME/objs/pdf/nma_199_49_57.pdf) |

The firmware stores the note sequences and synthesizes the performances. The three melodies are in the public domain.

See [Adding a melody](CONTRIBUTING.md#adding-a-melody) for the score format, menu updates, and firmware checks.

## Build and install

The [standalone image built from this branch](dist/chimera-cardputer-adv-standalone.bin) includes Songs.

[Install release v0.1.0 over USB](https://m0rg0t.github.io/chimera-cardputer-adv/#install) from a desktop browser such as Chrome or Edge. With the Cardputer-Adv in download mode, choose **Connect and install**, select its serial port, and confirm. The installer uses Web Serial through [ESP Web Tools](https://esphome.github.io/esp-web-tools/).

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

The [upstream CHIMERA engine](https://github.com/jamesondh/chimera-vocal-synth/tree/e8b95a37b64491dc93454b621c40295c7eb65b3e) uses a double-precision nonlinear finite-difference solver. This firmware implements a smaller single-precision simulation with a 12-section oral tract, an eight-section nasal branch, and a glottal pulse derived from CHIMERA. The pressure waves, stop releases, and frication are synthesized on the ESP32-S3. The glottal pulse passes through a presence filter and high-shelf filter calibrated against sustained vowels from the full engine. The tract still shapes that excitation and generates consonant noise locally. The firmware stores anatomy and pronunciation data, with no recorded speech.

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
- Desktop tests exercise sustained vowels, pitch, release, speech planning, out-of-dictionary words, long inputs, cancellation, and block-size independence. Song tests cover timing, the opening pitches, stopping, and restarting. The speech WAV is synthesized by the same C++ code that runs on the board.
- On desktop, the mean level-normalized spectral error against the full engine fell from 6.95 to 5.05 dB across 14 vowels at 110, 145, 185, and 220 Hz. The reference ran at 48 kHz with 2× oversampling and was resampled to 24 kHz. The metric is RMS error across 25 mel-spaced bands from 160 Hz to 6.2 kHz over the last 500 ms of each note. The 145 and 220 Hz pitches were held out of the fit. This measures steady vowel timbre.
- A local Whisper check recognized “The sun is shining today.” Longer lines with names remain harder to understand.
- An instrumented Say build ran “Hello, the sun is shining today” on a physical 240 MHz Cardputer-Adv at 24 kHz. At full volume it rendered 200 speech blocks with a mean of 0.938 ms and a maximum render time of 1.087 ms against a 10.667 ms deadline. All 200 blocks reached the speaker queue with zero enqueue failures or empty queue observations. The head update loop ran 61 times during speech and observed 24 phoneme changes. The test submitted text programmatically through the same Say handler as Enter. Acoustic output, LCD appearance, and physical key entry still need a hands-on check.

To repeat the Say benchmark, apply [the instrumentation patch](full-engine/say-benchmark.patch) with `patch -p1 < full-engine/say-benchmark.patch` in a copy of this project. Build and flash that copy, then capture the report at 115200 baud. The test sets full volume and submits the phrase automatically after boot.

CHIMERA is by Jameson Hodge. See [LICENSE](LICENSE) and [CMUDICT-LICENSE.txt](CMUDICT-LICENSE.txt). The framework and M5 library versions are pinned in [platformio.ini](platformio.ini).

The [full-engine probe](full-engine/README.md) runs CHIMERA’s original Rust human voice on the ESP32-S3. At 24 kHz on the Cardputer-Adv, it averaged 1,109 ms per block in float64 and 161 ms in float32 against a 10.667 ms deadline.
