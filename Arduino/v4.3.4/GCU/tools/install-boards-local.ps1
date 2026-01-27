param(
  [string]$ArduinoDataDir = (Join-Path $env:LOCALAPPDATA "Arduino15"),
  [string]$Source = (Join-Path $PSScriptRoot "boards.local.txt"),
  [bool]$Pause = $true
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path $Source)) {
  Write-Error "Source file not found: $Source"
}

$packagesRoot = Join-Path $ArduinoDataDir "packages\\esp32\\hardware\\esp32"
if (-not (Test-Path $packagesRoot)) {
  Write-Error "ESP32 core not found under: $packagesRoot"
}

$versions = Get-ChildItem -Path $packagesRoot -Directory | Where-Object {
  Test-Path (Join-Path $_.FullName "boards.txt")
}

if (-not $versions -or $versions.Count -eq 0) {
  Write-Error "No ESP32 core versions with boards.txt were found under: $packagesRoot"
}

foreach ($versionDir in $versions) {
  $target = Join-Path $versionDir.FullName "boards.local.txt"
  Copy-Item -Force -Path $Source -Destination $target
  Write-Host "Installed boards.local.txt -> $target"
}
