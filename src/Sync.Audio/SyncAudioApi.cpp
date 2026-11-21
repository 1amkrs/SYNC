#ifndef SYNCAUDIO_EXPORTS
#define SYNCAUDIO_EXPORTS
#endif
#include "SyncAudioApi.h"
#include "SyncEngine.h"

using namespace Sync::Audio;

SYNC_API int32_t SyncAudio_CreateEngine(void** outEngineHandle) {
    if (!outEngineHandle) return -1;
    try {
        auto* engine = new SyncEngine();
        if (!engine->Initialize()) {
            delete engine;
            *outEngineHandle = nullptr;
            return -2;
        }
        *outEngineHandle = engine;
        return 0;
    } catch (...) {
        return -3;
    }
}

SYNC_API int32_t SyncAudio_DestroyEngine(void* engineHandle) {
    if (!engineHandle) return -1;
    try {
        auto* engine = static_cast<SyncEngine*>(engineHandle);
        engine->Shutdown();
        delete engine;
        return 0;
    } catch (...) {
        return -2;
    }
}

SYNC_API int32_t SyncAudio_EnumerateDevices(void* engineHandle, DeviceInfoNative** outDevices, uint32_t* outCount) {
    if (!engineHandle || !outDevices || !outCount) return -1;
    try {
        auto* engine = static_cast<SyncEngine*>(engineHandle);
        auto devices = engine->EnumerateDevices();

        *outCount = static_cast<uint32_t>(devices.size());
        if (devices.empty()) {
            *outDevices = nullptr;
            return 0;
        }

        auto* nativeArray = new DeviceInfoNative[devices.size()];
        for (size_t i = 0; i < devices.size(); ++i) {
            std::memset(&nativeArray[i], 0, sizeof(DeviceInfoNative));
            wcsncpy_s(nativeArray[i].id, devices[i].id.c_str(), _TRUNCATE);
            wcsncpy_s(nativeArray[i].name, devices[i].name.c_str(), _TRUNCATE);
            wcsncpy_s(nativeArray[i].adapterName, devices[i].adapterName.c_str(), _TRUNCATE);
            nativeArray[i].transportType = static_cast<int32_t>(devices[i].transportType);
            nativeArray[i].isDefault = devices[i].isDefault ? 1 : 0;
            nativeArray[i].isBluetooth = devices[i].isBluetooth ? 1 : 0;
            nativeArray[i].sampleRate = devices[i].nativeFormat.sampleRate;
            nativeArray[i].channels = devices[i].nativeFormat.channels;
        }

        *outDevices = nativeArray;
        return 0;
    } catch (...) {
        return -2;
    }
}

SYNC_API void SyncAudio_FreeDeviceList(DeviceInfoNative* devices) {
    delete[] devices;
}

SYNC_API int32_t SyncAudio_SetCaptureDevice(void* engineHandle, const wchar_t* deviceId) {
    if (!engineHandle || !deviceId) return -1;
    auto* engine = static_cast<SyncEngine*>(engineHandle);
    return engine->SetCaptureDevice(deviceId) ? 0 : -2;
}

SYNC_API int32_t SyncAudio_GetCaptureDevice(void* engineHandle, wchar_t* outDeviceId, uint32_t maxLen) {
    if (!engineHandle || !outDeviceId || maxLen == 0) return -1;
    auto* engine = static_cast<SyncEngine*>(engineHandle);
    std::wstring dev = engine->GetCaptureDevice();
    wcsncpy_s(outDeviceId, maxLen, dev.c_str(), _TRUNCATE);
    return 0;
}

SYNC_API int32_t SyncAudio_AddOutputDevice(void* engineHandle, const wchar_t* deviceId) {
    if (!engineHandle || !deviceId) return -1;
    auto* engine = static_cast<SyncEngine*>(engineHandle);
    return engine->AddOutputDevice(deviceId) ? 0 : -2;
}

