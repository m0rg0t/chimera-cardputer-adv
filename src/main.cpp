#include <Arduino.h>
#include <M5Cardputer.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstring>

#include "screen.hpp"
#include "speech.hpp"

namespace {
using namespace chimera_adv;
constexpr unsigned kFrames = 256;
constexpr unsigned kBlockUs = kFrames * 1000000 / kSampleRate;
constexpr char kPiano[] = "awsedftgyhujk";
constexpr uint16_t kSlate = 0x1908;

struct SpeechRequest {
    SpeechLine line;
    bool cancel;
};

std::atomic<std::uint32_t> pianoControl{0};
std::atomic<unsigned> activePose{PhIdle};
std::atomic<bool> speaking{false};
std::atomic<unsigned> worstRenderUs{0};
std::atomic<unsigned> queueEmpty{0};
std::atomic<unsigned> enqueueFailures{0};
QueueHandle_t requests = nullptr;
Screen screen;
SpeechLine line;
std::size_t length = 0;
bool ready = false;
bool sayMode = true;
bool demo = false;
bool wasTab = false;
bool wasEnter = false;
bool wasBackspace = false;
bool wasEscape = false;
bool wasLower = false;
bool wasHigher = false;
bool wasDown = false;
bool wasUp = false;
bool dirty = true;
unsigned selectedVowel = 0;
unsigned octave = 3;
unsigned volume = 142;
unsigned demoStart = 0;
unsigned shownNote = 0;
unsigned shownVowel = 0;
unsigned lastHead = 0;
std::array<bool, 95> heldText{};
const char* notice = nullptr;

struct DemoNote {
    unsigned midi;
    unsigned vowel;
};
constexpr DemoNote kMelody[] = {
    {48, 0}, {52, 1}, {55, 2}, {0, 2}, {57, 3}, {55, 4},
    {52, 1}, {0, 1}, {50, 3}, {55, 0}, {48, 0}, {0, 0},
};
constexpr unsigned kDemoStepMs = 270;

void audioTask(void*) {
    Voice voice;
    Speech speech;
    static std::int16_t pcm[3][kFrames];
    unsigned slot = 0;
    bool pending = false;
    bool started = false;
    for (;;) {
        const auto queued = M5Cardputer.Speaker.isPlaying(0);
        if (queued >= 2) {
            vTaskDelay(1);
            continue;
        }
        if (!pending) {
            SpeechRequest request;
            if (xQueueReceive(requests, &request, 0) == pdTRUE) {
                if (request.cancel) {
                    speech.cancel();
                } else {
                    speech.start(request.line.text);
                }
            }

            const auto before = micros();
            const auto speechFrames = speech.render(voice, pcm[slot], kFrames);
            if (speechFrames < kFrames) {
                const auto control = pianoControl.load(std::memory_order_relaxed);
                if (control) {
                    voice.setControl(control & 0xFFu, (control >> 8u) - 1);
                } else {
                    voice.articulate(PhIdle, 126.0f, 0.0f, 0.0f, 0, 0.5f);
                }
                voice.render(pcm[slot] + speechFrames, kFrames - speechFrames);
            }

            activePose.store(voice.pose(), std::memory_order_relaxed);
            speaking.store(speech.active(), std::memory_order_relaxed);
            const auto duration = static_cast<unsigned>(micros() - before);
            if (duration > worstRenderUs.load(std::memory_order_relaxed)) {
                worstRenderUs.store(duration, std::memory_order_relaxed);
            }
            if (started && queued == 0) {
                queueEmpty.fetch_add(1, std::memory_order_relaxed);
            }
            pending = true;
        }
        // M5Unified keeps at most two caller-owned buffers alive.
        if (M5Cardputer.Speaker.playRaw(pcm[slot], kFrames, kSampleRate, false, 1, 0)) {
            slot = (slot + 1) % 3;
            pending = false;
            started = true;
        } else {
            enqueueFailures.fetch_add(1, std::memory_order_relaxed);
            vTaskDelay(1);
        }
    }
}

bool edge(bool held, bool& previous) {
    const bool changed = held && !previous;
    previous = held;
    return changed;
}

void cancel() {
    const SpeechRequest request{{}, true};
    xQueueOverwrite(requests, &request);
    notice = nullptr;
    dirty = true;
}

void edit(const Keyboard_Class::KeysState& keys, bool enter, bool backspace, bool escape) {
    std::array<bool, 95> down{};
    if (!keys.ctrl && !keys.fn && !keys.alt) {
        for (const auto c : keys.word) {
            if (c < 32 || c > 126 || down[c - 32]) {
                continue;
            }
            down[c - 32] = true;
            if (!heldText[c - 32] && length < kTextLimit) {
                line.text[length++] = c;
                line.text[length] = '\0';
                notice = nullptr;
                dirty = true;
            }
        }
    }
    heldText = down;
    if (backspace && length) {
        line.text[--length] = '\0';
        notice = nullptr;
        dirty = true;
    }
    if (escape) {
        cancel();
    }
    if (enter) {
        if (std::strpbrk(line.text, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789")) {
            const SpeechRequest request{line, false};
            xQueueOverwrite(requests, &request);
            notice = nullptr;
        } else {
            notice = "Type words first";
        }
        dirty = true;
    }
}

void piano(bool enter, unsigned now) {
    auto& keyboard = M5Cardputer.Keyboard;
    for (unsigned i = 0; i < kVowelCount; ++i) {
        if (keyboard.isKeyPressed(static_cast<char>('1' + i))) {
            selectedVowel = i;
        }
    }
    if (edge(keyboard.isKeyPressed('z'), wasLower) && octave > 2) {
        --octave;
        dirty = true;
    }
    if (edge(keyboard.isKeyPressed('x'), wasHigher) && octave < 5) {
        ++octave;
        dirty = true;
    }
    if (edge(keyboard.isKeyPressed('-'), wasDown)) {
        volume = volume < 16 ? 0 : volume - 16;
        M5Cardputer.Speaker.setVolume(volume);
        dirty = true;
    }
    if (edge(keyboard.isKeyPressed('='), wasUp)) {
        volume = std::min(255u, volume + 16);
        M5Cardputer.Speaker.setVolume(volume);
        dirty = true;
    }
    if (enter) {
        demo = !demo;
        demoStart = now;
        dirty = true;
    }
    unsigned note = 0;
    unsigned vowel = selectedVowel;
    if (demo) {
        const unsigned step = (now - demoStart) / kDemoStepMs;
        const auto item = kMelody[step % std::size(kMelody)];
        note = (now - demoStart) % kDemoStepMs < 227 ? item.midi : 0;
        vowel = item.vowel;
    } else {
        for (unsigned i = 0; kPiano[i]; ++i) {
            if (keyboard.isKeyPressed(kPiano[i])) {
                note = 12 * (octave + 1) + i;
            }
        }
    }
    pianoControl.store(((vowel + 1) << 8u) | note, std::memory_order_relaxed);
    if (shownNote != note || shownVowel != vowel) {
        shownNote = note;
        shownVowel = vowel;
        dirty = true;
    }
}

void fail(const char* message) {
    M5Cardputer.Speaker.end();
    M5Cardputer.Display.fillScreen(kSlate);
    M5Cardputer.Display.setTextColor(TFT_WHITE);
    M5Cardputer.Display.setCursor(8, 18);
    M5Cardputer.Display.println(message);
    Serial.println(message);
}
}  // namespace

void setup() {
    Serial.begin(115200);
    auto config = M5.config();
    config.internal_mic = false;
    config.internal_spk = true;
    M5Cardputer.begin(config, true);
    M5Cardputer.Display.setRotation(1);
    M5Cardputer.Display.setBrightness(144);
    M5Cardputer.Display.fillScreen(kSlate);
    if (M5.getBoard() != m5::board_t::board_M5CardputerADV) {
        fail("Cardputer-Adv required");
        return;
    }
    requests = xQueueCreate(1, sizeof(SpeechRequest));
    if (!requests || !screen.begin()) {
        fail("UI allocation failed");
        return;
    }
    M5Cardputer.Speaker.end();
    auto speaker = M5Cardputer.Speaker.config();
    speaker.sample_rate = kSampleRate;
    speaker.stereo = false;
    speaker.magnification = 8;
    speaker.dma_buf_len = 128;
    speaker.dma_buf_count = 4;
    speaker.task_priority = 4;
    speaker.task_pinned_core = 1;
    M5Cardputer.Speaker.config(speaker);
    M5Cardputer.Speaker.setVolume(volume);
    if (!M5Cardputer.Speaker.begin()) {
        fail("Speaker initialization failed");
        return;
    }
    if (xTaskCreatePinnedToCore(audioTask, "chimera-audio", 8192, nullptr, 3, nullptr, 1) != pdPASS) {
        fail("Audio task allocation failed");
        return;
    }
    Serial.printf("CHIMERA ready, %u Hz, %u us/block\n", kSampleRate, kBlockUs);
    ready = true;
}

void loop() {
    if (!ready) {
        delay(100);
        return;
    }
    M5Cardputer.update();
    const auto now = millis();
    const auto& keys = M5Cardputer.Keyboard.keysState();
    const bool enter = edge(keys.enter, wasEnter);
    const bool backspace = edge(keys.backspace, wasBackspace);
    const bool escape = edge(keys.esc, wasEscape);
    if (edge(keys.tab, wasTab)) {
        sayMode = !sayMode;
        demo = false;
        heldText.fill(false);
        pianoControl.store(0, std::memory_order_relaxed);
        cancel();
    } else if (sayMode) {
        edit(keys, enter, backspace, escape);
    } else {
        piano(enter, now);
    }

    const bool saying = speaking.load(std::memory_order_relaxed);
    static bool showedSpeech = false;
    if (dirty || saying != showedSpeech) {
        screen.controls(M5Cardputer.Display,
            {std::string_view(line.text, length), notice, shownNote, shownVowel,
             octave, (100 * volume + 127) / 255, sayMode, saying, demo});
        dirty = false;
        showedSpeech = saying;
    }
    if (now - lastHead >= 33) {
        lastHead = now;
        const auto phone = activePose.load(std::memory_order_relaxed);
        screen.anatomy(M5Cardputer.Display, static_cast<Phone>(phone),
                       sayMode ? saying && phone != PhIdle : shownNote != 0);
    }

    static unsigned lastReport = now;
    if (now - lastReport >= 5000) {
        lastReport = now;
        Serial.printf("render_max_us=%u budget_us=%u queue_empty=%u enqueue_failed=%u heap=%u\n",
                      worstRenderUs.load(), kBlockUs, queueEmpty.load(),
                      enqueueFailures.load(), static_cast<unsigned>(ESP.getFreeHeap()));
    }
    delay(3);
}
