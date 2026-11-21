#include "SyncEngine.h"

namespace Sync::Audio {

    SyncEngine::SyncEngine() {
        m_signalGenerator.Initialize(48000, 2);
    }

    SyncEngine::~SyncEngine() {
        Shutdown();
    }

    bool SyncEngine::Initialize() {
        std::lock_guard<std::mutex> lock(m_engineMutex);
        if (!m_deviceEnumerator.Initialize()) {
            return false;
        }

        m_deviceEnumerator.SetChangeCallback([this]() {
            OnDeviceHardwareChanged();
        });

        // Initialize capture with default device initially
        DeviceInfo defaultDev;
        if (m_deviceEnumerator.GetDefaultEndpoint(defaultDev)) {
            m_captureDeviceId = defaultDev.id;
        }

        return true;
    }

    void SyncEngine::Shutdown() {
        Stop();
        std::lock_guard<std::mutex> lock(m_engineMutex);
        m_renderOutputs.clear();
        m_latencyManager.Clear();
        m_deviceEnumerator.Shutdown();
    }

    std::vector<DeviceInfo> SyncEngine::EnumerateDevices() {
        return m_deviceEnumerator.EnumerateEndpoints();
    }

    bool SyncEngine::SetCaptureDevice(const std::wstring& deviceId) {
        std::lock_guard<std::mutex> lock(m_engineMutex);
        // Feedback loop prevention: Ensure device is not currently a render output
        for (const auto& out : m_renderOutputs) {
            if (out->GetId() == deviceId) {
                if (m_eventCallback) {
                    m_eventCallback(L"Error", L"Cannot set capture source to an active SYNC output device (feedback prevention).");
                }
                return false;
            }
        }

        m_captureDeviceId = deviceId;

        // If currently running, restart capture on new device
        if (m_isRunning.load()) {
            m_capture.Stop();
            return m_capture.Start(m_captureDeviceId, [this](const float* buffer, size_t frames, uint32_t channels, uint32_t sampleRate, int64_t timestampQpc) {
                OnAudioCaptured(buffer, frames, channels, sampleRate, timestampQpc);
            });
        }
        return true;
    }

    std::wstring SyncEngine::GetCaptureDevice() const {
        std::lock_guard<std::mutex> lock(m_engineMutex);
        return m_captureDeviceId;
    }

    bool SyncEngine::AddOutputDevice(const std::wstring& deviceId) {
        std::lock_guard<std::mutex> lock(m_engineMutex);

        // Feedback loop prevention: Ensure device is not current capture device
        if (!m_captureDeviceId.empty() && deviceId == m_captureDeviceId) {
            if (m_eventCallback) {
                m_eventCallback(L"Error", L"Cannot add capture source as an output device (feedback prevention).");
            }
            return false;
        }

        // Check if already added
        for (const auto& out : m_renderOutputs) {
            if (out->GetId() == deviceId) {
                return true;
            }
        }

        DeviceInfo info;
        if (!m_deviceEnumerator.GetEndpointById(deviceId, info)) {
            return false;
        }

        auto endpoint = std::make_unique<WasapiRenderEndpoint>(info);
        if (!endpoint->Initialize()) {
            return false;
        }

        // Apply any existing latency profile
        m_latencyManager.SetDeviceOffsetMs(deviceId, endpoint->GetDelayOffsetMs());
        m_latencyManager.SetDeviceWasapiLatencyMs(deviceId, 10.0f);
        m_latencyManager.Recalculate(info.nativeFormat.sampleRate);
        endpoint->SetTargetBufferFrames(m_latencyManager.GetTargetBufferFrames(deviceId));

        // If engine is already running, start this endpoint immediately
        if (m_isRunning.load()) {
            endpoint->Start();
        }

        m_renderOutputs.push_back(std::move(endpoint));
        return true;
    }

    bool SyncEngine::RemoveOutputDevice(const std::wstring& deviceId) {
        std::lock_guard<std::mutex> lock(m_engineMutex);
        auto it = std::find_if(m_renderOutputs.begin(), m_renderOutputs.end(), [&](const std::unique_ptr<WasapiRenderEndpoint>& ep) {
            return ep->GetId() == deviceId;
        });

        if (it != m_renderOutputs.end()) {
            (*it)->Shutdown();
            m_renderOutputs.erase(it);
            m_latencyManager.RemoveDevice(deviceId);
            m_latencyManager.Recalculate(48000);
            return true;
        }
        return false;
    }

    bool SyncEngine::IsOutputDeviceSelected(const std::wstring& deviceId) const {
        std::lock_guard<std::mutex> lock(m_engineMutex);
        for (const auto& out : m_renderOutputs) {
            if (out->GetId() == deviceId) return true;
        }
        return false;
    }

