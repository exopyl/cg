<#
.SYNOPSIS
    Cinquieme harnais : il garde le CHEMIN PBR, que rien ne gardait (reserve R28).

.DESCRIPTION
    Les quatre harnais existants passent tous par le chemin NON-PBR -- sink.3ds,
    cube-tex-spec.obj, une extrusion SVG, un OBJ a plage sans materiau. Aucun
    n'ouvre un fichier glTF portant un MaterialPbr, donc aucun ne fait travailler
    le second programme GLSL. E3b et E4 ont ete mesures A LA MAIN.

    Ce script mesure, par POPULATION DE MATERIAU, trois grandeurs sur le rendu
    PBR : luminance moyenne, ECART-TYPE, et gradient de Sobel moyen.

    DEUX REGLES DE PROTOCOLE, et les chiffres qui les imposent.

    1. LA SEGMENTATION VIENT DU CANAL `metallic`, PAS D'UN SEUIL DE CHROMA.

       Un seuil de chroma ne separait pas le bois des ferrures : il separait un
       reglage d'un autre. Sur la meme image, l'ecart bois - ferrures valait
       -0,1204 a un seuil de 0,10 et +0,0849 a 0,30 -- le SIGNE du critere etait
       fonction du parametre. Un critere dont le signe se regle n'est pas
       falsifiable.

       `pbrchannel metallic` fait ecrire au fragment la grandeur QU'IL CONSOMME
       lui-meme pour choisir entre reflectance et diffuse. La segmentation est
       donc celle du materiau : elle ne bouge ni avec les lumieres, ni avec le
       cadrage, ni avec un seuil. Le masque est bimodal par construction -- sur
       Lantern.glb, 17 023 pixels sous 0,1 et 3 648 au-dessus de 0,9, pour 360
       entre les deux, qui sont des melanges de texels que l'erosion retire.

       La silhouette vient du canal `normal`, et non d'un ecart au fond : tout
       fragment dessine y ecrit une normale UNITAIRE, donc |2c-1| = 1, quand le
       fond de la fenetre donne 1,73. Le test est une propriete de la donnee, la
       ou « ce pixel differe-t-il du fond ? » confond un bois sombre avec le fond
       et compte tous les melanges de bord.

    2. LES MASQUES SONT ERODES D'UN PIXEL, des deux cotes.

       Sans erosion, 25 % des pixels retenus sont des melanges modele/fond,
       achromatiques par construction, et ils portent L'INTEGRALITE du signe de
       l'ecart mesure : -0,0719 sur masque brut, -0,0137 apres une erosion,
       +0,0325 apres deux. Une seule erosion annule le signe, deux l'inversent.
       Ce que le masque brut mesurait etait l'anticrenelage.

       L'erosion porte sur la silhouette ET sur chaque population : un pixel a la
       frontiere bois/ferrure est un melange des deux au meme titre.

    TROIS POPULATIONS, ET NON DEUX. Les texels emissifs de Lantern portent aussi
    metallic = 1 : la flamme tombe donc dans la population metallique, ou
    quelques centaines de pixels satures deplacent la moyenne de 0,15 a 0,19 et
    l'ecart-type de 0,08 a 0,24. Une statistique de REFLECTANCE polluee par une
    SOURCE n'en est plus une, et l'ecart bois - ferrures y changeait de signe
    d'une vue a l'autre. `pbrchannel emissive` les isole, et elles sont comptees
    a part.

    L'ECART-TYPE DE LA POPULATION METALLIQUE est la grandeur a surveiller. Le
    terme d'environnement du fragment est aujourd'hui RIGOUREUSEMENT CONSTANT
    (`surface_program.cpp`, `gl_LightModel.ambient * (diffuseColor + f0)`) :
    toute variation spatiale du metal ne peut venir que des deux lumieres
    ponctuelles. Une valeur qui bougerait sans qu'on ait touche a l'eclairage
    signalerait un changement de regime.

