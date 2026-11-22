#pragma once
// Host stand-in for the few FreeRTOS calls the IMU driver uses: tasks are std::thread, a tick is 1 ms.
// Semaphores are really freed by vSemaphoreDelete, so AddressSanitizer catches a task that keeps using one.
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <thread>

typedef uint32_t TickType_t;
typedef int BaseType_t;
typedef void* TaskHandle_t;
typedef void (*TaskFunction_t)(void*);
#define pdPASS 1
#define pdTRUE 1
#define pdFALSE 0
#define pdMS_TO_TICKS(ms) (static_cast<TickType_t>(ms))
#define tskIDLE_PRIORITY 0

struct ChipoStubTaskExit {};  // thrown by vTaskDelete(nullptr) to end the calling "task"

inline TickType_t xTaskGetTickCount() {
    using namespace std::chrono;
    return static_cast<TickType_t>(duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
}
inline void vTaskDelay(TickType_t ticks) { std::this_thread::sleep_for(std::chrono::milliseconds(ticks)); }
inline void vTaskDelayUntil(TickType_t* last, TickType_t increment) {
    *last += increment;
    const TickType_t wait = *last - xTaskGetTickCount();
    if (static_cast<int32_t>(wait) > 0) vTaskDelay(wait);
}
inline BaseType_t xTaskCreate(TaskFunction_t fn, const char*, uint32_t, void* arg, int, TaskHandle_t* out) {
    static int dummy;
    if (out) *out = &dummy;
    std::thread([fn, arg] {
        try {
            fn(arg);
        } catch (const ChipoStubTaskExit&) {
        }
    }).detach();
    return pdPASS;
}
inline void vTaskDelete(TaskHandle_t) { throw ChipoStubTaskExit{}; }

struct ChipoStubSemaphore {
    std::mutex m;
    std::condition_variable cv;
    bool given = false;
};
typedef ChipoStubSemaphore* SemaphoreHandle_t;
inline SemaphoreHandle_t xSemaphoreCreateBinary() { return new ChipoStubSemaphore(); }
inline void vSemaphoreDelete(SemaphoreHandle_t s) { delete s; }
inline BaseType_t xSemaphoreGive(SemaphoreHandle_t s) {
    std::lock_guard<std::mutex> lock(s->m);
    s->given = true;
    s->cv.notify_all();
    return pdTRUE;
}
inline BaseType_t xSemaphoreTake(SemaphoreHandle_t s, TickType_t ticks) {
    std::unique_lock<std::mutex> lock(s->m);
    s->cv.wait_for(lock, std::chrono::milliseconds(ticks), [s] { return s->given; });
    const bool got = s->given;
    s->given = false;
    return got ? pdTRUE : pdFALSE;
}
