#!/usr/bin/env python3
"""Integrate CHIPO's GitHub Releases OTA channel into XiaoZhi."""
from pathlib import Path
import re
import sys

if len(sys.argv) != 3:
    raise SystemExit("Usage: integrate_ota.py <CHIPO repo root> <xiaozhi checkout>")

chipo = Path(sys.argv[1]).resolve()
xiaozhi = Path(sys.argv[2]).resolve()

# Use CHIPO's own semantic version in esp_app_desc. This is essential because
# the device compares the running image with the GitHub release tag.
version_file = chipo / "VERSION"
version_text = version_file.read_text(encoding="utf-8")
match = re.search(r"^CHIPO_OS_VERSION=(\d+\.\d+\.\d+)\s*$", version_text, re.MULTILINE)
if not match:
    raise SystemExit("VERSION must contain CHIPO_OS_VERSION=MAJOR.MINOR.PATCH")
chipo_version = match.group(1)
repo_match = re.search(r"^OTA_RELEASE_REPOSITORY=([A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+)?\s*$", version_text, re.MULTILINE)
ota_repo = repo_match.group(1) if repo_match else ""
if not repo_match:
    raise SystemExit("VERSION must define OTA_RELEASE_REPOSITORY=owner/repo (leave empty to disable device OTA)")

project_cmake = xiaozhi / "CMakeLists.txt"
cmake_text = project_cmake.read_text(encoding="utf-8")
version_pattern = r'set\(PROJECT_VER\s+"[^"]+"\)'
updated_cmake, replacements = re.subn(
    version_pattern, f'set(PROJECT_VER "{chipo_version}")', cmake_text, count=1
)
if replacements == 0:
    if f'set(PROJECT_VER "{chipo_version}")' not in cmake_text:
        raise SystemExit("Cannot safely set CHIPO firmware version in XiaoZhi CMakeLists.txt")
else:
    project_cmake.write_text(updated_cmake, encoding="utf-8", newline="\n")
print(f"[configured] CHIPO firmware version {chipo_version}")

def patch(path: Path, old: str, new: str, label: str):
    text = path.read_text(encoding="utf-8")
    if new in text:
        print(f"[already applied] {label}")
        return
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"Cannot safely patch {label}: expected one anchor, found {count}")
    path.write_text(text.replace(old, new, 1), encoding="utf-8", newline="\n")
    print(f"[patched] {label}")

app_h = xiaozhi / "main" / "application.h"
app_cc = xiaozhi / "main" / "application.cc"
if ota_repo:
    patch(
        app_h,
        "    void CheckNewVersion();\n",
        "    void CheckNewVersion();\n    void CheckChipoOta();\n",
        "Application CHIPO OTA declaration",
    )
    patch(
        app_cc,
        "#include <esp_log.h>\n",
        "#include <esp_log.h>\n#include <esp_app_format.h>\n#include <cstdio>\n#include <cstdlib>\n",
        "CHIPO OTA includes",
    )
else:
    print("[disabled] Device OTA is disabled until VERSION points to a public firmware-only repository.")

# Keep the upstream OTA service for activation and MQTT/WebSocket configuration,
# but do not let it replace CHIPO OS with an unrelated upstream XiaoZhi image.
patch(
    app_cc,
    """        if (ota_->HasNewVersion()) {
            if (UpgradeFirmware(ota_->GetFirmwareUrl(), ota_->GetFirmwareVersion())) {
                return;  // This line will never be reached after reboot
            }
            // If upgrade failed, continue to normal operation
        }
""",
    """        if (ota_->HasNewVersion()) {
            ESP_LOGW(TAG,
                     "Ignoring upstream firmware image; CHIPO OS updates are managed by GitHub Releases");
        }
""",
    "Protect CHIPO from upstream firmware replacement",
)

if ota_repo:
    patch(
        app_cc,
        """    // Check for new firmware version
    CheckNewVersion();

    // Initialize the protocol
""",
        """    // Check upstream service configuration, then check CHIPO's own release channel.
    CheckNewVersion();
    CheckChipoOta();

    // Initialize the protocol
""",
        "Run CHIPO OTA check during activation",
    )


