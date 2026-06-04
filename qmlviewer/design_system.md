# QmlViewer — Design System

Système de design de l'interface QmlViewer (Qt Quick). Adapté de la maquette
HTML/React `Design/Dock complet - Détaillé(.html)` (sources `hifi-app.jsx`,
`hifi-icons.jsx`, `tweaks-panel.jsx`).

> **Portée.** Reskin de la mise en page existante (arbre de dossiers · viewport
> Vulkan · bande de fichiers) avec le langage visuel de la maquette, plus un
> dock flottant, un **panneau d'outil ancré à droite** (toujours présent,
> ouvert par défaut sur la *Fiche technique*) et une légende. Fonctionnels :
> *Fiche technique*, *Courbure* (Desbrun) et *Mesures → Point* (picking de
> surface). *Comparaison / Épaisseur / Coupe* (et les autres modes de Mesures)
> restent des **aperçus UI**.

## Où ça vit

| Élément | Fichier |
| :-- | :-- |
| Tokens (couleurs, tailles, polices), icônes, métadonnées des outils | `qml/Theme.qml` *(singleton QML)* |
| Icône ligne 24px (rendu SVG via `QtQuick.Shapes`) | `qml/AppIcon.qml` |
| Surface flottante translucide réutilisable | `qml/GlassPanel.qml` |
| Dock flottant (contrôles de vue + outils) | `qml/AnalysisDock.qml` |
| Panneau d'outil **ancré à droite** (toujours présent) + contrôles (slider/toggle/seg/select/file) ou fiche lecture seule | `qml/AnalysisPanel.qml` |
| Carte de légende (heatmap / lecture de valeur) | `qml/LegendCard.qml` |
| Composition, chrome, overlays viewport | `qml/Main.qml` |

`Theme` est un singleton : on y accède via `import QmlViewer` puis `Theme.<token>`.
Il est déclaré singleton dans `qmlviewer/CMakeLists.txt`
(`set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)`).

---

## 1. Couleurs

**Deux thèmes : sombre (nuit) et clair (jour)**, basculés par `Theme.dark`
(bouton sun/moon du chrome). Chaque token couleur est un *binding* sur `dark`
— le changer restyle toute l'UI. L'**accent orange** et `ok`/`warn`/`danger`
sont **partagés** par les deux thèmes ; la densité compacte de la maquette
n'est pas portée. Les surfaces « verre » sont translucides — format Qt
`#AARRGGBB` (alpha en tête). Valeurs claires reprises de
`[data-theme="light"]` de la maquette.

### Fonds & surfaces
| Token | Valeur | Équivalent rgba | Usage |
| :-- | :-- | :-- | :-- |
| `bg` | `#17181B` | — | Fond fenêtre / barre de titre |
| `viewport` | `#141519` | — | Clear du viewport 3D / fond bande de fichiers |
| `panel` | `#1B1D22` | — | Panneaux latéraux pleins (arbre, tuiles) |
| `surface` | `#DB222429` | `rgba(34,36,41,.86)` | Cartes/panneaux flottants (verre) |
| `surface2` | `#E62C2F35` | `rgba(44,47,53,.90)` | Champs, états hover, boutons secondaires |
| `surfaceSolid` | `#212329` | — | Tooltips (opaque) |

### Traits (borders / dividers)
| Token | Valeur | rgba blanc | Usage |
| :-- | :-- | :-- | :-- |
| `stroke` | `#1AFFFFFF` | `.10` | Bordure standard des surfaces |
| `stroke2` | `#12FFFFFF` | `.07` | Dividers fins |
| `strokeStrong` | `#2EFFFFFF` | `.18` | Bordure accentuée, rails de slider, hover de champ |

### Texte
| Token | Valeur | Usage |
| :-- | :-- | :-- |
| `text` | `#ECEAE5` | Texte principal |
| `textSoft` | `#A3A097` | Texte secondaire, libellés |
| `textFaint` | `#6E6C66` | Texte estompé, hints, placeholders |

