$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$projectRoot = Split-Path -Parent $scriptDir
$portableDir = Join-Path $projectRoot "bin\portable"
$distDir = Join-Path $projectRoot "bin\dist"
$zipPath = Join-Path $distDir "SYNC-v1.0.0-win-x64-portable.zip"

Write-Host "Creating distribution directory: $distDir" -ForegroundColor Cyan
New-Item -ItemType Directory -Path $distDir -Force | Out-Null

if (-not (Test-Path $portableDir)) {
    Write-Host "Publishing project..." -ForegroundColor Yellow
    & dotnet publish (Join-Path $projectRoot "src\Sync.App\Sync.App.csproj") -c Release -r win-x64 --self-contained false -o $portableDir
}

# Copy LICENSE and README into portable folder
Copy-Item (Join-Path $projectRoot "LICENSE") -Destination $portableDir -Force
Copy-Item (Join-Path $projectRoot "README.md") -Destination $portableDir -Force

Write-Host "Compressing to $zipPath ..." -ForegroundColor Cyan
if (Test-Path $zipPath) { Remove-Item $zipPath -Force }
Compress-Archive -Path "$portableDir\*" -DestinationPath $zipPath

Write-Host "Successfully generated portable archive: $zipPath" -ForegroundColor Green
Get-Item $zipPath | Select-Object Name, Length
