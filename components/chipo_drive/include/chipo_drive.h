#pragma once

#include <cstdint>
#include <functional>

#include "chipo_drive_motion.h"
#include "chipo_drive_personality.h"
#include "chipo_drive_voice.h"
#include "chipo_face.h"

/**
 * CHIPO DRIVE: the copilot mode for the car. Switched on and off ONLY by voice (never by USB,
 * charger, ignition or any electrical signal).
 *
 *   voice text (server speech recognition, XiaoZhi "stt")  ->  OnVoiceText()
 *   IMU samples (via ChipoFace::SetSampleObserver)         ->  DriveMotion -> events
 *   events -> face reactions (ChipoFace::DriveReact / SetDriveLean) + occasional spoken comments
 *
 * It owns no driver and no audio: sounds go through the `play_clip` hook (the glue plays an
 * embedded clip with XiaoZhi's AudioService), so the voice and the speaker are shared with the
 * assistant. All methods run on the LVGL task with the display lock held.
 */
class ChipoDrive {
public:
    struct Hooks {
        // Play the clip with this id; false if no such clip is embedded.
        std::function<bool(const char* clip_id)> play_clip;
        // Show a short text instead of / besides sound.
        std::function<void(const char* text)> notify;
        // True while the assistant is listening, thinking or speaking (CHIPO then stays quiet).
        std::function<bool()> assistant_busy;
        // Milliseconds clock (defaults to lv_tick_get).
        std::function<uint32_t()> clock;
    };

    ChipoDrive();
    ~ChipoDrive();
    ChipoDrive(const ChipoDrive&) = delete;
    ChipoDrive& operator=(const ChipoDrive&) = delete;

    // `face` must outlive this object (or call Stop() first).
    bool Start(ChipoFace* face, Hooks hooks);
    void Stop();

    // Feed it every sentence recognised from the user's voice. Returns true when CHIPO answered
    // the command itself with a sound, so the caller must silence the server's spoken reply.
    // (Commands that CHIPO could only acknowledge with text return false.)
    bool OnVoiceText(const char* text);

    bool active() const { return active_; }
    DriveSensitivity sensitivity() const { return sensitivity_; }
    const DriveOutput& motion() const { return motion_.output(); }

    // Timers (periodic LVGL timer; public so tests can drive time).
    void Step(uint32_t now_ms);
    // Every IMU sample (set as ChipoFace sample observer by Start; public for tests).
    void OnSample(const ImuSample& sample, uint32_t sample_ms);

private:
    static void TimerCallback(lv_timer_t* timer);
    uint32_t Now() const;
    void Activate();
    void Deactivate();
    void SetSensitivity(DriveSensitivity s);
    bool PlayOrNotify(const char* clip_id);
    void Say(const DrivePhrase* phrase);
    void React(DriveEventKind kind, float intensity);

    ChipoFace* face_ = nullptr;
    Hooks hooks_;
    lv_timer_t* timer_ = nullptr;
    DriveMotion motion_;
    DrivePersonality personality_;
    DriveSensitivity sensitivity_ = DriveSensitivity::Normal;
    bool active_ = false;
    uint32_t last_event_ms_ = 0;
    uint32_t next_log_ms_ = 0;
};
