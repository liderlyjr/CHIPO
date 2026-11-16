#pragma once

#include "device_state.h"
#include "chipo_face.h"

// Keep this adapter in the XiaoZhi application layer, not in the reusable
// chipo_face component. This keeps the visual component independent of XiaoZhi.
inline ChipoFace::State ChipoStateFromXiaoZhi(DeviceState state) {
    switch (state) {
    case kDeviceStateConnecting:
    case kDeviceStateStarting:
    case kDeviceStateActivating:
    case kDeviceStateWifiConfiguring:
        return ChipoFace::State::Connecting;

    case kDeviceStateListening:
    case kDeviceStateAudioTesting:
        return ChipoFace::State::Listening;

    case kDeviceStateSpeaking:
    case kDeviceStateNotifying:
        return ChipoFace::State::Speaking;

    case kDeviceStateFatalError:
        return ChipoFace::State::Error;

    case kDeviceStateIdle:
    case kDeviceStateUnknown:
    case kDeviceStateUpgrading:
    default:
        return ChipoFace::State::Idle;
    }
}
