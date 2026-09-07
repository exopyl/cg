# Exporter les matériaux en OBJ et GLB depuis `maker` — étude de faisabilité

> **Statut** : **G0 à G5 exécutés** (G0–G3 le 2026-09-06, G4–G5 le 2026-09-07) ; **G6 reste à
> l'état de conception**, et le demeure tant que cgimg n'a pas d'encodeur PNG. Voir l'état
> d'exécution en tête du §6 et le journal du §7.
> **Date** : 2026-09-06. **Périmètre** : `cgmesh` (E/S), `maker` (pont WASM + pages web).
> **Origine** : le chantier « import SVG multi-formes avec couleurs » (K0→K5,
> `src/cgmesh/docs/svg_couleurs_faisabilite.md`) a livré 39 matériaux visibles sur le Tigre et a
> **exclu nommément l'export** de son périmètre (§K5, « chantier distinct »). C'est ce chantier.

**Convention de statut, appliquée à chaque affirmation :**

| Marque | Sens |
|---|---|
| **[V]** | vérifié par **lecture du code**, référence `fichier:ligne` donnée |
| **[M]** | **mesuré**, instrument nommé |
| **[H]** | hypothèse, avec ce qui la confirmerait |
| **[E]** | estimation, avec la méthode et l'ordre de grandeur |

---

## 1. Résumé exécutif

### 1.1 Trois prémisses de la commande sont **infirmées par lecture du code**

C'est le résultat le plus important de cette étude : le chantier est **beaucoup plus petit** que la
commande ne le suppose, et son centre de gravité n'est pas là où elle le place.

| Prémisse de la commande | Constat | Référence |
|---|---|---|
| « OBJ + MTL font deux fichiers — quel écrivain ZIP, faut-il en vendoriser un ? » | **Un écrivain ZIP existe, est écrit, testé et en production.** `ZipManager` (stored, sans zlib), consommé par `MeshIO::export_obj_zip_bytes`. | `src/cgmesh/zip_manager.h:35-58`, `src/cgmesh/mesh_io_obj.cpp:878-917` **[V]** |
| « `exportObj` appelle `meshToObj`, l'écrivain minimal sans `mtllib` » | **Faux.** `exportObj` délègue à `MeshIO::export_obj_zip_bytes` et rend une **archive ZIP OBJ+MTL**. Les pages de FORMES exportent donc **déjà** leurs matériaux. | `maker/wasm_api.cpp:481-513`, `maker/web/js/exporters.js:42-47` **[V]** |
| « aucune trace de glTF/GLB dans le dépôt » | **Faux.** `extern/tinygltf/tiny_gltf.h` (280 240 o) est vendorisé, `TINYGLTF_IMPLEMENTATION` est compilé dans `src/cgmesh/vmeshes_io.cpp:884`, et un **importeur** glTF **et GLB** tourne en natif (`VMeshesIO::import_gltf`). L'**écrivain** GLB de tinygltf est présent et inutilisé. | `src/cgmesh/vmeshes_io.cpp:1002-1013`, `extern/tinygltf/tiny_gltf.h:1454` et `:8496-8600` **[V]** |

### 1.2 Où est réellement le manque

Le trou est **circonscrit aux pages GABARIT** — celles pilotées par un graphe, dont `svg.html` fait
partie, donc **exactement celle du Tigre**.

```
PAGE DE FORMES (shapes/relief/pixels/gothic)   PAGE GABARIT (svg/text/template)
  shell.js:181  downloadObj                      template.js:738  downloadObjBtn
        |                                              |
  exporters.js:42  Module.exportObj                Module.graphExportObj(0)
        |                                              |
  wasm_api.cpp:481  exportObj                     wasm_api.cpp:785  graphExportObj
        |                                              |
  MeshIO::export_obj_zip_bytes                    meshToObj  (wasm_api.cpp:194)
        |                                              |
  ZIP { model.obj, model.mtl }  = COULEURS        .obj minimal  = SANS MATÉRIAUX
```

**[V]** `maker/wasm_api.cpp:194-217` (`meshToObj` : `v` et `f` seulement — ni `vt`, ni `vn`, ni
`usemtl`, ni `mtllib`), `:785-798` (`graphExportObj`), `maker/web/js/template.js:397` puis `:738-742`.

Autrement dit : **le chemin qui porte les 39 matériaux du Tigre est le seul qui ne sache pas les
écrire**, alors que le code qui sait le faire est à trois lignes de là.

### 1.3 Deux défauts vérifiés de l'écrivain OBJ existant, indépendants de `maker`

Découverts en lisant `mesh_io_obj.cpp` en entier. Ils touchent aussi `sinaia` et `vecna`.

1. **`MATERIAL_COLOR_ADV` n'est jamais écrit dans le `.mtl`.** Le `switch` de l'écrivain traite
   `MATERIAL_COLOR` (`:707-731`) et `MATERIAL_TEXTURE` (`:732-751`), puis `default: break;`
   (`:752-753`). Or `import_mtl` instancie **`MaterialColorExt`** pour tout `newmtl` lu
   (`:114`), dont `GetType()` rend `MATERIAL_COLOR_ADV` (`material.h:203`). **[V]**
   **Conséquence** : *importer un OBJ+MTL puis le ré-exporter produit un `.obj` plein de `usemtl` et
   un `.mtl` qui ne contient que trois lignes d'en-tête.* Le modèle ressort **sans couleur**, et
   l'archive ZIP embarque un `.mtl` inutile (le `slurpFile` le trouve non vide, `:908-910`).
   C'est le symptôme exactement décrit par le commentaire de `objMaterialName` (`:44-48`) pour une
   *autre* cause, déjà corrigée. Le même symptôme a donc **deux** causes, et la seconde est vivante.

2. **Une texture est référencée mais jamais livrée.** `export_obj` écrit
   `map_Kd <pMaterialTexture->GetFilename()>` (`:748`), et `export_obj_zip_bytes` ne met dans
   l'archive que le `.obj` et le `.mtl` (`:902-916`). **[V]** Le `map_Kd` d'une archive exportée
   **pend dans le vide**. `MaterialTexture` ne conserve d'ailleurs pas les octets d'origine, seulement
   l'image décodée et un nom de fichier (`material.h:266-274`) — ré-encoder est donc obligatoire, et
   cgimg **n'a d'encodeur ni PNG ni JPEG** : `ImgIO::export_png` est un `return -1` qui l'annonce
   (`src/cgimg/image_io_png.cpp:52-59`), et `ImgIO::save` n'accepte en écriture que `.bmp`, `.tga` et
   `.pbm`/`.pgm`/`.ppm` (`image_io.cpp:58-76`). **[V, corrigé le 2026-09-06 — la rédaction initiale
   « cgimg n'a pas d'encodeur » concluait une ABSENCE depuis le contenu de `extern/stb/`, sans ouvrir
   `image_io.h`. Trois encodeurs y étaient.]**

   **Tranché en G1** : la ligne `map_Kd` n'est écrite **que si l'image qu'elle nomme est présente à
   côté du `.mtl`**. Un export déposé auprès de ses textures d'origine garde donc sa référence ; une
   archive, qui ne livre aucune image, n'en écrit pas. Aucune image n'est ré-encodée : le faire
   écraserait le fichier source lors d'un export dans son propre répertoire.

### 1.4 Recommandation en une ligne

**Réutiliser l'écrivain OBJ/MTL existant partout (supprimer le doublon `meshToObj`), le compléter sur
ses deux trous, puis ajouter un écrivain GLB en C++ dans `cgmesh` appuyé sur `Mesh::materialRanges`
et sur le tinygltf déjà vendorisé.** Le ZIP est acquis ; la contrainte o3dv invoquée pour justifier
l'OBJ minimal **n'existe pas** (§4.3).

---

## 2. Cartographie de l'existant

### 2.1 Réutilisable **tel quel**, sans une ligne de code

