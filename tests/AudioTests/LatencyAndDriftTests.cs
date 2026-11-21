using System;
using System.Collections.Generic;
using Sync.Core;
using Sync.Interop;
using Xunit;

namespace AudioTests
{
    public class LatencyAndDriftTests
    {
        [Fact]
        public void SettingsService_SavesAndLoadsCalibratedOffsets()
        {
            var settings = new SettingsService();
            settings.Load();

            string testDeviceId = "{TEST-DEVICE-GUID-1234}";
            settings.SaveDeviceOffset(testDeviceId, -45);
            settings.SaveDeviceLabel(testDeviceId, "Living Room Soundbar");

            int offset = settings.GetSavedDeviceOffset(testDeviceId);
            string label = settings.GetCustomDeviceLabel(testDeviceId);

            Assert.Equal(-45, offset);
            Assert.Equal("Living Room Soundbar", label);
        }

        [Fact]
        public void QualityPresets_ApplyCorrectBufferSizes()
        {
            using var engine = new SyncAudioEngine();

            // Low Latency preset -> 50ms
            engine.SetBaseBufferingDelayMs(50);
            Assert.Equal(50, engine.GetBaseBufferingDelayMs());

            // Balanced preset -> 100ms
            engine.SetBaseBufferingDelayMs(100);
            Assert.Equal(100, engine.GetBaseBufferingDelayMs());

            // High Stability preset -> 200ms
            engine.SetBaseBufferingDelayMs(200);
            Assert.Equal(200, engine.GetBaseBufferingDelayMs());
        }
    }

    public class SimulatedAudioDeviceHarness
    {
        public class SimulatedEndpoint
        {
            public string Name { get; set; }
            public double ClockDriftPpm { get; set; } // e.g. +100 PPM means clock runs 0.01% faster
            public double LatencyMs { get; set; }
            public long TotalFramesConsumed { get; set; }

            public SimulatedEndpoint(string name, double driftPpm, double latencyMs)
            {
                Name = name;
                ClockDriftPpm = driftPpm;
                LatencyMs = latencyMs;
            }

            public long ConsumeFrames(long elapsedMs, uint sampleRate)
            {
                double effectiveRatio = 1.0 + (ClockDriftPpm / 1000000.0);
                long frames = (long)((elapsedMs * sampleRate * effectiveRatio) / 1000.0);
                TotalFramesConsumed += frames;
                return frames;
            }
        }

        [Fact]
        public void MultiDevice_ClockDriftSimulation_CalculatesPhaseDriftAccurately()
        {
            // Simulate 2 Bluetooth speakers with independent crystal clocks:
            // Speaker A: 48,000 Hz nominal, drifts +75 PPM (fast clock)
            // Speaker B: 48,000 Hz nominal, drifts -60 PPM (slow clock)
            var speakerA = new SimulatedEndpoint("Bluetooth Speaker A", +75.0, 150.0);
            var speakerB = new SimulatedEndpoint("Bluetooth Speaker B", -60.0, 180.0);

            uint sampleRate = 48000;
            long testDurationMs = 60000; // 1 minute simulation

            long framesA = speakerA.ConsumeFrames(testDurationMs, sampleRate);
            long framesB = speakerB.ConsumeFrames(testDurationMs, sampleRate);

            long frameDifference = framesA - framesB;
            double driftSeconds = (double)frameDifference / sampleRate;

            // At (75 - (-60)) = 135 PPM difference over 60 seconds:
            // Expected drift = 60s * 135e-6 = 0.0081 seconds = 8.1 ms = ~388 frames.
            Assert.True(frameDifference > 300 && frameDifference < 450);
            Assert.True(driftSeconds > 0.006 && driftSeconds < 0.010);

            // This demonstrates why real-time adaptive resampling (ASRC) is critical
            // to prevent buffer underrun/overflow on uncorrected Bluetooth hardware.
        }
    }
}
