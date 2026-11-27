// Wi-Fi state straight from ESP-IDF (the station's access point record).
#include <cstring>

#include "esp_wifi.h"
#include "wifi_board.h"
#include "board.h"
#include "chipo_settings_logic.h"

bool ChipoSettingsWifi(WifiView* out) {
    wifi_ap_record_t ap;
    std::memset(&ap, 0, sizeof(ap));
    if (esp_wifi_sta_get_ap_info(&ap) != ESP_OK) {
        out->connected = false;
        return true;
    }
    out->connected = true;
    out->rssi_dbm = ap.rssi;
    out->ssid = reinterpret_cast<const char*>(ap.ssid);
    return true;
}

// Reuse the exact provisioning flow provided by XiaoZhi for this Waveshare board.
void ChipoSettingsEnterWifiConfigMode() {
    static_cast<WifiBoard&>(Board::GetInstance()).EnterWifiConfigMode();
}
