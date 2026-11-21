#include "AcousticCalibrator.h"
#include <cmath>
#include <algorithm>

namespace Sync::Audio {

    bool AcousticCalibrator::AnalyzeChirpArrival(
        const float* micSignal,
        size_t micFrames,
        const float* referenceChirp,
        size_t chirpFrames,
        size_t& outPeakIndex,
        float& outPeakScore
    ) {
        if (!micSignal || !referenceChirp || micFrames <= chirpFrames || chirpFrames == 0) {
            outPeakIndex = 0;
            outPeakScore = 0.0f;
            return false;
        }

        // Energy of reference chirp
        double chirpEnergy = 0.0;
        for (size_t i = 0; i < chirpFrames; ++i) {
            chirpEnergy += referenceChirp[i] * referenceChirp[i];
        }

        if (chirpEnergy < 1e-12) {
            return false;
        }

        const size_t searchLen = micFrames - chirpFrames;
        double maxCorrelation = 0.0;
        size_t bestIndex = 0;

        for (size_t n = 0; n < searchLen; ++n) {
            double dot = 0.0;
            double localEnergy = 0.0;
            for (size_t k = 0; k < chirpFrames; ++k) {
                const float s = micSignal[n + k];
                dot += s * referenceChirp[k];
                localEnergy += s * s;
            }

            if (localEnergy > 1e-12) {
                const double normalizedCorr = (dot * dot) / (chirpEnergy * localEnergy);
                if (normalizedCorr > maxCorrelation) {
                    maxCorrelation = normalizedCorr;
                    bestIndex = n;
                }
            }
        }

        outPeakIndex = bestIndex;
        outPeakScore = static_cast<float>(std::sqrt(maxCorrelation));
        return outPeakScore >= 0.15f; // Confidence threshold
    }

    CalibrationResult AcousticCalibrator::CalculateOffsets(
        size_t arrivalFramesA,
        float scoreA,
        size_t arrivalFramesB,
        float scoreB,
        uint32_t sampleRate
    ) {
        CalibrationResult res;
        res.correlationScoreA = scoreA;
        res.correlationScoreB = scoreB;

        if (scoreA < 0.20f || scoreB < 0.20f) {
            res.success = false;
            res.statusMessage = L"Correlation score too low; ensure microphone is near both speakers and background noise is low.";
            return res;
        }

        const double arrivalSecA = static_cast<double>(arrivalFramesA) / sampleRate;
        const double arrivalSecB = static_cast<double>(arrivalFramesB) / sampleRate;
        const double diffMs = (arrivalSecB - arrivalSecA) * 1000.0;

        res.measuredLatencyDiffMs = static_cast<float>(diffMs);

        // If diffMs > 0, speaker B arrived later than speaker A by diffMs.
        // Therefore, speaker A needs to be delayed by diffMs so they arrive at the same time!
        if (diffMs >= 0.0) {
            res.suggestedOffsetMsA = static_cast<int32_t>(std::round(diffMs));
            res.suggestedOffsetMsB = 0;
        } else {
            res.suggestedOffsetMsA = 0;
            res.suggestedOffsetMsB = static_cast<int32_t>(std::round(-diffMs));
        }

        res.success = true;
        res.statusMessage = L"Acoustic calibration succeeded.";
        return res;
    }

} // namespace Sync::Audio
