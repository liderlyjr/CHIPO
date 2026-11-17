#ifdef ESP_PLATFORM

#include "chipo_imu.h"

#include "chipo_config.h"
#include "esp_log.h"

namespace {
constexpr char TAG[] = "ChipoImu";

// QMI8658 registers (Waveshare SensorLib REG/QMI8658Constants.h).
constexpr uint8_t kRegWhoAmI = 0x00;
constexpr uint8_t kRegCtrl1 = 0x02;
constexpr uint8_t kRegCtrl2 = 0x03;
constexpr uint8_t kRegCtrl5 = 0x06;
constexpr uint8_t kRegCtrl7 = 0x08;
constexpr uint8_t kRegAccelX_L = 0x35;
constexpr uint8_t kRegReset = 0x60;

constexpr uint8_t kWhoAmIValue = 0x05;
constexpr uint8_t kResetCommand = 0xB0;
// CTRL1: address auto-increment on, little-endian data (the register map reads AX_L then AX_H).
constexpr uint8_t kCtrl1Value = 0x40;
// CTRL2: accelerometer full scale +-4 g (0b001 << 4) and output data rate 250 Hz (0b0101).
constexpr uint8_t kCtrl2Value = (1 << 4) | 5;
constexpr float kLsbPerG = 8192.0f;  // 32768 / 4 g
// CTRL5: accelerometer low-pass filter on (mode 0).
constexpr uint8_t kCtrl5Value = 0x01;
// CTRL7: enable the accelerometer only.
constexpr uint8_t kCtrl7Value = 0x01;

constexpr int kI2cTimeoutMs = 50;
constexpr uint32_t kSamplePeriodMs = 40;  // 25 Hz
}  // namespace

ChipoImu::~ChipoImu() {
    Stop();
}

bool ChipoImu::WriteReg(uint8_t reg, uint8_t value) {
    const uint8_t buffer[2] = {reg, value};
    return i2c_master_transmit(dev_, buffer, sizeof(buffer), kI2cTimeoutMs) == ESP_OK;
}

bool ChipoImu::ReadRegs(uint8_t reg, uint8_t* data, size_t length) {
    return i2c_master_transmit_receive(dev_, &reg, 1, data, length, kI2cTimeoutMs) == ESP_OK;
}

