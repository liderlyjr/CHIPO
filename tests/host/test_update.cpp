#include <cstdio>
#include <cstring>
#include "chipo_update.h"

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); ++g_fail; } } while (0)

int main() {
    int v[3];
    CHECK(ChipoParseVersion("v0.6.0", v) && v[0] == 0 && v[1] == 6 && v[2] == 0);
    CHECK(ChipoParseVersion("1.2", v) && v[0] == 1 && v[1] == 2 && v[2] == 0);
    CHECK(!ChipoParseVersion("0.6.0-beta", v));
    CHECK(!ChipoParseVersion("", v));
    CHECK(!ChipoParseVersion("v", v));
    CHECK(!ChipoParseVersion("1..2", v));
    CHECK(!ChipoParseVersion("1.2.3.4", v));
    CHECK(!ChipoParseVersion(nullptr, v));
    CHECK(!ChipoParseVersion("99999999999999.1.1", v));

    CHECK(ChipoIsNewer("0.5.1", "v0.6.0"));
    CHECK(ChipoIsNewer("0.5.9", "0.5.10"));   // numeric, not textual
    CHECK(ChipoIsNewer("0.9.9", "1.0.0"));
    CHECK(!ChipoIsNewer("0.6.0", "v0.6.0"));  // same version
    CHECK(!ChipoIsNewer("0.6.0", "0.5.9"));   // never downgrade
    CHECK(!ChipoIsNewer("0.5.1", "nightly")); // junk tag ignored
    CHECK(!ChipoIsNewer("0.0.0-dev", "9.9.9"));

    char tag[32];
    CHECK(ChipoParseTagName("{\"url\":\"x\",\"tag_name\": \"v0.6.0\",\"name\":\"a\"}", tag, sizeof tag) && !strcmp(tag, "v0.6.0"));
    CHECK(ChipoParseTagName("{\"tag_name\":\"0.7", tag, sizeof tag) == false);        // truncated
    CHECK(!ChipoParseTagName("{\"message\":\"Not Found\"}", tag, sizeof tag));
    CHECK(!ChipoParseTagName("{\"tag_name\":\"\"}", tag, sizeof tag));
    CHECK(!ChipoParseTagName("{\"tag_name\":\"v1.0.0-very-long-tag-name-over-cap\"}", tag, 8));
    CHECK(!ChipoParseTagName("{\"tag_name\":\"a\\\"b\"}", tag, sizeof tag));
    CHECK(!ChipoParseTagName("{\"tag_name\":5}", tag, sizeof tag));

    char url[200];
    CHECK(ChipoBuildDownloadUrl("v0.6.0", url, sizeof url));
    CHECK(!strcmp(url, "https://github.com/liderlyjr/CHIPO/releases/download/v0.6.0/chipo-os-2.16.bin"));
    CHECK(!ChipoBuildDownloadUrl("../../evil", url, sizeof url));
    CHECK(ChipoIsTrustedReleaseUrl(url));
    CHECK(!ChipoBuildDownloadUrl("v0.6.0", url, 20));  // too small a buffer
    CHECK(!ChipoIsTrustedReleaseUrl("http://github.com/liderlyjr/CHIPO/releases/download/v1/chipo-os.bin"));
    CHECK(!ChipoIsTrustedReleaseUrl("https://github.com/someoneelse/CHIPO/releases/download/v1/chipo-os.bin"));
    CHECK(!ChipoIsTrustedReleaseUrl("https://api.tenclass.net/xiaozhi/firmware.bin"));

    CHECK(ChipoIsTrustedRedirectUrl("https://release-assets.githubusercontent.com/github-production-release-asset/1?sig=a"));
    CHECK(ChipoIsTrustedRedirectUrl("https://objects.githubusercontent.com/x"));
    CHECK(ChipoIsTrustedRedirectUrl("https://github.com/x"));
    CHECK(!ChipoIsTrustedRedirectUrl("http://objects.githubusercontent.com/x"));
    CHECK(!ChipoIsTrustedRedirectUrl("https://evilgithubusercontent.com/x"));
    CHECK(!ChipoIsTrustedRedirectUrl("https://github.com@evil.com/x"));
    CHECK(!ChipoIsTrustedRedirectUrl("https://objects.githubusercontent.com.evil.com/x"));
    CHECK(!ChipoIsTrustedRedirectUrl("https://api.tenclass.net/x"));
    CHECK(!ChipoIsTrustedRedirectUrl(nullptr));

    // Authorisation: one use, exact match; nothing authorised by default.
    CHECK(!ChipoUpdateIsAuthorized("https://api.tenclass.net/fw.bin"));
    ChipoUpdateAuthorize("https://objects.githubusercontent.com/abc");
    CHECK(!ChipoUpdateIsAuthorized("https://api.tenclass.net/fw.bin"));  // wrong address, and it burns the grant
    CHECK(!ChipoUpdateIsAuthorized("https://objects.githubusercontent.com/abc"));
    ChipoUpdateAuthorize("https://objects.githubusercontent.com/abc");
    CHECK(ChipoUpdateIsAuthorized("https://objects.githubusercontent.com/abc"));
    CHECK(!ChipoUpdateIsAuthorized("https://objects.githubusercontent.com/abc"));  // not reusable
    CHECK(!ChipoUpdateIsAuthorized(nullptr));

    if (g_fail) { printf("%d failure(s)\n", g_fail); return 1; }
    printf("update logic: all checks passed\n");
    return 0;
}
