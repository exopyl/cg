# Build de l'INSTRUMENT DE MESURE du jalon K4 (cf. k4_probe.cpp).
# Usage :  .\maker\tools\k4_probe\build.ps1
#
# Ce script ne construit pas `maker` et ne le modifie pas. Il relie le harnais a
# la bibliotheque cgmesh DEJA compilee par le preset `maker-wasm`, avec les memes
# options que la cible reelle -- meme -O3, meme -fexceptions, meme moteur : le
# chiffre mesure est celui du code livre, pas celui d'une compilation a part.
#
# Prerequis : `cmake --build --preset maker-wasm --target cgmesh` a jour.
# Sortie : maker\web\k4_probe.js + .wasm, ouvrables par maker\web\k4_probe.html
#          (servir maker\web, p.ex. `python maker\serve.py --directory maker\web`)
#          ou executables directement par le node de l'emsdk.

$ErrorActionPreference = "Stop"

$EMSDK_CANDIDATES = @($env:EMSDK, "C:\home\dev\extern\emsdk", "C:\home\bin\emsdk-6.0.3")
$EMSDK_DIR = $EMSDK_CANDIDATES |
  Where-Object { $_ -and (Test-Path (Join-Path $_ "upstream\emscripten\emcc.py")) } |
  Select-Object -First 1
if (-not $EMSDK_DIR) { Write-Error "emsdk introuvable. Essayes : $($EMSDK_CANDIDATES -join ', ')"; exit 1 }

$repo = Resolve-Path (Join-Path $PSScriptRoot "..\..\..")
$lib  = Join-Path $repo "build\maker-wasm\src\cgmesh\libcgmesh.a"
if (-not (Test-Path $lib)) {
  Write-Error "libcgmesh.a absente : lancer d'abord ``cmake --build --preset maker-wasm --target cgmesh``"
  exit 1
}

$env:EMSDK = $EMSDK_DIR
$env:PATH  = "$EMSDK_DIR\upstream\emscripten;$env:PATH"

$empp = Join-Path $EMSDK_DIR "upstream\emscripten\em++.exe"
$svg  = Join-Path $repo "test\data\svg\Ghostscript_Tiger.svg"
$out  = Join-Path $repo "maker\web\k4_probe.js"

# Les includes reprennent ceux de la cible cgmesh sous Emscripten
# (build\maker-wasm\src\cgmesh\CMakeFiles\cgmesh.dir\includes_CXX.rsp).
& $empp `
  (Join-Path $PSScriptRoot "k4_probe.cpp") `
  $lib `
  (Join-Path $repo "build\maker-wasm\src\cgimg\libcgimg.a") `
  (Join-Path $repo "build\maker-wasm\src\cgmath\libcgmath.a") `
  "-I$(Join-Path $repo 'src')" `
  "-I$(Join-Path $repo 'src\cgmesh')" `
  -isystem "$(Join-Path $repo 'extern')" `
  -O3 -std=gnu++17 -fexceptions -DNDEBUG `
  "-sALLOW_MEMORY_GROWTH=1" `
  "-sENVIRONMENT=web,node" `
  "-sEXIT_RUNTIME=0" `
  --embed-file "$svg@/tiger.svg" `
  -o $out
if ($LASTEXITCODE -ne 0) { Write-Error "echec de la compilation du harnais"; exit 1 }

Write-Host "OK -> maker\web\k4_probe.js" -ForegroundColor Green
