#include "WasapiLoopbackCapture.h"
#include <mmdeviceapi.h>
#include <cmath>
#include <algorithm>

namespace Sync::Audio {

    WasapiLoopbackCapture::WasapiLoopbackCapture() = default;

    WasapiLoopbackCapture::~WasapiLoopbackCapture() {
        Stop();
    }

    bool WasapiLoopbackCapture::Start(const std::wstring& deviceId, AudioCaptureCallback callback) {
        Stop();

        m_activeDeviceId = deviceId;
        m_callback = std::move(callback);
        m_stopRequested.store(false);

        if (!InitializeClient()) {
            CleanupClient();
            return false;
        }

        m_isRunning.store(true);
        m_captureThread = std::thread(&WasapiLoopbackCapture::CaptureThreadProc, this);
        return true;
    }

    void WasapiLoopbackCapture::Stop() {
        if (!m_isRunning.load()) return;

        m_stopRequested.store(true);
        if (m_captureEvent) {
            SetEvent(m_captureEvent);
        }

        if (m_captureThread.joinable()) {
            m_captureThread.join();
        }

        CleanupClient();
        m_isRunning.store(false);
    }

    bool WasapiLoopbackCapture::InitializeClient() {
        HRESULT hr = CoInitialize(nullptr);

        ComPtr<IMMDeviceEnumerator> enumerator;
        hr = CoCreateInstance(
            __uuidof(MMDeviceEnumerator),
            nullptr,
            CLSCTX_ALL,
            __uuidof(IMMDeviceEnumerator),
            reinterpret_cast<void**>(enumerator.GetAddressOf())
        );
        if (FAILED(hr)) return false;

        if (m_activeDeviceId.empty()) {
            hr = enumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, m_device.GetAddressOf());
        } else {
            hr = enumerator->GetDevice(m_activeDeviceId.c_str(), m_device.GetAddressOf());
        }
        if (FAILED(hr) || !m_device) return false;