| Élément | Emplacement | Ce qu'il fait déjà |
|---|---|---|
| Écrivain OBJ + MTL compagnon | `src/cgmesh/mesh_io_obj.cpp:549-761` | `mtllib` (`:573-592`), `usemtl` + `o` optionnel à chaque changement de matériau (`:620-639`), `f v/vt` (`:654-675`), `l`/`p` (`:680-683`), puis le `.mtl` : `newmtl`/`Ka`/`Kd`/`Ks`/`Ns`/`d`/`illum` (`:696-758`) |
| Nom de matériau unique entre `usemtl` et `newmtl` | `:53-66` | blancs vers `_`, repli `material_<index>` |
| Garde anti-traversée sur `mtllib`, `map_Kd`, `refl` | `:151-156`, `:196-201`, `:328-335` | `io_guard::isContainedRelativePath` |
| Écrivain ZIP générique, sans dépendance | `src/cgmesh/zip_manager.h:35-58`, `zip_manager.cpp` | entrées *stored*, CRC-32 exposé, horodatage figé (sortie déterministe) |
| OBJ+MTL en une archive, **en mémoire** | `mesh_io_obj.cpp:878-917` | répertoire temporaire privé (MEMFS sous Emscripten), `export_obj` inchangé, relecture, `ZipManager::BuildStored` |
| Pont WASM d'export zippé | `maker/wasm_api.cpp:481-513` (`exportObj`), `:426-466` (`exportPixelBlocks`) | rend `{ name, bytes }`, `bytes` en `typed_memory_view` |
| Téléchargement navigateur | `maker/web/js/exporters.js:28-47` | `saveBlob`, `downloadObj` (copie hors du tas WASM, Blob `application/zip`) |
| Indices **déjà groupés par matériau** | `src/cgmesh/mesh.h:144-165`, `mesh.cpp:757-928` | `PolygonRenderData` (positions, normals, texCoords, colors, indices, **materialRanges**) ; `buckets` est une `std::map` (`:809`), donc les plages sortent **triées par `materialId`**, `MATERIAL_NONE` (valeur `(unsigned)-1`) en dernier |
| Charge utile matériaux vers JS | `maker/mesh_payload.{h,cpp}` | `groups` (plages) + `materials` (`texture`/`color`/`none`), une entrée **par plage** |
| Alpha porté par le matériau | `src/cgmesh/material.h:96`, `:106` | `MaterialColor(r,g,b,a=255)`, `GetFloatAlpha()` |
| Alpha SVG effectivement rempli | `src/cgmesh/import_svg.cpp:221-236`, `:738-742` | `fill-opacity` multiplié par l'opacité de groupe, **stocké** dans le matériau |
| tinygltf, écrivain GLB compris | `extern/tinygltf/tiny_gltf.h:1454` (`WriteGltfSceneToStream`), impl. `:8585` ; conteneur GLB `:8496-8557` | en-tête `glTF`, version, longueur totale, chunks `JSON`/`BIN`, **padding 4 octets déjà géré** |
| nlohmann/json, **déjà dans le build WASM** | `extern/nlohmann/`, utilisé par `src/cgmesh/parameterized_shapes.cpp:17`, lui-même listé `src/cgmesh/CMakeLists.txt:104` | le coût binaire du JSON est **déjà payé** |

### 2.2 Ce qui manque réellement

| Manque | Site |
|---|---|
| Un export OBJ+MTL sur le chemin **graphe** | `maker/wasm_api.cpp:785-798`, `maker/web/js/template.js:738-742` |
| Un écrivain OBJ rendant la **chaîne** (pas seulement le ZIP), pour supprimer `meshToObj` | `mesh_io.h:39-60` (il manque un `export_obj_bytes`) |
| `MATERIAL_COLOR_ADV` dans le `.mtl` | `mesh_io_obj.cpp:752-753` |
| L'image d'une `MaterialTexture` dans l'archive | `mesh_io_obj.cpp:902-916` + absence d'encodeur PNG |
| Tout écrivain glTF/GLB | néant (l'importeur seul existe) |
| L'alpha dans la charge utile JS | `maker/mesh_payload.cpp:60-66` (`r`, `g`, `b` seulement) |

### 2.3 Contraintes héritées, lues dans les déclarations

- **`export_obj` écrit par NOM DE FICHIER** (`FILE*`, `fprintf`) : toute variante en mémoire passe
  par un fichier. Sous Emscripten c'est MEMFS, donc aucun disque — c'est déjà le montage retenu et
  documenté (`mesh_io_obj.cpp:767-778`). **[V]**
- **`export_obj` n'émet ni normales ni coordonnées de texture par coin** : trois blocs sont
  neutralisés par une condition constante fausse en tête de test (`:600`, `:641`, `:665`). Un OBJ
  exporté n'a donc **jamais** de `vn`. **[V]** Conséquence directe : l'aller-retour OBJ perd les
  normales franches des arêtes vives — ce que `viewer.js:214-218` documente déjà côté navigateur.
  **C'est un argument décisif contre tout chemin d'export qui transiterait par l'OBJ (§4.4).**
- **`MaterialPtr` clone à la copie** (`material.h:25-44`, `:65-66`) : un `Material` traversé par
  valeur est cloné polymorphiquement, l'`Img` d'une texture étant **partagée** et non dupliquée
  (`:272`). Aucune surprise de propriété à craindre côté écriture. **[V]**
- **La liste des sources WASM de `cgmesh` est EXPLICITE** (`src/cgmesh/CMakeLists.txt:7-106`), le
  natif est un `file(GLOB)` (`:108`). **Tout nouveau `.cpp` de `cgmesh` doit être ajouté à la main à
  la liste `EMSCRIPTEN`**, sans quoi il compile en natif et manque au lien WASM — piège déjà payé
  plusieurs fois d'après les commentaires `:38-42` et `:90-94`. **[V]**
- **`TINYGLTF_IMPLEMENTATION` est déjà défini dans `vmeshes_io.cpp:884`**, unité **exclue** du build
  WASM. Un second `#define` dans un nouveau fichier ferait **doubler les symboles au lien natif**. Il
  faut donc soit une unité d'implémentation dédiée partagée, soit un garde. **[V]**
- **Le pont WASM ne peut pas rendre d'octets en `std::string`** : embind la convertirait en chaîne JS
  avec décodage UTF-8, destructeur pour du binaire. D'où le `typed_memory_view` sur un tampon
  statique (`wasm_api.cpp:402-410`), valide **jusqu'au prochain appel**. **[V]**
- **Les tests sont glob-és** (`test/CMakeLists.txt:11`) : un nouveau `tu_*.cpp` ne demande **aucune**
  édition CMake. **[V]**
- **`maker` ne compile que sous Emscripten** (`maker/CMakeLists.txt:20-27`, `FATAL_ERROR`). Toute
  vérification par `ctest` doit donc porter sur `cgmesh`, jamais sur `maker`. **[V]**

### 2.4 Charge réelle du chemin d'export — un défaut de conception à relever au passage

L'opération la plus fréquente n'est pas l'export : c'est l'**évaluation**, rejouée à chaque mouvement
de curseur. Or `template.js:397` exécute `lastObj = Module.graphExportObj(0)` **à chaque évaluation**,
donc **sérialise le maillage entier en une chaîne OBJ à chaque pas de curseur**, alors que
`viewer.bootstrap(lastObj)` ne la consomme qu'**une seule fois** (`:416`) et que le bouton d'export
la relit plus tard (`:739`). **[V]**

Ordre de grandeur sur le Tigre (70 756 faces conservées, environ 37 000 sommets, K0 et §29 du
document SVG **[M]**) : `meshToObj` produit **4 à 8 Mo de `std::string`**, construite puis jetée, à
chaque pas de curseur **[E, méthode : environ 24 octets par ligne `v`, 30 octets par ligne `f`]**.
C'est une allocation de plusieurs mégaoctets sur le chemin interactif, pour un résultat consommé une
fois sur cent.

> **Conséquence de conception** : la sérialisation OBJ doit sortir du chemin d'évaluation et
> descendre **sous le bouton**. Ce n'est pas une optimisation opportuniste : c'est la règle
> « valider une structure contre une CHARGE » appliquée au chemin réel.

---

## 3. Ce que l'écrivain OBJ sait faire aujourd'hui — inventaire exact

Réponse à la question posée (écrit-il un `.mtl` ? avec quelles propriétés ? gère-t-il les textures en
écriture ?), par lecture intégrale de `mesh_io_obj.cpp`.

**Oui, il écrit un `.mtl`**, à côté du `.obj` et avec le même radical, l'extension étant remplacée
sur ses **trois derniers caractères** (`:578-579` — donc `model.obj` donne `model.mtl`, mais un nom
sans extension de trois lettres produit un résultat surprenant, cas non gardé **[V]**).

| Branche | Propriétés écrites | Ligne |
|---|---|---|
| `MATERIAL_COLOR` | `newmtl`, `Ka 0.2 0.2 0.2`, `Kd` (couleur), `Ks 0`, `Ns 0`, **`d <alpha>`**, `illum 1` | `:707-731` |
| `MATERIAL_TEXTURE` | `newmtl`, `Ka 1`, `Kd 1` (blanc obligatoire sous `illum 2`), `Ks 0`, `Ns 0`, `d 1`, `illum 2`, **`map_Kd <nom>`** | `:732-751` |
| `MATERIAL_COLOR_ADV` | **RIEN** — `default: break;` | `:752-753` |

**Textures en écriture : partiellement.** La *ligne* `map_Kd` est écrite ; l'*image* ne l'est jamais.
Ni par `export_obj` (qui n'écrit qu'un `.obj` et un `.mtl`), ni par `export_obj_zip_bytes`
(`:902-916`).

**Ce que l'écrivain n'écrit pas du tout** : `vn` (`:600`, `:665`), UV par coin (`:641`), `Tf`, `Ni`,
`map_Ka`, `map_Ks`, `map_Ns`, `map_d`, `map_Bump`, `refl` — alors que l'**importeur** lit tous ces
derniers (`:125`, `:144`, `:193-220`). L'asymétrie import/export est donc large, et l'aller-retour
est lossy par construction.

---

## 4. Les huit questions, tranchées

### 4.1 Q1 — Le doublon d'écrivain OBJ : peut-on supprimer `meshToObj` ?

**Oui, et le mécanisme est déjà en production.** `export_obj_zip_bytes` fait exactement ce que la
question propose : il laisse `export_obj` écrire ses deux fichiers dans un répertoire temporaire
privé, les relit octet par octet (`slurpFile`, `:783-795`) et rend des octets (`:878-917`). Sous
Emscripten ce répertoire est en MEMFS — « le détour ne touche aucun disque » (`:777-778`). **[V]**

**Obstacles réels, et ce qu'ils valent :**

| Obstacle | Réalité | Verdict |
|---|---|---|
| API fichier contre chaîne | résolu depuis longtemps par le détour MEMFS | **non bloquant [V]** |
| Chemins et sécurité | `basename` est réduit à son dernier composant (`:888-890`) et les ponts WASM filtrent sur alnum, `-` et `_` (`wasm_api.cpp:498-501`) | **non bloquant [V]** |
| Dépendances | `mesh_io_obj.cpp` ne dépend que de `Mesh`, `ZipManager` et `io_path_guard` — tout est dans la liste WASM (`CMakeLists.txt:37`, `:52`) | **non bloquant [V]** |
| Le consommateur de `meshToObj` veut une **chaîne .obj nue**, pas un ZIP | `regenerate()` et `graphExportObj()` alimentent `viewer.bootstrap()`, qui construit un `File` (`viewer.js:235`) | **réel, et c'est le seul** |
| L'OBJ complet porte `mtllib`, que le minimal omet | **la contrainte invoquée n'existe pas** — cf. §4.3 | **levé [V]** |
| Coût | `export_obj` fait 3 `fprintf` par coin de face (`debt_cgmesh.md:187`, E2 en `:4437`) contre des concaténations sur `std::string` | **réel mais renversé** : cf. §2.4, l'appel doit de toute façon sortir du chemin interactif |

**Recommandation.** Ajouter **`MeshIO::export_obj_bytes(const Mesh&, const std::string& basename,
bool emitObjectGroups = false)`** rendant un `std::string` : même montage que `export_obj_zip_bytes`,
mais avec le seul `.obj`. Puis remplacer les deux appels à `meshToObj` et **supprimer la fonction**.
Un écrivain, un comportement, une suite de tests.

> **Deux écrivains d'un même format sont une dette en soi**, et celle-ci a déjà coûté : le
> commentaire de `wasm_api.cpp:19-26` doit expliquer pourquoi il y en a deux, celui de
> `template.js:19-24` doit répéter la limite qui en découle, et le K5 du document SVG doit encore la
> re-déclarer. Trois commentaires pour tenir une divergence, c'est le prix de la duplication.

### 4.2 Q2 — Livrer deux fichiers au navigateur

**Question déjà tranchée dans le dépôt : ZIP, avec l'écrivain maison.** `ZipManager` produit des
entrées *stored* (non compressées), **sans zlib** — choix explicitement motivé par le fait que zlib
est désactivé dans la cible WebAssembly (`zip_manager.h:14-18`). **[V]** Rien à vendoriser.

| Piste | Pour | Contre | Verdict |
|---|---|---|---|
| **ZIP `ZipManager`** (existant) | un seul téléchargement ; `mtllib foo.mtl` résout tel quel après extraction (`mesh_io.h:47-51`) ; sortie déterministe donc testable ; extensible au 3MF | taille (stored) ; l'utilisateur doit dézipper | **RETENU** |
| Deux téléchargements successifs | pas de nouveau code | les navigateurs bloquent ou avertissent au second `a.click()` ; l'utilisateur peut n'en garder qu'un, et un `.obj` sans son `.mtl` est **muet** (pas d'erreur, juste du gris) | rejeté |
| `showSaveFilePicker` (File System Access) | vrai dossier de destination | API non universelle, exige un geste utilisateur par fichier | rejeté |
| Un seul `.obj` avec matériaux inline | un fichier | **n'existe pas** dans le format | impossible |
| Compression *deflate* | archives plus petites | zlib OFF en WASM, préfixage `Z_PREFIX` déjà piégeux (`zip_manager.h:14-18`) | hors périmètre |