SYNC_API int32_t SyncAudio_RemoveOutputDevice(void* engineHandle, const wchar_t* deviceId) {
    if (!engineHandle || !deviceId) return -1;
    auto* engine = static_cast<SyncEngine*>(engineHandle);
    return engine->RemoveOutputDevice(deviceId) ? 0 : -2;
}

SYNC_API int32_t SyncAudio_ClearOutputDevices(void* engineHandle) {
    if (!engineHandle) return -1;
    auto* engine = static_cast<SyncEngine*>(engineHandle);
    engine->ClearOutputDevices();
    return 0;
}

SYNC_API int32_t SyncAudio_SetOutputVolume(void* engineHandle, const wchar_t* deviceId, float volume) {
    if (!engineHandle || !deviceId) return -1;
    auto* engine = static_cast<SyncEngine*>(engineHandle);
    engine->SetOutputVolume(deviceId, volume);
    return 0;
}

SYNC_API int32_t SyncAudio_GetOutputVolume(void* engineHandle, const wchar_t* deviceId, float* outVolume) {
    if (!engineHandle || !deviceId || !outVolume) return -1;
    auto* engine = static_cast<SyncEngine*>(engineHandle);
    *outVolume = engine->GetOutputVolume(deviceId);
    return 0;
}

SYNC_API int32_t SyncAudio_SetOutputDelayMs(void* engineHandle, const wchar_t* deviceId, int32_t delayMs) {
    if (!engineHandle || !deviceId) return -1;
    auto* engine = static_cast<SyncEngine*>(engineHandle);
    engine->SetOutputDelayMs(deviceId, delayMs);
    return 0;
}

SYNC_API int32_t SyncAudio_GetOutputDelayMs(void* engineHandle, const wchar_t* deviceId, int32_t* outDelayMs) {
    if (!engineHandle || !deviceId || !outDelayMs) return -1;
    auto* engine = static_cast<SyncEngine*>(engineHandle);
    *outDelayMs = engine->GetOutputDelayMs(deviceId);
    return 0;
}

SYNC_API int32_t SyncAudio_SetOutputEnabled(void* engineHandle, const wchar_t* deviceId, int32_t enabled) {
    if (!engineHandle || !deviceId) return -1;
    auto* engine = static_cast<SyncEngine*>(engineHandle);
    engine->SetOutputEnabled(deviceId, enabled != 0);
    return 0;
}

SYNC_API int32_t SyncAudio_IsOutputEnabled(void* engineHandle, const wchar_t* deviceId, int32_t* outEnabled) {
    if (!engineHandle || !deviceId || !outEnabled) return -1;
    auto* engine = static_cast<SyncEngine*>(engineHandle);
    *outEnabled = engine->IsOutputEnabled(deviceId) ? 1 : 0;
    return 0;
}

SYNC_API int32_t SyncAudio_Start(void* engineHandle) {
    if (!engineHandle) return -1;
    auto* engine = static_cast<SyncEngine*>(engineHandle);
    return engine->Start() ? 0 : -2;
}

SYNC_API int32_t SyncAudio_Stop(void* engineHandle) {
    if (!engineHandle) return -1;
    auto* engine = static_cast<SyncEngine*>(engineHandle);
    engine->Stop();
    return 0;
}

SYNC_API int32_t SyncAudio_IsRunning(void* engineHandle, int32_t* outIsRunning) {
    if (!engineHandle || !outIsRunning) return -1;
    auto* engine = static_cast<SyncEngine*>(engineHandle);
    *outIsRunning = engine->IsRunning() ? 1 : 0;
    return 0;
}

SYNC_API int32_t SyncAudio_SetMasterVolume(void* engineHandle, float volume) {
    if (!engineHandle) return -1;
    auto* engine = static_cast<SyncEngine*>(engineHandle);
    engine->SetMasterVolume(volume);
    return 0;
}