### Accent & sémantique
| Token | Valeur | rgba | Usage |
| :-- | :-- | :-- | :-- |
| `accent` | `#FF7A1A` | — | Couleur d'accent (orange), sélection, primaire |
| `accentSoft` | `#29FF7A1A` | `rgba(255,122,26,.16)` | Fond d'accent doux (chip d'icône, sélection arbre) |
| `accentGlow` | `#66FF7A1A` | `rgba(255,122,26,.40)` | Lueur (réservé pour ombres d'accent) |
| `ok` | `#33B07A` | — | Statut OK (point de vie, résultat) |
| `warn` | `#E8B23A` | — | Avertissement (résultat) |
| `danger` | `#E94034` | — | Erreur, bouton fermeture survol |

### Divers
- `gridLine` : `#22FFFFFF` (token défini). La grille HUD du viewport est
  effectivement dessinée au Canvas à **`rgba(1,1,1,0.06)`** puis atténuée au
  centre (cf. §7).

---

## 2. Typographie

| Token | Famille | Remarque |
| :-- | :-- | :-- |
| `fontSans` | `IBM Plex Sans` | UI générale |
| `fontMono` | `IBM Plex Mono` | Valeurs numériques, chemins, raccourcis, gizmo |

> **IBM Plex n'est pas embarqué.** Si la police n'est pas installée, Qt retombe
> sur une police système. Installer *IBM Plex Sans* et *IBM Plex Mono* pour un
> rendu identique à la maquette.

### Échelle (tailles en `font.pixelSize`)
| Contexte | Taille | Graisse / style |
| :-- | :-- | :-- |
| Titre de panneau (`ph-t`) | 16 | DemiBold |
| Nom du maillage (stats) | 15 | DemiBold |
| Lecture de légende (gros chiffre) | 30 | DemiBold, mono |
| Texte UI courant | 13 | Regular/Medium |
| Barre de titre, libellé de menu | 13 | Medium pour la marque |
| Libellé de champ (`ctl-lbl`) | 11 | DemiBold, **MAJUSCULES**, `letterSpacing 0.6` |
| Titre de légende | 11 | DemiBold, MAJUSCULES, `letterSpacing 0.6` |
| Tooltip du dock | 12 | Medium |
| Valeurs mono (slider, ends, stats, source) | 10–12 | mono |
| Raccourci clavier (`kbd`) | 10 | mono |
| Badge d'extension (tuile) | 8 | Bold |

---

## 3. Géométrie & espacement

| Token | Valeur | Usage |
| :-- | :-- | :-- |
| `rLg` | 16 | Rayon des grands panneaux / dock |
| `rMd` | 11 | Rayon moyen (boutons dock, cartes, ghost) |
| `rSm` | 8 | Rayon petit (champs, tuiles, boutons de pied) |
| `dockPad` | 8 | Padding interne du dock |
| `dockBtn` | 46 | Côté d'un bouton du dock |
| `dockIcon` | 23 | Taille d'icône dans le dock |
| `panelWidth` | 312 | Largeur du panneau de paramètres |
| `panelPad` | 18 | Padding du panneau |
| `fieldGap` | 16 | Espacement vertical entre champs |

Marges de chrome flottant : **20 px** depuis les bords du viewport. Légende
remontée de **88 px** (au-dessus du gizmo). Hint de repos à **26 px** du bas.

---

## 4. Surfaces « verre » (glassmorphisme)

`GlassPanel` = `Rectangle` translucide arrondi avec bordure fine.

> **Approximation assumée.** Le vrai *backdrop blur* n'est pas réalisable à bas
> coût au-dessus de l'item Vulkan. On simule l'effet givré par un fond
> semi-opaque (`surface`) + bordure (`stroke`). Pas de flou réel ni d'ombre
> portée par défaut.

Propriétés : `fill` (défaut `surface`), `stroke` (défaut `stroke`),
`strokeWidth` (défaut 1), `radius` (défaut `rLg`).

---

## 5. Iconographie

Icônes ligne dessinées dans un viewBox `0 0 24 24`, trait `1.7`, bouts/joints
arrondis, `stroke = currentColor`, sans remplissage. Rendu par `AppIcon` via
`QtQuick.Shapes` (`ShapePath` + `PathSvg`), mis à l'échelle vers `size`.

`AppIcon` : `name` (clé), `size` (défaut 22), `color` (défaut `textSoft`),
`sw` (épaisseur, défaut 1.7). Les chemins sont dans `Theme.icons` ;
`Theme.iconPath(name)` retombe sur `"target"` si la clé est absente.

**Jeu d'icônes** (`Theme.icons`) :
`fit`, `orbit`, `grid`, `ruler`, `curvature`, `compare`, `measure`,
`thickness`, `section`, `home`, `rotate`, `export`, `settings`, `share`,
`chevron`, `close`, `warning`, `check`, `layers`, `reset`, `target`, `open`,
`spec`, `sun`, `moon`.

---

## 6. Composants

### GlassPanel
Surface de base (cf. §4).

### AppIcon
Icône vectorielle (cf. §5).

### AnalysisDock
Dock flottant centré en haut. Deux groupes séparés par un divider :
1. **Contrôles de vue** : `home` (Recadrer), `orbit` (Orbite), `grid` (Grille, état `on`).
2. **Outils d'analyse** : un bouton par entrée de `Theme.tools` (icône + tooltip + raccourci).

États du bouton (`DockBtn`) : *normal* (transparent), *hover* (`surface2`),
*on* (contrôle de vue activé — `surface2` + bordure `stroke`), *active* (outil
sélectionné — fond `accent`, icône blanche). Tooltip sous le bouton
(`surfaceSolid`, libellé + chip `kbd`). Animation d'enfoncement `scale 0.94`.

Signaux : `fitClicked`, `orbitClicked`, `gridToggled`, `toolToggled(string id)`.
Propriétés : `activeTool`, `gridOn`.

### AnalysisPanel
Panneau d'outil **ancré** : il remplit le volet droit (toujours présent — il ne
flotte plus au-dessus du viewport), et son contenu défile via un `Flickable` +
scrollbar discrète si la hauteur déborde. Structure : en-tête (chip d'icône
`accentSoft` + titre + chip `kbd`, **sans bouton fermer**), corps (blurb +
contrôles générés depuis `tool.params`, **ou** lignes label/valeur pour
`kind: "spec"`), bandeau de résultat optionnel (`ok`/`warn`), pied
(Réinitialiser / Évaluer ou Exporter — masqué pour la fiche).

