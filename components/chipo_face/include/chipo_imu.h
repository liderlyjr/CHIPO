#pragma once

#ifdef ESP_PLATFORM

#include <atomic>
#include <cstdint>

#include "chipo_tilt.h"
#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

/**
 * Minimal QMI8658 accelerometer + gyroscope reader.
 *
 * It does NOT create an I2C bus: it attaches to the one the XiaoZhi board code already opened
 * (i2c_master_get_bus_handle), so the board files stay untouched. The accelerometer is always on;
 * the gyroscope is switched on too unless CHIPO_ENABLE_GYRO is 0 or the sensor refuses it (then
 * samples simply carry has_gyro = false). A small low-priority task samples at ~25 Hz
 * (50 Hz in the car mode) and queues the readings; the UI task picks it up with Poll(), so no LVGL call ever
 * happens outside the UI task.
 *
 * If the sensor is missing or does not answer, Start() returns false and CHIPO simply works
 * without tilt reactions.
 *
 * Shutdown: Stop() wakes the task at once (it sleeps on a semaphore, even in the slow 1 s mode after
 * repeated errors) and waits until it has really finished before it releases the I2C device. The wait is
 * bounded (a stuck bus cannot hang the caller): in that case Stop() returns false and the object MUST
 * NOT be destroyed, because the task still uses it (ChipoFace leaks it on purpose instead).
 */
class ChipoImu {
public:
    ChipoImu() = default;
    ~ChipoImu();
    ChipoImu(const ChipoImu&) = delete;
    ChipoImu& operator=(const ChipoImu&) = delete;

    bool Start();
    // True once the task has finished and the I2C device is released; false if the task could not be
    // stopped in time (bus stuck). Safe to call again.
    bool Stop();

    // True and fills `out` when an unread sample is available (oldest first). Every sample the
    // task read is kept (a small queue), so a faster rate really delivers every sample.
    bool Poll(ImuSample* out);

    // 50 Hz readout instead of 25 Hz (CHIPO DRIVE needs short jolts). Thread safe.
    void SetFastSampling(bool fast) { fast_.store(fast); }
    // Paused: the task stops touching the shared I2C bus (the touch controller sits on it) until resumed.
    void SetPaused(bool paused) { paused_.store(paused); }

private:
    static void TaskEntry(void* arg);
    void Run();
    bool WriteReg(uint8_t reg, uint8_t value);
    bool ReadRegs(uint8_t reg, uint8_t* data, size_t length);

    i2c_master_dev_handle_t dev_ = nullptr;
    TaskHandle_t task_ = nullptr;
    SemaphoreHandle_t wake_ = nullptr;  // Stop() gives it to end the task's sleep early
    std::atomic<bool> running_{false};
    std::atomic<bool> task_done_{true};
    static constexpr uint32_t kRingSize = 16;
    ImuSample ring_[kRingSize];          // written by the task, read by the UI task
    std::atomic<uint32_t> head_{0};      // samples written so far
    uint32_t tail_ = 0;                  // samples read so far (UI task only)
    std::atomic<bool> fast_{false};
    std::atomic<bool> paused_{false};
    bool gyro_ok_ = false;  // set in Start() before the task starts, then read-only
};

#endif  // ESP_PLATFORM