La page est servie en statique (`maker/serve.py`, `SimpleHTTPRequestHandler` avec `no-store`) et le
téléchargement passe par `saveBlob` (`exporters.js:28-34`) : **aucune contrainte serveur**, le ZIP est
construit dans le tas WASM et copié dans un `Blob`. **[V]**

### 4.3 Q3 — La contrainte o3dv est-elle réelle ? **Non.** Vérifié dans le bundle.

Le bundle **actif** est `maker/web/vendor/o3dv-three.module.js` (1 202 224 octets) ; `o3dv.min.js` est
l'ancien UMD, plus chargé par aucune page (`vendor/README.md`). **[V]**

**(a) Un `mtllib` sans `.mtl` n'est PAS une erreur.** Le lecteur OBJ traite la directive ainsi
(bundle, texte dé-minifié) :

```js
else if (e === "mtllib") {
  if (t.length === 0) return true;
  let o = ul(n, e.length, "#"), a = this.callbacks.getFileBuffer(o);
  if (a !== null) { let l = xr(a); Yo(l, c => { this.WasError() || this.ProcessLine(c) }) }
  return true;                                   // succès dans TOUS les cas
}
```

Et le tampon manquant est simplement **recensé** :

```js
let s = new Xp(o => { let a = null, l = this.fileList.FindFileByPath(o);
  return (l === null || l.content === null)
    ? (this.missingFiles.push(o), a = null)
    : (this.usedFiles.push(o), a = l.content), a });
```

`missingFiles` part dans le **résultat de succès** (`onImportSuccess`), pas dans un chemin d'erreur.
**[V]** Donc le commentaire de `wasm_api.cpp:745-750` et celui de `template.js:19-24` **surestiment la
contrainte** : un OBJ portant `mtllib` se charge, le modèle s'affiche, seul le nom du fichier absent
est listé.

**(b) Une `FileList` à deux entrées fonctionne.** `LoadModelFromFileList(e)` appelle `ST(e)` puis
`FillFromInputFiles`, qui construit la liste **depuis le tableau passé** ; la résolution se fait sur
le **nom de base, insensible à la casse** :

```js
FindFileByPath(e) { let t = Ji(e).toLowerCase();
  for (let n = 0; n < this.files.length; n++) { let i = this.files[n];
    if (i.name.toLowerCase() === t) return i } return null }
// Ji(r) : retire la query string puis tout ce qui précède le dernier séparateur
```

**[V]** Donc `viewer.bootstrap` peut passer deux `File` — `model.obj` et `model.mtl` — et o3dv
**résoudra les matériaux**.

> **La contrainte tombe des deux côtés.** Ni l'aperçu ni le téléchargement n'ont de raison d'omettre
> les matériaux. Trois commentaires du dépôt affirment le contraire et sont à corriger :
> `maker/wasm_api.cpp:19-26` et `:745-750`, `maker/web/js/template.js:19-24`.

**Réserve R1, à lever avant de s'y appuyer.** Ceci est **[V] par lecture d'un bundle minifié**, pas
**[M] à l'exécution**. Le sens du biais est **optimiste** : je lis un chemin, pas tous. Instrument de
levée, à écrire en jalon G0 : une page jetable qui charge les deux fichiers, lit
`result.missingFiles.length` et compte les matériaux du modèle o3dv.

### 4.4 Q4 — Où vit l'écrivain GLB ? **En C++ dans `cgmesh`.**

Le dépôt offre **deux** précédents, et la commande demande lequel s'applique. Réponse : **celui de
l'OBJ**, et le précédent STL ne dit pas ce qu'on croit — il dit le contraire.

> « Le STL reste assemblé ici, **faute d'un point d'entrée WASM** ; c'est un candidat au même
> traitement que l'OBJ (`MeshIO::export_stl_binary` **existe déjà**). »
> — `maker/web/js/exporters.js:9-10` **[V]**

Le STL est en JS par **défaut de pont**, pas par choix d'architecture, et son propre commentaire le
déclare candidat au rapatriement. Invoquer le STL comme précédent pour écrire le GLB en JS, c'est
citer un aveu de dette comme une décision.

| Piste | Pour | Contre | Verdict |
|---|---|---|---|
| **A — C++ dans `cgmesh`, tinygltf** | tinygltf **déjà vendorisé et déjà compilé** dans le dépôt ; les matériaux vivent en C++ ; testable par `ctest` sans navigateur ; réutilisable par `sinaia`, `vecna`, `sulina` ; accès direct à `BuildPolygonRenderData` (normales franches, UV, plages) | quelques dizaines à centaines de Ko sur `maker.wasm` **[E, R2]** ; `TINYGLTF_IMPLEMENTATION` déjà pris (§2.3) ; API tinygltf verbeuse | **RETENU** |
| B — C++ dans `cgmesh`, écrivain **maison** sur nlohmann | nlohmann est **déjà** dans le build WASM (`CMakeLists.txt:104`) donc coût binaire quasi nul ; contrôle total sur alignement et accessors ; pas de conflit d'implémentation | il faut écrire le conteneur GLB (12 octets d'en-tête, deux chunks, padding) — environ 60 lignes — et le faire juste | **repli sérieux**, à retenir si R2 casse |
| C — JS, à la main dans `exporters.js` | zéro C++ | **non testé par `ctest`** ; propre à `maker` ; il faudrait **d'abord** faire traverser les matériaux complets (alpha, textures) à la charge utile — donc du C++ quand même | rejeté |
| D — JS, via **`OV.ExporterGltf`** du bundle vendorisé | **existe** : `ExporterGltf` est exporté par le bundle et sait produire du `.glb` (`CanExport(Binary, "glb")`) **[V]** ; zéro ligne d'écrivain | **rédhibitoire** : il exporte le *modèle o3dv*, c'est-à-dire ce qu'o3dv a **importé de l'OBJ d'amorçage** — or `updateInPlace` remplace la géométrie **dans three.js seulement** (`viewer.js:311-312`), jamais dans le modèle o3dv. On exporterait donc un modèle **périmé**. Le rafraîchir imposerait un rechargement OBJ complet à chaque export, avec **soudure des sommets et perte des normales franches** (`viewer.js:214-218`) | rejeté, mais **à connaître** |
| E — `three.js GLTFExporter` | standard | **absent du bundle** (`GLTFExporter` : 0 occurrence **[M, grep]**) ; il faudrait re-générer le bundle et alourdir 1,2 Mo de vendor | rejeté |

