#pragma once

#include <cstdint>
#include <functional>

#include "chipo_scenes_logic.h"
#include "lvgl.h"

/**
 * CHIPO SCENES: three full-screen decorations for the AMOLED (black = pixels off, saturated colours) and the dance command.
 *   - sound visualizer: 8 real frequency bands of what CHIPO plays (and hears, when the microphone is running)
 *   - aquarium: four fish, bubbles and swaying plants
 *   - lava lamp: slow glowing blobs
 * A touch (or "quita eso") closes the screen; they end by themselves after 10 minutes. No sound of their own.
 * Every method runs on the LVGL task with the display lock held.
 */
class ChipoScenes {
public:
    struct Hooks {
        // Fills 8 band values and the level (0..1); false when no fresh audio.
        std::function<bool(float* bands, float* level)> audio;
        std::function<void(const char* text)> notify;
        std::function<bool(const char* clip_id)> play_clip;
        std::function<void(bool on)> dance;       // ChipoFace::SetDance
        std::function<bool()> dancing;            // is the face dancing (for "para")
        std::function<int(int lo, int hi)> rand;  // default: lv_rand
        std::function<uint32_t()> clock;          // ms, defaults to lv_tick_get
    };

    ChipoScenes();
    ~ChipoScenes();
    ChipoScenes(const ChipoScenes&) = delete;
    ChipoScenes& operator=(const ChipoScenes&) = delete;

    bool Start(lv_obj_t* parent, Hooks hooks);
    void Stop();

    // True when CHIPO answered with a sound/notification, so the server's spoken reply must be dropped.
    bool OnVoiceText(const char* text);
    void Show(SceneKind kind);
    void Hide();
    void OnTouch() { Hide(); }
    SceneKind shown() const { return kind_; }
    bool screen_shown() const { return kind_ != SceneKind::None; }

    struct DebugInfo {
        SceneKind kind;
        int bars_px[BarsModel::kBars];  // current bar heights
        int fish_dir[AquariumModel::kFish];
        int visible_objects;
    };
    DebugInfo Debug() const;

    void Step(uint32_t now_ms);  // public so tests can drive time

private:
    static void TimerCallback(lv_timer_t* timer);
    static void EventCallback(lv_event_t* e);
    uint32_t Now() const;
    int Rand(int lo, int hi) const;
    void Build();
    void UpdateVisualizer();
    void UpdateAquarium();
    void UpdateLava();

    static constexpr int kBars = BarsModel::kBars;
    Hooks hooks_;
    int w_ = 368, h_ = 448;
    lv_obj_t* root_ = nullptr;
    lv_timer_t* timer_ = nullptr;
    SceneKind kind_ = SceneKind::None;
    bool running_ = false;
    uint32_t shown_ms_ = 0, last_ms_ = 0, start_ms_ = 0;

    BarsModel bars_;
    AquariumModel aquarium_;
    LavaModel lava_;
    lv_obj_t* bar_[kBars] = {};
    lv_obj_t* cap_[kBars] = {};
    // Fish: body, tail, eye. Bubbles: ring. Plants: stalk.
    lv_obj_t* fish_[AquariumModel::kFish][3] = {};
    lv_obj_t* bubble_[AquariumModel::kBubbles] = {};
    lv_obj_t* plant_[AquariumModel::kPlants] = {};
    lv_obj_t* blob_[LavaModel::kBlobs] = {};
    int bar_h_[kBars] = {};
};
