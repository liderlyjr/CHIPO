#include <cstdio>
#include <cstring>

#include "chipo_ha_media.h"

static int g_fail = 0;
static void Is(const char* text, HaMediaAction want, const char* service = nullptr) {
    const HaMediaCommand c = ParseHaMediaCommand(text);
    if (c.action != want || (service && std::strcmp(c.service, service) != 0)) {
        std::printf("FAIL \"%s\" -> %d '%s' (wanted %d '%s')\n", text ? text : "(null)", static_cast<int>(c.action), c.service, static_cast<int>(want), service ? service : "");
        ++g_fail;
    }
}

int main() {
    Is("Chipo, pon música", HaMediaAction::Play, "media_play");
    Is("reproduce la música", HaMediaAction::Play, "media_play");
    Is("continúa la música", HaMediaAction::Play, "media_play");
    Is("pausa la música", HaMediaAction::Pause, "media_pause");
    Is("para la música", HaMediaAction::Pause, "media_pause");
    Is("quita la música", HaMediaAction::Pause, "media_pause");
    Is("siguiente canción", HaMediaAction::Next, "media_next_track");
    Is("salta esta canción", HaMediaAction::Next, "media_next_track");
    Is("canción anterior", HaMediaAction::Previous, "media_previous_track");
    Is("vuelve a la canción anterior", HaMediaAction::Previous, "media_previous_track");
    Is("sube el volumen de la música", HaMediaAction::VolumeUp, "volume_up");
    Is("baja el volumen de la música", HaMediaAction::VolumeDown, "volume_down");
    Is("más volumen a la música", HaMediaAction::VolumeUp, "volume_up");
    // CHIPO's own volume, weather, ordinary talk, and other modules are not ours.
    Is("sube el volumen", HaMediaAction::None);
    Is("baja el volumen", HaMediaAction::None);
    Is("pausa", HaMediaAction::None);
    Is("para", HaMediaAction::None);
    Is("qué tiempo hace", HaMediaAction::None);
    Is("me gusta la música clásica de los años ochenta y noventa", HaMediaAction::None);
    Is("activa la escena música", HaMediaAction::None);
    Is("", HaMediaAction::None);
    Is(nullptr, HaMediaAction::None);
    if (!HaValidMediaPlayer("media_player.salon") || HaValidMediaPlayer("light.salon") || HaValidMediaPlayer("media_player.") || HaValidMediaPlayer("media_player.Salon")) {
        std::printf("FAIL media player validation\n");
        ++g_fail;
    }
    if (g_fail == 0) std::printf("ALL HA MEDIA TESTS PASSED\n");
    return g_fail == 0 ? 0 : 1;
}
