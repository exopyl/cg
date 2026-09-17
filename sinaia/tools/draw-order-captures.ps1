<#
.SYNOPSIS
    Detecteur de FUITE D'ETAT DE MATERIAU : meme chemin de rendu, deux ordres de
    dessin.

.DESCRIPTION
    CE SCRIPT N'EST PAS shader-captures.ps1, et ne mesure pas la meme chose.
    shader-captures.ps1 compare le PIPELINE FIXE au SHADER sur une meme image ;
    il cherche un ecart entre deux implementations du rendu. Ici on compare une
    image A UNE AUTRE IMAGE DU MEME CHEMIN, obtenue en changeant uniquement
    l'ORDRE DE DESSIN. C'est l'axe orthogonal, et shader-captures.ps1 ne l'a pas.

    CE QU'ON CHERCHE. L'egalite, et rien d'autre. Les parametres de materiau sont
    un ETAT GLOBAL du contexte GL : un canal qu'une branche de
    MaterialRenderer::ActivateMaterial ne pose pas garde la valeur du materiau
    precedent. L'apparence depend alors de l'ordre de dessin, ce qui est un
    defaut sans exception possible. Le critere est donc STRICT : ecart maximal
    nul, sur tous les pixels, sans sous-echantillonnage.

    LES DEUX MODELES. test/data/obj/emission_leak_{emissive,textured}_first.obj
    portent la meme geometrie et les memes deux materiaux ; seul l'ordre de leurs
    blocs newmtl differe, donc seul l'ordre de dessin differe. Leurs en-tetes
    portent les deux faits qui rendent la paire valide -- lire ces en-tetes avant
    de toucher aux fichiers.

    LES DEUX CHEMINS SONT VERIFIES, `shader off` et `shader on`. Une fuite d'etat
    GL est indifferente au chemin : elle se voit identiquement des deux cotes.
    C'est une propriete qu'on veut voir se maintenir, donc on la mesure.

    CE QUE CE SCRIPT NE COUVRE PAS -- le RENDU EN MODE IMMEDIAT. Deux regimes
    d'ordre coexistent dans cgre :
      - chemin VBO : VBOManager::DrawMaterialGroups dessine une plage par
        materiau, et Mesh::BuildRenderData les trie par IDENTIFIANT de materiau
        (std::map, mesh.cpp:826). L'ordre de dessin est donc celui des
        declarations de materiau, jamais celui des faces -- un test qui range ses
        faces croit piloter l'ordre et ne pilote rien. MATERIAL_NONE valant
        (unsigned)-1, les faces sans materiau passent EN DERNIER ;
      - chemin immediat : mesh_draw change de materiau dans l'ORDRE DES FACES.
    Ce script n'exerce que le premier, et aucun harnais pilote par sinaia ne
    peut faire autrement : wxOpenGLCanvas enregistre chaque maillage en
    CG_RENDERING_VBO, et le deroutement vers mesh_draw est explicitement exclu
    pour cette methode (mesh_renderer.cpp, garde `method != CG_RENDERING_VBO`).
    Aucune SURFACE de sinaia n'est donc dessinee en mode immediat -- mesh_draw
    n'y sert que les surcouches. Couvrir ce second regime demande un hote de
    cgre qui enregistre ses maillages autrement.

    ⚠ LA CLE D'APPARIEMENT DE R10 NE S'APPLIQUE PAS ICI. Ailleurs dans ce
    chantier, deux series ne se comparent que si leurs captures `_fixe` sont
    identiques au bit pres, le rendu absolu d'un modele variant d'une session a
    l'autre. Cette cle suppose que le correctif etudie ne touche pas le pipeline
    fixe -- ce qui est faux pour une correction d'etat de materiau, et ces deux
    modeles sont precisement ceux qu'une telle correction change. Ici la
    comparaison est INTERNE a une session et ne franchit jamais sa frontiere :
    les deux images d'une paire sont prises a quelques commandes d'intervalle,
    dans le meme processus. C'est ce qui la rend valide sans appariement.

.EXAMPLE
    # sinaia doit tourner. Produit les paires puis rend le verdict.
    .\draw-order-captures.ps1

