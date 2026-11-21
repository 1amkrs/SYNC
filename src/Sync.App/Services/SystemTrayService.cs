using System;
using System.Drawing;
using System.IO;
using System.Windows;
using Forms = System.Windows.Forms;
using Sync.Core;

namespace Sync.App.Services
{
    public class SystemTrayService : IDisposable
    {
        private Forms.NotifyIcon? _notifyIcon;
        private readonly AudioService _audio;
        private readonly Action _showMainWindowAction;
        private readonly Action _shutdownAppAction;

        public SystemTrayService(AudioService audio, Action showMainWindowAction, Action shutdownAppAction)
        {
            _audio = audio;
            _showMainWindowAction = showMainWindowAction;
            _shutdownAppAction = shutdownAppAction;

            Initialize();
        }

        private void Initialize()
        {
            try
            {
                _notifyIcon = new Forms.NotifyIcon();

                string iconPath = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "assets", "icon.ico");
                if (!File.Exists(iconPath))
                {
                    iconPath = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "..", "..", "..", "..", "assets", "icon.ico");
                }

                if (File.Exists(iconPath))
                {
                    _notifyIcon.Icon = new Icon(iconPath);
                }
                else
                {
                    _notifyIcon.Icon = SystemIcons.Application;
                }

                _notifyIcon.Text = "SYNC — Multi-Device Audio";
                _notifyIcon.Visible = true;

                var contextMenu = new Forms.ContextMenuStrip();

                var showItem = new Forms.ToolStripMenuItem("Open Dashboard", null, (s, e) =>
                {
                    _showMainWindowAction();
                });

                var startStopItem = new Forms.ToolStripMenuItem("Start Synchronization", null, (s, e) =>
                {
                    if (_audio.IsRunning) _audio.Stop();
                    else _audio.Start();
                    UpdateMenuText(contextMenu);
                });

                var muteItem = new Forms.ToolStripMenuItem("Mute All", null, (s, e) =>
                {
                    _audio.IsMutedAll = !_audio.IsMutedAll;
                    UpdateMenuText(contextMenu);
                });

                var exitItem = new Forms.ToolStripMenuItem("Exit", null, (s, e) =>
                {
                    _shutdownAppAction();
                });

                contextMenu.Items.Add(showItem);
                contextMenu.Items.Add(new Forms.ToolStripSeparator());
                contextMenu.Items.Add(startStopItem);
                contextMenu.Items.Add(muteItem);
                contextMenu.Items.Add(new Forms.ToolStripSeparator());
                contextMenu.Items.Add(exitItem);

                _notifyIcon.ContextMenuStrip = contextMenu;
                _notifyIcon.DoubleClick += (s, e) => _showMainWindowAction();

                _audio.StateChanged += () =>
                {
                    UpdateMenuText(contextMenu);
                };
            }
            catch (Exception ex)
            {
                System.Diagnostics.Debug.WriteLine($"Failed to initialize system tray: {ex.Message}");
            }
        }

        private void UpdateMenuText(Forms.ContextMenuStrip menu)
        {
            if (menu.Items.Count >= 4)
            {
                menu.Items[2].Text = _audio.IsRunning ? "Stop Synchronization" : "Start Synchronization";
                menu.Items[3].Text = _audio.IsMutedAll ? "Unmute All" : "Mute All";
            }
        }

        public void Dispose()
        {
            if (_notifyIcon != null)
            {
                _notifyIcon.Visible = false;
                _notifyIcon.Dispose();
                _notifyIcon = null;
            }
        }
    }
}
