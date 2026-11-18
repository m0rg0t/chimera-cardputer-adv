#include "songs.hpp"

#include <algorithm>
#include <iterator>

namespace chimera_adv {

struct SongNote {
    std::uint8_t midi;
    std::uint8_t eighths;
};

struct SongScore {
    const SongNote* notes;
    unsigned count;
    unsigned bpm;
    Phone vowel;
    Phone longVowel;
};

namespace {
enum Pitch : std::uint8_t { G3 = 55, C4 = 60, D4 = 62, E4 = 64, F4 = 65, Fs4 = 66, G4 = 67, A4 = 69 };

// CHIMERA arrangements of public-domain melodies. Source links are in the README.
constexpr SongNote kOdeToJoy[] = {
    {Fs4, 2}, {Fs4, 2}, {G4, 2}, {A4, 2},
    {A4, 2}, {G4, 2}, {Fs4, 2}, {E4, 2},
    {D4, 2}, {D4, 2}, {E4, 2}, {Fs4, 2},
    {Fs4, 3}, {E4, 1}, {E4, 4},
    {Fs4, 2}, {Fs4, 2}, {G4, 2}, {A4, 2},
    {A4, 2}, {G4, 2}, {Fs4, 2}, {E4, 2},
    {D4, 2}, {D4, 2}, {E4, 2}, {Fs4, 2},
    {E4, 3}, {D4, 1}, {D4, 4},
};

constexpr SongNote kFrereJacques[] = {
    {C4, 2}, {D4, 2}, {E4, 2}, {C4, 2},
    {C4, 2}, {D4, 2}, {E4, 2}, {C4, 2},
    {E4, 2}, {F4, 2}, {G4, 4},
    {E4, 2}, {F4, 2}, {G4, 4},
    {G4, 1}, {A4, 1}, {G4, 1}, {F4, 1}, {E4, 2}, {C4, 2},
    {G4, 1}, {A4, 1}, {G4, 1}, {F4, 1}, {E4, 2}, {C4, 2},
    {C4, 2}, {G3, 2}, {C4, 4},
    {C4, 2}, {G3, 2}, {C4, 4},
};

constexpr SongNote kTwinkle[] = {
    {C4, 2}, {C4, 2}, {G4, 2}, {G4, 2}, {A4, 2}, {A4, 2}, {G4, 4},
    {F4, 2}, {F4, 2}, {E4, 2}, {E4, 2}, {D4, 2}, {D4, 2}, {C4, 4},
    {G4, 2}, {G4, 2}, {F4, 2}, {F4, 2}, {E4, 2}, {E4, 2}, {D4, 4},
    {G4, 2}, {G4, 2}, {F4, 2}, {F4, 2}, {E4, 2}, {E4, 2}, {D4, 4},
    {C4, 2}, {C4, 2}, {G4, 2}, {G4, 2}, {A4, 2}, {A4, 2}, {G4, 4},
    {F4, 2}, {F4, 2}, {E4, 2}, {E4, 2}, {D4, 2}, {D4, 2}, {C4, 4},
};

constexpr SongScore kScores[] = {
    {kOdeToJoy, std::size(kOdeToJoy), 110, PhA, PhE},
    {kFrereJacques, std::size(kFrereJacques), 124, PhE, PhI},
    {kTwinkle, std::size(kTwinkle), 112, PhO, PhU},
};
static_assert(std::size(kScores) == kSongCount);
constexpr unsigned kReleaseFrames = kSampleRate * 45 / 1000;
}  // namespace

void SongPlayer::start(Song song) {
    *this = {};
    const auto selection = static_cast<unsigned>(song);
    if (selection < kSongCount) {
        score_ = &kScores[selection];
    }
}

void SongPlayer::stop() {
    score_ = nullptr;
    sounding_ = false;
}

std::size_t SongPlayer::render(Voice& voice, std::int16_t* output, std::size_t frames) {
    std::size_t written = 0;
    while (active() && written < frames) {
        if (elapsed_ == 0) {
            const auto& note = score_->notes[note_];
            eighths_ += note.eighths;
            // Round absolute beat positions so each note's rounding error cannot drift the tempo.
            const auto end = (eighths_ * kSampleRate * 30 + score_->bpm / 2) / score_->bpm;
            noteFrames_ = end - scoreFrames_;
            scoreFrames_ = end;
            releaseFrame_ = noteFrames_ - std::min(kReleaseFrames, noteFrames_ / 4);
            vowel_ = note.eighths >= 3 ? score_->longVowel : score_->vowel;
            voice.setControl(note.midi, vowel_);
            sounding_ = true;
        }
        if (elapsed_ == releaseFrame_) {
            voice.setControl(0, vowel_);
            sounding_ = false;
        }
        const auto next = sounding_ ? releaseFrame_ : noteFrames_;
        const auto available = std::min<std::size_t>(next - elapsed_, frames - written);
        voice.render(output + written, available);
        written += available;
        elapsed_ += available;
        if (elapsed_ == noteFrames_) {
            elapsed_ = 0;
            if (++note_ == score_->count) {
                stop();
            }
        }
    }
    return written;
}

}  // namespace chimera_adv
