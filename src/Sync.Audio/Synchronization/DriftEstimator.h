#pragma once

#include "Common.h"
#include <cstdint>
#include <algorithm>

namespace Sync::Audio {

    // PI controller based Clock Drift Estimator.
    // Tracks long-term buffer occupancy deviation to estimate crystal clock skew in PPM.
    class DriftEstimator {
    public:
        DriftEstimator();

        void Reset(size_t targetBufferFrames, uint32_t sampleRate);

        // Update with current buffer fill level
        // Returns estimated drift correction in parts-per-million (PPM)
        int32_t Update(size_t currentBufferFrames, uint32_t sampleRate);

        int32_t CurrentDriftPpm() const { return m_currentDriftPpm; }
        double FilteredErrorFrames() const { return m_filteredErrorFrames; }
        uint32_t UnderrunCount() const { return m_underrunCount; }
        void RecordUnderrun() { m_underrunCount++; }

    private:
        size_t m_targetBufferFrames = 0;
        uint32_t m_sampleRate = 48000;

        double m_filteredErrorFrames = 0.0;
        double m_integralError = 0.0;
        int32_t m_currentDriftPpm = 0;
        uint32_t m_underrunCount = 0;

        // Controller gains
        // Kp: proportional gain (PPM per frame error)
        // Ki: integral gain (PPM accumulation per second)
        static constexpr double KP = 0.05;
        static constexpr double KI = 0.002;
        static constexpr double DEADBAND_MS = 3.0; // 3 ms deadband to ignore OS buffer jitter
        static constexpr int32_t MAX_DRIFT_PPM = 1200; // ±1200 PPM limit (normal crystal skew is ±50-200 PPM)
    };

} // namespace Sync::Audio
