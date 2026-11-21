#include "chipo_fx.h"

#include <algorithm>
#include <cmath>

namespace {
void Flat(lv_obj_t* o) {
    lv_obj_remove_style_all(o);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(o, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(o, 0, LV_PART_MAIN);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
}
void Rect(lv_obj_t* o, int x, int y, int w, int h, int radius) {
    lv_obj_set_size(o, std::max(1, w), std::max(1, h));
    lv_obj_set_pos(o, x, y);
    lv_obj_set_style_radius(o, radius, LV_PART_MAIN);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_HIDDEN);
}
void Turn(lv_obj_t* o, int w, int h, int tenth_degrees) {
    lv_obj_set_style_transform_pivot_x(o, w / 2, LV_PART_MAIN);
    lv_obj_set_style_transform_pivot_y(o, h / 2, LV_PART_MAIN);
    lv_obj_set_style_transform_rotation(o, tenth_degrees, LV_PART_MAIN);
}
}  // namespace

bool ChipoFx::Start(lv_obj_t* parent, int32_t width, int32_t height) {
    if (!parent) return false;
    if (layer_) return true;
    layer_ = lv_obj_create(parent);
    if (!layer_) return false;
    lv_obj_remove_style_all(layer_);
    lv_obj_set_size(layer_, width > 0 ? width : 480, height > 0 ? height : 480);
    lv_obj_set_pos(layer_, 0, 0);
    lv_obj_remove_flag(layer_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(layer_, LV_OBJ_FLAG_SCROLLABLE);
    for (Slot& s : slots_) {
        s = Slot{};
        for (auto& o : s.piece) {
            o = lv_obj_create(layer_);
            if (!o) {
                Stop();
                return false;
            }
            Flat(o);
        }
    }
    return true;
}

void ChipoFx::Stop() {
    if (layer_) lv_obj_delete(layer_);
    layer_ = nullptr;
    for (Slot& s : slots_) s = Slot{};
}

void ChipoFx::Hide(Slot& slot) {
    if (!slot.shown) return;
    for (auto* o : slot.piece) lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
    slot.shown = false;
    slot.alpha = -1;
}

int ChipoFx::visible_slots() const {
    int n = 0;
    for (const Slot& s : slots_) n += s.shown ? 1 : 0;
    return n;
}

void ChipoFx::Render(const FxEngine& engine) {
    if (!layer_) return;
    for (int i = 0; i < FxEngine::kMax; ++i) {
        Slot& slot = slots_[i];
        const FxParticle& p = engine.at(i);
        const float alpha = FxEngine::Alpha(p);
        if (!p.alive || alpha <= 0.02f) {
            Hide(slot);
            continue;
        }
        Draw(slot, p, FxEngine::Scale(p), alpha);
    }
}

void ChipoFx::Draw(Slot& slot, const FxParticle& p, float scale, float alpha) {
    const int x = static_cast<int>(std::lround(p.x));
    const int y = static_cast<int>(std::lround(p.y));
    const int size = std::max(4, static_cast<int>(std::lround(p.size * scale)));
    const int a8 = std::min(255, static_cast<int>(alpha * 255.0f) & ~15);  // steps of 16: fewer repaints
    if (slot.shown && slot.kind == p.kind && slot.x == x && slot.y == y && slot.size == size &&
        slot.alpha == a8 && slot.rgb == p.rgb) {
        return;
    }
    const bool restyle = !slot.shown || slot.kind != p.kind || slot.rgb != p.rgb || slot.alpha != a8;
    const lv_color_t color = lv_color_hex(p.rgb);
    lv_obj_t** o = slot.piece;
    if (restyle) {
        for (int k = 0; k < kPieces; ++k) {
            lv_obj_set_style_bg_color(o[k], color, LV_PART_MAIN);
            lv_obj_set_style_bg_opa(o[k], static_cast<lv_opa_t>(a8), LV_PART_MAIN);
        }
        // A sparkle's core is white.
        if (p.kind == FxKind::Sparkle) lv_obj_set_style_bg_color(o[2], lv_color_white(), LV_PART_MAIN);
    }
    // Pieces that a shape does not use stay hidden.
    for (int k = 0; k < kPieces; ++k) lv_obj_add_flag(o[k], LV_OBJ_FLAG_HIDDEN);

    switch (p.kind) {
    case FxKind::Heart: {
        // A diamond (square turned 45 degrees) with two circles on its upper edges.
        const int q = std::max(4, static_cast<int>(size * 0.62f));
        const int half_diag = static_cast<int>(q * 0.7071f);
        const int cy = y + static_cast<int>(size * 0.12f);
        Rect(o[0], x - half_diag / 2 - q / 2, cy - half_diag / 2 - q / 2, q, q, LV_RADIUS_CIRCLE);
        Rect(o[1], x + half_diag / 2 - q / 2, cy - half_diag / 2 - q / 2, q, q, LV_RADIUS_CIRCLE);
        Rect(o[2], x - q / 2, cy - q / 2, q, q, 2);
        Turn(o[2], q, q, 450);
        break;
    }
    case FxKind::Sparkle: {
        const int t = std::max(2, size / 6);
        Rect(o[0], x - size / 2, y - t / 2, size, t, t / 2);
        Rect(o[1], x - t / 2, y - size / 2, t, size, t / 2);
        const int core = std::max(3, size / 3);
        Rect(o[2], x - core / 2, y - core / 2, core, core, LV_RADIUS_CIRCLE);
        break;
    }
    case FxKind::Tear: {
        const int w = std::max(5, size * 6 / 10);
        Rect(o[0], x - w / 2, y - size / 2, w, size, LV_RADIUS_CIRCLE);
        break;
    }
    case FxKind::Zzz: {
        const int t = std::max(2, size / 5);
        Rect(o[0], x - size / 2, y - size / 2, size, t, 1);
        Rect(o[2], x - size / 2, y + size / 2 - t, size, t, 1);
        const int len = static_cast<int>(size * 1.38f);
        Rect(o[1], x - len / 2, y - t / 2, len, t, 1);
        Turn(o[1], len, t, 3150);  // -45 degrees: from the top right corner to the bottom left one
        break;
    }
    }
    slot.shown = true;
    slot.kind = p.kind;
    slot.x = x;
    slot.y = y;
    slot.size = size;
    slot.alpha = a8;
    slot.rgb = p.rgb;
}
