#include "WasapiRenderEndpoint.h"
#include <cmath>
#include <algorithm>

namespace Sync::Audio {

    WasapiRenderEndpoint::WasapiRenderEndpoint(const DeviceInfo& info)
        : m_info(info)
    {
    }

    WasapiRenderEndpoint::~WasapiRenderEndpoint() {
        Shutdown();
    }

    bool WasapiRenderEndpoint::Initialize() {
        Shutdown();

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

        hr = enumerator->GetDevice(m_info.id.c_str(), m_device.GetAddressOf());
        if (FAILED(hr) || !m_device) return false;

        hr = m_device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, reinterpret_cast<void**>(m_audioClient.GetAddressOf()));
        if (FAILED(hr) || !m_audioClient) return false;

        WAVEFORMATEX* pMixFormat = nullptr;
        hr = m_audioClient->GetMixFormat(&pMixFormat);
        if (FAILED(hr) || !pMixFormat) return false;

        m_sampleRate = pMixFormat->nSamplesPerSec;
        m_channels = pMixFormat->nChannels;
        m_bitsPerSample = pMixFormat->wBitsPerSample;

        if (pMixFormat->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
            auto* pExt = reinterpret_cast<WAVEFORMATEXTENSIBLE*>(pMixFormat);
            m_isFloatFormat = (pExt->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT);
            std::memcpy(&m_mixFormatExt, pExt, sizeof(WAVEFORMATEXTENSIBLE));
        } else {
            m_isFloatFormat = (pMixFormat->wFormatTag == WAVE_FORMAT_IEEE_FLOAT);
            std::memcpy(&m_mixFormatExt, pMixFormat, sizeof(WAVEFORMATEX));
        }

        REFERENCE_TIME defaultPeriod = 0;
        REFERENCE_TIME minPeriod = 0;
        if (SUCCEEDED(m_audioClient->GetDevicePeriod(&defaultPeriod, &minPeriod))) {
            m_wasapiPeriodMs = static_cast<float>(defaultPeriod) / 10000.0f;
        }

        // Initialize with ~100ms buffer in WASAPI
        REFERENCE_TIME hnsBufferDuration = 1000000;
        hr = m_audioClient->Initialize(
            AUDCLNT_SHAREMODE_SHARED,
            AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
            hnsBufferDuration,
            0,
            pMixFormat,
            nullptr
        );

        CoTaskMemFree(pMixFormat);
        if (FAILED(hr)) return false;

        hr = m_audioClient->GetBufferSize(&m_endpointBufferFrames);
        if (FAILED(hr) || m_endpointBufferFrames == 0) return false;

        m_renderEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        if (!m_renderEvent) return false;

        hr = m_audioClient->SetEventHandle(m_renderEvent);
        if (FAILED(hr)) return false;

        hr = m_audioClient->GetService(__uuidof(IAudioRenderClient), reinterpret_cast<void**>(m_renderClient.GetAddressOf()));
        if (FAILED(hr) || !m_renderClient) return false;

        // Initialize ring buffer for 2 seconds capacity
        const size_t ringCapacity = m_sampleRate * 2;
        m_ringBuffer.Initialize(ringCapacity, 2); // 2 channels stereo

        // Initialize Resampler (assumes 48kHz input from loopback, converts to device m_sampleRate)
        m_resampler.Initialize(48000, m_sampleRate, 2);

        // Initialize DriftEstimator (target buffer frames corresponds to ~100ms default)
        const size_t targetFrames = (m_sampleRate * 100) / 1000;
        m_targetBufferFrames.store(targetFrames);
        m_driftEstimator.Reset(targetFrames, m_sampleRate);

        m_resampleInputBuffer.assign(4096 * 2, 0.0f);
        m_resampleOutputBuffer.assign(4096 * 2, 0.0f);

