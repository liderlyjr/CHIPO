#!/usr/bin/env python3
"""Apply the CHIPO overlay to a clean XiaoZhi ESP-IDF checkout."""
from pathlib import Path
import re
import shutil
import sys

# CHIPO OS targets one board only: the Waveshare ESP32-S3 Touch AMOLED 2.16 (480 x 480).
BOARD_SYMBOL = "CONFIG_BOARD_TYPE_WAVESHARE_ESP32_S3_TOUCH_AMOLED_2_16=y"
BOARD_NAME = "Waveshare ESP32-S3 Touch AMOLED 2.16"
# Symbols of boards CHIPO used to support: removed from sdkconfig.defaults if an older run left them there.
OLD_BOARD_SYMBOLS = {"CONFIG_BOARD_TYPE_WAVESHARE_ESP32_S3_TOUCH_AMOLED_1_8_v2=y"}
args = [a for a in sys.argv[1:] if not a.startswith("--board=")]
board_args = [a.split("=", 1)[1] for a in sys.argv[1:] if a.startswith("--board=")]
if len(args) != 2 or any(b != "2.16" for b in board_args):
    raise SystemExit("Usage: integrate_chipo.py <CHIPO repo root> <xiaozhi checkout>  (only the 2.16 board is supported)")
chipo = Path(args[0]).resolve()
xiaozhi = Path(args[1]).resolve()
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

# CHIPO owns the active WeChat-style UI used by this build (CONFIG_USE_WECHAT_MESSAGE_STYLE=y).
# The old patch hit the alternate legacy SetupUI() below #else, which is not compiled on this board.
lcd_display_cc = main / "display" / "lcd_display.cc"
replace_once(
    lcd_display_cc,
    "    lv_obj_center(low_battery_label_);\n"
    "    lv_obj_add_flag(low_battery_popup_, LV_OBJ_FLAG_HIDDEN);\n\n"
    "    emoji_image_ = lv_img_create(screen);\n"
    "    lv_obj_align(emoji_image_, LV_ALIGN_TOP_MID, 0,\n"
    "                 text_font->line_height + lvgl_theme->spacing(8));\n"
    "    lv_obj_add_flag(emoji_image_, LV_OBJ_FLAG_HIDDEN);\n\n"
    "    // Display AI logo while booting\n"
    "    emoji_label_ = lv_label_create(screen);\n"
    "    lv_obj_center(emoji_label_);\n"
    "    lv_obj_set_style_text_font(emoji_label_, large_icon_font, 0);\n"
    "    lv_obj_set_style_text_color(emoji_label_, lvgl_theme->text_color(), 0);\n"
    "    lv_label_set_text(emoji_label_, MATERIAL_SYMBOLS_ROBOT_2);\n",
    "    lv_obj_center(low_battery_label_);\n"
    "    lv_obj_add_flag(low_battery_popup_, LV_OBJ_FLAG_HIDDEN);\n\n"
    "    emoji_image_ = lv_img_create(screen);\n"
    "    lv_obj_align(emoji_image_, LV_ALIGN_TOP_MID, 0,\n"
    "                 text_font->line_height + lvgl_theme->spacing(8));\n"
    "    lv_obj_add_flag(emoji_image_, LV_OBJ_FLAG_HIDDEN);\n\n"
    "    // Display AI logo while booting (suppressed: CHIPO owns the display)\n"
    "    emoji_label_ = lv_label_create(screen);\n"
    "    lv_obj_center(emoji_label_);\n"
    "    lv_obj_set_style_text_font(emoji_label_, large_icon_font, 0);\n"
    "    lv_obj_set_style_text_color(emoji_label_, lvgl_theme->text_color(), 0);\n"
    "    lv_label_set_text(emoji_label_, MATERIAL_SYMBOLS_ROBOT_2);\n"
    "    // CHIPO-only display: black AMOLED root and hide every XiaoZhi root layer.\n"
    "    lv_obj_set_style_bg_color(screen, lv_color_hex(0x000000), LV_PART_MAIN);\n"
    "    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);\n"
    "    lv_obj_add_flag(container_, LV_OBJ_FLAG_HIDDEN);\n"
    "    lv_obj_add_flag(low_battery_popup_, LV_OBJ_FLAG_HIDDEN);\n"
    "    lv_obj_add_flag(emoji_image_, LV_OBJ_FLAG_HIDDEN);\n"
    "    lv_obj_add_flag(emoji_label_, LV_OBJ_FLAG_HIDDEN);\n",
    "Replace the compiled XiaoZhi WeChat screen with CHIPO black canvas",
)
replace_once(
    lcd_display_cc,
    "bool LcdDisplay::SetEmotion(const char* emotion) {\n",
    "bool LcdDisplay::SetEmotion(const char* emotion) {\n"
    "    // CHIPO draws its own eyes and mouth; never display XiaoZhi emoji or robot faces.\n"
    "    (void)emotion;\n"
    "    return true;\n",
    "Disable XiaoZhi emotion artwork in CHIPO mode",
)

for component in ("chipo_face", "chipo_weather", "chipo_update", "chipo_drive", "chipo_buddy", "chipo_ha", "chipo_fun", "chipo_pet", "chipo_remind", "chipo_ambient", "chipo_scenes", "chipo_settings", "chipo_tools", "chipo_boot"):

    component_src = chipo / "components" / component
    component_dst = xiaozhi / "components" / component
    if component_dst.exists():
        shutil.rmtree(component_dst)
    shutil.copytree(component_src, component_dst)

version_file = chipo / "VERSION"
chipo_version = None
for line in version_file.read_text(encoding="utf-8").splitlines():
    if line.startswith("CHIPO_OS_VERSION="):
        chipo_version = line.split("=", 1)[1].strip()
if not chipo_version or not re.fullmatch(r"\d+\.\d+\.\d+", chipo_version):
    raise SystemExit("VERSION must contain CHIPO_OS_VERSION=<major>.<minor>.<patch>")
(xiaozhi / "components" / "chipo_update" / "include" / "chipo_version.h").write_text(
    '#pragma once\n// Generated by scripts/integrate_chipo.py from VERSION.\n'
    f'#define CHIPO_OS_VERSION "{chipo_version}"\n',
    encoding="utf-8", newline="\n")
print(f"[generated] chipo_version.h = {chipo_version}")

