#!/usr/bin/env python3
"""Apply the CHIPO overlay to a clean XiaoZhi ESP-IDF checkout."""
from pathlib import Path
import shutil
import sys

if len(sys.argv) != 3:
    raise SystemExit("Usage: integrate_chipo.py <CHIPO repo root> <xiaozhi checkout>")
chipo = Path(sys.argv[1]).resolve()
xiaozhi = Path(sys.argv[2]).resolve()
main = xiaozhi / "main"
if not (main / "application.h").is_file() or not (main / "application.cc").is_file():
    raise SystemExit(f"Not a XiaoZhi source checkout: {xiaozhi}")

def replace_once(path: Path, old: str, new: str, label: str):
    text = path.read_text(encoding="utf-8")
    if new in text:
        print(f"[already applied] {label}")
        return
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"Cannot safely patch {label}: expected one anchor, found {count}")
    path.write_text(text.replace(old, new, 1), encoding="utf-8", newline="\n")
    print(f"[patched] {label}")

for component in ("chipo_face", "chipo_weather"):
    component_src = chipo / "components" / component
    component_dst = xiaozhi / "components" / component
    if component_dst.exists():
        shutil.rmtree(component_dst)
    shutil.copytree(component_src, component_dst)

adapter_src = chipo / "integration" / "xiaozhi" / "chipo_state_map.h"
shutil.copy2(adapter_src, main / "chipo_state_map.h")

app_h = main / "application.h"
replace_once(
    app_h,
    '#include "device_state.h"\n',
    '#include "device_state.h"\n#include "chipo_face.h"\n',
    "Application header include",
)
replace_once(
    app_h,
    "    std::unique_ptr<Ota> ota_;\n",
    "    std::unique_ptr<Ota> ota_;\n    std::unique_ptr<ChipoFace> chipo_face_;\n",
    "Application CHIPO member",
)
replace_once(
    app_h,
    '#include "chipo_face.h"\n',
    '#include "chipo_face.h"\n#include "chipo_weather_controller.h"\n',
    "Application weather header include",
)
replace_once(
    app_h,
    "    std::unique_ptr<ChipoFace> chipo_face_;\n",
    "    std::unique_ptr<ChipoFace> chipo_face_;\n    std::unique_ptr<ChipoWeather> chipo_weather_;\n",
    "Application weather member",
)

app_cc = main / "application.cc"
replace_once(
    app_cc,
    '#include "application.h"\n',
    '#include "application.h"\n#include "chipo_state_map.h"\n',
    "Application source include",
)
replace_once(
    app_cc,
    "    display->SetupUI();\n",
    """    display->SetupUI();

    // CHIPO visual layer: create it only after the board display has built its LVGL UI.
    {
        DisplayLockGuard lock(display);
        if (lock) {
            chipo_face_ = std::make_unique<ChipoFace>();
            if (!chipo_face_->Start(lv_screen_active())) {
                chipo_face_.reset();
                ESP_LOGE(TAG, "Failed to initialize CHIPO Face");
            }
        }
    }
""",
    "Create CHIPO face after SetupUI",
)
replace_once(
    app_cc,
    """                ESP_LOGE(TAG, "Failed to initialize CHIPO Face");
            }
        }
    }
""",
    """                ESP_LOGE(TAG, "Failed to initialize CHIPO Face");
            }
        }
    }

    // CHIPO weather screen: opens on a voice question or a tap, drawn above the face.
    {
        DisplayLockGuard lock(display);
        if (lock) {
            chipo_weather_ = std::make_unique<ChipoWeather>();
            if (!chipo_weather_->Start(lv_screen_active())) {
                chipo_weather_.reset();
                ESP_LOGE(TAG, "Failed to initialize CHIPO Weather");
            }
        }
    }
""",
    "Create CHIPO weather screen after the face",
)
replace_once(
    app_cc,
    """                Schedule([display, message = std::string(text->valuestring),
                          glyphs = std::move(glyphs), bpp]() {
                    display->AddTextGlyphs(glyphs, bpp);
                    display->SetChatMessage("user", message.c_str());
                });
""",
    """                Schedule([this, display, message = std::string(text->valuestring),
                          glyphs = std::move(glyphs), bpp]() {
                    display->AddTextGlyphs(glyphs, bpp);
                    display->SetChatMessage("user", message.c_str());
                    // CHIPO: the user's speech was recognised and the model is now working on
                    // the answer. This is the real "thinking" moment; Speaking clears it.
                    if (chipo_face_) {
                        DisplayLockGuard lock(display);
                        if (lock) {
                            chipo_face_->SetThinking(true);
                        }
                    }
                    // CHIPO: a weather question opens the weather screen while the server answers by voice.
                    if (chipo_weather_) {
                        DisplayLockGuard lock(display);
                        if (lock) {
                            chipo_weather_->OnVoiceText(message.c_str());
                        }
                    }
                });
""",
    "Show CHIPO thinking and weather once speech is recognised",
)
replace_once(
    app_cc,
    "    auto display = board.GetDisplay();\n    auto led = board.GetLed();\n",
    """    auto display = board.GetDisplay();
    if (chipo_face_) {
        DisplayLockGuard lock(display);
        if (lock) {
            chipo_face_->SetState(ChipoStateFromXiaoZhi(new_state));
        }
    }
    auto led = board.GetLed();
""",
    "Map XiaoZhi state to CHIPO face",
)

cmake = main / "CMakeLists.txt"
replace_once(
    cmake,
    "                    PRIV_REQUIRES\n",
    "                    PRIV_REQUIRES\n                        chipo_face\n",
    "Link chipo_face component",
)
replace_once(
    cmake,
    "                        chipo_face\n",
    "                        chipo_face\n                        chipo_weather\n",
    "Link chipo_weather component",
)

defaults = xiaozhi / "sdkconfig.defaults"
defaults_text = defaults.read_text(encoding="utf-8") if defaults.exists() else ""
settings = [
    "CONFIG_BOARD_TYPE_WAVESHARE_ESP32_S3_TOUCH_AMOLED_1_8_v2=y",
    "CONFIG_USE_WECHAT_MESSAGE_STYLE=y",
    # Wake word "Chipo": custom wake word through ESP-SR MultiNet (English model, written
    # phonetically). Needs tuning on the real board; see docs/WAKE_WORD.md.
    "CONFIG_USE_CUSTOM_WAKE_WORD=y",
    "CONFIG_SR_MN_CN_NONE=y",
    "CONFIG_SR_MN_EN_MULTINET7_QUANT=y",
    'CONFIG_CUSTOM_WAKE_WORD="chee po"',
    'CONFIG_CUSTOM_WAKE_WORD_DISPLAY="Chipo"',
    "CONFIG_CUSTOM_WAKE_WORD_THRESHOLD=20",
    # Fonts for the weather screen (LVGL built-in Montserrat, ASCII).
    "CONFIG_LV_FONT_MONTSERRAT_20=y",
    "CONFIG_LV_FONT_MONTSERRAT_24=y",
]
missing = [line for line in settings if line not in defaults_text]
if missing:
    with defaults.open("a", encoding="utf-8", newline="\n") as f:
        if defaults_text and not defaults_text.endswith("\n"):
            f.write("\n")
        f.write("\n".join(missing) + "\n")
print("[configured] Waveshare ESP32-S3 Touch AMOLED 1.8 V2")
print("[done] CHIPO overlay integrated. Build with: idf.py set-target esp32s3 && idf.py build")
