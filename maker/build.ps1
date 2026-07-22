# Build du module WASM `maker` (cgmesh via Emscripten).
# Usage :  .\maker\build.ps1
# Prerequis : emsdk, cmake, et Visual Studio (nmake) installes.
# Ajuste les 3 chemins ci-dessous si ton installation differe.

$ErrorActionPreference = "Stop"

$EMSDK_DIR = "C:\home\bin\emsdk-6.0.3"
$CMAKE_BIN = "C:\home\bin\cmake-4.2.0-windows-x86_64\bin"
# Repertoire de nmake (Visual Studio). Detecte automatiquement la version MSVC.
$MSVC_ROOT = "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC"

function Fail($msg) { Write-Error $msg; exit 1 }

if (-not (Test-Path $EMSDK_DIR))            { Fail "emsdk introuvable : $EMSDK_DIR" }
if (-not (Test-Path "$CMAKE_BIN\cmake.exe")) { Fail "cmake introuvable : $CMAKE_BIN" }
if (-not (Test-Path $MSVC_ROOT))             { Fail "MSVC introuvable : $MSVC_ROOT" }

# nmake : prend la version MSVC la plus recente disponible.
$nmakeDir = Get-ChildItem $MSVC_ROOT -Directory |
  Sort-Object Name -Descending |
  ForEach-Object { Join-Path $_.FullName "bin\HostX64\x64" } |
  Where-Object { Test-Path (Join-Path $_ "nmake.exe") } |
  Select-Object -First 1
if (-not $nmakeDir) { Fail "nmake.exe introuvable sous $MSVC_ROOT" }

$env:EMSDK = $EMSDK_DIR
$env:PATH  = "$CMAKE_BIN;$nmakeDir;$EMSDK_DIR\upstream\emscripten;$env:PATH"

$repo = Split-Path -Parent $PSScriptRoot   # maker\ -> racine du depot

Push-Location $repo
try {
  Write-Host "Configuration (preset maker-wasm)..." -ForegroundColor Cyan
  & cmake --preset maker-wasm
  if ($LASTEXITCODE -ne 0) { Fail "echec de la configuration cmake" }

  Write-Host "Compilation..." -ForegroundColor Cyan
  & cmake --build --preset maker-wasm
  if ($LASTEXITCODE -ne 0) { Fail "echec de la compilation" }
} finally { Pop-Location }

Write-Host "OK -> maker\web\maker.js + maker.wasm" -ForegroundColor Green
