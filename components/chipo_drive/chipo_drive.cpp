#include "chipo_drive.h"

#include <cmath>

#include "chipo_drive_config.h"

#ifdef ESP_PLATFORM
#include "esp_log.h"
#include "esp_random.h"
#else
#include "esp_log.h"  // tests/host/stub
#endif

namespace {
constexpr char TAG[] = "ChipoDrive";
constexpr uint32_t kTimerPeriodMs = 250;
constexpr uint32_t kCalmAfterEventMs = 30000;  // calm remarks only after this long without events

uint32_t RandomSeed() {
#ifdef ESP_PLATFORM
    return esp_random();
#else
    return 0xC0FFEEu;
#endif
}
}  // namespace

ChipoDrive::ChipoDrive()
    : motion_(CHIPO_DRIVE_MIRROR_X != 0), personality_(RandomSeed()) {
    motion_.SetForwardSign(CHIPO_DRIVE_FORWARD_SIGN);
}

ChipoDrive::~ChipoDrive() {
    Stop();
}

uint32_t ChipoDrive::Now() const {
    return hooks_.clock ? hooks_.clock() : lv_tick_get();
}

bool ChipoDrive::Start(ChipoFace* face, Hooks hooks) {
    if (face == nullptr || !face->IsRunning()) return false;
    Stop();
    face_ = face;
    hooks_ = std::move(hooks);
    face_->SetSampleObserver([this](const ImuSample& s, uint32_t t) { OnSample(s, t); });
    timer_ = lv_timer_create(TimerCallback, kTimerPeriodMs, this);
    if (timer_ == nullptr) {
        face_->SetSampleObserver(nullptr);
        face_ = nullptr;
        return false;
    }
    ESP_LOGI(TAG, "CHIPO DRIVE ready (switched on by voice only)");
    return true;
}

void ChipoDrive::Stop() {
    if (timer_) {
        lv_timer_delete(timer_);
        timer_ = nullptr;
    }
    if (face_) {
        face_->SetSampleObserver(nullptr);
        if (active_) {
            face_->SetDriveMode(false);
            face_->SetFastSampling(false);
        }
        face_ = nullptr;
    }
    active_ = false;
}

void ChipoDrive::TimerCallback(lv_timer_t* timer) {
    auto* self = static_cast<ChipoDrive*>(lv_timer_get_user_data(timer));
    if (self) self->Step(self->Now());
}

bool ChipoDrive::PlayOrNotify(const char* clip_id) {
    const bool played = hooks_.play_clip && hooks_.play_clip(clip_id);
    if (!played && hooks_.notify) {
        const char* text = DriveTextForClip(clip_id);
        if (text) hooks_.notify(text);
    }
    return played;
}

void ChipoDrive::Say(const DrivePhrase* phrase) {
    if (phrase) PlayOrNotify(phrase->id);
}

void ChipoDrive::Activate() {
    active_ = true;
    motion_.Reset();
    motion_.SetSensitivity(sensitivity_);
    personality_.SetSensitivity(sensitivity_);
    personality_.Begin(Now());
    last_event_ms_ = Now();
    face_->SetFastSampling(true);
    face_->SetDriveMode(true);  // the sunglasses start sliding down
    ESP_LOGI(TAG, "CHIPO DRIVE on (sensitivity %d)", static_cast<int>(sensitivity_));
}

void ChipoDrive::Deactivate() {
    active_ = false;
    face_->SetDriveLean(0);
    face_->SetDriveMode(false);  // the sunglasses slide off
    face_->SetFastSampling(false);
    ESP_LOGI(TAG, "CHIPO DRIVE off");
}

void ChipoDrive::SetSensitivity(DriveSensitivity s) {
    sensitivity_ = s;
    motion_.SetSensitivity(s);
    personality_.SetSensitivity(s);
}