.PARAMETER InvertMask
    VALIDATION DU DETECTEUR, et non une option d'usage. Echange les deux
    populations. Un masque qui segmente vraiment doit faire CHANGER DE SIGNE
    l'ecart ; un masque qui ne segmente rien rendrait deux fois la meme moyenne,
    donc un ecart a peu pres nul. Un detecteur qu'on n'a jamais vu rouge ne vaut
    rien.

.EXAMPLE
    # sinaia doit tourner.
    .\pbr-captures.ps1

.EXAMPLE
    .\pbr-captures.ps1 -InvertMask     # doit inverser le signe de l'ecart
#>

[CmdletBinding()]
param(
    # Vides par defaut : $PSScriptRoot n'est pas encore lie dans le bloc param
    # quand le script est lance par -File, et un Join-Path sur une chaine vide
    # echoue avant la premiere ligne utile.
    [string] $OutDir = '',
    [int]    $Port   = 7777,

    # Un fichier glTF portant un MaterialPbr. Lantern.glb vit a la racine du
    # depot et n'y est PAS SUIVI : son absence est un avertissement et non une
    # erreur, mais elle ampute le harnais de tout son objet.
    [string] $Model = '',

    [int[][]] $Views = @( @(35, 20), @(120, 55) ),

    [switch] $CompareOnly,
    [switch] $InvertMask
)

$ErrorActionPreference = 'Stop'

$scriptDir = if ($PSScriptRoot) { $PSScriptRoot }
             else { Split-Path -Parent $MyInvocation.MyCommand.Path }

$console = Join-Path $scriptDir 'sinaia-console.ps1'
if (-not (Test-Path $console)) { throw "Client de console introuvable : $console" }

if ([string]::IsNullOrWhiteSpace($OutDir)) { $OutDir = Join-Path $scriptDir 'captures-pbr' }
if ([string]::IsNullOrWhiteSpace($Model))  { $Model  = Join-Path $scriptDir '..\..\Lantern.glb' }

$OutDir = [System.IO.Path]::GetFullPath($OutDir)
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
$Model = [System.IO.Path]::GetFullPath($Model)

if (-not (Test-Path $Model)) {
    Write-Warning "Modele PBR introuvable : $Model"
    Write-Warning "LE CHEMIN PBR N'EST PAS COUVERT par cette execution : elle ne dit rien de lui."
    exit 2
}

function Get-ViewTag { param($v) "az$($v[0])_el$($v[1])" }

function Invoke-Capture {
    $commands = New-Object System.Collections.Generic.List[string]
    $commands.Add('glcheck on')

    # R23 : la surbrillance de survol entre dans l'image comme n'importe quel
    # pixel. Une serie en a rendu 18 appariables sur 24 pour cette seule raison,
    # et l'appariement R10 echoue alors sans rien dire.
    $commands.Add('hover off')

    $commands.Add("open $Model")
    foreach ($o in @('wireframe', 'points', 'warning', 'repere', 'grid')) {
        $commands.Add("toggle $o off")
    }
    $commands.Add('toggle fill on')
    $commands.Add('cuttingmat off')
    $commands.Add('shading materials')
    $commands.Add('camera reset')
    $commands.Add('camera pivot model 0')

    foreach ($v in $Views) {
        $tag = Get-ViewTag $v
        $commands.Add("camera azel $($v[0]) $($v[1])")

        # LE RENDU FIXE EST LA CLE D'APPARIEMENT (R10) : le rendu absolu de
        # sinaia est bistable d'une session a l'autre, normales plates ou
        # moyennees. Deux series ne se comparent que si leurs `_fixe` sont
        # identiques au bit pres.
        $commands.Add('shader off')
        $commands.Add("screenshot $(Join-Path $OutDir "${tag}_fixe.png")")

        $commands.Add('shader on')
        $commands.Add('pbrchannel off')
        $commands.Add("screenshot $(Join-Path $OutDir "${tag}_pbr.png")")
        $commands.Add('pbrchannel metallic')
        $commands.Add("screenshot $(Join-Path $OutDir "${tag}_metallic.png")")
        $commands.Add('pbrchannel normal')
        $commands.Add("screenshot $(Join-Path $OutDir "${tag}_normal.png")")
        $commands.Add('pbrchannel emissive')
        $commands.Add("screenshot $(Join-Path $OutDir "${tag}_emissive.png")")

        # REMISE A `off` DANS LA SERIE, et non a la fin : une serie interrompue
        # ne doit pas laisser la session dans un mode d'inspection.
        $commands.Add('pbrchannel off')
    }

    Write-Host "Capture : $($commands.Count) commandes vers 127.0.0.1:$Port" -ForegroundColor Cyan
    $reply = & $console -Port $Port -Command $commands
    $reply | Write-Verbose

    $errors = @(@($reply) | Where-Object { $_ -match '^ERR ' })
    if ($errors.Count -gt 0) {
        Write-Warning "$($errors.Count) commande(s) refusee(s) :"
        $errors | ForEach-Object { Write-Warning "  $_" }
    }
}

