#include <Arduino.h>
#include <esp_heap_caps.h>
#include <esp_timer.h>

#include <cmath>
#include <cstdint>

struct Probe;
extern "C" Probe* chimera_probe_new(uint32_t rate, uint32_t oversample, uint32_t* gestures);
extern "C" void chimera_probe_render(Probe* probe, float (*pcm)[256]);
extern "C" void chimera_probe_free(Probe* probe);

// The upstream planner and serde tables overflow the Arduino default 8 KiB task.
size_t getArduinoLoopTaskStackSize() {
    return 32768;
}

namespace {
constexpr unsigned frames = 256;

template <typename... Args>
void logBoth(const char* format, Args... args) {
    Serial0.printf(format, args...);
    Serial.printf(format, args...);
}

void benchmark(unsigned rate, unsigned oversample) {
    const unsigned before = ESP.getFreeHeap();
    const unsigned largest = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    logBoth("Starting %u Hz x%u, free_heap=%u largest=%u\n", rate, oversample, before, largest);
    uint32_t gestures = 0;
    auto* voice = chimera_probe_new(rate, oversample, &gestures);
    const unsigned live = before - ESP.getFreeHeap();
    logBoth("Voice ready, gestures=%lu current_heap=%u\n",
            static_cast<unsigned long>(gestures), ESP.getFreeHeap());
    const unsigned count = (3 * rate + frames - 1) / frames;
    float pcm[frames];
    int64_t sum = 0;
    int64_t worst = 0;
    float peak = 0.0f;
    bool finite = true;
    for (unsigned block = 0; block < count; ++block) {
        const auto start = esp_timer_get_time();
        chimera_probe_render(voice, &pcm);
        const auto took = esp_timer_get_time() - start;
        sum += took;
        if (took > worst) worst = took;
        for (const auto sample : pcm) {
            if (!std::isfinite(sample)) finite = false;
            else peak = fmaxf(peak, fabsf(sample));
        }
        if (block == 7) {
            logBoth("First 2048 samples, peak=%f finite=%d\n", static_cast<double>(peak), finite);
        }
        delay(1);
    }
    chimera_probe_free(voice);
    const auto budget = frames * 1000000u / rate;
    logBoth("%u Hz x%u, gestures=%lu voice_and_tables=%u min_heap=%u stack_low=%u, avg_us=%lld worst_us=%lld budget_us=%u, peak=%f finite=%d\n",
            rate, oversample, static_cast<unsigned long>(gestures), live, ESP.getMinFreeHeap(),
            static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)), sum / count, worst, budget, static_cast<double>(peak), finite);
}
} // namespace

void setup() {
    Serial0.begin(115200);
    Serial.begin(115200);
    delay(1200);
    logBoth("CHIMERA full human voice probe, CPU %u MHz\n", ESP.getCpuFreqMHz());
    benchmark(24000, 1);
    benchmark(24000, 2);
    benchmark(48000, 2);
    logBoth("CHIMERA probe finished\n");
}

void loop() {
    delay(1000);
}
