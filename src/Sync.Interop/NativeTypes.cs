using System;
using System.Runtime.InteropServices;

namespace Sync.Interop
{
    public enum DeviceTransportType
    {
        Unknown = 0,
        InternalSpeakers = 1,
        Headphones = 2,
        Bluetooth = 3,
        Usb = 4,
        Hdmi = 5
    }

    public enum TestSignalType
    {
        None = 0,
        PeriodicClick = 1,
        LogarithmicChirp = 2,
        DualPulse = 3,
        SineTone = 4
    }

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode, Pack = 8)]
    public struct DeviceInfoNative
    {
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 128)]
        public string Id;

        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 128)]
        public string Name;

        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 128)]
        public string AdapterName;

        public int TransportType;
        public int IsDefault;
        public int IsBluetooth;
        public uint SampleRate;
        public uint Channels;
    }

    [StructLayout(LayoutKind.Sequential, Pack = 8)]
    public struct MasterTelemetryNative
    {
        public uint ActiveOutputs;
        public uint CaptureSampleRate;
        public uint CaptureChannels;
        public float AudioLevelPeakL;
        public float AudioLevelPeakR;
        public ulong TotalCapturedFrames;
        public ulong TotalRenderedFrames;
        public int IsRunning;
    }

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode, Pack = 8)]
    public struct DeviceTelemetryNative
    {
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 128)]
        public string DeviceId;

        public uint BufferOccupancyPercent;
        public uint UnderrunCount;
        public int EstimatedDriftPpm;
        public int CalibratedDelayMs;
        public float MeasuredLatencyMs;
        public float Volume;
        public int IsEnabled;
        public int IsConnected;
    }

    [UnmanagedFunctionPointer(CallingConvention.StdCall, CharSet = CharSet.Unicode)]
    public delegate void EngineEventCallbackNative(string eventType, string message);

    internal static class NativeMethods
    {
        private const string DllName = "SyncAudio.dll";

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int SyncAudio_CreateEngine(out IntPtr outEngineHandle);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int SyncAudio_DestroyEngine(IntPtr engineHandle);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int SyncAudio_EnumerateDevices(IntPtr engineHandle, out IntPtr outDevices, out uint outCount);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void SyncAudio_FreeDeviceList(IntPtr devices);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Unicode)]
        public static extern int SyncAudio_SetCaptureDevice(IntPtr engineHandle, string deviceId);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Unicode)]
        public static extern int SyncAudio_GetCaptureDevice(IntPtr engineHandle, [Out] char[] outDeviceId, uint maxLen);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Unicode)]
        public static extern int SyncAudio_AddOutputDevice(IntPtr engineHandle, string deviceId);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Unicode)]
        public static extern int SyncAudio_RemoveOutputDevice(IntPtr engineHandle, string deviceId);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int SyncAudio_ClearOutputDevices(IntPtr engineHandle);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Unicode)]
        public static extern int SyncAudio_SetOutputVolume(IntPtr engineHandle, string deviceId, float volume);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Unicode)]
        public static extern int SyncAudio_GetOutputVolume(IntPtr engineHandle, string deviceId, out float outVolume);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Unicode)]
        public static extern int SyncAudio_SetOutputDelayMs(IntPtr engineHandle, string deviceId, int delayMs);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Unicode)]
        public static extern int SyncAudio_GetOutputDelayMs(IntPtr engineHandle, string deviceId, out int outDelayMs);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Unicode)]
        public static extern int SyncAudio_SetOutputEnabled(IntPtr engineHandle, string deviceId, int enabled);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Unicode)]
        public static extern int SyncAudio_IsOutputEnabled(IntPtr engineHandle, string deviceId, out int outEnabled);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int SyncAudio_Start(IntPtr engineHandle);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int SyncAudio_Stop(IntPtr engineHandle);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int SyncAudio_IsRunning(IntPtr engineHandle, out int outIsRunning);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int SyncAudio_SetMasterVolume(IntPtr engineHandle, float volume);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int SyncAudio_SetMuteAll(IntPtr engineHandle, int mute);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int SyncAudio_PlayTestTone(IntPtr engineHandle, int toneType);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int SyncAudio_StopTestTone(IntPtr engineHandle);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int SyncAudio_SetBaseBufferingDelayMs(IntPtr engineHandle, int delayMs);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int SyncAudio_GetBaseBufferingDelayMs(IntPtr engineHandle, out int outDelayMs);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int SyncAudio_GetMasterTelemetry(IntPtr engineHandle, out MasterTelemetryNative outTelemetry);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Unicode)]
        public static extern int SyncAudio_GetDeviceTelemetry(IntPtr engineHandle, string deviceId, out DeviceTelemetryNative outTelemetry);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int SyncAudio_RegisterEventCallback(IntPtr engineHandle, EngineEventCallbackNative? callback);
    }
}
