#pragma once

#include <cstdint>
#include <functional>
#include <string>

#include "lvgl.h"

/**
 * Full-screen retro clock of CHIPO BUDDY: four seven-segment digits drawn with LVGL rectangles
 * (no font needed), a caption above and a footer below. Used for the countdowns, the alarm and
 * the bedside clock. Lives above the face; hidden unless asked for.
 */
struct BuddyScreen {
    char digits[4] = {'0', '0', '0', '0'};  // '0'..'9' or ' ' (segment display off)
    bool colon = true;
    const char* caption = "";
    const char* footer = "";
    uint32_t color = 0xFF9A2E;  // amber
    uint8_t brightness = 255;   // 0..255: the bedside clock is dim
};

class ChipoBuddyView {
public:
    // short(click) / long press on the screen
    using TouchCallback = std::function<void(bool long_press)>;

    bool Create(lv_obj_t* parent, const lv_font_t* font);
    void Destroy();

    void Show(const BuddyScreen& screen);  // creates nothing new; only changes what changed
    // 0: upright. 1: turned clockwise (CHIPO lying on its left side, right edge up). 2: counter-clockwise.
    // While turned only the digits are drawn (bigger); the caption and footer are hidden.
    void SetRotation(int rotation);
    int rotation() const { return rotation_; }
    void Hide();
    bool IsShown() const { return shown_; }
    void SetTouchCallback(TouchCallback cb) { touch_ = std::move(cb); }

    struct DebugInfo {
        bool shown;
        char digits[5];
        bool colon_on;
        int lit_segments;  // how many segments are lit
        uint32_t color;
        int rotation;
        uint8_t brightness;
        int fx;               // 0 none, 1 appearing, 2 disappearing
        bool root_visible;    // still drawn (true while the disappear animation runs)
    };
    DebugInfo Debug() const;

private:
    static void EventCallback(lv_event_t* e);
    static void FxTimerCallback(lv_timer_t* t);
    void BuildGeometry();
    // Appear / disappear: the segments "strike" one digit after another like neon tubes (with a flicker),
    // and switch off in reverse order. Only the overall opacity of each piece changes (no layers, cheap).
    enum class Fx : uint8_t { None, In, Out };
    void StartFx(Fx fx);
    void StepFx();
    void SetPieceOpa(lv_obj_t* o, int opa);
    lv_timer_t* fx_timer_ = nullptr;
    Fx fx_ = Fx::None;
    uint32_t fx_t0_ = 0;
    int pending_rotation_ = -1;  // asked while the digits were switching off: applied when they are gone

    lv_obj_t* root_ = nullptr;
    lv_obj_t* seg_[4][7] = {};
    lv_obj_t* dot_[2] = {};
    lv_obj_t* caption_ = nullptr;
    lv_obj_t* footer_ = nullptr;
    bool shown_ = false;
    int rotation_ = 0;
    int screen_w_ = 480, screen_h_ = 480;
    BuddyScreen applied_;
    bool have_applied_ = false;
    std::string caption_text_, footer_text_;
    TouchCallback touch_;
};
