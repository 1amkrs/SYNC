#pragma once

#include "Common.h"
#include <vector>
#include <cmath>

namespace Sync::Audio {

    enum class TestSignalType {
        None = 0,
        PeriodicClick = 1,     // 1ms click every 500ms
        LogarithmicChirp = 2,  // 200 Hz to 8000 Hz sweep (50ms) every 1000ms
        DualPulse = 3,         // Two clicks 100ms apart every 1000ms
        SineTone = 4           // 1000 Hz continuous test tone
    };

    class SignalGenerator {
    public:
        SignalGenerator() = default;

        void Initialize(uint32_t sampleRate, uint32_t channels);
        void SetSignalType(TestSignalType type);
        TestSignalType GetSignalType() const { return m_signalType; }

        // Fills the destination buffer with test audio.
        // Returns true if audio was generated, false if None.
        bool Generate(float* buffer, size_t frames);

        // Generates a reference chirp buffer for cross-correlation analysis
        static std::vector<float> GenerateReferenceChirp(uint32_t sampleRate, double durationSec = 0.05, double f0 = 200.0, double f1 = 8000.0);

    private:
        uint32_t m_sampleRate = 48000;
        uint32_t m_channels = 2;
        TestSignalType m_signalType = TestSignalType::None;
        uint64_t m_samplePhase = 0;
    };

} // namespace Sync::Audio
