#include <cmath>
#include <cstdio>
#include <vector>

#include "chipo_audio_level.h"

static int g_failures = 0;
#define CHECK(cond, ...) do { if (!(cond)) { ++g_failures; std::printf("FAIL line %d: ", __LINE__); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)

static std::vector<int16_t> Tone(float hz, float amp, int rate, int n, int channels = 1) {
    std::vector<int16_t> v(static_cast<size_t>(n) * channels);
    for (int i = 0; i < n; ++i) {
        const int16_t s = static_cast<int16_t>(amp * 32767.0f * std::sin(2.0f * 3.14159265f * hz * static_cast<float>(i) / static_cast<float>(rate)));
        for (int c = 0; c < channels; ++c) v[static_cast<size_t>(i) * channels + c] = s;
    }
    return v;
}

int main() {
    ChipoAudioReset();
    CHECK(!ChipoAudioRead(1000, true).valid, "nothing before any audio");
    auto quiet = Tone(1000, 0.0f, 16000, 512);
    ChipoAudioFeed(quiet.data(), quiet.size(), 16000, 1, false, 1000);
    CHECK(ChipoAudioRead(1100, false).level < 0.01f, "silence is zero");
    // A 1 kHz tone lights band 3 (1000 Hz) most.
    auto t1k = Tone(1000, 0.5f, 16000, 512);
    ChipoAudioFeed(t1k.data(), t1k.size(), 16000, 1, false, 2000);
    ChipoAudioLevels l = ChipoAudioRead(2100, false);
    CHECK(l.valid && l.level > 0.6f, "loud tone gives a high level (%.2f)", l.level);
    int best = 0;
    for (int b = 1; b < kChipoAudioBands; ++b) if (l.band[b] > l.band[best]) best = b;
    CHECK(best == 3 && l.band[3] > 0.7f, "1 kHz lights band 3 (best %d: %.2f)", best, l.band[3]);
    CHECK(l.band[0] < l.band[3] * 0.5f && l.band[7] < l.band[3] * 0.5f, "far bands stay low (%.2f %.2f)", l.band[0], l.band[7]);
    // 150 Hz lights band 0; 4.5 kHz band 6 at 16 kHz (Nyquist 8 kHz: 7000 Hz is dropped).
    auto t150 = Tone(150, 0.5f, 16000, 512);
    ChipoAudioFeed(t150.data(), t150.size(), 16000, 1, false, 3000);
    l = ChipoAudioRead(3050, false);
    best = 0; for (int b = 1; b < kChipoAudioBands; ++b) if (l.band[b] > l.band[best]) best = b;
    CHECK(best == 0, "150 Hz lights band 0 (%d)", best);
    auto t4k5 = Tone(4500, 0.5f, 16000, 512);
    ChipoAudioFeed(t4k5.data(), t4k5.size(), 16000, 1, false, 4000);
    l = ChipoAudioRead(4050, false);
    best = 0; for (int b = 1; b < kChipoAudioBands; ++b) if (l.band[b] > l.band[best]) best = b;
    CHECK(best == 6, "4.5 kHz lights band 6 (%d)", best);
    CHECK(l.band[7] == 0.0f, "7 kHz cannot be seen at 16 kHz sampling");
    // Quieter is lower; stereo reads the first channel.
    auto soft = Tone(1000, 0.02f, 16000, 512, 2);
    ChipoAudioFeed(soft.data(), soft.size(), 16000, 2, false, 5000);
    l = ChipoAudioRead(5050, false);
    CHECK(l.level < 0.5f && l.level > 0.0f, "quiet tone is lower (%.2f)", l.level);
    // Stale data is ignored; the microphone only counts when asked for.
    CHECK(!ChipoAudioRead(5400, false).valid, "300 ms later the data is stale");
    ChipoAudioReset();
    ChipoAudioFeed(t1k.data(), t1k.size(), 16000, 1, true, 100);
    CHECK(!ChipoAudioRead(150, false).valid && ChipoAudioRead(150, true).valid, "microphone only when included");
    // Garbage in: no crash.
    ChipoAudioFeed(nullptr, 10, 16000, 1, false, 0);
    ChipoAudioFeed(t1k.data(), 5, 16000, 1, false, 0);
    ChipoAudioFeed(t1k.data(), 512, 0, 1, false, 0);
    const uint32_t c0 = ChipoAudioNowMs();
    CHECK(ChipoAudioNowMs() - c0 < 1000, "clock runs");
    if (g_failures == 0) std::printf("ALL AUDIO LEVEL TESTS PASSED\n");
    return g_failures == 0 ? 0 : 1;
}
