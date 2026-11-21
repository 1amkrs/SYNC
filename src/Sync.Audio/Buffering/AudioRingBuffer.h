#pragma once

#include <vector>
#include <atomic>
#include <cstdint>
#include <algorithm>
#include <cstring>

namespace Sync::Audio {

    // Single-Producer Single-Consumer (SPSC) lock-free ring buffer for audio frames (interleaved float samples).
    class AudioRingBuffer {
    public:
        AudioRingBuffer() : m_capacityFrames(0), m_channels(2), m_mask(0) {}

        explicit AudioRingBuffer(size_t capacityFrames, uint32_t channels = 2) {
            Initialize(capacityFrames, channels);
        }

        ~AudioRingBuffer() = default;

        // Initialize with power-of-two frame capacity for fast modulo masking
        void Initialize(size_t capacityFrames, uint32_t channels = 2) {
            m_channels = channels;
            // Round up to nearest power of 2
            size_t powerOfTwo = 1;
            while (powerOfTwo < capacityFrames) {
                powerOfTwo <<= 1;
            }
            m_capacityFrames = powerOfTwo;
            m_mask = m_capacityFrames - 1;
            m_buffer.assign(m_capacityFrames * m_channels, 0.0f);
            m_writeIndex.store(0, std::memory_order_relaxed);
            m_readIndex.store(0, std::memory_order_relaxed);
        }

        // Writes frames (interleaved float samples) into ring buffer. Returns actual frames written.
        size_t Write(const float* data, size_t frames) {
            if (!data || frames == 0 || m_capacityFrames == 0) return 0;

            const size_t writeIdx = m_writeIndex.load(std::memory_order_relaxed);
            const size_t readIdx = m_readIndex.load(std::memory_order_acquire);

            const size_t availableSpace = m_capacityFrames - (writeIdx - readIdx);
            const size_t framesToWrite = std::min(frames, availableSpace);

            if (framesToWrite == 0) return 0;

            const size_t startOffset = writeIdx & m_mask;
            const size_t firstChunk = std::min(framesToWrite, m_capacityFrames - startOffset);
            const size_t secondChunk = framesToWrite - firstChunk;

            std::memcpy(&m_buffer[startOffset * m_channels], data, firstChunk * m_channels * sizeof(float));

            if (secondChunk > 0) {
                std::memcpy(&m_buffer[0], data + (firstChunk * m_channels), secondChunk * m_channels * sizeof(float));
            }

            m_writeIndex.store(writeIdx + framesToWrite, std::memory_order_release);
            return framesToWrite;
        }

        // Reads frames (interleaved float samples) from ring buffer. Returns actual frames read.
        size_t Read(float* data, size_t frames) {
            if (!data || frames == 0 || m_capacityFrames == 0) return 0;

            const size_t readIdx = m_readIndex.load(std::memory_order_relaxed);
            const size_t writeIdx = m_writeIndex.load(std::memory_order_acquire);

            const size_t availableFrames = writeIdx - readIdx;
            const size_t framesToRead = std::min(frames, availableFrames);

            if (framesToRead == 0) return 0;

            const size_t startOffset = readIdx & m_mask;
            const size_t firstChunk = std::min(framesToRead, m_capacityFrames - startOffset);
            const size_t secondChunk = framesToRead - firstChunk;

            std::memcpy(data, &m_buffer[startOffset * m_channels], firstChunk * m_channels * sizeof(float));

            if (secondChunk > 0) {
                std::memcpy(data + (firstChunk * m_channels), &m_buffer[0], secondChunk * m_channels * sizeof(float));
            }

            m_readIndex.store(readIdx + framesToRead, std::memory_order_release);
            return framesToRead;
        }

        // Peek without advancing read pointer
        size_t Peek(float* data, size_t frames) const {
            if (!data || frames == 0 || m_capacityFrames == 0) return 0;

            const size_t readIdx = m_readIndex.load(std::memory_order_relaxed);
            const size_t writeIdx = m_writeIndex.load(std::memory_order_acquire);

            const size_t availableFrames = writeIdx - readIdx;
            const size_t framesToRead = std::min(frames, availableFrames);

            if (framesToRead == 0) return 0;

            const size_t startOffset = readIdx & m_mask;
            const size_t firstChunk = std::min(framesToRead, m_capacityFrames - startOffset);
            const size_t secondChunk = framesToRead - firstChunk;

            std::memcpy(data, &m_buffer[startOffset * m_channels], firstChunk * m_channels * sizeof(float));

            if (secondChunk > 0) {
                std::memcpy(data + (firstChunk * m_channels), &m_buffer[0], secondChunk * m_channels * sizeof(float));
            }

            return framesToRead;
        }

        // Discard frames from buffer
        size_t Discard(size_t frames) {
            const size_t readIdx = m_readIndex.load(std::memory_order_relaxed);
            const size_t writeIdx = m_writeIndex.load(std::memory_order_acquire);

            const size_t availableFrames = writeIdx - readIdx;
            const size_t framesToDiscard = std::min(frames, availableFrames);

            m_readIndex.store(readIdx + framesToDiscard, std::memory_order_release);
            return framesToDiscard;
        }

        // Returns count of readable frames
        size_t AvailableRead() const {
            const size_t readIdx = m_readIndex.load(std::memory_order_relaxed);
            const size_t writeIdx = m_writeIndex.load(std::memory_order_acquire);
            return (writeIdx >= readIdx) ? (writeIdx - readIdx) : 0;
        }

        // Returns count of writable frames
        size_t AvailableWrite() const {
            return m_capacityFrames - AvailableRead();
        }

        // Reset read and write pointers
        void Clear() {
            m_readIndex.store(0, std::memory_order_relaxed);
            m_writeIndex.store(0, std::memory_order_release);
        }

        size_t Capacity() const { return m_capacityFrames; }
        uint32_t Channels() const { return m_channels; }

    private:
        std::vector<float> m_buffer;
        size_t m_capacityFrames;
        uint32_t m_channels;
        size_t m_mask;

        // Cache-line aligned atomic indices to prevent false sharing
        alignas(64) std::atomic<size_t> m_writeIndex{ 0 };
        alignas(64) std::atomic<size_t> m_readIndex{ 0 };
    };

} // namespace Sync::Audio