# The Waveshare AXP2101 battery monitor shares I2C with the touch controller and other devices.
# A transient bus timeout must not become an unconditional ESP_ERROR_CHECK abort/reboot.
i2c_device_cc = main / "boards" / "common" / "i2c_device.cc"
replace_once(
    i2c_device_cc,
    """uint8_t I2cDevice::ReadReg(uint8_t reg) {
    uint8_t buffer[1];
    ESP_ERROR_CHECK(i2c_master_transmit_receive(i2c_device_, &reg, 1, buffer, 1, 100));
    return buffer[0];
}""",
    """uint8_t I2cDevice::ReadReg(uint8_t reg) {
    uint8_t buffer[1] = {0xFF};
    esp_err_t err = i2c_master_transmit_receive(i2c_device_, &reg, 1, buffer, 1, 100);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "I2C read timeout/error at address 0x%02X register 0x%02X: %s",
                 device_address_, reg, esp_err_to_name(err));
        return 0xFF;  // invalid-read sentinel; do not reboot the whole device for a transient bus fault
    }
    return buffer[0];
}""",
    "I2C: tolerate transient single-register read failures",
)
axp2101_cc = main / "boards" / "common" / "axp2101.cc"
replace_once(
    axp2101_cc,
    """    uint8_t adc = ReadReg(0x30);
    if ((adc & 0x10) == 0) {""",
    """    uint8_t adc = ReadReg(0x30);
    if (adc != 0xFF && (adc & 0x10) == 0) {""",
    "AXP2101: do not write back an invalid I2C read",
)
replace_once(
    axp2101_cc,
    """bool Axp2101::IsChargingDone() {
    uint8_t value = ReadReg(0x01);
    return (value & 0b00000111) == 0b00000100;
}""",
    """bool Axp2101::IsChargingDone() {
    uint8_t value = ReadReg(0x01);
    if (value == 0xFF) return false;
    return (value & 0b00000111) == 0b00000100;
}""",
    "AXP2101: guard charging-done status after I2C error",
)
replace_once(
    axp2101_cc,
    """    uint16_t high = ReadReg(0x3C);
    uint16_t low = ReadReg(0x3D);
    uint16_t raw = (uint16_t)(((high & 0x3F) << 8) | low);""",
    """    uint16_t high = ReadReg(0x3C);
    uint16_t low = ReadReg(0x3D);
    if (high == 0xFF || low == 0xFF) return false;
    uint16_t raw = (uint16_t)(((high & 0x3F) << 8) | low);""",
    "AXP2101: reject invalid temperature register reads",
)
replace_once(
    axp2101_cc,
    """void Axp2101::PowerOff() {
    uint8_t value = ReadReg(0x10);
    value = value | 0x01;
    WriteReg(0x10, value);
}""",
    """void Axp2101::PowerOff() {
    uint8_t value = ReadReg(0x10);
    if (value == 0xFF) {
        ESP_LOGE(TAG, "Cannot power off: AXP2101 I2C status read failed");
        return;
    }
    value = value | 0x01;
    WriteReg(0x10, value);
}""",
    "AXP2101: do not power off from an invalid register value",
)

replace_once(
    axp2101_cc,
    """int Axp2101::GetBatteryCurrentDirection() {
    return (ReadReg(0x01) & 0b01100000) >> 5;
}""",
    """int Axp2101::GetBatteryCurrentDirection() {
    const uint8_t value = ReadReg(0x01);
    if (value == 0xFF) return 0;  // unknown; never report charging/discharging from a failed read
    return (value & 0b01100000) >> 5;
}""",
    "AXP2101: reject invalid battery-current status",
)
replace_once(
    axp2101_cc,
    """int Axp2101::GetBatteryLevel() {
    return ReadReg(0xA4);
}""",
    """int Axp2101::GetBatteryLevel() {
    const uint8_t value = ReadReg(0xA4);
    return value == 0xFF ? -1 : value;
}""",
    "AXP2101: report failed battery-level reads as unavailable",
)
board_cc = main / "boards" / "waveshare" / "esp32-s3-touch-amoled-2.16" / "esp32-s3-touch-amoled-2.16.cc"
replace_once(
    board_cc,
    """        level = pmic_->GetBatteryLevel();
        return true;""",
    """        level = pmic_->GetBatteryLevel();
        if (level < 0 || level > 100) {
            ESP_LOGW(TAG, "Battery level unavailable (AXP2101 I2C read failed)");
            return false;
        }
        return true;""",
    "Waveshare 2.16: do not publish an invalid battery percentage",
)

# std::atomic for chipo_keep_on_ (read by XiaoZhi's power-save timer on the esp_timer task).
replace_once(
    main / "application.h",
    "#include <deque>\n",
    "#include <atomic>\n#include <deque>\n",
    "Application atomic header include",
)
# XiaoZhi's power-save timer asks this before dimming the screen and, 5 min later, powering the board off.
replace_once(
    main / "application.cc",
    "bool Application::CanEnterSleepMode() {\n    if (GetDeviceState() != kDeviceStateIdle) {\n",
    "bool Application::CanEnterSleepMode() {\n"
    "    // CHIPO: a timer, pomodoro, alarm, reminder, clock or screen is pending, or the board is on USB power.\n"
    "    if (chipo_keep_on_.load()) {\n        return false;\n    }\n"
    "    if (GetDeviceState() != kDeviceStateIdle) {\n",
    "Power save: CHIPO keeps the board on while something is pending",
)

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
    '#include "application.h"\n#include "chipo_state_map.h"\n#include "chipo_drive_voice.h"\n',
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
                    // CHIPO DRIVE: "Chipo, vámonos" and the other car-mode commands. When CHIPO answers with
                    // its own sound, the server's spoken reply to that sentence is dropped (see "tts" below).
                    if (chipo_drive_) {
                        DisplayLockGuard lock(display);
                        if (lock && chipo_drive_->OnVoiceText(message.c_str())) {
                            chipo_drive_silence_until_us_ = esp_timer_get_time() + 8000000;
                            if (chipo_face_) chipo_face_->SetThinking(false);
                        }
                    }
                    // Explicit face neutral recalibration; only fires on a clear two-word command.
                    if (chipo_face_ && esp_timer_get_time() >= chipo_drive_silence_until_us_) {
                        const std::string normalized = DriveNormalizeText(message.c_str());
                        if (normalized.find("recalibra") != std::string::npos &&
                            normalized.find("cara") != std::string::npos) {
                            DisplayLockGuard lock(display);
                            if (lock) {
                                chipo_face_->RecalibrateTilt();
                                display->ShowNotification("Ponte de pie y quieto para calibrar la cara", 3500);
                                chipo_face_->SetThinking(false);
                                chipo_drive_silence_until_us_ = esp_timer_get_time() + 8000000;
                            }
                        }
                    }
                    if (chipo_face_ && ChipoIsThanks(message.c_str())) {  // "gracias", "te quiero": a shower of hearts
                        DisplayLockGuard lock(display);
                        if (lock) {
                            chipo_face_->Celebrate(ChipoFace::Burst::Hearts, 5);
                            chipo_face_->ShowEmotion(ChipoFace::Emotion::Love, 2200);
                        }
                    }
                    if (chipo_face_ && esp_timer_get_time() >= chipo_drive_silence_until_us_) {  // "ponte el gorro", "quítate las gafas"
                        const CostumeCommand costume_cmd = ParseCostumeCommand(message.c_str());
                        if (costume_cmd.matched) {
                            DisplayLockGuard lock(display);
                            if (lock) {
                                chipo_face_->SetCostume(costume_cmd.costume);
                                display->ShowNotification(costume_cmd.costume == Costume::None
                                                              ? "Sin disfraz"
                                                              : "¡Disfraz puesto!", 2500);
                                chipo_drive_silence_until_us_ = esp_timer_get_time() + 8000000;
                                chipo_face_->SetThinking(false);
                                std::string_view clip = ChipoDriveClip("buddy_chime");
                                if (!clip.empty()) audio_service_.PlaySound(clip);
                            }
                        }
                    }
                    if (chipo_scenes_ && esp_timer_get_time() >= chipo_drive_silence_until_us_) {  // "baila", "pon el acuario", "quita eso"
                        DisplayLockGuard lock(display);
                        if (lock && chipo_scenes_->OnVoiceText(message.c_str())) {
                            chipo_drive_silence_until_us_ = esp_timer_get_time() + 8000000;
                            if (chipo_face_) chipo_face_->SetThinking(false);
                        }
                    }
                    // CHIPO BUDDY (timers, alarm, bedtime), FUN (lamp, dice, coin) and Home Assistant: same rule, a sound answers
                    // and the server's spoken reply to that sentence is dropped.
                    if ((chipo_tools_ || chipo_buddy_ || chipo_ha_ || chipo_fun_ || chipo_pet_ || chipo_remind_ || chipo_ambient_) && esp_timer_get_time() >= chipo_drive_silence_until_us_) {
                        DisplayLockGuard lock(display);
                        if (lock) {
                            bool handled = false;
                            if (chipo_tools_) handled = chipo_tools_->OnVoiceText(message.c_str());  // "nivel", "bola mágica", "dibuja", "subtítulos"
                            if (!handled && chipo_remind_) handled = chipo_remind_->OnVoiceText(message.c_str());  // "recuérdame a las 18:30 ..."
                            if (!handled && chipo_buddy_) handled = chipo_buddy_->OnVoiceText(message.c_str());
                            if (!handled && chipo_fun_) handled = chipo_fun_->OnVoiceText(message.c_str());
                            if (chipo_pet_) {  // mood and games: every sentence counts as attention
                                const bool pet_handled = chipo_pet_->OnVoiceText(message.c_str());
                                if (!handled) handled = pet_handled;
                            }
                            if (!handled && chipo_ambient_) handled = chipo_ambient_->OnVoiceText(message.c_str());  // rain, fire, noise
                            if (!handled && chipo_ha_) handled = chipo_ha_->OnVoiceText(message.c_str());
                            if (handled) {
                                chipo_drive_silence_until_us_ = esp_timer_get_time() + 8000000;
                                if (chipo_face_) chipo_face_->SetThinking(false);
                            }
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
            if (chipo_settings_) chipo_settings_->NotifyActivity();  // wakes the screen saver
        }
    }
    if (chipo_fun_ && new_state == kDeviceStateWifiConfiguring) chipo_fun_->SetOffline(true);  // no Wi-Fi yet
    auto led = board.GetLed();