Signaux : `paramChanged(string key, var value)`, `resetClicked`,
`exportClicked`, `evaluateClicked`. Propriétés : `tool`, `params`, `specRows`,
`isSpec`, `evaluable`.

**Contrôles** (sélectionnés par `param.t`) :
| Type | Widget | Comportement |
| :-- | :-- | :-- |
| `slider` | `SliderWidget` | Rail 5px (`strokeStrong`) + remplissage `accent` + bouton ; valeur mono à droite ; pas via `step` ; format virgule décimale si `step < 1` |
| `toggle` | `ToggleWidget` | Interrupteur 42×24, `accent` quand activé, bouton animé |
| `seg` | `SegWidget` | Segments (`RowLayout`), option active sur fond `accent` |
| `select` | `SelectWidget` | Liste déroulante (`Popup`) : ouvre la liste des options, coche l'option courante, chevron pivotant |
| `file` | `FileWidget` | Aperçu de fichier (icône `layers` + nom mono) + « Changer » *(stub)* |

`FootBtn` : variante `primary` (fond `accent`, blanc) ou secondaire (`surface2`
+ bordure). En pied, *Réinitialiser* prend la largeur minimale, *Exporter* remplit.

### LegendCard
Carte bas-gauche. Deux formes selon `tool.legend.kind` :
- **`scale`** : titre + unité estompée, barre de dégradé (Canvas, `legend.stops`),
  bornes gauche/milieu/droite en mono.
- **`readout`** : gros chiffre mono (30px) + unité + sous-texte. Valeur dynamique
  pour `section` (`5 + position% × 40`) ; sous-texte dynamique
  (`Plan X · contour visible/masqué`).

Propriétés : `tool`, `params`. Entrée en fondu (220 ms).

---

## 7. Mise en page & chrome

```
┌──────────────── barre de titre (frameless) ──────────────────┐
│ ◆ QmlViewer  File               QmlViewer…          _ ▢ ✕    │
├───────┬────────────────────────────────────┬─────────────────┤
│ arbre │        [ dock ]          [⤴ ⚙]     │  Fiche technique │
│  📂   │                                    │  ───────────────  │
│  📁   │        ◆ maillage 3D (Vulkan)      │  panneau d'outil  │
│       │ [légende]                          │  ancré (défaut =  │
│       │   ⌖ gizmo                          │  fiche technique) │
├───────┴────────────────────────────────────┴─────────────────┤
│ Dossier : …        [ tuiles .obj .stl .ply … ]                │
└────────────────────────────────────────────────────────────────┘
```

