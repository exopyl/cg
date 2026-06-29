# Reconstruction 3D à partir d'images — architecture & plan d'implémentation

> But : reconstruire une géométrie 3D (mesh, éventuellement texturé) à partir d'une
> ou plusieurs images. Contrainte de conception : **chaque étape est isolée derrière
> un contrat d'I/O explicite**, de sorte que l'algorithme d'une étape puisse être
> remplacé sans toucher aux autres.
>
> Ce document distingue strictement **ce qui est présent dans le code** (vérifié,
> avec le fichier/classe/fonction) de **ce qui reste à implémenter** (vérifié absent
> par recherche). Aucune capacité n'est supposée.

## Légende de statut

- ✅ **Disponible** : présent et vérifié dans le code (référence donnée).
- ◐ **Partiel** : une brique existe mais ne couvre qu'une partie de l'étape.
- ❌ **Absent** : vérifié absent du dépôt (`cgmesh`, `cgimg`, `reconstruction-cli`).
- 🔌 **Externe** : assuré par un outil tiers (pas dans le dépôt).

---

## 1. Vue d'ensemble

Deux paradigmes produisent une géométrie à partir d'images :

- **SfM + MVS** (géométrique) : features → poses caméra → profondeur multi-vues → fusion.
- **Mono-depth** (appris) : profondeur estimée par image → back-projection → fusion.

Ils divergent sur les étapes **2–4** et **partagent les étapes 5–12**. Isoler chaque
étape permet de basculer d'un paradigme à l'autre en ne remplaçant que 2–4.

> **Note d'implémentation** : `reconstruction-cli` est un **prototype**. Tout son code
> destiné à la production **migre dans `cgmesh`** (prétraitement, back-projection — cf.
> étapes 1 et 5), **sauf la couche ONNX/ML**, dont l'hébergement fait l'objet d'une
> décision dédiée (**§9**).

```
[1] Prétraitement
        │  Image'
        ▼
[2] Source de profondeur ──(mono-depth | stéréo/MVS)
        │  DepthMap (par vue)
        ▼
[3] Poses & calibration (SfM)         [4] Alignement d'échelle
        │  Camera(K,R,T) + SparseCloud      (DepthMap relative → métrique)
        ▼                                    │
[5] Back-projection ◄────────────────────────┘
        │  PointCloud (repère monde)
        ▼
[6] Fusion / recalage
        │  PointCloud unifié
        ▼
[7] Normales (si requis par [8])
        │
        ▼
[8] Reconstruction de surface → Mesh
        ▼
[9] Post-traitement mesh
        ▼
[10] Texturation → [11] Évaluation → [12] Visualisation
```

---

## 2. Étapes détaillées

Pour chaque étape : **rôle**, **contrat d'I/O** (la « couture » d'interchangeabilité),
**briques disponibles**, **à implémenter**, **candidats interchangeables**.

### Étape 1 — Prétraitement d'image
- **Rôle** : décoder, (dé)distordre, redimensionner, normaliser l'image avant usage.
- **Contrat** : `Image (fichier)` → `Image normalisée` (+ intrinsèques connues éventuelles).
- **Disponible** :
  - ✅ `cgimg/Img` : `load` (.png via stb_image, .bmp, .tga, .pnm), `width/height`,
    `get_pixel/set_pixel` (RGBA, index `(j*W+i)*4`), filtres (`gaussian_blur`,
    `bilateral_filtering`, `filter_sobel`…), `convert_to_grayscale`, `crop`.
    (`src/cgimg/image.h`, `image_io.cpp`)
  - ✅ `reconstruction-cli::preprocess` : rééchantillonnage bilinéaire **avec clamp** +
    normalisation ImageNet + agencement **NCHW**. (`reconstruction-cli/main.cpp`)
- **À implémenter / défauts connus** :
  - ↪ **Migrer vers `cgmesh`** : `reconstruction-cli::preprocess` (rééchantillonnage
    bilinéaire + normalisation ImageNet + NCHW) — actuellement dans le prototype.
  - ⓘ **undistortion** k1/k2 : les coefficients sont extraits dans `CameraView` mais
    **volontairement non appliqués** (approximation acceptée — décision 2026-06-26 ;
    distorsion faible sur photos de téléphone). Si besoin un jour : `colmap image_undistorter`
    (rectifie les images, k1/k2≈0, modèle sténopé exact) plutôt que de coder la correction.
  - ⚠ **bug** `Img::resize` (modes 0/1) : `x1=x0+1`/`y1=y0+1` atteignent
    `m_iWidth`/`m_iHeight` → lecture hors limites sur la dernière ligne/colonne
    (`src/cgimg/image.cpp:565`). Contourné dans `reconstruction-cli` (échantillonnage maison).
  - ❌ écriture **PNG** (`Img::export_png` renvoie -1 ; `image_io_png.cpp` ne vendore
    que le décodeur). Sorties images en .bmp/.tga/.pnm.
- **Interchangeables** : tout décodeur/redimensionneur respectant `Image → Image`.

