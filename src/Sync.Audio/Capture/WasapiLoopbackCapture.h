#pragma once

#include "Common.h"
#include <thread>
#include <atomic>
#include <functional>
#include <vector>

namespace Sync::Audio {

    using AudioCaptureCallback = std::function<void(const float* buffer, size_t frames, uint32_t channels, uint32_t sampleRate, int64_t timestampQpc)>;

    class WasapiLoopbackCapture {
    public:
        WasapiLoopbackCapture();
        ~WasapiLoopbackCapture();

        // Start capture on specified device (empty deviceId means Windows Default Multimedia Render Device)
        bool Start(const std::wstring& deviceId, AudioCaptureCallback callback);

        // Stop capture
        void Stop();

        bool IsRunning() const { return m_isRunning.load(std::memory_order_relaxed); }

        // Returns current capture format
        AudioFormat GetCaptureFormat() const { return m_captureFormat; }
        std::wstring GetCaptureDeviceId() const { return m_activeDeviceId; }

        // Audio peak levels [0.0, 1.0] for UI VU meter
        float GetPeakLevelL() const { return m_peakL.load(std::memory_order_relaxed); }
        float GetPeakLevelR() const { return m_peakR.load(std::memory_order_relaxed); }
        uint64_t GetTotalCapturedFrames() const { return m_totalCapturedFrames.load(std::memory_order_relaxed); }

    private:
        void CaptureThreadProc();
        bool InitializeClient();
        void CleanupClient();
        void ConvertToFloatStereo(const BYTE* pData, size_t numFrames, float* outStereo);

        std::atomic<bool> m_isRunning{ false };
        std::atomic<bool> m_stopRequested{ false };
        std::thread m_captureThread;

        std::wstring m_activeDeviceId;
        AudioCaptureCallback m_callback;

        ComPtr<IMMDevice> m_device;
        ComPtr<IAudioClient> m_audioClient;
        ComPtr<IAudioCaptureClient> m_captureClient;
        HANDLE m_captureEvent = nullptr;

        AudioFormat m_captureFormat;
        WAVEFORMATEXTENSIBLE m_waveFormatExt{};
        bool m_isFloatFormat = true;
        uint16_t m_bitsPerSample = 32;
        uint16_t m_channels = 2;
        uint32_t m_sampleRate = 48000;

        // Level metering
        std::atomic<float> m_peakL{ 0.0f };
        std::atomic<float> m_peakR{ 0.0f };
        std::atomic<uint64_t> m_totalCapturedFrames{ 0 };

        // Internal scratch buffer for float stereo conversion (reused across cycles, no reallocations)
        std::vector<float> m_conversionBuffer;
    };

} // namespace Sync::Audio
