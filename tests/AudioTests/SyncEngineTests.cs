using System;
using System.IO;
using System.Linq;
using Sync.Core;
using Sync.Interop;
using Xunit;

namespace AudioTests
{
    public class SyncEngineTests
    {
        [Fact]
        public void Engine_CanBeCreatedAndDestroyedCleanly()
        {
            using var engine = new SyncAudioEngine();
            Assert.False(engine.IsRunning);
        }

        [Fact]
        public void Engine_CanEnumerateAudioEndpoints()
        {
            using var engine = new SyncAudioEngine();
            var devices = engine.EnumerateDevices();

            Assert.NotNull(devices);
            // On a real machine, we expect at least 1 audio device
            Assert.NotEmpty(devices);

            foreach (var dev in devices)
            {
                Assert.False(string.IsNullOrWhiteSpace(dev.Id));
                Assert.False(string.IsNullOrWhiteSpace(dev.Name));
            }
        }

        [Fact]
        public void Engine_FeedbackPrevention_RejectsCaptureDeviceAsOutput()
        {
            using var engine = new SyncAudioEngine();
            var devices = engine.EnumerateDevices();
            Assert.NotEmpty(devices);

            var firstDev = devices[0];
            bool captureSet = engine.SetCaptureDevice(firstDev.Id);
            Assert.True(captureSet);

            // Attempting to add the capture device as an output endpoint must be rejected!
            bool outputAdded = engine.AddOutputDevice(firstDev.Id);
            Assert.False(outputAdded);
        }

        [Fact]
        public void Engine_DelayOffset_ClampsWithinPermittedRange()
        {
            using var engine = new SyncAudioEngine();
            var devices = engine.EnumerateDevices();
            if (devices.Count < 2) return;

            var targetDev = devices.FirstOrDefault(d => d.Id != engine.GetCaptureDevice());
            if (targetDev.Id == null) return;

            bool added = engine.AddOutputDevice(targetDev.Id);
            Assert.True(added);

            // Set delay within normal range
            engine.SetOutputDelayMs(targetDev.Id, 125);
            Assert.Equal(125, engine.GetOutputDelayMs(targetDev.Id));

            // Set negative delay
            engine.SetOutputDelayMs(targetDev.Id, -80);
            Assert.Equal(-80, engine.GetOutputDelayMs(targetDev.Id));

            // Set delay beyond max positive (should clamp to 500)
            engine.SetOutputDelayMs(targetDev.Id, 9999);
            Assert.Equal(500, engine.GetOutputDelayMs(targetDev.Id));

            // Set delay beyond min negative (should clamp to -500)
            engine.SetOutputDelayMs(targetDev.Id, -9999);
            Assert.Equal(-500, engine.GetOutputDelayMs(targetDev.Id));

            engine.RemoveOutputDevice(targetDev.Id);
        }

        [Fact]
        public void Engine_VolumeAndMute_OperateCorrectly()
        {
            using var engine = new SyncAudioEngine();
            var devices = engine.EnumerateDevices();
            if (devices.Count < 2) return;

            var targetDev = devices.FirstOrDefault(d => d.Id != engine.GetCaptureDevice());
            if (targetDev.Id == null) return;

            engine.AddOutputDevice(targetDev.Id);

            engine.SetOutputVolume(targetDev.Id, 0.75f);
            Assert.Equal(0.75f, engine.GetOutputVolume(targetDev.Id), 2);

            engine.SetMasterVolume(0.5f);
            engine.SetMuteAll(true);
            engine.SetMuteAll(false);

            engine.RemoveOutputDevice(targetDev.Id);
        }

        [Fact]
        public void Engine_CalibrationTestTones_CanBeTriggered()
        {
            using var engine = new SyncAudioEngine();
            engine.PlayTestTone(TestSignalType.PeriodicClick);
            engine.PlayTestTone(TestSignalType.LogarithmicChirp);
            engine.PlayTestTone(TestSignalType.DualPulse);
            engine.StopTestTone();
        }
    }
}