Le haut de la fenêtre est un `SplitView` horizontal **à trois volets** :
arbre de dossiers (gauche) · viewport (centre, extensible) · **panneau d'outil
(droite)**. Les trois sont redimensionnables ; le viewport prend l'espace
restant, donc le modèle 3D est centré dans la zone réellement visible.

- **Barre de titre** : frameless, déplacement/redimensionnement natifs via
  `winCtrl` ; marque `◆` en `accent` ; menu *File* (Open…/Quit) ; boutons
  min/max/close (glyphes Segoe MDL2, fermeture survol = `danger`).
- **Arbre de dossiers** (gauche) : `panel`, sélection sur `accentSoft` + bordure,
  hover `surface2`, chevrons ▸/▾.
- **Viewport** : item Vulkan `CgreQuickItem` en couche basse (gère la souris).
  Overlays au-dessus, **transparents à la souris** (sans handler) :
  - **Grille HUD** (Canvas, ~`rgba(1,1,1,0.06)`, pas 40px) atténuée au centre
    (composite `destination-out` radial) pour ne pas salir le modèle ; togglée
    par `gridOn`.
  - **Vignette** (dégradé radial, centre transparent → bords `rgba(0,0,0,.42)`).
  - **Gizmo d'axes** XYZ bas-gauche (Canvas) : projette les axes monde via
    `CgreQuickItem.axisTransform` (caméra × trackball) et **tourne avec le
    modèle** ; axes colorés (X rouge / Y vert / Z bleu), tri en profondeur et
    atténuation des axes orientés vers l'arrière.
- *(plus de carte de stats flottante : les caractéristiques du maillage — et
  l'erreur de chargement éventuelle — sont fournies par l'outil « Fiche
  technique », cf. §8.)*
- **Boutons (haut-droite)** : `share` (*stub*) et le **bouton sun/moon** qui
  bascule jour/nuit (`Theme.dark = !Theme.dark`) — icône `sun` en mode sombre
  (cliquer → jour), `moon` en mode clair (cliquer → nuit).
- **Hint** : message « idle » centré quand aucun maillage n'est chargé.
- **Bande de fichiers** (bas, fond `panel` comme les volets latéraux) :
  - **en-tête** : icône `open` + libellé `Dossier` en style `ctl-lbl`
    (MAJUSCULES, `textSoft`), chemin mono `textFaint` élidé, et **compteur de
    fichiers** à droite ;
  - **tuiles** en cartes au design system : rayon `rMd`, fond `surface2`,
    bordure `stroke` → `strokeStrong` au survol, **`accent`** + fond
    `accentSoft` si sélectionnée (nom en gras), enfoncement `scale 0.97` ;
  - **format** : petite **pastille de couleur** (`colorForExt`) + extension en
    mono `textSoft`, à la place de l'ancien badge plein bariolé — la couleur de
    format reste un repère discret intégré au thème sombre.

**Couleurs de format** (fonction `colorForExt`, repère ponctuel hors tokens,
réduit à une pastille) : obj `#3182ce` · stl `#d97706` · glb/gltf `#10b981` ·
ply `#8b5cf6` · 3ds `#eab308` · off `#6b7280` · défaut `#4a5568`.

---

## 8. Outils d'analyse (métadonnées)

Définis dans `Theme.tools`. Chaque entrée : `id`, `icon`, `label`, `key`
(raccourci), `blurb`, `legend`, `params[]`, `result`, et `evaluable`
(optionnel — déclenche le bouton primaire « Évaluer » au lieu de « Exporter »).

> **Courbure = réelle.** L'outil *Courbure* porte `evaluable: true` : le bouton
> « Évaluer » appelle `CgreQuickItem.evaluateCurvature(type)`, qui calcule la
> courbure par la **méthode Desbrun** (cgmesh `MeshAlgoTensorEvaluator` →
> `TENSOR_DESBRUN`, opérateurs discrets de Meyer/Desbrun/Schröder/Barr), mappe
> le scalaire choisi (Gaussienne/Moyenne/Minimale/Maximale) en heatmap *jet*
> par sommet, et l'affiche sur le modèle 3D. Les 4 autres outils restent des
> aperçus d'UI.
>
> Les tenseurs (κmin/κmax) sont **mis en cache** sur le maillage : changer le
> *type de courbure* dans la liste déclenche `recolorCurvature(type)` qui
> **recolore en direct** (re-mapping seul, sans réévaluation Desbrun), sans
> effet tant qu'aucune évaluation n'a eu lieu.