""",
    "Map XiaoZhi state to CHIPO face",
)

replace_once(
    app_cc,
    '    ESP_LOGI(TAG, "Network connected");\n',
    '    ESP_LOGI(TAG, "Network connected");\n    if (chipo_fun_) chipo_fun_->SetOffline(false);  // CHIPO stops murmuring\n',
    "Tell CHIPO Fun the network is back",
)
replace_once(
    app_cc,
    "void Application::HandleNetworkDisconnectedEvent() {\n",
    "void Application::HandleNetworkDisconnectedEvent() {\n    if (chipo_fun_) chipo_fun_->SetOffline(true);  // no internet: CHIPO murmurs now and then\n",
    "Tell CHIPO Fun the network is gone",
)

replace_once(
    app_cc,
    "            SetDeviceState(kDeviceStateIdle);\n            Alert(Lang::Strings::ERROR, last_error_message_.c_str(), \"cancel\",",
    """            if (chipo_fun_) {
                DisplayLockGuard lock(Board::GetInstance().GetDisplay());
                if (lock) chipo_fun_->OnServerError(static_cast<uint32_t>(esp_timer_get_time() / 1000));
            }
            SetDeviceState(kDeviceStateIdle);
            Alert(Lang::Strings::ERROR, last_error_message_.c_str(), "cancel",""",
    "CHIPO Fun reacts to a server error",
)

# Loudness and 8-band spectrum of what CHIPO plays and hears (dance, sound visualizer): one cheap call per audio block.
audio_service_cc = main / "audio" / "audio_service.cc"
replace_once(
    audio_service_cc,
    '#include "audio_service.h"\n',
    '#include "audio_service.h"\n#include "chipo_audio_level.h"\n',
    "Include the CHIPO audio level meter",
)
replace_once(
    audio_service_cc,
    "        codec_->OutputData(task.pcm);\n",
    "        ChipoAudioFeed(task.pcm.data(), task.pcm.size(), codec_->output_sample_rate(), codec_->output_channels(), false, ChipoAudioNowMs());\n        codec_->OutputData(task.pcm);\n",
    "Measure the audio CHIPO plays",
)
replace_once(
    audio_service_cc,
    "    /* Update the last input time */\n    last_input_time_ = std::chrono::steady_clock::now();\n",
    "    ChipoAudioFeed(data.data(), data.size(), sample_rate, codec_->input_channels(), true, ChipoAudioNowMs());\n    /* Update the last input time */\n    last_input_time_ = std::chrono::steady_clock::now();\n",
    "Measure the audio CHIPO hears",
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
replace_once(
    cmake,
    "                        chipo_weather\n",
    "                        chipo_weather\n                        chipo_update\n",
    "Link chipo_update component",
)

# --- CHIPO updates: GitHub Releases only; XiaoZhi's cloud can no longer replace the firmware ---
replace_once(
    app_cc,
    '#include "chipo_state_map.h"\n',
    '#include "chipo_state_map.h"\n#include "chipo_update.h"\n',
    "Application update include",
)
replace_once(
    app_cc,
    """                ESP_LOGE(TAG, "Failed to initialize CHIPO Weather");
            }
        }
    }
""",
    """                ESP_LOGE(TAG, "Failed to initialize CHIPO Weather");
            }
        }
    }

    // CHIPO self-update: looks for a newer GitHub Release in the background and flashes it
    // through the stock UpgradeFirmware() path once CHIPO is idle.
    {
        ChipoUpdaterHooks hooks;
        hooks.is_idle = [this]() { return GetDeviceState() == kDeviceStateIdle; };
        hooks.install = [this](const std::string& url, const std::string& tag) {
            Schedule([this, url, tag]() { UpgradeFirmware(url, tag); });
        };
        if (!ChipoUpdaterStart(std::move(hooks))) {
            ESP_LOGW(TAG, "CHIPO updater not started");
        }
    }
