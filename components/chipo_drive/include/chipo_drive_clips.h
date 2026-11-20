#pragma once

#include <string_view>

// The embedded OGG/Opus bytes of the clip called `id` (file components/chipo_drive/clips/<id>.ogg),
// or an empty view if that clip was not recorded / not embedded.
std::string_view ChipoDriveClip(const char* id);
