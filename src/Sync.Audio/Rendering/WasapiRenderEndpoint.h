#pragma once

#include "Common.h"
#include "../Buffering/AudioRingBuffer.h"
#include "../Resampling/Resampler.h"
#include "../Synchronization/DriftEstimator.h"
#include <thread>
#include <atomic>
#include <vector>
#include <string>

namespace Sync::Audio {

    class WasapiRenderEndpoint {
    public:
        explicit WasapiRenderEndpoint(const DeviceInfo& info);
        ~WasapiRenderEndpoint();

        // Initialize and open WASAPI shared-mode render client
        bool Initialize();
        void Shutdown();

        // Start real-time render thread
        bool Start();
        void Stop();

        bool IsRunning() const { return m_isRunning.load(std::memory_order_relaxed); }
        bool IsEnabled() const { return m_isEnabled.load(std::memory_order_relaxed); }
        void SetEnabled(bool enabled) { m_isEnabled.store(enabled, std::memory_order_relaxed); }

        // Volume control [0.0f, 1.0f]
        void SetVolume(float vol) { m_volume.store(std::clamp(vol, 0.0f, 1.0f), std::memory_order_relaxed); }
        float GetVolume() const { return m_volume.load(std::memory_order_relaxed); }

        // Manual calibrated delay offset in milliseconds (-500 to +500 ms)
        void SetDelayOffsetMs(int32_t delayMs) { m_delayOffsetMs.store(std::clamp(delayMs, -500, 500), std::memory_order_relaxed); }
        int32_t GetDelayOffsetMs() const { return m_delayOffsetMs.load(std::memory_order_relaxed); }

        // Target buffer frames calculated by LatencyManager
        void SetTargetBufferFrames(size_t targetFrames);

        // Writes stereo float audio frames into this device's queue
        size_t PushAudio(const float* stereoFloatData, size_t frames, uint32_t sampleRate);

        // Collect real-time telemetry
        DeviceTelemetry GetTelemetry() const;

        const DeviceInfo& GetInfo() const { return m_info; }
        const std::wstring& GetId() const { return m_info.id; }
        const std::wstring& GetName() const { return m_info.name; }

    private:
        void RenderThreadProc();
        void FillRenderBuffer(BYTE* pRenderData, size_t framesToFill);

        DeviceInfo m_info;

        std::atomic<bool> m_isRunning{ false };
        std::atomic<bool> m_isEnabled{ true };
        std::atomic<bool> m_stopRequested{ false };
        std::atomic<bool> m_isDeviceValid{ true };
        std::thread m_renderThread;

        ComPtr<IMMDevice> m_device;
        ComPtr<IAudioClient> m_audioClient;
        ComPtr<IAudioRenderClient> m_renderClient;
        HANDLE m_renderEvent = nullptr;

        UINT32 m_endpointBufferFrames = 0;
        WAVEFORMATEXTENSIBLE m_mixFormatExt{};
        bool m_isFloatFormat = true;
        uint16_t m_bitsPerSample = 32;
        uint16_t m_channels = 2;
        uint32_t m_sampleRate = 48000;
        float m_wasapiPeriodMs = 10.0f;

        // Synchronization & Buffering
        AudioRingBuffer m_ringBuffer;      // Holds incoming float stereo frames
        Resampler m_resampler;             // Converts to endpoint native sample rate & corrects clock drift
        DriftEstimator m_driftEstimator;   // Monitors buffer level and estimates drift PPM

        std::atomic<float> m_volume{ 1.0f };
        std::atomic<int32_t> m_delayOffsetMs{ 0 };
        std::atomic<size_t> m_targetBufferFrames{ 2400 };

        // Internal scratch buffers for format conversion
        std::vector<float> m_resampleInputBuffer;
        std::vector<float> m_resampleOutputBuffer;

        // Telemetry counters
        std::atomic<uint32_t> m_underrunCount{ 0 };
        std::atomic<uint32_t> m_bufferOccupancyPercent{ 0 };
        std::atomic<int32_t> m_estimatedDriftPpm{ 0 };
    };

} // namespace Sync::Audio