SYNC_API int32_t SyncAudio_SetMuteAll(void* engineHandle, int32_t mute) {
    if (!engineHandle) return -1;
    auto* engine = static_cast<SyncEngine*>(engineHandle);
    engine->SetMuteAll(mute != 0);
    return 0;
}

SYNC_API int32_t SyncAudio_PlayTestTone(void* engineHandle, int32_t toneType) {
    if (!engineHandle) return -1;
    auto* engine = static_cast<SyncEngine*>(engineHandle);
    engine->PlayTestTone(static_cast<TestSignalType>(toneType));
    return 0;
}

SYNC_API int32_t SyncAudio_StopTestTone(void* engineHandle) {
    if (!engineHandle) return -1;
    auto* engine = static_cast<SyncEngine*>(engineHandle);
    engine->StopTestTone();
    return 0;
}

SYNC_API int32_t SyncAudio_SetBaseBufferingDelayMs(void* engineHandle, int32_t delayMs) {
    if (!engineHandle) return -1;
    auto* engine = static_cast<SyncEngine*>(engineHandle);
    engine->SetBaseBufferingDelayMs(delayMs);
    return 0;
}

SYNC_API int32_t SyncAudio_GetBaseBufferingDelayMs(void* engineHandle, int32_t* outDelayMs) {
    if (!engineHandle || !outDelayMs) return -1;
    auto* engine = static_cast<SyncEngine*>(engineHandle);
    *outDelayMs = engine->GetBaseBufferingDelayMs();
    return 0;
}

SYNC_API int32_t SyncAudio_GetMasterTelemetry(void* engineHandle, MasterTelemetryNative* outTelemetry) {
    if (!engineHandle || !outTelemetry) return -1;
    auto* engine = static_cast<SyncEngine*>(engineHandle);
    MasterTelemetry t = engine->GetMasterTelemetry();
    outTelemetry->activeOutputs = t.activeOutputs;
    outTelemetry->captureSampleRate = t.captureSampleRate;
    outTelemetry->captureChannels = t.captureChannels;
    outTelemetry->audioLevelPeakL = t.audioLevelPeakL;
    outTelemetry->audioLevelPeakR = t.audioLevelPeakR;
    outTelemetry->totalCapturedFrames = t.totalCapturedFrames;
    outTelemetry->totalRenderedFrames = t.totalRenderedFrames;
    outTelemetry->isRunning = t.isRunning ? 1 : 0;
    return 0;
}

SYNC_API int32_t SyncAudio_GetDeviceTelemetry(void* engineHandle, const wchar_t* deviceId, DeviceTelemetryNative* outTelemetry) {
    if (!engineHandle || !deviceId || !outTelemetry) return -1;
    auto* engine = static_cast<SyncEngine*>(engineHandle);
    DeviceTelemetry t{};
    if (engine->GetDeviceTelemetry(deviceId, t)) {
        wcsncpy_s(outTelemetry->deviceId, t.deviceId, _TRUNCATE);
        outTelemetry->bufferOccupancyPercent = t.bufferOccupancyPercent;
        outTelemetry->underrunCount = t.underrunCount;
        outTelemetry->estimatedDriftPpm = t.estimatedDriftPpm;
        outTelemetry->calibratedDelayMs = t.calibratedDelayMs;
        outTelemetry->measuredLatencyMs = t.measuredLatencyMs;
        outTelemetry->volume = t.volume;
        outTelemetry->isEnabled = t.isEnabled ? 1 : 0;
        outTelemetry->isConnected = t.isConnected ? 1 : 0;
        return 0;
    }
    return -2;
}

SYNC_API int32_t SyncAudio_RegisterEventCallback(void* engineHandle, EngineEventCallbackNative callback) {
    if (!engineHandle) return -1;
    auto* engine = static_cast<SyncEngine*>(engineHandle);
    if (callback) {
        engine->SetEventCallback([callback](const wchar_t* evType, const wchar_t* msg) {
            callback(evType, msg);
        });
    } else {
        engine->SetEventCallback(nullptr);
    }
    return 0;
}