        hr = m_device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, reinterpret_cast<void**>(m_audioClient.GetAddressOf()));
        if (FAILED(hr) || !m_audioClient) return false;

        WAVEFORMATEX* pMixFormat = nullptr;
        hr = m_audioClient->GetMixFormat(&pMixFormat);
        if (FAILED(hr) || !pMixFormat) return false;

        // Copy format details
        m_sampleRate = pMixFormat->nSamplesPerSec;
        m_channels = pMixFormat->nChannels;
        m_bitsPerSample = pMixFormat->wBitsPerSample;

        if (pMixFormat->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
            auto* pExt = reinterpret_cast<WAVEFORMATEXTENSIBLE*>(pMixFormat);
            m_isFloatFormat = (pExt->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT);
            std::memcpy(&m_waveFormatExt, pExt, sizeof(WAVEFORMATEXTENSIBLE));
        } else {
            m_isFloatFormat = (pMixFormat->wFormatTag == WAVE_FORMAT_IEEE_FLOAT);
            std::memcpy(&m_waveFormatExt, pMixFormat, sizeof(WAVEFORMATEX));
        }

        m_captureFormat.sampleRate = m_sampleRate;
        m_captureFormat.channels = 2; // We normalize capture output to stereo
        m_captureFormat.bitsPerSample = 32;
        m_captureFormat.isFloat = true;

        // Initialize WASAPI loopback client in event-driven mode
        // 100ms buffer (1,000,000 100ns units)
        REFERENCE_TIME hnsBufferDuration = 1000000;
        hr = m_audioClient->Initialize(
            AUDCLNT_SHAREMODE_SHARED,
            AUDCLNT_STREAMFLAGS_LOOPBACK | AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
            hnsBufferDuration,
            0,
            pMixFormat,
            nullptr
        );

        CoTaskMemFree(pMixFormat);
        if (FAILED(hr)) return false;

        m_captureEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        if (!m_captureEvent) return false;

        hr = m_audioClient->SetEventHandle(m_captureEvent);
        if (FAILED(hr)) return false;

        hr = m_audioClient->GetService(__uuidof(IAudioCaptureClient), reinterpret_cast<void**>(m_captureClient.GetAddressOf()));
        if (FAILED(hr) || !m_captureClient) return false;

        // Pre-allocate conversion buffer for up to 96kHz 200ms
        m_conversionBuffer.assign(96000 * 2 / 5, 0.0f);

        return true;
    }

    void WasapiLoopbackCapture::CleanupClient() {
        if (m_audioClient) {
            m_audioClient->Stop();
        }
        m_captureClient.Reset();
        m_audioClient.Reset();
        m_device.Reset();

        if (m_captureEvent) {
            CloseHandle(m_captureEvent);
            m_captureEvent = nullptr;
        }
    }

    void WasapiLoopbackCapture::CaptureThreadProc() {
        CoInitialize(nullptr);

        DWORD taskIndex = 0;
        HANDLE hTask = AvSetMmThreadCharacteristicsW(L"Pro Audio", &taskIndex);

        HRESULT hr = m_audioClient->Start();
        if (FAILED(hr)) {
            if (hTask) AvRevertMmThreadCharacteristics(hTask);
            CoUninitialize();
            return;
        }

        while (!m_stopRequested.load(std::memory_order_relaxed)) {
            DWORD waitResult = WaitForSingleObject(m_captureEvent, 100);
            if (m_stopRequested.load(std::memory_order_relaxed)) break;

            if (waitResult == WAIT_OBJECT_0) {
                UINT32 packetLength = 0;
                hr = m_captureClient->GetNextPacketSize(&packetLength);

                while (SUCCEEDED(hr) && packetLength > 0) {
                    BYTE* pData = nullptr;
                    UINT32 numFramesAvailable = 0;
                    DWORD flags = 0;
                    UINT64 devicePosition = 0;
                    UINT64 qpcPosition = 0;

                    hr = m_captureClient->GetBuffer(
                        &pData,
                        &numFramesAvailable,
                        &flags,
                        &devicePosition,
                        &qpcPosition
                    );

                    if (SUCCEEDED(hr) && numFramesAvailable > 0) {
                        const int64_t captureQpc = GetCurrentQpcTicks();

                        if (m_conversionBuffer.size() < numFramesAvailable * 2) {
                            m_conversionBuffer.resize(numFramesAvailable * 2);
                        }

                        if (flags & AUDCLNT_BUFFERFLAGS_SILENT) {
                            // Windows indicated silence: zero out buffer
                            std::fill(m_conversionBuffer.begin(), m_conversionBuffer.begin() + (numFramesAvailable * 2), 0.0f);
                            m_peakL.store(0.0f, std::memory_order_relaxed);
                            m_peakR.store(0.0f, std::memory_order_relaxed);
                        } else if (pData) {
                            ConvertToFloatStereo(pData, numFramesAvailable, m_conversionBuffer.data());
                        }

                        m_totalCapturedFrames.fetch_add(numFramesAvailable, std::memory_order_relaxed);

                        // Dispatch to consumer callback
                        if (m_callback) {
                            m_callback(
                                m_conversionBuffer.data(),
                                numFramesAvailable,
                                2, // Stereo output
                                m_sampleRate,
                                captureQpc
                            );
                        }

                        m_captureClient->ReleaseBuffer(numFramesAvailable);
                    }

                    hr = m_captureClient->GetNextPacketSize(&packetLength);
                }

                if (hr == AUDCLNT_E_DEVICE_INVALIDATED) {
                    // Endpoint was disconnected or changed format, attempt clean reinit
                    CleanupClient();
                    Sleep(200);
                    if (InitializeClient()) {
                        m_audioClient->Start();
                    }
                }
            }
        }

        if (hTask) {
            AvRevertMmThreadCharacteristics(hTask);
        }
        CoUninitialize();
    }

    void WasapiLoopbackCapture::ConvertToFloatStereo(const BYTE* pData, size_t numFrames, float* outStereo) {
        float peakL = 0.0f;
        float peakR = 0.0f;

        if (m_isFloatFormat && m_bitsPerSample == 32) {
            const auto* pFloat = reinterpret_cast<const float*>(pData);
            if (m_channels == 2) {
                // Direct stereo copy
                for (size_t i = 0; i < numFrames; ++i) {
                    const float l = pFloat[i * 2 + 0];
                    const float r = pFloat[i * 2 + 1];
                    outStereo[i * 2 + 0] = l;
                    outStereo[i * 2 + 1] = r;
                    peakL = std::max(peakL, std::abs(l));
                    peakR = std::max(peakR, std::abs(r));
                }
            } else if (m_channels == 1) {
                // Mono to stereo duplicate
                for (size_t i = 0; i < numFrames; ++i) {
                    const float m = pFloat[i];
                    outStereo[i * 2 + 0] = m;
                    outStereo[i * 2 + 1] = m;
                    peakL = std::max(peakL, std::abs(m));
                    peakR = std::max(peakR, std::abs(m));
                }
            } else {
                // Multi-channel downmix (take first 2 channels or average)
                for (size_t i = 0; i < numFrames; ++i) {
                    const float l = pFloat[i * m_channels + 0];
                    const float r = pFloat[i * m_channels + 1];
                    outStereo[i * 2 + 0] = l;
                    outStereo[i * 2 + 1] = r;
                    peakL = std::max(peakL, std::abs(l));
                    peakR = std::max(peakR, std::abs(r));
                }
            }
        } else if (!m_isFloatFormat && m_bitsPerSample == 16) {
            const auto* pInt16 = reinterpret_cast<const int16_t*>(pData);
            constexpr float inv32768 = 1.0f / 32768.0f;
            if (m_channels >= 2) {
                for (size_t i = 0; i < numFrames; ++i) {
                    const float l = static_cast<float>(pInt16[i * m_channels + 0]) * inv32768;
                    const float r = static_cast<float>(pInt16[i * m_channels + 1]) * inv32768;
                    outStereo[i * 2 + 0] = l;
                    outStereo[i * 2 + 1] = r;
                    peakL = std::max(peakL, std::abs(l));
                    peakR = std::max(peakR, std::abs(r));
                }
            } else {
                for (size_t i = 0; i < numFrames; ++i) {
                    const float m = static_cast<float>(pInt16[i]) * inv32768;
                    outStereo[i * 2 + 0] = m;
                    outStereo[i * 2 + 1] = m;
                    peakL = std::max(peakL, std::abs(m));
                    peakR = std::max(peakR, std::abs(m));
                }
            }
        } else if (!m_isFloatFormat && m_bitsPerSample == 24) {
            constexpr float inv8388608 = 1.0f / 8388608.0f;
            const size_t bytesPerFrame = m_channels * 3;
            for (size_t i = 0; i < numFrames; ++i) {
                const BYTE* frameBytes = pData + (i * bytesPerFrame);
                // Channel 0
                int32_t valL = (static_cast<int8_t>(frameBytes[2]) << 16) | (frameBytes[1] << 8) | frameBytes[0];
                float l = static_cast<float>(valL) * inv8388608;
                // Channel 1
                float r = l;
                if (m_channels >= 2) {
                    int32_t valR = (static_cast<int8_t>(frameBytes[5]) << 16) | (frameBytes[4] << 8) | frameBytes[3];
                    r = static_cast<float>(valR) * inv8388608;
                }
                outStereo[i * 2 + 0] = l;
                outStereo[i * 2 + 1] = r;
                peakL = std::max(peakL, std::abs(l));
                peakR = std::max(peakR, std::abs(r));
            }
        }

        // Smooth peak decay for responsive meters
        const float oldL = m_peakL.load(std::memory_order_relaxed);
        const float oldR = m_peakR.load(std::memory_order_relaxed);
        m_peakL.store(std::max(peakL, oldL * 0.85f), std::memory_order_relaxed);
        m_peakR.store(std::max(peakR, oldR * 0.85f), std::memory_order_relaxed);
    }

} // namespace Sync::Audio
