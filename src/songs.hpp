#pragma once

#include "voice.hpp"

namespace chimera_adv {

enum class Song : std::uint8_t { OdeToJoy, FrereJacques, Twinkle, Count };
constexpr unsigned kSongCount = static_cast<unsigned>(Song::Count);
constexpr const char* kSongTitles[kSongCount] = {
    "Ode to Joy", "Frere Jacques", "Twinkle, Twinkle",
};

struct SongScore;

class SongPlayer {
public:
    void start(Song song);
    void stop();
    bool active() const { return score_ != nullptr; }
    Phone pose() const { return sounding_ ? static_cast<Phone>(vowel_) : PhIdle; }
    std::size_t render(Voice& voice, std::int16_t* output, std::size_t frames);

private:
    const SongScore* score_ = nullptr;
    unsigned note_ = 0;
    unsigned vowel_ = 0;
    unsigned eighths_ = 0;
    unsigned scoreFrames_ = 0;
    unsigned elapsed_ = 0;
    unsigned noteFrames_ = 0;
    unsigned releaseFrame_ = 0;
    bool sounding_ = false;
};

}  // namespace chimera_adv
