param(
  [Parameter(Mandatory = $true)]
  [string]$Ip,

  [int]$Retries = 3,

  [string]$Environment = "esp32dev"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
# Keep native command stderr (e.g. espota DEBUG lines) from being promoted to PowerShell errors.
if ($null -ne (Get-Variable -Name PSNativeCommandUseErrorActionPreference -ErrorAction SilentlyContinue)) {
  $PSNativeCommandUseErrorActionPreference = $false
}

$projectRoot = Split-Path -Parent $PSScriptRoot
$inoPath = Join-Path $projectRoot "esp32_relay_ota_webui.ino"
$srcDir = Join-Path $projectRoot "src"
$mainCppPath = Join-Path $srcDir "main.cpp"

if (-not (Test-Path $inoPath)) {
  throw "Sketch not found at $inoPath"
}

if (-not (Test-Path $srcDir)) {
  New-Item -ItemType Directory -Path $srcDir | Out-Null
}

Copy-Item -Path $inoPath -Destination $mainCppPath -Force
Write-Host "[OTA] Synced sketch to src/main.cpp" -ForegroundColor DarkCyan

function Invoke-OtaUpload {
  param(
    [string]$TargetIp,
    [string]$EnvName
  )

  Write-Host "[OTA] Building firmware..." -ForegroundColor Cyan
  & pio run -e $EnvName --upload-port $TargetIp
  if ($LASTEXITCODE -ne 0) {
    throw "Build failed with exit code $LASTEXITCODE"
  }

  Write-Host "[OTA] Uploading to $TargetIp ..." -ForegroundColor Cyan
  & pio run -e $EnvName -t upload --upload-port $TargetIp
  if ($LASTEXITCODE -ne 0) {
    throw "Upload failed with exit code $LASTEXITCODE"
  }
}

for ($attempt = 1; $attempt -le $Retries; $attempt++) {
  try {
    Write-Host "[OTA] Attempt $attempt/$Retries" -ForegroundColor Yellow
    Invoke-OtaUpload -TargetIp $Ip -EnvName $Environment
    Write-Host "[OTA] Success on attempt $attempt" -ForegroundColor Green
    exit 0
  }
  catch {
    Write-Warning "[OTA] Attempt $attempt failed: $($_.Exception.Message)"
    if ($attempt -eq $Retries) {
      Write-Error "[OTA] All retries failed."
      exit 1
    }
  }
}
