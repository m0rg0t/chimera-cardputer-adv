#pragma once

#include <cstdint>
#include "profiles.generated.hpp"

namespace chimera_adv {

class Voice {
public:
    // Midi note zero denotes release. Vowel is in a, e, i, o, u order.
    void setControl(unsigned note, unsigned vowel);
    void articulate(Phone phone, float hz, float breath, float noise,
                    unsigned jet, float noiseFilter);
    void render(std::int16_t* output, std::size_t frames);
    Phone pose() const { return phone_; }

private:
    static constexpr float kInverseRate = 1.0f / kSampleRate;
    static constexpr unsigned kNasalSections = 8;

    float reflection_[kSections - 1]{};
    float transmission_[kSections - 1]{};
    float targetReflection_[kSections - 1]{};
    float targetTransmission_[kSections - 1]{};
    float towardLips_[kSections]{};
    float towardGlottis_[kSections]{};
    float velum_[3]{};
    float toNose_[kNasalSections]{};
    float fromNose_[kNasalSections]{};

    Phone phone_ = PhCount;
    unsigned nasalIndex_ = 0;
    unsigned note_ = 0;
    unsigned jet_ = 0;
    float targetIncrement_ = 130.8128f * kInverseRate;
    float increment_ = targetIncrement_;
    float phase_ = 0.0f;
    float breath_ = 0.0f;
    float targetBreath_ = 0.0f;
    float noise_ = 0.0f;
    float targetNoise_ = 0.0f;
    float noiseFilter_ = 0.0f;
    float noiseSlow_ = 0.0f;
    float noiseFast_ = 0.0f;
    float previousPressure_ = 0.0f;
    float output_ = 0.0f;
    std::uint32_t rng_ = 777;
};

}  // namespace chimera_adv
