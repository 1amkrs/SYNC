# SYNC — Multi-Device Bluetooth Audio for Windows

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/Platform-Windows%2010%20%7C%2011%20(x64)-0078D6.svg)](https://microsoft.com/windows)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-00599C.svg)](https://en.cppreference.com/w/cpp/20)
[![.NET](https://img.shields.io/badge/.NET-8.0%20%2F%2011.0-512BD4.svg)](https://dotnet.microsoft.com/)

**SYNC** is a high-performance, native Windows 10/11 application that enables simultaneous playback of Windows system audio across multiple Bluetooth audio devices (speakers, soundbars, earbuds, headphones) as well as wired, USB, and HDMI endpoints, with real-time drift correction and millisecond-accurate delay synchronization.

Think of **Samsung Dual Audio**, but natively built for Windows PCs, supporting more than two simultaneous outputs with zero hardware brand lock-in.

---

## Key Features

- **True Multi-Device Playback**: Play system audio (YouTube, Spotify, games, browser, DAW) simultaneously across 2, 3, or more Bluetooth speakers or mixed audio endpoints.
- **Low-Latency Loopback Capture**: Event-driven WASAPI Loopback captures audio without virtual driver overhead, supporting 44.1 kHz, 48 kHz, and 96 kHz stereo streams.
- **Clock Drift Compensation (ASRC)**: Independent hardware crystal oscillators in Bluetooth speakers drift by 20–200 PPM. SYNC's Asynchronous Sample Rate Converter continuously adapts the resample ratio via a Proportional-Integral (PI) feedback loop, eliminating buffer overflows, underruns, and pitch artifacts.
- **Non-Destructive Delay Calibration**: Fine per-device delay tuning (-500 ms to +500 ms in 1 ms steps). When advancing a high-latency speaker via negative relative offset, SYNC dynamically schedules a common buffer delay for other endpoints to maintain phase alignment without negative time travel.
- **Feedback Loop Protection**: Intelligently prevents circular loopback routing by validating capture sources and render outputs.
- **Built-in Calibration Signals**: Generate periodic clicks, logarithmic sweeps/chirps, dual-pulse markers, and reference tones for acoustic alignment.
- **Modern Windows 11 Fluent Interface**: Clean dark mode dashboard, live stereo peak VU meters, buffer health monitors, real engine telemetry, and device profiles.
- **Background Tray Operation**: Seamlessly runs in the Windows system tray with quick-toggle playback and volume controls.

---

## Architecture Overview

```
SYNC/
├── src/
│   ├── Sync.Audio/       # C++20 Core Audio Engine (WASAPI, Ring Buffers, ASRC, Drift Estimator)
│   ├── Sync.Interop/     # C# P/Invoke Bindings & Safe Handles
│   ├── Sync.Core/        # .NET Audio Services, Telemetry, Profiles & Settings Persistence
│   └── Sync.App/         # Modern Fluent UI Application & System Tray Integration
├── tests/
│   └── AudioTests/       # xUnit Automated Tests (Ring Buffer, Drift Harness, P/Invoke)
├── assets/               # High-res App Icons & Graphical Assets
├── docs/                 # Engineering Specifications & Physics Whitepapers
├── installer/            # Portable Zip Packager & Windows Installer Scripts
└── SYNC.sln              # Visual Studio Unified Solution
```

### Signal Flow Pipeline

```
[Windows Applications] (Browser, Media Player, Games)
           │
           ▼
[WASAPI Mix Buffer]
           │
           ▼
[WasapiLoopbackCapture] (MMCSS "Pro Audio" Thread, QPC 100ns Timestamps)
           │
           ├─── Master VU Meter & Telemetry
           │
           ▼
   [Distribution Bus]
           │
    ┌──────┴──────────────────────────┬──────────────────────────┐
    ▼                                 ▼                          ▼
[Output 1: Bluetooth Spk A]   [Output 2: Bluetooth Spk B]   [Output 3: USB/HDMI]
  │                             │                             │
  ├─ SPSC Lock-free Ring Buffer ├─ SPSC Lock-free Ring Buffer ├─ SPSC Lock-free Ring Buffer
  ├─ PI Drift Estimator (PPM)   ├─ PI Drift Estimator (PPM)   ├─ PI Drift Estimator (PPM)
  ├─ Hermite Cubic Resampler    ├─ Hermite Cubic Resampler    ├─ Hermite Cubic Resampler
  ├─ Latency Delay Offset (ms)  ├─ Latency Delay Offset (ms)  ├─ Latency Delay Offset (ms)
  ▼                             ▼                             ▼
[WASAPI Render Client A]      [WASAPI Render Client B]      [WASAPI Render Client C]
```

---

## Prerequisites & Building

### Requirements
- Windows 10 (Build 19041+) or Windows 11 (64-bit).
- Visual Studio 2022 / 2026 with C++ Desktop Development (MSVC `v143` or `v145`).
- .NET 8.0 SDK or newer.

### Build via Command Line
```powershell
# 1. Clone repository
git clone https://github.com/sync-audio/sync.git
cd sync

# 2. Build complete solution with MSBuild
& "C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe" SYNC.sln /p:Configuration=Release /p:Platform=x64

# 3. Run automated tests
dotnet test tests/AudioTests/AudioTests.csproj -c Release --no-build

# 4. Generate portable distribution package
.\installer\build-portable-zip.ps1
```

---

## Quick Start Guide

1. **Pair your Bluetooth Speakers**: Open Windows Settings (`Win + I` -> **Bluetooth & devices**) and pair all Bluetooth speakers or headphones you wish to use.
2. **Launch SYNC**: Start `Sync.App.exe`.
3. **Select Outputs**: Navigate to **Device Manager** and check the boxes next to the speakers you want to group.
4. **Start Synchronization**: Click the blue **Start Synchronization** button on the Dashboard.
5. **Calibrate Delays**:
   - Click **Play Calibration Tone** to emit a periodic 1ms click or chirp.
   - Adjust the **Delay Offset** sliders on each device until the clicks from all speakers merge into a single crisp transient.
   - Click **Tone** again to stop the test signal and enjoy perfectly synchronized music or video audio.

---

## License

This project is licensed under the **GNU General Public License v3.0 (GPL-3.0)**. See [LICENSE](LICENSE) for details.