""",
    "Start CHIPO updater after the weather screen",
)
replace_once(
    app_cc,
    "bool Application::UpgradeFirmware(const std::string& url, const std::string& version) {\n",
    """bool Application::UpgradeFirmware(const std::string& url, const std::string& version) {
    // CHIPO: only an address the CHIPO updater itself authorised (a GitHub Release of this project)
    // may be flashed. This blocks the XiaoZhi OTA answer and the "self.upgrade_firmware" MCP tool.
    if (!ChipoUpdateIsAuthorized(url.c_str())) {
        ESP_LOGW(TAG, "CHIPO: refused a firmware upgrade that CHIPO did not request");
        return false;
    }
""",
    "Gate UpgradeFirmware to CHIPO releases",
)

# --- CHIPO DRIVE: copilot mode for the car (voice on/off, sunglasses, motion reactions) -------------
replace_once(
    app_h,
    '#include "chipo_weather_controller.h"\n',
    '#include "chipo_weather_controller.h"\n#include "chipo_drive.h"\n#include "chipo_buddy.h"\n#include "chipo_ha.h"\n#include "chipo_fun.h"\n#include "chipo_pet.h"\n#include "chipo_remind.h"\n#include "chipo_ambient.h"\n#include "chipo_scenes.h"\n#include "chipo_settings.h"\n#include "chipo_tools.h"\n#include "chipo_version.h"\n',
    "Application drive header include",
)
replace_once(
    app_h,
    "    std::unique_ptr<ChipoWeather> chipo_weather_;\n",
    "    std::unique_ptr<ChipoWeather> chipo_weather_;\n    std::unique_ptr<ChipoDrive> chipo_drive_;\n    std::unique_ptr<ChipoBuddy> chipo_buddy_;\n    std::unique_ptr<ChipoHa> chipo_ha_;\n    std::unique_ptr<ChipoFun> chipo_fun_;\n    std::unique_ptr<ChipoPet> chipo_pet_;\n    std::unique_ptr<ChipoRemind> chipo_remind_;\n    std::unique_ptr<ChipoAmbient> chipo_ambient_;\n    std::unique_ptr<ChipoScenes> chipo_scenes_;\n    std::unique_ptr<ChipoSettings> chipo_settings_;\n    std::unique_ptr<ChipoTools> chipo_tools_;\n    int64_t chipo_drive_silence_until_us_ = 0;\n    std::atomic<bool> chipo_keep_on_{false};  // CHIPO has something going on: no dimming, no power-off\n",
    "Application drive members",
)
replace_once(
    app_cc,
    '#include "chipo_update.h"\n',
    '#include "chipo_update.h"\n#include "chipo_drive_clips.h"\n#include "chipo_ha_client.h"\n#include "esp_timer.h"\n#include <sys/time.h>\n#include <ctime>\n',
    "Application drive includes",
)
replace_once(
    app_cc,
    """        if (!ChipoUpdaterStart(std::move(hooks))) {
            ESP_LOGW(TAG, "CHIPO updater not started");
        }
    }
