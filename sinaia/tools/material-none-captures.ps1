<#
.SYNOPSIS
    Détecteur de la PLAGE SANS MATÉRIAU : une région qui n'en déclare aucun ne
    doit porter aucun motif du matériau voisin.

.DESCRIPTION
    CE SCRIPT EST LE QUATRIÈME, et il ne mesure ce que mesure aucun des trois
    autres :
      - shader-captures.ps1      : pipeline fixe CONTRE shader, même image ;
      - draw-order-captures.ps1  : même chemin, DEUX ORDRES de dessin ;
      - material-color-captures.ps1 : la couleur d'un aplat contre celle du
                                      fichier qui la déclare ;
      - celui-ci                 : une région ne doit porter AUCUN MOTIF.

    ORACLE ABSOLU, comme material-color-captures.ps1 : il ne compare aucune
    capture à une autre capture, donc la réserve R10 -- le rendu absolu d'un
    modèle varie d'une session à l'autre -- est sans objet ici. Aucun
    appariement n'est requis.

    CE QU'IL GARDE. Les paramètres d'une plage de dessin sont un état GL : une
    plage qui n'active aucun matériau garde la texture liée, les uniformes et
    les canaux de matériau de la plage précédente. MATERIAL_NONE valant
    (unsigned)-1, c'est la plus grande clef du tri par identifiant de
    Mesh::BuildRenderData : cette plage est TOUJOURS dessinée en dernier, donc
    elle hérite systématiquement -- et non par hasard d'ordonnancement.

    LA GRANDEUR MESURÉE est l'ÉCART-TYPE DE LUMINANCE dans chaque moitié du
    modèle. Elle sépare un aplat d'un motif SANS dépendre de la couleur du
    matériau par défaut, qui n'est pas la propriété testée : changer ce bleu ne
    doit pas faire rougir ce détecteur. Le quad de gauche n'a pas de matériau,
    celui de droite porte la texture ; le premier doit être un aplat, le second
    ne l'est pas -- et c'est sa non-uniformité qui atteste que la mesure sait
    voir un motif quand il y en a un.

.EXAMPLE
    # sinaia doit tourner.
    .\material-none-captures.ps1

.EXAMPLE
    # Rejuger une capture déjà produite.
    .\material-none-captures.ps1 -CompareOnly
