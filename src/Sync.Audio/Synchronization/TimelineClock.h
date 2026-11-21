#pragma once

#include "Common.h"

namespace Sync::Audio {

    // Monotonic master timeline clock based on Windows QPC
    class TimelineClock {
    public:
        TimelineClock() {
            Reset();
        }

        void Reset() {
            m_startTicks = GetCurrentQpcTicks();
            m_baseFrameIndex = 0;
        }

        // Returns current master timeline position in 100ns units since start
        int64_t GetTimelineTicks() const {
            return GetCurrentQpcTicks() - m_startTicks;
        }

        // Convert sample count at given sample rate to 100ns ticks
        static int64_t FramesToTicks(uint64_t frames, uint32_t sampleRate) {
            if (sampleRate == 0) return 0;
            return static_cast<int64_t>((frames * 10000000ULL) / sampleRate);
        }

        // Convert 100ns ticks to sample frames at given sample rate
        static uint64_t TicksToFrames(int64_t ticks, uint32_t sampleRate) {
            if (ticks <= 0 || sampleRate == 0) return 0;
            return static_cast<uint64_t>((ticks * sampleRate) / 10000000ULL);
        }

        // Convert milliseconds to 100ns ticks
        static int64_t MillisToTicks(int32_t ms) {
            return static_cast<int64_t>(ms) * 10000LL;
        }

        // Convert 100ns ticks to milliseconds
        static double TicksToMillis(int64_t ticks) {
            return static_cast<double>(ticks) / 10000.0;
        }

    private:
        int64_t m_startTicks = 0;
        uint64_t m_baseFrameIndex = 0;
    };

} // namespace Sync::Audio
