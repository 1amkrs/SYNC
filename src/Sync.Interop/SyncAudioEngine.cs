using System;
using System.Collections.Generic;
using System.IO;
using System.Runtime.InteropServices;

namespace Sync.Interop
{
    public sealed class SyncAudioEngine : IDisposable
    {
        private IntPtr _handle = IntPtr.Zero;
        private EngineEventCallbackNative? _nativeCallbackHolder;
        private bool _disposed = false;

        public event Action? PlaybackStarted;
        public event Action? PlaybackStopped;
        public event Action? DeviceListChanged;
        public event Action<string, string>? EngineEvent;

        static SyncAudioEngine()
        {
            // Ensure SyncAudio.dll directory is in DLL search path
            string appDir = AppDomain.CurrentDomain.BaseDirectory;
            SetDllDirectory(appDir);

            // Also check relative build directories for developer environments
            string[] probePaths = [
                Path.Combine(appDir, "SyncAudio.dll"),
                Path.Combine(appDir, "..", "..", "..", "..", "bin", "x64", "Release", "SyncAudio.dll"),
                Path.Combine(appDir, "..", "..", "..", "..", "src", "Sync.Audio", "bin", "x64", "Release", "SyncAudio.dll")
            ];

            foreach (var path in probePaths)
            {
                if (File.Exists(path))
                {
                    LoadLibrary(path);
                    break;
                }
            }
        }