    void SyncEngine::ClearOutputDevices() {
        std::lock_guard<std::mutex> lock(m_engineMutex);
        for (auto& out : m_renderOutputs) {
            out->Shutdown();
        }
        m_renderOutputs.clear();
        m_latencyManager.Clear();
    }

    void SyncEngine::SetOutputVolume(const std::wstring& deviceId, float volume) {
        std::lock_guard<std::mutex> lock(m_engineMutex);
        for (auto& out : m_renderOutputs) {
            if (out->GetId() == deviceId) {
                out->SetVolume(volume);
                break;
            }
        }
    }

    float SyncEngine::GetOutputVolume(const std::wstring& deviceId) {
        std::lock_guard<std::mutex> lock(m_engineMutex);
        for (auto& out : m_renderOutputs) {
            if (out->GetId() == deviceId) {
                return out->GetVolume();
            }
        }
        return 1.0f;
    }

    void SyncEngine::SetOutputDelayMs(const std::wstring& deviceId, int32_t delayMs) {
        std::lock_guard<std::mutex> lock(m_engineMutex);
        for (auto& out : m_renderOutputs) {
            if (out->GetId() == deviceId) {
                out->SetDelayOffsetMs(delayMs);
                m_latencyManager.SetDeviceOffsetMs(deviceId, delayMs);
                break;
            }
        }
        m_latencyManager.Recalculate(48000);

        // Update target buffer frames on all outputs
        for (auto& out : m_renderOutputs) {
            out->SetTargetBufferFrames(m_latencyManager.GetTargetBufferFrames(out->GetId()));
        }
    }

    int32_t SyncEngine::GetOutputDelayMs(const std::wstring& deviceId) {
        std::lock_guard<std::mutex> lock(m_engineMutex);
        for (auto& out : m_renderOutputs) {
            if (out->GetId() == deviceId) {
                return out->GetDelayOffsetMs();
            }
        }
        return 0;
    }

    void SyncEngine::SetOutputEnabled(const std::wstring& deviceId, bool enabled) {
        std::lock_guard<std::mutex> lock(m_engineMutex);
        for (auto& out : m_renderOutputs) {
            if (out->GetId() == deviceId) {
                out->SetEnabled(enabled);
                break;
            }
        }
    }

    bool SyncEngine::IsOutputEnabled(const std::wstring& deviceId) {
        std::lock_guard<std::mutex> lock(m_engineMutex);
        for (auto& out : m_renderOutputs) {
            if (out->GetId() == deviceId) {
                return out->IsEnabled();
            }
        }
        return false;
    }

    bool SyncEngine::Start() {
        std::lock_guard<std::mutex> lock(m_engineMutex);
        if (m_isRunning.load()) return true;

        if (m_renderOutputs.empty()) {
            if (m_eventCallback) {
                m_eventCallback(L"Error", L"No output devices selected. Please select at least one output device.");
            }
            return false;
        }

        m_timelineClock.Reset();
        m_latencyManager.Recalculate(48000);

        // Start all render endpoints
        for (auto& out : m_renderOutputs) {
            out->SetTargetBufferFrames(m_latencyManager.GetTargetBufferFrames(out->GetId()));
            if (!out->Start()) {
                if (m_eventCallback) {
                    m_eventCallback(L"Warning", (L"Failed to start endpoint: " + out->GetName()).c_str());
                }
            }
        }

        // Start loopback capture
        bool captureOk = m_capture.Start(m_captureDeviceId, [this](const float* buffer, size_t frames, uint32_t channels, uint32_t sampleRate, int64_t timestampQpc) {
            OnAudioCaptured(buffer, frames, channels, sampleRate, timestampQpc);
        });

        if (!captureOk) {
            if (m_eventCallback) {
                m_eventCallback(L"Error", L"Failed to start WASAPI loopback capture on selected device.");
            }
            for (auto& out : m_renderOutputs) {
                out->Stop();
            }
            return false;
        }

        m_isRunning.store(true);
        if (m_eventCallback) {
            m_eventCallback(L"PlaybackStarted", L"Audio synchronization active.");
        }
        return true;
    }

    void SyncEngine::Stop() {
        std::lock_guard<std::mutex> lock(m_engineMutex);
        if (!m_isRunning.load()) return;

        m_capture.Stop();

        for (auto& out : m_renderOutputs) {
            out->Stop();
        }

        m_isRunning.store(false);
        if (m_eventCallback) {
            m_eventCallback(L"PlaybackStopped", L"Audio synchronization stopped.");
        }
    }

    bool SyncEngine::IsRunning() const {
        return m_isRunning.load(std::memory_order_relaxed);
    }

    void SyncEngine::SetMasterVolume(float vol) {
        m_masterVolume.store(std::clamp(vol, 0.0f, 1.0f), std::memory_order_relaxed);
    }