**Argument décisif** : un GLB dont les matériaux sont le résultat doit lire `Mesh::GetMaterial` et
`MaterialColor::GetFloatAlpha`. Ces objets sont en C++ ; les faire traverser vers JS pour les
ré-assembler en JS, c'est ajouter un format de transport intermédiaire à maintenir, pour finir par
écrire le même JSON de l'autre côté du pont.

### 4.5 Q5 — Structure du GLB

**Un `primitive` par `MaterialRange`.** C'est le mapping naturel, et il ne coûte rien : les plages
sont déjà calculées, contiguës, triées et de longueur multiple de 3 (`mesh.cpp:918-926`). **[V]**

```
buffer 0 (chunk BIN, sans URI)
  bufferView 0  POSITION     nv x 12 o    target 34962 ARRAY_BUFFER
  bufferView 1  NORMAL       nv x 12 o    target 34962
  bufferView 2  TEXCOORD_0   nv x  8 o    target 34962   (si rd.texCoords non vide)
  bufferView 3  COLOR_0      nv x 12 o    target 34962   (si "peint", cf. regle ci-dessous)
  bufferView 4  INDICES      ni x 2 ou 4  target 34963 ELEMENT_ARRAY_BUFFER

accessors : 0..3 partages par TOUTES les primitives ; un accessor d'indices PAR PLAGE,
            byteOffset = range.offset * sizeof(index), count = range.count

mesh 0 : primitives[k] = { attributes: {POSITION:0, NORMAL:1, ...},
                           indices: 4+k, material: k, mode: 4 (TRIANGLES) }
node 0 : { mesh: 0, TRS d'unite/orientation }      scene 0 : { nodes: [0] }
```

**Disposition : par attribut (non entrelacée).** Justification par la charge, pas par le goût :
`BuildPolygonRenderData` **rend déjà** quatre `std::vector<float>` contigus et séparés
(`mesh.h:151-160`). Une disposition entrelacée imposerait une passe de recopie en O(nv x attributs)
**et** un `byteStride` par vue ; elle ne se justifie que pour un chemin GPU chaud, ce qu'un fichier
écrit une fois n'est pas. **[V]**

