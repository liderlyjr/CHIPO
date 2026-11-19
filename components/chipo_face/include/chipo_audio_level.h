#pragma once

#include <cstddef>
#include <cstdint>

/**
 * Loudness and an 8-band spectrum of the audio CHIPO plays (speaker) and hears (microphone), for the dance and the
 * sound visualizer. The audio task calls ChipoAudioFeed with every block it plays or records (cheap: it looks at the
 * first 256 samples only); the UI task reads ChipoAudioRead. No allocation, no locks (single writer per source).
 *
 * Honest limits: it is a real RMS and a real 8-band Goertzel spectrum of what passes through CHIPO's own audio path.
 * Music playing on ANOTHER device (e.g. Home Assistant on a speaker) is only heard through the microphone, and the
 * microphone only runs while CHIPO is listening for its wake word or to the owner.
 */
constexpr int kChipoAudioBands = 8;

struct ChipoAudioLevels {
    bool valid = false;               // fresh data (< 300 ms old) from at least one source
    float level = 0;                  // 0..1 (-54 dBFS .. -12 dBFS)
    float band[kChipoAudioBands] = {};  // 0..1 (-60 dBFS .. -10 dBFS)
};

// pcm: interleaved 16-bit samples; only the first channel is analysed. now_ms: any monotonic millisecond clock.
void ChipoAudioFeed(const int16_t* pcm, size_t samples, int sample_rate, int channels, bool from_mic, uint32_t now_ms);
ChipoAudioLevels ChipoAudioRead(uint32_t now_ms, bool include_mic);
uint32_t ChipoAudioNowMs();  // steady clock, for the callers that have no tick of their own
void ChipoAudioReset();      // tests
