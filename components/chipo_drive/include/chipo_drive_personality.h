#pragma once

#include <cstdint>

#include "chipo_drive_types.h"

/**
 * What CHIPO says and how it looks when something happens in the car. No ESP-IDF, no LVGL.
 *
 * Phrases are pre-recorded clips (CHIPO has no voice synthesiser of its own: the voice comes from
 * the server). Each phrase has an id = the clip file name without ".ogg"
 * (components/chipo_drive/clips/<id>.ogg). If a clip is missing the text is shown instead.
 *
 * CHIPO must not talk all the time: a minimum silence between comments (by sensitivity), a
 * probability per kind of event, a cap per 5 minutes, no repeating the last phrases of a kind,
 * and no comment while the assistant is listening, thinking or speaking.
 */
struct DrivePhrase {
    const char* id;    // clip name
    const char* text;  // what the clip says (also the on-screen fallback)
    DriveEventKind kind;
};

// Fixed lines (not random comments).
// Full spoken line for the activation ("Brum, brum… ¡vámonos!"). If it is not recorded, the engine
// sound below plays and the assistant's own voice answers.
constexpr const char* kDriveClipStart = "drive_start";
constexpr const char* kDriveClipSfx = "drive_start_sfx";   // engine revving: "brum, brum"
constexpr const char* kDriveClipEnd = "drive_end";
constexpr const char* kDriveClipSoft = "drive_sens_soft";
constexpr const char* kDriveClipNormal = "drive_sens_normal";
constexpr const char* kDriveClipIntense = "drive_sens_intense";

const DrivePhrase* DrivePhrases(int* count);
// Text of a fixed clip id or of a comment id (nullptr if unknown).
const char* DriveTextForClip(const char* id);

// How the face should look for a moment.
enum class DriveLook : uint8_t { None, Excited, Surprised, Dizzy };

struct DriveReaction {
    DriveLook look = DriveLook::None;
    const DrivePhrase* say = nullptr;  // nullptr: stay quiet
};

class DrivePersonality {
public:
    explicit DrivePersonality(uint32_t seed = 0x1234567u) : rng_(seed ? seed : 1u) {}

    void SetSensitivity(DriveSensitivity s) { sens_ = s; }
    // A new session starts: the first comment comes no sooner than a little while after.
    void Begin(uint32_t now_ms);

    // `can_speak` is false while the assistant is busy (listening, thinking, speaking).
    DriveReaction OnEvent(DriveEventKind kind, float intensity, uint32_t now_ms, bool can_speak);
    // Now and then, when nothing has happened for a while: a calm remark ("Jefe, vamos finos").
    DriveReaction Tick(uint32_t now_ms, bool calm, bool can_speak);

    uint32_t last_speech_ms() const { return last_speech_ms_; }

private:
    uint32_t Rand();
    float Rand01();
    const DrivePhrase* Pick(DriveEventKind kind);
    bool MaySpeak(uint32_t now_ms, bool can_speak) const;
    void Spoke(uint32_t now_ms);

    uint32_t rng_;
    DriveSensitivity sens_ = DriveSensitivity::Normal;
    uint32_t begin_ms_ = 0;
    bool begun_ = false;
    uint32_t last_speech_ms_ = 0;
    bool spoke_ever_ = false;
    uint32_t next_calm_ms_ = 0;
    uint32_t recent_ms_[4] = {0, 0, 0, 0};  // times of the last comments (cap per 5 minutes)
    int recent_head_ = 0;
    const DrivePhrase* last_[6][2] = {};     // last two phrases of each kind
};
