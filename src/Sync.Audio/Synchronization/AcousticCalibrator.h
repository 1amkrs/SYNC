#pragma once

#include "Common.h"
#include <vector>
#include <string>

namespace Sync::Audio {

    struct CalibrationResult {
        bool success = false;
        int32_t suggestedOffsetMsA = 0;
        int32_t suggestedOffsetMsB = 0;
        float correlationScoreA = 0.0f;
        float correlationScoreB = 0.0f;
        float measuredLatencyDiffMs = 0.0f;
        std::wstring statusMessage;
    };

    class AcousticCalibrator {
    public:
        AcousticCalibrator() = default;

        // Perform cross-correlation of recorded microphone signal with the reference chirp.
        // Returns the peak delay in frames, and correlation confidence score [0.0, 1.0].
        static bool AnalyzeChirpArrival(
            const float* micSignal,
            size_t micFrames,
            const float* referenceChirp,
            size_t chirpFrames,
            size_t& outPeakIndex,
            float& outPeakScore
        );

        // Calculate suggested synchronization offsets given arrival times for speaker A and speaker B
        static CalibrationResult CalculateOffsets(
            size_t arrivalFramesA,
            float scoreA,
            size_t arrivalFramesB,
            float scoreB,
            uint32_t sampleRate
        );
    };

} // namespace Sync::Audio
