#include "Resampler.h"
#include <cmath>
#include <algorithm>

namespace Sync::Audio {

    Resampler::Resampler() {
        Initialize(48000, 48000, 2);
    }

    void Resampler::Initialize(uint32_t inSampleRate, uint32_t outSampleRate, uint32_t channels) {
        m_inSampleRate = (inSampleRate > 0) ? inSampleRate : 48000;
        m_outSampleRate = (outSampleRate > 0) ? outSampleRate : 48000;
        m_channels = (channels > 0) ? channels : 2;

        m_nominalRatio = static_cast<double>(m_inSampleRate) / static_cast<double>(m_outSampleRate);
        m_targetRatio = m_nominalRatio;
        m_currentRatio = m_nominalRatio;
        m_fractionalPhase = 0.0;

        m_history.assign(4 * m_channels, 0.0f);
        m_hasHistory = false;
    }

    void Resampler::SetDriftPpm(int32_t driftPpm) {
        // Clamp drift within realistic bounds: -5000 PPM to +5000 PPM (±0.5%)
        driftPpm = std::clamp(driftPpm, -5000, 5000);
        const double driftFactor = 1.0 + (static_cast<double>(driftPpm) / 1000000.0);
        m_targetRatio = m_nominalRatio * driftFactor;
    }

    void Resampler::Reset() {
        m_currentRatio = m_targetRatio;
        m_fractionalPhase = 0.0;
        std::fill(m_history.begin(), m_history.end(), 0.0f);
        m_hasHistory = false;
    }

    size_t Resampler::Process(const float* inBuffer, size_t inFrames, float* outBuffer, size_t maxOutFrames) {
        if (!inBuffer || !outBuffer || inFrames == 0 || maxOutFrames == 0) {
            return 0;
        }

        // If sample rates and drift are identical (1.0 ratio) and no fractional phase, fast copy
        if (m_inSampleRate == m_outSampleRate && std::abs(m_currentRatio - 1.0) < 1e-9 && std::abs(m_targetRatio - 1.0) < 1e-9 && m_fractionalPhase < 1e-9 && !m_hasHistory) {
            const size_t framesToCopy = std::min(inFrames, maxOutFrames);
            std::memcpy(outBuffer, inBuffer, framesToCopy * m_channels * sizeof(float));
            return framesToCopy;
        }

        size_t inIndex = 0;
        size_t outIndex = 0;

        // Initialize history with initial samples if first time
        if (!m_hasHistory && inFrames > 0) {
            for (uint32_t ch = 0; ch < m_channels; ++ch) {
                const float firstSample = inBuffer[ch];
                m_history[0 * m_channels + ch] = firstSample;
                m_history[1 * m_channels + ch] = firstSample;
                m_history[2 * m_channels + ch] = (inFrames > 1) ? inBuffer[m_channels + ch] : firstSample;
                m_history[3 * m_channels + ch] = (inFrames > 2) ? inBuffer[2 * m_channels + ch] : firstSample;
            }
            m_hasHistory = true;
        }

        // Generate output frames
        while (outIndex < maxOutFrames) {
            // Slew currentRatio toward targetRatio to avoid audible clicks or instant pitch shift
            if (m_currentRatio < m_targetRatio) {
                m_currentRatio = std::min(m_targetRatio, m_currentRatio + MAX_RATIO_SLEW_PER_FRAME);
            } else if (m_currentRatio > m_targetRatio) {
                m_currentRatio = std::max(m_targetRatio, m_currentRatio - MAX_RATIO_SLEW_PER_FRAME);
            }

            // Check if we need to advance input frames
            while (m_fractionalPhase >= 1.0) {
                m_fractionalPhase -= 1.0;
                inIndex++;

                // Shift history window: [y0, y1, y2, y3] -> [y1, y2, y3, newSample]
                for (uint32_t ch = 0; ch < m_channels; ++ch) {
                    m_history[0 * m_channels + ch] = m_history[1 * m_channels + ch];
                    m_history[1 * m_channels + ch] = m_history[2 * m_channels + ch];
                    m_history[2 * m_channels + ch] = m_history[3 * m_channels + ch];

                    if (inIndex + 1 < inFrames) {
                        m_history[3 * m_channels + ch] = inBuffer[(inIndex + 1) * m_channels + ch];
                    } else if (inIndex < inFrames) {
                        m_history[3 * m_channels + ch] = inBuffer[inIndex * m_channels + ch];
                    }
                }

                if (inIndex >= inFrames) {
                    // Consumed all input frames available in this batch
                    return outIndex;
                }
            }

            // Interpolate output sample for all channels
            const float mu = static_cast<float>(m_fractionalPhase);
            for (uint32_t ch = 0; ch < m_channels; ++ch) {
                const float y0 = m_history[0 * m_channels + ch];
                const float y1 = m_history[1 * m_channels + ch];
                const float y2 = m_history[2 * m_channels + ch];
                const float y3 = m_history[3 * m_channels + ch];

                const float interpolated = HermiteInterpolate(y0, y1, y2, y3, mu);
                outBuffer[outIndex * m_channels + ch] = interpolated;
            }

            outIndex++;
            m_fractionalPhase += m_currentRatio;
        }

        return outIndex;
    }

} // namespace Sync::Audio