| Point | Choix | Raison |
|---|---|---|
| Type d'index | `UNSIGNED_SHORT` si nv <= 65535, sinon `UNSIGNED_INT` | miroir exact de la règle déjà appliquée côté viewer (`viewer.js:263`) **[V]** ; sur le Tigre nv vaut environ 37 000, donc `USHORT` suffit, moitié moins d'octets **[M via K0]** |
| `min` et `max` sur POSITION | **obligatoires** | exigés par la spec pour l'accessor de POSITION ; leur absence fait échouer le validateur officiel |
| `NORMAL` | toujours, depuis `rd.normals` | c'est précisément ce que l'aller-retour OBJ perd (`mesh_io_obj.cpp:600`) — le GLB devient donc **strictement meilleur que l'OBJ** sur ce point |
| `TEXCOORD_0` | si `rd.texCoords` non vide | |
| `COLOR_0` | **seulement si peint** | `Mesh::InitVertices` remplit `m_vertexColors` de gris 0,5 pour **tout** maillage (`mesh_payload.cpp:89-100` **[V]**). Émettre `COLOR_0` inconditionnellement **multiplierait** `baseColorFactor` par 0,5 chez tout lecteur PBR : **toutes les couleurs seraient assombries de moitié**. Reprendre le test exact de `mesh_payload.cpp:97-100` |
| Matériau | `pbrMetallicRoughness.baseColorFactor = [r, g, b, a]` | depuis `MaterialColor::GetFloatRed/Green/Blue/Alpha` (`material.h:103-106`) |
| **Espace colorimétrique** | **`baseColorFactor` est LINÉAIRE, `MaterialColor` est en sRGB** | conversion obligatoire (seuil 0,04045 puis exposant 2,4). Le pendant existe déjà côté viewer (`tex.colorSpace = SRGBColorSpace`, `viewer.js:97`, avec le commentaire « sans quoi l'image ressort délavée ») **[V]**. **Sans cette conversion, le Tigre sortira délavé dans Blender** — et le défaut est silencieux |
| `metallicFactor` | **0.0** | le défaut glTF est **1.0**. Un matériau métallique sans carte d'environnement rend **noir** : c'est le mode d'échec classique du GLB « tout noir dans le visualiseur » |
| `roughnessFactor` | **0.9** | le défaut est 1.0 (parfaitement mat). 0,9 donne un plastique imprimé crédible sans spéculaire dur. Valeur de confort, assumée comme telle |
| `doubleSided` | **`true`** | `ExtrudeAppendOptions::normalizeOrientation` vaut **`false` par défaut** (`extrude_contours.h:59`) **[V]** : l'orientation des capots n'est pas garantie. En simple face, un capot inversé devient **invisible** chez tout lecteur qui élimine les faces arrière. Coût : un rendu deux fois plus lourd, négligeable pour un fichier livré |
| `alphaMode` | `OPAQUE`, et `BLEND` **seulement si** au moins un matériau a un alpha inférieur à 1 | ne pas armer la transparence sans raison : elle impose un tri par profondeur et dégrade le rendu |
| `MATERIAL_NONE` | un matériau gris par défaut, **explicite** | la plage `MATERIAL_NONE` existe et trie **en dernier** (valeur `(unsigned)-1`) ; omettre `material` renverrait au matériau par défaut du lecteur, qui est **métallique** (cf. ci-dessus) |
| Déduplication | **aucune** | les 39 matériaux du Tigre sont déjà dédupliqués par RGBA en amont (§5.2 du document SVG) ; refaire le travail ici masquerait une régression amont |

### 4.6 Q6 — Unités et orientation

**Ce que le dépôt produit, vérifié :** le dessin est dans le plan **XY**, l'extrusion se fait selon
**Z** (`extrude_contours.h:43-44`, `zBottom` et `zTop`) **[V]**, et les unités monde **sont des
millimètres** — le gabarit expose « Taille (mm) » (`svg.json:108-109`) et « Profondeur (mm) »
(`:120`), et `template.js:423` le redit (« Les unités monde du document SONT des millimètres »).
**[V]** Une pièce plate posée dans XY avec son épaisseur en Z, c'est un monde **Z-up**.

**Ce que glTF attend :** **Y-up**, et le mètre comme unité de fait.

| Piste | Effet dans Blender 4.x (import par défaut) | Trade-off | Verdict |
|---|---|---|---|
| **Transformation de NŒUD** (échelle 0,001 et rotation de -90 degrés autour de X) | Blender reconvertit Y-up vers Z-up à l'import : la pièce **retombe exactement comme elle a été dessinée**, et mesure `fitSize` **millimètres** | les données de sommets restent **identiques** à celles de l'OBJ, donc un écart entre les deux exports reste diagnosticable ; certains lecteurs très naïfs ignorent le graphe de scène | **RETENU** |
| Cuire dans les sommets | marche partout, même chez un lecteur sans graphe de scène | les coordonnées **divergent** de l'OBJ ; les `min` et `max` d'accessor ne se comparent plus à rien ; toute mesure croisée devient piégeuse | repli |
| Ne rien faire, documenter | zéro code | une pièce de 100 mm arrive comme un objet de **100 mètres**. « Documenter » ne corrige pas un facteur 1000 : c'est un critère d'acceptation non falsifiable déguisé | rejeté |

**Conséquence à faire trancher.** Mettre à l'échelle crée une **divergence assumée entre nos deux
exports** : le `.obj` restera en millimètres bruts (il n'a pas de convention d'unité), le `.glb` sera
en mètres. C'est cohérent avec chaque format pris isolément, et incohérent entre les deux. **Décision
utilisateur** (§8, D-2).

**Note de périmètre** : les *slicers* (PrusaSlicer, Cura, Bambu Studio) ne lisent **pas** le GLB. Les
lecteurs tiers qui comptent ici sont Blender, un visualiseur web three.js, Windows 3D Viewer,
Sketchfab et le validateur glTF officiel — **tous honorent le graphe de scène**. Pour l'impression
avec couleurs, le format est le **3MF**, pas le GLB (§8, D-3).

### 4.7 Q7 — Alpha et textures

**Alpha : oui, et le travail est presque nul.**

- **OBJ** : rien à faire, l'alpha traverse **déjà** (`d %f` depuis `GetFloatAlpha()`,
  `mesh_io_obj.cpp:727`, avec un commentaire qui explique pourquoi ce n'est pas `Tr`). **[V]**
- **GLB** : `baseColorFactor[3]`, gratuit.
- **Charge utile JS** : `DescribeMaterial` ne transmet que `r`, `g` et `b`
  (`mesh_payload.cpp:60-66`). L'ajouter est une ligne — mais c'est un changement d'**affichage**, pas
  d'export : il faudrait armer `material.transparent` côté three, ce qui change l'ordre de rendu et
  peut **dégrader** le résultat visible. **À isoler dans un jalon distinct, hors de ce chantier.**

**Textures : à échelonner, et pour une raison matérielle.**

Le GLB *sait* embarquer les images (`images[].bufferView` plus `mimeType`). Le blocage est en amont :

1. `MaterialTexture` ne conserve **pas** les octets d'origine — seulement l'`Img` décodée et un nom
   de fichier (`material.h:266-274`). **[V]** Il faut donc **ré-encoder**.
2. **Aucun encodeur PNG ni JPEG** — mais cgimg en a trois autres. `ImgIO::save` écrit `.bmp`, `.tga`
   et `.pnm` ; `export_png` est un `return -1` explicite et il n'existe pas d'`export_jpg`
   (`src/cgimg/image_io.cpp:58-76`, `image_io_png.cpp:52-59`). **[V]** Les options `CGIMG_WITH_PNG`
   et `CGIMG_WITH_JPG` ne pilotent que le **décodage** (`src/cgimg/CMakeLists.txt:12-36`). **[V]**
   Les formats qu'un `map_Kd` nomme en pratique sont justement les deux manquants.
3. Coût **[E]** : vendoriser `stb_image_write.h` (un fichier, environ 2 000 lignes) plus un point
   d'entrée « encoder RGBA en PNG dans un vecteur d'octets ». Poids WASM **[E] : 20 à 40 Ko**.
   Taille du GLB : une texture de relief d'image fait couramment 512x512 à 2048x2048, soit **0,3 à
   5 Mo** de PNG.

**Verdict** : hors du périmètre de ce chantier, **jalon G6 optionnel**. Et il vaudra pour l'OBJ aussi,
puisque le `map_Kd` de l'archive ZIP pend actuellement dans le vide (§1.3). Le maillage du Tigre ne
porte **que** des `MaterialColor` (39 matériaux de genre `color`, §29.2 du document SVG **[M]**) : la
texture ne concerne ni le Tigre ni le chantier qui l'a motivé.

### 4.8 Q8 — Tester un GLB dans `ctest` sans lecteur tiers

Quatre familles d'oracles, du plus mécanique au plus sémantique. Toutes tournent dans `TU`, sans
navigateur et sans Blender.

**1. Validité du conteneur** — le pendant exact du test ZIP existant (`tu_cgmesh_io.cpp:1177-1258`,
qui relit l'archive « comme le ferait un vrai lecteur ZIP »).

- octets 0 à 3 égaux à `glTF` ; 4 à 7 égaux à 2 ; 8 à 11 : **longueur totale égale à la taille du
  fichier** ;
- chunk 0 : type `0x4E4F534A` (JSON), longueur **multiple de 4** ;
- chunk 1 : type `0x004E4942` (BIN), longueur multiple de 4, **début aligné sur 4** ;
- 12 plus la somme des (8 plus longueur de chunk) égale la longueur totale.

**2. Cohérence du JSON** (nlohmann est déjà disponible) :

- `meshes[0].primitives.size()` égal à `rd.materialRanges.size()` — **le critère central** ;
- `materials.size()` égal à `primitives.size()` ;
- pour chaque primitive : `indices` valide, `accessors[indices].count` égal à `range.count`,
  `byteOffset` égal à `range.offset * sizeof(index)` ;
- `accessors[POSITION].count * 12` égal à `bufferViews[0].byteLength` ;
- `accessors[POSITION].min` et `.max` présents et **encadrant** la boîte englobante du maillage ;
- `metallicFactor` égal à 0, `doubleSided` vrai, `baseColorFactor` égal à la linéarisation de la
  couleur attendue **à 1e-3 près**.

**3. Relecture par un lecteur tiers — qui est déjà dans le dépôt.** `tinygltf::TinyGLTF` sait lire du
GLB (`vmeshes_io.cpp:1013`, `LoadBinaryFromFile`). Écrire avec, relire avec `LoadBinaryFromMemory`,
puis comparer sommets, indices et couleurs. **Limite honnête** : écrire et relire avec la *même*
bibliothèque ne détecte pas une erreur d'interprétation *commune aux deux* — c'est un oracle **de
cohérence**, pas de conformité. Il ne remplace pas le validateur officiel (§8, non-exécuté).

**4. Aller-retour OBJ+MTL — l'oracle que `mesh_io_obj.cpp` permet déjà, puisqu'il lit.**
`export_obj` puis `import_obj` (qui lit `mtllib`, `newmtl`, `Kd` et `d`), puis comparaison du nombre
de matériaux, des noms et des `Kd` à 1/255 près.

> **Ce test ÉCHOUE aujourd'hui**, et c'est ce qui en fait un bon critère : `import_mtl` produit des
> `MaterialColorExt` (`:114`) que `export_obj` ne réécrit pas (`:752-753`). Un second aller-retour
> perd **tous** les matériaux. **[V]**
> Le dépôt a déjà l'idiome pour ce cas : un test qui **fige le comportement actuel** et qui
> **échouera** le jour où il change, forçant une mise à jour consciente
> (`test/tu_cgmesh_vmeshes_io.cpp:315-328`). C'est le patron à reprendre en G0, puis à retourner
> en G1.

---

## 5. Recommandation

### 5.1 Architecture cible

```
                    +-------------------------------------------+
                    |  cgmesh — la sérialisation vit ICI        |
                    +-------------------------------------------+
 Mesh ------------- | mesh_io_obj.cpp   export_obj              | --> .obj
   |                |                   export_obj_bytes  (NEW) |
   |                |                   export_obj_zip_bytes    | --> .zip {obj,mtl}
   |                +-------------------------------------------+
   +-- BuildPolygon | mesh_io_gltf.cpp  export_glb        (NEW) | --> .glb
       RenderData   |                   export_glb_bytes  (NEW) |
       (materialRanges)  +--------------------------------------+
                                       |
                    +------------------+-------------------------+
                    |  maker/wasm_api.cpp — le PONT              |
                    |  exportObj(id)           (existe)          |
                    |  exportGlb(id)           (NEW)             |
                    |  graphExportObjZip(port) (NEW)             |
                    |  graphExportGlb(port)    (NEW)             |
                    |  graphExportObj(port) -> export_obj_bytes  |
                    |  meshToObj               SUPPRIME          |
                    +------------------+-------------------------+
                    +------------------+-------------------------+
                    |  exporters.js — nommer et télécharger      |
                    |  downloadObj / downloadGlb                 |
                    |  downloadObjFromGraph / downloadGlbFromGraph|
                    +--------------------------------------------+
```

### 5.2 Décisions

| # | Décision | Alternative écartée |
|---|---|---|
| **A** | **Un seul écrivain OBJ.** `meshToObj` supprimé, remplacé par `MeshIO::export_obj_bytes`. | garder les deux et documenter — c'est déjà ce qui se fait, au prix de trois commentaires et d'une limite re-déclarée à chaque chantier |
| **B** | **ZIP `ZipManager`** pour OBJ+MTL, sur **tous** les chemins. | deux téléchargements ; deflate |
| **C** | **Corriger l'écrivain MTL avant d'ajouter un format.** `MATERIAL_COLOR_ADV` doit produire un `newmtl`. | ajouter le GLB par-dessus un OBJ qui perd déjà les matériaux à l'aller-retour |
| **D** | **Écrivain GLB en C++ dans `cgmesh`**, un `primitive` par `MaterialRange`, tinygltf (repli : nlohmann à la main). | JS ; `OV.ExporterGltf` (modèle périmé) ; `three.GLTFExporter` (absent) |
| **E** | **Unité et orientation par transformation de nœud** (0,001 et -90 degrés autour de X). | cuire dans les sommets ; ne rien faire |
| **F** | **Alpha oui, textures non** (échelonnées en G6). | tout faire d'un coup, et découvrir l'absence d'encodeur PNG à mi-parcours |
| **G** | **Sortir la sérialisation OBJ du chemin d'évaluation** (`template.js:397`). | laisser 4 à 8 Mo de chaîne se construire à chaque pas de curseur |
| **H** | **Corriger les trois commentaires** qui affirment la contrainte o3dv. | les laisser : le prochain chantier les relira et refera le même choix |

---

## 6. Plan incrémental — jalons falsifiables

### État d'exécution

| Jalon | État | Ce qui l'atteste |
|---|---|---|
| **G0** | **fait** | `TEST_cgmesh_io.obj_mtl_survives_a_round_trip` (vu **rouge** avant G1) ; sondes `maker/probes/o3dv_mtl.js` et `graph_obj_cost.js` |
| **G1** | **fait** | branche `MATERIAL_COLOR_ADV` (`mesh_io_obj.cpp`) ; `map_Kd` conditionné à la présence de l'image (§1.3-(2)) ; le test de G0 passe au vert |
| **G2** | **fait** | `MeshIO::export_obj_bytes` ; `meshToObj` supprimé ; trois commentaires corrigés ; sérialisation sortie du chemin d'évaluation |
| **G3** | **fait** | `graphExportObjZip` + `downloadObjFromGraph` ; `test/tu_cgmesh_io_obj_materials.cpp` ; sonde `maker/probes/svg_page_export.js` |
| **G4** | **fait** | `src/cgmesh/mesh_io_gltf.cpp` ; `src/cgmesh/tinygltf_impl.cpp` (R3) ; `test/tu_cgmesh_io_gltf.cpp`, 10 tests. **Les cinq détecteurs ont été éprouvés avant d'être crus** : quatre vus ROUGES sous mutation de l'écrivain (sRGB neutralisée, indice de matériau hors bornes, primitives fondues en une seule, `min`/`max` de POSITION omis), le cinquième — le lecteur de conteneur — validé sur quatre GLB corrompus qu'il doit refuser, contrôle qui **reste dans la suite** |
| **G5** | **fait** | `exportGlb` / `graphExportGlb` ; `downloadGlb` / `downloadGlbFromGraph` ; bouton GLB sur les **deux** familles de page ; sondes `maker/probes/glb_page_export.js` et `shapes_glb_export.js` |
| **G6** | non commencé | bloqué en amont : cgimg n'encode ni PNG ni JPEG (§4.7) |

Deux écarts au plan, assumés :

- **la contrainte o3dv est tombée**, donc l'OBJ d'amorçage porte `mtllib` sur **les deux** familles de
  page et non sur la seule page gabarit. Le fichier absent est recensé par o3dv, pas refusé **[M]** ;
- **G2-3 se lit à l'envers** : le critère annonçait `missingFiles.length === 1` pour l'amorçage, ce
  qui est vrai, mais l'oracle retenu est le maillage affiché et les statistiques renseignées — un
  compte de fichiers manquants ne dit pas qu'un modèle est à l'écran.


Chaque jalon énonce **ce qui doit être vrai à la fin**, sous une forme qu'une vérification peut
**infirmer**, et **ce qu'il exclut nommément**.

### G0 — Mesurer et lever les réserves, **aucune fonctionnalité**

- **Construit** : (a) un test figeant le comportement actuel de l'aller-retour OBJ+MTL (patron
  `tu_cgmesh_vmeshes_io.cpp:315-328`) ; (b) une page jetable `maker/web/o3dv_probe.html` chargeant
  une liste de deux fichiers `[obj, mtl]` ; (c) trois mesures de référence.
- **Critères, falsifiables** :
  1. le test d'aller-retour **passe** en affirmant que le `.mtl` réexporté contient **0** `newmtl`
     alors que le maillage porte N matériaux, N supérieur à 0 ;
     **[si le test échoue, le §1.3-(1) est faux]**
  2. la sonde o3dv affiche `missingFiles.length === 0` **et** un nombre de matériaux supérieur ou
     égal à 2 sur un couple OBJ/MTL bi-matériau ; **[infirme ou confirme R1]**
  3. trois chiffres consignés : taille de `maker.wasm` (référence **[M]** : **2 194 911 octets**,
     `ls -la maker/web/maker.wasm`, 2026-09-06), médiane de `graphExportObj(0)` sur le Tigre
     (`performance.now()` autour de `template.js:397`, 5 tirages) et taille de `lastObj`.
- **Exclut nommément** : toute modification de `cgmesh` et de l'interface.

### G1 — Combler les deux trous de l'écrivain MTL (C++ pur, natif et WASM)

- **Construit** : branche `MATERIAL_COLOR_ADV` dans `mesh_io_obj.cpp:752` (`Kd` depuis
  `m_fDiffuse`, `Ka` depuis `m_fAmbient`, `Ks` depuis `m_fSpecular`, `Ns` égal à `m_fShininess`
  multiplié par 128, `d` depuis `m_fDiffuse[3]`, `illum 2`) ; et le retournement du test de G0.
- **Critères** :
  1. importer un OBJ+MTL du dépôt, réexporter, réimporter : `GetNMaterials()` identique, chaque `Kd`
     égal à **1/255 près**, chaque nom conservé ;
  2. `ctest` intégralement vert (référence **1717/1717**) ;
  3. un maillage **sans** matériau produit toujours une archive à **une seule** entrée — le test
     existant `obj_zip_omits_the_mtl_when_there_is_no_material` (`tu_cgmesh_io.cpp:1260-1280`) ne
     doit pas casser.
- **Exclut nommément** : les textures (le `map_Kd` reste une référence pendante, cf. G6), `vn`, les
  UV par coin, `refl` et les autres `map_*`.

### G2 — Un seul écrivain OBJ

- **Construit** : `MeshIO::export_obj_bytes` (`mesh_io.h`, `mesh_io_obj.cpp`) ; `regenerate()` et
  `graphExportObj()` y basculent ; `meshToObj` **supprimé** ; les trois commentaires sur o3dv
  corrigés (décision H) ; `template.js:397` déplacé sous le bouton (décision G).
- **Critères** :
  1. `grep -c "meshToObj" maker/wasm_api.cpp` rend **0** ;
  2. `shapes.html` et `svg.html` affichent leur modèle au chargement (capture) ;
  3. l'OBJ d'amorçage contient désormais `mtllib`, et o3dv le charge **sans erreur**, avec
     `missingFiles.length === 1` ; **[dépend de R1, levée en G0]**
  4. **charge** : la médiane de `graphEvaluate` plus charge utile sur le Tigre ne **régresse pas de
     plus de 10 ms** par rapport au chiffre de G0-3, et **aucune** chaîne OBJ n'est construite tant
     que le bouton n'est pas cliqué (instrument : compteur d'appels sur `graphExportObj`).
