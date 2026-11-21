#pragma once

#include "Common.h"
#include <unordered_map>
#include <string>
#include <mutex>

namespace Sync::Audio {

    struct DeviceLatencyProfile {
        int32_t userOffsetMs = 0;        // Configurable -500ms to +500ms
        float wasapiLatencyMs = 10.0f;    // Driver/buffer period latency reported by WASAPI
        float effectiveDelayMs = 0.0f;   // Calculated playback delay to schedule
        size_t targetBufferFrames = 2400; // Target frames in ring buffer for sync
    };

    // Coordinates latency offsets and common buffering delay across all active outputs
    class LatencyManager {
    public:
        LatencyManager();

        // Base buffering delay preset: Low Latency (50ms), Balanced (100ms), Stable (200ms)
        void SetBaseBufferingDelayMs(int32_t delayMs);
        int32_t GetBaseBufferingDelayMs() const;

        // Set manual calibration offset for a device (-500 to +500 ms)
        void SetDeviceOffsetMs(const std::wstring& deviceId, int32_t offsetMs);
        int32_t GetDeviceOffsetMs(const std::wstring& deviceId);

        // Update WASAPI driver latency for a device
        void SetDeviceWasapiLatencyMs(const std::wstring& deviceId, float wasapiLatencyMs);

        // Remove a device profile
        void RemoveDevice(const std::wstring& deviceId);

        // Recalculates effective delays and buffer targets for all registered devices.
        // If any device has a negative relative offset, the common base delay is increased
        // so that the faster device plays earlier relative to the others without negative time travel.
        void Recalculate(uint32_t sampleRate);

        // Get calculated effective delay for a device in milliseconds
        float GetEffectiveDelayMs(const std::wstring& deviceId);

        // Get target buffer frames for a device
        size_t GetTargetBufferFrames(const std::wstring& deviceId);

        // Clear all profiles
        void Clear();

    private:
        mutable std::mutex m_mutex;
        int32_t m_baseBufferingDelayMs = 100; // Default 100ms base buffer
        std::unordered_map<std::wstring, DeviceLatencyProfile> m_profiles;
    };

} // namespace Sync::Audio