""",
    """        if (!ChipoUpdaterStart(std::move(hooks))) {
            ESP_LOGW(TAG, "CHIPO updater not started");
        }
    }

    // CHIPO DRIVE (copilot mode for the car). Switched on and off by voice only.
    if (chipo_face_) {
        DisplayLockGuard lock(display);
        if (lock) {
            chipo_drive_ = std::make_unique<ChipoDrive>();
            ChipoDrive::Hooks hooks;
            // Sounds are clips embedded in the firmware, played through the assistant's own audio path.
            hooks.play_clip = [this](const char* id) {
                std::string_view clip = ChipoDriveClip(id);
                if (clip.empty()) return false;
                Schedule([this, clip]() { audio_service_.PlaySound(clip); });
                return true;
            };
            hooks.notify = [display](const char* text) { display->ShowNotification(text, 2500); };
            hooks.assistant_busy = [this]() {
                const auto state = GetDeviceState();
                return state == kDeviceStateListening || state == kDeviceStateSpeaking;
            };
            if (!chipo_drive_->Start(chipo_face_.get(), std::move(hooks))) {
                chipo_drive_.reset();
                ESP_LOGE(TAG, "Failed to initialize CHIPO Drive");
            }
        }
    }

    // CHIPO Home Assistant (only active when the address and token are stored in NVS; otherwise off).
    {
        DisplayLockGuard lock(display);
        if (lock) {
            chipo_ha_ = std::make_unique<ChipoHa>();
            HaConfig ha_cfg;
            ChipoHaLoadConfig(&ha_cfg);
            ChipoHa::Hooks ha_hooks;
            ha_hooks.play_clip = [this](const char* id) {
                std::string_view clip = ChipoDriveClip(id);
                if (clip.empty()) return false;
                Schedule([this, clip]() { audio_service_.PlaySound(clip); });
                return true;
            };
            ha_hooks.notify = [display](const char* text) { display->ShowNotification(text, 3000); };
            ha_hooks.post = ChipoHaPost;
            ha_hooks.get = ChipoHaGet;
            ha_hooks.run_async = ChipoHaRunAsync;
            if (!chipo_ha_->Start(ha_cfg, std::move(ha_hooks))) {
                chipo_ha_.reset();
                ESP_LOGE(TAG, "Failed to initialize CHIPO Home Assistant");
            }
        }
    }

    // CHIPO FUN (AMOLED): start-up screen, colour lamp, dice, coin. Created before BUDDY so that an alarm
    // ringing while the lamp is on is drawn above it.
    {
        DisplayLockGuard lock(display);
        if (lock) {
            chipo_fun_ = std::make_unique<ChipoFun>();
            ChipoFun::Hooks hooks;
            hooks.play_clip = [this](const char* id) {
                std::string_view clip = ChipoDriveClip(id);
                if (clip.empty()) return false;
                Schedule([this, clip]() { audio_service_.PlaySound(clip); });
                return true;
            };
            hooks.notify = [display](const char* text) { display->ShowNotification(text, 3000); };
            // The start-up screen ends: the face wakes up with a smile.
            hooks.boot_done = [this]() {
                if (chipo_face_) chipo_face_->ShowEmotion(ChipoFace::Emotion::Happy, 1800);
            };
            // A server error ("me caí"): the face gets sad for a moment.
            hooks.oops = [this]() {
                if (chipo_face_) chipo_face_->ShowEmotion(ChipoFace::Emotion::Sad, 3500);
            };
            // Offline murmur: the face laughs while CHIPO babbles (runs on the LVGL task, display lock held).
            hooks.babble = [this](uint32_t ms) {
                if (chipo_face_) chipo_face_->ShowEmotion(ChipoFace::Emotion::Laugh, ms);
            };
            // Eye colour chosen by voice ("ojos rosas"): applied now and kept across reboots.
            hooks.set_eye_color = [this](int64_t rgb) {
                if (!chipo_face_) return;
                if (rgb < 0) chipo_face_->ClearEyeTint();
                else chipo_face_->SetEyeTint(static_cast<uint32_t>(rgb));
                Settings settings("chipo", true);
                settings.SetInt("eye_rgb", static_cast<int32_t>(rgb));
            };
            {
                Settings saved("chipo", false);
                const int32_t eye_rgb = saved.GetInt("eye_rgb", -1);
                if (eye_rgb >= 0 && chipo_face_) chipo_face_->SetEyeTint(static_cast<uint32_t>(eye_rgb));
            }
            if (!chipo_fun_->Start(lv_screen_active(), std::move(hooks), CHIPO_OS_VERSION)) {
                chipo_fun_.reset();
                ESP_LOGE(TAG, "Failed to initialize CHIPO Fun");
            } else {
                chipo_fun_->ShowBoot();
            }
        }
    }

    // CHIPO PET: mood of the little computer and voice games (rock-paper-scissors, guess the number).
    {
        DisplayLockGuard lock(display);
        if (lock) {
            chipo_pet_ = std::make_unique<ChipoPet>();
            ChipoPet::Hooks hooks;
            hooks.play_clip = [this](const char* id) {
                std::string_view clip = ChipoDriveClip(id);
                if (clip.empty()) return false;
                Schedule([this, clip]() { audio_service_.PlaySound(clip); });
                return true;
            };
            hooks.notify = [display](const char* text) { display->ShowNotification(text, 3000); };
            hooks.emotion = [this](const char* name, uint32_t ms) {
                if (chipo_face_) chipo_face_->ShowEmotionByName(name, ms);
            };
            hooks.cry = [this](uint32_t ms) {  // very neglected: tears
                if (chipo_face_) chipo_face_->Cry(ms);
            };
            hooks.sore_loser = [this]() {  // lost a game: tantrum, then tears
                if (chipo_face_) chipo_face_->SoreLoser();
            };
            if (!chipo_pet_->Start(std::move(hooks))) {
                chipo_pet_.reset();
                ESP_LOGE(TAG, "Failed to initialize CHIPO Pet");
            }
        }
    }

    // The calendar chooses the costume (Santa hat from 1 Dec to 6 Jan). The clock is the server time plus the zone offset.
    if (chipo_face_) {
        DisplayLockGuard lock(display);
        if (lock) {
            chipo_face_->SetDateSource([](int* month, int* day) {
                struct timeval tv;
                gettimeofday(&tv, nullptr);
                if (tv.tv_sec < 1700000000) return false;
                struct tm tm_now;
                gmtime_r(&tv.tv_sec, &tm_now);
                *month = tm_now.tm_mon + 1;
                *day = tm_now.tm_mday;
                return true;
            });
        }
    }

    // CHIPO REMIND: spoken reminders at a time of day, kept in the device's NVS so they survive a reboot.
    {
        DisplayLockGuard lock(display);
        if (lock) {
            chipo_remind_ = std::make_unique<ChipoRemind>();
            ChipoRemind::Hooks hooks;
            hooks.play_clip = [this](const char* id) {
                std::string_view clip = ChipoDriveClip(id);
                if (clip.empty()) return false;
                Schedule([this, clip]() { audio_service_.PlaySound(clip); });
                return true;
            };
            hooks.notify = [display](const char* text) { display->ShowNotification(text, 3000); };
            hooks.emotion = [this](const char* name, uint32_t ms) {
                if (chipo_face_) chipo_face_->ShowEmotionByName(name, ms);
            };
            hooks.assistant_busy = [this]() {
                const auto state = GetDeviceState();
                return state == kDeviceStateListening || state == kDeviceStateSpeaking;
            };
            // XiaoZhi stores the server time plus the time zone offset as the system time (see BUDDY below).
            hooks.wall_time = [](ChipoRemind::Wall* out) {
                struct timeval tv;
                gettimeofday(&tv, nullptr);
                if (tv.tv_sec < 1700000000) return false;
                struct tm tm_now;
                gmtime_r(&tv.tv_sec, &tm_now);
                out->valid = true;
                out->hour = tm_now.tm_hour;
                out->minute = tm_now.tm_min;
                return true;
            };
            hooks.load = []() {
                Settings saved("chipo", false);
                return saved.GetString("reminders", "");
            };
            hooks.save = [](const std::string& data) {
                Settings settings("chipo", true);
                if (data.empty()) settings.EraseKey("reminders");
                else settings.SetString("reminders", data);
            };
            if (!chipo_remind_->Start(std::move(hooks))) {
                chipo_remind_.reset();
                ESP_LOGE(TAG, "Failed to initialize CHIPO Remind");
            }
        }
    }

    // CHIPO AMBIENT: rain / fire / soft noise in the background (one 0.96 s piece queued about every second).
    {
        DisplayLockGuard lock(display);
        if (lock) {
            chipo_ambient_ = std::make_unique<ChipoAmbient>();
            ChipoAmbient::Hooks hooks;
            hooks.clip_exists = [](const char* id) { return !ChipoDriveClip(id).empty(); };
            hooks.play_clip = [this](const char* id) {
                std::string_view clip = ChipoDriveClip(id);
                if (clip.empty()) return false;
                Schedule([this, clip]() { audio_service_.PlaySound(clip); });
                return true;
            };
            hooks.notify = [display](const char* text) { display->ShowNotification(text, 3000); };
            hooks.emotion = [this](const char* name, uint32_t ms) {
                if (chipo_face_) chipo_face_->ShowEmotionByName(name, ms);
            };
            hooks.assistant_busy = [this]() {
                const auto state = GetDeviceState();
                return state == kDeviceStateListening || state == kDeviceStateSpeaking;
            };
            if (!chipo_ambient_->Start(std::move(hooks))) {
                chipo_ambient_.reset();
                ESP_LOGE(TAG, "Failed to initialize CHIPO Ambient");
            }
        }
    }

    // CHIPO BUDDY: timer, pomodoro, stopwatch, alarm and bedside clock by voice.
    {
        DisplayLockGuard lock(display);
        if (lock) {
            chipo_buddy_ = std::make_unique<ChipoBuddy>();
            ChipoBuddy::Hooks hooks;
            hooks.play_clip = [this](const char* id) {
                std::string_view clip = ChipoDriveClip(id);
                if (clip.empty()) return false;
                Schedule([this, clip]() { audio_service_.PlaySound(clip); });
                return true;
            };
            hooks.notify = [display](const char* text) { display->ShowNotification(text, 2500); };
            hooks.assistant_busy = [this]() {
                const auto state = GetDeviceState();
                return state == kDeviceStateListening || state == kDeviceStateSpeaking;
            };
            hooks.in_drive = [this]() { return chipo_drive_ && chipo_drive_->active(); };
            // XiaoZhi stores the server time plus the time zone offset as the system time, so the
            // "UTC" fields already are the local clock. Before the first sync the year is 1970: not valid.
            hooks.wall_time = [](BuddyWallTime* out) {
                struct timeval tv;
                gettimeofday(&tv, nullptr);
                if (tv.tv_sec < 1700000000) return false;
                struct tm tm_now;
                gmtime_r(&tv.tv_sec, &tm_now);
                out->valid = true;
                out->hour = tm_now.tm_hour;
                out->minute = tm_now.tm_min;
                out->second = tm_now.tm_sec;
                return true;
            };
            hooks.scene = [this](const char* which) { if (chipo_ha_) chipo_ha_->RunScene(which); };
            if (!chipo_buddy_->Start(lv_screen_active(), std::move(hooks))) {
                chipo_buddy_.reset();
                ESP_LOGE(TAG, "Failed to initialize CHIPO Buddy");
            } else if (chipo_face_) {
                // Lying on its side for a while turns CHIPO into a desk clock (the face reports it).
                chipo_face_->SetDeskClockObserver([this](bool on, int side) {
                    if (chipo_buddy_) chipo_buddy_->SetDeskClock(on, side);
                });
            }
        }
    }

    // CHIPO SCENES: sound visualizer, aquarium, lava lamp and the dance. The face dances to the loudness of CHIPO's own audio.
    {
        DisplayLockGuard lock(display);
        if (lock) {
            if (chipo_face_) {
                chipo_face_->SetLevelSource([]() { return ChipoAudioRead(ChipoAudioNowMs(), true).level; });
            }
            chipo_scenes_ = std::make_unique<ChipoScenes>();
            ChipoScenes::Hooks hooks;
            hooks.audio = [](float* bands, float* level) {
                const ChipoAudioLevels l = ChipoAudioRead(ChipoAudioNowMs(), true);
                if (!l.valid) return false;
                for (int i = 0; i < kChipoAudioBands; ++i) bands[i] = l.band[i];
                *level = l.level;
                return true;
            };
            hooks.play_clip = [this](const char* id) {
                std::string_view clip = ChipoDriveClip(id);
                if (clip.empty()) return false;
                Schedule([this, clip]() { audio_service_.PlaySound(clip); });
                return true;
            };
            hooks.notify = [display](const char* text) { display->ShowNotification(text, 2500); };
            hooks.dance = [this](bool on) {
                if (chipo_face_) chipo_face_->SetDance(on);
            };
            hooks.dancing = [this]() { return chipo_face_ && chipo_face_->dancing(); };
            if (!chipo_scenes_->Start(lv_screen_active(), std::move(hooks))) {
                chipo_scenes_.reset();
                ESP_LOGE(TAG, "Failed to initialize CHIPO Scenes");
            }
        }
    }

    // CHIPO TOOLS: time and temperature around the face, pomodoro ring, spirit level, magic 8-ball, drawing canvas,
    // subtitles, and "shake to throw again". Created before SETTINGS so the settings menu is drawn above it.
    {
        DisplayLockGuard lock(display);
        if (lock) {
            chipo_tools_ = std::make_unique<ChipoTools>();
            ChipoTools::Hooks hooks;
            // Same rule as BUDDY: XiaoZhi stores the local time as system time; a year near 1970 means "not set yet".
            hooks.wall_time = [](int* hour, int* minute) {
                struct timeval tv;
                gettimeofday(&tv, nullptr);
                if (tv.tv_sec < 1700000000) return false;
                struct tm tm_now;
                gmtime_r(&tv.tv_sec, &tm_now);
                *hour = tm_now.tm_hour;
                *minute = tm_now.tm_min;
                return true;
            };
            hooks.temperature = [this](int* temp_c) {
                WeatherData data;
                if (!chipo_weather_ || !chipo_weather_->Latest(&data, 3u * 60u * 60u * 1000u)) return false;
                *temp_c = data.temp;
                return true;
            };
            hooks.refresh_weather = [this]() {
                if (chipo_weather_ && GetDeviceState() == kDeviceStateIdle) chipo_weather_->Prefetch();
            };
            hooks.pomodoro = [this](float* remaining, bool* work) {
                return chipo_buddy_ && chipo_buddy_->PomodoroProgress(remaining, work);
            };
            hooks.face_calm = [this]() {
                if (GetDeviceState() != kDeviceStateIdle || !chipo_face_) return false;
                const auto info = chipo_face_->Debug();
                return info.shown_state == ChipoFace::State::Idle && info.mood == ChipoFace::Mood::Normal &&
                       info.emotion == ChipoFace::Emotion::None && !info.fell && !info.drive_mode && !info.dancing;
            };
            hooks.other_screen = [this]() {
                return (chipo_weather_ && chipo_weather_->IsShown()) || (chipo_fun_ && chipo_fun_->mode() != ChipoFun::Mode::Off) ||
                       (chipo_buddy_ && (chipo_buddy_->screen_shown() || chipo_buddy_->bedtime())) ||
                       (chipo_scenes_ && chipo_scenes_->screen_shown()) || (chipo_drive_ && chipo_drive_->active());
            };
            hooks.emotion = [this](const char* name, uint32_t ms) {
                if (chipo_face_) chipo_face_->ShowEmotionByName(name, ms);
            };
            hooks.play_clip = [this](const char* id) {
                std::string_view clip = ChipoDriveClip(id);
                if (clip.empty()) return false;
                Schedule([this, clip]() { audio_service_.PlaySound(clip); });
                return true;
            };
            hooks.notify = [display](const char* text) { display->ShowNotification(text, 2500); };
            hooks.shake_other = [this](uint32_t now_ms) { return chipo_fun_ && chipo_fun_->OnShake(now_ms); };
            // Do not retain a raw font pointer borrowed from the active Theme: SetTheme() may
            // replace/destroy it while CHIPO TOOLS labels still reference it. The component uses
            // its built-in Montserrat font and folds accents when no long-lived custom font is supplied.
            hooks.load_flag = [](const char* key, bool def) {
                Settings settings("chipo", false);
                return settings.GetBool(key, def);
            };
            hooks.save_flag = [](const char* key, bool value) {
                Settings settings("chipo", true);
                settings.SetBool(key, value);
            };
            if (!chipo_tools_->Start(lv_screen_active(), std::move(hooks))) {
                chipo_tools_.reset();
                ESP_LOGE(TAG, "Failed to initialize CHIPO Tools");
            } else {
#ifdef CONFIG_BOARD_TYPE_WAVESHARE_ESP32_S3_TOUCH_AMOLED_2_16  // always true: CHIPO only builds for the 2.16
                // Waveshare documents GPIO18 on the 2.16" as a user key (10k pull-up, pressed = GND, "custom functions");
                // XiaoZhi leaves it unused. One click cycles the tools. Lives as long as the firmware.
                static Button* chipo_key = new Button(GPIO_NUM_18);
                // The button callback runs in the esp_timer task: never wait for the display lock there.
                chipo_key->OnClick([this]() {
                    Schedule([this]() {
                        DisplayLockGuard key_lock(Board::GetInstance().GetDisplay());
                        if (key_lock && chipo_tools_) chipo_tools_->OnKey();
                    });
                });
#endif
            }
            if (chipo_tools_ && chipo_face_) {
                // Shaking (accelerometer): the face keeps the car mode's own listener, this one is separate.
                chipo_face_->SetExtraSampleObserver([this](const ImuSample& sample, uint32_t now_ms) {
                    if (chipo_tools_) chipo_tools_->OnSample(sample, now_ms);
                });
            }
        }
    }

    // CHIPO SETTINGS (last, so it is drawn above everything): long-press menu, battery, screen saver,
    // burn-in drift of the face and touch reactions of the face.
    {
        DisplayLockGuard lock(display);
        if (lock) {
            chipo_settings_ = std::make_unique<ChipoSettings>();
            ChipoSettings::Hooks hooks;
            hooks.get_volume = []() {
                auto codec = Board::GetInstance().GetAudioCodec();
                return codec ? codec->output_volume() : 70;
            };
            hooks.set_volume = [](int volume) {
                auto codec = Board::GetInstance().GetAudioCodec();
                if (codec) codec->SetOutputVolume(volume);
            };
            hooks.get_brightness = []() {
                auto backlight = Board::GetInstance().GetBacklight();
                return backlight ? static_cast<int>(backlight->brightness()) : 80;
            };
            hooks.set_brightness = [](int percent, bool permanent) {
                auto backlight = Board::GetInstance().GetBacklight();
                if (backlight) backlight->SetBrightness(static_cast<uint8_t>(percent), permanent);
            };
            hooks.battery = [](int* level, bool* charging, bool* discharging) {
                return Board::GetInstance().GetBatteryLevel(*level, *charging, *discharging);
            };
            hooks.wifi = ChipoSettingsWifi;
            hooks.restart = []() { esp_restart(); };
            hooks.notify = [display](const char* text) { display->ShowNotification(text, 3000); };
            hooks.play_clip = [this](const char* id) {
                std::string_view clip = ChipoDriveClip(id);
                if (clip.empty()) return false;
                Schedule([this, clip]() { audio_service_.PlaySound(clip); });
                return true;
            };
            hooks.react = [this](TapKind kind) {
                if (!chipo_face_) return;
                if (chipo_pet_) chipo_pet_->OnTouch(kind == TapKind::Tickle ? ChipoPet::Touch::Tickle : ChipoPet::Touch::Tap);
                if (kind == TapKind::Tickle) {
                    chipo_face_->ShowEmotion(ChipoFace::Emotion::Laugh, 2500);
                    chipo_face_->Celebrate(ChipoFace::Burst::Sparkles, 6);
                } else {
                    chipo_face_->ShowEmotion(ChipoFace::Emotion::Wink, 900);
                    chipo_face_->Celebrate(ChipoFace::Burst::Hearts, 2);
                }
            };
            hooks.gaze = [this](float nx, float ny, bool down) {  // the eyes follow the finger
                if (!chipo_face_) return;
                if (down) chipo_face_->LookAt(nx, ny);
                else chipo_face_->StopLookAt();
            };
            hooks.shift = [this](int dx, int dy) {
                if (chipo_face_) chipo_face_->SetPixelShift(dx, dy);
            };
            auto overlay = [this]() {
                return (chipo_weather_ && chipo_weather_->IsShown()) || (chipo_fun_ && chipo_fun_->mode() != ChipoFun::Mode::Off) ||
                       (chipo_buddy_ && chipo_buddy_->screen_shown()) || (chipo_scenes_ && chipo_scenes_->screen_shown()) || (chipo_drive_ && chipo_drive_->active()) ||
                       (chipo_tools_ && chipo_tools_->screen_shown());
            };
            hooks.overlay_active = overlay;
            hooks.emotion = [this](const char* name, uint32_t ms) { if (chipo_face_) chipo_face_->ShowEmotionByName(name, ms); };
            hooks.keep_awake = [this, overlay]() { return GetDeviceState() != kDeviceStateIdle || overlay(); };
            if (!chipo_settings_->Start(lv_screen_active(), std::move(hooks), CHIPO_OS_VERSION)) {
                chipo_settings_.reset();
                ESP_LOGE(TAG, "Failed to initialize CHIPO Settings");
            } else if (chipo_weather_) {
                // The weather opens on a tap anywhere except on the face (which reacts) and over other screens.
                chipo_weather_->SetTapBlocker([this](int y) { return chipo_settings_ && chipo_settings_->BlocksWeatherTap(y); });
            }
            // CHIPO keeps the board awake. XiaoZhi's power-save timer (2.16 board code) dims the screen after 60 s and
            // POWERS THE BOARD OFF after 5 min without a conversation; a timer, a pomodoro, an alarm, a reminder or the
            // bedside clock would never ring. Once a second (LVGL task) CHIPO works out whether it has anything going
            // on; CanEnterSleepMode() (esp_timer task) reads the flag. On USB power CHIPO always stays on.
            lv_timer_create([](lv_timer_t* t) {
                auto* app = static_cast<Application*>(lv_timer_get_user_data(t));
                static uint32_t last_power_check_ms = 0;
                static bool external_power = false;
                const uint32_t now = lv_tick_get();
                if (last_power_check_ms == 0 || now - last_power_check_ms >= 10000) {
                    last_power_check_ms = now ? now : 1;
                    int level = 0;
                    bool charging = false, discharging = false;
                    // No reading (no battery gauge): treat it as plugged in, never power off on a guess.
                    external_power = !Board::GetInstance().GetBatteryLevel(level, charging, discharging) || charging || !discharging;
                }
                bool busy = external_power;
                if (app->chipo_buddy_) {
                    const auto& e = app->chipo_buddy_->engine();
                    busy = busy || e.timer_active() || e.pomodoro_phase() != PomodoroPhase::Off || e.stopwatch_running() ||
                           e.alarm_set() || app->chipo_buddy_->ringing() || app->chipo_buddy_->bedtime() ||
                           app->chipo_buddy_->desk_clock() || app->chipo_buddy_->screen_shown();
                }
                if (app->chipo_remind_) busy = busy || !app->chipo_remind_->store().items().empty() || app->chipo_remind_->ringing();
                if (app->chipo_ambient_) busy = busy || app->chipo_ambient_->playing();
                if (app->chipo_drive_) busy = busy || app->chipo_drive_->active();
                if (app->chipo_weather_) busy = busy || app->chipo_weather_->IsShown();
                if (app->chipo_scenes_) busy = busy || app->chipo_scenes_->screen_shown();
                if (app->chipo_fun_) busy = busy || app->chipo_fun_->mode() != ChipoFun::Mode::Off;
                if (app->chipo_tools_) busy = busy || app->chipo_tools_->screen_shown();
                app->chipo_keep_on_.store(busy);
            }, 1000, this);
        }
    }
