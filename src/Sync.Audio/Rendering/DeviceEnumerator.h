#pragma once

#include "Common.h"
#include <vector>
#include <string>
#include <functional>
#include <mutex>

namespace Sync::Audio {

    using DeviceChangeCallback = std::function<void()>;

    class DeviceEnumerator : public IMMNotificationClient {
    public:
        DeviceEnumerator();
        ~DeviceEnumerator();

        bool Initialize();
        void Shutdown();

        // Enumerate all active render endpoints
        std::vector<DeviceInfo> EnumerateEndpoints();

        // Get default render endpoint
        bool GetDefaultEndpoint(DeviceInfo& outInfo);

        // Get specific endpoint by ID
        bool GetEndpointById(const std::wstring& deviceId, DeviceInfo& outInfo);

        // Register callback for hardware arrival/removal/default change
        void SetChangeCallback(DeviceChangeCallback callback);

        // IUnknown implementation
        ULONG STDMETHODCALLTYPE AddRef() override;
        ULONG STDMETHODCALLTYPE Release() override;
        HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, VOID** ppvInterface) override;

        // IMMNotificationClient implementation
        HRESULT STDMETHODCALLTYPE OnDeviceStateChanged(LPCWSTR pwstrDeviceId, DWORD dwNewState) override;
        HRESULT STDMETHODCALLTYPE OnDeviceAdded(LPCWSTR pwstrDeviceId) override;
        HRESULT STDMETHODCALLTYPE OnDeviceRemoved(LPCWSTR pwstrDeviceId) override;
        HRESULT STDMETHODCALLTYPE OnDefaultDeviceChanged(EDataFlow flow, ERole role, LPCWSTR pwstrDefaultDeviceId) override;
        HRESULT STDMETHODCALLTYPE OnPropertyValueChanged(LPCWSTR pwstrDeviceId, const PROPERTYKEY key) override;

    private:
        static DeviceTransportType DetectTransportType(IMMDevice* device, const std::wstring& name);
        static AudioFormat GetNativeFormat(IMMDevice* device);

        std::atomic<ULONG> m_refCount{ 1 };
        ComPtr<IMMDeviceEnumerator> m_enumerator;
        DeviceChangeCallback m_changeCallback;
        std::mutex m_mutex;
        bool m_isRegistered = false;
    };

} // namespace Sync::Audio
