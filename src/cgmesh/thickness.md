# Épaisseur sur un maillage : état de l'art & faisabilité (cgmesh)

> Document produit via `$recipe sota-and-feasibility-analysis`.
> **Partie 1** — état de l'art (rôle `researcher`). **Partie 2** — faisabilité sur `src/cgmesh` (rôle `architect`).
> Date de consultation des sources : **2026-06-09**. Langue : français (termes techniques en anglais quand consacrés).
>
> **Cadrage retenu avec l'utilisateur :**
> - Définition d'« épaisseur » : **cartographier les familles** (SDF, paroi, axe médian) puis comparer.
> - Visualisation : **color map dans le viewer** *et* **export de mesh coloré**.
> - Périmètre faisabilité : **`src/cgmesh` seul** ; la color-map viewer est traitée comme « ce que cgmesh doit exposer + point d'accroche », sans audit profond de `qmlviewer`.

---

# Partie 1 — État de l'art

**Périmètre temporel** : travaux fondateurs (1967–2010) + couverture active 2019–2026.
**Question structurante** : comment définir et calculer un champ scalaire d'**épaisseur** en chaque point (sommet/face) d'un maillage triangulé 3D, et comment le **visualiser** proprement ?

## 0. Levée d'ambiguïté — trois définitions opérationnelles

Le terme « épaisseur » recouvre trois définitions distinctes, qui ne produisent pas le même champ et n'ont pas les mêmes pré-requis :

1. **Épaisseur volumique locale par rayons (Shape Diameter Function, SDF)** — diamètre local du volume mesuré en lançant des rayons vers l'intérieur depuis la surface. *Quasi-isotrope, pose-oblivious.*
2. **Épaisseur de paroi (wall/shell thickness)** — distance à la surface **opposée** d'une coque mince, typiquement par un rayon le long de la normale inverse. *Directionnelle ; domaine CAO / fabrication.*
3. **Épaisseur via axe médian / sphère inscrite (Medial Axis Transform, MAT)** — `épaisseur = 2 × rayon de la sphère maximale inscrite` tangente à la surface ; équivalent à `2 × distance au squelette` sur le champ de distance signé. *Isotrope, théoriquement la plus « pure », mais coûteuse et instable.*

> ⚠️ **Piège de vocabulaire** : « SDF » désigne dans la littérature **deux** notions différentes — *Shape Diameter Function* (famille 1, par rayons) et *Signed Distance Field* (champ de distance, famille 3). Elles sont distinctes.

## 1. Cartographie par familles

### Famille A — Shape Diameter Function & épaisseur volumique locale

**Définition fondatrice** : Shapira, Shamir & Cohen-Or (2008). En chaque point, on lance un **cône de rayons** (~120°, ~25 rayons d'après l'implémentation CGAL) le long de la **normale rentrante** ; pour chaque rayon on mesure la distance à la première intersection avec la surface opposée ; on **rejette les aberrants** (au-delà d'un écart-type autour de la médiane) puis on prend la **moyenne pondérée** (poids ∝ inverse de l'angle au cône). Post-traitement : remplissage des faces sans valeur, **lissage bilatéral**, normalisation [0,1]. Propriété : largement invariante à la pose.

Références (famille A) :
- **[Fondatrice]** L. Shapira, A. Shamir, D. Cohen-Or (2008). *Consistent mesh partitioning and skeletonisation using the shape diameter function*. **The Visual Computer** 24(4):249–259. DOI `10.1007/s00371-007-0197-5`. *(Texte intégral non lu — paywall ; paramètres corroborés via doc CGAL et dérivés ; divergence 60° vs 120° à arbitrer sur la source primaire.)*
- **[Amélioration, lue]** S. Chen et al. (2018). *Fast and robust shape diameter function*. **PLOS ONE** 13(1):e0190666. DOI `10.1371/journal.pone.0190666`. Surface offset (1 intersection/point) → ~10× plus rapide, robuste au bruit, tolère trous/fissures (pas de watertight requis).
- **[Récente]** B. Roy (2023). *Neural ShDF*. SIGGRAPH 2023 Posters. arXiv `2306.11737`, DOI ACM `10.1145/3588028.3603652`. Régression apprise des valeurs SDF, résolution-agnostique.
- **[Récente]** Puentes-Atencio et al. (2025). *Mixed 1D/2D Simplicial Approximation of Volumetric Medial Axis by Direct Palpation of Shape Diameter Function*. **Algorithms (MDPI)** 18(9):546. DOI `10.3390/a18090546`. Relie SDF et MAT.
- **[À vérifier]** *GPU-based Approaches for Shape Diameter Function Computation…* (2016). **Computers & Graphics**, ScienceDirect `S0097849316300796`. *Auteurs/DOI exacts non vérifiés.*