# Lecture d'une image en un seul bloc. GetPixel coute un appel par pixel : sur
# 630 000 pixels et plusieurs passes, la mesure durerait plus longtemps que la
# capture.
function Read-Rgb {
    param([string] $Path)
    Add-Type -AssemblyName System.Drawing
    $bmp = [System.Drawing.Bitmap]::FromFile($Path)
    try {
        $rect = New-Object System.Drawing.Rectangle 0, 0, $bmp.Width, $bmp.Height
        $data = $bmp.LockBits($rect, [System.Drawing.Imaging.ImageLockMode]::ReadOnly,
                              [System.Drawing.Imaging.PixelFormat]::Format24bppRgb)
        try {
            $stride = $data.Stride
            $bytes  = New-Object byte[] ($stride * $bmp.Height)
            [System.Runtime.InteropServices.Marshal]::Copy($data.Scan0, $bytes, 0, $bytes.Length)
        }
        finally { $bmp.UnlockBits($data) }
        return [pscustomobject]@{ W = $bmp.Width; H = $bmp.Height; Stride = $stride; Bytes = $bytes }
    }
    finally { $bmp.Dispose() }
}

# Erosion a 4 voisins. Un pixel ne survit que si ses quatre voisins sont dans le
# masque : c'est ce qui retire les melanges de bord.
function Get-Eroded {
    param([bool[]] $Mask, [int] $W, [int] $H)
    $out = New-Object bool[] ($W * $H)
    for ($y = 1; $y -lt $H - 1; $y++) {
        $row = $y * $W
        for ($x = 1; $x -lt $W - 1; $x++) {
            $i = $row + $x
            if ($Mask[$i] -and $Mask[$i - 1] -and $Mask[$i + 1] -and
                $Mask[$i - $W] -and $Mask[$i + $W]) { $out[$i] = $true }
        }
    }
    return $out
}

