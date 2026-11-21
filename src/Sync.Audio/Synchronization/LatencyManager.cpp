#include "LatencyManager.h"
#include <algorithm>

namespace Sync::Audio {

    LatencyManager::LatencyManager() : m_baseBufferingDelayMs(100) {}

    void LatencyManager::SetBaseBufferingDelayMs(int32_t delayMs) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_baseBufferingDelayMs = std::clamp(delayMs, 20, 1000);
    }

    int32_t LatencyManager::GetBaseBufferingDelayMs() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_baseBufferingDelayMs;
    }

    void LatencyManager::SetDeviceOffsetMs(const std::wstring& deviceId, int32_t offsetMs) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_profiles[deviceId].userOffsetMs = std::clamp(offsetMs, -500, 500);
    }

    int32_t LatencyManager::GetDeviceOffsetMs(const std::wstring& deviceId) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_profiles.find(deviceId);
        return (it != m_profiles.end()) ? it->second.userOffsetMs : 0;
    }

    void LatencyManager::SetDeviceWasapiLatencyMs(const std::wstring& deviceId, float wasapiLatencyMs) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_profiles[deviceId].wasapiLatencyMs = std::max(0.0f, wasapiLatencyMs);
    }

    void LatencyManager::RemoveDevice(const std::wstring& deviceId) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_profiles.erase(deviceId);
    }

    void LatencyManager::Recalculate(uint32_t sampleRate) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_profiles.empty()) return;

        // Find minimum user offset across all devices
        int32_t minOffset = 0;
        for (const auto& [id, profile] : m_profiles) {
            if (profile.userOffsetMs < minOffset) {
                minOffset = profile.userOffsetMs;
            }
        }

        // For each device:
        // Relative delay = userOffsetMs - minOffset >= 0
        // Effective delay = baseBufferingDelayMs + relativeDelay
        for (auto& [id, profile] : m_profiles) {
            const int32_t relativeOffset = profile.userOffsetMs - minOffset;
            profile.effectiveDelayMs = static_cast<float>(m_baseBufferingDelayMs + relativeOffset);

            // Compute target buffer fill in frames:
            // Target frames = (effectiveDelayMs * sampleRate) / 1000
            profile.targetBufferFrames = static_cast<size_t>(
                (static_cast<double>(profile.effectiveDelayMs) * static_cast<double>(sampleRate)) / 1000.0
            );
        }
    }

    float LatencyManager::GetEffectiveDelayMs(const std::wstring& deviceId) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_profiles.find(deviceId);
        return (it != m_profiles.end()) ? it->second.effectiveDelayMs : static_cast<float>(m_baseBufferingDelayMs);
    }

    size_t LatencyManager::GetTargetBufferFrames(const std::wstring& deviceId) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_profiles.find(deviceId);
        return (it != m_profiles.end()) ? it->second.targetBufferFrames : 2400;
    }

    void LatencyManager::Clear() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_profiles.clear();
    }

} // namespace Sync::Audio
