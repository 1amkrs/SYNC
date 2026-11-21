#pragma once

#include "Common.h"
#include <vector>
#include <cstdint>

namespace Sync::Audio {

    // Asynchronous Sample Rate Converter (ASRC) with cubic Hermite interpolation and adaptive drift correction.
    class Resampler {
    public:
        Resampler();
        ~Resampler() = default;

        void Initialize(uint32_t inSampleRate, uint32_t outSampleRate, uint32_t channels);

        // Sets dynamic drift adjustment in parts-per-million (e.g. +50 means output consumes faster, -50 means slower)
        void SetDriftPpm(int32_t driftPpm);

        // Process input samples into output samples.
        // inFrames: number of input frames available
        // outBuffer: destination buffer for interleaved output samples
        // maxOutFrames: capacity of outBuffer in frames
        // Returns number of frames generated in outBuffer.
        size_t Process(const float* inBuffer, size_t inFrames, float* outBuffer, size_t maxOutFrames);

        // Reset filter state
        void Reset();

        uint32_t InSampleRate() const { return m_inSampleRate; }
        uint32_t OutSampleRate() const { return m_outSampleRate; }
        uint32_t Channels() const { return m_channels; }
        double CurrentRatio() const { return m_currentRatio; }

    private:
        // Cubic Hermite interpolation for smooth sample stepping
        inline float HermiteInterpolate(float y0, float y1, float y2, float y3, float mu) const {
            const float mu2 = mu * mu;
            const float a0 = -0.5f * y0 + 1.5f * y1 - 1.5f * y2 + 0.5f * y3;
            const float a1 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
            const float a2 = -0.5f * y0 + 0.5f * y2;
            const float a3 = y1;
            return a0 * mu * mu2 + a1 * mu2 + a2 * mu + a3;
        }

        uint32_t m_inSampleRate = 48000;
        uint32_t m_outSampleRate = 48000;
        uint32_t m_channels = 2;

        double m_nominalRatio = 1.0;
        double m_targetRatio = 1.0;
        double m_currentRatio = 1.0;

        // Fractional input phase position [0.0, 1.0)
        double m_fractionalPhase = 0.0;

        // History samples for 4-point Hermite interpolation per channel: y0, y1, y2, y3
        std::vector<float> m_history; // size: 4 * m_channels
        bool m_hasHistory = false;

        // Maximum drift slew rate per processed output frame (prevents sudden pitch bends)
        static constexpr double MAX_RATIO_SLEW_PER_FRAME = 0.0000005; // ~0.5 PPM per frame
    };

} // namespace Sync::Audio
