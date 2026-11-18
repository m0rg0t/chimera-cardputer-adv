#pragma once

#include <string_view>
#include "voice.hpp"

namespace chimera_adv {

constexpr unsigned kTextLimit = 96;

struct SpeechLine {
    char text[kTextLimit + 1]{};
};

class Speech {
public:
    // A copy of the phonetic plan lives on the producer task.
    bool start(std::string_view text);
    void cancel();
    bool active() const { return current_ < count_; }
    std::size_t render(Voice& voice, std::int16_t* output, std::size_t frames);

private:
    struct Segment {
        Phone phone;
        uint8_t stress;
        uint16_t frames;
    };

    void append(Phone phone, unsigned stress, unsigned ms);
    void word(std::string_view word);
    void shape(Voice& voice) const;
    Phone nextVowel() const;

    // At most one ASCII character expands to two phones, except digits (up to
    // five phones plus a short gap). Leave room for the ending pause.
    Segment segments_[6 * kTextLimit + 1]{};
    unsigned count_ = 0;
    unsigned current_ = 0;
    unsigned elapsed_ = 0;
    bool question_ = false;
};

}  // namespace chimera_adv
