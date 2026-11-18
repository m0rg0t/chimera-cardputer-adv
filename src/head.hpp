#pragma once

#include <M5GFX.h>
#include "profiles.generated.hpp"

namespace chimera_adv {

class Head {
public:
    bool update(Phone phone, bool sounding);
    void draw(LovyanGFX& canvas) const;

private:
    int16_t x_[kPointCount]{};
    int16_t y_[kPointCount]{};
    Phone phone_ = PhIdle;
    bool sounding_ = false;
    bool first_ = true;
};

}  // namespace chimera_adv
