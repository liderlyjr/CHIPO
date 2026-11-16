#pragma once

#include <mutex>
#include <string>

class ChipoWeather {
public:
    static ChipoWeather& GetInstance();
    bool Start();
    std::string GetDisplayText() const;

private:
    ChipoWeather() = default;
    static void WorkerTask(void* arg);
    bool FetchCurrentWeather();

    mutable std::mutex mutex_;
    std::string display_text_ = "SBD WEATHER --";
    bool started_ = false;
};
