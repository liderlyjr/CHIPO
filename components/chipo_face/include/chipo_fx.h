#pragma once

#include <cstdint>

#include "chipo_fx_logic.h"
#include "lvgl.h"

/**
 * Draws FxEngine particles with a few flat LVGL shapes each (no shadows, no images, no PSRAM use).
 * A full-screen transparent layer above the face. Call from the LVGL task.
 */
class ChipoFx {
public:
    ~ChipoFx() { Stop(); }
    bool Start(lv_obj_t* parent, int32_t width, int32_t height);
    void Stop();
    void Render(const FxEngine& engine);
    bool running() const { return layer_ != nullptr; }
    int visible_slots() const;  // for tests

private:
    static constexpr int kPieces = 3;
    struct Slot {
        lv_obj_t* piece[kPieces] = {};
        bool shown = false;
        FxKind kind = FxKind::Heart;
        int x = -9999, y = -9999, size = -1, alpha = -1;
        uint32_t rgb = 0xFFFFFFFF;
    };
    void Hide(Slot& slot);
    void Draw(Slot& slot, const FxParticle& p, float scale, float alpha);
    lv_obj_t* layer_ = nullptr;
    Slot slots_[FxEngine::kMax];
};
