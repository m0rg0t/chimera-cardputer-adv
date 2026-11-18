#pragma once

#include <string_view>
#include "head.hpp"

namespace chimera_adv {

constexpr int kScreenWidth = 240;
constexpr int kScreenHeight = 135;
constexpr int kScreenSplit = 136;

struct ScreenState {
    std::string_view text;
    const char* notice = nullptr;
    unsigned note = 0;
    unsigned vowel = 0;
    unsigned octave = 3;
    unsigned volume = 56;
    bool say = true;
    bool playing = false;
    bool demo = false;
};

class Screen {
public:
    bool begin();
    void controls(LovyanGFX& display, const ScreenState& state);
    void anatomy(LovyanGFX& display, Phone phone, bool sounding);

private:
    m5gfx::M5Canvas sprite_;
    Head head_;
};

}  // namespace chimera_adv
