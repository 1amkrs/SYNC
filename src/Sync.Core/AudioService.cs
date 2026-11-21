using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using Sync.Interop;

namespace Sync.Core
{
    public class AudioService : ObservableObject, IDisposable
    {
        private readonly SyncAudioEngine _engine;
        private readonly SettingsService _settingsService;
        private readonly Timer _telemetryTimer;

        private bool _isRunning;
        private float _masterVolume = 1.0f;
        private bool _isMutedAll = false;
        private string _captureDeviceId = string.Empty;
        private string _captureDeviceName = string.Empty;
        private uint _captureSampleRate = 48000;
        private float _peakLevelL;
        private float _peakLevelR;
        private uint _activeOutputCount;
        private ulong _totalCapturedFrames;
        private ulong _totalRenderedFrames;
        private string _statusText = "Ready";

        public ObservableCollection<AudioDeviceModel> AvailableEndpoints { get; } = new();
        public ObservableCollection<AudioDeviceModel> SelectedOutputs { get; } = new();

        public event Action? StateChanged;
        public event Action<string>? ErrorOccurred;

        public AudioService(SettingsService settingsService)
        {
            _settingsService = settingsService;
            _engine = new SyncAudioEngine();

            _engine.PlaybackStarted += () =>
            {
                IsRunning = true;
                StatusText = "Synchronizing audio across outputs";
                StateChanged?.Invoke();
            };

            _engine.PlaybackStopped += () =>
            {
                IsRunning = false;
                StatusText = "Stopped";
                StateChanged?.Invoke();
            };

            _engine.DeviceListChanged += () =>
            {
                RefreshDevices();
            };

            _engine.EngineEvent += (type, msg) =>
            {
                if (type == "Error")
                {
                    StatusText = $"Error: {msg}";
                    ErrorOccurred?.Invoke(msg);
                }
            };

            // 20Hz telemetry polling timer
            _telemetryTimer = new Timer(OnTelemetryTick, null, 100, 50);

            RefreshDevices();
            ApplySettings();
        }

        public bool IsRunning
        {
            get => _isRunning;
            private set => SetProperty(ref _isRunning, value);
        }

        public float MasterVolume
        {
            get => _masterVolume;
            set
            {
                if (SetProperty(ref _masterVolume, Math.Clamp(value, 0.0f, 1.0f)))
                {
                    _engine.SetMasterVolume(_masterVolume);
                }
            }
        }

        public bool IsMutedAll
        {
            get => _isMutedAll;
            set
            {
                if (SetProperty(ref _isMutedAll, value))
                {
                    _engine.SetMuteAll(_isMutedAll);
                }
            }
        }

        public string CaptureDeviceId
        {
            get => _captureDeviceId;
            set
            {
                if (SetProperty(ref _captureDeviceId, value))
                {
                    _engine.SetCaptureDevice(_captureDeviceId);
                    UpdateCaptureDeviceName();
                }
            }
        }

        public string CaptureDeviceName
        {
            get => _captureDeviceName;
            private set => SetProperty(ref _captureDeviceName, value);
        }

        public uint CaptureSampleRate
        {
            get => _captureSampleRate;
            private set => SetProperty(ref _captureSampleRate, value);
        }

        public float PeakLevelL
        {
            get => _peakLevelL;
            private set => SetProperty(ref _peakLevelL, value);
        }

        public float PeakLevelR
        {
            get => _peakLevelR;
            private set => SetProperty(ref _peakLevelR, value);
        }

        public uint ActiveOutputCount
        {
            get => _activeOutputCount;
            private set => SetProperty(ref _activeOutputCount, value);
        }

        public ulong TotalCapturedFrames
        {
            get => _totalCapturedFrames;
            private set => SetProperty(ref _totalCapturedFrames, value);
        }

        public ulong TotalRenderedFrames
        {
            get => _totalRenderedFrames;
            private set => SetProperty(ref _totalRenderedFrames, value);
        }

        public string StatusText
        {
            get => _statusText;
            private set => SetProperty(ref _statusText, value);
        }

        public void RefreshDevices()
        {
            var rawList = _engine.EnumerateDevices();

            // Run update on main thread if needed
            var existingMap = AvailableEndpoints.ToDictionary(d => d.Id);
            var updatedList = new List<AudioDeviceModel>();

            foreach (var raw in rawList)
            {
                if (existingMap.TryGetValue(raw.Id, out var existing))
                {
                    existing.Name = raw.Name;
                    existing.AdapterName = raw.AdapterName;
                    existing.IsDefault = raw.IsDefault != 0;
                    existing.IsBluetooth = raw.IsBluetooth != 0;
                    existing.TransportType = (DeviceTransportType)raw.TransportType;
                    updatedList.Add(existing);
                }
                else
                {
                    var dev = new AudioDeviceModel
                    {
                        Id = raw.Id,
                        Name = raw.Name,
                        AdapterName = raw.AdapterName,
                        IsDefault = raw.IsDefault != 0,
                        IsBluetooth = raw.IsBluetooth != 0,
                        TransportType = (DeviceTransportType)raw.TransportType,
                        CustomLabel = _settingsService.GetCustomDeviceLabel(raw.Id),
                        DelayOffsetMs = _settingsService.GetSavedDeviceOffset(raw.Id),
                        Volume = 1.0f,
                        IsEnabled = true
                    };
                    updatedList.Add(dev);
                }
            }

            AvailableEndpoints.Clear();
            foreach (var d in updatedList)
            {
                AvailableEndpoints.Add(d);
            }

            // Sync Capture Device Name
            UpdateCaptureDeviceName();
        }

