#include <Arduino.h>
#include <esp_timer.h>
#include <cmath>
#include <cstdint>

struct Probe;
extern "C" Probe* chimera_probe_new(uint32_t, uint32_t, uint32_t*);
extern "C" void chimera_probe_render(Probe*, float (*)[256]);
extern "C" void chimera_probe_free(Probe*);
extern "C" uint32_t chimera_probe_scalar_bytes();

// The planner and serde tables need more than Arduino's default task stack.
size_t getArduinoLoopTaskStackSize() { return 32768; }

namespace {
constexpr unsigned kRate = 24000;
constexpr unsigned kFrames = 256;
constexpr unsigned kBlocks = 32;
unsigned meanUs;
unsigned worstUs;
unsigned heapAfterInit;
unsigned minHeap;
unsigned stackLow;
unsigned peakMicro;
bool allFinite = true;

void printResult() {
    Serial.printf("count=%u avg_us=%u worst_us=%u budget_us=%u\n",
                  kBlocks, meanUs, worstUs, kFrames * 1000000u / kRate);
    Serial.printf("heap=%u min_heap=%u stack_low=%u\n", heapAfterInit, minHeap, stackLow);
    Serial.printf("finite=%u peak_x1e6=%u\n", static_cast<unsigned>(allFinite), peakMicro);
    Serial.println("CHIMERA probe finished");
}
}

void setup() {
    Serial.begin(115200);
    delay(1200);
    Serial.printf("CHIMERA %u-bit 24k x1 probe at %u MHz\n",
                  8 * chimera_probe_scalar_bytes(), ESP.getCpuFreqMHz());
    uint32_t gestures = 0;
    auto* voice = chimera_probe_new(kRate, 1, &gestures);
    heapAfterInit = ESP.getFreeHeap();
    Serial.printf("Voice ready gestures=%u heap=%u\n", gestures, heapAfterInit);
    float pcm[kFrames];
    float peak = 0.0f;
    uint64_t totalUs = 0;
    for (unsigned block = 0; block < kBlocks; ++block) {
        const auto start = esp_timer_get_time();
        chimera_probe_render(voice, &pcm);
        const auto took = static_cast<unsigned>(esp_timer_get_time() - start);
        totalUs += took;
        worstUs = max(worstUs, took);
        for (float sample : pcm) {
            allFinite &= std::isfinite(sample);
            peak = fmaxf(peak, fabsf(sample));
        }
        if ((block + 1) % 8 == 0) {
            Serial.printf("block=%u avg_us=%u worst_us=%u\n", block + 1,
                          static_cast<unsigned>(totalUs / (block + 1)), worstUs);
        }
        delay(1);
    }
    meanUs = static_cast<unsigned>(totalUs / kBlocks);
    peakMicro = static_cast<unsigned>(peak * 1000000.0f);
    minHeap = ESP.getMinFreeHeap();
    stackLow = uxTaskGetStackHighWaterMark(nullptr);
    chimera_probe_free(voice);
    printResult();
}

void loop() {
    delay(1500);
    printResult();
}
