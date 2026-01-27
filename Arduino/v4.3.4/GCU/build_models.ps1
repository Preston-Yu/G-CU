param(
  [string]$Fqbn = "esp32:esp32:esp32s3",
  [string]$Sketch = ".",
  [string]$OutDir = "build_models",
  [string[]]$Models = @("v2.1", "v2.2.c", "v2.3.d"),
  [string]$Partitions = "min_spiffs",
  [string]$FirmwareVersion = "",
  [bool]$Pause = $true
)

$ErrorActionPreference = "Stop"

try {
if (-not (Get-Command arduino-cli -ErrorAction SilentlyContinue)) {
  Write-Error "arduino-cli not found in PATH."
  exit 1
}

$sketchPath = (Resolve-Path $Sketch).Path
$outDirPath = Resolve-Path -LiteralPath $OutDir -ErrorAction SilentlyContinue
if (-not $outDirPath) {
  $outDirPath = (New-Item -ItemType Directory -Force -Path $OutDir).FullName
} else {
  $outDirPath = $outDirPath.Path
}

$versionTag = $FirmwareVersion
if ([string]::IsNullOrWhiteSpace($versionTag)) {
  $inoFile = Get-ChildItem -Path $sketchPath -Filter "*.ino" | Select-Object -First 1
  if (-not $inoFile) {
    throw "No .ino file found under $sketchPath"
  }
  $matchInfo = Select-String -Path $inoFile.FullName -Pattern 'kFirmwareVersion\[\]\s*=\s*"([^"]+)"' -AllMatches | Select-Object -First 1
  if (-not $matchInfo -or $matchInfo.Matches.Count -eq 0) {
    throw "kFirmwareVersion not found in $($inoFile.Name)"
  }
  $versionTag = $matchInfo.Matches[0].Groups[1].Value
}

$fqbnWithPartition = $Fqbn
if ($Fqbn -notmatch "PartitionScheme=") {
  if ($Fqbn -match ":[^:]+=") {
    $fqbnWithPartition = "${Fqbn},PartitionScheme=$Partitions"
  } else {
    $fqbnWithPartition = "${Fqbn}:PartitionScheme=$Partitions"
  }
}

foreach ($model in $Models) {
  $modelDir = Join-Path $outDirPath $model
  New-Item -ItemType Directory -Force -Path $modelDir | Out-Null
  switch ($model) {
    "v2.1" { $modelFlag = "-DGCU_MODEL_V21" }
    "v2.2.c" { $modelFlag = "-DGCU_MODEL_V22C" }
    "v2.3.d" { $modelFlag = "-DGCU_MODEL_V23D" }
    default { throw "Unknown model: $model" }
  }
  $flags = 'build.extra_flags=' + $modelFlag
  Write-Host "==> Building model $model"
  arduino-cli compile --fqbn $fqbnWithPartition $sketchPath --output-dir $modelDir --build-property $flags

  $targetBase = "gcu-$model-$versionTag"
  $mergedBin = Get-ChildItem -Path $modelDir -Filter "*.merged.bin" | Select-Object -First 1
  if ($mergedBin) {
    Rename-Item -Path $mergedBin.FullName -NewName ($targetBase + ".merged.bin") -Force
  }
  $mainBin = Get-ChildItem -Path $modelDir -Filter "*.bin" |
    Where-Object { $_.Name -notlike "*.merged.bin" -and $_.Name -notlike "*bootloader*.bin" -and $_.Name -notlike "*partitions*.bin" } |
    Sort-Object Length -Descending | Select-Object -First 1
  if ($mainBin) {
    Rename-Item -Path $mainBin.FullName -NewName ($targetBase + ".bin") -Force
  }
}
} finally {
  if ($Pause) {
    Write-Host "Build finished. Press Enter to exit..."
    [void](Read-Host)
  }
}
