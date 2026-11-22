// Lifecycle of the QMI8658 reader (components/chipo_face/chipo_imu.cpp), built on a PC with a fake FreeRTOS
// and a fake I2C bus (tests/host/esp_stub). Run with AddressSanitizer: a task that touches its object, its
// semaphore or its I2C device after they were freed makes the run fail.
//
// The case that used to break: after 25 failed reads the task sleeps 1000 ms, Stop() waited only 500 ms and
// the owner then destroyed the object while the task was still alive.
#include <chrono>
#include <cstdio>
#include <memory>
#include <thread>

#include "chipo_imu.h"

static int g_failures = 0;
#define CHECK(cond)                                                                    \
    do {                                                                               \
        if (!(cond)) {                                                                 \
            std::printf("FAIL line %d: %s\n", __LINE__, #cond);                        \
            ++g_failures;                                                              \
        }                                                                              \
    } while (0)

static void SleepMs(int ms) { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }
static int NowMs() {
    using namespace std::chrono;
    return static_cast<int>(duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
}

// The old Stop() returned void; the new one reports whether the task really ended.
static bool StopOk(ChipoImu& imu) {
#ifdef OLD_IMU_API
    imu.Stop();
    return true;
#else
    return imu.Stop();
#endif
}

static void Reset(ChipoStubSensor mode, int hang_ms = 0) {
    g_i2c.sensor.store(mode);
    g_i2c.hang_ms.store(hang_ms);
    g_i2c.devices_added.store(0);
    g_i2c.devices_removed.store(0);
    g_i2c.reads.store(0);
}

static void TestNormalRun() {
    std::printf("-- normal run: samples arrive, Stop releases the device\n");
    Reset(ChipoStubSensor::Works);
    auto imu = std::make_unique<ChipoImu>();
    CHECK(imu->Start());
    SleepMs(250);
    ImuSample sample;
    int got = 0;
    while (imu->Poll(&sample)) ++got;
    CHECK(got >= 3);
    CHECK(StopOk(*imu));
    CHECK(g_i2c.devices_added.load() == 1);
    CHECK(g_i2c.devices_removed.load() == 1);
    imu.reset();
}

static void TestStopWhileSlowAfterErrors() {
    std::printf("-- sensor stops answering: Stop must end the 1 s sleep and never leave the task running\n");
    Reset(ChipoStubSensor::FailsAfterInit);
    auto imu = std::make_unique<ChipoImu>();
    CHECK(imu->Start());
    SleepMs(1300);  // 25 failed reads at 25 Hz, then the task sleeps 1000 ms per read
    const int before = NowMs();
    CHECK(StopOk(*imu));
    const int took = NowMs() - before;
    std::printf("   Stop() took %d ms\n", took);
#ifndef OLD_IMU_API
    CHECK(took < 300);  // woken at once instead of waiting for the sleep to end
    CHECK(g_i2c.devices_removed.load() == 1);
#endif
    imu.reset();  // what ChipoFace::Stop() does right after
    // Let a task that was left running (the old behaviour) touch the freed object: ASan reports it here.
    SleepMs(1600);
#ifndef OLD_IMU_API
    CHECK(g_i2c.devices_removed.load() == 1);
#endif
}

static void TestStopNeverStartedAndTwice() {
    std::printf("-- Stop on an object that never started, and twice\n");
    Reset(ChipoStubSensor::Works);
    ChipoImu idle;
    CHECK(StopOk(idle));
    CHECK(StopOk(idle));
    ChipoImu imu;
    CHECK(imu.Start());
    CHECK(StopOk(imu));
    CHECK(StopOk(imu));
    CHECK(g_i2c.devices_removed.load() == 1);  // released once, not twice
}

static void TestRestartCycles() {
    std::printf("-- 30 start/stop cycles, with and without a healthy sensor\n");
    Reset(ChipoStubSensor::Works);
    for (int i = 0; i < 30; ++i) {
        ChipoImu imu;
        CHECK(imu.Start());
        SleepMs(i % 3 == 0 ? 5 : 45);
        CHECK(StopOk(imu));
    }
    CHECK(g_i2c.devices_added.load() == 30);
    CHECK(g_i2c.devices_removed.load() == 30);
    Reset(ChipoStubSensor::FailsAfterInit);
    for (int i = 0; i < 5; ++i) {
        ChipoImu imu;
        CHECK(imu.Start());
        SleepMs(30);
        CHECK(StopOk(imu));
    }
    CHECK(g_i2c.devices_added.load() == g_i2c.devices_removed.load());
}

#ifndef OLD_IMU_API
static void TestStuckBus() {
    std::printf("-- I2C transfer stuck for 2.5 s: Stop gives up after ~1 s, keeps everything alive, retries OK\n");
    Reset(ChipoStubSensor::HangsThenFails, 2500);
    auto imu = std::make_unique<ChipoImu>();
    CHECK(imu->Start());
    SleepMs(150);  // the task is now inside the stuck transfer
    int before = NowMs();
    CHECK(!imu->Stop());  // cannot be stopped yet: the object must not be destroyed
    int took = NowMs() - before;
    std::printf("   first Stop() gave up after %d ms\n", took);
    CHECK(took >= 900 && took < 1600);  // bounded: no indefinite block
    CHECK(g_i2c.devices_removed.load() == 0);  // the device is still in use: not released
    const int added = g_i2c.devices_added.load();
    CHECK(!imu->Start());  // no second task while the first one still lives
    CHECK(g_i2c.devices_added.load() == added);
    bool stopped = false;  // once the transfer ends the task finishes and Stop() can release everything
    for (int attempt = 0; attempt < 5 && !stopped; ++attempt) stopped = imu->Stop();
    CHECK(stopped);
    CHECK(g_i2c.devices_removed.load() == 1);
    imu.reset();
    SleepMs(300);
}
#endif

int main() {
    TestNormalRun();
    TestStopWhileSlowAfterErrors();
    TestStopNeverStartedAndTwice();
    TestRestartCycles();
#ifndef OLD_IMU_API
    TestStuckBus();
#endif
    if (g_failures) {
        std::printf("%d check(s) failed\n", g_failures);
        return 1;
    }
    std::printf("IMU lifecycle OK\n");
    return 0;
}
