# SYNC Installer for Windows 10 / 11
# Installs SYNC to %LocalAppData%\Programs\SYNC and creates Start Menu and Desktop shortcuts.

$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$projectRoot = Split-Path -Parent $scriptDir
$sourceDir = Join-Path $projectRoot "bin\portable"

if (-not (Test-Path (Join-Path $sourceDir "Sync.App.exe"))) {
    Write-Host "Source binaries not found. Running build..." -ForegroundColor Yellow
    & dotnet publish (Join-Path $projectRoot "src\Sync.App\Sync.App.csproj") -c Release -r win-x64 --self-contained false -o $sourceDir
}

$installDir = Join-Path $env:LOCALAPPDATA "Programs\SYNC"
Write-Host "Installing SYNC to: $installDir" -ForegroundColor Cyan
New-Item -ItemType Directory -Path $installDir -Force | Out-Null

Copy-Item -Path "$sourceDir\*" -Destination $installDir -Recurse -Force

# Create Start Menu Shortcut
$startMenuDir = Join-Path $env:APPDATA "Microsoft\Windows\Start Menu\Programs\SYNC"
New-Item -ItemType Directory -Path $startMenuDir -Force | Out-Null

$wshShell = New-Object -ComObject WScript.Shell

$shortcutPath = Join-Path $startMenuDir "SYNC.lnk"
$shortcut = $wshShell.CreateShortcut($shortcutPath)
$shortcut.TargetPath = Join-Path $installDir "Sync.App.exe"
$shortcut.WorkingDirectory = $installDir
$shortcut.Description = "SYNC — Multi-Device Bluetooth Audio for Windows"
$iconPath = Join-Path $installDir "assets\icon.ico"
if (Test-Path $iconPath) {
    $shortcut.IconLocation = "$iconPath, 0"
}
$shortcut.Save()

# Create Desktop Shortcut
$desktopShortcutPath = Join-Path ([Environment]::GetFolderPath("Desktop")) "SYNC.lnk"
$desktopShortcut = $wshShell.CreateShortcut($desktopShortcutPath)
$desktopShortcut.TargetPath = Join-Path $installDir "Sync.App.exe"
$desktopShortcut.WorkingDirectory = $installDir
$desktopShortcut.Description = "SYNC — Multi-Device Bluetooth Audio for Windows"
if (Test-Path $iconPath) {
    $desktopShortcut.IconLocation = "$iconPath, 0"
}
$desktopShortcut.Save()

# Register in Windows Add/Remove Programs (Registry)
$regPath = "HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\SYNC"
New-Item -Path $regPath -Force | Out-Null
Set-ItemProperty -Path $regPath -Name "DisplayName" -Value "SYNC Multi-Device Audio"
Set-ItemProperty -Path $regPath -Name "DisplayVersion" -Value "1.0.0"
Set-ItemProperty -Path $regPath -Name "Publisher" -Value "SYNC Open Source Project"
Set-ItemProperty -Path $regPath -Name "DisplayIcon" -Value (Join-Path $installDir "Sync.App.exe")
Set-ItemProperty -Path $regPath -Name "InstallLocation" -Value $installDir
Set-ItemProperty -Path $regPath -Name "UninstallString" -Value "powershell.exe -ExecutionPolicy Bypass -File `"$installDir\Uninstall-SYNC.ps1`""

# Copy Uninstaller to install directory
Copy-Item (Join-Path $scriptDir "Uninstall-SYNC.ps1") -Destination $installDir -Force

Write-Host "Installation completed successfully!" -ForegroundColor Green
Write-Host "You can launch SYNC from your Start Menu or Desktop." -ForegroundColor Green
