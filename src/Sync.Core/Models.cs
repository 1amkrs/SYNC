using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Runtime.CompilerServices;
using Sync.Interop;

namespace Sync.Core
{
    public class ObservableObject : INotifyPropertyChanged
    {
        public event PropertyChangedEventHandler? PropertyChanged;

        protected void OnPropertyChanged([CallerMemberName] string? propertyName = null)
        {
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(propertyName));
        }

        protected bool SetProperty<T>(ref T field, T value, [CallerMemberName] string? propertyName = null)
        {
            if (EqualityComparer<T>.Default.Equals(field, value)) return false;
            field = value;
            OnPropertyChanged(propertyName);
            return true;
        }
    }

    public class AudioDeviceModel : ObservableObject
    {
        private string _id = string.Empty;
        private string _name = string.Empty;
        private string _customLabel = string.Empty;
        private string _adapterName = string.Empty;
        private DeviceTransportType _transportType = DeviceTransportType.Unknown;
        private bool _isDefault;
        private bool _isBluetooth;
        private bool _isSelected;
        private bool _isEnabled = true;
        private bool _isConnected = true;
        private float _volume = 1.0f;
        private int _delayOffsetMs = 0;
        private uint _bufferOccupancyPercent = 0;
        private uint _underrunCount = 0;
        private int _estimatedDriftPpm = 0;
        private float _measuredLatencyMs = 0.0f;

        public string Id
        {
            get => _id;
            set => SetProperty(ref _id, value);
        }

        public string Name
        {
            get => _name;
            set => SetProperty(ref _name, value);
        }

        public string CustomLabel
        {
            get => string.IsNullOrWhiteSpace(_customLabel) ? _name : _customLabel;
            set
            {
                SetProperty(ref _customLabel, value);
                OnPropertyChanged(nameof(DisplayName));
            }
        }

        public string DisplayName => string.IsNullOrWhiteSpace(_customLabel) ? _name : $"{_customLabel} ({_name})";

        public string AdapterName
        {
            get => _adapterName;
            set => SetProperty(ref _adapterName, value);
        }

        public DeviceTransportType TransportType
        {
            get => _transportType;
            set => SetProperty(ref _transportType, value);
        }

        public bool IsDefault
        {
            get => _isDefault;
            set => SetProperty(ref _isDefault, value);
        }

        public bool IsBluetooth
        {
            get => _isBluetooth;
            set => SetProperty(ref _isBluetooth, value);
        }

        public bool IsSelected
        {
            get => _isSelected;
            set => SetProperty(ref _isSelected, value);
        }

        public bool IsEnabled
        {
            get => _isEnabled;
            set => SetProperty(ref _isEnabled, value);
        }

        public bool IsConnected
        {
            get => _isConnected;
            set => SetProperty(ref _isConnected, value);
        }

        public float Volume
        {
            get => _volume;
            set => SetProperty(ref _volume, Math.Clamp(value, 0.0f, 1.0f));
        }

        public int DelayOffsetMs
        {
            get => _delayOffsetMs;
            set => SetProperty(ref _delayOffsetMs, Math.Clamp(value, -500, 500));
        }

        public uint BufferOccupancyPercent
        {
            get => _bufferOccupancyPercent;
            set => SetProperty(ref _bufferOccupancyPercent, value);
        }

        public uint UnderrunCount
        {
            get => _underrunCount;
            set => SetProperty(ref _underrunCount, value);
        }

        public int EstimatedDriftPpm
        {
            get => _estimatedDriftPpm;
            set => SetProperty(ref _estimatedDriftPpm, value);
        }

        public float MeasuredLatencyMs
        {
            get => _measuredLatencyMs;
            set => SetProperty(ref _measuredLatencyMs, value);
        }

        public string TransportDisplay => _transportType switch
        {
            DeviceTransportType.Bluetooth => "Bluetooth",
            DeviceTransportType.Headphones => "Headphones",
            DeviceTransportType.Usb => "USB Audio",
            DeviceTransportType.Hdmi => "HDMI / DisplayPort",
            DeviceTransportType.InternalSpeakers => "Built-in Speakers",
            _ => "Audio Endpoint"
        };
    }

    public class DeviceGroupModel
    {
        public string GroupName { get; set; } = "Default Group";
        public List<DeviceGroupEntry> Entries { get; set; } = new();
    }

    public class DeviceGroupEntry
    {
        public string DeviceId { get; set; } = string.Empty;
        public float Volume { get; set; } = 1.0f;
        public int DelayMs { get; set; } = 0;
        public bool IsEnabled { get; set; } = true;
    }

    public class AppSettingsModel
    {
        public bool LaunchAtStartup { get; set; } = false;
        public bool MinimizeToTray { get; set; } = true;
        public bool AutoStartPlayback { get; set; } = false;
        public int PreferredBufferMs { get; set; } = 100;
        public string QualityPreset { get; set; } = "Balanced"; // Low Latency (50ms), Balanced (100ms), High Stability (200ms)
        public string? PreferredCaptureDeviceId { get; set; }
        public string Theme { get; set; } = "Dark";
        public bool EnableDiagnosticLogging { get; set; } = true;
        public Dictionary<string, int> SavedDeviceOffsets { get; set; } = new();
        public Dictionary<string, string> CustomDeviceLabels { get; set; } = new();
        public List<DeviceGroupModel> SavedGroups { get; set; } = new();
    }
}