        [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
        private static extern bool SetDllDirectory(string lpPathName);

        [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
        private static extern IntPtr LoadLibrary(string lpLibFileName);

        public SyncAudioEngine()
        {
            int hr = NativeMethods.SyncAudio_CreateEngine(out _handle);
            if (hr != 0 || _handle == IntPtr.Zero)
            {
                throw new InvalidOperationException($"Failed to create native Sync Audio Engine (Error code: {hr}). Ensure SyncAudio.dll and audio endpoints are available.");
            }

            _nativeCallbackHolder = OnNativeEvent;
            NativeMethods.SyncAudio_RegisterEventCallback(_handle, _nativeCallbackHolder);
        }

        public IReadOnlyList<DeviceInfoNative> EnumerateDevices()
        {
            ThrowIfDisposed();
            int hr = NativeMethods.SyncAudio_EnumerateDevices(_handle, out IntPtr pDevices, out uint count);
            if (hr != 0 || pDevices == IntPtr.Zero || count == 0)
            {
                return Array.Empty<DeviceInfoNative>();
            }

            try
            {
                var list = new List<DeviceInfoNative>((int)count);
                int structSize = Marshal.SizeOf<DeviceInfoNative>();
                for (int i = 0; i < count; i++)
                {
                    IntPtr itemPtr = IntPtr.Add(pDevices, i * structSize);
                    var item = Marshal.PtrToStructure<DeviceInfoNative>(itemPtr);
                    list.Add(item);
                }
                return list;
            }
            finally
            {
                NativeMethods.SyncAudio_FreeDeviceList(pDevices);
            }
        }

        public bool SetCaptureDevice(string deviceId)
        {
            ThrowIfDisposed();
            return NativeMethods.SyncAudio_SetCaptureDevice(_handle, deviceId) == 0;
        }

        public string GetCaptureDevice()
        {
            ThrowIfDisposed();
            char[] buffer = new char[128];
            int hr = NativeMethods.SyncAudio_GetCaptureDevice(_handle, buffer, (uint)buffer.Length);
            return (hr == 0) ? new string(buffer).TrimEnd('\0') : string.Empty;
        }

        public bool AddOutputDevice(string deviceId)
        {
            ThrowIfDisposed();
            return NativeMethods.SyncAudio_AddOutputDevice(_handle, deviceId) == 0;
        }

        public bool RemoveOutputDevice(string deviceId)
        {
            ThrowIfDisposed();
            return NativeMethods.SyncAudio_RemoveOutputDevice(_handle, deviceId) == 0;
        }

        public void ClearOutputDevices()
        {
            ThrowIfDisposed();
            NativeMethods.SyncAudio_ClearOutputDevices(_handle);
        }

        public void SetOutputVolume(string deviceId, float volume)
        {
            ThrowIfDisposed();
            NativeMethods.SyncAudio_SetOutputVolume(_handle, deviceId, volume);
        }

        public float GetOutputVolume(string deviceId)
        {
            ThrowIfDisposed();
            int hr = NativeMethods.SyncAudio_GetOutputVolume(_handle, deviceId, out float vol);
            return (hr == 0) ? vol : 1.0f;
        }

        public void SetOutputDelayMs(string deviceId, int delayMs)
        {
            ThrowIfDisposed();
            NativeMethods.SyncAudio_SetOutputDelayMs(_handle, deviceId, delayMs);
        }

        public int GetOutputDelayMs(string deviceId)
        {
            ThrowIfDisposed();
            int hr = NativeMethods.SyncAudio_GetOutputDelayMs(_handle, deviceId, out int delay);
            return (hr == 0) ? delay : 0;
        }

        public void SetOutputEnabled(string deviceId, bool enabled)
        {
            ThrowIfDisposed();
            NativeMethods.SyncAudio_SetOutputEnabled(_handle, deviceId, enabled ? 1 : 0);
        }

        public bool IsOutputEnabled(string deviceId)
        {
            ThrowIfDisposed();
            int hr = NativeMethods.SyncAudio_IsOutputEnabled(_handle, deviceId, out int enabled);
            return hr == 0 && enabled != 0;
        }

        public bool Start()
        {
            ThrowIfDisposed();
            return NativeMethods.SyncAudio_Start(_handle) == 0;
        }

        public void Stop()
        {
            ThrowIfDisposed();
            NativeMethods.SyncAudio_Stop(_handle);
        }

        public bool IsRunning
        {
            get
            {
                if (_handle == IntPtr.Zero || _disposed) return false;
                NativeMethods.SyncAudio_IsRunning(_handle, out int running);
                return running != 0;
            }
        }

        public void SetMasterVolume(float volume)
        {
            ThrowIfDisposed();
            NativeMethods.SyncAudio_SetMasterVolume(_handle, volume);
        }

        public void SetMuteAll(bool mute)
        {
            ThrowIfDisposed();
            NativeMethods.SyncAudio_SetMuteAll(_handle, mute ? 1 : 0);
        }

        public void PlayTestTone(TestSignalType type)
        {
            ThrowIfDisposed();
            NativeMethods.SyncAudio_PlayTestTone(_handle, (int)type);
        }

        public void StopTestTone()
        {
            ThrowIfDisposed();
            NativeMethods.SyncAudio_StopTestTone(_handle);
        }

        public void SetBaseBufferingDelayMs(int delayMs)
        {
            ThrowIfDisposed();
            NativeMethods.SyncAudio_SetBaseBufferingDelayMs(_handle, delayMs);
        }

        public int GetBaseBufferingDelayMs()
        {
            ThrowIfDisposed();
            int hr = NativeMethods.SyncAudio_GetBaseBufferingDelayMs(_handle, out int delay);
            return (hr == 0) ? delay : 100;
        }

        public MasterTelemetryNative GetMasterTelemetry()
        {
            ThrowIfDisposed();
            NativeMethods.SyncAudio_GetMasterTelemetry(_handle, out var telemetry);
            return telemetry;
        }

        public bool TryGetDeviceTelemetry(string deviceId, out DeviceTelemetryNative telemetry)
        {
            ThrowIfDisposed();
            return NativeMethods.SyncAudio_GetDeviceTelemetry(_handle, deviceId, out telemetry) == 0;
        }

        private void OnNativeEvent(string eventType, string message)
        {
            switch (eventType)
            {
                case "PlaybackStarted":
                    PlaybackStarted?.Invoke();
                    break;
                case "PlaybackStopped":
                    PlaybackStopped?.Invoke();
                    break;
                case "DeviceListChanged":
                    DeviceListChanged?.Invoke();
                    break;
            }
            EngineEvent?.Invoke(eventType, message);
        }

        private void ThrowIfDisposed()
        {
            if (_disposed || _handle == IntPtr.Zero)
            {
                throw new ObjectDisposedException(nameof(SyncAudioEngine));
            }
        }

        public void Dispose()
        {
            if (!_disposed)
            {
                _disposed = true;
                if (_handle != IntPtr.Zero)
                {
                    NativeMethods.SyncAudio_RegisterEventCallback(_handle, null);
                    NativeMethods.SyncAudio_DestroyEngine(_handle);
                    _handle = IntPtr.Zero;
                }
                _nativeCallbackHolder = null;
            }
        }
    }
}