- **Exclut nommément** : les pages de FORMES, dont le chemin d'export est déjà correct.

### G3 — L'export OBJ+MTL sur les pages GABARIT — *le manque de la commande*

- **Construit** : `graphExportObjZip(port, base)` dans `wasm_api.cpp` (calqué sur `exportObj:481-513`,
  même tampon `g_zipBytes`) ; `downloadObjFromGraph` dans `exporters.js` ; câblage de
  `downloadObjBtn` (`template.js:738`) ; ajout du binding à `REQUIRED_BINDINGS` (`wasm.js:15-25`).
- **Critères** :
  1. sur `svg.html`, Tigre chargé, bascule « Couleurs du fichier » **cochée** : le fichier téléchargé
     est un `.zip` de **2** entrées ; **[falsifié par 1 entrée, ou par un `.obj` nu]**
  2. son `.mtl` contient **au moins 30** lignes `newmtl` et **au moins 30** valeurs `Kd` distinctes ;
     **[le Tigre en a 39, [M]]**
  3. son `.obj` porte **au moins 30** `usemtl`, et **chaque** nom `usemtl` a un `newmtl`
     correspondant ; **[c'est le défaut que `objMaterialName:44-48` décrit — le vérifier, pas le
     supposer]**
  4. `MeshIO::import_obj` sur le `.obj` extrait rend **au moins 30** matériaux ;
     **[oracle sans navigateur]**
  5. bascule **décochée** : l'archive a **1** entrée ou un seul `newmtl` — le comportement ne change
     pas hors du cas coloré.
- **Exclut nommément** : le GLB ; les textures ; les pages de formes.

> **À ce stade, la demande initiale « exporter les matériaux en OBJ » est satisfaite.** G4 à G6 sont
> un second chantier, arbitrable séparément.

### G4 — Écrivain GLB dans `cgmesh`

- **Construit** : `src/cgmesh/mesh_io_gltf.cpp` plus les déclarations dans `mesh_io.h`
  (`export_glb(const Mesh&, const char*)` et `export_glb_bytes(const Mesh&)` rendant un
  `std::string`) ; ajout à la **liste `EMSCRIPTEN`** de `src/cgmesh/CMakeLists.txt` (§2.3, piège
  connu) ; résolution du conflit `TINYGLTF_IMPLEMENTATION` avec `vmeshes_io.cpp:884` ;
  `test/tu_cgmesh_io_gltf.cpp` (aucune édition CMake, cf. `test/CMakeLists.txt:11`).