Implémentations de référence vérifiées : **CGAL** `Surface_mesh_segmentation` (`sdf_values()`, repose sur AABB-tree) ; **libigl** `igl/shape_diameter_function.h`.

### Famille B — Épaisseur de paroi (wall/shell thickness)

Trois variantes opérationnelles :
- **Rayon simple** : un rayon le long de −n, distance à la 1ʳᵉ intersection. Simple, mais sensible au bruit et **ambigu** (la 1ʳᵉ face touchée n'est pas toujours la paroi opposée pertinente : coins, jonctions en T).
- **Cône de rayons** = la SDF de la famille A (réduit l'ambiguïté par statistique).
- **Sphère inscrite / rolling ball** : pont vers la famille C.

Références (famille B) :
- **[Fondatrice, sphère]** T. Hildebrand, P. Rüegsegger (1997). *A new method for the model-independent assessment of thickness in three-dimensional images*. **J. of Microscopy** 185(1):67–75. DOI `10.1046/j.1365-2818.1997.1340694.x`. Épaisseur = diamètre de la plus grande sphère inscrite ; base des outils « rolling-ball ». *(Travaille sur voxels, pas directement sur maillage.)*
- **[Implémentation de référence]** R. P. Dougherty, K.-H. Kunzelmann (2007). *Computing Local Thickness of 3D Structures with ImageJ*. **Microscopy and Microanalysis** 13(S02):1678–1679. DOI `10.1017/S1431927607074430`. Plugin ImageJ « Local Thickness » (repris par BoneJ).
- **[Récente, topologique]** D. Cabiddu, M. Attene (2017). *Epsilon-shapes: characterizing, detecting and thickening thin features in geometric models*. **Computers & Graphics** ; preprint arXiv `1704.08049` *(lu via ar5iv)*. Épaisseur via opérations morphologiques (rayon max. avant changement de topologie) ; **critique explicitement la SDF** qui « peut produire des résultats inattendus » sur géométries en goulot ; orienté impression 3D.

> **Constat de couverture récente** : aucune publication académique *post-2019 spécifiquement « wall thickness sur maillage »* clairement identifiée au 2026-06-09 ; le domaine est surtout **outillé industriellement** (cf. infra) et en CT. Représentants académiques récents les plus proches : Chen 2018 et Cabiddu & Attene 2017.

### Famille C — Axe médian / sphère inscrite / champ de distance

**Définition** : `épaisseur = 2 × rayon de la sphère maximale inscrite`. Le **Medial Axis Transform** (axe médian + fonction rayon) est un descripteur complet/réversible. Notion connexe : *local feature size* (Amenta & Bern) = distance au medial axis (≈ demi-épaisseur pour une paroi mince, mais capture aussi la courbure).

Références (famille C) :
- **[Fondatrice]** H. Blum (1967). *A Transformation for Extracting New Descriptors of Shape*. MIT Press. *(Sans DOI ; date 1964/1967 à confirmer.)*
- **[Fondatrice]** N. Amenta, M. Bern (1999). *Surface Reconstruction by Voronoi Filtering*. **Discrete & Comput. Geometry** 22(4):481–504. DOI `10.1007/PL00009475`. Introduit le *local feature size*.
- N. Amenta, S. Choi, R. Kolluri (2001). *The Power Crust…*. **Comput. Geometry** 19(2–3):127–153. DOI `10.1016/S0925-7721(01)00017-7`. MAT ≈ union de boules.
- T. K. Dey, W. Zhao (2002/2003). *Approximate medial axis as a Voronoi subcomplex*. ACM SM 2002, DOI `10.1145/566282.566333`. Garantie de convergence.
- **[Régularisation]** B. Miklós, J. Giesen, M. Pauly (2010). *Discrete Scale Axis Representations for 3D Geometry*. **ACM TOG** 29(4) (SIGGRAPH). *(DOI : deux variantes vues, à confirmer.)* Réponse à l'instabilité.
- **[Récente]** D. Rebain et al. (2019). *LSMAT: Least Squares Medial Axis Transform*. **CGF** 38. arXiv `2010.05066`. Robuste au bruit par moindres carrés.
- **[Récente]** Z. Dou et al. (2022). *Coverage Axis*. **CGF** 41(2) (EG). arXiv `2110.00965`, DOI `10.1111/cgf.14484`, [code](https://github.com/Frank-ZY-Dou/Coverage_Axis). Suite : Coverage Axis++ (SGP 2024).
- **[Récente]** N. Wang et al. (2022). *Computing MAT with Feature Preservation via Restricted Power Diagram*. **ACM TOG** 41(6) (SA). arXiv `2210.13676`, DOI `10.1145/3550454.3555465`, [code MATFP](https://github.com/ningnawang/matfp). Suite : MATTopo (SA 2024).

> ⚠️ **CGAL ne fournit PAS l'épaisseur** par son module de squelettisation (`Mean_curvature_flow_skeletonization`) : il produit un **squelette curviligne 1D sans rayon**, donc inexploitable tel quel pour une épaisseur. Aucun module CGAL « MAT 3D avec rayons » de référence confirmé au 2026-06-09.

## 2. Comparaison transversale des familles

| Critère | A · SDF (cône de rayons) | B · Paroi (rayon simple −n) | C · MAT / sphère inscrite |
|---|---|---|---|
| Type d'épaisseur | quasi-isotrope, pose-oblivious | **directionnelle** (oblique) | isotrope, « pure » |
| Robustesse au bruit | bonne (rejet aberrants + lissage) ; très bonne (Chen 2018) | **faible** (1 rayon) | **faible** brute ; bonne après régularisation (LSMAT, Coverage Axis) |
| Continuité du champ | lissée a posteriori | discontinue (sauts aux arêtes) | continue le long des sheets, support instable |
| Coût | moyen→élevé (N faces × ~25 rayons) | **faible** (1 ray-cast/face) | **élevé** (Voronoï/RPD/optim.) |
| Pré-requis normales | orientation cohérente | orientation cohérente | — (volume/voxels) |
| Pré-requis watertight | souhaité (Chen : non) | fortement souhaité (sinon le rayon « fuit ») | watertight + échantillonnage dense |
| Ambiguïté parois multiples | réduite (statistique) | **forte** | faible (sphère = plus proche paroi) |
| Implémentation de réf. | CGAL, libigl | viewers CAO, BVH/AABB | Power Crust, MATFP, Coverage Axis |

## 3. Visualisation d'un champ scalaire (épaisseur) sur un maillage

**Palette** : utiliser une colormap **perceptuellement uniforme à luminance monotone** — **viridis/magma** (grandeur séquentielle comme une épaisseur absolue), ou une **divergente** type *cool/warm* de Moreland si l'on visualise un écart à une cible. **Éviter jet/rainbow** : absence d'ordre perceptuel, luminance non monotone, faux gradients (Borland & Taylor 2007 ; nuance : Reda et al. 2023, « pas toujours mauvaise » selon la tâche).

**Échelle** : normalisation **linéaire min-max** par défaut, avec **écrêtage des outliers** (percentiles 2/98 %) ; passer en **quantile/égalisation d'histogramme** seulement si la distribution est très asymétrique (l'axe des valeurs est alors distordu). Toujours une **barre de couleur légendée**.

**Maillage** : **per-vertex + interpolation de Gouraud** pour un champ continu (épaisseur), maillage assez dense pour ne pas lisser les extrema (Gouraud = interpolation linéaire → peut atténuer un pic intra-face) ; **per-face** si l'on veut la valeur non interpolée. **Isolignes** (marching-squares sur triangles, sans ambiguïté de selle) en surcouche pour matérialiser un seuil d'épaisseur minimale.

**Export** : **PLY** = format de référence — couleur par sommet (`red/green/blue` en uchar) **et** champ scalaire brut conservé via propriété custom (ex. `property float thickness`), ce qui permet de re-coloriser plus tard. **OBJ** ne supporte la couleur par sommet que via l'extension non standard `v x y z r g b` (interop non garantie ; MeshLab OK).

**Rendu temps réel** : appliquer la colormap en **fragment shader** via une **texture 1D (LUT)** indexée par le scalaire normalisé ; ne stocker que le scalaire par sommet (pas la couleur figée) pour changer palette/échelle à la volée.

Références (visualisation) : Borland & Taylor (2007) *IEEE CG&A* 27(2):14–17, DOI `10.1109/MCG.2007.323435` ; Moreland (2009) *ISVC*, LNCS 5876:92–103, DOI `10.1007/978-3-642-10520-3_9` ; Harrower & Brewer (2003) *Cartographic Journal* 40(1):27–37, DOI `10.1179/000870403235002042` ; Zhou & Hansen (2016) *IEEE TVCG* 22(8):2051–2069, DOI `10.1109/TVCG.2015.2489649` ; Reda et al. (2023) *IEEE CG&A*, DOI `10.1109/MCG.2023.3246111` *(auteurs à confirmer)*. PLY : spec G. Turk (~1994).

## 4. Algorithmes utilisés par les logiciels (étiquetés, hors quota académique)

*(Recherche dédiée, consultation 2026-06-09. Convention : **[documenté éditeur]** / **[déduit ou source tierce]** / **[non documenté]**.)*

Tous les logiciels d'analyse d'épaisseur se ramènent à **deux familles d'algorithmes** ; les éditeurs propriétaires documentent rarement laquelle, mais l'**open source** en révèle l'implémentation exacte.

### 4.1 Les deux méthodes (consensus industriel)

| | **Méthode RAYON** (ray-casting) | **Méthode SPHÈRE** (sphère inscrite max. / rolling ball) |
|---|---|---|
| Principe | Rayon depuis le point le long de la **normale inverse** → 1ʳᵉ intersection opposée ; épaisseur = distance | Plus grande **sphère inscrite** dans la matière contenant le point ; épaisseur = diamètre |
| Propriétés | Rapide ; **directionnelle**, distribution **discontinue** ; peut sur-estimer là où la pièce paraît fine | Coûteuse ; distribution **continue**, locale/intrinsèque ; **atteint les zones manquées par le rayon** |
| Usage | Détecter parois **minces** | Détecter zones **épaisses**, géométries complexes, congés |
| Accélération | uniform grid / **AABB-tree / BVH** | **k-d tree** / BVH (maillage) ; **EDT** sur grille (voxels) |

Sources éditeur explicites : whitepaper **GeomCaliper** (« Efficient Wall Thickness Analysis Methods for Casting Parts », définit les deux + « uniform grid pour le rayon, k-d tree pour la sphère ») ; blog **Onshape** (rayon directionnel/discontinu vs sphère « guaranteed continuous ») ; **Volume Graphics/Hexagon** (CT, la sphère « reach areas missed by the Ray method »).

### 4.2 Qui propose quoi

- **Les deux méthodes — [documenté]** : GeomCaliper, Volume Graphics/Hexagon (CT/voxel), Glovius, **Analysis Situs** (open source). **Geomagic Control X / Design X** : « ray firing / sphere growing » **[source tierce]** (aide Oqton inaccessible).
- **Méthode non exposée — [non documenté]** : **Materialise Magics & 3-matic** (le leader AM ne publie pas son algo), **Autodesk netfabb** (parle de « surface opposée » → rayon *déduit*), **ANSYS SpaceClaim** (seuls min thickness + sample spacing documentés), **Siemens NX**, services 3D-printing (i.materialise, Shapeways, Sculpteo — boîtes noires, ne publient que seuils et limites de détection).
- **Paradigme différent — [documenté partiel]** : **Autodesk Moldflow** mesure l'épaisseur par appariement des deux peaux d'un maillage *Dual Domain* (plasturgie), hors débat rayon/sphère.

### 4.3 Ce que l'open source révèle (implémentation réelle, vérifiable)

Les « détails » que les éditeurs cachent sont exactement ceux des deux papiers fondateurs :

**Méthode rayon = Shape Diameter Function (Shapira 2008)** — en pratique un *cône* de rayons :
- **CGAL** `compute_sdf_values` : 25 rayons/facette, cône **120° (2π/3)** autour de la normale inverse, **rejet d'aberrants** + moyenne, héritage des voisins, **lissage bilatéral**, normalisation [0,1], sur **AABB-tree**. [vérifié doc]
- **libigl** `shape_diameter_function.h` : même principe, cône **180°** et **moyenne uniforme** ; backend AABB ou Embree ; callback `shoot_ray` enfichable. [vérifié code]
- **MeshLab** : filtre SDF présenté comme mesure de « thickness », cœur **GPU par depth peeling**. [vérifié doc/blog]

**Méthode sphère = Hildebrand & Rüegsegger (1997)** — sur voxels :
- **ImageJ « Local Thickness » / BoneJ** : **EDT** (transformée de distance euclidienne, algo Saito-Toriwaki 1994) → **crête de distance** (centres des sphères maximales) → **carte d'épaisseur**. [vérifié doc]
- **Analysis Situs** : version surfacique *shrinking spheres* (sphère initialisée au milieu rayon/intersection, itérée jusqu'à stabilisation), sur **BVH/AABB**. [vérifié doc]

**Dichotomie d'accélération** : maillage → AABB-tree/BVH ; volume voxelisé (CT) → EDT séparable sur grille. Les libs généralistes (**trimesh, Open3D, VTK, PyMesh**) n'offrent **pas** de fonction « épaisseur » dédiée — seulement des briques (ray-casting, distance queries) à assembler. [vérifié]

### 4.4 Ce qui reste opaque

Aucune source ne fournit de pseudo-code propriétaire complet. Materialise, netfabb, NX, SpaceClaim et les services 3D ne publient ni méthode ni paramètres. Pour Geomagic et VGStudio, le rattachement aux familles est *déduit* (ou via tiers), non confirmé en source primaire éditeur.

**Lien avec `cgmesh`** : la **méthode rayon/SDF** correspond exactement aux briques déjà présentes (`GetIntersectionWithRay` + octree, cf. Partie 2 §1) — c'est ce que CGAL/libigl posent au-dessus d'un AABB-tree. La **méthode sphère** exigerait une EDT 3D, absente du code (cf. M3, Partie 2 §2).

Sources : [GeomCaliper whitepaper](https://geomcaliper.geometricglobal.com/files/2017/05/Whitepaper-Efficient-Wall-Thickness-Analysis.pdf) · [Onshape – Thickness Analysis](https://www.onshape.com/en/blog/thickness-analysis) · [Volume Graphics/Hexagon – Spheres and Rays](https://volumegraphics.hexagon.com/en/explore-nde/our-stories/spheres-and-rays-wall-thickness-analysis.html) · [CGAL – Surface Mesh Segmentation](https://doc.cgal.org/latest/Surface_mesh_segmentation/index.html) · [libigl – shape_diameter_function.h](https://github.com/libigl/libigl/blob/main/include/igl/shape_diameter_function.h) · [Analysis Situs – thickness](https://analysissitus.org/features/features_thickness.html) · [BoneJ – Thickness](https://bonej.org/thickness) · [Dougherty – Local Thickness (ImageJ)](https://www.optinav.info/Local_Thickness.htm).

## 5. Frontières & limites de la revue

- **Consensus** : pour un maillage triangulé fermé, la **SDF par rayons** est la méthode de référence pratique (implémentations matures CGAL/libigl). Pour la visualisation, le rejet de jet/rainbow au profit de colormaps perceptuelles est solidement établi.
- **Débats / angles morts** : ambiguïté « surface opposée » non entièrement résolue (epsilon-shapes vs SDF en goulot) ; MAT puissant mais instable, sans implémentation CGAL de référence pour l'épaisseur ; peu de jalon académique purement « wall thickness sur maillage » post-2019 (migration vers l'outillage AM).
- **Limites** : plusieurs textes intégraux non lus (paywall : Shapira 2008, Hildebrand 1997, Dougherty 2007, Amenta-Bern 1999) — apports résumés via abstracts/notices/doc secondaire. Quelques DOI marqués « à vérifier » ci-dessus. Lectures intégrales confirmées : Chen 2018, Cabiddu & Attene 2017, et les preprints arXiv de la famille C/visualisation. **≥ 18 publications académiques distinctes** mobilisées (quota ≥ 12 atteint).

## Méthodes retenues

*(Contrat d'interface consommé par la Partie 2.)*

- **M1 — SDF par cône de rayons (Shapira 2008, robustifiée façon Chen 2018)** — méthode principale. Pré-requis : ray-cast mesh, octree, normales par sommet/face orientées, normalisation par la diagonale de la bbox.
- **M2 — Épaisseur de paroi par rayon simple le long de −n** — MVP minimal (cas particulier de M1 à 1 rayon). Mêmes briques que M1.
- **M3 — Épaisseur via sphère inscrite / champ de distance (MAT)** — cible avancée. Pré-requis lourds : voxelisation + transformée de distance 3D (EDT) **ou** structure de Voronoï/MAT ; instable, à régulariser.
- **V1 — Visualisation color map** — champ scalaire par sommet → colormap **perceptuelle (viridis)** → couleurs par sommet rendues dans le viewer (Gouraud). Barre de couleur + écrêtage outliers.
- **V2 — Export mesh coloré** — **PLY** avec couleurs par sommet + propriété scalaire `thickness` custom.

---

# Partie 2 — Faisabilité sur `src/cgmesh`

**Rôle `architect`** (lecture seule du dépôt ; pas d'écriture de code d'implémentation). Langage dominant : **C++** (style historique : `vec3` bruts, pointeurs nus, `std::vector` introduits récemment).

## 1. Cartographie de l'existant (vérifié par lecture du code)

### Briques réutilisables — calcul (M1/M2)

| Besoin | Existant dans `src/cgmesh` | Référence |
|---|---|---|
| **Ray-cast maillage** | `Mesh::GetIntersectionWithRay(o,d,*t,i,n)` + variante octree `GetIntersectionWithRayInOctree(...)` — ⚠️ **back-face culling** (cf. note ci-dessous), inutilisable telle quelle pour un rayon interne | `mesh.cpp:1448`, `mesh.cpp:1385` |
| **Accélération spatiale** | `Mesh::m_pOctree` + `Octree::BuildForTriangles(...)`, requêtes `GetClosestPoints` / `GetClosestIndicesPoints` | `mesh.h:405`, `octree.h:26`, `octree.h:40,45` |
| **Test bbox du rayon** | `Mesh::GetIntersectionBboxWithRay` | `mesh.h:119` |
| **Normales par sommet/face** + inversion (pour −n) | `Normals::EvalOnVertices/EvalOnFaces`, `invert_vertices_normales` ; `Mesh::ComputeNormals` | `normals.h:27-30`, `mesh.h:317` |
| **Barycentre de face** (point de tir pour SDF par face) | `Mesh::GetFaceBarycenter` | `mesh.h:217` |
| **Normalisation d'échelle** | `bbox()`, `bbox_diagonal_length()`, `GetLargestLength()` | `mesh.h:304-306` |
| **Détection bord / manifold** (pré-requis watertight) | `Mesh::GetTopologicIssues`, `Mesh_half_edge::is_manifold/is_border` | `mesh.h:268`, `mesh_half_edge.h:44-45` |
| **Voisinage / lissage du champ** | demi-arêtes (`Mesh_half_edge`), `get_n_neighbours`, poids cotangente | `mesh_half_edge.h:73,74` |

> ⚠️ **Note — back-face culling (découverte à l'implémentation)** : `Triangle::GetIntersectionWithRay` (`geometry.cpp:616`, `if (b>=0) return 0`) ne retient que les faces dont la normale fait face au rayon. Sur un maillage orienté vers l'extérieur, un rayon tiré vers l'intérieur (−n) atteint la **face arrière** de la paroi opposée → **rejetée**. La primitive (conçue pour le rendu) est donc **inexploitable** pour un rayon interne. L'étape 1 implémente une intersection rayon-triangle **sans culling** (Möller–Trumbore). L'étape 2 (octree + cône) devra donc soit ajouter une requête octree sans culling, soit paramétrer le culling de `Triangle`.

### Briques réutilisables — visualisation (V1/V2)

| Besoin | Existant | Référence |
|---|---|---|
| **Scalaire par sommet → couleurs** | `Mesh::InitVertexColorsFromArray(float* array, char* defined)` — normalise min/max, masque les indéfinis (→ noir), applique une colormap | `mesh.cpp:190`, normalisation `mesh.cpp:~200-245` |
| **Pattern identique déjà en place** | `Mesh::InitVertexColorsFromCurvatures(CurvatureType)` calcule un champ par sommet puis appelle `InitVertexColorsFromArray` | `mesh.cpp:159` |
| **Stockage couleurs par sommet** | `Mesh::m_pVertexColors` (`std::vector<float>`, RGB ∈ [0,1]) | `mesh.h:395` |
| **Pipeline de rendu consommant les couleurs** | `Mesh::BuildPolygonRenderData()` remplit `PolygonRenderData::colors` par render-vertex (chemin VBO) | `mesh.h:190`, `mesh.cpp:666,740` |
| **Colormap disponible** | `color_jet()` (cgimg) | `cgimg/color.cpp:230`, `color.h:55` |
| **Export/Import PLY** | `Mesh::export_ply` / `import_ply` (via rply) | `mesh_io.cpp:1620`, `:1560+` |

### Voxels (piste M3)

`Voxels` existe avec activation, `set_data`/`get_data` (champ `m_fData` par voxel), **dilation morphologique** (`voxels.h:64`), threshold, `ToMesh()`. Mais **pas de transformée de distance euclidienne (EDT)** ni de calcul d'axe médian. (Vérifié par lecture de l'en-tête `voxels.h`.)

## 2. Ce qui manque (et discipline vérifié / hypothèse)

1. **Algorithme SDF lui-même** (M1) — **n'existe pas**. À écrire : génération d'un cône de directions autour de −n, tir de K rayons via `GetIntersectionWithRayInOctree`, agrégation robuste (médiane + rejet d'aberrants + moyenne pondérée), puis lissage sur le voisinage demi-arête. **Vérifié** : aucune fonction `sdf`/`thickness`/`diameter` trouvée dans `src/cgmesh` (grep).
2. **`export_ply` n'écrit PAS les couleurs** — **vérifié par lecture** (`mesh_io.cpp:1620-1657`) : seules `x,y,z` et `nx,ny,nz` sont déclarées/écrites, alors que la *lecture* PLY gère `diffuse_red/green/blue` (`mesh_io.cpp:1595-1597`). ⇒ V2 nécessite d'ajouter les propriétés couleur (et idéalement une propriété scalaire `thickness`) à l'export.
3. **Pas de colormap perceptuelle** — **vérifié** : seul `color_jet` existe (`cgimg/color.cpp:230`). `InitVertexColorsFromArray` appelle `color_jet` en dur (`mesh.cpp:245`). ⇒ V1 « propre » impose d'ajouter `color_viridis` (table 256 entrées) dans `cgimg/color.*` et de rendre la colormap paramétrable.
4. **Robustesse aux pré-requis** (M1/M2) — *hypothèse à valider par exécution* : sur maillage non watertight ou normales mal orientées, le rayon « fuit » → valeurs aberrantes. La détection existe (`GetTopologicIssues`, `is_border`) mais **le garde-fou n'est pas branché** sur un futur calcul d'épaisseur. À router vers `debugger` si comportement anormal observé.
5. **Famille C (M3)** — **largement manquante** : ni EDT 3D, ni Voronoï/MAT. Coût d'implémentation élevé et hors périmètre raisonnable d'un MVP ; à considérer seulement comme extension.
6. **Barre de couleur / légende & isolignes** — non examiné côté viewer (périmètre `src/cgmesh` seul). **Déclaré non examiné** : le rendu de la légende et des isolignes relève de `qmlviewer`/`scene` et n'a pas été audité.

## 3. Plan d'implémentation incrémental (MVP → cible)

> Frontière `architect`/`developer` : ci-dessous = **plan**, pas de code. Chaque étape implémentée passe ensuite par `developer` puis **`$recipe code-review` obligatoire**.

- **Étape 1 — MVP (M2 + V1) — ✅ RÉALISÉE** : module `thickness.{h,cpp}` (`MeshAlgoThickness`). `ComputeWallThickness(Mesh&, std::vector<float>& out, std::vector<char>& defined)` : pour chaque sommet, 1 rayon le long de −n ; `defined[i]=0` si pas d'intersection. `ColorizeWallThickness(...)` enchaîne avec `InitVertexColorsFromArray` (rendu viewer immédiat). Tests : `test/tu_cgmesh_thickness.cpp` (8 cas, dont épaisseur exacte sur boîte fermée, garde anti-auto-intersection, fuite→indéfini, octree multi-feuilles). **Écart vs conception** : `GetIntersectionWithRay` n'a **pas** pu être réutilisé (voir note culling) ; l'intersection rayon-triangle est faite par un Möller–Trumbore **sans culling**.
  - **Accélération (réalisée)** : la force brute O(V·F) initiale a été remplacée par une **traversée octree** (`Octree::BuildForTriangles` + traversée maison sans culling, pruning AABB) → ~**O(V·log F)**. Octree construit une fois par appel sur la géométrie courante. Corrige au passage une fuite de `~Octree` (`m_pTriangles` non libéré + `delete`→`delete[]`).
  - **Câblage viewer (réalisé)** : mode « Épaisseur » branché dans `qmlviewer` (`CgreQuickItem::evaluateThickness`, outil QML `evaluable`).
- **Étape 2 — SDF robuste (M1) — ✅ RÉALISÉE** : `MeshAlgoThickness::ComputeShapeDiameter(...)` — **cône de K rayons** (échantillonnage déterministe : angle ∝ √k, azimut par angle d'or) autour de −n, réutilisant la traversée octree no-cull ; **agrégation robuste** (médiane + rejet à 1σ + moyenne pondérée par cos de l'angle) ; **lissage 1-anneau** (Laplacien sur adjacence dérivée des faces, restreint aux sommets définis). Paramètres : `numRays=16`, `coneHalfAngleDeg=60`, `smoothIterations=1`. Dégénère exactement vers M2 si `numRays=1, cône=0, lissage=0` (testé). Câblé dans le viewer (`evaluateThickness("SDF (cône)")`, méthode par défaut du select). Tests : 5 cas SDF (équivalence M2, bornes du cône, déterminisme, lissage, gardes).
- **Étape 3 — V1 propre — ✅ RÉALISÉE** : palette perceptuelle **cool-warm de Moreland** ajoutée dans `cgimg/color.*` (`color_coolwarm`), utilisée **inversée** → **rouge = mince, bleu = épais** (convention fabrication, cf. SOLIDWORKS/netfabb ; remplace le `jet` trompeur). Colorisation dédiée `colorizeThicknessField` dans `thickness.cpp` (n'altère plus `InitVertexColorsFromArray`, donc les courbures gardent leur palette). **Échelle paramétrable** : `Colorize*` acceptent `scaleMin/scaleMax` (auto = min/max réel si non fournis ; sinon écrêtage sur [min,max]). Viewer : toggle « Échelle automatique » (défaut) + sliders min/max ; légende rouge→bleu alignée sur le rendu. Tests : sens couleur (mince=rouge/épais=bleu) + clamp d'échelle.
- **Étape 4 — V2 export** : étendre `Mesh::export_ply` pour écrire `red/green/blue` (uchar) depuis `m_pVertexColors` + une propriété `float thickness` depuis le tableau scalaire. PLY = format recommandé ; OBJ `v x y z r g b` en option seulement si demandé.
- **Étape 5 (optionnelle, cible) — M3** : voxelisation + EDT 3D (sphère inscrite) ou portage d'une lib MAT. **Lourd** ; à arbitrer séparément.

## 4. Trade-offs explicites

- **M2 vs M1** : M2 (1 rayon) est trivial et rapide mais bruité et ambigu aux arêtes ; M1 (cône) coûte ~K× plus cher mais donne un champ exploitable. → Livrer M2 d'abord (boucle de feedback courte), faire évoluer vers M1.
- **Per-vertex vs per-face** : per-vertex se branche directement sur `m_pVertexColors`/Gouraud (gratuit côté rendu) ; per-face est plus fidèle à la valeur brute mais impose une duplication de sommets (cf. chemin n-gon de `BuildPolygonRenderData`). → per-vertex pour le MVP.
- **viridis vs jet** : jet est déjà là (coût nul) mais trompeur ; viridis exige une table 256 entrées (~petit) mais modifie `cgimg`. → Ajouter viridis sans retirer jet (rétro-compatibilité).
- **Octree** : le (re)construire pour le ray-cast a un coût amont mais amortit massivement N tirs de rayons. → construire une fois, réutiliser.

## 5. Handoff

- Implémentation → **`developer`** (+ contexte `cpp-specialist`, skills `cpp-coding-standards`, `cmake-patterns`), puis **`$recipe code-review`** obligatoire (cf. `dev-task`).
- Points #4 (fuite de rayon) et toute valeur d'épaisseur suspecte → **`debugger`** pour reproduction (ce sont des **hypothèses** tant que non exécutées).
- Décision « modifier `cgimg` pour viridis » → à confirmer avec l'utilisateur (le périmètre validé est `src/cgmesh` seul).

## Cohérence Partie 1 ↔ Partie 2

Les méthodes instruites en faisabilité (M1, M2, M3, V1, V2) sont exactement les « Méthodes retenues » de la Partie 1. La famille A (SDF) est la voie principale car ses pré-requis (ray-cast + octree + normales) sont **déjà présents** ; la famille B (rayon simple) sert de MVP ; la famille C (MAT) est cohérente avec le SOTA (puissante mais coûteuse/instable, sans support CGAL) et donc reléguée en extension. La visualisation suit le consensus du SOTA (colormap perceptuelle, per-vertex+Gouraud, export PLY), avec deux écarts identifiés dans le code existant (jet en dur, export PLY sans couleurs) traités aux étapes 3 et 4.
