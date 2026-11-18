#include "speech.hpp"
#include "songs.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <fstream>
#include <iostream>
#include <vector>

using namespace chimera_adv;
using Sample = std::int16_t;

int peak(const Sample* first, const Sample* last) {
    int result = 0;
    for (; first != last; ++first) {
        result = std::max(result, std::abs(static_cast<int>(*first)));
    }
    return result;
}

std::vector<Sample> renderVowel(unsigned vowel) {
    Voice voice;
    voice.setControl(57, vowel);  // A3 is 220 Hz.
    std::vector<Sample> pcm(kSampleRate);
    voice.render(pcm.data(), pcm.size());
    return pcm;
}

void writeWave(const char* path, const std::vector<Sample>& pcm) {
    std::ofstream file(path, std::ios::binary);
    const auto little = [&file](std::uint32_t value, unsigned bytes) {
        for (unsigned i = 0; i < bytes; ++i) {
            file.put(static_cast<char>((value >> (8u * i)) & 0xffu));
        }
    };
    const unsigned dataBytes = pcm.size() * 2;
    file.write("RIFF", 4);
    little(36 + dataBytes, 4);
    file.write("WAVEfmt ", 8);
    little(16, 4);
    little(1, 2);
    little(1, 2);
    little(kSampleRate, 4);
    little(kSampleRate * 2, 4);
    little(2, 2);
    little(16, 2);
    file.write("data", 4);
    little(dataBytes, 4);
    for (auto sample : pcm) {
        little(static_cast<std::uint16_t>(sample), 2);
    }
    assert(file);
}

std::vector<Sample> renderSpeech(std::string_view text, std::size_t blockSize) {
    Voice voice;
    Speech speech;
    assert(speech.start(text));
    std::vector<Sample> buffer(blockSize);
    std::vector<Sample> result;
    while (speech.active()) {
        const auto n = speech.render(voice, buffer.data(), buffer.size());
        assert(n > 0);
        result.insert(result.end(), buffer.begin(), buffer.begin() + n);
    }
    return result;
}

std::vector<Sample> renderSong(Song song, std::size_t blockSize) {
    Voice voice;
    SongPlayer player;
    player.start(song);
    std::vector<Sample> buffer(blockSize);
    std::vector<Sample> result;
    while (player.active()) {
        const auto n = player.render(voice, buffer.data(), buffer.size());
        assert(n > 0 && n <= buffer.size());
        assert(player.pose() < PhCount);
        result.insert(result.end(), buffer.begin(), buffer.begin() + n);
    }
    assert(player.pose() == PhIdle);
    return result;
}

void testSongs() {
    constexpr unsigned beats[] = {32, 32, 48};
    constexpr unsigned bpm[] = {110, 124, 112};
    constexpr unsigned firstNotePeriod[] = {65, 92, 92};  // F#4, C4, C4.
    std::array<std::vector<Sample>, kSongCount> songs;
    for (unsigned i = 0; i < kSongCount; ++i) {
        const auto track = static_cast<Song>(i);
        auto& audio = songs[i];
        audio = renderSong(track, 256);
        assert(audio == renderSong(track, 73));
        assert(audio.size() == (beats[i] * kSampleRate * 60 + bpm[i] / 2) / bpm[i]);
        const int amplitude = peak(audio.data(), audio.data() + audio.size());
        assert(amplitude > 1000 && amplitude <= kVoicePeakPcm);

        double periodError = 0, otherError = 0;
        const auto period = firstNotePeriod[i];
        for (unsigned n = 5000; n < 6000; ++n) {
            const double aligned = audio[n] - audio[n - period];
            const double misaligned = audio[n] - audio[n - period * 4 / 5];
            periodError += aligned * aligned;
            otherError += misaligned * misaligned;
        }
        assert(periodError < otherError * .08);
        for (unsigned previous = 0; previous < i; ++previous) {
            assert(audio != songs[previous]);
        }

        Voice voice;
        SongPlayer player;
        std::array<Sample, 256> block;
        player.start(track);
        assert(player.render(voice, block.data(), block.size()) == block.size());
        assert(player.pose() != PhIdle);
        player.stop();
        assert(!player.active() && player.pose() == PhIdle);
        assert(player.render(voice, block.data(), block.size()) == 0);
        voice = Voice{};
        player.start(track);
        assert(player.render(voice, block.data(), block.size()) == block.size());
        assert(std::equal(block.begin(), block.end(), audio.begin()));
        std::cout << kSongTitles[i] << ", " << double(audio.size()) / kSampleRate
                  << " seconds, peak " << amplitude << '\n';
    }
}

