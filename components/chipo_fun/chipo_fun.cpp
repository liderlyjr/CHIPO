#include "chipo_fun.h"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace {
constexpr uint32_t kTickMs = 50;
constexpr uint32_t kBootMs = 2600;
constexpr uint32_t kRollMs = 1300;
constexpr uint32_t kCoinMs = 1500;
constexpr uint32_t kResultMs = 7000;
constexpr uint32_t kLampMaxMs = 30u * 60u * 1000u;  // the lamp switches itself off: it is the most power-hungry thing here
constexpr uint32_t kNeonCyan = 0x00E5FF;
constexpr uint32_t kNeonPink = 0xFF2E93;
constexpr float kPi = 3.14159265f;

lv_obj_t* Box(lv_obj_t* parent, int x, int y, int w, int h, int radius, uint32_t color, int opa = 255) {
    lv_obj_t* o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, w, h);
    lv_obj_set_style_radius(o, radius, 0);
    lv_obj_set_style_bg_color(o, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(o, static_cast<lv_opa_t>(opa), 0);
    lv_obj_remove_flag(o, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
    return o;
}

void Glow(lv_obj_t* o, uint32_t color, int width, int opa) {
    lv_obj_set_style_shadow_width(o, width, 0);
    lv_obj_set_style_shadow_color(o, lv_color_hex(color), 0);
    lv_obj_set_style_shadow_opa(o, static_cast<lv_opa_t>(opa), 0);
    lv_obj_set_style_shadow_spread(o, 2, 0);
}

uint32_t Rgb(lv_color_t c) { return (static_cast<uint32_t>(c.red) << 16) | (static_cast<uint32_t>(c.green) << 8) | c.blue; }

// Pip layout of a die face: bit k = grid cell k (3x3, row by row).
constexpr uint16_t kPips[7] = {0, 0b000010000, 0b100000001, 0b100010001, 0b101000101, 0b101010101, 0b101101101};

void SetHidden(lv_obj_t* o, bool hidden) {
    if (!o) return;
    if (hidden) lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_remove_flag(o, LV_OBJ_FLAG_HIDDEN);
}
}  // namespace

ChipoFun::ChipoFun() = default;
ChipoFun::~ChipoFun() { Stop(); }

uint32_t ChipoFun::Now() const { return hooks_.clock ? hooks_.clock() : lv_tick_get(); }
bool ChipoFun::Play(const char* clip) { return hooks_.play_clip && hooks_.play_clip(clip); }
void ChipoFun::Note(const char* text) { if (hooks_.notify) hooks_.notify(text); }

