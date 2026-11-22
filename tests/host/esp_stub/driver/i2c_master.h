#pragma once
// Fake I2C bus for the IMU lifecycle test. Devices are heap objects that rm_device really deletes, so a
// transfer on a released device is a use-after-free that AddressSanitizer reports.
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <thread>

typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1
inline const char* esp_err_to_name(esp_err_t e) { return e == ESP_OK ? "ESP_OK" : "ESP_FAIL"; }

typedef int i2c_port_num_t;
typedef void* i2c_master_bus_handle_t;
struct ChipoStubI2cDevice {
    uint8_t address;
    volatile int magic = 0x1234;
};
typedef ChipoStubI2cDevice* i2c_master_dev_handle_t;
#define I2C_ADDR_BIT_LEN_7 0
struct i2c_device_config_t {
    int dev_addr_length;
    uint16_t device_address;
    uint32_t scl_speed_hz;
};

// What the fake sensor does when the accelerometer registers are read (WHO_AM_I always answers).
enum class ChipoStubSensor { Works, FailsAfterInit, HangsThenFails };
struct ChipoStubI2c {
    std::atomic<ChipoStubSensor> sensor{ChipoStubSensor::Works};
    std::atomic<int> hang_ms{0};
    std::atomic<int> devices_added{0};
    std::atomic<int> devices_removed{0};
    std::atomic<int> reads{0};
};
inline ChipoStubI2c g_i2c;

inline esp_err_t i2c_master_get_bus_handle(i2c_port_num_t, i2c_master_bus_handle_t* bus) {
    static int dummy;
    *bus = &dummy;
    return ESP_OK;
}
inline esp_err_t i2c_master_probe(i2c_master_bus_handle_t, uint16_t address, int) {
    return address == 0x6B ? ESP_OK : ESP_FAIL;
}
inline esp_err_t i2c_master_bus_add_device(i2c_master_bus_handle_t, const i2c_device_config_t* cfg,
                                           i2c_master_dev_handle_t* out) {
    *out = new ChipoStubI2cDevice{static_cast<uint8_t>(cfg->device_address)};
    ++g_i2c.devices_added;
    return ESP_OK;
}
inline esp_err_t i2c_master_bus_rm_device(i2c_master_dev_handle_t dev) {
    ++g_i2c.devices_removed;
    delete dev;
    return ESP_OK;
}
inline esp_err_t i2c_master_transmit(i2c_master_dev_handle_t dev, const uint8_t*, size_t, int) {
    return dev->magic == 0x1234 ? ESP_OK : ESP_FAIL;  // touching `dev` is what ASan checks
}
inline esp_err_t i2c_master_transmit_receive(i2c_master_dev_handle_t dev, const uint8_t* reg, size_t,
                                             uint8_t* data, size_t length, int) {
    if (dev->magic != 0x1234) return ESP_FAIL;
    for (size_t i = 0; i < length; ++i) data[i] = 0;
    if (reg[0] == 0x00) {  // WHO_AM_I
        data[0] = 0x05;
        return ESP_OK;
    }
    ++g_i2c.reads;
    switch (g_i2c.sensor.load()) {
        case ChipoStubSensor::Works:
            return ESP_OK;
        case ChipoStubSensor::FailsAfterInit:
            return ESP_FAIL;
        case ChipoStubSensor::HangsThenFails:
            std::this_thread::sleep_for(std::chrono::milliseconds(g_i2c.hang_ms.load()));
            return ESP_FAIL;
    }
    return ESP_FAIL;
}