        private void UpdateCaptureDeviceName()
        {
            string capId = _engine.GetCaptureDevice();
            if (string.IsNullOrEmpty(capId))
            {
                var def = AvailableEndpoints.FirstOrDefault(d => d.IsDefault);
                CaptureDeviceName = def != null ? $"{def.DisplayName} (Default Loopback)" : "Windows Default Audio";
            }
            else
            {
                var match = AvailableEndpoints.FirstOrDefault(d => d.Id == capId);
                CaptureDeviceName = match != null ? match.DisplayName : "Windows Audio Loopback";
            }
        }

        public bool ToggleOutputSelection(AudioDeviceModel device)
        {
            if (device.IsSelected)
            {
                return RemoveOutput(device);
            }
            else
            {
                return AddOutput(device);
            }
        }

        public bool AddOutput(AudioDeviceModel device)
        {
            // Feedback loop check
            if (!string.IsNullOrEmpty(_captureDeviceId) && device.Id == _captureDeviceId)
            {
                StatusText = "Cannot select the capture source as an output (feedback loop prevention).";
                ErrorOccurred?.Invoke(StatusText);
                return false;
            }

            if (!_engine.AddOutputDevice(device.Id))
            {
                StatusText = $"Failed to open output: {device.DisplayName}";
                return false;
            }

            device.IsSelected = true;
            if (!SelectedOutputs.Contains(device))
            {
                SelectedOutputs.Add(device);
            }

            _engine.SetOutputVolume(device.Id, device.Volume);
            _engine.SetOutputDelayMs(device.Id, device.DelayOffsetMs);
            _engine.SetOutputEnabled(device.Id, device.IsEnabled);

            StateChanged?.Invoke();
            return true;
        }

        public bool RemoveOutput(AudioDeviceModel device)
        {
            _engine.RemoveOutputDevice(device.Id);
            device.IsSelected = false;
            SelectedOutputs.Remove(device);
            StateChanged?.Invoke();
            return true;
        }

        public void SetOutputVolume(AudioDeviceModel device, float volume)
        {
            device.Volume = volume;
            _engine.SetOutputVolume(device.Id, volume);
        }

        public void SetOutputDelay(AudioDeviceModel device, int delayMs)
        {
            device.DelayOffsetMs = delayMs;
            _engine.SetOutputDelayMs(device.Id, delayMs);
            _settingsService.SaveDeviceOffset(device.Id, delayMs);
        }

        public void SetOutputEnabled(AudioDeviceModel device, bool enabled)
        {
            device.IsEnabled = enabled;
            _engine.SetOutputEnabled(device.Id, enabled);
        }

        public bool Start()
        {
            if (SelectedOutputs.Count == 0)
            {
                StatusText = "Please select at least one output device.";
                ErrorOccurred?.Invoke(StatusText);
                return false;
            }

            bool ok = _engine.Start();
            IsRunning = _engine.IsRunning;
            return ok;
        }

        public void Stop()
        {
            _engine.Stop();
            IsRunning = false;
        }

        public void PlayCalibrationTestTone(TestSignalType type)
        {
            _engine.PlayTestTone(type);
        }

        public void StopCalibrationTestTone()
        {
            _engine.StopTestTone();
        }

        public void SetQualityPreset(string preset)
        {
            int bufferMs = preset switch
            {
                "Low Latency" => 50,
                "High Stability" => 200,
                _ => 100
            };
            _settingsService.Current.QualityPreset = preset;
            _settingsService.Current.PreferredBufferMs = bufferMs;
            _settingsService.Save();
            _engine.SetBaseBufferingDelayMs(bufferMs);
        }

        public void SaveDeviceGroup(string groupName)
        {
            var group = new DeviceGroupModel
            {
                GroupName = groupName,
                Entries = SelectedOutputs.Select(o => new DeviceGroupEntry
                {
                    DeviceId = o.Id,
                    Volume = o.Volume,
                    DelayMs = o.DelayOffsetMs,
                    IsEnabled = o.IsEnabled
                }).ToList()
            };

            var existing = _settingsService.Current.SavedGroups.FirstOrDefault(g => g.GroupName == groupName);
            if (existing != null)
            {
                _settingsService.Current.SavedGroups.Remove(existing);
            }
            _settingsService.Current.SavedGroups.Add(group);
            _settingsService.Save();
        }