bool ChipoDrive::OnVoiceText(const char* text) {
    if (face_ == nullptr) return false;
    switch (ParseDriveIntent(text)) {
        case DriveIntent::Activate:
            if (active_) return false;  // already in the car: let the assistant answer normally
            Activate();
            if (hooks_.play_clip && hooks_.play_clip(kDriveClipStart)) return true;  // the recorded line
            // No recorded line: engine sound + text while the glasses slide down; the assistant answers.
            if (hooks_.play_clip) hooks_.play_clip(kDriveClipSfx);
            if (hooks_.notify) hooks_.notify(DriveTextForClip(kDriveClipStart));
            return false;
        case DriveIntent::Deactivate:
            if (!active_) return false;
            Deactivate();
            return PlayOrNotify(kDriveClipEnd);
        case DriveIntent::SensitivitySoft:
            SetSensitivity(DriveSensitivity::Soft);
            return PlayOrNotify(kDriveClipSoft);
        case DriveIntent::SensitivityNormal:
            SetSensitivity(DriveSensitivity::Normal);
            return PlayOrNotify(kDriveClipNormal);
        case DriveIntent::SensitivityIntense:
            SetSensitivity(DriveSensitivity::Intense);
            return PlayOrNotify(kDriveClipIntense);
        default:
            return false;
    }
}

void ChipoDrive::React(DriveEventKind kind, float intensity) {
    const bool busy = hooks_.assistant_busy && hooks_.assistant_busy();
    const DriveReaction r = personality_.OnEvent(kind, intensity, Now(), !busy);
    last_event_ms_ = Now();
    switch (kind) {
        case DriveEventKind::Accelerate: face_->DriveReact(ChipoFace::DriveReaction::Accelerate, intensity); break;
        case DriveEventKind::Brake: face_->DriveReact(ChipoFace::DriveReaction::Brake, intensity); break;
        case DriveEventKind::Bump: face_->DriveReact(ChipoFace::DriveReaction::Bump, intensity); break;
        case DriveEventKind::Abrupt:
            // The personality decides between surprise and dizziness through the intensity.
            face_->DriveReact(ChipoFace::DriveReaction::Abrupt, r.look == DriveLook::Dizzy ? 1.0f : 0.5f);
            break;
        default: break;  // curves: the lean is the reaction
    }
    Say(r.say);
}

void ChipoDrive::OnSample(const ImuSample& sample, uint32_t sample_ms) {
    const DriveOutput& o = motion_.Update(sample, sample_ms);
    if (!active_ || face_ == nullptr) return;
    face_->SetDriveLean(o.lean);
    // One event at a time, the most important first.
    if (o.abrupt) React(DriveEventKind::Abrupt, o.intensity);
    else if (o.bump) React(DriveEventKind::Bump, o.intensity);
    else if (o.brake) React(DriveEventKind::Brake, o.intensity);
    else if (o.accelerate) React(DriveEventKind::Accelerate, o.intensity);
    else if (o.curve_start) React(DriveEventKind::Curve, o.intensity);
#if CHIPO_DRIVE_DEBUG_LOG
    const uint32_t now = Now();
    if (now >= next_log_ms_) {
        next_log_ms_ = now + 1000;
        ESP_LOGI(TAG, "mounted=%d fwd_known=%d yaw=%.1f lat=%.2f fwd=%.2f vert=%.2f curve=%d",
                 o.mounted, o.forward_known, o.yaw_dps, o.lateral_g, o.forward_g, o.vertical_g, o.in_curve);
    }
#endif
}

void ChipoDrive::Step(uint32_t now_ms) {
    if (!active_ || face_ == nullptr) return;
    const bool busy = hooks_.assistant_busy && hooks_.assistant_busy();
    const bool calm = now_ms - last_event_ms_ > kCalmAfterEventMs && !motion_.output().in_curve;
    const DriveReaction r = personality_.Tick(now_ms, calm, !busy);
    Say(r.say);
}
