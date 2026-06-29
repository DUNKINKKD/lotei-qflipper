<#
  setup-lotei.ps1 — fetch LOTEI's runtime deps (Piper TTS + neural voices) and
  the Ollama model, so qFlipper.exe has everything it needs.

  Usage:   ./setup-lotei.ps1 -AppDir .\build
  -AppDir  = the folder that contains qFlipper.exe (Piper goes in <AppDir>\piper).
#>
param(
    [string]$AppDir = ".\build"
)

$ErrorActionPreference = "Stop"
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12

if (-not (Test-Path $AppDir)) { New-Item -ItemType Directory -Force -Path $AppDir | Out-Null }
$AppDir    = (Resolve-Path $AppDir).Path
$piperDir  = Join-Path $AppDir "piper"
$voicesDir = Join-Path $piperDir "voices"
Write-Host "Setting up LOTEI in: $AppDir" -ForegroundColor Magenta

# 1) Piper binary -----------------------------------------------------------
if (Test-Path (Join-Path $piperDir "piper.exe")) {
    Write-Host "Piper already installed."
} else {
    Write-Host "Downloading Piper..."
    $rel   = Invoke-RestMethod -Uri "https://api.github.com/repos/rhasspy/piper/releases/latest" -Headers @{ "User-Agent" = "lotei-setup" }
    $asset = $rel.assets | Where-Object { $_.name -match 'windows' -and $_.name -match 'amd64' -and $_.name -match 'zip$' } | Select-Object -First 1
    if (-not $asset) { throw "Could not find a Piper windows_amd64 release asset." }
    $zip = Join-Path $env:TEMP "piper_win.zip"
    Invoke-WebRequest -Uri $asset.browser_download_url -OutFile $zip
    $ex = Join-Path $env:TEMP "piper_ex"
    New-Item -ItemType Directory -Force -Path $ex | Out-Null
    Expand-Archive -Path $zip -DestinationPath $ex -Force
    $exe = Get-ChildItem -Path $ex -Recurse -Filter piper.exe | Select-Object -First 1
    New-Item -ItemType Directory -Force -Path $piperDir | Out-Null
    Copy-Item -Path (Join-Path $exe.DirectoryName '*') -Destination $piperDir -Recurse -Force
    Write-Host "  Piper installed -> $piperDir" -ForegroundColor Green
}

# 2) Neural voices ----------------------------------------------------------
New-Item -ItemType Directory -Force -Path $voicesDir | Out-Null
$base   = "https://huggingface.co/rhasspy/piper-voices/resolve/main"
$voices = @(
    "en/en_US/ryan/high/en_US-ryan-high",     # default (warm US male)
    "en/en_GB/alan/medium/en_GB-alan-medium", # British
    "en/en_US/joe/medium/en_US-joe-medium"    # alt US male
)
foreach ($v in $voices) {
    $name = Split-Path $v -Leaf
    if (Test-Path (Join-Path $voicesDir "$name.onnx")) { Write-Host "Voice $name already present."; continue }
    Write-Host "Downloading voice $name..."
    foreach ($ext in @(".onnx", ".onnx.json")) {
        Invoke-WebRequest -Uri "$base/$v$ext" -OutFile (Join-Path $voicesDir "$name$ext")
    }
}

# 3) Ollama model -----------------------------------------------------------
Write-Host ""
if (Get-Command ollama -ErrorAction SilentlyContinue) {
    Write-Host "Pulling qwen2.5:7b (~4.7 GB)..."
    & ollama pull qwen2.5:7b
} else {
    Write-Host "Ollama not found. Install it from https://ollama.com, then run:  ollama pull qwen2.5:7b" -ForegroundColor Yellow
}

Write-Host ""
Write-Host "LOTEI setup complete. Make sure 'ollama serve' is running, then launch qFlipper.exe. 🐬" -ForegroundColor Magenta
