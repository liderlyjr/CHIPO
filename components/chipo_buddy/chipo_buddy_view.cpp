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
    if (w <= 0) w = 368;
    if (h <= 0) h = 448;
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
    if (!root_ || rotation == rotation_) return;
    rotation_ = rotation;
    BuildGeometry();
}

void ChipoBuddyView::Destroy() {
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
        lv_obj_remove_flag(root_, LV_OBJ_FLAG_HIDDEN);
        shown_ = true;
        have_applied_ = false;
    }
    const bool restyle = !have_applied_ || s.color != applied_.color || s.brightness != applied_.brightness;
    for (int d = 0; d < 4; ++d) {
        if (!restyle && s.digits[d] == applied_.digits[d]) continue;
        const uint8_t mask = MaskOf(s.digits[d]);
        for (int k = 0; k < 7; ++k) {
            lv_obj_set_style_bg_color(seg_[d][k], lv_color_hex(s.color), 0);
            // Off segments stay faintly visible, like the unlit bars of a real display.
            const int on = s.brightness, off = s.brightness / 14;
            lv_obj_set_style_bg_opa(seg_[d][k], static_cast<lv_opa_t>((mask >> k) & 1 ? on : off), 0);
        }
    }
    if (restyle || s.colon != applied_.colon) {
        for (lv_obj_t* dot : dot_) {
            lv_obj_set_style_bg_color(dot, lv_color_hex(s.color), 0);
            lv_obj_set_style_bg_opa(dot, static_cast<lv_opa_t>(s.colon ? s.brightness : s.brightness / 14), 0);
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
    if (root_ && shown_) lv_obj_add_flag(root_, LV_OBJ_FLAG_HIDDEN);
    shown_ = false;
    have_applied_ = false;
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
    d.colon_on = have_applied_ && applied_.colon;
    for (int i = 0; i < 4; ++i) {
        d.digits[i] = have_applied_ ? applied_.digits[i] : ' ';
        for (int k = 0; k < 7; ++k) d.lit_segments += have_applied_ && ((MaskOf(applied_.digits[i]) >> k) & 1);
    }
    d.digits[4] = 0;
    return d;
}