""",
    "Create CHIPO Drive after the face",
)
replace_once(
    app_cc,
    """                Schedule([this]() {
                    aborted_ = false;
                    SetDeviceState(kDeviceStateSpeaking);
                });""",
    """                Schedule([this]() {
                    aborted_ = false;
                    if (esp_timer_get_time() < chipo_drive_silence_until_us_) {
                        // CHIPO DRIVE already answered this command: do not play the server's reply.
                        chipo_drive_silence_until_us_ = 0;
                        AbortSpeaking(kAbortReasonNone);
                        return;
                    }
                    SetDeviceState(kDeviceStateSpeaking);
                });""",
    "Drop the server reply to a car-mode command",
)
replace_once(
    cmake,
    "                        chipo_update\n",
    "                        chipo_update\n                        chipo_drive\n                        chipo_buddy\n                        chipo_ha\n                        chipo_fun\n                        chipo_pet\n                        chipo_remind\n                        chipo_ambient\n                        chipo_scenes\n                        chipo_settings\n",
    "Link chipo_drive component",
)

# --- CHIPO TOOLS: time/weather strip, pomodoro ring, level, magic 8-ball, canvas, subtitles, shaking -------------------
replace_once(
    cmake,
    "                        chipo_settings\n",
    "                        chipo_settings\n                        chipo_tools\n",
    "Link chipo_tools component",
)
replace_once(
    app_cc,
    '#include "settings.h"\n',
    '#include "settings.h"\n#include "lvgl_font.h"\n',
    "Application font header include (subtitles use the display's own text font)",
)
replace_once(
    app_cc,
    '#include "lvgl_font.h"\n',
    '#include "lvgl_font.h"\n#include "button.h"\n',
    "Application button header include (user key on the 2.16\")",
)

# --- Boot summary: a few clear lines in the serial log (version, memory, modules started / failed) -------------------
replace_once(
    cmake,
    "                        chipo_tools\n",
    "                        chipo_tools\n                        chipo_boot\n",
    "Link chipo_boot component",
)
replace_once(
    app_cc,
    '#include "lvgl_font.h"\n#include "button.h"\n',
    '#include "lvgl_font.h"\n#include "button.h"\n#include "chipo_boot_report.h"\n',
    "Application boot report include",
)
replace_once(
    app_cc,
    """            }, 1000, this);
        }
    }