function Measure-View {
    param([string] $Tag)

    $pbr = Read-Rgb (Join-Path $OutDir "${Tag}_pbr.png")
    $met = Read-Rgb (Join-Path $OutDir "${Tag}_metallic.png")
    $nrm = Read-Rgb (Join-Path $OutDir "${Tag}_normal.png")
    $emi = Read-Rgb (Join-Path $OutDir "${Tag}_emissive.png")
    $W = $pbr.W; $H = $pbr.H; $N = $W * $H

    if ($met.W -ne $W -or $met.H -ne $H -or $nrm.W -ne $W -or $nrm.H -ne $H -or
        $emi.W -ne $W -or $emi.H -ne $H) {
        throw "Tailles d'images differentes pour $Tag : la fenetre a bouge pendant la serie."
    }

    $lum = New-Object double[] $N
    $sil = New-Object bool[]   $N
    $mtl = New-Object bool[]   $N
    $emis = New-Object bool[]  $N
    for ($y = 0; $y -lt $H; $y++) {
        $src = $y * $pbr.Stride
        $row = $y * $W
        for ($x = 0; $x -lt $W; $x++) {
            $o = $src + $x * 3
            # Format24bppRgb range les octets en B, G, R.
            $b = $pbr.Bytes[$o]; $g = $pbr.Bytes[$o + 1]; $r = $pbr.Bytes[$o + 2]
            $lum[$row + $x] = (0.299 * $r + 0.587 * $g + 0.114 * $b) / 255.0

            # SILHOUETTE : une normale unitaire encodee donne |2c-1| = 1. Le fond
            # de la fenetre donne 1,73 -- il ne peut pas etre confondu.
            $nb = ($nrm.Bytes[$o]     / 127.5) - 1.0
            $ng = ($nrm.Bytes[$o + 1] / 127.5) - 1.0
            $nr = ($nrm.Bytes[$o + 2] / 127.5) - 1.0
            $len = [Math]::Sqrt($nr * $nr + $ng * $ng + $nb * $nb)
            $sil[$row + $x] = ([Math]::Abs($len - 1.0) -lt 0.08)

            # SEGMENTATION : le canal metallique, ecrit tel quel par le fragment.
            # Le seuil de 0,5 n'est pas un reglage : le masque est bimodal, il
            # n'y a presque rien entre les deux modes, et l'erosion retire le
            # reste.
            $mtl[$row + $x] = ($met.Bytes[$o + 2] -gt 127)

            # EMISSION : seuil bas et non median. Ce n'est pas une segmentation
            # mais une EXCLUSION -- tout pixel qui emet, si peu que ce soit, sort
            # d'une statistique de reflectance.
            $emis[$row + $x] = (($emi.Bytes[$o] + $emi.Bytes[$o + 1] + $emi.Bytes[$o + 2]) -gt 12)
        }
    }

    $silE = Get-Eroded $sil $W $H

    $aRaw = New-Object bool[] $N     # metal, hors emission
    $bRaw = New-Object bool[] $N     # dielectrique, hors emission
    $cRaw = New-Object bool[] $N     # emission
    for ($i = 0; $i -lt $N; $i++) {
        if (-not $silE[$i]) { continue }
        if     ($emis[$i]) { $cRaw[$i] = $true }
        elseif ($mtl[$i])  { $aRaw[$i] = $true }
        else               { $bRaw[$i] = $true }
    }
    $metalMask = Get-Eroded $aRaw $W $H
    $woodMask  = Get-Eroded $bRaw $W $H
    $emisMask  = Get-Eroded $cRaw $W $H

    # L'INVERSION NE PORTE QUE SUR LES DEUX POPULATIONS COMPAREES : echanger la
    # flamme avec l'une d'elles ne testerait pas le masque, il testerait
    # l'emission.
    if ($InvertMask) { $swap = $metalMask; $metalMask = $woodMask; $woodMask = $swap }

    $rows = @()
    foreach ($pop in @(@{ nom = 'ferrures (metallic)'; m = $metalMask },
                       @{ nom = 'bois (dielectrique)'; m = $woodMask },
                       @{ nom = 'flamme (emissive)';   m = $emisMask })) {
        $m = $pop.m
        $n = 0; $s = 0.0; $s2 = 0.0; $grad = 0.0
        for ($y = 1; $y -lt $H - 1; $y++) {
            $row = $y * $W
            for ($x = 1; $x -lt $W - 1; $x++) {
                $i = $row + $x
                if (-not $m[$i]) { continue }
                $l = $lum[$i]
                $n++; $s += $l; $s2 += $l * $l
                # Sobel pris DANS la population : un gradient au travers d'une
                # frontiere de materiau mesurerait la frontiere.
                $gx = ($lum[$i - $W + 1] + 2 * $lum[$i + 1] + $lum[$i + $W + 1]) -
                      ($lum[$i - $W - 1] + 2 * $lum[$i - 1] + $lum[$i + $W - 1])
                $gy = ($lum[$i + $W - 1] + 2 * $lum[$i + $W] + $lum[$i + $W + 1]) -
                      ($lum[$i - $W - 1] + 2 * $lum[$i - $W] + $lum[$i - $W + 1])
                $grad += [Math]::Sqrt($gx * $gx + $gy * $gy)
            }
        }
        # Une flamme absente d'une vue est un fait de cadrage, pas une erreur ;
        # les deux populations de surface, elles, doivent exister.
        if ($n -eq 0) {
            if ($pop.nom -like 'flamme*') { continue }
            throw "Population vide pour $Tag : $($pop.nom)"
        }
        $moy = $s / $n
        $var = [Math]::Max(0.0, ($s2 / $n) - ($moy * $moy))
        $rows += [pscustomobject]@{
            Vue        = $Tag
            Population = $pop.nom
            Pixels     = $n
            Moyenne    = [Math]::Round($moy, 4)
            EcartType  = [Math]::Round([Math]::Sqrt($var), 4)
            Sobel      = [Math]::Round($grad / $n, 4)
        }
    }
    return $rows
}

