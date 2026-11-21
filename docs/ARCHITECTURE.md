# SYNC Architecture Specification

This document provides a comprehensive technical overview of the **SYNC** audio engine, synchronization pipelines, threading models, and interop boundaries.

---

## 1. Core Audio Pipeline

### 1.1 WASAPI Loopback Capture
Standard Windows audio APIs provide playback and recording separately. Windows Audio Session API (WASAPI) exposes a loopback mode (`AUDCLNT_STREAMFLAGS_LOOPBACK`), allowing an application to tap into the render stream of an audio output endpoint after all desktop applications have been mixed by the Windows Audio Engine (`audiodg.exe`).

#### Loopback Mechanics & Endpoint Isolation
- **The Duplication Phenomenon**: Capturing from the Windows default output device does not stop Windows from sending that mixed buffer to the physical hardware of that endpoint (e.g. laptop internal speakers).
- **SYNC Mitigation Strategies**:
  1. *Volume Attenuation*: WASAPI loopback captures the pre-attenuated digital stream. Muting the physical endpoint in Windows master volume allows capturing audio while internal speakers remain silent.
  2. *Designated Secondary / Virtual Endpoint*: If an auxiliary or virtual audio device (such as an unused HDMI monitor or virtual cable) is designated as Windows Default, SYNC captures from that silent endpoint and redirects the sound cleanly to Bluetooth speakers.
  3. *Circular Feedback Protection*: If the user attempts to add the currently active capture endpoint as a SYNC playback output, `SyncEngine` immediately rejects the operation with an explicit error to prevent infinite audio recursion.

### 1.2 Multi-Output WASAPI Rendering
Each selected output device (whether Bluetooth A2DP, USB DAC, or 3.5mm headphone jack) is opened as an independent WASAPI render client in shared mode:
- Initialized with `AUDCLNT_STREAMFLAGS_EVENTCALLBACK`.
- Bound to an OS event handle signaled by the Windows audio engine whenever a hardware buffer period expires (typically 10ms).
- Handled by a dedicated thread elevated to MMCSS priority (`AvSetMmThreadCharacteristicsW(L"Pro Audio")`).

---

## 2. Memory & Real-Time Thread Safety

Real-time audio threads must never block on locks, allocate heap memory, or perform disk/network I/O. Any blocking can cause audio buffer underruns (audible clicks, pops, or stutter).

### 2.1 Lock-Free SPSC Audio Ring Buffer
SYNC implements a wait-free Single-Producer Single-Consumer (`AudioRingBuffer`):
- Memory is allocated once during endpoint initialization (capacity = 2 seconds of float stereo audio).
- Read and write pointers are stored as `std::atomic<size_t>` aligned to 64-byte cache boundaries (`alignas(64)`) to eliminate CPU false sharing.
- Index wrapping uses bitwise masking (`& (capacity - 1)`) via power-of-two buffer sizes.

---

## 3. Native Interoperability (C ABI)

The C++ audio engine is compiled into a native dynamic library (`SyncAudio.dll`) exposing a clean C ABI with `extern "C" __declspec(dllexport)`.

The C# UI and service layers interact with the engine via .NET P/Invoke (`DllImport`):
- All strings are marshaled as `LPWStr` (`wchar_t*`).
- Structs use explicit 8-byte packing (`#pragma pack(push, 8)` and `Pack = 8`) to guarantee bit-identical binary layouts across C++ and C# runtimes.
- Callbacks use `__stdcall` calling convention with persistent delegate GC roots in `SyncAudioEngine.cs` to prevent garbage collection while registered.
