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
    case kDeviceStateUpgrading:  // flashing an update: look busy, never idle (nobody should unplug it now)
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
    default:
        return ChipoFace::State::Idle;
    }
}
