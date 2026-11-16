#!/usr/bin/env python3
"""Integrate CHIPO's background Open-Meteo weather feed into the XiaoZhi UI."""
from pathlib import Path
import shutil
import sys

if len(sys.argv) != 2:
    raise SystemExit("Usage: integrate_weather.py <xiaozhi checkout>")

xiaozhi = Path(sys.argv[1]).resolve()
repo = Path(__file__).resolve().parents[1]
main = xiaozhi / "main"


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


component_src = repo / "components" / "chipo_weather"
component_dst = xiaozhi / "components" / "chipo_weather"
if component_dst.exists():
    shutil.rmtree(component_dst)
shutil.copytree(component_src, component_dst)

# Enable the ESP-IDF CA bundle so Open-Meteo can be queried over verified HTTPS.
defaults = xiaozhi / "sdkconfig.defaults"
defaults_text = defaults.read_text(encoding="utf-8") if defaults.exists() else ""
cert_settings = [
    "CONFIG_MBEDTLS_CERTIFICATE_BUNDLE=y",
    "CONFIG_MBEDTLS_CERTIFICATE_BUNDLE_DEFAULT_FULL=y",
]
missing = [line for line in cert_settings if line not in defaults_text]
if missing:
    with defaults.open("a", encoding="utf-8", newline="\n") as stream:
        if defaults_text and not defaults_text.endswith("\n"):
            stream.write("\n")
        stream.write("\n".join(missing) + "\n")

cmake = main / "CMakeLists.txt"
patch(cmake, "                        chipo_face\n",
      "                        chipo_face\n                        chipo_weather\n",
      "Link CHIPO weather component")

app = main / "application.cc"
patch(app, '#include "chipo_timer_manager.h"\n',
      '#include "chipo_timer_manager.h"\n#include "chipo_weather.h"\n',
      "Include CHIPO weather manager")

face_init = """            if (!chipo_face_->Start(lv_screen_active())) {
                chipo_face_.reset();
                ESP_LOGE(TAG, "Failed to initialize CHIPO Face");
            }
"""
patch(app, face_init, face_init + """            ChipoWeather::GetInstance().Start();
""", "Start background weather service after CHIPO Face")

timer_display = "if (lock) chipo_face_->SetTimerStatus(timer_text.c_str());"
patch(app, timer_display,
      timer_display + """
                const std::string weather_text = ChipoWeather::GetInstance().GetDisplayText();
                chipo_face_->SetWeatherStatus(weather_text.c_str());
""",
      "Refresh weather label on the LVGL task")

print("[done] CHIPO weather integrated: live Sabadell conditions, HTTPS, 15-minute refresh and offline retry.")