        public void LoadDeviceGroup(DeviceGroupModel group)
        {
            // Clear current selection
            foreach (var outDev in SelectedOutputs.ToList())
            {
                RemoveOutput(outDev);
            }

            foreach (var entry in group.Entries)
            {
                var match = AvailableEndpoints.FirstOrDefault(d => d.Id == entry.DeviceId);
                if (match != null)
                {
                    match.Volume = entry.Volume;
                    match.DelayOffsetMs = entry.DelayMs;
                    match.IsEnabled = entry.IsEnabled;
                    AddOutput(match);
                }
            }
        }

        public string GenerateDiagnosticReport()
        {
            var sb = new StringBuilder();
            sb.AppendLine("=== SYNC Audio Diagnostic Report ===");
            sb.AppendLine($"Timestamp: {DateTime.Now:yyyy-MM-dd HH:mm:ss}");
            sb.AppendLine($"Engine Running: {IsRunning}");
            sb.AppendLine($"Capture Device: {CaptureDeviceName} (ID: {_captureDeviceId})");
            sb.AppendLine($"Capture Format: {CaptureSampleRate} Hz, 2 Channels Float");
            sb.AppendLine($"Total Captured Frames: {TotalCapturedFrames}");
            sb.AppendLine($"Total Rendered Frames: {TotalRenderedFrames}");
            sb.AppendLine($"Active Outputs: {SelectedOutputs.Count}");
            sb.AppendLine();
            sb.AppendLine("--- Selected Output Devices ---");
            foreach (var outDev in SelectedOutputs)
            {
                sb.AppendLine($"Device: {outDev.DisplayName}");
                sb.AppendLine($"  ID: {outDev.Id}");
                sb.AppendLine($"  Transport: {outDev.TransportDisplay}");
                sb.AppendLine($"  Volume: {(int)(outDev.Volume * 100)}%");
                sb.AppendLine($"  Calibrated Delay: {outDev.DelayOffsetMs} ms");
                sb.AppendLine($"  Buffer Occupancy: {outDev.BufferOccupancyPercent}%");
                sb.AppendLine($"  Underruns: {outDev.UnderrunCount}");
                sb.AppendLine($"  Estimated Drift: {outDev.EstimatedDriftPpm} PPM");
                sb.AppendLine($"  Hardware/WASAPI Latency: {outDev.MeasuredLatencyMs:F1} ms");
                sb.AppendLine($"  Status: {(outDev.IsConnected ? "Connected" : "Disconnected")}");
                sb.AppendLine();
            }

            sb.AppendLine("--- All Available Endpoints ---");
            foreach (var dev in AvailableEndpoints)
            {
                sb.AppendLine($"[{dev.TransportDisplay}] {dev.DisplayName} (Default: {dev.IsDefault}, BT: {dev.IsBluetooth})");
            }

            return sb.ToString();
        }

        private void OnTelemetryTick(object? state)
        {
            try
            {
                if (!_engine.IsRunning && !IsRunning)
                {
                    PeakLevelL = 0.0f;
                    PeakLevelR = 0.0f;
                    return;
                }

                var master = _engine.GetMasterTelemetry();
                CaptureSampleRate = master.CaptureSampleRate;
                PeakLevelL = master.AudioLevelPeakL;
                PeakLevelR = master.AudioLevelPeakR;
                ActiveOutputCount = master.ActiveOutputs;
                TotalCapturedFrames = master.TotalCapturedFrames;
                TotalRenderedFrames = master.TotalRenderedFrames;

                foreach (var outDev in SelectedOutputs)
                {
                    if (_engine.TryGetDeviceTelemetry(outDev.Id, out var devTelem))
                    {
                        outDev.BufferOccupancyPercent = devTelem.BufferOccupancyPercent;
                        outDev.UnderrunCount = devTelem.UnderrunCount;
                        outDev.EstimatedDriftPpm = devTelem.EstimatedDriftPpm;
                        outDev.MeasuredLatencyMs = devTelem.MeasuredLatencyMs;
                        outDev.IsConnected = devTelem.IsConnected != 0;
                    }
                }
            }
            catch
            {
                // Telemetry tick should not throw
            }
        }

        private void ApplySettings()
        {
            _engine.SetBaseBufferingDelayMs(_settingsService.Current.PreferredBufferMs);
            if (!string.IsNullOrEmpty(_settingsService.Current.PreferredCaptureDeviceId))
            {
                CaptureDeviceId = _settingsService.Current.PreferredCaptureDeviceId;
            }
        }

        public void Dispose()
        {
            _telemetryTimer.Dispose();
            _engine.Dispose();
        }
    }
}