### Étape 2 — Source de profondeur / correspondances
- **Rôle** : produire une carte de profondeur par vue (ou des correspondances inter-vues).
- **Contrat** : `Image [+ voisines] [+ Camera]` → `DepthMap` (ou disparité / matches).
- **Disponible** :
  - ✅ **Mono-depth appris** : `reconstruction-cli` (ONNX Runtime 1.20.1) + modèle Depth
    Anything V2 ONNX. Entrée `image[1,3,H,W]`, sortie `depth[1,H,W]` (disparité
    relative). (`reconstruction-cli/main.cpp`)
  - ◐ **Stéréo dense** : `cgimg/DisparityBirchfield` (Birchfield-Tomasi, scanline,
    `SetStereoPair/Process/PostProcess/GetDisparity`) et `cgimg/DisparityEvaluator`
    (block). (`src/cgimg/image_disparity_birchfield.h`, `disparity.h`)
  - ✅ **MVS dense (COLMAP CUDA)** : `image_undistorter` → `patch_match_stereo` (GPU) →
    `stereo_fusion` → `fused.ply` (nuage **dense + normales orientées + couleurs**), chargé
    par `recon::loadPointCloudPly` (`recon_io.{h,cpp}`) → directement Poisson (court-circuite
    déprojection/échelle/fusion). **Nécessite le build CUDA** `colmap-x64-windows-cuda`
    (`patch_match_stereo` est CUDA-only ; le build nocuda échoue). Driver :
    `reconstruction-cli --poisson-ply <fused.ply> [depth] [trimQuantile] [removePlane]` →
    `recon_dense.obj` **+ `recon_dense_cloud.ply`** (nuage exporté via
    `recon::savePointCloudPly`). Le nuage reconstruit est aussi exportable **dans le
    pipeline** via `Pipeline::exportCloudPath` (écrit avant l'étape 8 ; activé en multi-vues).
  - ✅ **Retrait du plan dominant** (option) : `recon::removeDominantPlane` (RANSAC) —
    enlève le **plan de table** sous l'objet (sinon il domine la reconstruction). Sur le
    benchy : 240k pts → ~187k (table) retirés → **53k pts objet** → Poisson sort un benchy
    reconnaissable (au lieu d'une nappe bosselée table+objet). Activé par `removePlane=1`.
- **À implémenter** :
  - ✅ **Migré dans `cgmesh`** : `recon::OnnxDepthSource` (session ORT + chargement
    modèle + inférence + pré-traitement ImageNet/NCHW), derrière `ENABLE_ONNX` /
    `CG_HAS_ONNX` (pimpl ; ORT lié en PUBLIC, dépendance optionnelle). Décision **§9**,
    Option C. Aucun Python au runtime. `reconstruction-cli` n'est plus qu'un pilote.
  - ❌ **rectification stéréo** (prérequis de la stéréo dense sur paire non rectifiée).
  - ❌ **MVS multi-vues** (plane-sweep / patch-match).
- **Interchangeables** : autre modèle ONNX (Depth Anything autre taille, MiDaS…),
  stéréo, MVS — tous derrière `→ DepthMap`.

### Étape 3 — Poses caméra & calibration (SfM)
- **Rôle** : estimer intrinsèques `K` et extrinsèques `R,T` par vue, + nuage épars.
- **Contrat** : `ImageSet` → `Camera[](K,R,T)` + `SparseCloud` (+ visibilité par point).
- **Disponible** :
  - ◐ **Import** seulement : `cgmesh/Bundle::Load` / `Load2` parse le fichier
    **`bundle.out`** (format Bundler de Snavely : `nCameras nPoints`, paramètres
    caméra, points 3D + visibilité). (`src/cgmesh/bundle.cpp:34,98`)
  - ✅ `cgmesh/BundleCamera` : conteneur calibré — `f_mm`, `f_pxl`, `CCDWidth/Height_mm`,
    `k1,k2`, `R` (4×4 row-major), `T`, `Rinv`, `d` (direction), `pos`.
    (`src/cgmesh/bundle_camera.h`)
  - ◐ `Bundle::convert_vlleafsift_to_siftlowe` : **convertisseur de format** de clés
    SIFT (VLFeat → Lowe), pour alimenter un Bundler externe. (`bundle.cpp:702`)
- **À implémenter** :
  - ❌ **le solveur SfM lui-même** : détection de features (SIFT/ORB/Harris),
    appariement, matrice essentielle/fondamentale, **triangulation multi-vues**,
    **PnP**, **bundle adjustment** (Levenberg-Marquardt). Vérifié absent de `cgmesh`
    et `cgimg`.
- **Décision (actée)** : 🔌 **COLMAP** assure le SfM hors ligne → export **format
  Bundler** (`bundle.out`) → `Bundle::Load`. **Aucune bibliothèque liée à `cg`.**
  Compatibilité vérifiée et reste-à-faire détaillés en **§7**. Alternative reportée :
  SfM in-engine (sans lib, sur `cgmath`).
- **Dépendance** : **aucune bibliothèque externe liée à `cg`**. COLMAP/Bundler sont des
  **outils autonomes** exécutés hors ligne ; le seul couplage est le **fichier
  `bundle.out`**, déjà lu par `Bundle::Load`. C'est le design historique de `cg`
  (VLFeat + Bundler externes → `bundle.out` → import). Un SfM in-engine n'ajouterait
  pas non plus de lib (le solveur s'appuierait sur `cgmath`).

### Étape 4 — Alignement d'échelle
- **Rôle** : ramener chaque `DepthMap` relative (mono-depth) à une échelle commune.
- **Contrat** : `(DepthMap, points épars projetés dans la vue)` → `DepthMap` recalée
  (`Z' = s·f(disp) + b`).
- **Disponible** :
  - ◐ Les **points épars** et leur **visibilité par caméra** sont fournis par
    `Bundle` (matière première de la calibration). Aucune routine d'alignement n'existe.
- **À implémenter** :
  - ❌ ajustement (moindres carrés) des paramètres `s,b` entre la profondeur mono et
    les profondeurs connues des points épars visibles dans la vue.
  - ⚠ **`Load2` ne retient PAS les observations 2D** : `imgx/imgy` sont parsés puis
    **jetés** (`bundle.cpp:243-247`) ; seuls les indices caméra par point sont stockés
    (`pt_visible_from_cameras`). → pour l'alignement, soit **étendre `Bundle`** pour
    conserver `(point, caméra, imgx, imgy)`, soit **reprojeter** les points épars 3D
    dans chaque vue (nécessite `w/h` + convention, cf. §7).
- **Interchangeables** : alignement global affine, par région, ou consistance inter-vues.
- *Note* : étape **non requise** si la profondeur est déjà métrique (stéréo calibrée).

### Étape 5 — Back-projection (déprojection)
- **Rôle** : transformer `DepthMap + Camera` en points 3D (repère monde).
- **Contrat** : `(DepthMap, Camera)` → `PointCloud`.
- **Disponible** :
  - ◐ `reconstruction-cli` : déprojection sténopé `X=(u-cx)/f·Z`, `Y=-(v-cy)/f·Z`,
    `Z=1/(disp+ε)`, en **repère caméra** ; + coupe des faces aux discontinuités de
    profondeur + seuil de premier plan `disp_min`. (`reconstruction-cli/main.cpp`)
- **À implémenter** :
  - ✅ **Migré dans `cgmesh`** : `recon::PinholeUnprojector` (déprojection sténopé,
    repère caméra, seuil `dispMin`, sortie nuage). *(La coupe aux discontinuités / export
    OBJ restent côté `reconstruction-cli` ; inutiles ici — la surface est maillée par l'étape 8.)*
  - ❌ passage en **repère monde** : composer avec `R,T` (multi-vues, cf. §7).
- **Interchangeables** : modèle sténopé / avec distorsion ; sortie nuage seul vs nuage+maillage.

### Étape 6 — Fusion / recalage
- **Rôle** : fusionner les nuages par vue en un nuage cohérent (recaler si besoin).
- **Contrat** : `PointCloud[]` → `PointCloud` unifié.
- **Disponible** :
  - ◐ `cgmesh/Voxels` : grille scalaire/labélisée générique (`init`, `set_data/get_data`,
    `smooth_data`, `threshold_data`) — utilisable comme **accumulateur** spatial.
    (`src/cgmesh/voxels.h`)
  - ◐ `cgmesh/Mesh::MergeVertices(tolerance)` : soudure géométrique des points
    coïncidents (downsample/merge simple). (`src/cgmesh/mesh.h`)
  - ◐ `cgmesh/DirectVisibilityOfPointSets` : hidden-point removal (Katz 2007).
  - ◐ `cgmesh/Octree`, `cgmesh/BVH` : requêtes de voisinage / plus proche point
    (`BVH::closest_distance2`).
  - ✅ **ICP générique** `icp_align` (`icp.{h,cpp}`, **algorithme autonome global**, hors
    namespace recon comme `bvh`/`mesh_metrics`/`thickness`, 2026-06-29) : recalage
    point-to-surface (correspondances closest-point BVH + transfo optimale par **quaternion
    de Horn**, Jacobi 4×4), option **échelle** (`withScale` → similarité 7 DDL) / **rigide**
    (6 DDL), **trimming** des pires paires (robustesse outliers), pose initiale. Tests
    `icp_rigid_recovers_pose` / `icp_scale_needs_similarity`. **Brique réutilisable** (sert
    aussi l'étape 11 pour recaler sur une référence).
- **À implémenter** :
  - ◐ **`IFuser` rigide via ICP** : la primitive `icp_align` existe ; reste à l'envelopper
    dans un fuser. ⚠ **faible priorité** : la voie dense COLMAP n'exerce **pas** l'étape 6
    (`fused.ply` déjà fusionné) ; utile surtout pour la voie mono-depth multivue.
  - ❌ **intégration TSDF** dans `Voxels` (la grille existe, la logique d'intégration
    pondérée non).
  - ❌ orchestration multi-nuages (downsample voxel, débruitage statistique).
- **Interchangeables** : weld par tolérance / downsample voxel / TSDF / ICP rigide.

### Étape 7 — Estimation de normales (de nuage)
- **Rôle** : doter le nuage de normales (orientées) si l'étape 8 l'exige.
- **Contrat** : `PointCloud` → `PointCloud + Normales`.
- **Disponible** :
  - ◐ `cgmesh/Normals::EvalOnVertices(Mesh_half_edge*, MethodId)` : normales **de
    sommets de mesh** (Gouraud/Thürmer/Max/Desbrun) — suppose une connectivité, donc
    **pas** applicable tel quel à un nuage non structuré. (`src/cgmesh/normals.h`)
  - ◐ `cgmesh/Cmesh_orientation_pca` : orientation **globale d'un mesh** par PCA — ce
    n'est **pas** une estimation de normale par point. (`src/cgmesh/orientation_pca.h`)
- **Disponible (ajout)** :
  - ✅ **Normales orientées estimées à la déprojection** : `PinholeUnprojector::withNormals`
    (produit vectoriel des tangentes sur la grille de profondeur, orientées vers la caméra).
    Conservées par `ConcatFuser` → consommées par Poisson. C'est la voie retenue (cheap, orientée).
    Test synthétique `unprojector_with_normals_frontal_plane`.
- **À implémenter** :
  - ❌ **normales de nuage non structuré** par PCA des k plus proches voisins + propagation
    d'orientation (MST) — utile si les points ne viennent pas d'une grille de profondeur.
- *Note* : **non requis** pour la reconstruction par `PointCloudField` (étape 8), qui
  travaille sur un champ scalaire non signé. Nécessaire seulement pour un
  reconstructeur de type Poisson.

### Étape 8 — Reconstruction de surface (maillage)
- **Rôle** : transformer le nuage en mesh.
- **Contrat** : `PointCloud (+Normales)` → `Mesh`.
- **Disponible** :
  - ✅ `cgmesh/PointCloudField` (champ gaussien somme-de-Gaussiennes depuis un nuage,
    accéléré par octree) **+** `cgmesh/ImplicitSurface` (**marching cubes** :
    `set_bbox`, `set_value` iso, `set_resolution_per_unit`, `set_eval_func` +
    `set_eval_data`, `compute`, `get_triangulation(&nv,&verts,&nf,&faces)`).
    (`src/cgmesh/surface_implicit_pointcloud.h`, `surface_implicit.h`)
  - ✅ Pont vers `Mesh` : `Mesh::SetVertices(nV, float*)` + `Mesh::SetFaces(nF, 3, uint*)`.
  - ✅ **Variante heightmap** : `recon::HeightmapReconstructor` sur nuage **organisé**
    (grille, cf. `PointCloud::width/height/valid`) — 2 triangles/cellule + coupe aux
    discontinuités. Préserve le détail (mono-vue), bien meilleur que l'implicite sur une
    nappe 2.5D.
  - ✅ **Variante Poisson** — **séparée en deux couches (2026-06-29)** :
    - **cœur GÉNÉRIQUE** `poisson_reconstruct(pos, nor, n, PoissonParams)`
      (`poisson_surface.{h,cpp}`, global) : porte **PoissonRecon 18.76 vendorisé, IN-PROCESS**
      (derrière `ENABLE_POISSON`/`CG_HAS_POISSON`). `Reconstructors.h` autonome (aucune
      dépendance PNG/JPEG, namespace `PoissonRecon`). Correctifs MSVC : `_HAS_STD_BYTE=0` +
      `/bigobj` (réglages **target-wide** sur cgmesh). Réutilisable hors reconstruction.
    - **adaptateur MINCE** `recon::PoissonReconstructor` (`recon_surface_poisson.{h,cpp}`,
      `ISurfaceReconstructor`) : déballe le `PointCloud` et appelle le cœur.
    **Requiert des normales orientées** (étape 7). Test `surface_poisson_sphere`. ⚠ lent en
    **Debug**. **Trim densité intégré** (`trimQuantile`) : `outputDensity` → densité par sommet
    → coupe les triangles sous le quantile + compactage (équivalent in-process du
    `SurfaceTrimmer`, supprime les « ballons » d'extrapolation basse densité).
  - ✅ **Câblage validé (2026-06-26)** via `recon::ImplicitSurfaceReconstructor`
    (`PointCloudField` → `ImplicitSurface` → `Mesh`) ; test `surface_implicit_sphere`
    (sphère → mesh non vide). NB : triangulation MC **plafonnée à 100000 verts/faces**
    (`surface_implicit.cpp`) → garder `gridCells` modéré sur des nuages denses.
- **À implémenter** :
  - ✅ **Poisson** : `recon::PoissonReconstructor` (PoissonRecon master vendorisé, in-process —
    cf. ci-dessus). ❌ **Delaunay / alpha-shapes** ; ❌ Ball-Pivoting. *(Poisson exige des
    normales orientées → étape 7, encore à implémenter pour l'usage multi-vues.)*
- **Interchangeables** : implicite blobby (dispo) / Poisson / Delaunay, tous `→ Mesh`.

### Étape 9 — Post-traitement mesh
- **Rôle** : nettoyer, alléger, lisser, boucher.
- **Contrat** : `Mesh` → `Mesh`.
- **Disponible** :
  - ✅ **Décimation QEM** : `Mesh_half_edge::simplify(target_ratio, SimplifyOptions)`
    (manifold-in → manifold-out, features/attributs/UV, borne Hausdorff). (`simplification.cpp`)
  - ✅ **Lissage** : `smoothing_taubin`, `smoothing_laplacian`. (`src/cgmesh/smoothing*.h`)
  - ✅ **Soudure** : `Mesh::MergeVertices`. ✅ **Subdivision** : Loop/sqrt3/Karbacher.
  - ✅ **Triangulation** : `Mesh::Triangulate` / `BuildTriangulation`.
- **À implémenter** :
  - ❌ **bouchage de trous**.
- **Interchangeables** : tout filtre `Mesh → Mesh`.

### Étape 10 — Texturation
- **Rôle** : projeter les photos sur le mesh.
- **Contrat** : `(Mesh, ImageSet, Camera[])` → `Mesh texturé`.
- **Disponible** :
  - ✅ **`recon::ProjectiveTexturer`** (`recon_texture.{h,cpp}`, `ITexturer`) : pour chaque face,
    choisit la **caméra la plus frontale NON OCCULTÉE** (test d'occlusion par **rayon d'ombre BVH**
    barycentre→centre caméra `C=-Rᵀ·T`, option `occlusion`, défaut true), y projette les sommets,
    et **ÉCLATE le mesh** (3 sommets propres/face portant leurs UV → **vertex-parallèle**, correct
    à la fois dans le rendu cgre ET à l'export). Image assignée comme `MaterialTexture` (nom unique).
    Export **OBJ+MTL** (`vt`+`usemtl`+`map_Kd`) lisible MeshLab/Blender/sinaia. Faces vues par aucune
    caméra non occultée → **repli gris** (plutôt que du bleeding). Driver :
    `reconstruction-cli --texture <mesh.obj> <bundle.out> <list.txt> <images_dir> [out.obj]`. Validé sur
    le benchy (12 caméras, 273k faces) : occlusion → **25 %** des faces changent de caméra, **12 %**
    en repli gris ; **résidu UV vérifié 0 px** (vs projection vérité-terrain). *(Voir §8 pour les deux
    bugs corrigés : moucheté UV et décalage `R` transposée.)*
  - ✅ Support **UV/matériaux** côté `Mesh` (`m_pTextureCoordinates`, per-face vs vertex-parallèle,
    `Material`/`Material_Add`) ; rendu cgre `mesh_renderer` (lit les UV **par face**) et
    `BuildPolygonRenderData`/VBO (UV **vertex-parallèle**). L'éclatement satisfait les deux.
- **À implémenter** :
  - ❌ **blending / pondération inter-vues** : une seule caméra par face → coutures et sauts
    d'exposition visibles. L'occlusion décide *qui* est admissible ; le blending déciderait
    *comment mélanger* sur les faces vues par plusieurs caméras.
  - ❌ **UV unwrap / atlas** ; lissage de couture par graph-cut (type Lempitsky-Ivanov 2007).
- **Note legacy** : `bundle.cpp::project_textures_naive` / `_lempitsky07` (+ `project_texture_*`,
  intrinsèques `f_mm` NaN, court-circuit `1||` à `bundle.cpp:447`) sont l'**ancienne voie, NON
  utilisée** — remplacée par `ProjectiveTexturer`. Conservées pour mémoire.
- **Interchangeables** : projection frontale (dispo) / pondérée-blendée / atlas.

### Étape 11 — Évaluation
- **Rôle** : mesurer la qualité géométrique vs une référence.
- **Contrat** : `(Mesh, Mesh référence)` → `Metrics`.
- **Disponible** :
  - ✅ **Métrique générique** `mesh_metrics` (`mesh_metrics.{h,cpp}`) : `mesh_hausdorff` renvoie
    max a→b / b→a / symétrique **+ moyenne, RMS, p95** (statistiques sur la distribution des
    distances échantillon→surface, robustes là où le seul max sature sur un point aberrant) ;
    `mesh_hausdorff_relative` (/diagonale bbox) ; `mesh_pointwise_distance` (distance par sommet
    → **heatmap d'erreur**). Échantillonnage déterministe (sommets + barycentres), accéléré BVH.
  - ✅ **Adaptateur recon** `recon::HausdorffEvaluator` (`recon_evaluate.{h,cpp}`, `IEvaluator`)
    → `recon::Metrics` { hausdorff, hausdorffRelative, meanError, rmsError, p95Error }.
  - ✅ **Validé sur CUBE SYNTHÉTIQUE** (oracle analytique exact) : driver
    `reconstruction-cli --eval-cube [depth] [res]` génère un nuage cube orienté → Poisson → évalue vs
    cube analytique → **heatmap PLY colorée** `eval_cube_heatmap.ply`. depth 7 (côté 1.0) :
    max 0.0176, moy 0.0006, RMS 0.0012, p95 0.0016 — erreur **concentrée sur arêtes/coins**
    (Poisson arrondit les features vives), faces planes quasi exactes ; vérifié numériquement.
    Test `evaluate_cube_poisson`.
- **À implémenter** :
  - ❌ **pré-alignement** : l'évaluateur suppose les deux mesh **déjà recalés** (vrai pour le cube
    synthétique = identité). Pour une référence **réelle** (ex. `3DBenchy.stl`, échelle/pose
    arbitraires), brancher `icp_align` en **similarité** (cf. étape 6 / `icp.h`) **en amont**. La
    brique ICP existe ; reste à la câbler comme pré-étape.
  - ❌ Chamfer ; métriques **complétude / précision** (recall/precision à seuil de distance).
- **Note** : `Mesh::export_ply` est **vertices-only** (ni couleurs ni faces) ; la heatmap est
  écrite par un mini-writer PLY ASCII dans le driver. Un export PLY de **mesh coloré** générique
  reste à ajouter si besoin.
- **Interchangeables** : Hausdorff / Chamfer / complétude-précision.

### Étape 12 — Visualisation
- **Rôle** : afficher/inspecter le résultat.
- **Disponible** :
  - ✅ **sinaia** (viewer OpenGL, caméra interactive) — charge un mesh via dialogue
    (pas d'argument CLI). `reconstruction-cli` exporte en `.obj` chargeable.

---

## 3. Contrats d'interface (les « coutures »)

Pour rendre chaque étape interchangeable, on regroupe **types de données partagés** et
**interfaces abstraites** dans **`cgmesh`** (`src/cgmesh/recon_types.h`, `recon_interfaces.h`).
**Tout le code de reconstruction vit dans `cgmesh` — aucun module séparé.**
*(✅ implémenté — Phase 0, cf. §4.)*

> **Principe d'architecture (acté 2026-06-29)** : un **algorithme générique** de géométrie
> vit au niveau `cgmesh` **hors** namespace `recon` (comme `bvh`, `mesh_metrics`, `thickness`,
> `icp`, `poisson_surface`) ; les fichiers **`recon_*`** ne contiennent que les **adaptateurs**
> qui implémentent une interface `I…` en déballant les types recon et en appelant le cœur
> générique. La couture recon, ce sont les **interfaces** — pas les algorithmes.

Types de données :
- `DepthMap` { `int w,h`; `std::vector<float> z` } *(ou réutiliser `Img`)*
- `CameraView` { `K` (f, cx, cy, k1, k2), `R`, `T` } *(adaptateur au-dessus de `BundleCamera`)*
- `PointCloud` { positions, normales?, couleurs? } *(adaptateur au-dessus de `PointSet`)*

Interfaces (une par étape, toutes sans état partagé) :
```
IPreprocessor   : Image            -> Image
IDepthSource    : Image[,voisines] -> DepthMap
IPoseSource     : ImageSet         -> std::vector<CameraView> + PointCloud (épars)
IDepthScaler    : (DepthMap, épars projetés) -> DepthMap
IUnprojector    : (DepthMap, CameraView)     -> PointCloud (monde)
IFuser          : std::vector<PointCloud>    -> PointCloud
INormalEstimator: PointCloud       -> PointCloud (+normales)
ISurfaceRecon   : PointCloud       -> Mesh
IMeshPostProc   : Mesh             -> Mesh
ITexturer       : (Mesh, ImageSet, std::vector<CameraView>) -> Mesh
IEvaluator      : (Mesh, Mesh ref) -> Metrics
```
Chaque implémentation concrète est un **adaptateur** au-dessus d'une brique existante
ou un nouveau composant. Le pipeline = composition ordonnée de ces interfaces ; en
changer une = fournir une autre implémentation de la même interface.

---

## 4. Plan d'implémentation (par phases, étapes isolées)

### Phase 0 — Le squelette (les coutures) — ✅ fait (2026-06-26)
- Créé dans `cgmesh` (namespace `recon`) :
  - `recon_types.h` — `DepthMap`, `CameraView`, `PointCloud`, `Metrics`.
  - `recon_interfaces.h` — les 11 interfaces `I…` (une par étape).
  - `recon_pipeline.{h,cpp}` — `Pipeline` : `runSingleView` / `runMultiView`, étapes
    injectées (pointeurs **non possédés**, `nullptr` = ignorée). Aucune logique
    d'algorithme — uniquement le câblage.
- **Aucun module séparé** — tout dans `cgmesh`.
- **Vérifié** : compile dans `cgmesh` ; `test/tu_cgmesh_recon.cpp` (valeurs par défaut
  des types, garde « étape requise manquante → nullptr », câblage mono-vue avec étapes
  factices) — **3/3 OK**.

### Phase 1 — Chaîne mono-vue (refactor en interfaces) — 🔶 en cours (2026-06-26)
- ✅ `IUnprojector` ← `recon::PinholeUnprojector` (back-projection sténopé, repère caméra).
- ✅ `ISurfaceReconstructor` ← `recon::ImplicitSurfaceReconstructor` (`PointCloudField` +
  `ImplicitSurface`) — **wiring validé** (test `surface_implicit_sphere`), lève le risque
  « aucun appelant existant » de l'étape 8.
- ✅ `IMeshPostProcessor` ← `recon::MergeVerticesPostProcessor` (in place). *(QEM/Taubin :
  à ajouter avec recopie du résultat, `simplify` remplaçant le mesh.)*
- ✅ `IDepthSource` ← `recon::OnnxDepthSource` (Depth Anything V2), logé dans `cgmesh`
  derrière `ENABLE_ONNX` / `CG_HAS_ONNX` (pimpl, ORT lié en PUBLIC ; cf. §9). Pré-traitement
  ImageNet + NCHW migré depuis `reconstruction-cli`.
- ✅ **End-to-end validé** : `reconstruction-cli` est devenu un **pilote** de `recon::Pipeline`
  (depth → unproject → surface → post → OBJ) ; sur `plante01.png` : mesh produit en ~3,5 s
  (3644 v / 7284 f).
- ⚠ **Qualité grossière** (hors périmètre Phase 1) : le reconstructeur implicite sur-lisse
  la nappe mono-vue en un blob et, sans seuil premier-plan, fusionne le fond. Leviers :
  `PinholeUnprojector::dispMin`, `ImplicitSurfaceReconstructor::gridCells`/`isoDistance`,
  ou **brancher un autre `ISurfaceReconstructor`** (p. ex. heightmap) — c'est précisément
  ce que permet le découplage par interfaces.
- Tests : `test/tu_cgmesh_recon.cpp` — **6/6 OK**.

### Phase 2 — Chaîne multi-vues avec poses externes (le « vrai » Niveau 2) — 🔶 en cours
Briques validées en **synthétique** (sans COLMAP) :
1. ✅ `IPoseSource` ← `recon::BundleSource` (parse `bundle.out`, w/h via images, R/T/focale).
   Test `pose_bundle_source` (bundle.out fabriqué). 🔌 poses réelles = **COLMAP→Bundler**.
2. ✅ `IDepthSource` ← `recon::OnnxDepthSource` (Phase 1).
3. ✅ `IDepthScaler` ← `recon::AffineDepthScaler` (fit `1/Z = a·disp + b` sur les épars
   reprojetés → depth métrique cohérente entre vues). Test synthétique `depth_scaler_affine`.
4. ✅ `IUnprojector` ← `recon::PinholeUnprojector` en **repère monde** (`P = R^T·(Pc−T)`).
   Test `unprojector_world_pose`.
5. ✅ `IFuser` ← `recon::ConcatFuser` (union). Test `fuser_concat`. *(downsample/ICP/TSDF à venir.)*
- ✅ Orchestration : `Pipeline::runMultiView` validée (`pipeline_multiview_wires_stages`).
- ✅ **Run réel sur `plante01/02/03`** (2026-06-26) : COLMAP (poses, voir §7) →
  `reconstruction-cli --multiview` → mesh en ~8 s (3712 v / 7416 f). Les 3 vues fusionnent en un
  **volume connexe cohérent** (poses + échelle OK). ⚠ **Qualité** toujours limitée par le
  reconstructeur implicite (blob) — à régler/​remplacer (Phase 3), pas un défaut de la chaîne.
6. `INormalEstimator` ← *(optionnel : non requis par `PointCloudField`)*.
7. `ISurfaceRecon` ← `PointCloudField` + `ImplicitSurface`. **[réutilise]**
8. `IMeshPostProc` ← QEM + Taubin. **[réutilise]**
9. `ITexturer` ← `Bundle::project_textures_naive`. **[réutilise]**
10. `IEvaluator` ← Hausdorff. **[réutilise]** + visu sinaia.

### Phase 3 — Interchangeabilité (remplacements à froid) — 🔶 démarré (2026-06-26)
- ✅ `ISurfaceReconstructor` : implicite (blob) → **`recon::HeightmapReconstructor`** (sur
  nuage **organisé**). Même `Pipeline`, **un seul stage changé** → le mono-vue passe du
  blob (~3,6 k faces) à un **relief détaillé** (~100 k faces) montrant les feuilles de la
  plante en 3D. Tests `surface_heightmap_grid` / `_culls_discontinuity`.
  *(Heightmap = mono-vue / nuage organisé ; le multi-vues fusionné garde l'implicite.)*
- ✅ `ISurfaceReconstructor` : → **`recon::PoissonReconstructor`** (PoissonRecon 18.76
  vendorisé in-process). **Run réel multi-vues** `reconstruction-cli --multiview` sur
  `plante01/02/03` (poses COLMAP + normales `withNormals` + `AffineDepthScaler` + fusion) :
  ~5 s en **Release**, mesh **fermé** 15 280 v / 30 564 f. ⚠ **Qualité décevante** :
  Poisson extrapole/ballonne sur le nuage mono-depth fusionné (épars, échelle/normales
  approximatives, sans *surface trimming*). Le **plafond vient de l'entrée** (fusion
  mono-depth), pas du reconstructeur. Pistes : `SurfaceTrimmer` (densité), vraie MVS dense,
  meilleur alignement d'échelle, crop premier-plan.
- Autres remplacements possibles, chacun ne touchant qu'une classe : `IDepthSource`
  ONNX→stéréo `DisparityBirchfield` ; `IFuser` union→TSDF (`Voxels`) ; `IPoseSource`
  COLMAP→SfM in-engine.

---

## 5. Récapitulatif « disponible vs à faire »

| Étape | Disponible (réf.) | À implémenter |
|---|---|---|
| 1 Prétraitement | ◐ `Img`, `reconstruction-cli::preprocess` | **migrer preprocess→cgmesh** ; undistort ; fix `Img::resize` ; export PNG |
| 2 Profondeur | ✅ mono-depth ONNX ; ✅ **MVS dense (COLMAP CUDA → fused.ply → `loadPointCloudPly`)** ; ◐ stéréo Birchfield | rectification stéréo ; MVS maison |
| 3 Poses (SfM) | ✅ `recon::BundleSource` (parse + w/h via images) ; `BundleCamera` | 🔌 poses réelles via **COLMAP**→Bundler ; alignement d'échelle (étape 4) |
| 4 Échelle | ✅ `recon::AffineDepthScaler` (fit `1/Z=a·disp+b` sur épars ; test `depth_scaler_affine`) | — |
| 5 Back-projection | ✅ `recon::PinholeUnprojector` (repère monde `R,T`) | mapping disparité→Z (heuristique) |
| 6 Fusion | ✅ `recon::ConcatFuser` (union) ; ✅ **`icp_align` générique (rigide/​similarité+échelle, trimming)** ; `Voxels`, `MergeVertices`, HPR dispo | envelopper ICP dans `IFuser` (faible prio) ; downsample voxel ; TSDF |
| 7 Normales nuage | ✅ orientées à la déprojection (`withNormals`) ; ◐ PCA mesh (pas par point) | normales k-NN orientées (nuage non structuré) |
| 8 Surface | ✅ implicite (MC) ; heightmap ; **Poisson 18.76 (cœur générique `poisson_surface` + adaptateur recon) +trim** | Delaunay / alpha-shapes ; Ball-Pivoting |
| 9 Post mesh | ✅ QEM, Taubin/Laplacian, MergeVertices, subdivision ; ✅ `removeDominantPlane` | câbler QEM/Taubin dans le pipeline ; bouchage de trous |
| 10 Texture | ✅ `recon::ProjectiveTexturer` (caméra frontale **non occultée** → UV, mesh éclaté, OBJ+MTL) ; **occlusion BVH** ; driver `--texture` | blending inter-vues ; UV unwrap/atlas |
| 11 Évaluation | ✅ **`mesh_metrics` (Hausdorff max+moyenne+RMS+p95, distance par sommet)** ; **`HausdorffEvaluator`** ; driver `--eval-cube` + heatmap | pré-alignement ICP pour référence réelle ; Chamfer ; complétude/précision |
| 12 Visualisation | ✅ sinaia ; export `.obj` | — |

---

## 6. Points à vérifier avant implémentation (non supposés ici)

- **Format Bundler de COLMAP vs `Bundle::Load`** : le **format numérique est vérifié
  compatible** (cf. §7). Restent à confirmer empiriquement : (a) que COLMAP **génère
  bien la liste d'images** attendue par `Load2` (existence, format, **même ordre** que
  les caméras) ; (b) que la **convention de repère** de l'export Bundler de COLMAP est
  bien le `-Z` de Bundler supposé par les calculs `pos`/`d` de `Load2`.
- ✅ **`project_textures_naive` — lu (2026-06-26)** : projette les sommets de face en UV
  via les caméras et assigne un matériau-texture par face, **mais non fonctionnel en
  l'état** (court-circuit `1 ||` à `bundle.cpp:447` + intrinsèques mm absentes). Détails
  et correctifs en **étape 10**.
- **`PointCloudField`/`ImplicitSurface`** : paramétrage (sigma/iso, résolution,
  bbox) et coût mémoire de la grille MC sur un nuage dense — à mesurer.
- **`PointSet`** : pas de setter public de normales/couleurs ; si l'on veut les
  porter, prévoir un conteneur `PointCloud` propre.
- **Validation multi-vues (hors code)** : nécessite un **COLMAP installé** + un **jeu
  d'images multi-vues réel** ; la compatibilité de format `bundle.out` et la convention
  de repère ne pourront être confirmées **qu'en exécutant COLMAP**. La chaîne
  **mono-vue** (étapes 1,2,5,8,9,11,12), elle, est implémentable et validable **sans
  rien d'externe**.

---

## 7. Branchement COLMAP → cgmesh (compatibilité vérifiée & à implémenter)

**Outil retenu : COLMAP** (autonome, hors ligne). Pipeline type :
```
colmap feature_extractor   --database_path db.db --image_path images/
colmap exhaustive_matcher  --database_path db.db
colmap mapper              --database_path db.db --image_path images/ --output_path sparse/
colmap image_undistorter   --image_path images/ --input_path sparse/0 --output_path undist/   # (pour k1/k2)
colmap model_converter     --input_path sparse/0 --output_path scene --output_type Bundler    # -> scene.out (+ liste)
```
Le `.out` produit est ensuite lu par `cgmesh/Bundle::Load` / `Load2`.

> **✅ Vérifié (2026-06-26)** — COLMAP 4.1.0 (build nocuda) sur `plante01/02/03` :
> - `model_converter --output_type Bundler` écrit `bundle.bundle.out` (format
>   `# Bundle file v0.3`, `f k1 k2` + 3 lignes R + T par caméra, points xyz/rgb/visibilité)
>   **directement parsé par `Bundle::Load`** — compatibilité format **confirmée**.
> - **Liste d'images** (`bundle.list.txt`) = **ordre d'enregistrement COLMAP** (≠ ordre
>   d'entrée) → l'ordre des caméras suit ce fichier (le driver le lit pour ordonner).
> - **Gotchas CPU/headless** : option CPU = `--FeatureExtraction.use_gpu 0` /
>   `--FeatureMatching.use_gpu 0` (et non `--SiftExtraction.*`) ; définir
>   `QT_PLUGIN_PATH=<colmap>/plugins` + `QT_QPA_PLATFORM=offscreen` (sinon
>   `opengl_utils: context_.create()` échoue en headless).
> - **Convention** : Bundler `Pc = R·P + T`, caméra vers −Z → cohérente avec
>   `recon::PinholeUnprojector` (repère monde) et `AffineDepthScaler`.

### Compatibilité **vérifiée** dans le code
- Le parseur lit **exactement le format Bundler standard** : header, `nCameras nPoints`,
  puis par caméra `f k1 k2` + 3 lignes de `R` + `T`, puis par point `xyz` / `rgb` /
  liste de visibilité. (`bundle.cpp:34` `Load`, `bundle.cpp:98` `Load2`.)
- **Focale** : `f_pxl` est lue **en pixels** → directement exploitable pour la
  déprojection. (Le `f_mm` calculé est, lui, inutilisable : voir ci-dessous.)
- **Pose** : `Load2` calcule déjà `Rinv`, le centre caméra `pos = -Rᵀ·T` et la
  direction `d = Rᵀ·(0,0,-1)` selon la **convention Bundler (-Z en avant)**.
  (`bundle.cpp:178-188`.)

### À implémenter / corriger côté cgmesh
1. **Peupler `w`/`h` (et le point principal)** : le constructeur met `w=h=0`
   (`bundle_camera.cpp:8-9`) et le code de lecture des dimensions d'image dans `Load2`
   est **commenté** (`bundle.cpp:150-152,160-168`). Conséquences : pas de point
   principal (`cx=w/2, cy=h/2`) pour la déprojection/texturation, et `f_mm =
   CCDWidth·f_pxl/w = 0/0 = NaN` (`bundle.cpp:168`). → charger les dimensions (via
   `cgimg::Img` sur la liste d'images, ou depuis COLMAP).
2. **Unprojecteur en repère monde respectant la convention Bundler** : projection
   `p = -P/P_z` avec `P = R·X + T`, caméra vers `-Z`. La déprojection
   `(pixel, Z) → monde` doit utiliser `pos`/`d`/`Rinv` en cohérence (étape 5 « repère
   monde »).
3. **Liste d'images** alignée sur l'ordre des caméras pour `Load2` *(à vérifier que
   COLMAP la fournit, sinon la générer)*.
4. **Undistortion k1/k2** : soit l'appliquer dans cg (coeffs présents), soit exporter
   des images déjà non-distordues via `colmap image_undistorter` (plus simple).
5. **Robustesse `Load2`** : `malloc(nPoints*sizeof(unsigned int))` au lieu de
   `sizeof(unsigned int*)` (`bundle.cpp:124`, ré-alloué correctement en 207 mais à
   nettoyer) ; boucle de visibilité avec variable `j` masquée (`bundle.cpp:225-248`) à
   réviser. `Load` (variante simple) code en dur `f_mm = 5.4·f_pxl/640` (`bundle.cpp:64`).

---

## 8. Bugs

- ✅ **Corrigé (2026-06-26) — segfault `cgimg::Img::resize`** (`src/cgimg/image.cpp`,
  mode 1 bilinéaire) : `x1 = x0+1` / `y1 = y0+1` atteignaient `m_iWidth` / `m_iHeight`
  sur la dernière ligne/colonne → **lecture hors limites** du buffer source. Inoffensif
  sur petites images, **fatal sur grandes** (constaté : crash 541×800 → 518×518).
  **Correctif** : clamp de `x1`/`y1` à `m_iWidth-1`/`m_iHeight-1` (résultat inchangé —
  les voisins clampés ont un poids nul). **Test de non-régression** :
  `test/tu_cgimg_img.cpp` → `resize_large_bilinear_no_oob`. *(Le contournement dans
  `reconstruction-cli::preprocess` reste en place, inoffensif.)*
  - ✅ **Corrigé (2026-06-26) — bilinéaire réel** : `x/y` désormais calculés en
    arithmétique **flottante** explicite (avant : division entière → `tx=ty=0` →
    plus-proche-voisin). Le mode 1 interpole vraiment. **Note** : ceci **change la
    sortie de `resize(...,1)` pour les appelants existants** (image lissée au lieu de
    crénelée) — comportement voulu. Test : `test/tu_cgimg_img.cpp` →
    `resize_bilinear_interpolates`.

- ✅ **Corrigé (2026-06-29) — texture mouchetée (UV par face vs vertex-parallèle)** :
  le rendu cgre (`mesh_renderer` immédiat ET `BuildPolygonRenderData`/VBO) consomme des
  UV **vertex-parallèles** ; `ProjectiveTexturer` produisait des UV **par face** → UV
  erronées envoyées au GPU → rendu en « sel et poivre ». **Correctif** : le texturer
  **éclate** le mesh (chaque face → 3 sommets propres portant leurs UV) → vertex-parallèle,
  correct en rendu **et** à l'export OBJ.

- ✅ **Corrigé (2026-06-29) — décalage texture/géométrie (~80 px) : `R` transposée** :
  `Bundle::Load` applique `cam->R.SetInverse()` (`bundle.cpp:69`) → `bc->R = R_fichierᵀ`.
  `BundleSource` recopiait cette matrice telle quelle → le texturer **et** l'unprojector
  projetaient avec une rotation transposée. **Diagnostic** : projection des sommets parfaite
  sur la photo avec `R_fichier` mais pas avec sa transposée ; confirmé que `R_fichier` ==
  pose COLMAP→Bundler **exactement** (diff 0). **Correctif** (`recon_pose_bundle.cpp`) :
  transposer à la recopie (`v.R[r*3+c]=bc->R.at(c,r)`) pour restituer la vraie matrice
  monde→caméra. Résidu UV **80 px → 0 px** (vérifié).

---

## 9. Hébergement de la couche ONNX / ML — décision en attente

`reconstruction-cli` est un **prototype**. Son code géométrique/image (prétraitement,
back-projection) **migre dans `cgmesh`** (cf. étapes 1 et 5). Reste la **couche ONNX**
(session ONNX Runtime, chargement de modèle, inférence) : où la loger ?

- **Option A — nouveau module `cgml`** : regroupe tout le ML (wrapper ONNX Runtime +
  futurs modèles : segmentation, normales/depth apprises…). `cgmesh` **reste géométrie
  pure** et ne dépend pas d'ONNX Runtime ; seuls les consommateurs ML lient `cgml`.
- **Option B — interface ONNX dans `cgmesh`** : `cgmesh/onnx_*.{h,cpp}` enveloppe ORT ;
  pas de nouveau module (conforme à « tout dans `cgmesh` »), mais **`cgmesh` dépend
  d'ONNX Runtime** pour tous ses consommateurs.
- **Option C — Option B derrière un flag** `ENABLE_ONNX` : `cgmesh` ne lie ORT que si
  activé (dépendance **optionnelle**), code ML isolé dans des fichiers dédiés de `cgmesh`.

**Décision retenue (2026-06-25) : Option C.** Réponses : ampleur ML *incertaine*,
exécution *inférence pure C++ (modèles exportés)*, déploiement *selon le contexte*.
→ **Pas de module `cgml` maintenant** (périmètre incertain + préférence « tout dans
`cgmesh` »), mais **couche ONNX isolée dans `cgmesh` derrière `ENABLE_ONNX`** :
dépendance ORT **optionnelle** (test/sinaia non impactés sauf activation), **aucun
Python au runtime** (autonomie de production préservée). Le Python reste **hors ligne
uniquement** (export de modèles : `torch.onnx`, `skl2onnx`…), ce qui couvre le mode
proto. Code regroupé (p. ex. `cgmesh/onnx_*.{h,cpp}`) et **interface `IDepthSource`
agnostique** → si le ML grandit, la promotion vers un futur `cgml` = simple
déplacement de fichiers, sans redesign.

---

## 10. État de synthèse, améliorations possibles, perspectives

### 10.1 Ce qui est fait (pipeline opérationnel de bout en bout)

Voie principale **dense** validée end-to-end sur le benchy :
**images → COLMAP (SfM + MVS CUDA, `fused.ply`) → `loadPointCloudPly` → retrait plan
dominant (option) → Poisson (+trim) → texturation projective (occlusion) → sinaia / OBJ**,
et **évaluation Hausdorff** validée sur cube synthétique.

| Étape | Statut | Brique |
|---|---|---|
| 1 Prétraitement | ✅ | `cgimg/Img` + preprocess ImageNet/NCHW |
| 2 Profondeur | ✅ | MVS dense COLMAP (`fused.ply`) ; mono-depth ONNX (Depth Anything V2) |
| 3 Poses (SfM) | ✅ | `BundleSource` ← COLMAP→Bundler |
| 4 Échelle | ✅ | `AffineDepthScaler` (voie mono) |
| 5 Back-projection | ✅ | `PinholeUnprojector` (repère monde) |
| 6 Fusion | ◐ | `ConcatFuser` (union) ; `icp_align` dispo, non câblé en `IFuser` |
| 7 Normales | ◐ | orientées à la déprojection / fournies par MVS |
| 8 Surface | ✅ | `PoissonReconstructor` (cœur générique +trim) ; implicite ; heightmap |
| 9 Post mesh | ◐ | `MergeVertices`, `removeDominantPlane` ; QEM/Taubin dispo non câblés |
| 10 Texture | ✅ | `ProjectiveTexturer` + **occlusion BVH** (blending à faire) |
| 11 Évaluation | ✅ | `HausdorffEvaluator` (+ mean/RMS/p95 + heatmap), validé cube |
| 12 Visualisation | ✅ | sinaia (OBJ/PLY) |

Tout le code de reconstruction est dans `cgmesh`, derrière des **interfaces** ; les
**algorithmes génériques** (`icp`, `poisson_surface`, `mesh_metrics`, `bvh`) sont hors
namespace `recon`. **18 tests synthétiques** (`tu_cgmesh_recon.cpp`) couvrent chaque étape.

### 10.2 Ce qui pourrait être amélioré (raffinements des étapes existantes)

- **Texturation (10)** : **blending multi-vues** (mélange pondéré sur les faces vues par
  plusieurs caméras) pour effacer coutures et sauts d'exposition ; UV unwrap / atlas.
- **Évaluation (11)** : **pré-alignement par `icp_align` similarité** pour comparer à une
  référence réelle (ex. `3DBenchy.stl`) ; métriques complétude/précision ; export PLY de
  mesh coloré générique (au lieu du mini-writer du driver).
- **Fusion (6)** : envelopper `icp_align` dans un `IFuser` rigide (utile pour la voie
  mono-depth ; inutile pour la voie dense) ; downsample voxel ; intégration TSDF.
- **Post-traitement (9)** : câbler **QEM/Taubin** dans le pipeline (recopie du résultat) ;
  **bouchage de trous**.
- **Normales (7)** : estimation k-NN + propagation d'orientation (MST) pour des nuages
  non structurés (autres sources que la grille de profondeur / MVS).
- **Acquisition / qualité** : c'est le **plafond réel** observé sur le benchy (plastique
  brillant + peu de vues → bruit/trous MVS). Objet **mat**, plus de vues, masquage
  premier-plan avant MVS.
- **Robustesse build** : `Mesh::export_ply` vertices-only (ni couleurs ni faces) ;
  asymétrie `savePointCloudPly` (ASCII) / `loadPointCloudPly` (binaire) — un nuage exporté
  n'est pas relisable tel quel.

### 10.3 Fonctionnalités envisageables (extensions)

- **SfM in-engine** (sans COLMAP) : features + appariement + triangulation + bundle
  adjustment sur `cgmath` — supprime la dépendance à l'outil externe (gros chantier).
- **MVS maison** (plane-sweep / patch-match) pour s'affranchir du build CUDA de COLMAP.
- **Autres reconstructeurs de surface** : Delaunay / alpha-shapes, Ball-Pivoting
  (préservent mieux les arêtes vives que Poisson — cf. heatmap cube).
- **Pipeline ML élargi** (cf. §9) : segmentation/masquage d'objet appris, normales
  apprises, depth multi-vues apprise — sous `ENABLE_ONNX`, éventuelle promotion en `cgml`.
- **Mono-vue → objet complet** : modèles génératifs 3D (TRELLIS, SF3D) explorés mais non
  retenus ; piste image-unique distincte de la voie photogrammétrique.
- **Comparaison d'algorithmes pilotée par la métrique (11)** : maintenant que l'évaluation
  existe, balayer systématiquement les variantes (Poisson depth/trim, plan, résolution MVS,
  blending) et choisir sur chiffres plutôt qu'à l'œil.