.EXAMPLE
    # Comparer une serie deja produite, sans recapturer.
    .\draw-order-captures.ps1 -CompareOnly
#>
[CmdletBinding()]
param(
    # Laisse vide : $PSScriptRoot n'est pas encore peuple dans un bloc param()
    # sous Windows PowerShell 5.1. Le defaut est calcule dans le corps.
    [string]  $OutDir = '',
    # Deux points de vue suffisent : la fuite porte sur un canal de materiau, pas
    # sur une geometrie, donc elle ne se cache pas dans un angle.
    [int[][]] $Views = @( @(0, 0), @(35, 20) ),
    [int]     $Port = 7777,
    [switch]  $CompareOnly
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$scriptDir = if ($PSScriptRoot) { $PSScriptRoot }
             else { Split-Path -Parent $MyInvocation.MyCommand.Path }

$console = Join-Path $scriptDir 'sinaia-console.ps1'
if (-not (Test-Path $console)) { throw "Client de console introuvable : $console" }

if ([string]::IsNullOrWhiteSpace($OutDir)) {
    $OutDir = Join-Path $scriptDir '..\..\build\draw-order-captures'
}
$OutDir = [System.IO.Path]::GetFullPath($OutDir)
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

# La PAIRE n'est pas un parametre : c'est elle qui definit ce que le script
# mesure. Deux fichiers quelconques ne diraient rien d'un ordre de dessin.
$dataDir  = [System.IO.Path]::GetFullPath((Join-Path $scriptDir '..\..\test\data\obj'))
$variants = [ordered]@{
    'emissive_first' = Join-Path $dataDir 'emission_leak_emissive_first.obj'
    'textured_first' = Join-Path $dataDir 'emission_leak_textured_first.obj'
}

foreach ($v in $variants.Keys) {
    if (-not (Test-Path $variants[$v])) {
        throw ("Modele de la paire introuvable : $($variants[$v])`n" +
               "Sans les deux variantes, ce script ne mesure rien.")
    }
}

# ---------------------------------------------------------------------------
# 1. Capture
# ---------------------------------------------------------------------------
function Invoke-Capture {
    $commands = New-Object System.Collections.Generic.List[string]
    $commands.Add('glcheck on')

    # R23 : SURBRILLANCE DE SURVOL COUPEE AVANT TOUTE CAPTURE. Les aretes
    # jaunes de l'AABB du modele sous le curseur entrent dans l'image comme
    # n'importe quel pixel : une serie en a rendu 18 appariables sur 24 pour
    # cette seule raison, et l'appariement R10 echoue alors sans rien dire.
    # Une commande, et non un deplacement de curseur, qui dependrait de la
    # geometrie de l'ecran.
    $commands.Add('hover off')

    foreach ($v in $variants.Keys) {
        # `open` cree un NOUVEL onglet : les deux variantes ne se melangent pas.
        $commands.Add("open $($variants[$v])")

        # Etat d'affichage FIXE. Le tapis de coupe est retire : il est dessine en
        # fixe-fonction et ajouterait du bruit commun aux deux variantes, mais il
        # deplacerait le cadrage de `camera reset`.
        $commands.Add('toggle fill on')
        foreach ($o in @('wireframe', 'points', 'warning', 'repere', 'grid')) {
            $commands.Add("toggle $o off")
        }
        $commands.Add('cuttingmat off')
        $commands.Add('shading materials')

        foreach ($view in $Views) {
            $az, $el = $view[0], $view[1]
            $commands.Add('camera reset')
            $commands.Add("camera azel $az $el")

            foreach ($path in @('off', 'on')) {
                $commands.Add("shader $path")
                $stem = "{0}_az{1}_el{2}_shader-{3}" -f $v, $az, $el, $path
                $commands.Add("screenshot $(Join-Path $OutDir ($stem + '.png'))")
            }
        }
        $commands.Add('shader on')
    }

    Write-Host "Capture : $($commands.Count) commandes vers 127.0.0.1:$Port" -ForegroundColor Cyan
    # Liaison EXPLICITE au parametre -Command : passe en positionnel, le tableau
    # serait aplati en une seule chaine dont sinaia ne lirait que le premier mot.
    $reply = & $console -Port $Port -Command $commands.ToArray()
    $reply | Write-Verbose

    $errors = @(@($reply) | Where-Object { $_ -match '^ERR ' })
    if ($errors.Count -gt 0) {
        Write-Warning "$($errors.Count) commande(s) refusee(s) :"
        $errors | ForEach-Object { Write-Warning "  $_" }
    }
}

# ---------------------------------------------------------------------------
# 2. Verdict
# ---------------------------------------------------------------------------
# TOUS LES PIXELS, sans le pas de 2 qu'emploie shader-captures.ps1. Celui-ci
# cherche un ordre de grandeur sur un ecart attendu ; ici l'ecart attendu est
# NUL, et un sous-echantillonnage pourrait manquer la seule plage fautive.
function Compare-Orders {
    Add-Type -AssemblyName System.Drawing

    $refs = @(Get-ChildItem -Path $OutDir -Filter 'emissive_first_*.png' -ErrorAction SilentlyContinue |
              Sort-Object Name)
    if ($refs.Count -eq 0) { Write-Warning "Aucune capture dans $OutDir"; return }

    $failures = 0
    $rows = foreach ($a in $refs) {
        $bPath = $a.FullName -replace 'emissive_first_', 'textured_first_'
        if (-not (Test-Path $bPath)) { Write-Warning "Sans pendant : $($a.Name)"; continue }

        $ia = [System.Drawing.Bitmap]::FromFile($a.FullName)
        $ib = [System.Drawing.Bitmap]::FromFile($bPath)
        try {
            if ($ia.Width -ne $ib.Width -or $ia.Height -ne $ib.Height) {
                $failures++
                [pscustomobject]@{ Cas = $a.BaseName -replace '^emissive_first_',''
                                   Differents = 'TAILLES DIFFERENTES'; EcartMax = '-'; Verdict = 'ECHEC' }
                continue
            }

            $differing = 0; $maxDelta = 0
            $total = $ia.Width * $ia.Height
            for ($y = 0; $y -lt $ia.Height; $y++) {
                for ($x = 0; $x -lt $ia.Width; $x++) {
                    $pa = $ia.GetPixel($x, $y); $pb = $ib.GetPixel($x, $y)
                    $d = [Math]::Max([Math]::Abs($pa.R - $pb.R),
                         [Math]::Max([Math]::Abs($pa.G - $pb.G), [Math]::Abs($pa.B - $pb.B)))
                    if ($d -gt $maxDelta) { $maxDelta = $d }
                    if ($d -gt 8) { $differing++ }
                }
            }

            # STRICT : la geometrie, les materiaux et la camera sont identiques,
            # la rasterisation est deterministe. Le seul ecart possible est un
            # etat herite. Aucune tolerance n'a de justification ici.
            $ok = ($maxDelta -eq 0)
            if (-not $ok) { $failures++ }
            [pscustomobject]@{
                Cas        = $a.BaseName -replace '^emissive_first_',''
                Differents = '{0,6:N2} %' -f (100.0 * $differing / $total)
                EcartMax   = $maxDelta
                Verdict    = if ($ok) { 'ok' } else { 'FUITE' }
            }
        }
        finally { $ia.Dispose(); $ib.Dispose() }
    }

    $rows | Format-Table -AutoSize
    Write-Host "Images dans $OutDir" -ForegroundColor Cyan

    if ($failures -gt 0) {
        Write-Host ("FUITE D'ETAT : $failures cas sur " + @($rows).Count +
                    " dependent de l'ordre de dessin.") -ForegroundColor Red
        Write-Host ("Une branche de MaterialRenderer::ActivateMaterial ne pose pas " +
                    "un canal de materiau et herite de celui du materiau precedent.") -ForegroundColor Red
        exit 1
    }
    Write-Host "Aucune dependance a l'ordre de dessin." -ForegroundColor Green
}

if (-not $CompareOnly) { Invoke-Capture }
Compare-Orders
