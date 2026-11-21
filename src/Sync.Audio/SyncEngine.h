#pragma once

#include "Common.h"
#include "Rendering/DeviceEnumerator.h"
#include "Capture/WasapiLoopbackCapture.h"
#include "Rendering/WasapiRenderEndpoint.h"
#include "Synchronization/LatencyManager.h"
#include "Synchronization/SignalGenerator.h"
#include "Synchronization/AcousticCalibrator.h"
#include "Synchronization/TimelineClock.h"

#include <vector>
#include <memory>
#include <mutex>
#include <string>
#include <functional>

namespace Sync::Audio {

    using EngineEventCallback = std::function<void(const wchar_t* eventType, const wchar_t* message)>;

    class SyncEngine {
    public:
        SyncEngine();
        ~SyncEngine();

        bool Initialize();
        void Shutdown();

        // Enumerate all active render endpoints
        std::vector<DeviceInfo> EnumerateDevices();

        // Capture device selection
        bool SetCaptureDevice(const std::wstring& deviceId);
        std::wstring GetCaptureDevice() const;

        // Output device management
        bool AddOutputDevice(const std::wstring& deviceId);
        bool RemoveOutputDevice(const std::wstring& deviceId);
        bool IsOutputDeviceSelected(const std::wstring& deviceId) const;
        void ClearOutputDevices();

        // Output device parameter controls
        void SetOutputVolume(const std::wstring& deviceId, float volume);
        float GetOutputVolume(const std::wstring& deviceId);
        void SetOutputDelayMs(const std::wstring& deviceId, int32_t delayMs);
        int32_t GetOutputDelayMs(const std::wstring& deviceId);
        void SetOutputEnabled(const std::wstring& deviceId, bool enabled);
        bool IsOutputEnabled(const std::wstring& deviceId);

        // Global playback controls
        bool Start();
        void Stop();
        bool IsRunning() const;

        void SetMasterVolume(float vol);
        float GetMasterVolume() const { return m_masterVolume.load(); }
        void SetMuteAll(bool mute);
        bool IsMutedAll() const { return m_isMutedAll.load(); }

        // Test signal playback for calibration
        void PlayTestTone(TestSignalType type);
        void StopTestTone();
        TestSignalType GetActiveTestTone() const;

        // Base buffering delay preset (50ms, 100ms, 200ms)
        void SetBaseBufferingDelayMs(int32_t delayMs);
        int32_t GetBaseBufferingDelayMs() const;

        // Telemetry
        MasterTelemetry GetMasterTelemetry() const;
        bool GetDeviceTelemetry(const std::wstring& deviceId, DeviceTelemetry& outTelemetry) const;
        std::vector<DeviceTelemetry> GetAllDeviceTelemetry() const;

        // Set engine event callback for UI notification
        void SetEventCallback(EngineEventCallback callback);

    private:
        void OnAudioCaptured(const float* buffer, size_t frames, uint32_t channels, uint32_t sampleRate, int64_t timestampQpc);
        void OnDeviceHardwareChanged();

        mutable std::mutex m_engineMutex;
        std::atomic<bool> m_isRunning{ false };
        std::atomic<float> m_masterVolume{ 1.0f };
        std::atomic<bool> m_isMutedAll{ false };

        DeviceEnumerator m_deviceEnumerator;
        WasapiLoopbackCapture m_capture;
        std::vector<std::unique_ptr<WasapiRenderEndpoint>> m_renderOutputs;
        LatencyManager m_latencyManager;
        SignalGenerator m_signalGenerator;
        TimelineClock m_timelineClock;

        std::wstring m_captureDeviceId;
        EngineEventCallback m_eventCallback;

        // Pre-allocated distribution buffer
        std::vector<float> m_distBuffer;
        std::atomic<uint64_t> m_totalRenderedFrames{ 0 };
    };

} // namespace Sync::Audio