ota_method = r'''
void Application::CheckChipoOta() {
    constexpr char RELEASE_API[] =
        "https://api.github.com/repos/__OTA_REPO__/releases/latest";
    constexpr char FIRMWARE_ASSET[] =
        "CHIPO-OS-Waveshare-ESP32-S3-AMOLED-1.8-V2.bin";
    constexpr char TRUSTED_DOWNLOAD_PREFIX[] =
        "https://github.com/__OTA_REPO__/releases/download/";

    auto& board = Board::GetInstance();
    auto network = board.GetNetwork();
    if (!network) {
        ESP_LOGW(TAG, "CHIPO OTA skipped: network interface unavailable");
        return;
    }

    auto http = network->CreateHttp(0);
    if (!http) {
        ESP_LOGW(TAG, "CHIPO OTA skipped: could not create HTTP client");
        return;
    }
    http->SetHeader("Accept", "application/vnd.github+json");
    http->SetHeader("X-GitHub-Api-Version", "2022-11-28");
    http->SetHeader("User-Agent", "CHIPO-OS");

    auto opened = http->Open("GET", RELEASE_API);
    if (!opened) {
        ESP_LOGW(TAG, "CHIPO OTA release check failed: %s",
                 opened.error().ToString().c_str());
        http->Close();
        return;
    }

    auto status = http->GetStatusCode();
    if (!status || *status != 200) {
        ESP_LOGW(TAG, "CHIPO OTA release API returned HTTP %d",
                 status ? *status : -1);
        http->Close();
        return;
    }

    std::string response = http->ReadAll();
    http->Close();

    cJSON* root = cJSON_Parse(response.c_str());
    if (!root) {
        ESP_LOGW(TAG, "CHIPO OTA: invalid GitHub release JSON");
        return;
    }

    auto tag = cJSON_GetObjectItemCaseSensitive(root, "tag_name");
    auto assets = cJSON_GetObjectItemCaseSensitive(root, "assets");
    if (!cJSON_IsString(tag) || !cJSON_IsArray(assets)) {
        ESP_LOGW(TAG, "CHIPO OTA: release is missing tag_name or assets");
        cJSON_Delete(root);
        return;
    }

    auto parse_version = [](const char* input, int output[3]) -> bool {
        if (!input) return false;
        if (*input == 'v' || *input == 'V') ++input;
        char* end = nullptr;
        for (int i = 0; i < 3; ++i) {
            long value = std::strtol(input, &end, 10);
            if (end == input || value < 0 || value > 9999) return false;
            output[i] = static_cast<int>(value);
            if (i < 2) {
                if (*end != '.') return false;
                input = end + 1;
            } else if (*end != '\0' && *end != '-' && *end != '+') {
                return false;
            }
        }
        return true;
    };

    int latest_version[3] = {};
    int current_version[3] = {};
    const char* running = esp_app_get_description()->version;
    const bool versions_valid =
        parse_version(tag->valuestring, latest_version) &&
        parse_version(running, current_version);

    bool newer = false;
    if (versions_valid) {
        for (int i = 0; i < 3; ++i) {
            if (latest_version[i] > current_version[i]) {
                newer = true;
                break;
            }
            if (latest_version[i] < current_version[i]) break;
        }
    }

    if (!versions_valid || !newer) {
        ESP_LOGI(TAG, "CHIPO OS is up to date (running %s, latest %s)",
                 running, tag->valuestring);
        cJSON_Delete(root);
        return;
    }

    std::string firmware_url;
    cJSON* asset = nullptr;
    cJSON_ArrayForEach(asset, assets) {
        auto name = cJSON_GetObjectItemCaseSensitive(asset, "name");
        auto url = cJSON_GetObjectItemCaseSensitive(asset, "browser_download_url");
        if (cJSON_IsString(name) && cJSON_IsString(url) &&
            std::strcmp(name->valuestring, FIRMWARE_ASSET) == 0) {
            firmware_url = url->valuestring;
            break;
        }
    }
    const std::string version = tag->valuestring;
    cJSON_Delete(root);

    if (firmware_url.rfind(TRUSTED_DOWNLOAD_PREFIX, 0) != 0) {
        ESP_LOGW(TAG, "CHIPO OTA release has no trusted firmware asset");
        return;
    }

    int battery_level = 0;
    bool charging = false;
    bool discharging = false;
    if (!board.GetBatteryLevel(battery_level, charging, discharging)) {
        ESP_LOGW(TAG, "CHIPO OTA postponed: battery status is unavailable");
        return;
    }
    if (!charging && battery_level < 30) {
        ESP_LOGW(TAG, "CHIPO OTA postponed: battery is %d%% and not charging",
                 battery_level);
        board.GetDisplay()->ShowNotification(
            "Actualizacion pausada: carga CHIPO antes de actualizar", 6000);
        return;
    }

    ESP_LOGI(TAG, "New CHIPO OS release %s available; starting OTA", version.c_str());
    UpgradeFirmware(firmware_url, version);
}

'''
if ota_repo:
    ota_method = ota_method.replace("__OTA_REPO__", ota_repo)
    patch(
        app_cc,
        "void Application::InitializeProtocol() {",
        ota_method + "void Application::InitializeProtocol() {",
        "GitHub Releases OTA implementation",
    )
    print(f"[done] CHIPO OTA configured for public repository {ota_repo}.")
else:
    print("[pending] No public firmware repository configured; device OTA remains safely disabled.")
