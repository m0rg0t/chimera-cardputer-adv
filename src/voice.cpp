#include "voice.hpp"

#include <algorithm>
#include <cmath>

namespace chimera_adv {

void Voice::setControl(unsigned note, unsigned vowel) {
    const auto phone = static_cast<Phone>(std::min<unsigned>(vowel, kVowelCount - 1));
    note = std::min(note, 127u);
    if (note == note_ && phone == phone_ && targetNoise_ == 0.0f &&
        targetBreath_ == (note == 0 ? 0.0f : 1.0f)) {
        return;
    }
    const float hz = note ? 440.0f * std::exp2((static_cast<float>(note) - 69.0f) / 12.0f)
                          : targetIncrement_ * kSampleRate;
    articulate(phone, hz, note ? 1.0f : 0.0f, 0.0f, 0, 0.5f);
    note_ = note;
}

void Voice::articulate(Phone phone, float hz, float breath, float noise,
                       unsigned jet, float noiseFilter) {
    const auto& profile = kProfiles[phone];
    if (phone != phone_) {
        for (std::size_t i = 0; i < kSections - 1; ++i) {
            const float r = profile.reflection[i];
            targetReflection_[i] = r;
            targetTransmission_[i] = std::sqrt(1.0f - r * r);
        }
        if (phone_ == PhCount || breath_ + noise_ < 0.0001f) {
            std::copy_n(targetReflection_, kSections - 1, reflection_);
            std::copy_n(targetTransmission_, kSections - 1, transmission_);
            std::copy_n(profile.velum, 3, velum_);
        }
        phone_ = phone;
    }
    targetIncrement_ = hz * kInverseRate;
    if (breath_ < 0.001f) {
        increment_ = targetIncrement_;
    }
    targetBreath_ = breath;
    targetNoise_ = noise;
    jet_ = jet;
    noiseFilter_ = noiseFilter;
    note_ = 0;
}

void Voice::render(std::int16_t* output, std::size_t frames) {
    if (phone_ == PhCount) {
        setControl(0, 0);
    }
    if (targetBreath_ == 0.0f && targetNoise_ == 0.0f &&
        breath_ == 0.0f && noise_ == 0.0f) {
        std::fill_n(output, frames, 0);
        return;
    }
    const auto& profile = kProfiles[phone_];
    for (std::size_t frame = 0; frame < frames; ++frame) {
        breath_ += 0.005f * (targetBreath_ - breath_);
        noise_ += 0.012f * (targetNoise_ - noise_);
        if (targetBreath_ == 0.0f && targetNoise_ == 0.0f && breath_ + noise_ < 1e-6f) {
            breath_ = noise_ = 0.0f;
            std::fill_n(towardLips_, kSections, 0.0f);
            std::fill_n(towardGlottis_, kSections, 0.0f);
            std::fill_n(toNose_, kNasalSections, 0.0f);
            std::fill_n(fromNose_, kNasalSections, 0.0f);
            previousPressure_ = output_ = 0.0f;
            std::fill(output + frame, output + frames, 0);
            break;
        }

        increment_ += 0.004f * (targetIncrement_ - increment_);
        phase_ += increment_;
        if (phase_ >= 1.0f) {
            phase_ -= 1.0f;
        }
        const float position = phase_ * kPulseSteps;
        const auto index = static_cast<unsigned>(position);
        const float pulse = kGlottalPulse[index] +
            (position - index) * (kGlottalPulse[index + 1] - kGlottalPulse[index]);

        float turbulence = 0.0f;
        if (noise_ > 1e-6f) {
            rng_ ^= rng_ << 13;
            rng_ ^= rng_ >> 17;
            rng_ ^= rng_ << 5;
            const float white = static_cast<float>(rng_ >> 8) * (1.0f / 8388608.0f) - 1.0f;
            noiseSlow_ += 0.15f * (white - noiseSlow_);
            noiseFast_ += noiseFilter_ * (white - noiseFast_);
            turbulence = noise_ * (noiseFast_ - noiseSlow_);
        }

        const float incomingGlottis = towardGlottis_[0];
        const float incomingLips = towardLips_[kSections - 1];
        const float incomingNose = toNose_[nasalIndex_];
        constexpr float propagation = 0.993f;
        float forward = towardLips_[0];
        for (std::size_t i = 0; i < kSections - 1; ++i) {
            const float nextForward = towardLips_[i + 1];
            const float backward = towardGlottis_[i + 1];
            if (i + 1 == kVelum) {
                for (unsigned j = 0; j < 3; ++j) {
                    velum_[j] += 0.005f * (profile.velum[j] - velum_[j]);
                }
                const float backNose = fromNose_[nasalIndex_];
                // Three-way power-normalized scattering at the velopharyngeal junction.
                const float pressure = 2.0f * (velum_[0] * forward + velum_[1] * backward +
                                                 velum_[2] * backNose);
                towardGlottis_[i] = propagation * (velum_[0] * pressure - forward);
                towardLips_[i + 1] = propagation * (velum_[1] * pressure - backward);
                toNose_[nasalIndex_] = 0.87f * (velum_[2] * pressure - backNose);
            } else {
                reflection_[i] += 0.005f * (targetReflection_[i] - reflection_[i]);
                transmission_[i] += 0.005f * (targetTransmission_[i] - transmission_[i]);
                const float r = reflection_[i];
                const float t = transmission_[i];
                towardLips_[i + 1] = propagation * (t * forward - r * backward);
                towardGlottis_[i] = propagation * (r * forward + t * backward);
            }
            forward = nextForward;
        }
        towardLips_[0] = 0.82f * incomingGlottis + breath_ * pulse;
        if (jet_ < kSections) {
            towardLips_[jet_] += turbulence;
        }
        towardGlottis_[kSections - 1] = -0.86f * incomingLips;
        fromNose_[nasalIndex_] = -0.75f * incomingNose;
        nasalIndex_ = (nasalIndex_ + 1) % kNasalSections;

        // Both open ends radiate the derivative of pressure. The direct noise term
        // represents jets at the teeth and lip opening.
        const float pressure = incomingLips + 0.88f * incomingNose +
                               (jet_ == kSections ? turbulence : 0.0f);
        output_ += 0.52f * ((pressure - previousPressure_) - output_);
        previousPressure_ = pressure;
        const float pcm = std::clamp(73000.0f * output_, -30000.0f, 30000.0f);
        output[frame] = static_cast<std::int16_t>(pcm);
    }
}

}  // namespace chimera_adv
