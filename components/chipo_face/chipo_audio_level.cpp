#include "chipo_audio_level.h"

#include <atomic>
#include <chrono>
#include <cmath>

namespace {
constexpr int kWindow = 256;
constexpr float kBandHz[kChipoAudioBands] = {150, 300, 600, 1000, 1700, 2800, 4500, 7000};
constexpr uint32_t kFreshMs = 300;

struct Source {
    std::atomic<float> level{0};
    std::atomic<float> band[kChipoAudioBands];
    std::atomic<uint32_t> stamp{0};
    std::atomic<bool> any{false};
    Source() {
        for (auto& b : band) b.store(0);
    }
};
Source g_src[2];  // 0 speaker, 1 microphone

// Hann window table (the audio task must not spend a cos() per sample and band). Built once, thread-safe:
// the microphone task and the speaker task both call this.
struct HannWindow {
    float v[kWindow];
    HannWindow() {
        for (int i = 0; i < kWindow; ++i) {
            v[i] = 0.5f - 0.5f * std::cos(2.0f * 3.14159265f * static_cast<float>(i) / static_cast<float>(kWindow - 1));
        }
    }
};
const float* HannTable() {
    static const HannWindow table;  // C++ guarantees one initialisation even with two threads
    return table.v;
}

float Map(float amplitude, float floor_db, float ceil_db) {
    if (amplitude < 1e-6f) return 0.0f;
    const float db = 20.0f * std::log10(amplitude);
    const float v = (db - floor_db) / (ceil_db - floor_db);
    return v < 0 ? 0.0f : (v > 1 ? 1.0f : v);
}
}  // namespace

uint32_t ChipoAudioNowMs() {
    return static_cast<uint32_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
}

void ChipoAudioReset() {
    for (Source& s : g_src) {
        s.level.store(0);
        for (auto& b : s.band) b.store(0);
        s.stamp.store(0);
        s.any.store(false);
    }
}

void ChipoAudioFeed(const int16_t* pcm, size_t samples, int sample_rate, int channels, bool from_mic, uint32_t now_ms) {
    if (!pcm || samples == 0 || sample_rate <= 0) return;
    const size_t stride = channels > 1 ? static_cast<size_t>(channels) : 1;
    size_t n = samples / stride;
    if (n > kWindow) n = kWindow;
    if (n < 32) return;
    Source& src = g_src[from_mic ? 1 : 0];

    double sum_sq = 0;
    for (size_t i = 0; i < n; ++i) {
        const float x = static_cast<float>(pcm[i * stride]) / 32768.0f;
        sum_sq += static_cast<double>(x) * x;
    }
    const float rms = static_cast<float>(std::sqrt(sum_sq / static_cast<double>(n)));
    src.level.store(Map(rms, -54.0f, -12.0f));

    const float* hann_table = HannTable();
    const float rate = static_cast<float>(sample_rate);
    for (int b = 0; b < kChipoAudioBands; ++b) {
        if (kBandHz[b] > rate * 0.45f) {  // above what this sample rate can carry
            src.band[b].store(0);
            continue;
        }
        // Goertzel on a Hann-windowed block.
        const float w = 2.0f * 3.14159265f * kBandHz[b] / rate;
        const float coeff = 2.0f * std::cos(w);
        float s1 = 0, s2 = 0;
        for (size_t i = 0; i < n; ++i) {
            const float hann = hann_table[i * (kWindow - 1) / (n - 1)];
            const float x = static_cast<float>(pcm[i * stride]) / 32768.0f * hann;
            const float s0 = x + coeff * s1 - s2;
            s2 = s1;
            s1 = s0;
        }
        const float power = s1 * s1 + s2 * s2 - coeff * s1 * s2;
        const float amplitude = 2.0f * std::sqrt(power > 0 ? power : 0.0f) / (static_cast<float>(n) * 0.5f);  // Hann: mean 0.5
        src.band[b].store(Map(amplitude, -60.0f, -10.0f));
    }
    src.any.store(true);
    src.stamp.store(now_ms);
}

ChipoAudioLevels ChipoAudioRead(uint32_t now_ms, bool include_mic) {
    ChipoAudioLevels out;
    for (int k = 0; k < (include_mic ? 2 : 1); ++k) {
        const Source& s = g_src[k];
        if (!s.any.load()) continue;
        if (now_ms - s.stamp.load() > kFreshMs) continue;
        out.valid = true;
        const float lv = s.level.load();
        if (lv > out.level) out.level = lv;
        for (int b = 0; b < kChipoAudioBands; ++b) {
            const float v = s.band[b].load();
            if (v > out.band[b]) out.band[b] = v;
        }
    }
    return out;
}
