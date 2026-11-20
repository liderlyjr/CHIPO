#pragma once

#include <string>

/**
 * Music through a Home Assistant media_player (pure logic, no ESP-IDF, unit tested on a PC).
 *
 * Only documented media_player services are used, all as POST <base>/api/services/media_player/<service> with
 * {"entity_id": "<media_player.x>"}: media_play, media_pause, media_next_track, media_previous_track,
 * volume_up, volume_down. CHIPO itself plays no music: the speaker that plays is the one Home Assistant controls.
 */
enum class HaMediaAction { None, Play, Pause, Next, Previous, VolumeUp, VolumeDown };

struct HaMediaCommand {
    HaMediaAction action = HaMediaAction::None;
    const char* service = "";  // Home Assistant service name
    const char* ok_text = "";  // shown when it worked
};

// "pon música", "pausa la música", "siguiente canción", "sube el volumen de la música"...
// A music word (música, canción, tema, reproductor) is always required, so "sube el volumen" stays CHIPO's own.
HaMediaCommand ParseHaMediaCommand(const char* text);

// true for "media_player.salon"
bool HaValidMediaPlayer(const std::string& entity_id);
