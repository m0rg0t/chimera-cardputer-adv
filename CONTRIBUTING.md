# Contributing to CHIMERA for Cardputer-Adv

## Adding a melody

The public Songs menu uses public-domain melodies arranged for CHIMERA's single-voice synthesizer. Check the source of the composition and add a link to a historical score or library record in the [README repertoire](README.md#songs). Write the note sequence in the firmware. Keep recordings, copied modern arrangements, and private filming scores and builds outside the public Git repository.

### Write the score

1. In [src/songs.cpp](src/songs.cpp), create a `constexpr SongNote[]`. Each entry contains a MIDI note and its duration in eighth notes. Group entries by measure so the sequence is easy to compare with the source.
2. Add the score to `kScores` with its array size, tempo in beats per minute, and the two vowel poses. Use one of `PhA`, `PhE`, `PhI`, `PhO`, and `PhU` for each pose. The second vowel is used for notes lasting at least three eighths. The player rounds absolute beat positions to samples at 24 kHz and releases the voice briefly between successive notes.
3. In [src/songs.hpp](src/songs.hpp), update the `Song` enum and `kSongTitles` in the same order as `kScores`. Keep `Song::Count` last. Use a short ASCII label for the LCD and the full title in the README. Update the README key and source table.

This two-note example holds middle C for a quarter note and E4 for a half note. The `Pitch` enum in `src/songs.cpp` gives readable names to the current MIDI notes. Add a named pitch there when you need a new one.

```cpp
constexpr SongNote kExample[] = {
    {C4, 2}, {E4, 4},
};

// Example entry in kScores at 110 beats per minute.
{kExample, std::size(kExample), 110, PhA, PhE},
```

Use MIDI notes from 1 to 127 and positive durations and tempo. The current score format has one pitch per event and a built-in release between notes. Explicit rests, chords, or lyrics need player support and tests before they can be encoded.

### Keep the menu and tests in sync

The current LCD menu fits three songs. To change the repertoire without changing the menu, replace a slot and keep its key assignment. To add a fourth entry, update the layout or add scrolling in `drawSongs` in [src/screen.cpp](src/screen.cpp), and check the key handling in `chooseSong` in [src/main.cpp](src/main.cpp). The current selection loop maps consecutive slots to keys `1`, `2`, and `3`.

Update the corresponding `beats`, `bpm`, and `firstNotePeriod` cases in `testSongs` in [tests/voice_test.cpp](tests/voice_test.cpp). The initial period is the nearest integer to `24000 / frequencyHz`. If a score has a half-beat total, express the duration expectation in eighths and round `eighths × 24000 × 30 / bpm` to the nearest sample. Keep the existing checks for distinct waveforms, block-size independence, stop, and restart.

Run from the repository root.

```sh
tools/test-host.sh
pio run -e cardputer-adv
```

With SDL2 installed, render the menu and inspect `songs.ppm` for clipped labels and overlapping rows.

```sh
pio run -e screen-preview
mkdir -p /tmp/chimera-screens
.pio/build/screen-preview/program /tmp/chimera-screens
```

If a Cardputer-Adv is available for testing, press **Tab** twice from Say and play, stop, and restart each edited entry. Check that the head follows the performance. USB serial at 115200 baud reports maximum render time against the roughly 10.67 ms budget per audio block, along with speaker-queue errors. Record which checks were run.

### Refresh firmware images when publishing a build

Build the public checkout to produce the ready-to-flash files under `dist/`. With esptool 4.x available, use the application, bootloader, and partition table from the same PlatformIO build. The following assumes PlatformIO's default packages directory or the directory set in `PLATFORMIO_CORE_DIR`.

```sh
build=.pio/build/cardputer-adv
pio_core=${PLATFORMIO_CORE_DIR:-$HOME/.platformio}
cp "$build/firmware.bin" dist/chimera-cardputer-adv-app.bin
esptool.py --chip esp32s3 merge_bin \
  --flash_mode dio --flash_freq 80m --flash_size 8MB \
  -o dist/chimera-cardputer-adv-standalone.bin \
  0x0 "$build/bootloader.bin" \
  0x8000 "$build/partitions.bin" \
  0xe000 "$pio_core/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin" \
  0x10000 "$build/firmware.bin"
(
  cd dist
  shasum -a 256 chimera-cardputer-adv-app.bin chimera-cardputer-adv-standalone.bin \
    chimera-screen-preview.png chimera-speech-preview.wav chimera-voice-preview.wav > SHA256SUMS
  shasum -a 256 -c SHA256SUMS
)
```

The installer on GitHub Pages uses a pinned release through [docs/firmware-manifest.json](docs/firmware-manifest.json). Update its version and firmware URL when publishing a new installer release.
