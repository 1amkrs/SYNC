using System;
using System.IO;
using System.Text.Json;

namespace Sync.Core
{
    public class SettingsService
    {
        private static readonly string SettingsFolder = Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
            "SYNC"
        );
        private static readonly string SettingsFilePath = Path.Combine(SettingsFolder, "settings.json");

        private readonly object _lock = new();
        private AppSettingsModel _current = new();

        public AppSettingsModel Current
        {
            get
            {
                lock (_lock)
                {
                    return _current;
                }
            }
        }

        public void Load()
        {
            lock (_lock)
            {
                try
                {
                    if (File.Exists(SettingsFilePath))
                    {
                        string json = File.ReadAllText(SettingsFilePath);
                        var loaded = JsonSerializer.Deserialize<AppSettingsModel>(json);
                        if (loaded != null)
                        {
                            _current = loaded;
                            return;
                        }
                    }
                }
                catch (Exception ex)
                {
                    System.Diagnostics.Debug.WriteLine($"Failed to load settings: {ex.Message}");
                }
                _current = new AppSettingsModel();
            }
        }

        public void Save()
        {
            lock (_lock)
            {
                try
                {
                    Directory.CreateDirectory(SettingsFolder);
                    var options = new JsonSerializerOptions { WriteIndented = true };
                    string json = JsonSerializer.Serialize(_current, options);
                    File.WriteAllText(SettingsFilePath, json);
                }
                catch (Exception ex)
                {
                    System.Diagnostics.Debug.WriteLine($"Failed to save settings: {ex.Message}");
                }
            }
        }

        public void SaveDeviceOffset(string deviceId, int offsetMs)
        {
            lock (_lock)
            {
                _current.SavedDeviceOffsets[deviceId] = offsetMs;
                Save();
            }
        }

        public int GetSavedDeviceOffset(string deviceId)
        {
            lock (_lock)
            {
                return _current.SavedDeviceOffsets.TryGetValue(deviceId, out int offset) ? offset : 0;
            }
        }

        public void SaveDeviceLabel(string deviceId, string customLabel)
        {
            lock (_lock)
            {
                _current.CustomDeviceLabels[deviceId] = customLabel;
                Save();
            }
        }

        public string GetCustomDeviceLabel(string deviceId)
        {
            lock (_lock)
            {
                return _current.CustomDeviceLabels.TryGetValue(deviceId, out var label) ? label : string.Empty;
            }
        }
    }
}
