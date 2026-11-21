using System;
using System.Collections.ObjectModel;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Windows;
using System.Windows.Input;
using Sync.Core;
using Sync.Interop;

namespace Sync.App.ViewModels
{
    public class MainViewModel : ObservableObject
    {
        private readonly AudioService _audio;
        private readonly SettingsService _settings;

        private string _activeTab = "Dashboard";
        private TestSignalType _currentTestSignal = TestSignalType.None;
        private string _newGroupName = string.Empty;

        public AudioService Audio => _audio;
        public SettingsService Settings => _settings;

        public string ActiveTab
        {
            get => _activeTab;
            set => SetProperty(ref _activeTab, value);
        }

        public string NewGroupName
        {
            get => _newGroupName;
            set => SetProperty(ref _newGroupName, value);
        }

        public TestSignalType CurrentTestSignal
        {
            get => _currentTestSignal;
            set
            {
                if (SetProperty(ref _currentTestSignal, value))
                {
                    OnPropertyChanged(nameof(TestToneButtonText));
                    OnPropertyChanged(nameof(IsTestToneActive));
                }
            }
        }

        public bool IsTestToneActive => _currentTestSignal != TestSignalType.None;

        public string TestToneButtonText => _currentTestSignal switch
        {
            TestSignalType.PeriodicClick => "Tone: 1ms Click (Playing)",
            TestSignalType.LogarithmicChirp => "Tone: Log Chirp (Playing)",
            TestSignalType.DualPulse => "Tone: Dual Pulse (Playing)",
            TestSignalType.SineTone => "Tone: 1 kHz Sine (Playing)",
            _ => "Play Calibration Tone"
        };

        public ObservableCollection<DeviceGroupModel> SavedGroups =>
            new ObservableCollection<DeviceGroupModel>(_settings.Current.SavedGroups);

        public MainViewModel(AudioService audio, SettingsService settings)
        {
            _audio = audio;
            _settings = settings;

            _audio.PropertyChanged += (s, e) =>
            {
                if (e.PropertyName == nameof(AudioService.IsRunning))
                {
                    OnPropertyChanged(nameof(PlayButtonText));
                    OnPropertyChanged(nameof(StatusPillColor));
                    OnPropertyChanged(nameof(StatusPillText));
                }
                else if (e.PropertyName == nameof(AudioService.PeakLevelL) || e.PropertyName == nameof(AudioService.PeakLevelR))
                {
                    OnPropertyChanged(nameof(PeakLevelLeftPercent));
                    OnPropertyChanged(nameof(PeakLevelRightPercent));
                }
            };
        }

        public string PlayButtonText => _audio.IsRunning ? "Stop Synchronization" : "Start Synchronization";

        public string StatusPillText => _audio.IsRunning ? "SYNCHRONIZING" : "IDLE";

        public string StatusPillColor => _audio.IsRunning ? "#00E5A3" : "#8E9AA8";

        public double PeakLevelLeftPercent => Math.Clamp(_audio.PeakLevelL * 100.0, 0.0, 100.0);

        public double PeakLevelRightPercent => Math.Clamp(_audio.PeakLevelR * 100.0, 0.0, 100.0);

        public void TogglePlayback()
        {
            if (_audio.IsRunning)
            {
                _audio.Stop();
            }
            else
            {
                _audio.Start();
            }
            OnPropertyChanged(nameof(PlayButtonText));
        }

        public void ToggleMuteAll()
        {
            _audio.IsMutedAll = !_audio.IsMutedAll;
        }

        public void CycleTestTone()
        {
            var next = _currentTestSignal switch
            {
                TestSignalType.None => TestSignalType.PeriodicClick,
                TestSignalType.PeriodicClick => TestSignalType.LogarithmicChirp,
                TestSignalType.LogarithmicChirp => TestSignalType.DualPulse,
                TestSignalType.DualPulse => TestSignalType.SineTone,
                _ => TestSignalType.None
            };

            CurrentTestSignal = next;
            if (next == TestSignalType.None)
            {
                _audio.StopCalibrationTestTone();
            }
            else
            {
                _audio.PlayCalibrationTestTone(next);
            }
        }

        public void StopTestTone()
        {
            CurrentTestSignal = TestSignalType.None;
            _audio.StopCalibrationTestTone();
        }

        public void RefreshDevices()
        {
            _audio.RefreshDevices();
        }

        public void ToggleDeviceSelection(AudioDeviceModel device)
        {
            _audio.ToggleOutputSelection(device);
        }

        public void AdjustDeviceDelay(AudioDeviceModel device, int deltaMs)
        {
            int newDelay = Math.Clamp(device.DelayOffsetMs + deltaMs, -500, 500);
            _audio.SetOutputDelay(device, newDelay);
        }

        public void SaveCurrentGroup()
        {
            string name = string.IsNullOrWhiteSpace(_newGroupName) ? $"Group {_settings.Current.SavedGroups.Count + 1}" : _newGroupName.Trim();
            _audio.SaveDeviceGroup(name);
            NewGroupName = string.Empty;
            OnPropertyChanged(nameof(SavedGroups));
        }

        public void LoadGroup(DeviceGroupModel group)
        {
            _audio.LoadDeviceGroup(group);
        }

        public void ResetAllCalibration()
        {
            foreach (var dev in _audio.SelectedOutputs)
            {
                _audio.SetOutputDelay(dev, 0);
            }
            _settings.Current.SavedDeviceOffsets.Clear();
            _settings.Save();
        }

        public void ExportDiagnosticReport()
        {
            string report = _audio.GenerateDiagnosticReport();
            string folder = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.DesktopDirectory));
            string fileName = $"SYNC_Diagnostics_{DateTime.Now:yyyyMMdd_HHmmss}.txt";
            string fullPath = Path.Combine(folder, fileName);

            try
            {
                File.WriteAllText(fullPath, report);
                System.Windows.Clipboard.SetText(report);
                System.Windows.MessageBox.Show($"Diagnostic report saved to:\n{fullPath}\n\n(The report has also been copied to your clipboard).", "Diagnostic Report Exported", System.Windows.MessageBoxButton.OK, System.Windows.MessageBoxImage.Information);
            }
            catch (Exception ex)
            {
                System.Windows.Clipboard.SetText(report);
                System.Windows.MessageBox.Show($"Report copied to clipboard. (File save error: {ex.Message})", "SYNC Report", System.Windows.MessageBoxButton.OK, System.Windows.MessageBoxImage.Information);
            }
        }

        public void OpenBluetoothSettings()
        {
            try
            {
                Process.Start(new ProcessStartInfo("ms-settings:bluetooth") { UseShellExecute = true });
            }
            catch { }
        }

        public void OpenSoundSettings()
        {
            try
            {
                Process.Start(new ProcessStartInfo("ms-settings:sound") { UseShellExecute = true });
            }
            catch { }
        }
    }
}