bool ChipoImu::Start() {
    if (running_.load()) return true;

    i2c_master_bus_handle_t bus = nullptr;
    esp_err_t err = i2c_master_get_bus_handle(static_cast<i2c_port_num_t>(CHIPO_IMU_I2C_PORT), &bus);
    if (err != ESP_OK || bus == nullptr) {
        ESP_LOGW(TAG, "I2C port %d is not initialised (%s): tilt reactions disabled",
                 CHIPO_IMU_I2C_PORT, esp_err_to_name(err));
        return false;
    }

    uint8_t address = 0;
    for (uint8_t candidate : {static_cast<uint8_t>(CHIPO_IMU_ADDR_PRIMARY),
                              static_cast<uint8_t>(CHIPO_IMU_ADDR_FALLBACK)}) {
        if (i2c_master_probe(bus, candidate, kI2cTimeoutMs) == ESP_OK) {
            address = candidate;
            break;
        }
    }
    if (address == 0) {
        ESP_LOGW(TAG, "QMI8658 not found at 0x%02X or 0x%02X: tilt reactions disabled",
                 CHIPO_IMU_ADDR_PRIMARY, CHIPO_IMU_ADDR_FALLBACK);
        return false;
    }

    i2c_device_config_t config = {};
    config.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    config.device_address = address;
    config.scl_speed_hz = 400000;
    err = i2c_master_bus_add_device(bus, &config, &dev_);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Could not attach to the I2C bus (%s)", esp_err_to_name(err));
        dev_ = nullptr;
        return false;
    }

    uint8_t who = 0;
    if (!ReadRegs(kRegWhoAmI, &who, 1) || who != kWhoAmIValue) {
        ESP_LOGW(TAG, "Device at 0x%02X is not a QMI8658 (WHO_AM_I=0x%02X)", address, who);
        i2c_master_bus_rm_device(dev_);
        dev_ = nullptr;
        return false;
    }

    // Datasheet soft reset, then accelerometer-only configuration.
    bool ok = WriteReg(kRegReset, kResetCommand);
    vTaskDelay(pdMS_TO_TICKS(30));
    ok = ok && WriteReg(kRegCtrl1, kCtrl1Value);
    ok = ok && WriteReg(kRegCtrl2, kCtrl2Value);
    ok = ok && WriteReg(kRegCtrl5, kCtrl5Value);
    ok = ok && WriteReg(kRegCtrl7, kCtrl7Value);
    if (!ok) {
        ESP_LOGW(TAG, "QMI8658 configuration failed");
        i2c_master_bus_rm_device(dev_);
        dev_ = nullptr;
        return false;
    }
    vTaskDelay(pdMS_TO_TICKS(30));

    running_.store(true);
    task_done_.store(false);
    if (xTaskCreate(TaskEntry, "chipo_imu", 3072, this, tskIDLE_PRIORITY + 1, &task_) != pdPASS) {
        ESP_LOGW(TAG, "Could not start the IMU task");
        running_.store(false);
        task_done_.store(true);
        i2c_master_bus_rm_device(dev_);
        dev_ = nullptr;
        return false;
    }
    ESP_LOGI(TAG, "QMI8658 accelerometer ready at 0x%02X (+-4 g, 25 Hz readout)", address);
    return true;
}

void ChipoImu::Stop() {
    if (running_.exchange(false)) {
        for (int i = 0; i < 50 && !task_done_.load(); ++i) vTaskDelay(pdMS_TO_TICKS(10));
    }
    task_ = nullptr;
    if (dev_ != nullptr && task_done_.load()) {
        i2c_master_bus_rm_device(dev_);
        dev_ = nullptr;
    }
}

bool ChipoImu::Poll(ImuSample* out) {
    const uint32_t sequence = sequence_.load();
    if (sequence == polled_sequence_) return false;
    polled_sequence_ = sequence;
    out->x = x_milli_g_.load() / 1000.0f;
    out->y = y_milli_g_.load() / 1000.0f;
    out->z = z_milli_g_.load() / 1000.0f;
    return true;
}

void ChipoImu::TaskEntry(void* arg) {
    static_cast<ChipoImu*>(arg)->Run();
}

void ChipoImu::Run() {
    uint32_t consecutive_errors = 0;
    TickType_t last_wake = xTaskGetTickCount();
    while (running_.load()) {
        uint8_t raw[6];
        if (ReadRegs(kRegAccelX_L, raw, sizeof(raw))) {
            consecutive_errors = 0;
            const int16_t ax = static_cast<int16_t>(raw[0] | (raw[1] << 8));
            const int16_t ay = static_cast<int16_t>(raw[2] | (raw[3] << 8));
            const int16_t az = static_cast<int16_t>(raw[4] | (raw[5] << 8));
            x_milli_g_.store(static_cast<int32_t>(ax * 1000.0f / kLsbPerG));
            y_milli_g_.store(static_cast<int32_t>(ay * 1000.0f / kLsbPerG));
            z_milli_g_.store(static_cast<int32_t>(az * 1000.0f / kLsbPerG));
            sequence_.fetch_add(1);
        } else if (++consecutive_errors == 25) {
            ESP_LOGW(TAG, "IMU not answering; slowing down");
        }
        // After a long run of errors poll once per second instead of hammering the shared bus.
        const uint32_t period = consecutive_errors >= 25 ? 1000 : kSamplePeriodMs;
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(period));
    }
    task_done_.store(true);
    vTaskDelete(nullptr);
}

#endif  // ESP_PLATFORM