""",
    """            }, 1000, this);
        }
    }

    // CHIPO boot summary: version, free memory and which modules started (a module that failed to start left its
    // pointer empty). A few lines instead of searching the whole log.
    ChipoLogBootReport(CHIPO_OS_VERSION, {
        {"face", chipo_face_ != nullptr},
        {"weather", chipo_weather_ != nullptr},
        {"drive", chipo_drive_ != nullptr},
        {"ha", chipo_ha_ != nullptr},
        {"fun", chipo_fun_ != nullptr},
        {"pet", chipo_pet_ != nullptr},
        {"remind", chipo_remind_ != nullptr},
        {"ambient", chipo_ambient_ != nullptr},
        {"buddy", chipo_buddy_ != nullptr},
        {"scenes", chipo_scenes_ != nullptr},
        {"tools", chipo_tools_ != nullptr},
        {"settings", chipo_settings_ != nullptr},
    });
""",
    "Log the CHIPO boot summary after the last module",
)
replace_once(
    app_cc,
    """                    Schedule([display, message = std::string(text->valuestring),
                              glyphs = std::move(glyphs), bpp]() {
                        display->AddTextGlyphs(glyphs, bpp);
                        display->SetChatMessage("assistant", message.c_str());
""",
    """                    Schedule([this, display, message = std::string(text->valuestring),
                              glyphs = std::move(glyphs), bpp]() {
                        display->AddTextGlyphs(glyphs, bpp);
                        display->SetChatMessage("assistant", message.c_str());
                        if (chipo_tools_) {  // subtitles (only when the owner turned them on)
                            DisplayLockGuard lock(display);
                            if (lock) chipo_tools_->ShowSubtitle(message.c_str());
                        }
""",
    "Subtitles: the sentence CHIPO is saying",
)

ota_cc = main / "ota.cc"
replace_once(
    ota_cc,
    """        ESP_LOGW(TAG, "No firmware section found!");
    }
""",
    """        ESP_LOGW(TAG, "No firmware section found!");
    }
    // CHIPO: the XiaoZhi server must never offer firmware (this also overrides "force").
    // CHIPO updates itself from its own GitHub Releases (components/chipo_update).
    if (has_new_version_) {
        ESP_LOGW(TAG, "Ignoring firmware offered by the OTA server (CHIPO updates from GitHub)");
    }
    has_new_version_ = false;
""",
    "Ignore firmware offered by the XiaoZhi OTA server",
)
# --- CHIPO display defaults: black CHIPO canvas and 90-degree counter-clockwise orientation ----------------------
board_config = main / "boards" / "waveshare" / "esp32-s3-touch-amoled-2.16" / "config.h"
replace_once(
    board_config,
    "#define DISPLAY_SWAP_XY false",
    "#define DISPLAY_SWAP_XY true",
    "Waveshare 2.16: rotate the complete display 90 degrees counter-clockwise (swap XY)",
)
replace_once(
    board_config,
    "#define DISPLAY_MIRROR_Y false",
    "#define DISPLAY_MIRROR_Y true",
    "Waveshare 2.16: complete counter-clockwise display rotation (mirror Y)",
)
board_cc = main / "boards" / "waveshare" / "esp32-s3-touch-amoled-2.16" / "esp32-s3-touch-amoled-2.16.cc"
replace_once(
    board_cc,
    """                .swap_xy = 0,
                .mirror_x = 0,
                .mirror_y = 0,""",
    """                .swap_xy = 1,
                .mirror_x = 0,
                .mirror_y = 1,""",
    "Waveshare 2.16: keep touch coordinates aligned with counter-clockwise screen rotation",
)
lcd_display_cc = main / "display" / "lcd_display.cc"
replace_once(
    lcd_display_cc,
    """    Settings settings("display", false);
    std::string theme_name = settings.GetString("theme", "light");
    current_theme_ = LvglThemeManager::GetInstance().GetTheme(theme_name);""",
    """    // CHIPO uses the dark theme; the active LVGL root is explicitly forced to opaque pure black in SetupUI().
    // Do not inherit a previously saved light-theme preference on this dedicated device.
    current_theme_ = LvglThemeManager::GetInstance().GetTheme("dark");""",
    "CHIPO: force the white light theme for the complete XiaoZhi interface",
)

root_cmake = xiaozhi / "CMakeLists.txt"
text = root_cmake.read_text(encoding="utf-8")
new_text, n = re.subn(r'set\(PROJECT_VER "[^"]*"\)', f'set(PROJECT_VER "{chipo_version}")', text, count=1)
if n != 1:
    raise SystemExit("Cannot find set(PROJECT_VER ...) in the XiaoZhi CMakeLists.txt")
if new_text != text:
    root_cmake.write_text(new_text, encoding="utf-8", newline="\n")
    print(f"[patched] PROJECT_VER = {chipo_version}")
else:
    print("[already applied] PROJECT_VER")

defaults = xiaozhi / "sdkconfig.defaults"
defaults_text = defaults.read_text(encoding="utf-8") if defaults.exists() else ""
settings = [
    BOARD_SYMBOL,
    # Spanish device language: XiaoZhi's on-screen texts and system voice prompts, and the Accept-Language
    # header sent to the server (ota.cc). Without it the device is in English (the Kconfig default).
    "CONFIG_LANGUAGE_ES_ES=y",
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
    "CONFIG_LV_FONT_MONTSERRAT_32=y",  # time and temperature strip, level, 8-ball
    "CONFIG_LV_USE_CANVAS=y",          # drawing canvas
]
kept_lines = [ln for ln in defaults_text.splitlines() if ln.strip() not in OLD_BOARD_SYMBOLS]
if len(kept_lines) != len(defaults_text.splitlines()):
    defaults_text = "\n".join(kept_lines) + "\n"
    defaults.write_text(defaults_text, encoding="utf-8", newline="\n")
    print("[patched] removed the symbol of a board CHIPO no longer supports from sdkconfig.defaults")
missing = [line for line in settings if line not in defaults_text]
if missing:
    with defaults.open("a", encoding="utf-8", newline="\n") as f:
        if defaults_text and not defaults_text.endswith("\n"):
            f.write("\n")
        f.write("\n".join(missing) + "\n")
print(f"[configured] {BOARD_NAME}")
print("[done] CHIPO overlay integrated. Build with: idf.py set-target esp32s3 && idf.py build")