int main(int argc, char** argv) {
    for (const auto& profile : kProfiles) {
        for (const auto& point : profile.head) {
            assert(point.x < 85 && point.y < 120);
        }
    }
    std::array<std::vector<Sample>, kVowelCount> vowels;
    for (unsigned vowel = 0; vowel < kVowelCount; ++vowel) {
        auto& pcm = vowels[vowel];
        pcm = renderVowel(vowel);
        const int amplitude = peak(pcm.data(), pcm.data() + pcm.size());
        assert(amplitude >= 100 && amplitude < kVoicePeakPcm);

        // One steady A3 cycle is 109.09 samples. A one-cycle delay should match
        // much better than a nonharmonic delay regardless of which formants dominate.
        double onePeriodError = 0;
        double offsetError = 0;
        for (std::size_t i = kSampleRate / 2; i < pcm.size(); ++i) {
            const double periodic = pcm[i] - pcm[i - 109];
            const double other = pcm[i] - pcm[i - 93];
            onePeriodError += periodic * periodic;
            offsetError += other * other;
        }
        assert(onePeriodError < offsetError * 0.02);
        for (unsigned other = 0; other < vowel; ++other) {
            assert(pcm != vowels[other]);
        }
        std::cout << "AEIOU"[vowel] << " at A3, peak " << amplitude << '\n';
    }

    Voice voice;
    std::vector<Sample> pcm(kSampleRate);
    voice.render(pcm.data(), pcm.size());
    assert(peak(pcm.data(), pcm.data() + pcm.size()) == 0);
    voice.setControl(57, 0);
    for (std::size_t offset = 0; offset < pcm.size(); offset += 256) {
        voice.render(pcm.data() + offset, std::min<std::size_t>(256, pcm.size() - offset));
    }
    assert(pcm == vowels[0]);

    voice.setControl(0, 4);
    voice.render(pcm.data(), pcm.size());
    assert(peak(pcm.data(), pcm.data() + 256) > 0);
    assert(peak(pcm.data() + kSampleRate / 5, pcm.data() + pcm.size()) == 0);

    // Exercise the keyboard range and several live vowel changes at every note.
    Voice stress;
    int stressPeak = 0;
    for (unsigned note = 36; note <= 84; ++note) {
        for (unsigned vowel = 0; vowel < kVowelCount; ++vowel) {
            stress.setControl(note, vowel);
            stress.render(pcm.data(), 1024);
            stressPeak = std::max(stressPeak, peak(pcm.data(), pcm.data() + 1024));
        }
    }

    assert(stressPeak <= kVoicePeakPcm);
    std::cout << "Keyboard stress peak " << stressPeak << " / " << kVoicePeakPcm << '\n';

    std::vector<Sample> preview;
    Voice performer;
    for (unsigned vowel = 0; vowel < kVowelCount; ++vowel) {
        performer.setControl(48, vowel);
        performer.render(pcm.data(), kSampleRate / 2);
        preview.insert(preview.end(), pcm.begin(), pcm.begin() + kSampleRate / 2);
        performer.setControl(0, vowel);
        performer.render(pcm.data(), kSampleRate / 8);
        preview.insert(preview.end(), pcm.begin(), pcm.begin() + kSampleRate / 8);
    }
    constexpr unsigned notes[] = {48, 52, 55, 57, 55, 52, 50, 55, 48};
    for (unsigned i = 0; i < std::size(notes); ++i) {
        performer.setControl(notes[i], i % kVowelCount);
        performer.render(pcm.data(), kSampleRate / 4);
        preview.insert(preview.end(), pcm.begin(), pcm.begin() + kSampleRate / 4);
    }
    performer.setControl(0, 0);
    performer.render(pcm.data(), kSampleRate / 4);
    preview.insert(preview.end(), pcm.begin(), pcm.begin() + kSampleRate / 4);
    assert(peak(preview.data(), preview.data() + preview.size()) <= kVoicePeakPcm);
    if (argc >= 2) {
        writeWave(argv[1], preview);
    }
    constexpr auto text = "Hello, this is Chimera. The Cardputer can speak on its own.";
    const auto speech = renderSpeech(text, 256);
    assert(speech == renderSpeech(text, 71));
    assert(speech != renderSpeech("Are there five voices on this machine?", 256));
    assert(peak(speech.data(), speech.data() + speech.size()) > 1000);
    const auto atLimit = std::count_if(speech.begin(), speech.end(),
                                      [](auto n) { return std::abs(int(n)) == kVoicePeakPcm; });
    assert(static_cast<std::size_t>(atLimit) * 1000 < speech.size());
    const auto fallback = renderSpeech("Zibberquux can't say 429.", 256);
    assert(peak(fallback.data(), fallback.data() + fallback.size()) > 1000);
    const auto longLine = renderSpeech(std::string(kTextLimit, '7'), 2048);
    assert(longLine.size() > 30 * kSampleRate);
    Speech planner;
    assert(!planner.start("  ... "));
    assert(planner.start("Hello."));
    planner.cancel();
    assert(!planner.active());

    if (argc >= 3) {
        writeWave(argv[2], renderSpeech("The sun is shining today.", 256));
    }
    std::cout << "Speech preview: " << static_cast<double>(speech.size()) / kSampleRate
              << " seconds, peak " << peak(speech.data(), speech.data() + speech.size())
              << ", samples at limiter ceiling " << atLimit << '\n';
    testSongs();
    std::cout << "Voice, speech and song tests passed\n";
}
