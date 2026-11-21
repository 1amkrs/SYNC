#include "DeviceEnumerator.h"
#include <propsys.h>
#include <propkey.h>

namespace Sync::Audio {

    DeviceEnumerator::DeviceEnumerator() = default;

    DeviceEnumerator::~DeviceEnumerator() {
        Shutdown();
    }

    bool DeviceEnumerator::Initialize() {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_enumerator) return true;

        HRESULT hr = CoCreateInstance(
            __uuidof(MMDeviceEnumerator),
            nullptr,
            CLSCTX_ALL,
            __uuidof(IMMDeviceEnumerator),
            reinterpret_cast<void**>(m_enumerator.GetAddressOf())
        );

        if (FAILED(hr)) return false;

        hr = m_enumerator->RegisterEndpointNotificationCallback(this);
        m_isRegistered = SUCCEEDED(hr);

        return true;
    }

    void DeviceEnumerator::Shutdown() {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_enumerator && m_isRegistered) {
            m_enumerator->UnregisterEndpointNotificationCallback(this);
            m_isRegistered = false;
        }
        m_enumerator.Reset();
    }

    std::vector<DeviceInfo> DeviceEnumerator::EnumerateEndpoints() {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<DeviceInfo> list;
        if (!m_enumerator) return list;

        ComPtr<IMMDevice> defaultDevice;
        std::wstring defaultId;
        if (SUCCEEDED(m_enumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, defaultDevice.GetAddressOf()))) {
            LPWSTR defIdStr = nullptr;
            if (SUCCEEDED(defaultDevice->GetId(&defIdStr)) && defIdStr) {
                defaultId = defIdStr;
                CoTaskMemFree(defIdStr);
            }
        }

        ComPtr<IMMDeviceCollection> collection;
        HRESULT hr = m_enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, collection.GetAddressOf());
        if (FAILED(hr) || !collection) return list;

        UINT count = 0;
        collection->GetCount(&count);

        for (UINT i = 0; i < count; ++i) {
            ComPtr<IMMDevice> device;
            if (FAILED(collection->Item(i, device.GetAddressOf())) || !device) continue;

            LPWSTR idStr = nullptr;
            if (FAILED(device->GetId(&idStr)) || !idStr) continue;
            std::wstring id = idStr;
            CoTaskMemFree(idStr);

            DeviceInfo info;
            info.id = id;
            info.isDefault = (!defaultId.empty() && id == defaultId);

            ComPtr<IPropertyStore> props;
            if (SUCCEEDED(device->OpenPropertyStore(STGM_READ, props.GetAddressOf())) && props) {
                PROPVARIANT varName;
                PropVariantInit(&varName);
                if (SUCCEEDED(props->GetValue(PKEY_Device_FriendlyName, &varName)) && varName.pwszVal) {
                    info.name = varName.pwszVal;
                }
                PropVariantClear(&varName);

                PROPVARIANT varDesc;
                PropVariantInit(&varDesc);
                if (SUCCEEDED(props->GetValue(PKEY_Device_DeviceDesc, &varDesc)) && varDesc.pwszVal) {
                    info.adapterName = varDesc.pwszVal;
                }
                PropVariantClear(&varDesc);
            }

            if (info.name.empty()) {
                info.name = L"Audio Device " + std::to_wstring(i + 1);
            }

            info.transportType = DetectTransportType(device.Get(), info.name);
            info.isBluetooth = (info.transportType == DeviceTransportType::Bluetooth);
            info.nativeFormat = GetNativeFormat(device.Get());

            list.push_back(std::move(info));
        }

        return list;
    }

    bool DeviceEnumerator::GetDefaultEndpoint(DeviceInfo& outInfo) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_enumerator) return false;

        ComPtr<IMMDevice> device;
        if (FAILED(m_enumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, device.GetAddressOf())) || !device) {
            return false;
        }

        LPWSTR idStr = nullptr;
        if (FAILED(device->GetId(&idStr)) || !idStr) return false;
        outInfo.id = idStr;
        CoTaskMemFree(idStr);
        outInfo.isDefault = true;

        ComPtr<IPropertyStore> props;
        if (SUCCEEDED(device->OpenPropertyStore(STGM_READ, props.GetAddressOf())) && props) {
            PROPVARIANT varName;
            PropVariantInit(&varName);
            if (SUCCEEDED(props->GetValue(PKEY_Device_FriendlyName, &varName)) && varName.pwszVal) {
                outInfo.name = varName.pwszVal;
            }
            PropVariantClear(&varName);
        }

        outInfo.transportType = DetectTransportType(device.Get(), outInfo.name);
        outInfo.isBluetooth = (outInfo.transportType == DeviceTransportType::Bluetooth);
        outInfo.nativeFormat = GetNativeFormat(device.Get());
        return true;
    }

    bool DeviceEnumerator::GetEndpointById(const std::wstring& deviceId, DeviceInfo& outInfo) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_enumerator) return false;

        ComPtr<IMMDevice> device;
        if (FAILED(m_enumerator->GetDevice(deviceId.c_str(), device.GetAddressOf())) || !device) {
            return false;
        }

        outInfo.id = deviceId;
        ComPtr<IPropertyStore> props;
        if (SUCCEEDED(device->OpenPropertyStore(STGM_READ, props.GetAddressOf())) && props) {
            PROPVARIANT varName;
            PropVariantInit(&varName);
            if (SUCCEEDED(props->GetValue(PKEY_Device_FriendlyName, &varName)) && varName.pwszVal) {
                outInfo.name = varName.pwszVal;
            }
            PropVariantClear(&varName);
        }

        outInfo.transportType = DetectTransportType(device.Get(), outInfo.name);
        outInfo.isBluetooth = (outInfo.transportType == DeviceTransportType::Bluetooth);
        outInfo.nativeFormat = GetNativeFormat(device.Get());
        return true;
    }

    void DeviceEnumerator::SetChangeCallback(DeviceChangeCallback callback) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_changeCallback = callback;
    }

    DeviceTransportType DeviceEnumerator::DetectTransportType(IMMDevice* device, const std::wstring& name) {
        // Check keywords in name
        std::wstring lowerName = name;
        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::towlower);

        if (lowerName.find(L"bluetooth") != std::wstring::npos ||
            lowerName.find(L"hands-free") != std::wstring::npos ||
            lowerName.find(L"airpods") != std::wstring::npos ||
            lowerName.find(L"buds") != std::wstring::npos ||
            lowerName.find(L"bt ") != std::wstring::npos ||
            lowerName.find(L"bth") != std::wstring::npos) {
            return DeviceTransportType::Bluetooth;
        }

        if (lowerName.find(L"headphone") != std::wstring::npos ||
            lowerName.find(L"headset") != std::wstring::npos) {
            return DeviceTransportType::Headphones;
        }

        if (lowerName.find(L"hdmi") != std::wstring::npos ||
            lowerName.find(L"displayport") != std::wstring::npos ||
            lowerName.find(L"nvidia") != std::wstring::npos ||
            lowerName.find(L"amd high definition") != std::wstring::npos) {
            return DeviceTransportType::Hdmi;
        }

        if (lowerName.find(L"usb") != std::wstring::npos) {
            return DeviceTransportType::Usb;
        }

        // Check FormFactor property if available
        ComPtr<IPropertyStore> props;
        if (SUCCEEDED(device->OpenPropertyStore(STGM_READ, props.GetAddressOf())) && props) {
            PROPVARIANT varFormFactor;
            PropVariantInit(&varFormFactor);
            // PKEY_AudioEndpoint_FormFactor: {1da5d803-d492-4edd-8c23-e0c0ffee7f0e}, 0
            static const PROPERTYKEY keyFormFactor = { { 0x1da5d803, 0xd492, 0x4edd, { 0x8c, 0x23, 0xe0, 0xc0, 0xff, 0xee, 0x7f, 0x0e } }, 0 };
            if (SUCCEEDED(props->GetValue(keyFormFactor, &varFormFactor))) {
                const auto formFactor = static_cast<EndpointFormFactor>(varFormFactor.uintVal);
                if (formFactor == Headphones || formFactor == Headset) {
                    PropVariantClear(&varFormFactor);
                    return DeviceTransportType::Headphones;
                }
                if (formFactor == DigitalAudioDisplayDevice) {
                    PropVariantClear(&varFormFactor);
                    return DeviceTransportType::Hdmi;
                }
                PropVariantClear(&varFormFactor);
            }
        }

        return DeviceTransportType::InternalSpeakers;
    }

    AudioFormat DeviceEnumerator::GetNativeFormat(IMMDevice* device) {
        AudioFormat fmt;
        ComPtr<IAudioClient> client;
        if (SUCCEEDED(device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, reinterpret_cast<void**>(client.GetAddressOf()))) && client) {
            WAVEFORMATEX* mixFormat = nullptr;
            if (SUCCEEDED(client->GetMixFormat(&mixFormat)) && mixFormat) {
                fmt.sampleRate = mixFormat->nSamplesPerSec;
                fmt.channels = mixFormat->nChannels;
                fmt.bitsPerSample = mixFormat->wBitsPerSample;
                fmt.isFloat = (mixFormat->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) ||
                    (mixFormat->wFormatTag == WAVE_FORMAT_EXTENSIBLE &&
                     reinterpret_cast<WAVEFORMATEXTENSIBLE*>(mixFormat)->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT);
                CoTaskMemFree(mixFormat);
            }
        }
        return fmt;
    }

    // IUnknown methods
    ULONG STDMETHODCALLTYPE DeviceEnumerator::AddRef() {
        return ++m_refCount;
    }

    ULONG STDMETHODCALLTYPE DeviceEnumerator::Release() {
        ULONG count = --m_refCount;
        if (count == 0) delete this;
        return count;
    }

    HRESULT STDMETHODCALLTYPE DeviceEnumerator::QueryInterface(REFIID riid, VOID** ppvInterface) {
        if (!ppvInterface) return E_POINTER;
        if (riid == __uuidof(IUnknown) || riid == __uuidof(IMMNotificationClient)) {
            *ppvInterface = static_cast<IMMNotificationClient*>(this);
            AddRef();
            return S_OK;
        }
        *ppvInterface = nullptr;
        return E_NOINTERFACE;
    }

    // IMMNotificationClient callbacks
    HRESULT STDMETHODCALLTYPE DeviceEnumerator::OnDeviceStateChanged(LPCWSTR, DWORD) {
        if (m_changeCallback) m_changeCallback();
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE DeviceEnumerator::OnDeviceAdded(LPCWSTR) {
        if (m_changeCallback) m_changeCallback();
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE DeviceEnumerator::OnDeviceRemoved(LPCWSTR) {
        if (m_changeCallback) m_changeCallback();
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE DeviceEnumerator::OnDefaultDeviceChanged(EDataFlow flow, ERole, LPCWSTR) {
        if (flow == eRender && m_changeCallback) {
            m_changeCallback();
        }
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE DeviceEnumerator::OnPropertyValueChanged(LPCWSTR, const PROPERTYKEY) {
        return S_OK;
    }

} // namespace Sync::Audio