- **Critères** (les quatre familles du §4.8) :
  1. conteneur : magic, version 2, longueur égale à la taille, chunks alignés sur 4, somme cohérente ;
  2. `meshes[0].primitives.size()` égal à `BuildPolygonRenderData().materialRanges.size()`, sur
     **trois** maillages : mono-matériau, bi-matériau, et un maillage à faces `MATERIAL_NONE` ;
  3. `baseColorFactor` égal à la linéarisation sRGB de la couleur attendue à **1e-3** ;
     `metallicFactor` égal à 0 ; `doubleSided` vrai ;
  4. relecture par `tinygltf::LoadBinaryFromMemory` : succès, même nombre de sommets et de triangles ;
  5. `COLOR_0` **absent** d'un maillage non peint ;
     **[falsifie l'assombrissement de moitié, §4.5]**
  6. `ctest` vert, et **`maker.wasm` ne dépasse pas +250 Ko** par rapport à G0-3.
     **[c'est R2 ; si le seuil saute, basculer sur la piste B du §4.4]**
- **Exclut nommément** : les textures, les extensions `KHR_*`, les animations, les scènes à plusieurs
  nœuds, et le glTF **texte** (`.gltf` plus `.bin` : deux fichiers, donc le même problème que l'OBJ,
  sans le bénéfice).

### G5 — Bouton GLB dans `maker`, sur les deux chemins

- **Construit** : `exportGlb(id, base)` et `graphExportGlb(port, base)` (`wasm_api.cpp`) ;
  `downloadGlb` et `downloadGlbFromGraph` (`exporters.js`) ; bouton dans le pied de `shell.js:52-60`
  et dans `svg.html` et `template.html` ; bindings dans `REQUIRED_BINDINGS`.
- **Critères** :
  1. le `.glb` téléchargé depuis `svg.html` (Tigre) s'ouvre dans **Blender 4.x, réglages d'import par
     défaut** : **au moins 30** matériaux dans l'Outliner ; **[oracle manuel, hors ctest — assumé]**
  2. ses **dimensions** dans Blender valent `fitSize` millimètres à **0,1 mm près**, et la pièce est
     **posée à plat** (épaisseur selon le Z de Blender) ; **[falsifie la décision E : un facteur 1000
     ou une pièce debout infirme la transformation de nœud]**
  3. le même fichier passe le **validateur glTF officiel** sans erreur ; **[hors ctest]**
  4. le STL et l'OBJ existants restent inchangés octet pour octet sur un cas témoin.
- **Exclut nommément** : le format `.gltf` texte, et les slicers (qui ne lisent pas le GLB).

### G6 — Textures embarquées (optionnel, hors v1)

- **Construit** : vendorisation de `stb_image_write.h` ; encodage PNG en mémoire dans `cgimg` ;
  `images[]` et `bufferView` dans le GLB ; et, symétriquement, l'image **ajoutée à l'archive ZIP**
  avec un `map_Kd` qui la désigne (correction du §1.3-(2)).
- **Critères** : un relief d'image exporté en GLB s'ouvre texturé dans Blender ; l'archive ZIP OBJ
  contient **3** entrées et son `map_Kd` désigne l'entrée présente ; `MeshIO::import_obj` sur
  l'archive extraite retrouve la texture.
- **Exclut nommément** : le Tigre, qui ne porte **que** des `MaterialColor` **[M, §29.2 du document
  SVG]**.

---

## 7. Journal de prédictions

Alimenté à chaque jalon terminé. Colonne **Cause** dans la liste fermée (1 affirmation non vérifiée,
2 instrument biaisé, 3 hypothèse d'environnement, 4 structure sans charge, 5 critère non falsifiable,
6 excès de pessimisme, 7 vérification neutralisée par le code).

| Étape | Prédiction | Instrument | Résultat | Écart | Cause |
|---|---|---|---|---|---|
| G0-1 | l'aller-retour OBJ+MTL perd **100 %** des matériaux | `ctest`, `obj_mtl_survives_a_round_trip` | **exact** : 0 `newmtl` pour 2 matériaux, 0 matériau relu, 2 faces sur 2 à `<aucun>` ; le `.mtl` réexporté est ses trois lignes d'en-tête | aucun | — |
| G0-2 | o3dv charge les deux fichiers avec `missingFiles.length === 0` | `maker/probes/o3dv_mtl.js`, Chrome sans tête | **exact** : paire → `charge`, `missingFiles` vide, 2 matériaux aux couleurs du `.mtl`. Orphelin → `charge`, `missingFiles = [sonde.mtl]`, 1 matériau par défaut | aucun | — |
| G0-3 | `graphExportObj` sur le Tigre coûte **20 à 60 ms** et **4 à 8 Mo** de chaîne **[E]** | `maker/probes/graph_obj_cost.js`, médiane de 5 | **59,1 ms** (dans la fourchette) et **2 937 838 o** (sous la fourchette). `maker.wasm` : 2 194 911 o. Maillage : 74 296 sommets, 70 756 triangles | taille surestimée d'environ 40 % | **6** — l'estimation comptait 24 o par ligne `v` et 30 par ligne `f` ; `%.6g` en produit moins |
| G1 | la branche `MATERIAL_COLOR_ADV` tient en **moins de 20 lignes** | diff | **19 lignes**, accolades de cas comprises (plus 10 de commentaire), et 5 accesseurs `const` ajoutés à `MaterialColorExt` | aucun | — |
| G2 | supprimer `meshToObj` ne change **rien** à l'écran, hors `mtllib` dans l'amorçage | `maker/probes/pages_bootstrap.js` (oracle : maillage affiché et statistiques renseignées, pas un pixel) | **tenu** : `shapes.html` « 8 sommets · 12 faces », `svg.html` « 2540 sommets · 2480 triangles », bandeau d'erreur masqué des deux côtés | aucun | — |
| G2 | sortir la sérialisation du chemin d'évaluation **gagne 20 à 60 ms** par pas de curseur | compteur d'appels sur `graphExportObj` depuis la page réelle (`svg_page_export.js`) + coût par fonction (`graph_obj_cost.js`) | **59,1 ms et 2,9 Mo par pas, ramenés à zéro** : 0 appel pour 5 pas de curseur. Coût du chemin restant : `graphEvaluate` 0,0 ms + `graphMeshData` 2,0 ms | aucun | — |
| G3 | l'archive du Tigre porte **39** `newmtl` | archive interceptée au téléchargement de la page (`svg_page_export.js`), plus `ctest` (`tu_cgmesh_io_obj_materials.cpp`) | **40** `newmtl` pour **39** `usemtl` distincts, aucun orphelin, `mtllib` résolvant | +1 | **1** — la palette du document a 39 couleurs, mais `mesh.color` ajoute le sien à la table même quand il ne peint aucune face |
| G4 | l'écrivain GLB tient en **250 à 400 lignes** avec tinygltf | `wc -l src/cgmesh/mesh_io_gltf.cpp` | **384 lignes** : 229 de code, 108 de commentaire, 47 vides | aucun sur le total ; le code seul est sous la fourchette | — |
| G4 | `maker.wasm` croît de **80 à 250 Ko** | `stat -c%s maker/web/maker.wasm`, avant et après | 2 197 122 → **2 367 356 o**, soit **+170 234 o** (+7,8 %) | aucun | — |
| G4 | le GLB du Tigre pèse **3 à 6 Mo [E]** (nv fois 24 octets, plus ni fois 2) | `ls -la tu_tiger.glb` | **2 646 772 o** : chunk JSON 14 568 o, chunk BIN 2 632 176 o | sous la fourchette d'environ 12 % | **6** — l'estimation comptait juste sur les attributs (POSITION + NORMAL = 24 o par sommet) mais prenait les indices à 2 o alors qu'ils sont à 4 (74 296 sommets, donc `UNSIGNED_INT`), et surestimait en supposant des UV et des couleurs par sommet, que ce maillage n'a pas |
| G4 | R3 est **certaine** : un second `#define TINYGLTF_IMPLEMENTATION` doublerait les symboles au lien natif | lecture de `src/cgmesh/CMakeLists.txt` **avant** d'écrire une ligne | **exacte, et elle en cachait une pire** : `vmeshes_io.cpp` est **absent de la liste EMSCRIPTEN**, donc la seule implémentation de tinygltf du dépôt n'existait **pas** côté WASM. Sans unité dédiée, le natif aurait doublé ses symboles **et** le lien WASM n'aurait rien eu à résoudre | le document ne nommait qu'une moitié du risque | **1** — « `TINYGLTF_IMPLEMENTATION` est déjà défini » avait été vérifié ; « et l'unité qui le définit n'est pas compilée partout où on en a besoin » ne l'avait pas été |
| G4 | R4 : sans la conversion sRGB, le défaut est **silencieux** | `TEST_cgmesh_io_gltf.base_color_factor_is_the_srgb_colour_linearised`, exécuté conversion **neutralisée** | **exacte** : écart de **0,286** sur le canal rouge (0,502 écrit contre 0,216 attendu), et **aucun** des neuf autres tests ne bronche. Un GLB parfaitement valide, aux couleurs fausses | aucun | — |
| G5 | Blender montre **39** matériaux et `fitSize` mm | Blender **non exécuté** ; substitut : `maker/probes/glb_page_export.js`, lecteur o3dv — importeur glTF **indépendant** de tinygltf | **40 matériaux, 40 couleurs distinctes, écart maximal avec l'export OBJ relu par le même lecteur : 0/255 sur les 40**. Échelle : 97,008 × 100 × 3 mm en OBJ → 0,0970079 × 0,003 × 0,1 m en GLB, rapport **0,001 exact sur les trois axes**. Orientation : l'épaisseur passe de l'axe z (OBJ) à l'axe y (GLB), bornes **[0 ; +0,003]**, donc −90° et non +90° | 40 matériaux et non 39, comme pour l'archive OBJ de G3 (même cause) ; Blender lui-même n'a pas été ouvert (§8) | — |

**Prédictions déjà closes par cette étude** — elles portaient sur la commande, pas sur un jalon :

| Prédiction de la commande | Résultat | Écart | Cause |
|---|---|---|---|
| « il faut choisir ou vendoriser un écrivain ZIP » | `ZipManager` existe et est en production | total | **1** — affirmation sur l'existant, non vérifiée |
| « `exportObj` omet `mtllib` » | `exportObj` rend un ZIP OBJ+MTL depuis `wasm_api.cpp:481` | total | **1** |
| « aucune trace de glTF/GLB dans le dépôt » | tinygltf vendorisé, importeur GLB **en service** | total | **1** — l'instrument (recherche par nom) est majoré par les homonymes et **minoré par les indirections** : les sites qui consomment `tinygltf` ne contiennent ni « glTF » ni « GLB » dans leurs identifiants |
| « o3dv chercherait un `.mtl` absent de sa FileList » (commentaire du dépôt) | o3dv **tolère** l'absence et **accepte** deux fichiers | total | **1** |

> **Motif déjà visible : quatre erreurs sur quatre relèvent de la cause 1.** La règle de méthode qui
> s'en déduit, et qui vaut pour la suite de ce chantier : *avant d'écrire « X n'existe pas » ou « X
> fait Y », ouvrir la déclaration de X.* Une recherche textuelle établit une présence, jamais une
> absence ni un comportement.

---

## 8. Risques, réserves, et ce qui n'a pas été examiné

### Réserves nommées, avec leur condition de levée et le sens de leur biais

| # | Réserve | Levée par | Biais |
|---|---|---|---|
| **R1** | ~~o3dv tolère un `mtllib` orphelin et accepte deux fichiers~~ — **LEVÉE le 2026-09-06**, `maker/probes/o3dv_mtl.js` : les deux comportements sont **[M]**, sur `OV.EmbeddedViewer`, le point d'entrée de production | — | — |
| ~~**R2**~~ | ~~poids de tinygltf dans `maker.wasm`~~ — **LEVÉE le 2026-09-07** : **+170 234 octets** mesurés (2 197 122 → 2 367 356), dans la fourchette estimée et sous le seuil de G4-6. La piste B (écrivain maison sur nlohmann) n'a donc pas lieu d'être ouverte | — | — |
| ~~**R3**~~ | ~~conflit `TINYGLTF_IMPLEMENTATION`~~ — **LEVÉE le 2026-09-07**, et elle en cachait une pire : `vmeshes_io.cpp` n'est **pas** compilé en WASM, l'implémentation manquait donc de ce côté. Résolue par `src/cgmesh/tinygltf_impl.cpp`, unité dédiée compilée **des deux côtés**, et par les trois `TINYGLTF_NO_*` remontés en `target_compile_definitions(... PUBLIC)` — ces options changent la classe `TinyGLTF` elle-même, donc toutes les unités doivent les voir, la suite de tests comprise | — | — |
| ~~**R4**~~ | ~~la conversion sRGB vers linéaire est-elle requise ?~~ — **LEVÉE le 2026-09-07, dans les deux sens.** (a) Elle l'est : l'importeur glTF d'o3dv applique `LinearToSRGB` à `baseColorFactor`, l'omettre ferait donc ressortir le modèle **plus clair**. (b) Le défaut est bien **silencieux** : neutralisée, la conversion ne fait échouer qu'**un** test sur dix, celui écrit pour elle. Mesure de bout en bout : **0/255 d'écart** entre les 40 couleurs relues du GLB et celles relues de l'OBJ | — | — |
| **R5** | taille de `BuildPolygonRenderData` sur le Tigre **non mesurée** — les n-gons dupliquent leurs coins (`mesh.cpp:845-905`), donc le nombre de sommets de rendu dépasse celui du maillage d'un facteur inconnu | G0-3 étendu | **minorant** : l'estimation de 3 à 6 Mo part du nombre de sommets du maillage |
| **R6** | le Tigre a-t-il des matériaux à alpha inférieur à 1 ? Le SVG stocke l'alpha (`import_svg.cpp:738`) mais aucune mesure ne le dit | un compteur en G3 | si oui, `alphaMode: BLEND` s'arme et le rendu change |
| **R7** | le seuil « +250 Ko » de G4-6 est **arbitraire** — aucune contrainte de taille n'est écrite dans le dépôt | arbitrage utilisateur | c'est un garde-fou, pas une mesure |
| **R8** | `export_obj` en MEMFS sur environ 10^5 faces : 3 `fprintf` par coin (`debt_cgmesh.md:4437`), coût non mesuré à cette taille | G0-3 | l'ordre de grandeur importe : 20 ms et 2 s ne se traitent pas pareil |

### Ce que je n'ai **pas** examiné — déclaration explicite

- **`sinaia`, `vecna`, `sulina`** : leurs menus d'export ne sont pas lus. `MeshIO::export_obj` y est
  très probablement appelé, donc G1 les affecte — **en bien**, mais sans vérification.
- **`VMeshesIO::export_obj`** (`vmeshes_io.cpp:142`) : un chemin d'export **parallèle** pour les
  multi-maillages, dont `test/tu_cgmesh_vmeshes_io.cpp:30` dit que trois exports sur cinq sont des
  stubs. Non analysé ; il pourrait mériter le même traitement.
- **Le format 3MF**, cité par `zip_manager.h:12` comme futur consommateur du ZIP. **C'est le format
  qui porte la couleur POUR L'IMPRESSION** — ce que le GLB ne fait pas, aucun slicer ne le lisant.
  Si la finalité réelle est l'impression couleur, **3MF est un candidat plus pertinent que GLB**, et
  il réutiliserait `ZipManager` (le 3MF *est* une archive OPC). **Piste non instruite, à arbitrer.**
- **Le validateur glTF officiel** (`gltf-validator`) : non installé, non exécuté — **après G4 non plus**.
  Les oracles du §4.8 restent des oracles de cohérence interne, pas de conformité. Le relecteur tiers
  employé à la place, l'importeur glTF d'o3dv, est une implémentation **indépendante de tinygltf** :
  cela lève l'objection « écrire et relire avec la même bibliothèque », mais ne vaut pas conformité.
- **Blender** : non ouvert. Le critère G5-1 (« au moins 30 matériaux dans l'Outliner », « `fitSize`
  mm à 0,1 mm près ») a été rendu par une sonde navigateur, pas par Blender. Ce qu'un import Blender
  ajouterait et que la sonde ne donne pas : le comportement du convertisseur Y-up vers Z-up **de
  Blender en particulier**, et l'aspect du modèle sous son moteur de rendu.
- **L'orientation des FACES** (capot retourné) : `doubleSided` la rend sans effet sur la visibilité,
  et rien ne l'a mesurée. Une face inversée resterait donc invisible **en tant que défaut**.
- **Draco, meshopt, `KHR_texture_transform`** : hors périmètre.
- **`maker/probes` et `k4_probe.html`** : le harnais de mesure existant n'a pas été lu ; il porte
  peut-être déjà l'instrument dont G0-3 a besoin.
- **Le comportement d'o3dv à l'exécution** : rien n'a été lancé. Tout ce qui concerne o3dv est **[V]
  par lecture**, jamais **[M]**.
- **La compilation** : rien n'a été construit. Aucune affirmation de ce document ne repose sur un
  build, et les chiffres de taille sont des **[E]** sauf la référence `maker.wasm = 2 194 911
  octets`, qui est **[M]** par `ls -la`.

### Points à faire trancher par l'utilisateur

| # | Décision | Enjeu |
|---|---|---|
| **D-1** | **Périmètre** : s'arrêter à G3 (OBJ coloré partout, la demande initiale) ou aller jusqu'à G5 (GLB) ? | G0 à G3 est du travail petit et sûr ; G4 et G5 sont un format neuf |
| **D-2** | **Unités du GLB** : mètres via nœud (conforme glTF, Blender juste) **ou** millimètres bruts (cohérent avec notre OBJ) ? | crée une divergence assumée entre nos deux exports |
| **D-3** | **GLB ou 3MF ?** Si la finalité est l'**impression couleur**, aucun slicer ne lit le GLB ; le 3MF, lui, est lu par tous et réutilise `ZipManager` | le GLB sert le web et Blender, le 3MF sert l'imprimante |
| **D-4** | **tinygltf ou écrivain maison sur nlohmann ?** | quelques dizaines à centaines de Ko de WASM plus un conflit d'implémentation à résoudre, contre environ 60 lignes de conteneur à écrire soi-même |
| **D-5** | **Alpha dans le viewer** : le faire traverser jusqu'à three.js, ou seulement jusqu'aux fichiers ? | armer la transparence change l'ordre de rendu et peut **dégrader** ce qui est à l'écran |
| **D-6** | **Corriger `MATERIAL_COLOR_ADV` (G1) avant ou après G3 ?** | avant : l'aller-retour OBJ redevient fidèle. Après : G3 livre plus vite, sur un écrivain qu'on sait incomplet |
| **D-7** | **Seuil de taille WASM** en G4-6 : « +250 Ko » est arbitraire | fixe le point de bascule vers l'écrivain maison |