        m_isDeviceValid.store(true);
        return true;
    }

    void WasapiRenderEndpoint::Shutdown() {
        Stop();

        m_renderClient.Reset();
        m_audioClient.Reset();
        m_device.Reset();

        if (m_renderEvent) {
            CloseHandle(m_renderEvent);
            m_renderEvent = nullptr;
        }
    }

    bool WasapiRenderEndpoint::Start() {
        if (m_isRunning.load()) return true;
        if (!m_audioClient && !Initialize()) return false;

        m_stopRequested.store(false);

        // Pre-roll with initial silence so WASAPI starts cleanly
        BYTE* pBuffer = nullptr;
        if (SUCCEEDED(m_renderClient->GetBuffer(m_endpointBufferFrames, &pBuffer))) {
            std::memset(pBuffer, 0, m_endpointBufferFrames * m_mixFormatExt.Format.nBlockAlign);
            m_renderClient->ReleaseBuffer(m_endpointBufferFrames, 0);
        }

        HRESULT hr = m_audioClient->Start();
        if (FAILED(hr)) return false;

        m_isRunning.store(true);
        m_renderThread = std::thread(&WasapiRenderEndpoint::RenderThreadProc, this);
        return true;
    }

    void WasapiRenderEndpoint::Stop() {
        if (!m_isRunning.load()) return;

        m_stopRequested.store(true);
        if (m_renderEvent) {
            SetEvent(m_renderEvent);
        }

        if (m_renderThread.joinable()) {
            m_renderThread.join();
        }

        if (m_audioClient) {
            m_audioClient->Stop();
        }

        m_isRunning.store(false);
    }

    void WasapiRenderEndpoint::SetTargetBufferFrames(size_t targetFrames) {
        m_targetBufferFrames.store(targetFrames);
        m_driftEstimator.Reset(targetFrames, m_sampleRate);
    }

    size_t WasapiRenderEndpoint::PushAudio(const float* stereoFloatData, size_t frames, uint32_t sampleRate) {
        if (!stereoFloatData || frames == 0) return 0;

        // If sample rate of input doesn't match resampler input, re-init resampler input
        if (m_resampler.InSampleRate() != sampleRate) {
            m_resampler.Initialize(sampleRate, m_sampleRate, 2);
        }

        return m_ringBuffer.Write(stereoFloatData, frames);
    }

    void WasapiRenderEndpoint::RenderThreadProc() {
        CoInitialize(nullptr);

        DWORD taskIndex = 0;
        HANDLE hTask = AvSetMmThreadCharacteristicsW(L"Pro Audio", &taskIndex);

        while (!m_stopRequested.load(std::memory_order_relaxed)) {
            DWORD waitResult = WaitForSingleObject(m_renderEvent, 100);
            if (m_stopRequested.load(std::memory_order_relaxed)) break;

            if (waitResult == WAIT_OBJECT_0) {
                UINT32 paddingFrames = 0;
                HRESULT hr = m_audioClient->GetCurrentPadding(&paddingFrames);

                if (SUCCEEDED(hr)) {
                    const UINT32 framesNeeded = m_endpointBufferFrames - paddingFrames;
                    if (framesNeeded > 0) {
                        BYTE* pRenderData = nullptr;
                        hr = m_renderClient->GetBuffer(framesNeeded, &pRenderData);
                        if (SUCCEEDED(hr) && pRenderData) {
                            FillRenderBuffer(pRenderData, framesNeeded);
                            m_renderClient->ReleaseBuffer(framesNeeded, 0);
                        }
                    }
                } else if (hr == AUDCLNT_E_DEVICE_INVALIDATED) {
                    m_isDeviceValid.store(false);
                    // Device disconnected or format changed
                    break;
                }
            }
        }

        if (hTask) {
            AvRevertMmThreadCharacteristics(hTask);
        }
        CoUninitialize();
    }

    void WasapiRenderEndpoint::FillRenderBuffer(BYTE* pRenderData, size_t framesToFill) {
        const bool enabled = m_isEnabled.load(std::memory_order_relaxed);
        const float volume = m_volume.load(std::memory_order_relaxed);

        const size_t availableFrames = m_ringBuffer.AvailableRead();

        // Update buffer occupancy telemetry
        const size_t capacity = m_ringBuffer.Capacity();
        if (capacity > 0) {
            const uint32_t percent = static_cast<uint32_t>((availableFrames * 100) / capacity);
            m_bufferOccupancyPercent.store(percent, std::memory_order_relaxed);
        }

        // Update clock drift estimation and apply to resampler
        const int32_t driftPpm = m_driftEstimator.Update(availableFrames, m_sampleRate);
        m_estimatedDriftPpm.store(driftPpm, std::memory_order_relaxed);
        m_resampler.SetDriftPpm(driftPpm);

        // Check if we need resampling from capture sample rate (e.g. 48kHz -> m_sampleRate)
        const double resampleRatio = m_resampler.CurrentRatio();
        const size_t inputFramesNeeded = static_cast<size_t>(std::ceil(framesToFill * resampleRatio)) + 4;

        if (m_resampleInputBuffer.size() < inputFramesNeeded * 2) {
            m_resampleInputBuffer.resize(inputFramesNeeded * 2);
        }
        if (m_resampleOutputBuffer.size() < framesToFill * 2) {
            m_resampleOutputBuffer.resize(framesToFill * 2);
        }

        size_t framesRead = 0;
        if (availableFrames >= inputFramesNeeded) {
            framesRead = m_ringBuffer.Read(m_resampleInputBuffer.data(), inputFramesNeeded);
        } else {
            // Underrun condition
            m_underrunCount.fetch_add(1, std::memory_order_relaxed);
            m_driftEstimator.RecordUnderrun();

            // Read whatever is available
            framesRead = m_ringBuffer.Read(m_resampleInputBuffer.data(), availableFrames);
            // Fill remaining with silence
            std::fill(m_resampleInputBuffer.begin() + (framesRead * 2), m_resampleInputBuffer.begin() + (inputFramesNeeded * 2), 0.0f);
        }

        // Resample input samples to endpoint sample rate
        const size_t framesProduced = m_resampler.Process(
            m_resampleInputBuffer.data(),
            inputFramesNeeded,
            m_resampleOutputBuffer.data(),
            framesToFill
        );

        // Fill any gap if resampler produced slightly fewer frames
        if (framesProduced < framesToFill) {
            std::fill(m_resampleOutputBuffer.begin() + (framesProduced * 2), m_resampleOutputBuffer.begin() + (framesToFill * 2), 0.0f);
        }

        // Apply volume and mute
        const float effectiveGain = enabled ? volume : 0.0f;

        // Convert stereo float output into endpoint native format
        if (m_isFloatFormat && m_bitsPerSample == 32) {
            auto* pOutFloat = reinterpret_cast<float*>(pRenderData);
            if (m_channels == 2) {
                for (size_t i = 0; i < framesToFill; ++i) {
                    pOutFloat[i * 2 + 0] = m_resampleOutputBuffer[i * 2 + 0] * effectiveGain;
                    pOutFloat[i * 2 + 1] = m_resampleOutputBuffer[i * 2 + 1] * effectiveGain;
                }
            } else {
                for (size_t i = 0; i < framesToFill; ++i) {
                    const float l = m_resampleOutputBuffer[i * 2 + 0] * effectiveGain;
                    const float r = m_resampleOutputBuffer[i * 2 + 1] * effectiveGain;
                    pOutFloat[i * m_channels + 0] = l;
                    if (m_channels > 1) pOutFloat[i * m_channels + 1] = r;
                    for (uint16_t c = 2; c < m_channels; ++c) {
                        pOutFloat[i * m_channels + c] = 0.0f;
                    }
                }
            }
        } else if (!m_isFloatFormat && m_bitsPerSample == 16) {
            auto* pOutInt16 = reinterpret_cast<int16_t*>(pRenderData);
            for (size_t i = 0; i < framesToFill; ++i) {
                const float l = std::clamp(m_resampleOutputBuffer[i * 2 + 0] * effectiveGain, -1.0f, 1.0f);
                const float r = std::clamp(m_resampleOutputBuffer[i * 2 + 1] * effectiveGain, -1.0f, 1.0f);
                const int16_t sampleL = static_cast<int16_t>(l * 32767.0f);
                const int16_t sampleR = static_cast<int16_t>(r * 32767.0f);

                pOutInt16[i * m_channels + 0] = sampleL;
                if (m_channels > 1) pOutInt16[i * m_channels + 1] = sampleR;
                for (uint16_t c = 2; c < m_channels; ++c) {
                    pOutInt16[i * m_channels + c] = 0;
                }
            }
        } else if (!m_isFloatFormat && m_bitsPerSample == 24) {
            const size_t bytesPerFrame = m_channels * 3;
            for (size_t i = 0; i < framesToFill; ++i) {
                const float l = std::clamp(m_resampleOutputBuffer[i * 2 + 0] * effectiveGain, -1.0f, 1.0f);
                const float r = std::clamp(m_resampleOutputBuffer[i * 2 + 1] * effectiveGain, -1.0f, 1.0f);
                const int32_t valL = static_cast<int32_t>(l * 8388607.0f);
                const int32_t valR = static_cast<int32_t>(r * 8388607.0f);

                BYTE* pDst = pRenderData + (i * bytesPerFrame);
                pDst[0] = static_cast<BYTE>(valL & 0xFF);
                pDst[1] = static_cast<BYTE>((valL >> 8) & 0xFF);
                pDst[2] = static_cast<BYTE>((valL >> 16) & 0xFF);

                if (m_channels > 1) {
                    pDst[3] = static_cast<BYTE>(valR & 0xFF);
                    pDst[4] = static_cast<BYTE>((valR >> 8) & 0xFF);
                    pDst[5] = static_cast<BYTE>((valR >> 16) & 0xFF);
                }
                for (uint16_t c = 2; c < m_channels; ++c) {
                    pDst[c * 3 + 0] = 0;
                    pDst[c * 3 + 1] = 0;
                    pDst[c * 3 + 2] = 0;
                }
            }
        }
    }

    DeviceTelemetry WasapiRenderEndpoint::GetTelemetry() const {
        DeviceTelemetry t{};
        wcsncpy_s(t.deviceId, m_info.id.c_str(), _TRUNCATE);
        t.bufferOccupancyPercent = m_bufferOccupancyPercent.load(std::memory_order_relaxed);
        t.underrunCount = m_underrunCount.load(std::memory_order_relaxed);
        t.estimatedDriftPpm = m_estimatedDriftPpm.load(std::memory_order_relaxed);
        t.calibratedDelayMs = m_delayOffsetMs.load(std::memory_order_relaxed);
        t.measuredLatencyMs = m_wasapiPeriodMs + static_cast<float>(m_delayOffsetMs.load(std::memory_order_relaxed));
        t.volume = m_volume.load(std::memory_order_relaxed);
        t.isEnabled = m_isEnabled.load(std::memory_order_relaxed);
        t.isConnected = m_isDeviceValid.load(std::memory_order_relaxed);
        return t;
    }

} // namespace Sync::Audio
