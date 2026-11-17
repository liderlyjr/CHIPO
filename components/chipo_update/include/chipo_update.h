#pragma once
// CHIPO self-update: portable logic (version compare, release parsing, URL trust) plus the
// ESP-only updater task. The portable part is unit-tested on the host.
#include <cstddef>
#include <functional>
#include <string>

// "v0.6.0" / "0.6.0" / "0.6" -> {0,6,0}. Anything else (pre-release suffix, junk) is rejected.
bool ChipoParseVersion(const char* text, int out[3]);
// true only when `latest` parses and is strictly greater than `current`.
bool ChipoIsNewer(const char* current, const char* latest);
// Pulls "tag_name" out of the GitHub "latest release" JSON (only the start of it is needed).
bool ChipoParseTagName(const char* json, char* out, size_t cap);
// https://github.com/<repo>/releases/download/<tag>/<asset>
bool ChipoBuildDownloadUrl(const char* tag, char* out, size_t cap);
// The GitHub URL must be https and belong to CHIPO_UPDATE_REPO's releases.
bool ChipoIsTrustedReleaseUrl(const char* url);
// The redirect target GitHub gives us must be https on a GitHub-owned host.
bool ChipoIsTrustedRedirectUrl(const char* url);

// Only an address CHIPO itself authorised may be flashed. XiaoZhi's cloud (OTA answer, MCP tool
// "self.upgrade_firmware") can never authorise one, so it cannot replace CHIPO's firmware.
void ChipoUpdateAuthorize(const char* url);
bool ChipoUpdateIsAuthorized(const char* url);  // one-shot: consumes the authorisation

struct ChipoUpdaterHooks {
    std::function<bool()> is_idle;                                              // safe to reboot now?
    std::function<void(const std::string& url, const std::string& tag)> install;  // starts the flash
};

#ifdef ESP_PLATFORM
// Background task: looks for a newer Release now and then. Never touches LVGL.
bool ChipoUpdaterStart(ChipoUpdaterHooks hooks);
#endif
