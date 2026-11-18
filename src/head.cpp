#include "head.hpp"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>

namespace chimera_adv {
namespace {
constexpr uint16_t rgb(unsigned r, unsigned g, unsigned b) {
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
}

constexpr auto kBackground = rgb(9, 20, 26);
constexpr auto kGrid = rgb(18, 35, 44);
constexpr auto kSkin = rgb(26, 47, 61);
constexpr auto kSkinEdge = rgb(94, 137, 159);
constexpr auto kSkull = rgb(47, 74, 88);
constexpr auto kCavity = rgb(9, 24, 34);
constexpr auto kAirway = rgb(90, 199, 226);
constexpr auto kNose = rgb(27, 72, 90);
constexpr auto kTongue = rgb(70, 133, 100);
constexpr auto kTongueEdge = rgb(180, 237, 141);
constexpr auto kLip = rgb(249, 145, 181);
constexpr auto kJaw = rgb(216, 145, 100);
constexpr auto kBone = rgb(219, 227, 213);
constexpr auto kVelumColor = rgb(174, 153, 229);
constexpr auto kText = rgb(168, 194, 207);

constexpr unsigned sumOutlines(unsigned end) {
    unsigned sum = 0;
    for (unsigned i = 0; i < end; ++i) {
        sum += kOutlineLengths[i];
    }
    return sum;
}

constexpr unsigned kSkinUpper = 0;
constexpr unsigned kChin = 1;
constexpr unsigned kTonguePath = 2;
constexpr unsigned kAirwayPath = 3;
constexpr unsigned kLipsUpper = 4;
constexpr unsigned kLipsLower = 5;
constexpr unsigned kTeethUpper = 6;
constexpr unsigned kTeethLower = 7;
constexpr unsigned kSoftPalate = 8;
constexpr unsigned kNasalPath = 9;

struct Path {
    const Point* points;
    unsigned size;
};

Path outline(const Point* points, unsigned part) {
    return {points + sumOutlines(part), kOutlineLengths[part]};
}

void stroke(LovyanGFX& canvas, Path path, uint16_t color) {
    for (unsigned i = 1; i < path.size; ++i) {
        const auto [x0, y0] = path.points[i - 1];
        const auto [x1, y1] = path.points[i];
        canvas.drawLine(x0, y0, x1, y1, color);
    }
}

void fill(LovyanGFX& canvas, Path path, uint16_t color) {
    // The tongue and facial profile are concave. Pair the scanline crossings
    // instead of drawing a triangle fan across the mouth.
    constexpr unsigned kMaxVertices = 48;
    int xCross[kMaxVertices];
    int lo = 135;
    int hi = 0;
    for (unsigned i = 0; i < path.size; ++i) {
        lo = std::min(lo, int(path.points[i].y));
        hi = std::max(hi, int(path.points[i].y));
    }
    for (int y = lo; y < hi; ++y) {
        unsigned n = 0;
        auto a = path.points[path.size - 1];
        for (unsigned i = 0; i < path.size; ++i) {
            const auto b = path.points[i];
            if ((a.y <= y && b.y > y) || (b.y <= y && a.y > y)) {
                xCross[n++] = a.x + (y - a.y) * (b.x - a.x) / (b.y - a.y);
            }
            a = b;
        }
        std::sort(xCross, xCross + n);
        for (unsigned i = 0; i + 1 < n; i += 2) {
            canvas.drawFastHLine(xCross[i], y, xCross[i + 1] - xCross[i] + 1, color);
        }
    }
}

const char* phoneLabel(Phone phone) {
    constexpr const char* labels[] = {
        "A", "E", "I", "O", "U", "", "AH", "AE", "UH", "AX", "AW", "EH",
        "ER", "IH", "OO", "B", "D", "G", "P", "T", "K", "DH", "F", "S",
        "SH", "TH", "V", "Z", "ZH", "H", "L", "L", "M", "N", "NG", "R",
        "W", "Y", "R", "CH", "J", "OW", "AI", "AY", "OH", "OY",
    };
    static_assert(std::size(labels) == PhCount);
    return labels[phone];
}

}  // namespace

bool Head::update(Phone phone, bool sounding) {
    const auto& target = kProfiles[phone].head;
    bool dirty = first_ || phone != phone_ || sounding != sounding_;
    for (unsigned i = 0; i < kPointCount; ++i) {
        const int dx = (static_cast<int>(target[i].x) << 5) - x_[i];
        const int dy = (static_cast<int>(target[i].y) << 5) - y_[i];
        if (dx || dy) {
            dirty = true;
            x_[i] += first_ || std::abs(dx) < 4 ? dx : dx / 2;
            y_[i] += first_ || std::abs(dy) < 4 ? dy : dy / 2;
        }
    }
    first_ = false;
    phone_ = phone;
    sounding_ = sounding;
    return dirty;
}

void Head::draw(LovyanGFX& canvas) const {
    canvas.fillScreen(kBackground);
    canvas.drawFastVLine(0, 0, 135, kGrid);
    for (int x = 8; x <= 98; x += 10) {
        for (int y = 6; y < 118; y += 10) {
            canvas.drawPixel(x, y, kGrid);
        }
    }

    Point points[kPointCount];
    for (unsigned i = 0; i < kPointCount; ++i) {
        points[i] = {static_cast<uint8_t>(10 + (x_[i] * 35 + 512) / 1024),
                     static_cast<uint8_t>(4 + (y_[i] * 30 + 512) / 1024)};
    }
    const auto upper = outline(points, kSkinUpper);
    const auto chin = outline(points, kChin);
    const auto tongue = outline(points, kTonguePath);
    const auto airway = outline(points, kAirwayPath);
    const auto lowerLip = outline(points, kLipsLower);
    const auto nose = outline(points, kNasalPath);
    constexpr auto kSilhouettePoints = kOutlineLengths[kSkinUpper] + kOutlineLengths[kChin];
    static_assert(kSilhouettePoints <= 48);
    Point silhouette[kSilhouettePoints];
    std::copy_n(upper.points, upper.size, silhouette);
    std::reverse_copy(chin.points, chin.points + chin.size, silhouette + upper.size);
    fill(canvas, {silhouette, kSilhouettePoints}, kSkin);

    // The skull and cervical spine sit in the background of the sagittal cut.
    constexpr Point skull[] = {
        {31, 49}, {25, 42}, {22, 31}, {27, 21}, {40, 14}, {55, 12},
        {67, 17}, {75, 24}, {78, 31}, {73, 35}, {62, 36}, {50, 40}, {41, 46}, {31, 49},
    };
    stroke(canvas, {skull, std::size(skull)}, kSkull);
    constexpr Point ridge[] = {{29, 36}, {35, 26}, {48, 20}, {59, 20}, {69, 24}};
    stroke(canvas, {ridge, std::size(ridge)}, kSkull);
    for (int y = 60; y <= 111; y += 8) {
        canvas.drawRoundRect(30, y, 11, 6, 1, kSkull);
    }

    stroke(canvas, upper, kSkinEdge);
    stroke(canvas, chin, kSkinEdge);

    constexpr auto kCavityPoints = kOutlineLengths[kAirwayPath] + kOutlineLengths[kTonguePath] + 2;
    static_assert(kCavityPoints <= 48);
    Point cavity[kCavityPoints];
    std::copy_n(airway.points, airway.size, cavity);
    // The lip's frontmost point is the lower boundary of the mouth opening.
    cavity[airway.size] = *std::max_element(lowerLip.points, lowerLip.points + lowerLip.size,
        [](Point a, Point b) { return a.x < b.x; });
    std::reverse_copy(tongue.points, tongue.points + tongue.size, cavity + airway.size + 1);
    cavity[kCavityPoints - 1] = {static_cast<uint8_t>(airway.points[0].x + 8), airway.points[0].y};
    fill(canvas, {cavity, kCavityPoints}, kCavity);
    fill(canvas, nose, kNose);
    stroke(canvas, nose, kAirway);
    stroke(canvas, airway, kAirway);
    canvas.drawLine(airway.points[0].x + 8, airway.points[0].y,
                    tongue.points[0].x + 2, tongue.points[0].y, kAirway);

    fill(canvas, tongue, kTongue);
    stroke(canvas, tongue, kTongueEdge);
    canvas.drawLine(chin.points[4].x - 3, chin.points[4].y - 2,
                    chin.points[7].x, chin.points[7].y - 4, kJaw);
    stroke(canvas, outline(points, kTeethUpper), kBone);
    stroke(canvas, outline(points, kTeethLower), kBone);
    stroke(canvas, outline(points, kSoftPalate), kVelumColor);
    stroke(canvas, outline(points, kLipsUpper), kLip);
    stroke(canvas, lowerLip, kLip);

    canvas.drawEllipse(82, 36, 4, 3, kSkinEdge);
    canvas.fillCircle(84, 36, 1, kBone);
    const Point glottis = airway.points[0];
    canvas.drawFastHLine(glottis.x, glottis.y, 9, sounding_ ? kTongueEdge : kSkinEdge);
    if (sounding_) {
        canvas.fillCircle(glottis.x + 4, glottis.y, 2, kTongueEdge);
    }

    canvas.drawFastHLine(7, 119, 90, kGrid);
    canvas.setFont(&fonts::Font0);
    canvas.setTextSize(1);
    canvas.setTextColor(kText);
    canvas.setCursor(8, 124);
    canvas.print("VOCAL TRACT");
    const auto* label = phoneLabel(phone_);
    if (*label) {
        canvas.setTextColor(sounding_ ? kTongueEdge : kText);
        canvas.setCursor(96 - 6 * std::strlen(label), 124);
        canvas.print(label);
    }
}

}  // namespace chimera_adv
