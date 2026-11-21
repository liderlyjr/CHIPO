#include "chipo_scenes.h"

#include <algorithm>
#include <cmath>

namespace {
constexpr uint32_t kTickMs = 40;
constexpr uint32_t kAutoOffMs = 10u * 60u * 1000u;

lv_obj_t* Flat(lv_obj_t* parent, uint32_t rgb, int radius) {
    lv_obj_t* o = lv_obj_create(parent);
    if (!o) return nullptr;
    lv_obj_remove_style_all(o);
    lv_obj_set_style_bg_color(o, lv_color_hex(rgb), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(o, radius, LV_PART_MAIN);
    lv_obj_set_style_border_width(o, 0, LV_PART_MAIN);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
    return o;
}
void SetShown(lv_obj_t* o, bool on) {
    if (!o) return;
    if (on) lv_obj_remove_flag(o, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
}
uint32_t Lerp(uint32_t a, uint32_t b, float t) {
    auto ch = [&](int shift) {
        const float x = static_cast<float>((a >> shift) & 0xFF), y = static_cast<float>((b >> shift) & 0xFF);
        return static_cast<uint32_t>(x + (y - x) * t) & 0xFF;
    };
    return (ch(16) << 16) | (ch(8) << 8) | ch(0);
}
uint32_t Hsv(float h, float s, float v) {
    h = h - std::floor(h);
    const float k = h * 6.0f;
    const int i = static_cast<int>(k);
    const float f = k - static_cast<float>(i);
    const float p = v * (1 - s), q = v * (1 - s * f), t = v * (1 - s * (1 - f));
    float r, g, b;
    switch (i % 6) {
    case 0: r = v; g = t; b = p; break;
    case 1: r = q; g = v; b = p; break;
    case 2: r = p; g = v; b = t; break;
    case 3: r = p; g = q; b = v; break;
    case 4: r = t; g = p; b = v; break;
    default: r = v; g = p; b = q; break;
    }
    return (static_cast<uint32_t>(r * 255) << 16) | (static_cast<uint32_t>(g * 255) << 8) | static_cast<uint32_t>(b * 255);
}
}  // namespace

ChipoScenes::ChipoScenes() = default;
ChipoScenes::~ChipoScenes() { Stop(); }

uint32_t ChipoScenes::Now() const { return hooks_.clock ? hooks_.clock() : lv_tick_get(); }
int ChipoScenes::Rand(int lo, int hi) const {
    if (hooks_.rand) return hooks_.rand(lo, hi);
    return static_cast<int>(lv_rand(static_cast<uint32_t>(lo), static_cast<uint32_t>(hi)));
}

bool ChipoScenes::Start(lv_obj_t* parent, Hooks hooks) {
    if (!parent) return false;
    if (running_) return true;
    hooks_ = std::move(hooks);
    int32_t w = lv_obj_get_width(parent), h = lv_obj_get_height(parent);
    w_ = w > 0 ? w : 480;
    h_ = h > 0 ? h : 480;
    {   // design pixels 368 x 448: bars and plants keep their proportions on the 480 x 480 panel
        const float sc = static_cast<float>(w_) / 368.0f;
        bar_w_ = static_cast<int>(34 * sc + 0.5f);
        bar_gap_ = static_cast<int>(8 * sc + 0.5f);
        base_y_ = h_ - 56;
        max_bar_h_ = h_ * 300 / 448;
        plant_x0_ = static_cast<int>(40 * sc + 0.5f);
        plant_dx_ = static_cast<int>(140 * sc + 0.5f);
    }
    root_ = lv_obj_create(parent);
    if (!root_) return false;
    lv_obj_remove_style_all(root_);
    lv_obj_set_size(root_, w_, h_);
    lv_obj_set_pos(root_, 0, 0);
    lv_obj_set_style_bg_color(root_, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(root_, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_remove_flag(root_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(root_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(root_, EventCallback, LV_EVENT_CLICKED, this);
    lv_obj_add_flag(root_, LV_OBJ_FLAG_HIDDEN);
    Build();
    timer_ = lv_timer_create(TimerCallback, kTickMs, this);
    if (!timer_) {
        Stop();
        return false;
    }
    lv_timer_pause(timer_);
    running_ = true;
    return true;
}

void ChipoScenes::Stop() {
    if (timer_) lv_timer_delete(timer_);
    timer_ = nullptr;
    if (root_) lv_obj_delete(root_);
    root_ = nullptr;
    for (auto& o : bar_) o = nullptr;
    for (auto& o : cap_) o = nullptr;
    for (auto& f : fish_) for (auto& o : f) o = nullptr;
    for (auto& o : bubble_) o = nullptr;
    for (auto& o : plant_) o = nullptr;
    for (auto& o : blob_) o = nullptr;
    kind_ = SceneKind::None;
    running_ = false;
}

void ChipoScenes::Build() {
    const int x0 = (w_ - (kBars * bar_w_ + (kBars - 1) * bar_gap_)) / 2;
    for (int i = 0; i < kBars; ++i) {
        const uint32_t c = Lerp(0x00E5FF, 0xFF2EC8, static_cast<float>(i) / (kBars - 1));
        bar_[i] = Flat(root_, c, 8);
        cap_[i] = Flat(root_, 0xFFFFFF, 3);
        if (bar_[i]) { lv_obj_set_size(bar_[i], bar_w_, 4); lv_obj_set_pos(bar_[i], x0 + i * (bar_w_ + bar_gap_), base_y_ - 4); }
        if (cap_[i]) { lv_obj_set_size(cap_[i], bar_w_, 5); lv_obj_set_pos(cap_[i], x0 + i * (bar_w_ + bar_gap_), base_y_ - 12); }
    }
    for (int i = 0; i < AquariumModel::kFish; ++i) {
        fish_[i][0] = Flat(root_, 0xFFFFFF, LV_RADIUS_CIRCLE);   // body
        fish_[i][1] = Flat(root_, 0xFFFFFF, 3);                  // tail (turned square)
        fish_[i][2] = Flat(root_, 0x001018, LV_RADIUS_CIRCLE);   // eye
    }
    for (auto& b : bubble_) {
        b = Flat(root_, 0x000000, LV_RADIUS_CIRCLE);
        if (b) {
            lv_obj_set_style_bg_opa(b, LV_OPA_TRANSP, LV_PART_MAIN);
            lv_obj_set_style_border_width(b, 2, LV_PART_MAIN);
            lv_obj_set_style_border_color(b, lv_color_hex(0xBFEFFF), LV_PART_MAIN);
            lv_obj_set_style_border_opa(b, LV_OPA_60, LV_PART_MAIN);
        }
    }
    for (int i = 0; i < AquariumModel::kPlants; ++i) {
        plant_[i] = Flat(root_, i == 1 ? 0x2BD97A : 0x14B86A, 6);
        if (plant_[i]) {
            const int hgt = 110 + i * 30;
            lv_obj_set_size(plant_[i], 12, hgt);
            lv_obj_set_pos(plant_[i], plant_x0_ + i * plant_dx_, h_ - hgt);
            lv_obj_set_style_transform_pivot_x(plant_[i], 6, LV_PART_MAIN);
            lv_obj_set_style_transform_pivot_y(plant_[i], hgt, LV_PART_MAIN);
        }
    }
    for (auto& b : blob_) {
        b = Flat(root_, 0xFF6A1F, LV_RADIUS_CIRCLE);
        if (b) lv_obj_set_style_bg_opa(b, LV_OPA_70, LV_PART_MAIN);  // overlaps glow brighter, no shadows (they hang big objects)
    }
}

void ChipoScenes::Show(SceneKind kind) {
    if (!running_) return;
    if (kind == SceneKind::None) {
        Hide();
        return;
    }
    kind_ = kind;
    shown_ms_ = last_ms_ = start_ms_ = Now();
    SceneRand rand = [this](int lo, int hi) { return Rand(lo, hi); };
    if (kind == SceneKind::Visualizer) bars_ = BarsModel{};
    if (kind == SceneKind::Aquarium) aquarium_.Init(w_, h_, rand);
    if (kind == SceneKind::Lava) lava_.Init(w_, h_, rand);
    for (int i = 0; i < kBars; ++i) { SetShown(bar_[i], kind == SceneKind::Visualizer); SetShown(cap_[i], kind == SceneKind::Visualizer); bar_h_[i] = -1; }
    for (auto& f : fish_) for (auto* o : f) SetShown(o, kind == SceneKind::Aquarium);
    for (auto* o : bubble_) SetShown(o, kind == SceneKind::Aquarium);
    for (auto* o : plant_) SetShown(o, kind == SceneKind::Aquarium);
    for (auto* o : blob_) SetShown(o, kind == SceneKind::Lava);
    lv_obj_remove_flag(root_, LV_OBJ_FLAG_HIDDEN);
    lv_timer_resume(timer_);
    Step(shown_ms_);
}

void ChipoScenes::Hide() {
    if (!running_ || kind_ == SceneKind::None) return;
    kind_ = SceneKind::None;
    lv_obj_add_flag(root_, LV_OBJ_FLAG_HIDDEN);
    lv_timer_pause(timer_);
}

bool ChipoScenes::OnVoiceText(const char* text) {
    if (!running_) return false;
    const bool dancing = hooks_.dancing ? hooks_.dancing() : false;
    const DanceIntent dance = ParseDanceCommand(text, dancing);
    if (dance != DanceIntent::None) {
        if (hooks_.dance) hooks_.dance(dance == DanceIntent::On);
        if (hooks_.notify) hooks_.notify(dance == DanceIntent::On ? "¡A bailar!" : "Vale, paro");
        return hooks_.play_clip && hooks_.play_clip("buddy_chime");
    }
    const SceneCommand cmd = ParseSceneCommand(text, screen_shown());
    if (!cmd.matched) return false;
    if (cmd.off) {
        Hide();
        if (hooks_.notify) hooks_.notify("Listo");
    } else {
        Show(cmd.kind);
        if (hooks_.notify) {
            hooks_.notify(cmd.kind == SceneKind::Visualizer ? "Visualizador de sonido"
                          : cmd.kind == SceneKind::Aquarium ? "Acuario" : "Lámpara de lava");
        }
    }
    return hooks_.play_clip && hooks_.play_clip("buddy_chime");
}

void ChipoScenes::Step(uint32_t now_ms) {
    if (!running_ || kind_ == SceneKind::None) return;
    if (now_ms - start_ms_ >= kAutoOffMs) {
        Hide();
        return;
    }
    switch (kind_) {
    case SceneKind::Visualizer: UpdateVisualizer(); break;
    case SceneKind::Aquarium: UpdateAquarium(); break;
    case SceneKind::Lava: UpdateLava(); break;
    case SceneKind::None: break;
    }
    last_ms_ = now_ms;
}

void ChipoScenes::UpdateVisualizer() {
    const float dt = std::min(0.1f, std::max(0.001f, static_cast<float>(Now() - last_ms_) / 1000.0f));
    float bands[kBars] = {}, level = 0;
    const bool fresh = hooks_.audio && hooks_.audio(bands, &level);
    bars_.Update(fresh ? bands : nullptr, dt, static_cast<float>(Now() - shown_ms_) / 1000.0f);
    const int x0 = (w_ - (kBars * bar_w_ + (kBars - 1) * bar_gap_)) / 2;
    for (int i = 0; i < kBars; ++i) {
        const int bh = std::max(4, static_cast<int>(bars_.bar[i] * max_bar_h_));
        if (bh != bar_h_[i]) {
            bar_h_[i] = bh;
            lv_obj_set_size(bar_[i], bar_w_, bh);
            lv_obj_set_pos(bar_[i], x0 + i * (bar_w_ + bar_gap_), base_y_ - bh);
            const int cy = base_y_ - std::max(bh + 8, static_cast<int>(bars_.peak[i] * max_bar_h_) + 8);
            lv_obj_set_pos(cap_[i], x0 + i * (bar_w_ + bar_gap_), cy);
        } else {
            const int cy = base_y_ - std::max(bh + 8, static_cast<int>(bars_.peak[i] * max_bar_h_) + 8);
            lv_obj_set_pos(cap_[i], x0 + i * (bar_w_ + bar_gap_), cy);
        }
    }
}

void ChipoScenes::UpdateAquarium() {
    const float dt = std::min(0.1f, std::max(0.001f, static_cast<float>(Now() - last_ms_) / 1000.0f));
    SceneRand rand = [this](int lo, int hi) { return Rand(lo, hi); };
    aquarium_.Update(dt, rand);
    for (int i = 0; i < AquariumModel::kFish; ++i) {
        const Fish& f = aquarium_.fish[i];
        const int bw = static_cast<int>(f.size), bh = static_cast<int>(f.size * 0.6f);
        const int x = static_cast<int>(f.x), y = static_cast<int>(f.y);
        lv_obj_t** p = fish_[i];
        lv_obj_set_style_bg_color(p[0], lv_color_hex(f.rgb), LV_PART_MAIN);
        lv_obj_set_style_bg_color(p[1], lv_color_hex(f.rgb), LV_PART_MAIN);
        lv_obj_set_size(p[0], bw, bh);
        lv_obj_set_pos(p[0], x - bw / 2, y - bh / 2);
        const int tw = bh * 7 / 10;
        lv_obj_set_size(p[1], tw, tw);
        lv_obj_set_pos(p[1], x - f.dir * (bw / 2 + tw / 4) - tw / 2, y - tw / 2);
        lv_obj_set_style_transform_pivot_x(p[1], tw / 2, LV_PART_MAIN);
        lv_obj_set_style_transform_pivot_y(p[1], tw / 2, LV_PART_MAIN);
        lv_obj_set_style_transform_rotation(p[1], 450, LV_PART_MAIN);
        const int ew = std::max(4, bh / 5);
        lv_obj_set_size(p[2], ew, ew);
        lv_obj_set_pos(p[2], x + f.dir * (bw / 4) - ew / 2, y - bh / 6 - ew / 2);
    }
    for (int i = 0; i < AquariumModel::kBubbles; ++i) {
        const Bubble& b = aquarium_.bubble[i];
        const int d = static_cast<int>(b.r * 2);
        lv_obj_set_size(bubble_[i], d, d);
        lv_obj_set_pos(bubble_[i], static_cast<int>(b.x) - d / 2, static_cast<int>(b.y) - d / 2);
    }
    for (int i = 0; i < AquariumModel::kPlants; ++i) {
        lv_obj_set_style_transform_rotation(plant_[i], static_cast<int>(60.0f * std::sin(aquarium_.plant_phase[i])), LV_PART_MAIN);  // +-6 degrees
    }
}

void ChipoScenes::UpdateLava() {
    const float dt = std::min(0.1f, std::max(0.001f, static_cast<float>(Now() - last_ms_) / 1000.0f));
    lava_.Update(dt);
    for (int i = 0; i < LavaModel::kBlobs; ++i) {
        const LavaBlob& b = lava_.blob[i];
        const int d = static_cast<int>(b.r * 2);
        lv_obj_set_size(blob_[i], d, d);
        lv_obj_set_pos(blob_[i], static_cast<int>(b.x) - d / 2, static_cast<int>(b.y) - d / 2);
        // Warm palette: red-orange to magenta, drifting slowly.
        const uint32_t c = Hsv(0.97f + 0.12f * std::sin(b.hue * 6.2831853f), 0.85f, 1.0f);
        lv_obj_set_style_bg_color(blob_[i], lv_color_hex(Lerp(c, 0xFFFFFF, 0.35f)), LV_PART_MAIN);  // lit top...
        lv_obj_set_style_bg_grad_color(blob_[i], lv_color_hex(c), LV_PART_MAIN);                    // ...to the saturated bottom
        lv_obj_set_style_bg_grad_dir(blob_[i], LV_GRAD_DIR_VER, LV_PART_MAIN);
    }
}

ChipoScenes::DebugInfo ChipoScenes::Debug() const {
    DebugInfo d{};
    d.kind = kind_;
    for (int i = 0; i < kBars; ++i) d.bars_px[i] = bar_h_[i];
    for (int i = 0; i < AquariumModel::kFish; ++i) d.fish_dir[i] = aquarium_.fish[i].dir;
    int n = 0;
    if (root_ && !lv_obj_has_flag(root_, LV_OBJ_FLAG_HIDDEN)) {  // a hidden screen shows nothing
        const uint32_t cnt = lv_obj_get_child_count(root_);
        for (uint32_t i = 0; i < cnt; ++i) {
            lv_obj_t* c = lv_obj_get_child(root_, static_cast<int32_t>(i));
            if (c && !lv_obj_has_flag(c, LV_OBJ_FLAG_HIDDEN)) ++n;
        }
    }
    d.visible_objects = n;
    return d;
}

void ChipoScenes::TimerCallback(lv_timer_t* timer) {
    auto* self = static_cast<ChipoScenes*>(lv_timer_get_user_data(timer));
    if (self) self->Step(self->Now());
}

void ChipoScenes::EventCallback(lv_event_t* e) {
    auto* self = static_cast<ChipoScenes*>(lv_event_get_user_data(e));
    if (self) self->OnTouch();
}
