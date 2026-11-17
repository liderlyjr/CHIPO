#include "chipo_update.h"

#include <cstdio>
#include <cstring>
#include <mutex>

#include "chipo_update_config.h"

bool ChipoParseVersion(const char* text, int out[3]) {
    if (!text) return false;
    if (*text == 'v' || *text == 'V') ++text;
    int parts[3] = {0, 0, 0};
    int idx = 0;
    bool digit_seen = false;
    for (const char* p = text;; ++p) {
        if (*p >= '0' && *p <= '9') {
            parts[idx] = parts[idx] * 10 + (*p - '0');
            if (parts[idx] > 100000) return false;
            digit_seen = true;
        } else if ((*p == '.' || *p == '\0') && digit_seen) {
            if (*p == '\0') break;
            if (++idx > 2) return false;
            digit_seen = false;
        } else {
            return false;  // "-beta", spaces, empty parts...
        }
    }
    for (int i = 0; i < 3; ++i) out[i] = parts[i];
    return true;
}

bool ChipoIsNewer(const char* current, const char* latest) {
    int a[3], b[3];
    if (!ChipoParseVersion(current, a) || !ChipoParseVersion(latest, b)) return false;
    for (int i = 0; i < 3; ++i) {
        if (b[i] != a[i]) return b[i] > a[i];
    }
    return false;
}

bool ChipoParseTagName(const char* json, char* out, size_t cap) {
    if (!json || !out || cap < 2) return false;
    const char* key = strstr(json, "\"tag_name\"");
    if (!key) return false;
    const char* p = key + strlen("\"tag_name\"");
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') ++p;
    if (*p != ':') return false;
    ++p;
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') ++p;
    if (*p != '"') return false;
    ++p;
    size_t n = 0;
    while (*p && *p != '"') {
        if (*p == '\\' || n + 1 >= cap) return false;  // a tag never needs escapes
        out[n++] = *p++;
    }
    if (*p != '"' || n == 0) return false;  // truncated document or empty tag
    out[n] = '\0';
    return true;
}

bool ChipoBuildDownloadUrl(const char* tag, char* out, size_t cap) {
    int v[3];
    if (!tag || !out || !ChipoParseVersion(tag, v)) return false;  // only well-formed tags
    const int n = snprintf(out, cap, "https://github.com/%s/releases/download/%s/%s", CHIPO_UPDATE_REPO, tag,
                           CHIPO_UPDATE_ASSET);
    return n > 0 && static_cast<size_t>(n) < cap;
}

bool ChipoIsTrustedReleaseUrl(const char* url) {
    if (!url) return false;
    char prefix[96];
    snprintf(prefix, sizeof prefix, "https://github.com/%s/releases/", CHIPO_UPDATE_REPO);
    return strncmp(url, prefix, strlen(prefix)) == 0;
}

bool ChipoIsTrustedRedirectUrl(const char* url) {
    if (!url || strncmp(url, "https://", 8) != 0) return false;
    const char* host = url + 8;
    const char* end = host;
    while (*end && *end != '/' && *end != ':' && *end != '?' && *end != '@') ++end;
    if (*end == '@') return false;  // user-info tricks
    const size_t len = static_cast<size_t>(end - host);
    auto ends_with = [&](const char* suffix) {
        const size_t s = strlen(suffix);
        return len >= s && strncmp(host + len - s, suffix, s) == 0;
    };
    auto equals = [&](const char* name) { return len == strlen(name) && strncmp(host, name, len) == 0; };
    return equals("github.com") || ends_with(".githubusercontent.com");
}

namespace {
std::mutex g_mutex;
std::string g_authorized;
}  // namespace

void ChipoUpdateAuthorize(const char* url) {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_authorized = url ? url : "";
}

bool ChipoUpdateIsAuthorized(const char* url) {
    std::lock_guard<std::mutex> lock(g_mutex);
    const bool ok = url && !g_authorized.empty() && g_authorized == url;
    g_authorized.clear();  // one use only
    return ok;
}
