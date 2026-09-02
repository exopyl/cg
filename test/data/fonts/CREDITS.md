# Polices du dépôt — provenance et licences

Ce dossier est la **source unique** des polices à contours du projet : fixtures
des tests unitaires (`tu_cgmath_font`, `tu_cggraph_nodes`, …) *et* catalogue
proposé par la page « Texte 3D » du maker, qui les reçoit par copie de build
(cf. `maker/CMakeLists.txt`). Le manifeste lu par le navigateur est
`catalogue.json`.

**Toute police déposée ici doit être redistribuable** — OFL 1.1, Apache 2.0 ou
domaine public. Ce n'est pas une préférence : le build les sert au navigateur,
donc les ajouter revient à les redistribuer. Le texte de licence correspondant
va dans `licenses/`, l'OFL 1.1 l'exige explicitement.

| Fichier | Famille amont | Auteur | Licence | Texte |
|---|---|---|---|---|
| `Roboto.ttf` | `google/fonts` `ofl/roboto` — `Roboto[wdth,wght].ttf` | Christian Robertson | OFL 1.1 | `licenses/roboto.txt` |
| `OpenSans.ttf` | `google/fonts` `ofl/opensans` — `OpenSans[wdth,wght].ttf` | Steve Matteson | OFL 1.1 | `licenses/opensans.txt` |
| `NotoSans.ttf` | `google/fonts` `ofl/notosans` — `NotoSans[wdth,wght].ttf` | Google | OFL 1.1 | `licenses/notosans.txt` |
| `RobotoSlab.ttf` | `google/fonts` `apache/robotoslab` — `RobotoSlab[wght].ttf` | Christian Robertson | Apache 2.0 | `licenses/robotoslab.txt` |
| `PlayfairDisplay.ttf` | `google/fonts` `ofl/playfairdisplay` — `PlayfairDisplay[wght].ttf` | Claus Eggers Sørensen | OFL 1.1 | `licenses/playfairdisplay.txt` |
| `Cinzel.ttf` | `google/fonts` `ofl/cinzel` — `Cinzel[wght].ttf` | Natanael Gama | OFL 1.1 | `licenses/cinzel.txt` |
| `UnifrakturMaguntia.ttf` | `google/fonts` `ofl/unifrakturmaguntia` — `UnifrakturMaguntia-Book.ttf` | j. 'mach' wust, P. Wiegel | OFL 1.1 | `licenses/unifrakturmaguntia.txt` |
| `BebasNeue.ttf` | `google/fonts` `ofl/bebasneue` — `BebasNeue-Regular.ttf` | Ryoichi Tsunekawa | OFL 1.1 | `licenses/bebasneue.txt` |
| `Lobster.ttf` | `google/fonts` `ofl/lobster` — `Lobster-Regular.ttf` | Pablo Impallari | OFL 1.1 | `licenses/lobster.txt` |
| `GreatVibes.ttf` | `google/fonts` `ofl/greatvibes` — `GreatVibes-Regular.ttf` | TypeSETit | OFL 1.1 | `licenses/greatvibes.txt` |
| `JetBrainsMono.ttf` | `google/fonts` `ofl/jetbrainsmono` — `JetBrainsMono[wght].ttf` | JetBrains | OFL 1.1 | `licenses/jetbrainsmono.txt` |
| `Inconsolata.ttf` | `google/fonts` `ofl/inconsolata` — `Inconsolata[wdth,wght].ttf` | Raph Levien | OFL 1.1 | `licenses/inconsolata.txt` |
| `FiraSans.otf` | `mozilla/Fira` — `otf/FiraSans-Regular.otf` | Erik Spiekermann, Ralph du Carrois | OFL 1.1 | `licenses/firasans.txt` |
| `BloomingGrove.otf` | — (déjà présente) | Nathan Eady | domaine public | — |
| `DejaVuSans.ttf` | `dejavu-fonts/dejavu-fonts` | Bitstream / DejaVu / Arev | Bitstream Vera + domaine public | `licenses/dejavusans.txt` |

## Renommage volontaire

Les fichiers Google Fonts ont perdu leur suffixe d'axes (`Roboto[wdth,wght].ttf`
→ `Roboto.ttf`) pour deux raisons :

1. `[` et `]` sont **réservés** par la RFC 3986 dans un chemin d'URL ; ces
   fichiers sont servis par HTTP au navigateur.
2. Le suffixe annonce une sélection d'axes que le moteur **ne sait pas faire**
   (voir ci-dessous) — un nom qui promet plus que le code ne tient.

La colonne « Famille amont » ci-dessus conserve le nom d'origine, de quoi
re-télécharger ou vérifier une mise à jour.

## Variable fonts : instance par défaut uniquement

Google Fonts ne publie plus que des **variable fonts** (table `fvar`). Or
`cgmath/font.h` est une couche mince sur `stb_truetype`, qui **n'interprète pas
`fvar`** : la table `glyf` d'un fichier variable contient les contours de
l'instance par défaut, et c'est celle-là — le Regular — qui est lue. Demander le
Bold ou le Light du même fichier ne marchera pas.

Pour de vraies instances statiques, le dépôt `google/fonts` ne sert à rien (il ne
stocke que le fichier variable) : passer par
[google-webfonts-helper](https://gwfh.mranftl.com/fonts), qui sert du TTF
instancié, ou par la release amont de la famille.

## Formats

`font.h` accepte TrueType (`.ttf`, table `glyf`, quadratiques), OpenType/CFF
(`.otf`, charstrings Type 2, cubiques) et les collections `.ttc`/`.otc`
indexées. Il **refuse** WOFF/WOFF2 (tables compressées) — donc ni Fontsource ni
le CDN `fonts.gstatic.com`, qui ne servent que du WOFF2.

Les deux chemins de contours restent couverts par le catalogue : `FiraSans.otf`
et `BloomingGrove.otf` sont en CFF cubique, tout le reste en `glyf` quadratique.
Aucun `.ttc`/`.otc` redistribuable ici — les collections du système
(`C:\Windows\Fonts\*.ttc`, Noto CJK) conviennent pour un essai local mais ne
peuvent pas devenir des fixtures.
