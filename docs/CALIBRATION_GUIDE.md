# SYNC Acoustic Calibration Guide

Synchronizing wireless speakers in the physical world requires accounting for internal speaker latency and acoustic propagation. This guide covers how to achieve sub-millisecond perceptual alignment using SYNC.

---

## Method 1: Manual Calibration with Test Tones (Recommended)

Human hearing is exceptionally sensitive to transient time differences (the precedence or *Haas effect*):
- Delays $> 30$ ms are perceived as distinct echoes.
- Delays between $5$ and $30$ ms produce hollow *comb filtering* or phase flanging.
- Delays $< 2$ ms merge into a single solid acoustic image.

### Step-by-Step Calibration Procedure

1. **Connect and Select Speakers**: Ensure your Bluetooth devices are connected and checked in SYNC's **Device Manager**.
2. **Start Synchronization**: Click **Start Synchronization** on the Dashboard.
3. **Activate Calibration Tone**: Click **Play Calibration Tone** once. The button will toggle to:
   ```
   Tone: 1ms Click (Playing)
   ```
4. **Listen to the Transient**:
   - If the speakers are out of sync, you will hear a rapid double-click (`cl-click ... cl-click`).
5. **Adjust Delay Sliders**:
   - Locate the device card for the secondary speaker.
   - Move the **Sync Delay Offset** slider (or click the **-1** / **+1** buttons for 1ms fine adjustment).
   - If the second click moves further away, move the slider in the opposite direction.
   - When the two clicks coalesce into a single sharp, punchy snap (`click ... click`), the two speakers are perfectly synchronized!
6. **Stop Tone**: Click the tone button until it returns to `Play Calibration Tone`.
7. **Save Profile**: Enter a group name (e.g. *"Living Room Pair"*) and click **Save Group**.

---

## Method 2: Microphone-Assisted Automatic Calibration

SYNC includes an automatic acoustic cross-correlation analyzer for microphone-guided calibration.

### How It Works
1. A reference 50ms logarithmic chirp ($200\text{ Hz} \to 8000\text{ Hz}$) is generated using a Hann window.
2. The chirp is played through Speaker A, and recorded by the default microphone.
3. The chirp is then played through Speaker B, and recorded by the same microphone.
4. SYNC computes the normalized cross-correlation:
   $$R_{xy}[n] = \frac{\sum_k x[k] y[k + n]}{\sqrt{\sum_k x^2[k] \sum_k y^2[k + n]}}$$
5. The arrival peak time difference $\Delta t = \frac{n_B^* - n_A^*}{f_s}$ reveals the exact physical delay difference between the two units down to the sample level.

### Best Practices for Measurement Accuracy
- Place the microphone approximately equidistant from both speakers.
- Minimize background noise during the measurement chirp.
- Avoid placing speakers directly against reflective glass walls or resonant metal surfaces during measurement.
