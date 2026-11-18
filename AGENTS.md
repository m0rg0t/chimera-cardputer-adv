# CHIMERA for Cardputer-Adv

## Songs

- Follow [Adding a melody](CONTRIBUTING.md#adding-a-melody) for score units, index order, LCD layout, tests, and packaging.
- Keep the `Song` enum, `kSongTitles`, `kScores`, host expectations, and README key mapping in sync. The current LCD menu fits three entries.
- Use verified public-domain compositions for the public repertoire. Write the arrangements as note events and cite the historical sources in the README. Keep private scores, recordings, and local filming builds out of public commits, firmware downloads, and the Pages site.
- For score or player changes, run `tools/test-host.sh` and `pio run -e cardputer-adv`. For menu changes, also render the [desktop preview](README.md#desktop-screen-preview) and inspect the Songs frame. Report the hardware checks performed.
- When refreshing the public firmware images, build from the public checkout and regenerate `dist/SHA256SUMS`. Review the staged files before pushing.