bool ChipoFun::Start(lv_obj_t* parent, Hooks hooks, const char* version, const lv_font_t* font) {
    if (running_ || !parent) return running_;
    hooks_ = std::move(hooks);
#if LV_FONT_MONTSERRAT_24
    if (!font) font = &lv_font_montserrat_24;
#endif
    if (!font) font = lv_font_get_default();
    int32_t w = lv_obj_get_width(parent), h = lv_obj_get_height(parent);
    if (w <= 0) w = 368;
    if (h <= 0) h = 448;
    w_ = w;
    h_ = h;
    root_ = lv_obj_create(parent);
    if (!root_) return false;
    lv_obj_remove_style_all(root_);
    lv_obj_set_size(root_, w, h);
    lv_obj_set_pos(root_, 0, 0);
    lv_obj_set_style_bg_color(root_, lv_color_black(), 0);  // true black: AMOLED pixels off
    lv_obj_set_style_bg_opa(root_, LV_OPA_COVER, 0);
    lv_obj_remove_flag(root_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(root_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(root_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(root_, EventCallback, LV_EVENT_CLICKED, this);
    Build(font, version ? version : "", w, h);
    timer_ = lv_timer_create(TimerCallback, kTickMs, this);
    if (!timer_) {
        lv_obj_delete(root_);
        root_ = nullptr;
        return false;
    }
    lv_timer_pause(timer_);  // nothing to animate until something is shown
    murmur_timer_ = lv_timer_create(MurmurCallback, 1000, this);
    running_ = true;
    return true;
}

// w/h come from Start: an object's width reads 0 until LVGL has run its layout.
void ChipoFun::Build(const lv_font_t* font, const char* version, int w, int h) {
    const int cx = w / 2;

    // ---- start-up screen: a neon "happy Mac" ---------------------------------------------------
    boot_ = Box(root_, 0, 0, w, h, 0, 0x000000, 0);
    const int bw = 168, bh = 190, bx = cx - bw / 2, by = h / 2 - 150;
    lv_obj_t* body = Box(boot_, bx, by, bw, bh, 30, 0x001018);
    lv_obj_set_style_border_color(body, lv_color_hex(kNeonCyan), 0);
    lv_obj_set_style_border_width(body, 6, 0);
    Glow(body, kNeonCyan, 36, 200);
    lv_obj_t* screen = Box(boot_, bx + 22, by + 24, bw - 44, bh - 78, 14, 0x00323C);
    lv_obj_set_style_border_color(screen, lv_color_hex(0x00B8D0), 0);
    lv_obj_set_style_border_width(screen, 3, 0);
    boot_parts_[0] = body;
    boot_parts_[1] = screen;
    boot_parts_[2] = Box(boot_, cx - 28, by + 52, 9, 26, 4, kNeonCyan);   // left eye
    boot_parts_[3] = Box(boot_, cx + 19, by + 52, 9, 26, 4, kNeonCyan);   // right eye
    Glow(boot_parts_[2], kNeonCyan, 18, 220);
    Glow(boot_parts_[3], kNeonCyan, 18, 220);
    lv_obj_t* smile = lv_arc_create(boot_);                                // smile
    lv_obj_remove_style_all(smile);
    lv_obj_set_size(smile, 64, 64);
    lv_obj_set_pos(smile, cx - 32, by + 48);
    lv_arc_set_bg_angles(smile, 0, 360);
    lv_arc_set_angles(smile, 28, 152);
    lv_obj_set_style_arc_opa(smile, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_arc_width(smile, 7, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(smile, lv_color_hex(kNeonCyan), LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(smile, true, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(smile, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_remove_flag(smile, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
    boot_parts_[4] = smile;
    boot_parts_[5] = Box(boot_, bx + 30, by + bh - 36, 70, 8, 4, 0x00B8D0);     // floppy slot
    boot_parts_[6] = Box(boot_, bx + bw - 52, by + bh - 36, 16, 8, 4, kNeonPink);  // power light
    Glow(boot_parts_[6], kNeonPink, 16, 230);
    boot_title_ = lv_label_create(boot_);
    lv_label_set_text(boot_title_, "CHIPO OS");
    lv_obj_set_style_text_font(boot_title_, font, 0);
    lv_obj_set_style_text_color(boot_title_, lv_color_hex(kNeonCyan), 0);
    lv_obj_set_style_text_letter_space(boot_title_, 6, 0);
    lv_obj_align(boot_title_, LV_ALIGN_TOP_MID, 0, by + bh + 44);
    boot_version_ = lv_label_create(boot_);
    char buf[32];
    std::snprintf(buf, sizeof buf, "%s", version);
    lv_label_set_text(boot_version_, buf);
    lv_obj_set_style_text_font(boot_version_, font, 0);
    lv_obj_set_style_text_color(boot_version_, lv_color_hex(0x7A8C94), 0);
    lv_obj_align(boot_version_, LV_ALIGN_TOP_MID, 0, by + bh + 84);
    lv_obj_remove_flag(boot_title_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(boot_version_, LV_OBJ_FLAG_CLICKABLE);

    // ---- lamp -------------------------------------------------------------------------------------
    lamp_ = Box(root_, 0, 0, w, h, 0, 0x000000);
    lv_obj_set_style_bg_grad_dir(lamp_, LV_GRAD_DIR_VER, 0);

    // ---- dice -----------------------------------------------------------------------------------
    for (int d = 0; d < 2; ++d) {
        die_[d] = Box(root_, 0, 0, 10, 10, 30, 0x120008);
        lv_obj_set_style_border_color(die_[d], lv_color_hex(kNeonPink), 0);
        lv_obj_set_style_border_width(die_[d], 6, 0);
        Glow(die_[d], kNeonPink, 34, 190);
        for (auto& p : pip_[d]) {
            p = Box(die_[d], 0, 0, 10, 10, 20, 0xFFE9F3);
            Glow(p, kNeonPink, 12, 200);
        }
    }

    // ---- coin -------------------------------------------------------------------------------------
    // Glow = flat translucent discs (LVGL's software blur on a 230 px disc is far too slow for 25 fps).
    coin_halo_ = Box(root_, 0, 0, 10, 10, LV_RADIUS_CIRCLE, 0xFFA000, 26);
    coin_ = Box(root_, 0, 0, 10, 10, LV_RADIUS_CIRCLE, 0xFFB000);
    lv_obj_set_style_bg_grad_color(coin_, lv_color_hex(0xFF6A00), 0);
    lv_obj_set_style_bg_grad_dir(coin_, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_border_color(coin_, lv_color_hex(0xFFE27A), 0);
    lv_obj_set_style_border_width(coin_, 8, 0);
    lv_obj_set_style_outline_width(coin_, 14, 0);
    lv_obj_set_style_outline_color(coin_, lv_color_hex(0xFFA000), 0);
    lv_obj_set_style_outline_opa(coin_, 60, 0);
    for (int i = 0; i < 5; ++i) coin_face_[i] = Box(coin_, 0, 0, 10, 10, 5, 0x3A1A00);

    caption_ = lv_label_create(root_);
    lv_label_set_text(caption_, "");
    lv_obj_set_style_text_font(caption_, font, 0);
    lv_obj_set_style_text_color(caption_, lv_color_hex(0xF0F0F0), 0);
    lv_obj_set_style_text_align(caption_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(caption_, w - 40);
    lv_obj_remove_flag(caption_, LV_OBJ_FLAG_CLICKABLE);
}

void ChipoFun::Stop() {
    if (timer_) lv_timer_delete(timer_);
    timer_ = nullptr;
    if (murmur_timer_) lv_timer_delete(murmur_timer_);
    murmur_timer_ = nullptr;
    if (root_) lv_obj_delete(root_);
    root_ = boot_ = lamp_ = coin_ = coin_halo_ = caption_ = boot_title_ = boot_version_ = nullptr;
    for (auto& o : boot_parts_) o = nullptr;
    for (auto& o : die_) o = nullptr;
    for (auto& row : pip_) for (auto& o : row) o = nullptr;
    for (auto& o : coin_face_) o = nullptr;
    running_ = false;
    mode_ = Mode::Off;
}

bool ChipoFun::OnServerError(uint32_t now_ms) {
    if (!running_) return false;
    if (had_oops_ && now_ms - last_oops_ms_ < 20000u) return false;
    had_oops_ = true;
    last_oops_ms_ = now_ms;
    ++oops_count_;
    Note("Me caí... uuuy, me caí");
    Play("oops");  // without the clip the text and the face are still shown
    if (hooks_.oops) hooks_.oops();
    return true;
}

void ChipoFun::MurmurCallback(lv_timer_t* timer) {
    auto* self = static_cast<ChipoFun*>(lv_timer_get_user_data(timer));
    if (self) self->MurmurTick(self->Now());
}

void ChipoFun::SetOffline(bool offline) { offline_.store(offline); }

void ChipoFun::MurmurTick(uint32_t now_ms) {
    if (!running_) return;
    const bool offline = offline_.load();
    if (!offline) {
        offline_seen_ = false;
        return;
    }
    if (!offline_seen_) {  // just lost the connection: the first murmur comes a little sooner
        offline_seen_ = true;
        next_murmur_ms_ = now_ms + 8000u;
        return;
    }
    if (static_cast<int32_t>(now_ms - next_murmur_ms_) < 0) return;
    next_murmur_ms_ = now_ms + static_cast<uint32_t>(lv_rand(30000, 60000));
    if (mode_ != Mode::Off) return;  // not over the lamp, dice or coin
    // Never the same clip twice in a row.
    int k = static_cast<int>(lv_rand(1, 4));
    if (k == murmur_last_) k = k % 4 + 1;
    char id[16];
    std::snprintf(id, sizeof(id), "murmur_%d", k);
    if (!Play(id)) return;
    murmur_last_ = k;
    ++murmur_count_;
    if (hooks_.babble) hooks_.babble(1400);
}

void ChipoFun::TimerCallback(lv_timer_t* timer) {
    auto* self = static_cast<ChipoFun*>(lv_timer_get_user_data(timer));
    if (self) self->Step(self->Now());
}

void ChipoFun::EventCallback(lv_event_t* e) {
    auto* self = static_cast<ChipoFun*>(lv_event_get_user_data(e));
    if (self) self->OnTouch();
}

void ChipoFun::SetMode(Mode m, uint32_t now_ms) {
    mode_ = m;
    mode_since_ = now_ms;
    SetHidden(boot_, m != Mode::Boot);
    SetHidden(lamp_, m != Mode::Lamp);
    SetHidden(die_[0], m != Mode::Dice);
    SetHidden(die_[1], m != Mode::Dice || dice_count_ < 2);
    SetHidden(coin_, m != Mode::Coin);
    SetHidden(coin_halo_, m != Mode::Coin);
    SetHidden(caption_, m != Mode::Dice && m != Mode::Coin);
    if (m == Mode::Off) {
        Hide();
    } else {
        lv_obj_remove_flag(root_, LV_OBJ_FLAG_HIDDEN);
        lv_timer_resume(timer_);
    }
}

void ChipoFun::Hide() {
    if (root_) lv_obj_add_flag(root_, LV_OBJ_FLAG_HIDDEN);
    if (timer_) lv_timer_pause(timer_);
}

void ChipoFun::ShowBoot() {
    if (!running_) return;
    const uint32_t now = Now();
    boot_alpha_ = 0;
    boot_alpha_applied_ = -1;
    SetMode(Mode::Boot, now);
    Play("fun_boot");
    UpdateBoot(now);
}

void ChipoFun::StartLamp(LampColor c, uint32_t now_ms) {
    lamp_color_ = c;
    lamp_applied_rgb_ = 0xFFFFFFFF;
    SetMode(Mode::Lamp, now_ms);
    UpdateLamp(now_ms);
}

void ChipoFun::StartDice(int count, uint32_t now_ms) {
    dice_count_ = count < 2 ? 1 : 2;
    const int w = w_, h = h_;
    const int size = dice_count_ == 1 ? 196 : 152, gap = 24;
    const int total = dice_count_ * size + (dice_count_ - 1) * gap;
    for (int d = 0; d < dice_count_; ++d) {
        lv_obj_set_size(die_[d], size, size);
        lv_obj_set_pos(die_[d], (w - total) / 2 + d * (size + gap), h / 2 - size / 2 - 20);
        const int cell = (size - 12) / 3, pip = cell * 5 / 9;
        for (int k = 0; k < 9; ++k) {
            lv_obj_set_size(pip_[d][k], pip, pip);
            lv_obj_set_pos(pip_[d][k], 6 + (k % 3) * cell + (cell - pip) / 2 - 3, 6 + (k / 3) * cell + (cell - pip) / 2 - 3);
        }
        dice_final_[d] = static_cast<int>(lv_rand(1, 6));
        dice_shown_[d] = 0;
    }
    rolling_ = true;
    next_shuffle_ = now_ms;
    SetMode(Mode::Dice, now_ms);
    lv_label_set_text(caption_, "");
    lv_obj_align(caption_, LV_ALIGN_TOP_MID, 0, h / 2 + 130);
    UpdateDice(now_ms);
}

void ChipoFun::StartCoin(uint32_t now_ms) {
    coin_heads_ = lv_rand(0, 1) == 0;
    rolling_ = true;
    coin_w_applied_ = -1;
    coin_content_shown_ = false;
    const int w = w_, h = h_;
    lv_obj_set_size(coin_, 230, 230);
    lv_obj_set_pos(coin_, (w - 230) / 2, h / 2 - 135);
    lv_obj_set_size(coin_halo_, 300, 300);
    lv_obj_set_pos(coin_halo_, (w - 300) / 2, h / 2 - 170);
    SetMode(Mode::Coin, now_ms);
    lv_label_set_text(caption_, "");
    lv_obj_align(caption_, LV_ALIGN_TOP_MID, 0, h / 2 + 130);
    UpdateCoin(now_ms);
}

bool ChipoFun::OnVoiceText(const char* text) {
    if (!running_ || mode_ == Mode::Boot) return false;
    const FunCommand cmd = ParseFunCommand(text);
    const uint32_t now = Now();
    switch (cmd.intent) {
    case FunIntent::None: return false;
    case FunIntent::Lamp:
        StartLamp(cmd.color, now);
        return Play("buddy_chime");
    case FunIntent::LampOff:
        if (mode_ != Mode::Lamp) return false;  // "apaga la luz" with no lamp on: Home Assistant / the assistant
        SetMode(Mode::Off, now);
        return Play("buddy_chime");
    case FunIntent::Dice:
        StartDice(cmd.dice, now);
        return Play("fun_roll");
    case FunIntent::Coin:
        StartCoin(now);
        return Play("fun_coin");
    }
    return false;
}

void ChipoFun::OnTouch() {
    if (!running_ || mode_ == Mode::Off) return;
    SetMode(Mode::Off, Now());
}

void ChipoFun::Step(uint32_t now_ms) {
    if (!running_) return;
    switch (mode_) {
    case Mode::Off: break;
    case Mode::Boot:
        UpdateBoot(now_ms);
        if (now_ms - mode_since_ >= kBootMs) SetMode(Mode::Off, now_ms);
        break;
    case Mode::Lamp:
        UpdateLamp(now_ms);
        if (now_ms - mode_since_ >= kLampMaxMs) {
            SetMode(Mode::Off, now_ms);
            Note("Lámpara apagada (30 min)");
        }
        break;
    case Mode::Dice:
        UpdateDice(now_ms);
        if (!rolling_ && now_ms - mode_since_ >= kResultMs + kRollMs) SetMode(Mode::Off, now_ms);
        break;
    case Mode::Coin:
        UpdateCoin(now_ms);
        if (!rolling_ && now_ms - mode_since_ >= kResultMs + kCoinMs) SetMode(Mode::Off, now_ms);
        break;
    }
}

void ChipoFun::UpdateBoot(uint32_t now_ms) {
    const float t = static_cast<float>(now_ms - mode_since_);
    // fade in (0.5 s), hold, fade out (0.5 s)
    float a = t < 500.0f ? t / 500.0f : (t > kBootMs - 500.0f ? (kBootMs - t) / 500.0f : 1.0f);
    if (a < 0) a = 0;
    boot_alpha_ = a;
    const int opa = (static_cast<int>(a * 255.0f) / 8) * 8;
    if (opa != boot_alpha_applied_) {
        lv_obj_set_style_opa(boot_, static_cast<lv_opa_t>(opa), 0);
        boot_alpha_applied_ = opa;
    }
}

void ChipoFun::UpdateLamp(uint32_t now_ms) {
    const float t = static_cast<float>(now_ms - mode_since_);
    lv_color_t top{}, bottom{};
    switch (lamp_color_) {
    case LampColor::Warm:   top = lv_color_hex(0xFFB347); bottom = lv_color_hex(0xFF5A1F); break;
    case LampColor::Cool:   top = lv_color_hex(0x7FD8FF); bottom = lv_color_hex(0x1E5BFF); break;
    case LampColor::Green:  top = lv_color_hex(0x8CFF9E); bottom = lv_color_hex(0x00B85C); break;
    case LampColor::Pink:   top = lv_color_hex(0xFF8CE6); bottom = lv_color_hex(0xB400FF); break;
    case LampColor::Rainbow: {
        const uint16_t hue = static_cast<uint16_t>(static_cast<uint32_t>(t / 40.0f) % 360u);
        top = lv_color_hsv_to_rgb(hue, 85, 100);
        bottom = lv_color_hsv_to_rgb(static_cast<uint16_t>((hue + 70u) % 360u), 100, 100);
        break;
    }
    }
    // Rainbow changes in steps of 6 degrees (8 fps): enough to look smooth, little to repaint.
    const uint32_t key = Rgb(top) ^ (Rgb(bottom) << 1);
    lamp_rgb_ = Rgb(top);
    if (lamp_color_ == LampColor::Rainbow) {
        static_assert(sizeof(uint32_t) == 4, "key");
        if (now_ms % 125 > kTickMs && lamp_applied_rgb_ != 0xFFFFFFFF) return;
    }
    if (key == lamp_applied_rgb_) return;
    lamp_applied_rgb_ = key;
    lv_obj_set_style_bg_color(lamp_, top, 0);
    lv_obj_set_style_bg_grad_color(lamp_, bottom, 0);
}

void ChipoFun::ShowPips(int die, int value) {
    const uint16_t mask = value >= 1 && value <= 6 ? kPips[value] : 0;
    for (int k = 0; k < 9; ++k) SetHidden(pip_[die][k], !((mask >> k) & 1));
}

void ChipoFun::UpdateDice(uint32_t now_ms) {
    const uint32_t t = now_ms - mode_since_;
    if (rolling_) {
        if (t >= kRollMs) {
            rolling_ = false;
            for (int d = 0; d < dice_count_; ++d) {
                dice_shown_[d] = dice_final_[d];
                ShowPips(d, dice_final_[d]);
            }
            char buf[40];
            if (dice_count_ == 1) std::snprintf(buf, sizeof buf, "Ha salido un %d", dice_final_[0]);
            else std::snprintf(buf, sizeof buf, "%d + %d = %d", dice_final_[0], dice_final_[1], dice_final_[0] + dice_final_[1]);
            lv_label_set_text(caption_, buf);
            Note(buf);
            return;
        }
        if (static_cast<int32_t>(now_ms - next_shuffle_) >= 0) {
            // Faster at the start, slower at the end, like a die running out of energy.
            next_shuffle_ = now_ms + 60 + t / 8;
            for (int d = 0; d < dice_count_; ++d) {
                int v;
                do { v = static_cast<int>(lv_rand(1, 6)); } while (v == dice_shown_[d]);
                dice_shown_[d] = v;
                ShowPips(d, v);
            }
        }
    }
}

void ChipoFun::UpdateCoin(uint32_t now_ms) {
    const uint32_t t = now_ms - mode_since_;
    const int full = 230;
    int w = full;
    bool content = true;
    if (rolling_) {
        if (t >= kCoinMs) {
            rolling_ = false;
            Note(coin_heads_ ? "Cara" : "Cruz");
            lv_label_set_text(caption_, coin_heads_ ? "CARA" : "CRUZ");
        } else {
            // 3 half-turns slowing down: the width follows |cos|.
            const float p = static_cast<float>(t) / static_cast<float>(kCoinMs);
            const float angle = (1.0f - (1.0f - p) * (1.0f - p)) * 3.0f * kPi + (coin_heads_ ? 0.0f : kPi);
            const float c = std::fabs(std::cos(angle));
            w = static_cast<int>(18.0f + (full - 18.0f) * c);
            content = c > 0.45f;
        }
    }
    w = (w / 4) * 4;
    if (w != coin_w_applied_) {
        const int sw = w_;
        lv_obj_set_width(coin_, w);
        lv_obj_set_x(coin_, (sw - w) / 2);
        SetHidden(coin_halo_, w < full - 8);  // glow only while the coin is face-on
        coin_w_applied_ = w;
    }
    if (content != coin_content_shown_ || !rolling_) {
        coin_content_shown_ = content;
        for (auto* o : coin_face_) SetHidden(o, true);
        if (content) {
            const int c = full / 2;
            if (coin_heads_) {  // happy face: two eyes and a smile (a flat bar plus two rounded ends)
                lv_obj_set_pos(coin_face_[0], c - 62, c - 52); lv_obj_set_size(coin_face_[0], 20, 46);
                lv_obj_set_pos(coin_face_[1], c + 34, c - 52); lv_obj_set_size(coin_face_[1], 20, 46);
                lv_obj_set_pos(coin_face_[2], c - 52, c + 30); lv_obj_set_size(coin_face_[2], 104, 16);
                lv_obj_set_pos(coin_face_[3], c - 62, c + 14); lv_obj_set_size(coin_face_[3], 16, 28);
                lv_obj_set_pos(coin_face_[4], c + 46, c + 14); lv_obj_set_size(coin_face_[4], 16, 28);
                for (auto* o : coin_face_) SetHidden(o, false);
            } else {            // cross: two bars
                lv_obj_set_pos(coin_face_[0], c - 12, c - 72); lv_obj_set_size(coin_face_[0], 24, 144);
                lv_obj_set_pos(coin_face_[1], c - 72, c - 12); lv_obj_set_size(coin_face_[1], 144, 24);
                SetHidden(coin_face_[0], false);
                SetHidden(coin_face_[1], false);
            }
        }
    }
}

ChipoFun::DebugInfo ChipoFun::Debug() const {
    DebugInfo d{};
    d.mode = mode_;
    d.rolling = rolling_;
    d.dice_values[0] = dice_shown_[0];
    d.dice_values[1] = dice_shown_[1];
    d.dice_count = dice_count_;
    d.coin_heads = coin_heads_;
    d.lamp = lamp_color_;
    d.lamp_rgb = lamp_rgb_;
    d.boot_alpha = boot_alpha_;
    return d;
}
