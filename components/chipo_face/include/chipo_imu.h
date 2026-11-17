#pragma once

#ifdef ESP_PLATFORM

#include <atomic>
#include <cstdint>

#include "chipo_tilt.h"
#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/**
 * Minimal QMI8658 accelerometer reader.
 *
 * It does NOT create an I2C bus: it attaches to the one the XiaoZhi board code already opened
 * (i2c_master_get_bus_handle), so the board files stay untouched. Only the accelerometer is
 * enabled (the gyroscope stays off to save power). A small low-priority task samples at ~25 Hz
 * and publishes the latest reading; the UI task picks it up with Poll(), so no LVGL call ever
 * happens outside the UI task.
 *
 * If the sensor is missing or does not answer, Start() returns false and CHIPO simply works
 * without tilt reactions.
 */
class ChipoImu {
public:
    ChipoImu() = default;
    ~ChipoImu();
    ChipoImu(const ChipoImu&) = delete;
    ChipoImu& operator=(const ChipoImu&) = delete;

    bool Start();
    void Stop();

    // True when a sample newer than the last Poll() is available.
    bool Poll(ImuSample* out);

private:
    static void TaskEntry(void* arg);
    void Run();
    bool WriteReg(uint8_t reg, uint8_t value);
    bool ReadRegs(uint8_t reg, uint8_t* data, size_t length);

    i2c_master_dev_handle_t dev_ = nullptr;
    TaskHandle_t task_ = nullptr;
    std::atomic<bool> running_{false};
    std::atomic<bool> task_done_{true};
    std::atomic<int32_t> x_milli_g_{0};
    std::atomic<int32_t> y_milli_g_{0};
    std::atomic<int32_t> z_milli_g_{0};
    std::atomic<uint32_t> sequence_{0};
    uint32_t polled_sequence_ = 0;
};

#endif  // ESP_PLATFORM
