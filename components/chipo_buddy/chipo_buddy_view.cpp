#include "chipo_buddy_view.h"

#include <cstring>

namespace {

constexpr int kDigitW = 62, kDigitH = 124, kThick = 11, kGap = 12, kColonW = 26;  // portrait sizes
// Segments: 0 top, 1 top-right, 2 bottom-right, 3 bottom, 4 bottom-left, 5 top-left, 6 middle.
constexpr uint8_t kMask[10] = {0b0111111, 0b0000110, 0b1011011, 0b1001111, 0b1100110,
                               0b1101101, 0b1111101, 0b0000111, 0b1111111, 0b1101111};

uint8_t MaskOf(char c) { return c >= '0' && c <= '9' ? kMask[c - '0'] : 0; }

lv_obj_t* Rect(lv_obj_t* parent, int x, int y, int w, int h, int radius) {
    lv_obj_t* o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, w, h);
    lv_obj_set_style_radius(o, radius, 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_TRANSP, 0);
    lv_obj_remove_flag(o, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
    return o;
}

// AMOLED look of one bar: a glowing tube (pale hot core on top fading to the saturated colour) with a soft halo
// when lit; unlit bars stay faintly visible, like the dark bars of a real display, with no halo.
void StyleLit(lv_obj_t* o, uint32_t rgb, int brightness, bool lit) {
    const lv_color_t color = lv_color_hex(rgb);
    lv_obj_set_style_bg_color(o, lit ? lv_color_mix(lv_color_white(), color, 70) : color, 0);
    lv_obj_set_style_bg_grad_color(o, color, 0);
    lv_obj_set_style_bg_grad_dir(o, lit ? LV_GRAD_DIR_VER : LV_GRAD_DIR_NONE, 0);
    lv_obj_set_style_bg_opa(o, static_cast<lv_opa_t>(lit ? brightness : brightness / 14), 0);
    lv_obj_set_style_shadow_color(o, color, 0);
    lv_obj_set_style_shadow_width(o, lit ? 22 : 0, 0);
    lv_obj_set_style_shadow_opa(o, static_cast<lv_opa_t>(lit ? brightness * 3 / 5 : 0), 0);
}

lv_obj_t* Text(lv_obj_t* parent, const lv_font_t* font) {
    lv_obj_t* l = lv_label_create(parent);
    lv_label_set_text(l, "");
    if (font) lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_remove_flag(l, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
    return l;
}

}  // namespace

bool ChipoBuddyView::Create(lv_obj_t* parent, const lv_font_t* font) {
    if (root_ || !parent) return false;
    int32_t w = lv_obj_get_width(parent), h = lv_obj_get_height(parent);
    if (w <= 0) w = 480;
    if (h <= 0) h = 480;
    screen_w_ = static_cast<int>(w);
    screen_h_ = static_cast<int>(h);
    root_ = lv_obj_create(parent);
    if (!root_) return false;
    lv_obj_remove_style_all(root_);
    lv_obj_set_size(root_, w, h);
    lv_obj_set_pos(root_, 0, 0);
    lv_obj_set_style_bg_color(root_, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(root_, LV_OPA_COVER, 0);
    lv_obj_remove_flag(root_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(root_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(root_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(root_, EventCallback, LV_EVENT_CLICKED, this);
    lv_obj_add_event_cb(root_, EventCallback, LV_EVENT_LONG_PRESSED, this);

    for (auto& row : seg_) for (auto& o : row) o = Rect(root_, 0, 0, 1, 1, 1);
    for (auto& o : dot_) o = Rect(root_, 0, 0, 1, 1, 1);
    caption_ = Text(root_, font);
    lv_obj_set_width(caption_, w - 40);
    footer_ = Text(root_, font);
    lv_obj_set_width(footer_, w - 40);
    BuildGeometry();
    return true;
}

// Digits are laid out on a logical canvas: the screen itself (portrait) or the screen turned on its side
// (landscape, bigger digits). Rectangles stay rectangles when turned by 90 degrees, so no rotated layer
// is needed: only the position and size of each segment change.
void ChipoBuddyView::BuildGeometry() {
    const bool turned = rotation_ != 0;
    const int lw = turned ? screen_h_ : screen_w_, lh = turned ? screen_w_ : screen_h_;
    const float sc = turned ? 1.3f : 1.0f;
    const int dw = static_cast<int>(kDigitW * sc), dh = static_cast<int>(kDigitH * sc), th = static_cast<int>(kThick * sc);
    const int gap = static_cast<int>(kGap * sc), colon_w = static_cast<int>(kColonW * sc);
    const int total = 4 * dw + 2 * gap + colon_w + 2 * gap;
    const int x0 = (lw - total) / 2;
    const int y0 = turned ? (lh - dh) / 2 : lh / 2 - dh / 2 - 6;
    const int hh = (dh - 3 * th) / 2;
    int x = x0;
    auto put = [&](lv_obj_t* o, int lx, int ly, int w, int h, int radius) {
        int sx = lx, sy = ly, sw = w, sh = h;
        if (rotation_ == 1) {  // turned clockwise: the screen's right edge is now up
            sx = screen_w_ - ly - h; sy = lx; sw = h; sh = w;
        } else if (rotation_ == 2) {  // turned counter-clockwise
            sx = ly; sy = screen_h_ - lx - w; sw = h; sh = w;
        }
        lv_obj_set_pos(o, sx, sy);
        lv_obj_set_size(o, sw, sh);
        lv_obj_set_style_radius(o, radius, 0);
    };
    for (int d = 0; d < 4; ++d) {
        if (d == 2) x += colon_w + 2 * gap - gap;
        put(seg_[d][0], x + th / 2, y0, dw - th, th, th / 2);
        put(seg_[d][6], x + th / 2, y0 + th + hh, dw - th, th, th / 2);
        put(seg_[d][3], x + th / 2, y0 + dh - th, dw - th, th, th / 2);
        put(seg_[d][5], x, y0 + th / 2 + 2, th, hh + th - 4, th / 2);
        put(seg_[d][1], x + dw - th, y0 + th / 2 + 2, th, hh + th - 4, th / 2);
        put(seg_[d][4], x, y0 + th + hh + th / 2 + 2, th, hh + th - 4, th / 2);
        put(seg_[d][2], x + dw - th, y0 + th + hh + th / 2 + 2, th, hh + th - 4, th / 2);
        x += dw + gap;
        if (d == 1) {
            const int cx = x - gap + (colon_w + 2 * gap - gap) / 2 - 6;
            const int ds = static_cast<int>(12 * sc);
            put(dot_[0], cx, y0 + dh / 3 - ds / 2, ds, ds, ds / 2);
            put(dot_[1], cx, y0 + 2 * dh / 3 - ds / 2, ds, ds, ds / 2);
        }
    }
    if (turned) {
        lv_obj_add_flag(caption_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(footer_, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_remove_flag(caption_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(footer_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_align(caption_, LV_ALIGN_TOP_MID, 0, y0 - 70);
        lv_obj_align(footer_, LV_ALIGN_TOP_MID, 0, y0 + dh + 34);
    }
}

void ChipoBuddyView::SetRotation(int rotation) {
    if (rotation < 0 || rotation > 2) rotation = 0;
    if (root_ && fx_ == Fx::Out) {  // do not turn the digits that are fading out
        pending_rotation_ = rotation == rotation_ ? -1 : rotation;
        return;
    }
    if (!root_ || rotation == rotation_) return;
    rotation_ = rotation;
    BuildGeometry();
}

void ChipoBuddyView::Destroy() {
    if (fx_timer_) { lv_timer_delete(fx_timer_); fx_timer_ = nullptr; }
    fx_ = Fx::None;
    pending_rotation_ = -1;
    if (root_) lv_obj_delete(root_);
    root_ = caption_ = footer_ = nullptr;
    for (auto& row : seg_) for (auto& s : row) s = nullptr;
    dot_[0] = dot_[1] = nullptr;
    shown_ = false;
    have_applied_ = false;
    rotation_ = 0;
}

void ChipoBuddyView::Show(const BuddyScreen& s) {
    if (!root_) return;
    if (!shown_) {
        const bool was_leaving = fx_ == Fx::Out;
        lv_obj_remove_flag(root_, LV_OBJ_FLAG_HIDDEN);
        shown_ = true;
        have_applied_ = false;
        if (was_leaving) {  // came back mid fade-out: just show it (turned the way it was last asked)
            StartFx(Fx::None);
            if (pending_rotation_ >= 0) { rotation_ = pending_rotation_; BuildGeometry(); }
            pending_rotation_ = -1;
        }
        else StartFx(Fx::In);
    }
    const bool restyle = !have_applied_ || s.color != applied_.color || s.brightness != applied_.brightness;
    for (int d = 0; d < 4; ++d) {
        if (!restyle && s.digits[d] == applied_.digits[d]) continue;
        const uint8_t mask = MaskOf(s.digits[d]);
        for (int k = 0; k < 7; ++k) {
            const bool lit = ((mask >> k) & 1) != 0;
            StyleLit(seg_[d][k], s.color, s.brightness, lit);
        }
    }
    if (restyle || s.colon != applied_.colon) {
        for (lv_obj_t* dot : dot_) {
            StyleLit(dot, s.color, s.brightness, s.colon);
        }
    }
    if (!have_applied_ || std::strcmp(s.caption, applied_.caption) != 0 || restyle) {
        lv_label_set_text(caption_, s.caption);
        lv_obj_set_style_text_color(caption_, lv_color_hex(s.color), 0);
        lv_obj_set_style_text_opa(caption_, static_cast<lv_opa_t>(s.brightness), 0);
    }
    if (!have_applied_ || std::strcmp(s.footer, applied_.footer) != 0 || restyle) {
        lv_label_set_text(footer_, s.footer);
        lv_obj_set_style_text_color(footer_, lv_color_hex(0xB8B8B8), 0);
        lv_obj_set_style_text_opa(footer_, static_cast<lv_opa_t>(s.brightness * 3 / 4), 0);
    }
    applied_ = s;
    // The caption / footer pointers may be temporaries: keep nothing that points outside.
    applied_.caption = "";
    applied_.footer = "";
    caption_text_.assign(s.caption);
    footer_text_.assign(s.footer);
    applied_.caption = caption_text_.c_str();
    applied_.footer = footer_text_.c_str();
    have_applied_ = true;
}

void ChipoBuddyView::Hide() {
    if (root_ && shown_) StartFx(Fx::Out);  // hidden for real when the switch-off animation ends
    shown_ = false;
    have_applied_ = false;
}

namespace {
constexpr uint32_t kFxTickMs = 30;
constexpr int kInDigitMs = 110, kInSegMs = 22, kInPieceMs = 170;   // appear: per digit, per segment, each piece
constexpr int kOutDigitMs = 70, kOutSegMs = 14, kOutPieceMs = 140; // disappear
constexpr int kSegRank[7] = {0, 1, 3, 6, 4, 2, 5};  // a, b, c, d, e, f, g: top first, then round the digit
// A neon tube striking: quick rise, a dip (flicker), then full.
int StrikeOpa(int t) {
    if (t <= 0) return 0;
    if (t < 45) return 255 * t / 45;
    if (t < 80) return 90;
    if (t < kInPieceMs) return 90 + 165 * (t - 80) / (kInPieceMs - 80);
    return 255;
}
}  // namespace

void ChipoBuddyView::SetPieceOpa(lv_obj_t* o, int opa) {
    if (o) lv_obj_set_style_opa(o, static_cast<lv_opa_t>(opa < 0 ? 0 : (opa > 255 ? 255 : opa)), LV_PART_MAIN);
}

void ChipoBuddyView::StartFx(Fx fx) {
    fx_ = fx;
    fx_t0_ = lv_tick_get();
    if (fx == Fx::None) {
        if (fx_timer_) { lv_timer_delete(fx_timer_); fx_timer_ = nullptr; }
        for (auto& row : seg_) for (auto* o : row) SetPieceOpa(o, 255);
        for (auto* o : dot_) SetPieceOpa(o, 255);
        SetPieceOpa(caption_, 255);
        SetPieceOpa(footer_, 255);
        return;
    }
    if (!fx_timer_) fx_timer_ = lv_timer_create(FxTimerCallback, kFxTickMs, this);
    StepFx();
}

void ChipoBuddyView::FxTimerCallback(lv_timer_t* t) {
    auto* self = static_cast<ChipoBuddyView*>(lv_timer_get_user_data(t));
    if (self) self->StepFx();
}

void ChipoBuddyView::StepFx() {
    if (!root_ || fx_ == Fx::None) return;
    const int t = static_cast<int>(lv_tick_get() - fx_t0_);
    int last = 0;
    if (fx_ == Fx::In) {
        for (int d = 0; d < 4; ++d)
            for (int k = 0; k < 7; ++k) {
                const int delay = d * kInDigitMs + kSegRank[k] * kInSegMs;
                SetPieceOpa(seg_[d][k], StrikeOpa(t - delay));
                last = delay + kInPieceMs > last ? delay + kInPieceMs : last;
            }
        for (auto* o : dot_) SetPieceOpa(o, StrikeOpa(t - (2 * kInDigitMs - 40)));
        SetPieceOpa(caption_, 255 * (t > 300 ? 300 : t) / 300);
        SetPieceOpa(footer_, 255 * (t < 200 ? 0 : (t > 500 ? 300 : t - 200)) / 300);
        if (t >= last && t >= 500) StartFx(Fx::None);
        return;
    }
    // Out: right-most digit first, each segment fading in reverse order; the colon goes last.
    for (int d = 0; d < 4; ++d)
        for (int k = 0; k < 7; ++k) {
            const int delay = (3 - d) * kOutDigitMs + (6 - kSegRank[k]) * kOutSegMs;
            const int u = t - delay;
            SetPieceOpa(seg_[d][k], u <= 0 ? 255 : 255 - 255 * (u > kOutPieceMs ? kOutPieceMs : u) / kOutPieceMs);
            last = delay + kOutPieceMs > last ? delay + kOutPieceMs : last;
        }
    const int colon_delay = 4 * kOutDigitMs;
    const int uc = t - colon_delay;
    for (auto* o : dot_) SetPieceOpa(o, uc <= 0 ? 255 : 255 - 255 * (uc > kOutPieceMs ? kOutPieceMs : uc) / kOutPieceMs);
    SetPieceOpa(caption_, 255 - 255 * (t > 200 ? 200 : t) / 200);
    SetPieceOpa(footer_, 255 - 255 * (t > 200 ? 200 : t) / 200);
    if (t >= last && t >= colon_delay + kOutPieceMs) {
        lv_obj_add_flag(root_, LV_OBJ_FLAG_HIDDEN);
        StartFx(Fx::None);  // back to full opacity for the next time it shows
        if (pending_rotation_ >= 0) { rotation_ = pending_rotation_; BuildGeometry(); }
        pending_rotation_ = -1;
    }
}

void ChipoBuddyView::EventCallback(lv_event_t* e) {
    auto* self = static_cast<ChipoBuddyView*>(lv_event_get_user_data(e));
    if (self && self->touch_) self->touch_(lv_event_get_code(e) == LV_EVENT_LONG_PRESSED);
}

ChipoBuddyView::DebugInfo ChipoBuddyView::Debug() const {
    DebugInfo d{};
    d.shown = shown_;
    d.color = applied_.color;
    d.rotation = rotation_;
    d.brightness = have_applied_ ? applied_.brightness : 0;
    d.fx = static_cast<int>(fx_);
    d.root_visible = root_ && !lv_obj_has_flag(root_, LV_OBJ_FLAG_HIDDEN);
    d.colon_on = have_applied_ && applied_.colon;
    for (int i = 0; i < 4; ++i) {
        d.digits[i] = have_applied_ ? applied_.digits[i] : ' ';
        for (int k = 0; k < 7; ++k) d.lit_segments += have_applied_ && ((MaskOf(applied_.digits[i]) >> k) & 1);
    }
    d.digits[4] = 0;
    return d;
}
