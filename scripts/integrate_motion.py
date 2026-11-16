from pathlib import Path
import sys

xiaozhi = Path(sys.argv[1]).resolve()
main = xiaozhi / "main"

def patch(path, old, new, label):
    text = path.read_text(encoding="utf-8")
    if new in text:
        print(f"[already applied] {label}")
        return
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"Cannot safely patch {label}: expected one anchor, found {count}")
    path.write_text(text.replace(old, new, 1), encoding="utf-8", newline="\n")
    print(f"[patched] {label}")

app_h = main / "application.h"
patch(app_h, "    void Schedule(std::function<void()>&& callback);\n",
      "    void Schedule(std::function<void()>&& callback);\n"
      "    void UpdateChipoMotion(int x_mg, int y_mg, int z_mg);\n",
      "Application motion API")

app_cc = main / "application.cc"
patch(app_cc,
"""void Application::Schedule(std::function<void()>&& callback) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        main_tasks_.push_back(std::move(callback));
    }
    xEventGroupSetBits(event_group_, MAIN_EVENT_SCHEDULE);
}
""",
"""void Application::Schedule(std::function<void()>&& callback) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        main_tasks_.push_back(std::move(callback));
    }
    xEventGroupSetBits(event_group_, MAIN_EVENT_SCHEDULE);
}

void Application::UpdateChipoMotion(int x_mg, int y_mg, int z_mg) {
    if (!chipo_face_) return;
    auto display = Board::GetInstance().GetDisplay();
    DisplayLockGuard lock(display);
    if (lock) chipo_face_->OnMotion(x_mg, y_mg, z_mg);
}
""", "Apply IMU samples on UI task")

board = main / "boards/waveshare/esp32-s3-touch-amoled-1.8-v2/esp32-s3-touch-amoled-1.8-v2.cc"
patch(board, "#include <driver/i2c_master.h>\n",
      "#include <driver/i2c_master.h>\n#include <cstdlib>\n",
      "IMU include")
patch(board, "    i2c_master_bus_handle_t codec_i2c_bus_;\n",
      "    i2c_master_bus_handle_t codec_i2c_bus_;\n"
      "    i2c_master_dev_handle_t motion_device_ = nullptr;\n"
      "    TaskHandle_t motion_task_handle_ = nullptr;\n",
      "QMI8658 handles")

patch(board, "    // 初始化工具\n    void InitializeTools() {",
"""    static esp_err_t ReadMotionRegister(i2c_master_dev_handle_t device, uint8_t reg,
                                        uint8_t* data, size_t length) {
        return i2c_master_transmit_receive(device, &reg, 1, data, length, 50);
    }

    static esp_err_t WriteMotionRegister(i2c_master_dev_handle_t device, uint8_t reg,
                                         uint8_t value) {
        const uint8_t data[] = {reg, value};
        return i2c_master_transmit(device, data, sizeof(data), 50);
    }

    static int16_t DecodeMotionSample(const uint8_t* data) {
        return static_cast<int16_t>(static_cast<uint16_t>(data[0]) |
                                    (static_cast<uint16_t>(data[1]) << 8));
    }

    static void MotionTask(void* arg) {
        auto* self = static_cast<WaveshareEsp32s3TouchAMOLED1inch8*>(arg);
        while (true) {
            uint8_t data[6] = {};
            if (ReadMotionRegister(self->motion_device_, 0x35, data, sizeof(data)) == ESP_OK) {
                const int x = static_cast<int>(DecodeMotionSample(data)) * 1000 / 4096;
                const int y = static_cast<int>(DecodeMotionSample(data + 2)) * 1000 / 4096;
                const int z = static_cast<int>(DecodeMotionSample(data + 4)) * 1000 / 4096;
                Application::GetInstance().Schedule([x, y, z]() {
                    Application::GetInstance().UpdateChipoMotion(x, y, z);
                });
            }
            vTaskDelay(pdMS_TO_TICKS(120));
        }
    }

    void InitializeMotion() {
        esp_err_t result = ESP_ERR_NOT_FOUND;
        const uint8_t addresses[] = {0x6A, 0x6B};
        for (uint8_t address : addresses) {
            if (i2c_master_probe(codec_i2c_bus_, address, 50) != ESP_OK) continue;
            i2c_device_config_t config = {};
            config.device_address = address;
            config.scl_speed_hz = 400000;
            result = i2c_master_bus_add_device(codec_i2c_bus_, &config, &motion_device_);
            if (result == ESP_OK) break;
        }
        if (result != ESP_OK || motion_device_ == nullptr) {
            ESP_LOGW(TAG, "QMI8658 not found; CHIPO motion reactions disabled");
            return;
        }

        uint8_t id = 0;
        result = ReadMotionRegister(motion_device_, 0x00, &id, 1);
        if (result == ESP_OK && id != 0x05) result = ESP_ERR_NOT_FOUND;
        // Accelerometer +/-8g at 250Hz; enable accelerometer.
        if (result == ESP_OK) result = WriteMotionRegister(motion_device_, 0x02, 0x60);
        if (result == ESP_OK) result = WriteMotionRegister(motion_device_, 0x03, 0x25);
        if (result == ESP_OK) result = WriteMotionRegister(motion_device_, 0x08, 0x01);
        if (result != ESP_OK) {
            ESP_LOGW(TAG, "QMI8658 initialization failed: %s", esp_err_to_name(result));
            i2c_master_bus_rm_device(motion_device_);
            motion_device_ = nullptr;
            return;
        }

        if (xTaskCreate(MotionTask, "chipo_motion", 3072, this, 4, &motion_task_handle_) != pdPASS) {
            ESP_LOGW(TAG, "Could not create CHIPO motion task");
            i2c_master_bus_rm_device(motion_device_);
            motion_device_ = nullptr;
            return;
        }
        ESP_LOGI(TAG, "QMI8658 ready: CHIPO tilt and shake reactions enabled");
    }

    // 初始化工具
    void InitializeTools() {""", "QMI8658 polling and shake task")

patch(board, "         InitializeTouch();\n        InitializeButtons();",
      "         InitializeTouch();\n        InitializeMotion();\n        InitializeButtons();",
      "Start QMI8658 after touch")
