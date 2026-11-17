#pragma once
// CHIPO self-update from GitHub Releases. Everything here can be overridden at build time (-D).

#ifndef CHIPO_UPDATE_ENABLED
#define CHIPO_UPDATE_ENABLED 1  // 0 = never look for updates
#endif
#ifndef CHIPO_UPDATE_REPO
#define CHIPO_UPDATE_REPO "liderlyjr/CHIPO"  // owner/name; only this repository is trusted
#endif
#ifndef CHIPO_UPDATE_ASSET
#define CHIPO_UPDATE_ASSET "chipo-os.bin"  // file attached to every Release (the app image)
#endif
#ifndef CHIPO_UPDATE_AUTO_INSTALL
#define CHIPO_UPDATE_AUTO_INSTALL 1  // 1 = install by itself when CHIPO is idle, 0 = only log it
#endif
#ifndef CHIPO_UPDATE_FIRST_CHECK_MS
#define CHIPO_UPDATE_FIRST_CHECK_MS 90000  // first look 90 s after boot (Wi-Fi/activation settle)
#endif
#ifndef CHIPO_UPDATE_INTERVAL_MS
#define CHIPO_UPDATE_INTERVAL_MS (6UL * 60UL * 60UL * 1000UL)  // then every 6 hours
#endif
#ifndef CHIPO_UPDATE_IDLE_WAIT_MS
#define CHIPO_UPDATE_IDLE_WAIT_MS (10UL * 60UL * 1000UL)  // wait up to 10 min for CHIPO to be idle
#endif
#ifndef CHIPO_UPDATE_MAX_TRIES
#define CHIPO_UPDATE_MAX_TRIES 3  // attempts per release and per boot before giving up
#endif
#ifndef CHIPO_UPDATE_HTTP_TIMEOUT_MS
#define CHIPO_UPDATE_HTTP_TIMEOUT_MS 15000
#endif
