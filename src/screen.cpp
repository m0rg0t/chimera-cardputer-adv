#include "screen.hpp"

#include <array>
#include <cstdio>
#include <cstring>

#include "speech.hpp"

namespace chimera_adv {
namespace {
constexpr uint16_t rgb(unsigned r, unsigned g, unsigned b) {
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
}

constexpr auto kPanel = rgb(11, 23, 30);
constexpr auto kSurface = rgb(20, 38, 48);
constexpr auto kBorder = rgb(47, 74, 89);
constexpr auto kAccent = rgb(187, 242, 150);
constexpr auto kText = rgb(230, 239, 237);
constexpr auto kMuted = rgb(150, 181, 194);

void printAt(LovyanGFX& canvas, int x, int y, const char* value, uint16_t color) {
    canvas.setTextColor(color);
    canvas.setCursor(x, y);
    canvas.print(value);
}

void rightAt(LovyanGFX& canvas, int x, int y, const char* value, uint16_t color) {
    printAt(canvas, x - 6 * static_cast<int>(std::strlen(value)), y, value, color);
}

void drawEditor(LovyanGFX& canvas, std::string_view text) {
    constexpr unsigned kColumns = 18;
    constexpr unsigned kRows = 4;
    struct TextRow { unsigned start, size; };
    std::array<TextRow, kTextLimit + 1> rows{};
    unsigned count = 0;
    for (unsigned start = 0; start < text.size();) {
        const auto remaining = text.substr(start, kColumns);
        auto width = remaining.size();
        auto advance = width;
        // Wrap when a cut would split a word and there's space earlier on the line.
        if (start + width < text.size() && text[start + width] != ' ' && remaining.back() != ' ') {
            const auto space = remaining.rfind(' ');
            if (space != std::string_view::npos && space > 0) {
                width = space;
                advance = space + 1;
            }
        }
        rows[count++] = {start, static_cast<unsigned>(width)};
        start += advance;
    }
    if (!count || rows[count - 1].size == kColumns) {
        rows[count++] = {static_cast<unsigned>(text.size()), 0};
    }
    const unsigned first = count > kRows ? count - kRows : 0;
    canvas.fillRoundRect(7, 43, 122, 49, 5, kSurface);
    canvas.drawRoundRect(7, 43, 122, 49, 5, kBorder);
    canvas.setTextColor(kText);
    for (unsigned i = first; i < count; ++i) {
        const auto row = rows[i];
        canvas.setCursor(13, 49 + 10 * (i - first));
        for (unsigned j = 0; j < row.size; ++j) {
            canvas.print(text[row.start + j]);
        }
    }
    if (text.empty()) {
        printAt(canvas, 20, 49, "Type English text", kMuted);
    }
    const auto cursorX = 13 + 6 * rows[count - 1].size;
    const auto cursorY = 49 + 10 * (count - 1 - first);
    canvas.drawFastVLine(cursorX, cursorY - 1, 8, kAccent);
    if (first) {
        canvas.fillTriangle(125, 48, 123, 52, 127, 52, kMuted);
    }
}

void drawSay(LovyanGFX& canvas, const ScreenState& state) {
    printAt(canvas, 8, 29, "SAY", kAccent);
    char counter[12];
    std::snprintf(counter, sizeof counter, "%u/%u", static_cast<unsigned>(state.text.size()), kTextLimit);
    rightAt(canvas, 128, 29, counter, kMuted);
    drawEditor(canvas, state.text);
    if (state.notice) {
        printAt(canvas, 8, 101, state.notice, kAccent);
    } else if (state.playing) {
        canvas.fillCircle(11, 104, 2, kAccent);
        printAt(canvas, 19, 101, "Speaking", kText);
        rightAt(canvas, 128, 101, "Fn+` Stop", kMuted);
    } else {
        canvas.fillRoundRect(8, 97, 86, 18, 4, kAccent);
        printAt(canvas, 14, 102, "Enter Speak", kPanel);
    }
    printAt(canvas, 8, 123, "Tab Piano", kMuted);
    char volume[16];
    std::snprintf(volume, sizeof volume, "Fn-/= %u%%", state.volume);
    rightAt(canvas, 128, 123, volume, kMuted);
}

void drawPiano(LovyanGFX& canvas, const ScreenState& state) {
    printAt(canvas, 8, 29, "PIANO", kAccent);
    char details[16];
    std::snprintf(details, sizeof details, "Z/X Oct%u %u%%", state.octave, state.volume);
    rightAt(canvas, 128, 29, details, kMuted);

    constexpr char kVowels[] = "AEIOU";
    for (unsigned i = 0; i < kVowelCount; ++i) {
        const bool chosen = i == state.vowel;
        const int x = 8 + 24 * i;
        canvas.fillRoundRect(x, 43, 20, 23, 4, chosen ? kAccent : kSurface);
        if (!chosen) {
            canvas.drawRoundRect(x, 43, 20, 23, 4, kBorder);
        }
        const char vowel[] = {kVowels[i], 0};
        const char key[] = {static_cast<char>('1' + i), 0};
        printAt(canvas, x + 7, 46, vowel, chosen ? kPanel : kText);
        printAt(canvas, x + 7, 56, key, chosen ? kPanel : kMuted);
    }

    const char* status = state.demo ? "Demo" : "Ready";
    char note[12];
    if (state.note) {
        constexpr const char* names[] = {"C", "C#", "D", "D#", "E", "F",
                                          "F#", "G", "G#", "A", "A#", "B"};
        std::snprintf(note, sizeof note, "%s%u", names[state.note % 12], state.note / 12 - 1);
        status = note;
        canvas.fillCircle(11, 78, 2, kAccent);
    }
    printAt(canvas, state.note ? 18 : 8, 75, status, kText);
    rightAt(canvas, 128, 75, state.demo ? "Enter Stop" : "Enter Demo", kMuted);
    constexpr char kNaturals[] = "asdfghjk";
    constexpr int kNaturalSemitones[] = {0, 2, 4, 5, 7, 9, 11, 12};
    constexpr char kSharps[] = "wetyu";
    constexpr int kSharpAfter[] = {0, 1, 3, 4, 5};
    const int base = 12 * (state.octave + 1);
    const int semitone = static_cast<int>(state.note) - base;
    for (unsigned i = 0; i < std::size(kNaturalSemitones); ++i) {
        const auto x = 8 + 15 * i;
        const bool down = semitone == kNaturalSemitones[i];
        canvas.fillRoundRect(x, 89, 14, 27, 2, down ? kAccent : kBorder);
        char key[] = {kNaturals[i], 0};
        printAt(canvas, x + 4, 106, key, down ? kPanel : kText);
    }
    for (unsigned i = 0; i < std::size(kSharpAfter); ++i) {
        const auto x = 18 + 15 * kSharpAfter[i];
        const bool down = semitone == kNaturalSemitones[kSharpAfter[i]] + 1;
        canvas.fillRoundRect(x, 89, 9, 15, 2, down ? kAccent : kPanel);
        char key[] = {kSharps[i], 0};
        printAt(canvas, x + 2, 91, key, down ? kPanel : kMuted);
    }
    printAt(canvas, 8, 123, "Tab Say", kMuted);
    rightAt(canvas, 128, 123, "-/= Vol", kMuted);
}
}  // namespace

bool Screen::begin() {
    sprite_.setColorDepth(16);
    return sprite_.createSprite(kScreenSplit, kScreenHeight) != nullptr;
}

void Screen::controls(LovyanGFX& display, const ScreenState& state) {
    sprite_.fillScreen(kPanel);
    sprite_.fillRoundRect(8, 8, 3, 14, 1, kAccent);
    sprite_.setFont(&fonts::Font0);
    sprite_.setTextSize(2);
    printAt(sprite_, 16, 8, "CHIMERA", kText);
    sprite_.setTextSize(1);
    sprite_.drawFastHLine(8, 24, 120, kBorder);
    if (state.say) {
        drawSay(sprite_, state);
    } else {
        drawPiano(sprite_, state);
    }
    sprite_.pushSprite(&display, 0, 0);
}

void Screen::anatomy(LovyanGFX& display, Phone phone, bool sounding) {
    if (head_.update(phone, sounding)) {
        // The display clips the shared sprite at the right edge.
        head_.draw(sprite_);
        sprite_.pushSprite(&display, kScreenSplit, 0);
    }
}

}  // namespace chimera_adv
