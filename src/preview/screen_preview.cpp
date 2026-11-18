#include "../screen.hpp"

#include <array>
#include <cstdio>
#include <string>

namespace {

bool writePpm(LovyanGFX& display, const std::string& path) {
    using namespace chimera_adv;
    std::array<lgfx::rgb888_t, kScreenWidth * kScreenHeight> pixels;
    display.readRect(0, 0, kScreenWidth, kScreenHeight, pixels.data());
    FILE* file = std::fopen(path.c_str(), "wb");
    if (!file) {
        return false;
    }
    std::fprintf(file, "P6\n%d %d\n255\n", kScreenWidth, kScreenHeight);
    for (const auto pixel : pixels) {
        const unsigned char color[] = {pixel.r, pixel.g, pixel.b};
        std::fwrite(color, 1, 3, file);
    }
    return std::fclose(file) == 0;
}
}  // namespace

int main(int argc, char** argv) {
    using namespace chimera_adv;
    if (argc != 2) {
        std::fputs("usage: program OUTPUT_DIR\n", stderr);
        return 2;
    }
    m5gfx::M5Canvas display;
    display.setColorDepth(16);
    Screen screen;
    if (!display.createSprite(kScreenWidth, kScreenHeight) || !screen.begin()) {
        return 1;
    }

    struct Frame {
        const char* name;
        ScreenState state;
        Phone pose;
        bool live;
    };
    const Frame frames[] = {
        {"say-ready", {}, PhIdle, false},
        {"say-speaking", {"The sun is shining today.", nullptr, 0, 0, 3, 56, true, true}, PhAy, true},
        {"say-long", {"Welcome aboard. Type a phrase and listen to what the vocal tract does."}, PhIdle, false},
        {"say-limit", {"012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345"}, PhIdle, false},
        {"piano", {{}, nullptr, 55, 2, 3, 56, false}, PhI, true},
    };
    for (const auto& frame : frames) {
        screen.controls(display, frame.state);
        for (int tick = 0; tick < 12; ++tick) {
            screen.anatomy(display, frame.pose, frame.live);
        }
        if (!writePpm(display, std::string(argv[1]) + "/" + frame.name + ".ppm")) {
            return 1;
        }
    }
    return 0;
}
