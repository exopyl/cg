# Sert maker\web sur http://127.0.0.1:8099/ (Ctrl+C pour arreter).
# Les modules ES + le .wasm exigent HTTP (pas file://). localhost est un
# contexte securise, donc tout fonctionne en local.
param([int]$Port = 8099)

$web = Join-Path $PSScriptRoot "web"
if (-not (Test-Path (Join-Path $web "maker.js"))) {
  Write-Warning "maker\web\maker.js absent : lance d'abord .\maker\build.ps1"
}
# serve.py plutot que `python -m http.server` : il ajoute Cache-Control:no-store,
# sans quoi le navigateur garde le maker.js/maker.wasm de la build precedente
# apres un build.ps1 (voir l'en-tete de serve.py).
python (Join-Path $PSScriptRoot "serve.py") --port $Port --bind 127.0.0.1 --directory $web
