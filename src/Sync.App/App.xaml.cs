using System;
using System.Windows;
using Sync.App.Services;
using Sync.Core;

namespace Sync.App
{
    public partial class App : System.Windows.Application
    {
        private SystemTrayService? _trayService;
        public static SettingsService Settings { get; private set; } = null!;
        public static AudioService Audio { get; private set; } = null!;

        protected override void OnStartup(StartupEventArgs e)
        {
            base.OnStartup(e);

            // Initialize services
            Settings = new SettingsService();
            Settings.Load();

            Audio = new AudioService(Settings);

            // Initialize System Tray Service
            _trayService = new SystemTrayService(Audio, ShowMainWindow, ShutdownApp);

            // Auto-start playback if configured
            if (Settings.Current.AutoStartPlayback && Audio.SelectedOutputs.Count > 0)
            {
                Audio.Start();
            }
        }

        public void ShowMainWindow()
        {
            if (MainWindow != null)
            {
                if (MainWindow.WindowState == WindowState.Minimized)
                {
                    MainWindow.WindowState = WindowState.Normal;
                }
                MainWindow.Show();
                MainWindow.Activate();
            }
        }

        public void ShutdownApp()
        {
            Audio.Stop();
            _trayService?.Dispose();
            _trayService = null;
            Audio.Dispose();
            Shutdown();
        }

        protected override void OnExit(ExitEventArgs e)
        {
            _trayService?.Dispose();
            Audio?.Dispose();
            base.OnExit(e);
        }
    }
}