if (-not $CompareOnly) { Invoke-Capture }

$echecs = 0
$tous = @()
foreach ($v in $Views) {
    $tag = Get-ViewTag $v
    foreach ($suffixe in @('pbr', 'metallic', 'normal', 'emissive', 'fixe')) {
        $png = Join-Path $OutDir "${tag}_$suffixe.png"
        if (-not (Test-Path $png)) { Write-Warning "Capture absente : $png"; exit 1 }
    }
    $tous += Measure-View $tag
}

$tous | Format-Table Vue, Population, Pixels, Moyenne, EcartType, Sobel -AutoSize |
    Out-String | Write-Host

foreach ($v in $Views) {
    $tag = Get-ViewTag $v
    $m = $tous | Where-Object { $_.Vue -eq $tag -and $_.Population -like 'ferrures*' }
    $b = $tous | Where-Object { $_.Vue -eq $tag -and $_.Population -like 'bois*' }
    $ecart = [Math]::Round($b.Moyenne - $m.Moyenne, 4)

    # LE CHEMIN PBR SEPARE-T-IL ENCORE LES DEUX MATERIAUX ? C'est l'assertion.
    # Sur Lantern, le metal est du fer sombre : sa reflectance de base vaut 0,068
    # contre un albedo de 0,298 pour le bois. Un fragment qui cesserait de lire
    # le canal metallique -- uniforme non pose, unite mal liee, carte MR absente
    # -- rendrait les deux populations a la meme luminance, et l'ecart tomberait.
    $seuil = 0.03
    $ok = if ($InvertMask) { $ecart -lt (-1 * $seuil) } else { $ecart -gt $seuil }

    if (-not $ok) { $echecs++ }
    $etat = if ($ok) { 'ok' } else { 'ECHEC' }
    Write-Host ("{0,-14} bois - ferrures {1,8}   ecart-type metal {2,7}   {3}" -f `
                $tag, $ecart, $m.EcartType, $etat) `
               -ForegroundColor $(if ($ok) { 'Green' } else { 'Red' })
}

Write-Host ""
Write-Host "Images dans $OutDir"
if ($InvertMask) {
    Write-Host "MASQUE INVERSE : le signe de l'ecart doit s'etre inverse. C'est la validation du detecteur."
}
if ($echecs -gt 0) {
    Write-Host "$echecs vue(s) sans separation des deux materiaux." -ForegroundColor Red
    exit 1
}
Write-Host "Le chemin PBR separe les deux populations sur toutes les vues."
exit 0
