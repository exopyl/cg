<#
.SYNOPSIS
    Detecteur des MaterialColor : chaque aplat doit rendre SA couleur sous le
    programme de surface.

.DESCRIPTION
    CE SCRIPT EST UN ORACLE ABSOLU, pas differentiel. Il ne compare pas deux
    images : il lit la couleur au centre de six aplats et la confronte a celle
    que le fichier source declare. C'est ce qui le rend insensible a la reserve
    R10 -- aucun appariement de session n'est requis, puisqu'aucune capture n'est
    comparee a une autre capture.

    CE QU'IL GARDE, sur les DEUX CHEMINS.

    Chemin shader : un MaterialColor ne porte sa couleur que par glColor et
    GL_COLOR_MATERIAL, qui n'existent plus sous programme lie. Si la branche
    MATERIAL_COLOR de MaterialRenderer::ActivateMaterial cesse de poser
    GL_AMBIENT et GL_DIFFUSE, le fragment lit un gl_FrontMaterial.diffuse
    perime : chaque plage rend la couleur de la PLAGE PRECEDENTE, et la premiere
    celle du materiau par defaut. Mesure de reference : 0 aplat sur 6 sans ces
    deux appels, 6 sur 6 avec.

    Pipeline fixe : il rendait ces six aplats en GRIS UNIFORME tant que le
    tableau de couleurs par sommet etait lie sans egard au mode d'ombrage, ce
    tableau primant sur glColor sous GL_COLOR_MATERIAL. Depuis que la liaison
    suit le mode, il rend 6 sur 6 lui aussi et l'ecart entre les deux chemins est
    tombe de 17,9 % des pixels a 0,00 % -- maximum 2 niveaux, soit le bruit de
    rasterisation. Les deux chemins sont donc JUGES, et non plus l'un juge et
    l'autre rapporte : une regression qui ne toucherait que le fixe passait
    jusqu'ici sans faire echouer ce script.

    POURQUOI PAS shader-captures.ps1, qui compare pourtant ces deux chemins. Son
    tableau n'a pas de verdict : il se lit a l'oeil, et une convergence exacte y
    apparaitrait comme trois lignes a zero parmi trente-six. L'oracle ABSOLU
    ci-dessous echoue, avec un code de sortie, et nomme la branche fautive.

    LE MODELE N'EST PAS UN FICHIER DE MAILLAGE. Aucun format importable ne porte
    de MaterialColor : la classe n'existe que derriere des generateurs. Le
    modele est donc construit par la commande `svgextrude`, qui active
    SvgExtrudeOptions::perShapeMaterials -- un MaterialColor par couleur de
    remplissage.

.EXAMPLE
    # sinaia doit tourner.
    .\material-color-captures.ps1

.EXAMPLE
    # Rejuger une capture deja produite.
    .\material-color-captures.ps1 -CompareOnly
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
    $OutDir = Join-Path $scriptDir '..\..\build\material-color-captures'
}
$OutDir = [System.IO.Path]::GetFullPath($OutDir)
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

$svg = [System.IO.Path]::GetFullPath(
           (Join-Path $scriptDir '..\..\test\data\svg\material_color_palette.svg'))
if (-not (Test-Path $svg)) {
    throw "Modele introuvable : $svg`nSans lui, aucun MaterialColor n'est atteignable."
}

# Les six aplats, dans l'ordre de lecture du SVG. Le fichier les pose aux sommets
# et aretes du cube RVB : deux quelconques different d'au moins 255 sur un canal,
# donc la tolerance n'a pas besoin d'etre serree pour etre concluante.
$attendu = @(
    @{ nom = 'rouge';   rgb = @(255,   0,   0) },
    @{ nom = 'vert';    rgb = @(  0, 255,   0) },
    @{ nom = 'bleu';    rgb = @(  0,   0, 255) },
    @{ nom = 'jaune';   rgb = @(255, 255,   0) },
    @{ nom = 'magenta'; rgb = @(255,   0, 255) },
    @{ nom = 'cyan';    rgb = @(  0, 255, 255) }
)
# 60/255 : l'eclairage assombrit un aplat sature d'environ 35/255 (mesure : 220
# au lieu de 255). La tolerance couvre cela sans jamais confondre deux aplats,
# qui sont a 255 l'un de l'autre sur au moins un canal.
$tolerance = 60

