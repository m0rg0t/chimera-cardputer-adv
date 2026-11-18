#include "speech.hpp"

#include <algorithm>
#include <cstring>

extern "C" const unsigned char chimera_lexicon[];

namespace chimera_adv {
namespace {
constexpr unsigned kFramesPerMs = kSampleRate / 1000;
constexpr unsigned kLexiconBlock = 16;

struct Rule {
    std::string_view pre, spelling, post;
    Phone phones[3];
    unsigned count;
};

#include "fallback.generated.inc"

bool vowel(Phone phone) {
    return kProfiles[phone].manner == Manner::Vowel;
}

unsigned read32(const unsigned char* p) {
    return p[0] | (unsigned(p[1]) << 8) | (unsigned(p[2]) << 16) |
           (unsigned(p[3]) << 24);
}

const unsigned char* pronunciation(std::string_view word) {
    const unsigned entries = read32(chimera_lexicon);
    const unsigned blocks = (entries + kLexiconBlock - 1) / kLexiconBlock;
    const auto* index = chimera_lexicon + 4;
    const auto* data = index + 4 * blocks;
    const auto block = [=](unsigned i) { return data + read32(index + 4 * i); };
    unsigned lo = 0;
    unsigned hi = blocks;
    while (lo < hi) {
        const unsigned mid = lo + (hi - lo) / 2;
        // A block's first word has no prefix and can be compared directly.
        if (word.compare(reinterpret_cast<const char*>(block(mid) + 1)) < 0) {
            hi = mid;
        } else {
            lo = mid + 1;
        }
    }
    if (!lo) {
        return nullptr;
    }
    const unsigned id = lo - 1;
    const auto* cursor = block(id);
    const unsigned n = std::min(kLexiconBlock, entries - id * kLexiconBlock);
    char candidate[64]{};
    for (unsigned i = 0; i < n; ++i) {
        const unsigned prefix = *cursor++;
        const auto* suffix = reinterpret_cast<const char*>(cursor);
        const auto len = std::strlen(suffix);
        std::memcpy(candidate + prefix, suffix, len + 1);
        cursor += len + 1;
        const int order = word.compare(candidate);
        if (order == 0) {
            return cursor;
        }
        if (order < 0) {
            return nullptr;
        }
        cursor += 1 + *cursor;
    }
    return nullptr;
}

char lower(char c) {
    return c >= 'A' && c <= 'Z' ? static_cast<char>(c + ('a' - 'A')) : c;
}

bool isLetter(char c) {
    return (c >= 'a' && c <= 'z') || c == '\'';
}

bool context(std::string_view word, int pos, std::string_view rule, int direction) {
    for (unsigned i = 0; i < rule.size(); ++i, pos += direction) {
        const char expected = rule[direction > 0 ? i : rule.size() - 1 - i];
        const char c = pos >= 0 && static_cast<unsigned>(pos) < word.size() ? word[pos] : 0;
        const bool isVowel = c && std::strchr("aeiouy", c);
        if (expected == '#' ? c != 0 : expected == 'V' ? !isVowel :
            expected == 'C' ? !c || isVowel : c != expected) {
            return false;
        }
    }
    return true;
}
}  // namespace

void Speech::append(Phone phone, unsigned stress, unsigned ms) {
    if (count_ < std::size(segments_)) {
        segments_[count_++] = {phone, static_cast<uint8_t>(stress),
                              static_cast<uint16_t>(ms * kFramesPerMs)};
    }
}

void Speech::word(std::string_view token) {
    while (!token.empty() && token.front() == '\'') { token.remove_prefix(1); }
    while (!token.empty() && token.back() == '\'') { token.remove_suffix(1); }
    if (token.empty()) {
        return;
    }
    const unsigned start = count_;
    const auto add = [this](Phone phone, unsigned stress) {
        const auto& profile = kProfiles[phone];
        const unsigned ms = vowel(phone) ? (stress ? profile.ms : profile.ms * 68 / 100) :
                           profile.ms * (profile.manner == Manner::Stop ? 76 :
                                         profile.manner == Manner::Fricative ? 77 : 93) / 100;
        append(phone, stress, ms);
    };

    if (const auto* packed = pronunciation(token)) {
        for (unsigned i = 1; i <= packed[0]; ++i) {
            add(static_cast<Phone>(packed[i] & 63), packed[i] >> 6);
        }
    } else {
        bool stressed = false;
        for (unsigned i = 0; i < token.size();) {
            unsigned consumed = 1;
            for (const auto& rule : kRules) {
                if (token.substr(i, rule.spelling.size()) != rule.spelling ||
                    !context(token, static_cast<int>(i) - 1, rule.pre, -1) ||
                    !context(token, i + rule.spelling.size(), rule.post, 1)) {
                    continue;
                }
                for (unsigned j = 0; j < rule.count; ++j) {
                    const auto phone = rule.phones[j];
                    add(phone, vowel(phone) && !stressed ? 1 : 0);
                    stressed |= vowel(phone);
                }
                consumed = rule.spelling.size();
                break;
            }
            i += consumed;
        }
    }
    for (unsigned i = start; i < count_; ++i) {
        auto& segment = segments_[i];
        const bool atEnd = i + 1 == count_;
        if (segment.phone == PhL && (atEnd || !vowel(segments_[i + 1].phone))) {
            segment.phone = PhDarkL;
        }
        if ((segment.phone == PhT || segment.phone == PhD) && !atEnd && i > start &&
            vowel(segments_[i - 1].phone) && vowel(segments_[i + 1].phone) &&
            !segments_[i + 1].stress) {
            segment.phone = PhTap;
            segment.frames = kProfiles[PhTap].ms * kFramesPerMs;
        }
    }
}

bool Speech::start(std::string_view text) {
    cancel();
    unsigned letters = 0;
    char token[kTextLimit + 1];
    const auto emit = [&]() {
        if (!letters) {
            return;
        }
        const auto previous = count_;
        if (previous && segments_[previous - 1].phone != PhIdle) {
            append(PhIdle, 0, 28);
        }
        word(std::string_view(token, letters));
        if (count_ == previous + 1 && segments_[previous].phone == PhIdle) {
            count_ = previous;
        }
        letters = 0;
    };
    constexpr const char* digits[] = {"zero", "one", "two", "three", "four",
                                     "five", "six", "seven", "eight", "nine"};
    for (const char input : text.substr(0, kTextLimit)) {
        const char c = lower(input);
        if (isLetter(c)) {
            token[letters++] = c;
        } else {
            emit();
            if (c >= '0' && c <= '9') {
                word(digits[c - '0']);
                append(PhIdle, 0, 25);
            } else if (count_ && std::strchr(".,!?;:", c) && c) {
                if (segments_[count_ - 1].phone != PhIdle) {
                    append(PhIdle, 0, c == ',' ? 115 : 175);
                }
                question_ = c == '?';
            }
        }
    }
    emit();
    if (count_ && segments_[count_ - 1].phone != PhIdle) {
        append(PhIdle, 0, 140);
    }
    return active();
}

void Speech::cancel() {
    count_ = current_ = elapsed_ = 0;
    question_ = false;
}

Phone Speech::nextVowel() const {
    for (unsigned i = current_ + 1; i < count_; ++i) {
        const auto phone = segments_[i].phone;
        if (vowel(phone)) {
            return phone;
        }
        if (phone == PhIdle) {
            break;
        }
    }
    return PhAx;
}

void Speech::shape(Voice& voice) const {
    const auto& segment = segments_[current_];
    const auto& profile = kProfiles[segment.phone];
    const float t = static_cast<float>(elapsed_) / segment.frames;
    const float position = static_cast<float>(current_) / count_;
    const float contour = question_ ? 28.0f * position : -26.0f * position;
    const float hz = 128.0f + contour + 19.0f * segment.stress * (1.0f - t);
    const float buzz = profile.voiced ? (vowel(segment.phone) ? 1.05f + 0.20f * segment.stress :
                                         0.57f) : 0.0f;
    Phone pose = segment.phone;
    float drive = buzz;
    float jetGain = 0.0f;
    unsigned jet = profile.jet;
    float bandwidth = 0.65f;
    switch (profile.manner) {
        case Manner::Stop:
            if (t >= 0.74f) {
                pose = nextVowel();
                jetGain = segment.phone == PhTap ? 0.004f : 0.30f * (1.0f - t);
                drive = buzz * 0.68f;
            } else {
                drive *= 0.34f;
            }
            break;
        case Manner::Affricate:
            if (t >= 0.37f) {
                pose = profile.voiced ? PhZh : PhSh;
                jetGain = 0.060f;
                bandwidth = 0.55f;
                jet = kProfiles[pose].jet;
            } else {
                drive *= 0.25f;
            }
            break;
        case Manner::Fricative:
            if (segment.phone == PhS || segment.phone == PhZ) {
                jetGain = 0.060f;
            } else if (segment.phone == PhSh || segment.phone == PhZh) {
                jetGain = 0.055f;
                bandwidth = 0.54f;
            } else {
                jetGain = 0.038f;
                bandwidth = 0.5f;
            }
            break;
        case Manner::Breath:
            pose = nextVowel();
            jetGain = 0.042f;
            bandwidth = 0.6f;
            break;
        case Manner::Vowel:
            if (t > 0.45f) {
                switch (segment.phone) {
                    case PhAw: pose = PhUh; break;
                    case PhAy:
                    case PhEy:
                    case PhOy: pose = PhIx; break;
                    case PhOw: pose = PhU; break;
                    default: break;
                }
            }
            break;
        case Manner::Pause:
            if (segment.frames < 35 * kFramesPerMs && current_ > 0) {
                pose = segments_[current_ - 1].phone;
            }
            break;
        case Manner::Nasal:
        case Manner::Glide:
            break;
    }
    voice.articulate(pose, hz, drive, jetGain, jet, bandwidth);
}

std::size_t Speech::render(Voice& voice, std::int16_t* output, std::size_t frames) {
    std::size_t written = 0;
    while (active() && written < frames) {
        // Update control at deterministic times even when the host changes buffer sizes.
        constexpr unsigned kControlFrames = 96;
        if (elapsed_ % kControlFrames == 0) {
            shape(voice);
        }
        const unsigned step = std::min(segments_[current_].frames - elapsed_,
                                       kControlFrames - elapsed_ % kControlFrames);
        const auto n = std::min<std::size_t>(step, frames - written);
        voice.render(output + written, n);
        written += n;
        elapsed_ += n;
        if (elapsed_ >= segments_[current_].frames) {
            ++current_;
            elapsed_ = 0;
        }
    }
    return written;
}

}  // namespace chimera_adv
