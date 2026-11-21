#pragma once

#include "Common.h"
#include <deque>
#include <mutex>
#include <vector>

namespace Sync::Audio {

    struct AudioChunk {
        int64_t timestampQpc = 0; // 100ns units
        std::vector<float> data;  // Interleaved float samples
        size_t frames = 0;
        uint32_t channels = 2;
        uint32_t sampleRate = 48000;
    };

    // Thread-safe FIFO of timestamped audio chunks for master timeline alignment
    class TimestampedAudioBuffer {
    public:
        TimestampedAudioBuffer() : m_maxCapacityFrames(48000 * 5) {} // default 5 seconds capacity

        void SetMaxCapacity(size_t maxFrames) {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_maxCapacityFrames = maxFrames;
        }

        void Push(const float* data, size_t frames, uint32_t channels, uint32_t sampleRate, int64_t timestampQpc) {
            if (!data || frames == 0) return;

            AudioChunk chunk;
            chunk.timestampQpc = timestampQpc;
            chunk.frames = frames;
            chunk.channels = channels;
            chunk.sampleRate = sampleRate;
            chunk.data.assign(data, data + (frames * channels));

            std::lock_guard<std::mutex> lock(m_mutex);
            m_chunks.push_back(std::move(chunk));
            m_totalFrames += frames;

            // Trim if exceeding capacity
            while (m_totalFrames > m_maxCapacityFrames && !m_chunks.empty()) {
                m_totalFrames -= m_chunks.front().frames;
                m_chunks.pop_front();
            }
        }

        // Pops the oldest chunk
        bool Pop(AudioChunk& outChunk) {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_chunks.empty()) return false;

            outChunk = std::move(m_chunks.front());
            m_chunks.pop_front();
            m_totalFrames -= outChunk.frames;
            return true;
        }

        // Peeks at the timestamp of the oldest chunk
        bool PeekTimestamp(int64_t& outTimestampQpc) {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_chunks.empty()) return false;
            outTimestampQpc = m_chunks.front().timestampQpc;
            return true;
        }

        size_t TotalFrames() const {
            std::lock_guard<std::mutex> lock(m_mutex);
            return m_totalFrames;
        }

        bool IsEmpty() const {
            std::lock_guard<std::mutex> lock(m_mutex);
            return m_chunks.empty();
        }

        void Clear() {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_chunks.clear();
            m_totalFrames = 0;
        }

    private:
        mutable std::mutex m_mutex;
        std::deque<AudioChunk> m_chunks;
        size_t m_totalFrames = 0;
        size_t m_maxCapacityFrames = 48000 * 5;
    };

} // namespace Sync::Audio
