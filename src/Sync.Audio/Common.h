#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <endpointvolume.h>
#include <functiondiscoverykeys_devpkey.h>
#include <avrt.h>
#include <wrl/client.h>

#include <cstdint>
#include <cstddef>
#include <cmath>
#include <string>
#include <vector>
#include <memory>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <functional>
#include <sstream>
#include <iostream>

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "avrt.lib")
#pragma comment(lib, "winmm.lib")

namespace Sync::Audio {

    using Microsoft::WRL::ComPtr;

    // High resolution monotonic timestamp in 100-nanosecond units (Windows FILETIME-style ticks)
    inline int64_t GetCurrentQpcTicks() {
        static const int64_t qpcFrequency = []() {
            LARGE_INTEGER freq;
            QueryPerformanceFrequency(&freq);
            return freq.QuadPart;
        }();

        LARGE_INTEGER counter;
        QueryPerformanceCounter(&counter);
        // Scale to 100ns units (10,000,000 units per second)
        return (counter.QuadPart * 10000000LL) / qpcFrequency;
    }

    inline double GetCurrentTimeSeconds() {
        return static_cast<double>(GetCurrentQpcTicks()) / 10000000.0;
    }

    struct AudioFormat {
        uint32_t sampleRate = 48000;
        uint16_t channels = 2;
        uint16_t bitsPerSample = 32;
        bool isFloat = true;

        bool operator==(const AudioFormat& other) const {
            return sampleRate == other.sampleRate &&
                   channels == other.channels &&
                   bitsPerSample == other.bitsPerSample &&
                   isFloat == other.isFloat;
        }

        bool operator!=(const AudioFormat& other) const {
            return !(*this == other);
        }

        uint32_t FrameSize() const {
            return channels * (bitsPerSample / 8);
        }
    };

    enum class DeviceTransportType {
        Unknown = 0,
        InternalSpeakers = 1,
        Headphones = 2,
        Bluetooth = 3,
        Usb = 4,
        Hdmi = 5
    };

    struct DeviceInfo {
        std::wstring id;
        std::wstring name;
        std::wstring adapterName;
        DeviceTransportType transportType = DeviceTransportType::Unknown;
        bool isDefault = false;
        bool isBluetooth = false;
        AudioFormat nativeFormat;
    };

    struct DeviceTelemetry {
        wchar_t deviceId[128];
        uint32_t bufferOccupancyPercent;
        uint32_t underrunCount;
        int32_t estimatedDriftPpm;
        int32_t calibratedDelayMs;
        float measuredLatencyMs;
        float volume;
        bool isEnabled;
        bool isConnected;
    };

    struct MasterTelemetry {
        uint32_t activeOutputs;
        uint32_t captureSampleRate;
        uint32_t captureChannels;
        float audioLevelPeakL;
        float audioLevelPeakR;
        uint64_t totalCapturedFrames;
        uint64_t totalRenderedFrames;
        bool isRunning;
    };

} // namespace Sync::Audio
