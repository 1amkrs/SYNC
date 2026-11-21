#pragma once

#include <cstdint>

#ifdef SYNCAUDIO_EXPORTS
#define SYNC_API extern "C" __declspec(dllexport)
#else
#define SYNC_API extern "C" __declspec(dllimport)
#endif

#pragma pack(push, 8)

struct DeviceInfoNative {
    wchar_t id[128];
    wchar_t name[128];
    wchar_t adapterName[128];
    int32_t transportType;
    int32_t isDefault;
    int32_t isBluetooth;
    uint32_t sampleRate;
    uint32_t channels;
};

struct MasterTelemetryNative {
    uint32_t activeOutputs;
    uint32_t captureSampleRate;
    uint32_t captureChannels;
    float audioLevelPeakL;
    float audioLevelPeakR;
    uint64_t totalCapturedFrames;
    uint64_t totalRenderedFrames;
    int32_t isRunning;
};

struct DeviceTelemetryNative {
    wchar_t deviceId[128];
    uint32_t bufferOccupancyPercent;
    uint32_t underrunCount;
    int32_t estimatedDriftPpm;
    int32_t calibratedDelayMs;
    float measuredLatencyMs;
    float volume;
    int32_t isEnabled;
    int32_t isConnected;
};

#pragma pack(pop)

using EngineEventCallbackNative = void(__stdcall*)(const wchar_t* eventType, const wchar_t* message);

SYNC_API int32_t SyncAudio_CreateEngine(void** outEngineHandle);
SYNC_API int32_t SyncAudio_DestroyEngine(void* engineHandle);

SYNC_API int32_t SyncAudio_EnumerateDevices(void* engineHandle, DeviceInfoNative** outDevices, uint32_t* outCount);
SYNC_API void SyncAudio_FreeDeviceList(DeviceInfoNative* devices);

SYNC_API int32_t SyncAudio_SetCaptureDevice(void* engineHandle, const wchar_t* deviceId);
SYNC_API int32_t SyncAudio_GetCaptureDevice(void* engineHandle, wchar_t* outDeviceId, uint32_t maxLen);

SYNC_API int32_t SyncAudio_AddOutputDevice(void* engineHandle, const wchar_t* deviceId);
SYNC_API int32_t SyncAudio_RemoveOutputDevice(void* engineHandle, const wchar_t* deviceId);
SYNC_API int32_t SyncAudio_ClearOutputDevices(void* engineHandle);

SYNC_API int32_t SyncAudio_SetOutputVolume(void* engineHandle, const wchar_t* deviceId, float volume);
SYNC_API int32_t SyncAudio_GetOutputVolume(void* engineHandle, const wchar_t* deviceId, float* outVolume);

SYNC_API int32_t SyncAudio_SetOutputDelayMs(void* engineHandle, const wchar_t* deviceId, int32_t delayMs);
SYNC_API int32_t SyncAudio_GetOutputDelayMs(void* engineHandle, const wchar_t* deviceId, int32_t* outDelayMs);

SYNC_API int32_t SyncAudio_SetOutputEnabled(void* engineHandle, const wchar_t* deviceId, int32_t enabled);
SYNC_API int32_t SyncAudio_IsOutputEnabled(void* engineHandle, const wchar_t* deviceId, int32_t* outEnabled);

SYNC_API int32_t SyncAudio_Start(void* engineHandle);
SYNC_API int32_t SyncAudio_Stop(void* engineHandle);
SYNC_API int32_t SyncAudio_IsRunning(void* engineHandle, int32_t* outIsRunning);

SYNC_API int32_t SyncAudio_SetMasterVolume(void* engineHandle, float volume);
SYNC_API int32_t SyncAudio_SetMuteAll(void* engineHandle, int32_t mute);

SYNC_API int32_t SyncAudio_PlayTestTone(void* engineHandle, int32_t toneType);
SYNC_API int32_t SyncAudio_StopTestTone(void* engineHandle);

SYNC_API int32_t SyncAudio_SetBaseBufferingDelayMs(void* engineHandle, int32_t delayMs);
SYNC_API int32_t SyncAudio_GetBaseBufferingDelayMs(void* engineHandle, int32_t* outDelayMs);

SYNC_API int32_t SyncAudio_GetMasterTelemetry(void* engineHandle, MasterTelemetryNative* outTelemetry);
SYNC_API int32_t SyncAudio_GetDeviceTelemetry(void* engineHandle, const wchar_t* deviceId, DeviceTelemetryNative* outTelemetry);

SYNC_API int32_t SyncAudio_RegisterEventCallback(void* engineHandle, EngineEventCallbackNative callback);