    void SyncEngine::SetMuteAll(bool mute) {
        m_isMutedAll.store(mute, std::memory_order_relaxed);
    }

    void SyncEngine::PlayTestTone(TestSignalType type) {
        std::lock_guard<std::mutex> lock(m_engineMutex);
        m_signalGenerator.SetSignalType(type);
    }

    void SyncEngine::StopTestTone() {
        std::lock_guard<std::mutex> lock(m_engineMutex);
        m_signalGenerator.SetSignalType(TestSignalType::None);
    }

    TestSignalType SyncEngine::GetActiveTestTone() const {
        return m_signalGenerator.GetSignalType();
    }

    void SyncEngine::SetBaseBufferingDelayMs(int32_t delayMs) {
        m_latencyManager.SetBaseBufferingDelayMs(delayMs);
        std::lock_guard<std::mutex> lock(m_engineMutex);
        m_latencyManager.Recalculate(48000);
        for (auto& out : m_renderOutputs) {
            out->SetTargetBufferFrames(m_latencyManager.GetTargetBufferFrames(out->GetId()));
        }
    }

    int32_t SyncEngine::GetBaseBufferingDelayMs() const {
        return m_latencyManager.GetBaseBufferingDelayMs();
    }

    void SyncEngine::OnAudioCaptured(const float* buffer, size_t frames, uint32_t channels, uint32_t sampleRate, int64_t /*timestampQpc*/) {
        if (!buffer || frames == 0) return;

        // If test tone is active, generate tone instead of passing captured audio
        const bool testToneActive = (m_signalGenerator.GetSignalType() != TestSignalType::None);

        if (m_distBuffer.size() < frames * 2) {
            m_distBuffer.resize(frames * 2);
        }

        if (testToneActive) {
            m_signalGenerator.Generate(m_distBuffer.data(), frames);
        } else {
            const float masterGain = m_isMutedAll.load(std::memory_order_relaxed) ? 0.0f : m_masterVolume.load(std::memory_order_relaxed);
            for (size_t i = 0; i < frames * 2; ++i) {
                m_distBuffer[i] = buffer[i] * masterGain;
            }
        }

        // Distribute to all active render outputs
        std::lock_guard<std::mutex> lock(m_engineMutex);
        for (auto& out : m_renderOutputs) {
            if (out->IsRunning() && out->IsEnabled()) {
                out->PushAudio(m_distBuffer.data(), frames, sampleRate);
            }
        }

        m_totalRenderedFrames.fetch_add(frames, std::memory_order_relaxed);
    }

    void SyncEngine::OnDeviceHardwareChanged() {
        if (m_eventCallback) {
            m_eventCallback(L"DeviceListChanged", L"Audio hardware endpoints changed.");
        }
    }

    MasterTelemetry SyncEngine::GetMasterTelemetry() const {
        MasterTelemetry t{};
        std::lock_guard<std::mutex> lock(m_engineMutex);

        uint32_t activeCount = 0;
        for (const auto& out : m_renderOutputs) {
            if (out->IsRunning() && out->IsEnabled()) {
                activeCount++;
            }
        }
        t.activeOutputs = activeCount;

        const auto fmt = m_capture.GetCaptureFormat();
        t.captureSampleRate = fmt.sampleRate;
        t.captureChannels = fmt.channels;
        t.audioLevelPeakL = m_capture.GetPeakLevelL();
        t.audioLevelPeakR = m_capture.GetPeakLevelR();
        t.totalCapturedFrames = m_capture.GetTotalCapturedFrames();
        t.totalRenderedFrames = m_totalRenderedFrames.load(std::memory_order_relaxed);
        t.isRunning = m_isRunning.load(std::memory_order_relaxed);

        return t;
    }

    bool SyncEngine::GetDeviceTelemetry(const std::wstring& deviceId, DeviceTelemetry& outTelemetry) const {
        std::lock_guard<std::mutex> lock(m_engineMutex);
        for (const auto& out : m_renderOutputs) {
            if (out->GetId() == deviceId) {
                outTelemetry = out->GetTelemetry();
                return true;
            }
        }
        return false;
    }

    std::vector<DeviceTelemetry> SyncEngine::GetAllDeviceTelemetry() const {
        std::lock_guard<std::mutex> lock(m_engineMutex);
        std::vector<DeviceTelemetry> list;
        list.reserve(m_renderOutputs.size());
        for (const auto& out : m_renderOutputs) {
            list.push_back(out->GetTelemetry());
        }
        return list;
    }

    void SyncEngine::SetEventCallback(EngineEventCallback callback) {
        std::lock_guard<std::mutex> lock(m_engineMutex);
        m_eventCallback = std::move(callback);
    }

} // namespace Sync::Audio
