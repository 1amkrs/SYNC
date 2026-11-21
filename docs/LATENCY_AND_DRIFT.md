# The Physics of Bluetooth Latency and Clock Drift

This document explains the real-world acoustic, hardware, and radio factors governing wireless audio synchronization, and how SYNC resolves them.

---

## 1. Deconstructing Audio Latency

A common misconception in computer audio is that the timestamp provided by the operating system driver represents the exact moment sound waves hit human ears. In reality, total acoustic latency is the sum of five distinct pipeline stages:

$$L_{\text{total}} = L_{\text{WASAPI}} + L_{\text{Codec}} + L_{\text{RF}} + L_{\text{DSP}} + L_{\text{Acoustic}}$$

1. **Host WASAPI Buffering ($L_{\text{WASAPI}}$: 5–20 ms)**: Time spent in the Windows Audio Engine mixing buffer.
2. **Codec Encoding ($L_{\text{Codec}}$: 20–40 ms)**: Encoding PCM frames into Bluetooth A2DP packets (SBC, AAC, or aptX).
3. **RF Transmission & Queuing ($L_{\text{RF}}$: 15–40 ms)**: Bluetooth Asynchronous Connectionless (ACL) packet scheduling, frequency hopping over 2.4 GHz ISM band, and retransmissions.
4. **Speaker Internal Hardware Buffer & DSP ($L_{\text{DSP}}$: 50–250 ms)**: Bluetooth speakers contain their own microcontrollers, anti-jitter jitter buffers (often 100–200 ms to avoid dropouts when wireless packets are retransmitted), EQ/bass DSPs, and DACs.
5. **Acoustic Air Propagation ($L_{\text{Acoustic}}$: ~3 ms per meter)**: Physical sound travel from speaker cone to listener.

> **Crucial Rule**: WASAPI device timestamps only capture $L_{\text{WASAPI}}$. They **cannot** reveal $L_{\text{DSP}}$ inside a remote wireless speaker. True acoustic synchronization therefore requires either empirical calibration (using SYNC's manual slider or microphone cross-correlation) or manual tuning.

---

## 2. The Clock Drift Dilemma (PPM Skew)

Every digital audio endpoint relies on a physical quartz crystal oscillator to clock its DAC. In an ideal world, all devices would tick at precisely 48,000.000000 Hz. In reality, crystal oscillators suffer from:
- Manufacturing tolerances: typically $\pm 20$ to $\pm 100$ Parts-Per-Million (PPM).
- Thermal variation: temperature rise during playback changes resonant frequency.
- Aging: slight drift over time.

### Quantitative Impact
At $+100$ PPM skew:
- The device renders $48,004.8$ samples every second instead of $48,000$.
- Over 1 minute: $288$ extra samples consumed.
- Over 15 minutes: $4,320$ extra samples consumed ($90$ ms of audio).
- Without compensation, the device's playback buffer will completely drain, leading to recurring underruns, clicks, and dropouts.

---

## 3. SYNC's Synchronization Engine

SYNC implements a dual-layer synchronization architecture:

### 3.1 Adaptive Buffer Watermark Tracking (PI Loop)
The `DriftEstimator` monitors the buffer fill level $F(t)$ relative to target watermark $F_{\text{target}}$:

$$e(t) = F(t) - F_{\text{target}}$$

To prevent chasing normal Windows thread scheduling jitter (1–3 ms), an initial deadband filter is applied:

$$e_{\text{eff}}(t) = \begin{cases} e(t) - \delta & \text{if } e(t) > \delta \\ e(t) + \delta & \text{if } e(t) < -\delta \\ 0 & \text{otherwise} \end{cases}$$

The filtered error is fed into a Proportional-Integral (PI) controller to calculate clock skew in PPM:

$$\text{Drift}_{\text{PPM}}(t) = K_p \cdot e_{\text{eff}}(t) + K_i \int e_{\text{eff}}(\tau) d\tau$$

### 3.2 Asynchronous Sample Rate Converter (ASRC)
The calculated drift PPM modifies the effective conversion ratio of the Hermite cubic resampler:

$$R_{\text{effective}} = \frac{f_{\text{in}}}{f_{\text{out}}} \times \left(1.0 + \frac{\text{Drift}_{\text{PPM}}}{10^6}\right)$$

A slew-rate limiter limits ratio acceleration to $\le 0.5$ PPM per frame, completely eliminating audible pitch warbling, clicks, or phase smearing.

---

## 4. Negative Delay Offset Resolution

When Speaker A has 250 ms internal latency and Speaker B has 50 ms latency:
- User sets offset for Speaker A to $-200$ ms.
- Because an audio engine cannot play audio before it was captured (negative time paradox), SYNC finds the global minimum offset:
  $$\Delta_{\text{min}} = \min_i(O_i) = -200\text{ ms}$$
- All devices $j$ receive a dynamic compensation delay:
  $$D_j = D_{\text{base}} + (O_j - \Delta_{\text{min}})$$
- For Speaker A: $D_A = 100 + (-200 - (-200)) = 100\text{ ms}$.
- For Speaker B: $D_B = 100 + (0 - (-200)) = 300\text{ ms}$.
- Result: Speaker B's audio is delayed by 200 ms in software, so both speakers emit sound simultaneously into the room!