#>
[CmdletBinding()]
param(
    [string] $OutDir = '',
    [int]    $Port = 7777,
    [switch] $CompareOnly
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$scriptDir = if ($PSScriptRoot) { $PSScriptRoot }
             else { Split-Path -Parent $MyInvocation.MyCommand.Path }

$console = Join-Path $scriptDir 'sinaia-console.ps1'
if (-not (Test-Path $console)) { throw "Client de console introuvable : $console" }

if ([string]::IsNullOrWhiteSpace($OutDir)) {
    $OutDir = Join-Path $scriptDir '..\..\build\material-none-captures'
}
$OutDir = [System.IO.Path]::GetFullPath($OutDir)
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

$model = [System.IO.Path]::GetFullPath(
             (Join-Path $scriptDir '..\..\test\data\obj\material_none_inherits.obj'))
if (-not (Test-Path $model)) {
    throw ("Modele introuvable : $model`n" +
           "Son quad de gauche est declare AVANT tout usemtl : sans lui, aucune " +
           "plage MATERIAL_NONE n'est atteignable.")
}

# 0,12 : mesure de reference sur cette paire. Un aplat eclaire releve 0,059 --
# l'ecart-type n'est pas nul a cause des pixels de bord antialiases -- et la
# region texturee 0,187. Le seuil est place a mi-chemin en echelle, assez loin
# des deux pour ne pas dependre du filtrage ni de la taille de la fenetre.
$seuilAplat = 0.12

function Invoke-Capture {
    # Pas de `glcheck on` : ce script ne lit pas la fenetre de journalisation ou
    # il ecrit, et le laisser actif serialise le pipeline pour la suite de la
    # session. Le compte d'erreurs GL se releve par `caps`.
    # R23 : SURBRILLANCE DE SURVOL COUPEE AVANT TOUTE CAPTURE. Les aretes
    # jaunes de l'AABB du modele sous le curseur entrent dans l'image comme
    # n'importe quel pixel : une serie en a rendu 18 appariables sur 24 pour
    # cette seule raison, et l'appariement R10 echoue alors sans rien dire.
    # Une commande, et non un deplacement de curseur, qui dependrait de la
    # geometrie de l'ecran.
    $commands = @(
        'hover off',
        "open $model",
        'toggle fill on', 'toggle wireframe off', 'toggle points off',
        'toggle warning off', 'toggle repere off', 'toggle grid off',
        'cuttingmat off', 'shading materials',
        'camera reset', 'camera azel 0 0',
        'shader off', "screenshot $(Join-Path $OutDir 'material_none_shader-off.png')",
        'shader on',  "screenshot $(Join-Path $OutDir 'material_none_shader-on.png')"
    )
    Write-Host "Capture : $($commands.Count) commandes vers 127.0.0.1:$Port" -ForegroundColor Cyan
    $reply = & $console -Port $Port -Command $commands
    $reply | Write-Verbose

    $errors = @(@($reply) | Where-Object { $_ -match '^ERR ' })
    if ($errors.Count -gt 0) {
        Write-Warning "$($errors.Count) commande(s) refusee(s) :"
        $errors | ForEach-Object { Write-Warning "  $_" }
    }
}

# Écart-type de luminance dans chaque moitié de la boîte englobante du modèle.
# Les bornes sont déduites des pixels rendus : la mesure suit le cadrage au lieu
# de supposer une position en pixels.
function Measure-Halves {
    param([string] $Path)

    Add-Type -AssemblyName System.Drawing
    $bmp = [System.Drawing.Bitmap]::FromFile($Path)
    try {
        $bg = $bmp.GetPixel(0, 0)
        $x0 = $bmp.Width; $x1 = -1; $y0 = $bmp.Height; $y1 = -1
        for ($y = 0; $y -lt $bmp.Height; $y += 2) {
            for ($x = 0; $x -lt $bmp.Width; $x += 2) {
                $p = $bmp.GetPixel($x, $y)
                if ([Math]::Abs($p.R - $bg.R) + [Math]::Abs($p.G - $bg.G) +
                    [Math]::Abs($p.B - $bg.B) -gt 5) {
                    if ($x -lt $x0) { $x0 = $x }; if ($x -gt $x1) { $x1 = $x }
                    if ($y -lt $y0) { $y0 = $y }; if ($y -gt $y1) { $y1 = $y }
                }
            }
        }
        if ($x1 -lt 0) { throw "Aucun pixel de modele dans $Path (fenetre non dimensionnee ?)" }

        $xm = [int](($x0 + $x1) / 2)
        # Quatre pixels de garde de part et d'autre du milieu : l'interstice
        # entre les deux quads ne doit entrer dans aucune des deux mesures.
        $regions = @(
            @{ nom = 'gauche (SANS materiau)'; xa = $x0;      xb = $xm - 4 },
            @{ nom = 'droite (texturee)';      xa = $xm + 4;  xb = $x1     }
        )

        $rows = @()
        foreach ($r in $regions) {
            $n = 0; $somme = 0.0; $sommeCarres = 0.0
            for ($y = $y0; $y -le $y1; $y++) {
                for ($x = $r.xa; $x -le $r.xb; $x++) {
                    $p = $bmp.GetPixel($x, $y)
                    if ([Math]::Abs($p.R - $bg.R) + [Math]::Abs($p.G - $bg.G) +
                        [Math]::Abs($p.B - $bg.B) -le 5) { continue }
                    $l = (0.299 * $p.R + 0.587 * $p.G + 0.114 * $p.B) / 255.0
                    $n++; $somme += $l; $sommeCarres += $l * $l
                }
            }
            if ($n -eq 0) { throw "Region vide dans $Path : $($r.nom)" }
            $moy = $somme / $n
            $variance = [Math]::Max(0.0, ($sommeCarres / $n) - ($moy * $moy))
            $rows += [pscustomobject]@{
                Region    = $r.nom
                Pixels    = $n
                Moyenne   = '{0:N4}' -f $moy
                EcartType = '{0:N4}' -f [Math]::Sqrt($variance)
                Brut      = [Math]::Sqrt($variance)
            }
        }
        return $rows
    }
    finally { $bmp.Dispose() }
}

if (-not $CompareOnly) { Invoke-Capture }

$echecs = 0
foreach ($chemin in @('shader-off', 'shader-on')) {
    $png = Join-Path $OutDir "material_none_$chemin.png"
    if (-not (Test-Path $png)) { Write-Warning "Capture absente : $png"; exit 1 }

    Write-Host "`nChemin $chemin :" -ForegroundColor Cyan
    $rows = Measure-Halves -Path $png
    $rows | Format-Table Region, Pixels, Moyenne, EcartType -AutoSize

    $sansMateriau = $rows[0].Brut
    $texturee     = $rows[1].Brut

    if ($sansMateriau -gt $seuilAplat) {
        Write-Host ("  HERITAGE : la plage sans materiau porte un motif " +
                    "(ecart-type {0:N4} > {1:N2})." -f $sansMateriau, $seuilAplat) -ForegroundColor Red
        $echecs++
    }
    # LE DETECTEUR SE CONTROLE LUI-MEME : si la region texturee passait pour un
    # aplat, la mesure ne saurait plus voir un motif, et un « aucun heritage »
    # ne vaudrait rien.
    elseif ($texturee -le $seuilAplat) {
        Write-Host ("  MESURE NON PROBANTE : la region texturee passe pour un aplat " +
                    "(ecart-type {0:N4}). Le detecteur ne sait plus voir un motif." -f $texturee) -ForegroundColor Red
        $echecs++
    }
    else {
        Write-Host "  aplat a gauche, motif a droite : ok" -ForegroundColor Green
    }
}

Write-Host "`nImages dans $OutDir" -ForegroundColor Cyan
if ($echecs -gt 0) {
    Write-Host ("PLAGE SANS MATERIAU : $echecs chemin(s) en echec. La boucle de " +
                "DrawMaterialGroups ne repose pas d'etat pour une plage qui " +
                "n'active aucun materiau.") -ForegroundColor Red
    exit 1
}
Write-Host "Aucun heritage par la plage sans materiau." -ForegroundColor Green