| Outil | Touche | Légende | Paramètres |
| :-- | :-- | :-- | :-- |
| **Fiche technique** | `F` | — | *(aucun — `kind: "spec"`, fiche lecture seule alimentée par `meshModel` via `specRows`)* |
| **Courbure** | `C` | scale (Faible→Forte) | type *(select : Gaussienne/Moyenne/Minimale/Maximale)*, sensibilité *(slider %)*, lissage *(slider %)* |
| **Comparaison** | `D` | scale (−2,0 → +2,0 mm) | référence *(file)*, alignement *(select)*, tolérance± *(slider mm)* |
| **Mesures** | `M` | readout | mode *(seg : **Point**/Distance/Angle/Rayon)*, unités *(select)*, aimanter *(toggle)* |
| **Épaisseur** | `T` | scale (0,8 → 6,0 mm) | méthode *(select)*, épaisseur min/max *(slider mm)* |
| **Coupe** | `S` | readout (Position du plan) | plan *(seg X/Y/Z)*, position *(slider %)*, contour *(toggle)* |

`Theme.toolById(id)` retourne l'entrée. L'état des paramètres est conservé par
outil dans `root.toolState` (`Main.qml`), initialisé aux `def` et muté de façon
immuable (clonage) pour déclencher la réactivité.

---

## 9. Interactions

| Action | Comportement | Réel / Stub |
| :-- | :-- | :-- |
| Dock → bouton d'outil (ou touches `F` `C` `D` `M` `T` `S`) | Affiche cet outil dans le volet droit (toujours ouvert). Re-cliquer l'outil actif revient à la **Fiche technique** | Réel (UI) |
| `Échap` | Revient à la **Fiche technique** (volet jamais vide) | Réel |
| Dock → **Recadrer** | `CgreQuickItem.resetView()` (reset trackball + zoom) | **Réel** |
| Dock → **Grille** | Bascule l'overlay grille HUD | **Réel** |
| Dock → **Orbite** | Réservé (rotation continue) | No-op |
| Boutons d'outils + panneau + légende | Aperçu visuel des futures analyses | **Stub** |
| Panneau → **Réinitialiser** | Restaure les `def` de l'outil + `clearAnalysis()` (retire la heatmap) | Réel |
| Panneau **Courbure** → **Évaluer** | `CgreQuickItem.evaluateCurvature(type)` : courbure **Desbrun** (cgmesh) → heatmap jet sur le modèle | **Réel** |
| Panneau **Courbure** → changer le *type* | `recolorCurvature(type)` : recolore en direct depuis les tenseurs cachés (après une 1ʳᵉ évaluation) | **Réel** |
| **Mesures / Point** (survol) | Picking : déprojection curseur → rayon → espace maillage → `Mesh::GetIntersectionWithRay` (octree cgmesh) ; coordonnées X/Y/Z affichées dans la **carte de légende** (bas‑gauche) | **Réel** |
| Panneau (autres) → **Exporter le rapport** | `console.log` | Stub |
| Bouton **sun/moon** (haut-droite) | Bascule le thème jour/nuit (`Theme.dark`) | **Réel** |
| Bouton **share** | — | Stub |
| Viewport | Glisser-gauche = rotation (trackball), molette = zoom | Réel |

---

## 10. Notes & limitations

- **Thème** : jour/nuit via `Theme.dark` (bouton sun/moon). La **densité
  compacte** de la maquette n'est pas portée.
- **Backdrop blur** : non (cf. §4).
- **Polices** : repli système si IBM Plex absent (cf. §2).
- **Backend** : *Fiche technique*, *Courbure* (Desbrun) et *Mesures → Point*
  (picking) sont réels ; *Comparaison / Épaisseur / Coupe* sont des aperçus d'UI.

## Correspondance avec la maquette

| Maquette (`Design/`) | Implémentation |
| :-- | :-- |
| `hifi-app.jsx` (HTOOLS, dock, panneau, légende) | `Theme.tools`, `AnalysisDock`, `AnalysisPanel`, `LegendCard` |
| `hifi-icons.jsx` (HIFI_ICONS, `<HIcon>`) | `Theme.icons`, `AppIcon` |
| Variables CSS `:root` (`Dock complet - Détaillé.html`) | tokens `Theme` |
| `tweaks-panel.jsx` (thème/densité/accent) | non porté (thème sombre figé) |
