#pragma once

#include <cstdint>
#include <functional>

#include "chipo_ambient_logic.h"
#include "lvgl.h"

/**
 * CHIPO AMBIENT: rain / fire / soft noise in the background, started and stopped by voice.
 *
 * XiaoZhi's playback queue is tiny (20 Opus packets = 1.2 s) and PlaySound blocks its caller until the clip is queued, so the
 * sound is stored as 9 pieces of 0.96 s (16 packets each, consecutive parts of one stream, see scripts/make_ambient_sfx.py)
 * and ONE piece is queued about every second. It pauses while the assistant listens or speaks (XiaoZhi clears its queue
 * then anyway), resumes afterwards and switches itself off after 30 minutes (the speaker would keep the codec on).
 * Call everything on the LVGL task.
 */
class ChipoAmbient {
public:
    static constexpr int kPieces = 9;
    static constexpr uint32_t kPieceMs = 960;
    static constexpr uint32_t kMaxPlayMs = 30u * 60u * 1000u;

    struct Hooks {
        std::function<bool(const char* clip_id)> clip_exists;
        std::function<bool(const char* clip_id)> play_clip;  // false if the clip does not exist
        std::function<void(const char* text)> notify;
        std::function<void(const char* emotion, uint32_t ms)> emotion;
        std::function<bool()> assistant_busy;                // listening / speaking: pause
        std::function<uint32_t()> clock;                     // ms, defaults to lv_tick_get
    };

    ChipoAmbient();
    ~ChipoAmbient();
    ChipoAmbient(const ChipoAmbient&) = delete;
    ChipoAmbient& operator=(const ChipoAmbient&) = delete;

    bool Start(Hooks hooks);
    void Stop();

    // True when CHIPO answered with a sound, so the server's spoken reply must be dropped.
    bool OnVoiceText(const char* text);

    bool playing() const { return playing_; }
    AmbientSound sound() const { return sound_; }
    int pieces_sent() const { return pieces_sent_; }
    void Step(uint32_t now_ms);  // public so tests can drive time

private:
    static void TimerCallback(lv_timer_t* timer);
    uint32_t Now() const;
    void StopPlaying(const char* text);

    Hooks hooks_;
    lv_timer_t* timer_ = nullptr;
    bool running_ = false;
    bool playing_ = false;
    bool paused_ = false;
    AmbientSound sound_ = AmbientSound::Rain;
    int piece_ = 0;
    int pieces_sent_ = 0;
    uint32_t started_ms_ = 0;
    uint32_t next_push_ms_ = 0;
};
