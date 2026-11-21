# SYNC Uninstaller for Windows

$ErrorActionPreference = "SilentlyContinue"

Write-Host "Uninstalling SYNC..." -ForegroundColor Yellow

# Kill running processes
Get-Process -Name "Sync.App" -ErrorAction SilentlyContinue | Stop-Process -Force

# Remove Shortcuts
$startMenuDir = Join-Path $env:APPDATA "Microsoft\Windows\Start Menu\Programs\SYNC"
Remove-Item -Path $startMenuDir -Recurse -Force

$desktopShortcut = Join-Path ([Environment]::GetFolderPath("Desktop")) "SYNC.lnk"
Remove-Item -Path $desktopShortcut -Force

# Remove Registry Key
Remove-Item -Path "HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\SYNC" -Recurse -Force
Remove-ItemProperty -Path "HKCU:\Software\Microsoft\Windows\CurrentVersion\Run" -Name "SYNC_Audio" -Force

# Remove Install Directory
$installDir = Join-Path $env:LOCALAPPDATA "Programs\SYNC"
Start-Sleep -Milliseconds 500
Remove-Item -Path $installDir -Recurse -Force

Write-Host "SYNC has been successfully uninstalled from your PC." -ForegroundColor Green
