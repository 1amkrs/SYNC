using System;
using System.ComponentModel;
using System.Windows;
using System.Windows.Controls;
using Button = System.Windows.Controls.Button;
using CheckBox = System.Windows.Controls.CheckBox;
using Sync.App.ViewModels;
using Sync.Core;

namespace Sync.App.Views
{
    public partial class MainWindow : Window
    {
        private readonly MainViewModel _vm;

        public MainWindow()
        {
            InitializeComponent();
            _vm = new MainViewModel(App.Audio, App.Settings);
            DataContext = _vm;
        }

        // Navigation
        private void NavDashboard_Click(object sender, RoutedEventArgs e)
        {
            TabDashboard.Visibility = Visibility.Visible;
            TabDevices.Visibility = Visibility.Collapsed;
            TabSettings.Visibility = Visibility.Collapsed;
            _vm.ActiveTab = "Dashboard";
        }

        private void NavDevices_Click(object sender, RoutedEventArgs e)
        {
            TabDashboard.Visibility = Visibility.Collapsed;
            TabDevices.Visibility = Visibility.Visible;
            TabSettings.Visibility = Visibility.Collapsed;
            _vm.ActiveTab = "Devices";
        }

        private void NavSettings_Click(object sender, RoutedEventArgs e)
        {
            TabDashboard.Visibility = Visibility.Collapsed;
            TabDevices.Visibility = Visibility.Collapsed;
            TabSettings.Visibility = Visibility.Visible;
            _vm.ActiveTab = "Settings";
        }

        // Playback Actions
        private void BtnTogglePlay_Click(object sender, RoutedEventArgs e)
        {
            _vm.TogglePlayback();
        }

        private void QuickMute_Click(object sender, RoutedEventArgs e)
        {
            _vm.ToggleMuteAll();
        }

        private void BtnTestSignal_Click(object sender, RoutedEventArgs e)
        {
            _vm.CycleTestTone();
        }

        // Stepper Buttons
        private void DelayMinus_Click(object sender, RoutedEventArgs e)
        {
            if (sender is Button btn && btn.Tag is AudioDeviceModel dev)
            {
                _vm.AdjustDeviceDelay(dev, -1);
            }
        }

        private void DelayPlus_Click(object sender, RoutedEventArgs e)
        {
            if (sender is Button btn && btn.Tag is AudioDeviceModel dev)
            {
                _vm.AdjustDeviceDelay(dev, +1);
            }
        }

        // Device Selection
        private void DeviceCheckbox_Click(object sender, RoutedEventArgs e)
        {
            if (sender is CheckBox chk && chk.DataContext is AudioDeviceModel dev)
            {
                _vm.ToggleDeviceSelection(dev);
            }
        }

        private void DeviceActionButton_Click(object sender, RoutedEventArgs e)
        {
            if (sender is Button btn && btn.Tag is AudioDeviceModel dev)
            {
                _vm.ToggleDeviceSelection(dev);
            }
        }

        private void BtnRefreshDevices_Click(object sender, RoutedEventArgs e)
        {
            _vm.RefreshDevices();
        }

        private void BtnPairBluetooth_Click(object sender, RoutedEventArgs e)
        {
            _vm.OpenBluetoothSettings();
        }

        // Groups
        private void BtnSaveGroup_Click(object sender, RoutedEventArgs e)
        {
            _vm.SaveCurrentGroup();
        }

        private void BtnLoadGroup_Click(object sender, RoutedEventArgs e)
        {
            if (sender is Button btn && btn.Tag is DeviceGroupModel group)
            {
                _vm.LoadGroup(group);
            }
        }

        // Settings
        private void Setting_Changed(object sender, RoutedEventArgs e)
        {
            if (App.Settings != null)
            {
                App.Settings.Save();
                if (sender == ChkStartup)
                {
                    StartupService.SetLaunchAtStartup(App.Settings.Current.LaunchAtStartup);
                }
            }
        }

        private void PresetLowLatency_Click(object sender, RoutedEventArgs e)
        {
            _vm.Audio.SetQualityPreset("Low Latency");
        }

        private void PresetBalanced_Click(object sender, RoutedEventArgs e)
        {
            _vm.Audio.SetQualityPreset("Balanced");
        }

        private void PresetHighStability_Click(object sender, RoutedEventArgs e)
        {
            _vm.Audio.SetQualityPreset("High Stability");
        }

        private void BtnResetCalibration_Click(object sender, RoutedEventArgs e)
        {
            _vm.ResetAllCalibration();
        }

        private void BtnExportReport_Click(object sender, RoutedEventArgs e)
        {
            _vm.ExportDiagnosticReport();
        }

        private void BtnOpenSoundSettings_Click(object sender, RoutedEventArgs e)
        {
            _vm.OpenSoundSettings();
        }

        // Window Closing -> Minimize to tray
        protected override void OnClosing(CancelEventArgs e)
        {
            if (App.Settings != null && App.Settings.Current.MinimizeToTray)
            {
                e.Cancel = true;
                Hide();
            }
            else
            {
                base.OnClosing(e);
            }
        }
    }
}
