#include "DriftEstimator.h"
#include <cmath>

namespace Sync::Audio {

    DriftEstimator::DriftEstimator() {
        Reset(2400, 48000); // 50ms default target
    }

    void DriftEstimator::Reset(size_t targetBufferFrames, uint32_t sampleRate) {
        m_targetBufferFrames = targetBufferFrames;
        m_sampleRate = (sampleRate > 0) ? sampleRate : 48000;
        m_filteredErrorFrames = 0.0;
        m_integralError = 0.0;
        m_currentDriftPpm = 0;
        m_underrunCount = 0;
    }

    int32_t DriftEstimator::Update(size_t currentBufferFrames, uint32_t sampleRate) {
        m_sampleRate = (sampleRate > 0) ? sampleRate : 48000;

        // Instantaneous error: positive = buffer accumulating, negative = buffer draining
        const double rawError = static_cast<double>(currentBufferFrames) - static_cast<double>(m_targetBufferFrames);

        // Low-pass filter the error to eliminate high-frequency scheduling jitter (EWMA with alpha ~ 0.02)
        const double alpha = 0.02;
        m_filteredErrorFrames = (1.0 - alpha) * m_filteredErrorFrames + alpha * rawError;

        // Deadband check: ignore errors smaller than DEADBAND_MS
        const double deadbandFrames = (DEADBAND_MS * m_sampleRate) / 1000.0;
        double effectiveError = 0.0;

        if (std::abs(m_filteredErrorFrames) > deadbandFrames) {
            effectiveError = (m_filteredErrorFrames > 0.0)
                ? (m_filteredErrorFrames - deadbandFrames)
                : (m_filteredErrorFrames + deadbandFrames);
        }

        // Accumulate integral with anti-windup
        m_integralError += effectiveError * (1.0 / m_sampleRate);
        const double maxIntegral = 1000.0;
        m_integralError = std::clamp(m_integralError, -maxIntegral, maxIntegral);

        // Calculate PI output in PPM
        const double pTerm = KP * effectiveError;
        const double iTerm = KI * m_integralError * m_sampleRate;
        const double totalPpm = pTerm + iTerm;

        m_currentDriftPpm = std::clamp(static_cast<int32_t>(std::round(totalPpm)), -MAX_DRIFT_PPM, MAX_DRIFT_PPM);
        return m_currentDriftPpm;
    }

} // namespace Sync::Audio