function Invoke-Capture {
    # Pas de `glcheck on` : ce script ne lit jamais la fenetre de journalisation
    # ou il ecrit, et le laisser actif serialise le pipeline pour tout le reste
    # de la session. Le compte d'erreurs GL se releve par `caps`, qui le rend
    # dans sa reponse.
    # R23 : SURBRILLANCE DE SURVOL COUPEE AVANT TOUTE CAPTURE. Les aretes
    # jaunes de l'AABB du modele sous le curseur entrent dans l'image comme
    # n'importe quel pixel : une serie en a rendu 18 appariables sur 24 pour
    # cette seule raison, et l'appariement R10 echoue alors sans rien dire.
    # Une commande, et non un deplacement de curseur, qui dependrait de la
    # geometrie de l'ecran.
    $commands = @(
        'hover off',
        "svgextrude $svg",
        'toggle fill on', 'toggle wireframe off', 'toggle points off',
        'toggle warning off', 'toggle repere off', 'toggle grid off',
        'cuttingmat off', 'shading materials',
        'camera reset', 'camera azel 0 0',
        'shader off', "screenshot $(Join-Path $OutDir 'palette_shader-off.png')",
        'shader on',  "screenshot $(Join-Path $OutDir 'palette_shader-on.png')"
    )
    # L'ONGLET RESTE OUVERT : aucune commande de la console n'en ferme un --
    # `drop N` pose un modele sur le plan Z = 0, il ne referme rien. Un tir
    # repete empile donc les onglets. C'est une gene d'interface et non un
    # risque de mesure : `svgextrude` cree un onglet NEUF qui devient l'onglet
    # actif, avec sa propre scene et sa propre camera, de sorte que la capture
    # porte toujours sur le modele que ce tir vient de construire.
    Write-Host "Capture : $($commands.Count) commandes vers 127.0.0.1:$Port" -ForegroundColor Cyan
    $reply = & $console -Port $Port -Command $commands
    $reply | Write-Verbose

    $errors = @(@($reply) | Where-Object { $_ -match '^ERR ' })
    if ($errors.Count -gt 0) {
        Write-Warning "$($errors.Count) commande(s) refusee(s) :"
        $errors | ForEach-Object { Write-Warning "  $_" }
    }
}

# Lit la couleur au centre de chaque aplat. Les centres sont deduits de la boite
# englobante des pixels du modele : la mesure suit donc le cadrage au lieu de
# supposer une position en pixels.
function Measure-Patches {
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

        $rows = @()
        for ($i = 0; $i -lt 6; $i++) {
            $r = [Math]::Floor($i / 3); $c = $i % 3
            $cx = [int]($x0 + ($x1 - $x0) * (1.0/6.0 + $c / 3.0))
            $cy = [int]($y0 + ($y1 - $y0) * (0.25 + 0.5 * $r))
            $p  = $bmp.GetPixel($cx, $cy)
            $exp = $attendu[$i].rgb
            $d = [Math]::Max([Math]::Abs($p.R - $exp[0]),
                 [Math]::Max([Math]::Abs($p.G - $exp[1]), [Math]::Abs($p.B - $exp[2])))
            $rows += [pscustomobject]@{
                Aplat   = $attendu[$i].nom
                Attendu = '{0},{1},{2}' -f $exp[0], $exp[1], $exp[2]
                Lu      = '{0},{1},{2}' -f $p.R, $p.G, $p.B
                Ecart   = $d
                Verdict = if ($d -le $tolerance) { 'ok' } else { 'FAUX' }
            }
        }
        return $rows
    }
    finally { $bmp.Dispose() }
}

if (-not $CompareOnly) { Invoke-Capture }

$shaderPng = Join-Path $OutDir 'palette_shader-on.png'
$fixePng   = Join-Path $OutDir 'palette_shader-off.png'
if (-not (Test-Path $shaderPng)) { Write-Warning "Capture absente : $shaderPng"; exit 1 }

$echecs = @()

Write-Host "`nChemin SHADER :" -ForegroundColor Cyan
$rows = Measure-Patches -Path $shaderPng
$rows | Format-Table -AutoSize
$bonsShader = @($rows | Where-Object { $_.Verdict -eq 'ok' }).Count
if ($bonsShader -lt 6) {
    $echecs += ("shader : $bonsShader/6. La branche MATERIAL_COLOR " +
                "d'ActivateMaterial ne pose pas GL_AMBIENT et GL_DIFFUSE -- le " +
                "fragment lit un materiau perime et chaque plage rend la couleur " +
                "de la precedente.")
}

if (-not (Test-Path $fixePng)) { Write-Warning "Capture absente : $fixePng"; exit 1 }

Write-Host "`nPipeline FIXE :" -ForegroundColor Cyan
$rowsFixe = Measure-Patches -Path $fixePng
$rowsFixe | Format-Table -AutoSize
$bonsFixe = @($rowsFixe | Where-Object { $_.Verdict -eq 'ok' }).Count
if ($bonsFixe -lt 6) {
    $echecs += ("pipeline fixe : $bonsFixe/6. Le tableau de couleurs par sommet " +
                "est lie sans egard au mode d'ombrage -- il prime sur glColor sous " +
                "GL_COLOR_MATERIAL et efface la couleur des materiaux.")
}

Write-Host "`nImages dans $OutDir" -ForegroundColor Cyan
if ($echecs.Count -gt 0) {
    Write-Host "MaterialColor PERDU :" -ForegroundColor Red
    $echecs | ForEach-Object { Write-Host "  $_" -ForegroundColor Red }
    exit 1
}
Write-Host "6/6 aplats a leur couleur sur les deux chemins." -ForegroundColor Green
