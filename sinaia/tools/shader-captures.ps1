<#
.SYNOPSIS
    Captures de reference pour la bascule du rendu de surface par shader.

.DESCRIPTION
    Produit, pour chaque combinaison (modele x point de vue x mode d'ombrage),
    une PAIRE d'images : pipeline fixe et shader. Puis les compare deux a deux.

    POURQUOI CE SCRIPT. cgre n'a aucun test de rendu, et la seule facon de juger
    un shader reste de le regarder. Le regarder « de memoire », en basculant a la
    main sur deux ou trois modeles, ne dit rien de reproductible : c'est
    exactement ainsi que le defaut de shininess a survecu des annees sans etre
    vu. Ici la camera est deterministe (camera azel), les bascules d'affichage
    sont idempotentes, et l'ecart entre les deux images est CHIFFRE.

    CE QU'ON CHERCHE. Pas l'egalite : le passage de Gouraud a Phong change le
    rendu, c'est l'objet du chantier. Les speculaires deviennent nets et
    apparaissent la ou l'interpolation par sommet les ratait. On cherche les
    ecarts NON EXPLIQUES -- une texture absente, un reflet deplace, une face
    noire -- qui se voient a un pourcentage de pixels differents anormalement
    eleve, ou concentre sur une seule combinaison.

    UN TEMOIN EST TOUJOURS AJOUTE a la serie : test/data/obj/cube-tex-spec.obj,
    seul OBJ TEXTURE du depot dont la couleur eclairee depasse 1,0. Il couvre le
    regime de sur-eclairement, ou le pipeline fixe ecrete AVANT l'etage de
    texturage et ou un shader qui ne le ferait pas teinterait le depassement au
    lieu de le perdre. Il n'est capture qu'en mode `materials` : les deux autres
    modes passent par ActivateNeutralMaterial, qui ne pose ni texture ni
    speculaire, et n'exerceraient donc rien.

    CE TEMOIN EST BISTABLE (reserve R10) : son rendu en PIPELINE FIXE change d'une
    session a l'autre, normales plates ou moyennees. Toute serie l'embarquant exige
    donc l'appariement des captures -- ne comparer deux series que si leurs images
    `_fixe` sont identiques au bit pres.

.EXAMPLE
    # sinaia doit tourner. Produit les paires puis les compare.
    .\shader-captures.ps1

.EXAMPLE
    .\shader-captures.ps1 -OutDir C:\tmp\ref -Models "C:\chemin\a.glb","C:\chemin\b.glb"

.EXAMPLE
    # Comparer une serie deja produite, sans recapturer.
    .\shader-captures.ps1 -CompareOnly
#>
[CmdletBinding()]
param(
    # Laisse vide : $PSScriptRoot n'est pas encore peuple dans un bloc param()
    # sous Windows PowerShell 5.1. Le defaut est calcule dans le corps.
    [string]   $OutDir = '',
    [string[]] $Models = @(
        'C:\home\perso\cg\test\data\Duck.glb',
        'C:\home\perso\cg\test\data\pbr_sphere.glb'
    ),
    # Points de vue, en degres : azimut (lacet) puis elevation (tangage).
    # Trois suffisent : de face, trois quarts, et plongee -- ce dernier montre le
    # dessus, ou les normales retournees (gl_FrontFacing) se voient le mieux.
    [int[][]] $Views = @( @(0, 0), @(35, 20), @(120, 55) ),
    [string[]] $Modes = @('materials', 'neutral', 'vertexcolors'),
    # ECLAIRAGE ETEINT AUTANT QU'ALLUME, par defaut et non sur demande.
    #
    # C'est le regime ou le pipeline fixe LIT le tableau de couleurs par sommet :
    # sous eclairage il l'ignore, GL_COLOR_MATERIAL etant desactive. Une serie
    # limitee a `on` ne mesure donc jamais ce que la liaison du tableau decide --
    # et Mesh::InitVertices donne un tableau a TOUT maillage, gris 0,5.
    #
    # NOMMAGE ASYMETRIQUE, et c'est delibere : `on` garde les noms de fichiers
    # historiques, `off` recoit le suffixe `_nolight`. Renommer les premiers
    # aurait rendu incomparable un corpus de captures que la reserve R10 rend
    # couteux a reconstituer -- plusieurs sessions par serie appariee.
    [string[]] $Lighting = @('on', 'off'),
    [int]      $Port = 7777,
    [switch]   $CompareOnly
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$scriptDir = if ($PSScriptRoot) { $PSScriptRoot }
             else { Split-Path -Parent $MyInvocation.MyCommand.Path }

$console = Join-Path $scriptDir 'sinaia-console.ps1'
if (-not (Test-Path $console)) { throw "Client de console introuvable : $console" }

if ([string]::IsNullOrWhiteSpace($OutDir)) {
    $OutDir = Join-Path $scriptDir '..\..\build\mat\sinaia\Release\captures'
}
$OutDir = [System.IO.Path]::GetFullPath($OutDir)
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

# TEMOIN DE SUR-ECLAIREMENT, ajoute a TOUTE serie -- y compris a une serie
# passee par -Models, et c'est voulu. Dans un materiau texture, import_mtl force
# Ka et Kd a blanc et ne reporte du MTL que Ks : cube-tex-spec.obj est le seul
# OBJ TEXTURE du depot dont la couleur eclairee depasse 1,0, donc le seul ou une
# divergence sur l'ECRETAGE AVANT TEXTURAGE se voie sur ce chemin. Sans lui, la
# serie rend « aucun ecart » sur un regime qu'elle n'a pas visite.
#
# Les chemins sont NORMALISES avant comparaison : sans cela, un appelant qui
# nomme le temoin par un chemin relatif ou non canonique le ferait capturer deux
# fois.
$witness     = [System.IO.Path]::GetFullPath(
                   (Join-Path $scriptDir '..\..\test\data\obj\cube-tex-spec.obj'))
$witnessAuto = $false
$Models = @(@($Models) | ForEach-Object {
    try { [System.IO.Path]::GetFullPath($_) } catch { $_ }
})
if (Test-Path $witness) {
    if ($Models -notcontains $witness) {
        $Models      = @($Models) + $witness
        $witnessAuto = $true
    }
}
elseif (-not $CompareOnly) {
    # Un temoin qui disparait en silence est EXACTEMENT le defaut qu'il existe
    # pour empecher : la serie rendrait « aucun ecart » sur un regime qu'elle
    # n'a pas visite. Avertissement et non erreur -- le reste de la serie garde
    # sa valeur, c'est sa COUVERTURE qui est amputee.
    Write-Warning ("Temoin introuvable : $witness")
    Write-Warning ("La serie NE COUVRE PAS le regime de couleur eclairee > 1,0 : " +
                   "un ecretage manquant avant texturage n'y sera pas detecte.")
}

# ---------------------------------------------------------------------------
# 1. Capture
# ---------------------------------------------------------------------------
function Invoke-Capture {
    $commands = New-Object System.Collections.Generic.List[string]

    # Le controle d'erreur GL reste allume pendant toute la serie : si un uniforme
    # est mal pose ou une texture invalide, la fenetre de log le dira au lieu de
    # nous laisser interpreter une image bizarre.
    $commands.Add('glcheck on')

    # R23 : SURBRILLANCE DE SURVOL COUPEE AVANT TOUTE CAPTURE. Les aretes
    # jaunes de l'AABB du modele sous le curseur entrent dans l'image comme
    # n'importe quel pixel : une serie en a rendu 18 appariables sur 24 pour
    # cette seule raison, et l'appariement R10 echoue alors sans rien dire.
    # Une commande, et non un deplacement de curseur, qui dependrait de la
    # geometrie de l'ecran.
    $commands.Add('hover off')

    foreach ($model in $Models) {
        if (-not (Test-Path $model)) { Write-Warning "Modele absent, ignore : $model"; continue }
        $name = [System.IO.Path]::GetFileNameWithoutExtension($model)

        # `open` cree un NOUVEL onglet : la serie ne depend donc pas de ce qui
        # etait affiche avant, et deux modeles ne se melangent pas.
        $commands.Add("open $model")

        # Etat d'affichage FIXE et explicite. Les surcouches restent en
        # fixe-fonction quel que soit l'etat du shader : les laisser allumees
        # ajouterait du bruit identique des deux cotes, mais masquerait la surface.
        $commands.Add('toggle fill on')
        foreach ($o in @('wireframe', 'points', 'warning', 'repere', 'grid')) {
            $commands.Add("toggle $o off")
        }

        # Le temoin ajoute d'office ne se capture qu'en `materials` : les deux
        # autres modes passent par ActivateNeutralMaterial, qui ne pose ni
        # texture ni speculaire, donc n'exercent pas le regime pour lequel il
        # existe. `materials` s'impose meme si -Modes ne le demande pas, sinon
        # le temoin ne couvrirait rien. Un temoin nomme explicitement par
        # l'appelant garde, lui, les modes demandes.
        $modesForModel = if ($witnessAuto -and $model -eq $witness) { @('materials') }
                         else { $Modes }

        foreach ($view in $Views) {
            $az, $el = $view[0], $view[1]
            $commands.Add('camera reset')
            $commands.Add("camera azel $az $el")

            foreach ($mode in $modesForModel) {
                $commands.Add("shading $mode")

                foreach ($light in $Lighting) {
                    $commands.Add("toggle lighting $light")
                    $suffixe = if ($light -eq 'off') { '_nolight' } else { '' }
                    $stem = "{0}_az{1}_el{2}_{3}{4}" -f $name, $az, $el, $mode, $suffixe

                    # L'ordre compte : off puis on, sans rien changer d'autre
                    # entre les deux. C'est ce qui fait de la paire une
                    # comparaison et non deux images sans rapport.
                    $commands.Add('shader off')
                    $commands.Add("screenshot $(Join-Path $OutDir ($stem + '_fixe.png'))")
                    $commands.Add('shader on')
                    $commands.Add("screenshot $(Join-Path $OutDir ($stem + '_shader.png'))")
                }
                # L'eclairage est une bascule de VUE, pas de modele : le laisser
                # eteint contaminerait la combinaison suivante.
                $commands.Add('toggle lighting on')
            }
        }
        $commands.Add('shader off')
    }

    Write-Host "Capture : $($commands.Count) commandes vers 127.0.0.1:$Port" -ForegroundColor Cyan
    # Liaison EXPLICITE au parametre -Command. Passe en positionnel, le tableau
    # etait aplati en une seule chaine par ValueFromRemainingArguments, et les
    # 119 commandes partaient comme une seule ligne -- dont sinaia ne lisait que
    # le premier mot.
    $reply = & $console -Port $Port -Command $commands.ToArray()
    $reply | Write-Verbose

    $errors = @(@($reply) | Where-Object { $_ -match '^ERR ' })
    if ($errors.Count -gt 0) {
        Write-Warning "$($errors.Count) commande(s) refusee(s) :"
        $errors | ForEach-Object { Write-Warning "  $_" }
    }
}

# ---------------------------------------------------------------------------
# 2. Comparaison
# ---------------------------------------------------------------------------
# Deux mesures, parce qu'elles ne disent pas la meme chose :
#   - le POURCENTAGE de pixels qui different d'au moins un seuil perceptible dit
#     l'ETENDUE de l'ecart (une texture absente touche toute la silhouette) ;
#   - l'ecart MAXIMAL dit son INTENSITE (un speculaire deplace touche peu de
#     pixels, mais violemment).
# Un ecart large et faible est attendu (Phong). Un ecart etroit et violent, ou
# large ET violent, demande un coup d'oeil.
function Compare-Pairs {
    Add-Type -AssemblyName System.Drawing

    # @() : sans lui, une serie vide rend $null et .Count leve sous StrictMode.
    $pairs = @(Get-ChildItem -Path $OutDir -Filter '*_fixe.png' -ErrorAction SilentlyContinue | Sort-Object Name)
    if ($pairs.Count -eq 0) { Write-Warning "Aucune paire dans $OutDir"; return }

    $rows = foreach ($fixe in $pairs) {
        $shader = $fixe.FullName -replace '_fixe\.png$', '_shader.png'
        if (-not (Test-Path $shader)) {
            Write-Warning "Sans pendant : $($fixe.Name)"
            continue
        }

        $a = [System.Drawing.Bitmap]::FromFile($fixe.FullName)
        $b = [System.Drawing.Bitmap]::FromFile($shader)
        try {
            if ($a.Width -ne $b.Width -or $a.Height -ne $b.Height) {
                [pscustomobject]@{ Cas = $fixe.BaseName -replace '_fixe$',''
                                   Differents = 'TAILLES DIFFERENTES'; EcartMax = '-' }
                continue
            }

            $differing = 0; $maxDelta = 0; $total = $a.Width * $a.Height
            # Un pas de 2 px suffit pour caracteriser un ecart et divise le temps
            # par quatre : on cherche un ordre de grandeur, pas une signature.
            for ($y = 0; $y -lt $a.Height; $y += 2) {
                for ($x = 0; $x -lt $a.Width; $x += 2) {
                    $pa = $a.GetPixel($x, $y); $pb = $b.GetPixel($x, $y)
                    $d = [Math]::Max([Math]::Abs($pa.R - $pb.R),
                         [Math]::Max([Math]::Abs($pa.G - $pb.G), [Math]::Abs($pa.B - $pb.B)))
                    if ($d -gt $maxDelta) { $maxDelta = $d }
                    if ($d -gt 8) { $differing++ }   # 8/255 : sous le seuil de l'oeil
                }
            }
            $sampled = [Math]::Ceiling($a.Height / 2.0) * [Math]::Ceiling($a.Width / 2.0)
            [pscustomobject]@{
                Cas        = $fixe.BaseName -replace '_fixe$',''
                Differents = '{0,6:N2} %' -f (100.0 * $differing / $sampled)
                EcartMax   = $maxDelta
            }
        }
        finally { $a.Dispose(); $b.Dispose() }
    }

    $rows | Format-Table -AutoSize
    Write-Host "Images dans $OutDir" -ForegroundColor Cyan
    Write-Host "Regarder a l'oeil toute ligne dont l'ecart est large ET violent." -ForegroundColor DarkGray
}

if (-not $CompareOnly) { Invoke-Capture }
Compare-Pairs
