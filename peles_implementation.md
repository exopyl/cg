# `peles` — exécution du lot 1

**Ouvert le** : 2026-09-22.
**Document de décision associé** : `peles.md` (faisabilité, pistes, verrous, décisions D-1…D-9, P-1…P-5).
Ce document-ci est **l'exécution** : la conception retenue, le découpage en étapes, les critères
d'acceptation et le **§12 Journal de prédictions**.

**Périmètre du lot 1**, arrêté par l'utilisateur le 2026-09-22 :
arbre de répertoires à **gauche**, liste de fichiers filtrée en **bas**, panneaux d'informations à
**droite**, **vue 3D `cgre` au centre**. **Pas de panneau Treatments.** Cible neuve `peles/`,
`sinaia/` non touché. RmlUi par `FetchContent`.

---

## 1. Les décisions de conception, et leur raison

### 1.1 🔴 Renderer RmlUi : **GL2**, pas GL3

Trois arguments vérifiés par lecture du dépôt, un documentaire.

**a. GL2 exige exactement le contexte que `cgre` exige.** `src/cgre/surface_program.h:33-40` :
GLSL `#version 120`, `gl_ModelViewProjectionMatrix`, `gl_NormalMatrix`, `gl_LightSource[]`,
`gl_FrontMaterial`, chemin VBO par `glVertexPointer`/`glEnableClientState` — *« C'est un ECHAFAUDAGE,
pas une cible. Il lie cgre au profil de compatibilité. »* Le renderer GL2 de RmlUi est un renderer de
pipeline fixe : il vit dans le même profil. GL3 demande un niveau 3.3.
Un contexte de compatibilité 3.3+ est un sur-ensemble du core 3.3 et exposerait donc ce que GL3 veut —
⚗ **mais c'est conditionné à ce que le pilote accorde un profil de compatibilité ≥ 3.3**, ce qui n'est
pas vérifiable par lecture et relève de la sonde. **GL2 ne pose pas cette condition du tout.**

**b. La collision de chargeurs GL — vérifiée, et c'est le point le plus concret.**
`src/cgre/glad/gl.h` ne déclare pas seulement des pointeurs, il **redéfinit chaque entrée GL par macro** :

    src/cgre/glad/gl.h:4345-4346   #define glPopAttrib  glad_glPopAttrib
    src/cgre/glad/gl.h:4949-4950   #define glUseProgram glad_glUseProgram

**Règle non négociable qui en découle** : *aucune unité de traduction n'inclut à la fois
`src/cgre/gl_wrapper.h` et un en-tête GL de RmlUi.* Le risque vaut pour GL2 comme pour GL3, mais GL2
n'a besoin que d'entrées GL 1.x, exportées statiquement par `opengl32.lib`/`libGL` : il n'amène pas un
second chargeur complet dans le processus.

**c. En profil de compatibilité, l'hôte sauve l'état lui-même — c'est la réponse structurelle à V4.**
`glPushAttrib(GL_ALL_ATTRIB_BITS)` / `glPopAttrib` et leurs équivalents client existent et sont chargés
par le glad de `cgre` (`glad/gl.h:4345, :4465` ; `glad/gl.c:1280, :1283`). **Le dépôt s'en sert déjà
ainsi** : `src/cgre/mesh_renderer.cpp:126/145, :155/170, :373/394, :405/441, :447/477` ;
`src/cgre/widgets_renderer.cpp:172/188, :244/257`. Le moteur de sauvegarde d'état le plus complet
disponible est celui du pilote, et **il n'existe qu'en compatibilité**.

**d.** ⚗ GL2, étant un pipeline fixe, n'a ni VAO, ni programme, ni FBO : il ne peut pas les salir.
**Raisonnement, pas lecture** — `RmlUi_Renderer_GL2.cpp` n'est pas encore dans l'arbre. Le contrat §1.2
est conçu pour rester valide si ce raisonnement est faux.

### 1.2 🔴 Contrat d'état OpenGL — il ne fait confiance à personne

**Principe** : ni `cgre` ni RmlUi ne sont réputés propres. **L'hôte** pose une clôture RAII
(`app/gl_state_fence.h`) autour de chacun.

**Qui salit quoi, relevé par lecture :**

| Moteur | État sali | Site |
|---|---|---|
| `cgre` / `DrawGL` | `GL_NORMALIZE` activé, **jamais désactivé** | `sinaia/wxOpenGLCanvas.cpp:1370` — défaut n°3 de `debt_sinaia.md` |
| `cgre` / `MeshRenderer::Draw` | `GL_NORMALIZE` activé puis désactivé — **ne fuit pas** | `src/cgre/mesh_renderer.cpp:96` et `:484` |
| `cgre` / `DrawGL` | `GL_LIGHTING`, `glShadeModel`, `glClearColor`, la modelview | `wxOpenGLCanvas.cpp:1363-1382` |
| `cgre` / surbrillance de survol | ✅ correctement encadré par `glPushAttrib`/`glPopAttrib` | `:1453` et `:1480` |
| `cgre` / `GlProgram` | `glUseProgram` apparié **dans** `GlProgram` | `src/cgre/gl_program.cpp:187, :192` |
| RmlUi GL2 | **non examiné** — le fichier n'est pas dans l'arbre | — |

⚠ **`glPushAttrib` ne couvre PAS le programme courant, ni les VAO, ni les liaisons de tampon.**
`glUseProgram(0)` est donc posé explicitement **aux deux bords** : un appelant qui laisserait un
programme lié verrait le pipeline fixe du voisin dessiner à travers son fragment shader, **sans erreur
GL**.

**Ordre des 8 phases d'une image :**

| # | Phase | Clôture |
|---|---|---|
| 1 | `glfwPollEvents`, arbitrage des entrées | — |
| 2 | `Rml::Context::Update()` | — |
| 3 | Lecture du rectangle de `#viewport` | — |
| 4 | `glViewport` plein cadre, `glClearColor`, `glClear` — **l'hôte efface, pas `cgre`** | — |
| 5 | Vue 3D : `glViewport`+`glScissor` sur le rectangle, puis le chemin `DrawGL` porté | `GlStateFence` |
| 6 | `glDisable(GL_SCISSOR_TEST\|GL_DEPTH_TEST)`, `glViewport` plein cadre | — |
| 7 | UI : `Rml::Context::Render()` | `GlStateFence` |
| 8 | `glfwSwapBuffers` | — |

**Trois invariants globaux, posés à l'initialisation et jamais touchés** : `GL_FRAMEBUFFER_SRGB`
**désactivé**, aucun FBO lié, **aucun VAO lié**. Les trois sont posés par `ApplyGlobalInvariants()`,
qui pose en outre `glUseProgram(0)` — le pendant à l'initialisation de ce que la clôture pose à chaque
bord. ⚠ **Le troisième invariant n'existait pas dans le code jusqu'à la revue de l'étape 2** : il est
désormais posé (`glBindVertexArray(0)`, sous garde de pointeur, l'entrée n'existant qu'à partir de
GL 3.0). Il n'est pas sans objet en profil de compatibilité — le VAO 0 y est un objet valide, et c'est
lui que le pipeline fixe et les tableaux clients de `cgre` utilisent ; un VAO laissé lié par un tiers
ferait lire les attributs **ailleurs**, sans erreur GL.

⚠ **Le grief de `peles.md` §4 V4 se retourne, et c'est vérifié** :
`src/cgre/surface_program.h:72-75` dit que le programme PBR encode la sortie par `pow(1/2,2)` **dans le
fragment**, précisément parce que *« GL_FRAMEBUFFER_SRGB re-encoderait aussi les surcouches, dessinées
en fixe-fonction dans le même tampon »*. Que le renderer de RmlUi désactive `GL_FRAMEBUFFER_SRGB` sans
le réactiver est donc, **pour ce projet, le comportement souhaité**. C'est posé comme invariant, pas
comme état à restaurer.

**Comment le contrat se vérifie** — `peles --state-audit` relève, **de part et d'autre de CHACUNE des
deux phases sous clôture (5 et 7)**, les états nommés suivants :

| Famille | États |
|---|---|
| Programme | `GL_CURRENT_PROGRAM` |
| Matrices — **hors `glPushAttrib`** | `GL_MATRIX_MODE`, `GL_PROJECTION_MATRIX`, `GL_TEXTURE_MATRIX`, `GL_MODELVIEW_MATRIX` (comparaison **bit à bit** sur les 16 flottants) |
| Textures | `GL_ACTIVE_TEXTURE`, `GL_TEXTURE_BINDING_2D[0…N-1]` |
| Bascules | `GL_BLEND`, `GL_DEPTH_TEST`, `GL_CULL_FACE`, `GL_NORMALIZE`, `GL_LIGHTING`, `GL_SCISSOR_TEST`, `GL_STENCIL_TEST` |
| Masques et cadre | `GL_STENCIL_WRITEMASK`, `GL_VIEWPORT`, `GL_FRAMEBUFFER_SRGB`, `GL_FRAMEBUFFER_BINDING` |

**Critère : les deux relevés sont identiques, sur les deux phases.** Un écart **nomme** l'état fautif.

⚠ **AUCUN NOMBRE D'ÉTATS N'EST ÉCRIT DANS LE CODE NI DANS CE TABLEAU.** `N` est dérivé de
`GL_MAX_TEXTURE_IMAGE_UNITS` borné à 8 — huit couvre les unités 0 et 1 du pipeline fixe **et** les
unités 2 à 6 du chemin PBR de `cgre` (`src/cgre/vertex_buffer_manager.cpp`, `kPbrUnitOffset`). Le
décompte imprimé par l'instrument est dérivé du relevé lui-même (`GlStateSnapshot::Entries()` est
l'unique source de `Describe()`, `StateCount()` et `DifferingStates()`). **Mesuré sur la machine de
référence : 25 états** (N = 8).

⚠ **`GL_CURRENT_PROGRAM` n'est pas relevé au même instant que les autres, et il ne peut pas l'être.**
La clôture pose `glUseProgram(0)` **aux deux bords** : deux relevés pris de part et d'autre par
l'appelant vaudraient 0 des deux côtés **par construction**, quoi qu'ait fait le moteur encadré —
l'état figurerait au relevé sans pouvoir jamais diverger. C'est pourquoi **c'est la clôture qui
relève**, et qu'elle lit le programme **avant** son `glUseProgram(0)` de sortie. Le reste est lu après
dépilement, puisque c'est le dépilement que le contrat mesure.

⚠ **Correction du 2026-09-22, appliquée à l'étape 2.** Ce paragraphe faisait porter `--state-audit`
par **`peles_probe`**. **C'était une erreur** : `peles_probe` ne lie pas RmlUi — c'est précisément ce
qui le rend bâtissable là où RmlUi ne peut pas être tiré — donc il ne peut pas mesurer l'état autour
de `Context::Render()`. Le mode est porté par **`peles`**.

⚠ **Biais structurel de cet instrument, mesuré.** La comparaison est **intra-image** : un état qui
fuit reste fuit, donc le relevé « avant » de l'image suivante le contient déjà et les deux coïncident
à nouveau. Un défaut de clôture se voit sur la **première** image et se masque ensuite. Mesuré :
clôture retirée de la phase 7, le relevé rend « **1 image sur 3** divergent », pas 3 sur 3.
Le compte d'images divergentes ne se lit donc **pas** comme un taux — et **l'avertissement est porté
par la ligne de verdict elle-même**, pas par un commentaire invisible depuis la console.

⚠ **Conséquence directe sur la preuve imprimée** : l'instrument conserve et imprime le couple
avant/après de la **première image divergente**, en nommant son numéro et sa phase — jamais celui de
la dernière image rendue, qui serait conforme sous un verdict d'échec exactement dans le cas où l'on
en a le plus besoin.

⚠ **Une image dont les huit phases n'ont pas couru n'est pas une image auditée.** Fenêtre réduite, ou
`#viewport` de taille nulle : `RenderFrame` rend `false`, l'image n'entre ni dans le compte d'images
auditées ni dans les temps, et le verdict devient `AUDIT NON CONCLUANT` / `EXIT=1`. C'est la même
distinction que le `SKIP` de P8 à l'étape 1 ; l'étape 2 la rendait `PASS`, qui est le seul des trois
verdicts franchement faux.

**Escalade écrite d'avance, avec son seuil** : si un état échappe à `glPushAttrib`, (1) l'ajouter au
relevé et au contrat **côté hôte** ; (2) **seulement si (1) a échoué**, copier
`RmlUi_Renderer_GL2.{cpp,h}` sous `peles/rmlui/` avec un en-tête de provenance et un diff documenté —
ce que RmlUi autorise et attend pour ses *backends*. **On ne copie pas « au cas où ».**

**Niveau (1) appliqué, et aux quatre états concernés** : les trois matrices et le mode de matrice sont
empilés par la clôture **et** relevés par l'instrument. Jusqu'à la revue de l'étape 2, ils étaient
empilés sans être relevés — l'instrument n'aurait donc pas vu leur disparition. Le niveau (2) n'a pas
été atteint : aucune copie de `RmlUi_Renderer_GL2.{cpp,h}` n'existe.

### 1.3 🔴 Le rectangle du viewport — **on lit la géométrie RmlUi, on ne la recalcule pas**

> Un `<div id="viewport"/>` est placé dans le RML, et sa géométrie résolue est lue à chaque image
> **après** `Context::Update()`. **Le C++ ne connaît aucune largeur de panneau.**

C'est l'inverse de LichtFeld Studio (`panel_layout.cpp:1145-1162`, cf. `peles.md` §2.5), et le précédent
ne transpose pas : leur layout n'est pas purement RCSS — ils portent leurs propres séparateurs et font
déjà le test de collision en C++. Ici, D-3 exclut le docking flottant et P-1 fige quatre zones :
**le layout est un flex RCSS, et RmlUi en est la seule source.**

| Objection | Traitement |
|---|---|
| Ordre update/render | Résolu par l'ordre des 8 phases : `Update()` (2) précède la lecture (3). ⚗ à confirmer : que `Update()` n'émette aucun appel GL — le relevé `--state-audit` le vérifie |
| Élément introuvable vs élément de taille nulle | ⚠ **Deux cas de nature différente, et les confondre rend le repli muet.** `ReadViewportRect` rend un `ViewportRectStatus` à trois valeurs. `ElementMissing` est une **faute de configuration** — le `.rml` est une donnée d'exécution éditable sans recompiler (§1.10) — instruite **à l'initialisation**, avec le **code 3**, en nommant l'identifiant et le répertoire d'assets. `DegenerateSize` n'est **pas** une faute : rien à dessiner ⇒ **phase 5 sautée**, et l'image ne compte pas comme rendue. **Aucun repli sur le plein cadre** : tant que `#viewport` occupe tout le cadre, un tel repli serait **indiscernable du succès** |
| Unités `dp` vs pixels | **Tranché par décision : le lot 1 n'emploie AUCUNE unité `dp`.** Seulement `px`, `%`, flex. `Context::SetDimensions` reçoit la taille du *framebuffer*. **Conséquence assumée : pas de HiDPI dans le lot 1** |
| Origine des coordonnées | RmlUi : haut-gauche. GL : bas-gauche. La conversion vit à **un seul endroit**, `ViewportRect::ToGlViewport()` |

⚠ **Une troisième règle RCSS, ajoutée à l'étape 4, et c'est la correction d'un défaut MESURÉ :
`min-height: 0px` (et `min-width: 0px`) sur les quatre zones et sur `#row`.** Le minimum
**automatique** d'un élément flex vaut la taille minimale de son **contenu** : dès que la zone gauche
a reçu 1000 lignes, `#row` a mesuré **18 826 px** de haut au lieu de 720, la vue 3D a suivi, et GL a
borné le viewport à `GL_MAX_VIEWPORT_DIMS` = 16 384. **`overflow: hidden` ne suffit pas** — il coupe
l'affichage, il ne contraint pas la taille minimale. C'est le détecteur permanent de concordance
`GL_VIEWPORT` (étape 3, critère 3) qui l'a nommé : `attendu (240, -18026, 760, 18826), relevé
(240, -18026, 760, 16384)`. **Sans ce détecteur, la géométrie serait devenue dépendante du répertoire
ouvert, et `--dump-layout` — qui ne peuple aucun panneau — serait resté vert.**

**Deux règles RCSS sur `#viewport` qui ne sont pas cosmétiques** : `background: transparent` (sinon la
phase 7 repeint par-dessus la 3D) ; et l'élément **reste sensible au pointeur** — c'est lui qui sert de
cible de survol pour l'arbitrage souris. **Ne pas** y poser `pointer-events: none`.

### 1.4 🔴 L'architecture, et comment le cycle de `sinaia` est empêché

```
peles/
  main.cpp                    hôte : arguments, modes --dump-*, construit Application
  assets/peles.{rml,rcss}     la structure des 4 zones + la feuille de style
  assets/fonts/               une police OFL versionnée, avec sa licence

  core/      → peles_core      [ lie cgmesh. NE LIE NI glfw NI RmlUi NI cgre ]
     document, document_observer, loader, file_catalog, directory_tree,
     file_list, model_info, topology_stats, ui_dto
  view3d/    → peles_view3d    [ lie peles_core + cgre. NE LIE PAS RmlUi ]
     camera, picking, scene_renderer
  platform/  → peles_platform  [ lie glfw + RmlUi. NE LIE PAS cgre ]
     window_glfw, rml_system_interface, rml_render_interface, rml_host
  ui/        → peles_ui        [ lie peles_core + RmlUi. NE LIE NI cgre NI glfw ]
     view_model, ui_controller, input_router, viewport_rect
  app/       → peles (exe)     [ lie les quatre + cgre + glfw + RmlUi ]
     application, gl_state_fence, commands, asset_paths, dump_modes
```

Les dépendances sont strictement descendantes. **Aucune flèche entre `ui` et `view3d`.**

⚠ **Correction de la revue de l'étape 2 — la réserve sur l'exécutable était trop pessimiste.** Il avait
été écrit que, l'exécutable liant les quatre cibles et voyant donc tous les répertoires d'inclusion, la
frontière n'y était plus tenue que par l'ordre d'inclusion d'`app/application.cpp`. **C'est faux, et
c'est vérifié** : `src/cgre/glad/gl.h:36-37` rend `#error` si `__gl_h_` est déjà défini, et c'est ce que
définit tout en-tête GL de système (`um/gl/GL.h:16-20` du SDK Windows, comme celui de Mesa). Le seul
en-tête qui pourrait entrer en conflit ici, `RmlUi_Renderer_GL2.h`, est de surcroît **inatteignable** :
`Backends/` est un chemin d'inclusion **PRIVATE** de `peles_platform`. **La frontière est donc tenue
par le compilateur dans l'exécutable aussi.** Une réserve surévaluée coûte autant qu'une réserve
manquée : elle fait porter une vigilance humaine là où une vérification mécanique existe déjà.

> **Le cycle est empêché mécaniquement** : `peles_core` est une **cible CMake réelle qui ne lie ni
> RmlUi, ni glfw, ni `cgre`**. Qu'un fichier de `core/` inclue l'un des trois **ne compile pas**. La
> frontière est vérifiée par le compilateur à chaque build, pas par un relecteur.

⚠ **L'arête `view3d → app` n'est PAS une arête d'édition de liens, et elle n'est pas couverte par
l'argument ci-dessus.** `app` **est** l'exécutable : aucune bibliothèque statique ne peut le lier, donc
un `#include "app/application.h"` écrit depuis `view3d/` ne produirait pas même un symbole non résolu —
`application.cpp` est dans la même cible finale. **Cette arête est tenue par le seul chemin
d'inclusion**, et c'est la raison pour laquelle **aucune** des quatre bibliothèques ne reçoit
`${CMAKE_CURRENT_SOURCE_DIR}` : `app/…`, `platform/…` et `ui/…` sont introuvables depuis elles. La
racine n'est donnée qu'à l'exécutable (`peles/CMakeLists.txt`, `target_include_directories(peles
PRIVATE …)`), pour le seul `#include "view3d/camera.h"` d'`app/application.cpp` — le préfixe y est
nécessaire parce que `src/cgmesh/camera.h` existe et que `cgmesh` publie son répertoire. Les unités de
`view3d/` écrivent `#include "camera.h"` sans préfixe : la règle « répertoire du fichier incluant
d'abord » les fait résoudre vers `view3d/camera.h`.

⚠ **Cette arête a été réellement ouverte, et c'est mesuré** (revue de l'étape 3, F2) : tant que
`peles_view3d` recevait la racine en `PUBLIC`, `#include "app/application.h"` depuis
`view3d/scene_renderer.cpp` **compilait et liait** — `application.h` ne nomme `platform::Window`,
`platform::RmlHost`, `view3d::*` et `ui::GlViewport` que par déclaration anticipée. `SceneRenderer`
pouvait remonter à `Application` exactement comme `MyGLCanvas` remonte à `MyFrame`. **La batterie de cas
négatifs couvre désormais cette surface**, et pas seulement `gl_wrapper.h` et `<RmlUi/Core.h>`.

C'est ce qui manque à `sinaia` : `MyGLCanvas` et `MyFrame` sont dans la **même** cible, donc
`dynamic_cast<MyFrame*>(wxGetTopLevelParent(this))` compile sans obstacle — six fois
(`wxOpenGLCanvas.cpp:51, 327, 1615, 1703, 1772, 1817`). Ici il n'y a **pas de type auquel remonter** :
`scene_renderer` ne connaît pas `Application`.

**Remplacement du couplage inverse** : `HighlightModelRow` → `core::Document` porte `hovered`, que
`ui::ViewModel` relit à chaque image (personne n'appelle personne) ; `RefreshModelSelection` → le clic
produit une `Command::SelectModel` remise à `Application`.

⚠ **Correction de l'étape 6 — le type de commande vit dans `ui/`, pas dans `app/`.** Ce paragraphe
et la liste ci-dessus le rangeaient sous `app/commands`. **C'est impossible, et c'est le pare-feu
lui-même qui l'interdit** : le producteur est un rappel `data-event-` lié par `peles_ui`, et `app/`
est introuvable depuis cette cible — c'est le cas négatif n° 17. Le type est donc
`ui/ui_command.h` ; `Application` le draine par `UiController::TakeCommands()`. La direction des
dépendances est inchangée : personne n'appelle vers le haut, l'hôte vient lire.

⚠ **Et le survol n'est PAS relu par `ui::ViewModel`, contrairement à ce que ce paragraphe annonçait.**
`UiController` ne connaît pas le document — il reçoit ce que l'hôte lui désigne, et c'est ce qui le
garde libre de tout chemin vers la vue 3D. Au terme de l'étape 6, `Document::Hovered` n'est lu que
par `view3d::SceneRenderer`, pour la boîte jaune : la mise en évidence de la **ligne** correspondante
n'est pas câblée.

⚠ **Pas de classe `Managers` dans `peles`, et le motif est de fond** — non pas que
`sinaia/Managers.{h,cpp}` soit du code mort (corps intégralement en commentaire, `Managers.h:3-25`,
58 l. compilées pour rien, `sinaia/CMakeLists.txt:32`), mais qu'**un objet nommé « Managers » n'a pas
d'invariant** : il ne répond à aucune question de la forme « qu'est-ce qui est vrai de cet objet ». Un
sac que tout le monde atteint redevient la frame de `sinaia` sous un autre nom, et le cycle rentrerait
par la porte qu'on vient de fermer.

### 1.5 🟠 Data binding pour les quatre zones, aucune manipulation directe du DOM

Les cinq limites déclarées par RmlUi (`peles.md` §2.4) sont toutes neutralisées **par la structure**, pas
contournées :

| Limite | Pourquoi elle ne mord pas |
|---|---|
| *dirty* à la variable racine seulement | L'opération réelle (`Toggle`, `Populate`, une sélection) **reconstruit le vecteur entier**. Il n'y a rien à salir plus finement |
| suppression d'élément dans `data-for` = comportement indéfini | Personne n'appelle `RemoveChild` sur un élément engendré ; seul le **vecteur source** change de taille. ✅ **Confirmé** : le rétrécissement ne produit ni plantage ni erreur ASan (§6, étape 7). ⚠ **Mais une SEPTIÈME contrainte s'y cache, trouvée à l'étape 7 — voir ci-dessous** |
| pas de binding sur membres hérités ni `const` | Les DTO **n'héritent de rien et n'ont aucun membre `const`** — c'est leur unique raison d'être. ⚠ Décisif pour les matériaux : `Material` est **polymorphe** (`dynamic_cast` vers `MaterialColor*`, `MaterialTexture*`, `MaterialColorExt*`, `MaterialPbr*`, `SinaiaFrame.cpp:2314, 2326, 2386, 2408`) — le lier directement serait **impossible** |
| `data-` ajouté après attachement = sans effet | Tous les `data-` sont dans le `.rml` |
| incompatibilité `<tabset>`/`<panel>`/`<tab>` | **Le lot 1 n'a aucun onglet** (P-2 + D-3). **La pire limitation documentée de RmlUi ne mord pas sur ce lot** — argument pour faire celui-là en premier |

⚠ **Une SIXIÈME contrainte, que RmlUi ne documente pas et que la mesure a trouvée (étape 4) :
le modèle de données doit exister AVANT `LoadDocument`.** Un élément résout son attribut
`data-model` **à son attachement** ; un modèle créé après ne sera jamais retrouvé. RmlUi ne rend
alors aucune erreur au code appelant : il journalise `Could not locate data model 'peles' in element
body` et affiche un document **vide**. D'où la séparation de `RmlHost::Create` (interfaces,
contexte) et de `RmlHost::LoadDocument` — le liage vit entre les deux.
⚠ **Le mode de panne est pernicieux parce qu'il rend l'instrument vert** : avec le modèle introuvable,
le temps de `Context::Update()` mesuré à **100, 1000 et 5000 lignes valait 0,015 ms dans les trois
cas**. Un tel plateau est la signature d'un document qui ne construit rien, et c'est ce plateau — pas
un message — qui a révélé la faute.

⚠ **SEPTIÈME CONTRAINTE, trouvée par la mesure à l'étape 7, et c'est la plus coûteuse des sept :
aucun `data-attr-*` ni `data-style-*` ne peut vivre sur l'élément qui porte `data-for`.**

RmlUi trie ses vues par **profondeur** avant de les mettre à jour, précisément pour que la vue
`data-for` retire ses éléments en trop **avant** que les vues de ces éléments ne relisent le vecteur
(`Source/Core/DataView.cpp:104`, commentaire à l'appui). Le tri se fait sur
`sort_order = biais + 1000 + 2000 × profondeur` ; or `DataViewFor` et `DataViewAttribute` ont tous
deux un biais **nul** (`Source/Core/DataViewDefault.cpp:427` et `:60`). Sur le **même** élément, les
deux vues sont donc **à égalité**, `std::sort` n'est pas stable, et l'ordre retenu dépend des
adresses des vues — donc du tas. Une ligne engendrée lit alors `rows[i]` avec `i ≥ size`, RmlUi
journalise `Data array index out of bounds` en **`LT_WARNING`**, et le contrôle de journal de
l'étape 5 fait sortir le processus en **3**.

⚠ **Le défaut ne se déclenche qu'au RÉTRÉCISSEMENT d'un vecteur lié, et il est NON DÉTERMINISTE.**
Mesuré sur le binaire de l'étape 6, douze exécutions par instrument : `--reopen` **5 fois sur 10**
en `EXIT=3`, `--open-second` **5 sur 10**, `--churn-tree 100` **1 sur 10**, `--reselect 3` **0 sur
10** (ses vecteurs ne rétrécissent pas). C'est ce qui l'a fait traverser six revues : une exécution
verte ne prouve rien.

**Correction** : les quatre listes liées sont faites de **deux `div` imbriqués** — l'extérieur porte
`data-for` et les `data-event-*`, l'intérieur porte `data-attr-class`. Le `data-attr-*` est alors
un niveau **plus bas**, donc trié **après**. Mesuré après correction : **0 sur 48** exécutions,
tous instruments confondus. ⚠ **Et la géométrie ne bouge pas** : les seize instruments rendent une
sortie **identique octet pour octet**, `--dump-scroll` compris (4334 px, 16 crans).

**Une seule racine de binding**, `ViewModel`. Découper en trois serait une fausse économie : le coût est
dans le parcours, et les trois zones se salissent **ensemble** (une sélection de fichier les touche
toutes). ⚠ **Repli connu** si le profilage de l'étape 4 dit le contraire : découpage en trois racines.

### 1.6 🟠 Ce que les panneaux remplacent

| Ce qui disparaît | Remplacé par |
|---|---|
| `wxPropertyGrid` | `<div class="propgrid">` en `display: table`, deux colonnes, `data-for` sur `vector<InfoRow>`. ⚠ **Correction de l'étape 5 : `table-layout` N'EXISTE PAS dans RmlUi** (`Source/Core/StyleSheetSpecification.cpp:307`) — sa mise en page de table se comporte toujours comme la variante fixe. L'écrire produirait un `LT_WARNING`, donc `EXIT=3` |
| `wxTreeCtrl` ×2 | **arbres aplatis** : `vector<InfoRow>` avec un champ `depth`, une classe RCSS `.d0`…`.d4` (l'étape 5 en a eu besoin de cinq : racine, maillage, matériau, détail, sous-détail). Pas de repli/dépli — `sinaia` les laisse dépliés (`Expand(root)` `:2267, :2465, :2468`) |
| **Le défilement du `wxTreeCtrl`** | ✅ **Rendu à l'étape 6** : `overflow-y: auto` sur les trois zones, la barre stylée dans `peles.rcss` — RmlUi n'embarque aucune feuille par défaut, donc sans ces règles la barre existerait et serait **invisible** — et la molette routée par l'arbitre. **MESURÉ avant** : 4008 px de contenu dans 720, **82 % inatteignable**. **MESURÉ après** (`--dump-scroll`, zone de 620 px) : 4334 px de contenu, la dernière ligne passe de `y ∈ [4316, 4332]` à `y ∈ [602, 618]`, donc **0 % inatteignable**. ⚠ **La hauteur du contenu augmente de 4008 à 4334 px, et c'est la barre elle-même** : elle réduit la largeur utile, donc quelques libellés passent sur deux lignes. ⚠ **`overflow-x` reste `hidden`** : les lignes sont en `white-space: nowrap`, et une barre horizontale mangerait de la hauteur à chaque changement de répertoire |
| `wxImageList` + `MakeColorSwatch` (25 l. de GDI) + **sa purge** (`:2279-2282`, nécessaire car *« sur un modèle à 98 matériaux, ça se voit vite »*) | un champ `std::string swatch` et `data-style-background-color`. **Zéro ligne de dessin, zéro liste d'images, zéro fuite.** C'est la démonstration la plus nette du motif D-1 dans tout le lot. ⚠ **Correction de l'étape 5 : le champ vaut `"#rrggbb"` ou `"transparent"`, JAMAIS la chaîne vide.** Mesuré : une valeur vide fait journaliser `Syntax error parsing inline property declaration 'background-color: ;'` à chaque image |
| L'infobulle du chemin (`SetHelpString` `:2211`) | RmlUi n'a pas d'infobulles → une **14ᵉ ligne** `Path`, portée par un `<textarea>`. ✅ **D-5 est REFERMÉE à l'étape 7**, par **trois** choses et non deux : routage clavier (`Context::ProcessKeyDown`/`KeyUp`), surcharge de `SystemInterface::SetClipboardText`/`GetClipboardText` vers glfw, et — ce qui n'était pas prévu — **le clic donne déjà le focus au contrôle**, donc aucune sélection n'était à réécrire. ⚠ **RmlUi 6.3 n'implémente toujours pas `readonly`** : ce qui tient la ligne non modifiable est que **seules dix touches sont routées** — A, C, les quatre flèches, Origine, Fin, Page haut/bas. Ni `ProcessTextInput`, ni `KI_V`, ni `KI_X`, ni `KI_BACK`, ni `KI_DELETE`. **Ajouter une entrée à `ui::Key` rend la ligne modifiable** ; c'est écrit à côté de l'énumération |

⚠ **Un couplage de `sinaia` à casser, absent de `peles.md`** : `Borders` et `Non manifold edges`
viennent de la **vue** (`pGLCanvas->GetNBorders()`, `GetNNonManifoldEdges()`,
`SinaiaFrame.cpp:2219-2220`). Dans `peles` ils viennent de `core/topology_stats` — sans quoi `core`
dépendrait de `view3d` et le pare-feu tomberait dès la première étape. C'est aussi ce qui rend
`--dump-info` possible **sans contexte GL**.

### 1.7 🟠 L'arbitrage souris

Rien dans la chaîne glfw → `Context::ProcessMouse*` ne dit « ce clic était pour la 3D ». L'arbitre est
`Context::GetHoverElement()` comparé à `"viewport"` — **le test de survol n'est pas réimplémenté** : le
refaire en comparant aux bords du rectangle donnerait un second calcul de géométrie, exactement la
duplication que §1.3 refuse, et il se tromperait dès qu'un panneau recouvre la vue.
⚗ Le `bool` rendu par `ProcessMouse*` **n'est pas utilisé** : sa sémantique (« non consommé » vs « pas
de survol ») est ambiguë dans la doc.

**Règle de capture** : dès qu'un bouton est enfoncé **au-dessus de** `#viewport`, le propriétaire est
verrouillé sur `Viewport` jusqu'au relâchement de **ce** bouton, quel que soit le survol entre-temps.
Sans elle, un orbite qui passe sous un panneau rendrait la main à l'UI en plein glisser — c'est le
défaut n°5 de `debt_sinaia.md`, que `sinaia` a faute d'appeler `CaptureMouse`.

⚠ **Forme retenue à l'étape 6, plus forte que l'énoncé** : le propriétaire se fige au **premier**
enfoncement et ne change plus tant qu'un bouton reste enfoncé. Ré-arbitrer sur un second bouton
produirait un état mixte — un bouton à la vue, un autre à l'interface — dont le relâchement irait au
mauvais destinataire, et l'un des deux garderait un bouton enfoncé à jamais.

⚠ **Le gel du propriétaire ne suffit PAS, et c'est la correction H1 de l'étape 6.** Il ferme l'état
mixte du côté de l'arbitre et le laissait **entièrement ouvert du côté de la caméra** :
`ApplyViewportGesture` mappait *chaque* enfoncement et *chaque* relâchement sur un trackball qui n'a
qu'**un seul slot d'état** (`tb_state` / `tb_button`), et `Ctrackball::mouse_press` pose
`tb_state = RELEASED` **quel que soit le bouton**. Un second bouton relâché pendant un glisser tuait
donc le geste en cours — rotation exactement 0° — et, dans la variante gauche-pendant-un-dolly, son
déplacement nul passait sous le seuil clic/glisser et **sélectionnait un modèle au milieu du dolly**.
`InputRouter::GestureButton()` mémorise le bouton qui a ouvert le geste ; `BeginViewportGesture` et
`EndViewportGesture` n'agissent que pour lui. **Mesuré par `--drag-bump`, sur ses deux versants.**

⚠ **R-6 — statut exact, corrigé à l'étape 6.** *Levée sur **Win32** par lecture de source* : le
backend Win32 appelle `SetCapture` au premier bouton enfoncé et `ReleaseCapture` au dernier
relâchement, donc les déplacements continuent d'arriver hors de la zone client. *Supposée vraie sous
X11 et Wayland **par le protocole*** — grab implicite du serveur X sur `ButtonPress`, grab implicite
de `wl_pointer` — **non lue, non mesurée**. **`--probe-drag` n'a pas été lancé et P8 reste un
`SKIP`.**

⚠ **La preuve citée jusqu'ici ne portait pas la conclusion sous X11** : glfw ne pose **aucun**
`XGrabPointer` sur enfoncement — `captureCursor()` n'y est appelé que pour `GLFW_CURSOR_DISABLED`.
La conclusion reste juste (le grab implicite du serveur suffit), mais elle vient du **protocole**, pas
du code de glfw.

⚠ **Le rappel de sortie de cadre ne rompt PAS la capture**, et c'est délibéré : un glisser qui sort
de la fenêtre est un glisser qui continue. Il ne fait qu'ordonner à RmlUi d'oublier son survol.

⚠ **TOUT `move` va aux deux** (caméra **et** RmlUi), **y compris sous capture de la vue**, et la
règle écrite ici dit désormais ce que le code fait — elle ne disait que le cas « sans bouton ».
Deux raisons, et la seconde est la plus forte :

- sans cela le survol de RmlUi resterait figé sur le dernier élément touché et l'arbitrage
  deviendrait collant dès la fin du glisser ;
- **c'est ce qui rend la règle de capture falsifiable**. La garde d'inutilité de `--input-script`
  (étape 3, `i == 2`) exige que le survol de RmlUi ait **quitté** la vue au déplacement sur le
  panneau : si le `move` n'y était plus remis, « vue pour les trois » serait vrai sans que la règle
  de capture ait rien eu à tenir, et le seul détecteur d'arbitrage du lot deviendrait vide.

⚠ **Conséquence produit assumée, et elle est visible** : `#dir-tree div:hover` et
`#file-list div:hover` (`peles.rcss:247, :281`) allument les lignes traversées **pendant une
orbite**. C'est un arbitrage entre un défaut cosmétique et la perte du détecteur ; il est tranché en
faveur du détecteur, et **le défaut est consigné**, pas ignoré. Le refermer demande de rendre le
survol de RmlUi observable autrement qu'en le lui faisant suivre — ce qui n'existe pas dans son
interface aujourd'hui.

⚠ **Conversion de repère — le piège qu'on ne voit pas à l'œil.** `Ctrackball::set_dimensions` et
`mouse_move` attendent des coordonnées **dans le repère de la vue**. Dans `sinaia`, le canvas *est* la
fenêtre de rendu, donc `event.GetX()` convient. Dans `peles`, glfw donne des coordonnées **fenêtre** :
il faut soustraire l'origine du rectangle et appeler `set_dimensions(rect.w, rect.h)`, pas la taille de
la fenêtre. **Oublier cette conversion produit une orbite qui saute et un picking décalé de la largeur
du panneau gauche.** Critère dédié à l'étape 6.

⚠ **La soustraction vit dans UNE fonction, `ui::WindowToView`, et ses deux consommateurs l'appellent**
— l'arbitre d'entrées et `--dump-pick`. Ce n'est pas une préférence de style : c'est ce qui rend le
détecteur éprouvable. Neutraliser cette seule fonction fait passer le critère 1 au rouge (écart mesuré
**0.266** au lieu de 0, `EXIT=1`) ; une soustraction recopiée chez chaque appelant laisserait
l'instrument vert pendant que le produit est faux.

⚠ **CORRECTION DE L'ÉTAPE 6 — la phrase ci-dessus n'était vraie que du terme X.** Dans le document du
produit la vue est collée au bord haut : `rect.y` vaut **0**, donc neutraliser `viewY` ne change
**aucun** résultat et les six critères restent verts. Le détecteur n'éprouvait qu'un terme sur deux.
D'où `assets/layout_probe.rml` et l'option `--document` : une mise en page où la vue est placée
**sous une barre de 140 px**, donc où les deux termes sont non nuls. **Mesuré** : `viewY` neutralisé
laisse `--dump-pick 1400x900 680 360` à **0.000000000, `EXIT=0`** sur le document du produit, et rend
**0.150858477, `EXIT=1`** sur `layout_probe.rml`.

### 1.8 🟡 Possession, et le contrat de destruction

| État | Propriétaire |
|---|---|
| La scène (`VModels`) | `core::Document` — la chaîne de possession **existe déjà** et est correcte : `src/cgmesh/vmodels.h:57-80`, `unique_ptr`, adresses stables `:79` |
| Sélection, survol, répertoire courant | `core::Document` (états de **document**, pas de vue — `sinaia` les met dans le canvas, `wxOpenGLCanvas.h:554-556`) |
| Caméra, cadrage, pivot | `view3d::Camera` (état de **vue**) |
| Identifiants GL | `cgre`, par ses singletons — **subi, pas choisi** (V2) |
| Les DTO | `ui::ViewModel`, **copies**. RmlUi ne voit jamais un `Mesh`, un `Model` ni un `Material` |

⚠ **Contrat de destruction, armé** : `MeshRenderer::RemoveMesh` (`mesh_renderer.h:126`) **doit** être
appelé **avant** la destruction d'un `Mesh`, sinon `m_meshToId` garde une clé pendante et un `Mesh`
réalloué à la même adresse hériterait des tampons GL du précédent. D'où `core::DocumentObserver` :
`Document` **annonce** avant de détruire, `SceneRenderer` **réagit**. Interface déclarée dans `core/`,
implémentée dans `view3d/` — inversion de dépendance, donc pas de cycle.
⚠ **Correction du 2026-09-22, étape 3.** Ce paragraphe affirmait que `RemoveMesh` **ne libère aucun
tampon GL** et que `peles` héritait de la fuite. **C'est faux depuis le troisième lot de
`debt_cgre.md`** (ligne 49, statut **fait**) : `VBOManager::removeMesh (int)` est devenu public et
`MeshRenderer::RemoveMesh` l'appelle — vérifié par lecture, `src/cgre/mesh_renderer.cpp:563-571`.
Le bloc `debt_cgre.md:481-491` qui décrit la fuite est la **section historique** du document, pas son
état courant. L'affirmation portait sur l'existant et n'avait pas été relue.
**Conséquence pour le contrat de destruction : il ne change pas, il se renforce.** `RemoveMesh`
libère maintenant de vrais objets GL, donc il exige en plus un **contexte du groupe de partage
courant** — ce que l'ordre de déclaration des membres d'`Application` garantit, la vue mourant avant
la fenêtre.

### 1.9 🟡 Chargement long : synchrone, avec image de transition

`VMeshesIO::load` est **synchrone** (`wxOpenGLCanvas.cpp:1097-1099`) et peut durer plusieurs secondes
sur un STEP via OCCT. Sur `Command::OpenFile`, `Application` pose le statut, exécute **une image
complète**, **puis** charge.

✅ **Écrit et mesuré à l'étape 6.** `RequestOpen` pose le statut et **diffère** le chargement ;
`FinishPendingOpen` ne court qu'après une image **réellement rendue**. Mesuré par
`--double-click` : la ligne `statut a l'image de transition : Chargement de cube.obj ...` est
imprimée **avant** `modele : cube.obj`, donc avant l'appel à `VMeshesIO::load`.
⚠ **Conséquence à connaître : une image non rendue ne déclenche pas le chargement.** Fenêtre réduite,
ou vue de taille nulle : la demande reste en attente jusqu'à la première image rendue. C'est le
contrat — « une image complète, PUIS le chargement » — et non un oubli.

**Pourquoi pas un fil de travail** : la thread-safety de `cgmesh` est **non examinée**. Introduire un
défaut de concurrence dans le lot dont l'objet est l'apparence serait un mauvais échange.
**Ce que ça coûte, dit franchement** : pendant le chargement la fenêtre ne répond pas et Windows la
grise. **C'est le comportement actuel de `sinaia`** (`wxThread` : 0 occurrence). Pas une régression,
pas un progrès.

### 1.10 🟡 Assets : quatre niveaux de résolution, et le POST_BUILD reste

`ResolveAssetsDir()` essaie dans l'ordre : **(1)** `--assets <dir>` ; **(2)** `PELES_ASSETS_DIR` en
environnement ; **(3)** `<dir de l'exe>/assets` ; **(4)** la macro compilée vers le source.
(3) avant (4) fait qu'une **installation** trouve ses assets même si l'arbre source existe encore.
(1) et (2) laissent pointer la source **sans recompiler** — ce que veut l'itération visuelle, qui est
le motif D-1. `sinaia` s'arrête à (3)+(4) (`CMakeLists.txt:62`, `:135-143`).

**Critère falsifiable** : `peles --assets <répertoire inexistant>` sort **non nul** avec le répertoire
nommé, et **n'ouvre pas** une fenêtre vide. C'est le mode de panne le plus probable du lot ; il doit
être bruyant.

### 1.11 🟡 L'instrument, faute de console

`peles.md` D-4 exige que le premier livrable soit l'instrument. Le lot 1 **est** une interface et ne
contient pas `RemoteConsole` (hors périmètre, et `cgnet` n'est bâti que sous `WIN32`,
`CMakeLists.txt:247-249`). Substitut retenu : **quatre modes de vidage**, `--dump-tree`, `--dump-files`,
`--dump-info`, `--dump-layout`, qui impriment un état sur `stdout` et sortent.

Ils sont **presque gratuits** parce qu'ils tombent de la règle §1.4 : le contenu de chaque panneau est
produit par une **fonction pure** de l'état vers un vecteur de DTO, appelable sans RmlUi. Sans cette
règle, il n'y a pas d'oracle.

⚠ **Bénéfice non cherché** : `--dump-tree`, `--dump-files` et `--dump-info` n'ouvrent **aucun contexte
GL** (ils n'appellent que `peles_core`). **Ils sont donc exécutables en CI Linux.**

⚠ **L'instrument vit dans `app/dump_modes.{h,cpp}`, et pas dans `app/application.cpp`.** Le motif est
mesuré, pas esthétique : à la fin de l'étape 3, l'instrumentation pesait **≈ 277 des 672 lignes
d'`application.cpp`, soit 41 %** — relevé d'état, concordance du viewport, temps par image, lecture de
pixel, vidage de mise en page. Les étapes 4 à 7 lui ajoutent `--dump-tree`, `--dump-files`,
`--dump-info`, `--dump-pick`, le routage d'entrées, l'image de transition et les statuts d'erreur :
`application.cpp` passait **800 lignes dès l'étape 4**, et l'ordre des huit phases s'y serait noyé.
`dump_modes.h` suit la même règle que `gl_state_fence.h` — **aucun type OpenGL dans la déclaration**.

### 1.12 🟠 L'éclairage : **l'oracle ne prescrit pas le produit**

`InitGL` (`sinaia/wxOpenGLCanvas.cpp:1930-1965`) est **réécrit**, pas « non porté » — la formule
importe : `glDisable(GL_CULL_FACE)` et `glEnable(GL_DEPTH_TEST)` (`:1906-1907`) **sont** portés, dans
`SceneRenderer::Draw`. Ce qui est réécrit, c'est **l'éclairage**, et il l'est **derrière un
commutateur**.

| | Éclairage **produit** (défaut) | Éclairage **de mesure** (`--readpixel`) |
|---|---|---|
| Ambiante globale | `0,12` | noire |
| `GL_LIGHT_MODEL_TWO_SIDE` | `GL_TRUE` | `GL_FALSE` |
| Lampe 0 — diffuse / direction | `0,75` / `(-0,4 ; 0,6 ; 1,0)` | blanche / `(0 ; 0 ; 1)` |
| Lampe 0 — spéculaire | **blanche** | noire |
| Lampe 1 (appoint) | `0,28` / `(0,7 ; 0,25 ; 0,4)`, allumée | noire, éteinte |

⚠ **Sans ce commutateur, le critère 4 ne *constate* pas l'écart d'éclairage : il le *prescrit*.** Il
compare `peles` à `peles_probe`, or les deux partageaient exactement la déviation qu'il devrait
détecter. La valeur prédite `(26,128,204)` n'est atteignable **que** sous l'éclairage de mesure : toute
restauration de fidélité à `InitGL` rendait le critère rouge. L'oracle verrouillait le produit.

**L'argument « sinon le critère 4 est inécrivable » ne tenait pas, et le code le prouvait** :
`Application::CreateSceneAndDocument` posait **déjà** une pose de mesure sous `--readpixel` —
`SetFrontalRotation()` **et** `SetFlatShading(true)`. L'éclairage appartient au même commutateur, et il
y est. **Le défaut est l'éclairage produit** ; l'inverse ferait de la mesure le comportement normal.

⚠ **`GL_LIGHT_MODEL_TWO_SIDE` est inerte sur le chemin nominal** — `src/cgre/surface_program.cpp:142-148`
retourne `N` lui-même quand `!gl_FrontFacing`, **dans le fragment** — **mais il mord dès
`SetSurfaceShaderEnabled(false)`**, c'est-à-dire exactement sur la branche de repli que la sonde prévoit
si P7 échoue. C'est le cas que le commentaire d'`InitGL:1940-1944` documente : OBJ à enroulements
incohérents → faces quasi noires.

**Ce que le commutateur ne couvre pas, et pourquoi** : `glEnable(GL_COLOR_MATERIAL)` d'`InitGL:1973-1974`
n'est porté dans **aucune** des deux branches. `GL_COLOR_MATERIAL` n'existe plus sous programme lié
(`surface_program.cpp:152-157` : la diffuse vient de `gl_FrontMaterial` ou de la couleur de sommet,
jamais de `glColor`), et `MaterialRenderer` le coupe à chaque maillage. Le rallumer ne changerait rien
sur le chemin nominal et changerait la couleur sur le chemin de repli.

---

## 2. Les 7 étapes

| # | Contenu | Critères d'acceptation | Ce qui casse si elle échoue |
|---|---|---|---|
| **1** | **La sonde `peles_probe`** : glfw, deux variantes de contexte, glad, cube en mémoire, `MeshRenderer`, `Ctrackball`, `glReadPixels`. **Aucun RmlUi, aucun asset, aucun fichier lu** | Les 8 assertions P1…P8 (§3), code de sortie 0. Le rapport est **archivé** : c'est la mesure de référence du profil GL | **Tout.** Voir la table de branches §4 |
| **2** | **L'hôte RmlUi** : `FetchRmlUi.cmake`, `platform/`, les 8 phases, `GlStateFence`, `asset_paths`, un `.rml` minimal, `Rml::Debugger`. La vue dessine le **cube de la sonde**, pas un fichier | (a) `peles --assets <inexistant>` sort non nul en nommant le répertoire ; (b) `peles --state-audit` rend `AUDIT OK` **et** `auditedFrames > 0` : tous les états relevés — décompte **dérivé**, pas écrit — identiques de part et d'autre des **deux** phases sous clôture (5 et 7), sur des images dont les **huit** phases ont couru ; (c) `--dump-timing` sur 120 images **effectivement rendues**, médiane < 16,6 ms ⚠ **seuil = estimation**, rôle = détecter un ordre de grandeur aberrant, pas qualifier la performance. ⚠ **(a), (b) et (c) sont falsifiables, et chacun a été vu rouge** : identifiant `#viewport` cassé ⇒ `EXIT=3` ; clôture affaiblie ⇒ `AUDIT ECHOUE` ; vue de taille nulle ⇒ `AUDIT NON CONCLUANT` + `EXIT=1` | R-P6 ou R-1 mordent → repli vendoring, retour utilisateur sur D-7. R-P3 mord → escalade §1.2 |
| **3** | **La vue 3D dans son rectangle** : `viewport_rect`, portage de `DrawGL`, `camera`, les 4 zones **vides**, `--dump-layout`. Fichier passé en ligne de commande | Les 3 critères §1.3 (rectangle exact, variation ±1 px, `GL_VIEWPORT` concordant) + `--readpixel` rend la **même valeur que la sonde** sur le même cube ⇒ le portage de `DrawGL` n'a rien changé | §1.3 tombe → repli sur le calcul manuel à la LichtFeld, **géométrie dupliquée en C++**, et le 3ᵉ critère devient inécrivable. Dégradation de conception, pas arrêt |
| **4** | **Arbre + liste, ensemble** (elles partagent représentation plate, plafond, tri, catalogue) + catalogue détypé + **test de cohérence** + `--dump-tree`/`--dump-files` | (1) `--dump-tree` identique octet pour octet sur deux exécutions ; (2) identique au fichier attendu versionné ; (3) `--dump-files` liste exactement les extensions du catalogue — la fixture contient un `.mtl` et un `.txt` qui ne doivent **pas** apparaître ; (4) `tu_supported_formats` passe ; (5) **mesure de R-4/R-5** à 100/1000/5000 lignes — **consignée quel qu'en soit le résultat** | Si (5) montre que 1000 lignes ne tiennent pas, le plafond descend et une pagination apparaît : un lot de plus, pas un arrêt |
| **5** | **Les panneaux d'informations** : `model_info`, `topology_stats`, table 2 colonnes, arbres aplatis, pastilles, `--dump-info` | (1) exactement **14 lignes**, dans l'ordre de `SinaiaFrame.cpp:2187-2201` + `Path`, **même sans fichier chargé** ; (2) sur 3 fichiers de référence, les valeurs numériques **identiques** à `sinaia`, relevées une fois et consignées ; (3) format des nombres identique (`%.4g`, `:2239, :2242`) — ce qui rend (2) comparable **textuellement** ; (4) sur ≥ 50 matériaux, **la mémoire ne croît pas** sur trois sélections du même fichier | Si `data-style-background-color` ne marche pas comme documenté : repli sur une classe RCSS par couleur quantifiée (grille 4×4×4). Pas un arrêt |
| **6** | **L'interaction** : `input_router`, `picking`, câblage complet, image de transition | (1) ⚠ **la conversion de repère** — `--dump-pick` : le rayon du pixel central du rectangle est parallèle à l'axe de vue (produit vectoriel < 1e-5). **Rien d'autre dans le lot ne détecterait un décalage de la largeur du panneau gauche** ; (2) `set_dimensions(rect.w, rect.h)` vérifié sur deux tailles de fenêtre donnant le même rayon ; (3) séquence scriptée `down@viewport → move@panneau → up@panneau` rend `Viewport` **pour les trois** ; (4) `Alt+clic` sans surface laisse le pivot inchangé **au bit près** ; (5) ⚠ **le défilement des panneaux de droite, ajouté par la revue de l'étape 5** : sur `many_materials.obj`, dont l'arbre des matériaux mesure **4008 px dans 720**, la **dernière** ligne doit être atteignable — barre **et** routage de la molette, les deux, une barre sans molette ne servant à rien ; (6) ⚠ **`LoadModel` ne sélectionne que si rien n'est sélectionné** (`core/loader.cpp:70-71`) : bénin tant qu'un seul fichier est ouvert, mais dès que l'étape 6 permet d'en ouvrir un second, les panneaux décriraient **le premier**. Le critère : ouvrir A puis B, la ligne `File` doit dire **B** | (3) est le seul qui peut forcer un changement de conception : si `GetHoverElement()` ne se comporte pas comme documenté, repli sur le test de rectangle manuel, avec la duplication que §1.3 refuse |
| **7** | **Finitions** : erreurs de chargement au statut, plafonds et « … N entrées non affichées », icône de fenêtre, `Rml::Shutdown` **avant** la destruction des interfaces (piège documenté, `peles.md` §2.2) | `peles --frames 3 <fichier illisible>` sort **avec 0** (ce n'est pas une panne applicative) et le statut nomme le fichier et la raison ; sous ASan, 60 images et deux ouvertures **sans erreur** — ⚠ **sauf celles de `cgre`, qui sont connues, héritées, et doivent être listées nommément AVANT**, pas découvertes | — |

⚠ **Le découpage est déséquilibré** : l'étape 4 est la plus grosse et porte seule la levée de R-4 et
R-5. Si elle doit être scindée, la ligne de coupe est **« arbre + liste » / « détypage + test »** —
surtout **pas** « arbre » / « liste », qui partagent tout leur mécanisme.

---

## 3. Les 8 assertions de la sonde

| # | Assertion | Instrument | Ce qui l'infirme |
|---|---|---|---|
| P1 | glfw ouvre fenêtre + contexte **sans aucun hint de version** | `glfwCreateWindow != nullptr` | nullptr, ou `glfwGetError` non nul |
| P2 | Ce contexte est de **compatibilité** | `glGetString(GL_VERSION)` + `GL_CONTEXT_PROFILE_MASK` si ≥ 3.2, sinon `GL_ARB_compatibility` | masque = `GL_CONTEXT_CORE_PROFILE_BIT` |
| P3 | Idem avec hints **explicites 3.3 + `COMPAT_PROFILE`** (2ᵉ fenêtre, même binaire) | idem | échec de création, ou masque core |
| P4 | `gladLoaderLoadGL()` ≥ 1.5 **et** `glad_glBegin`, `glad_glPushAttrib`, `glad_glVertexPointer`, `glad_glEnableClientState` non nuls | **test de pointeur, avant tout appel** | l'un est nul ⇒ le chemin fixe de `cgre` est inatteignable |
| P5 | Un `glBegin`/`glVertex3f`/`glEnd` passe | `glGetError()` juste après `glEnd` | `GL_INVALID_OPERATION` |
| P6 | **`cgre` dessine** un cube généré en mémoire | `glReadPixels` d'**un** pixel central + **un** pixel de coin | l'un hors tolérance |
| P7 | `cgre::SurfaceProgram()` non nul | pointeur + journal via `cgre::SetLogSink` (`diagnostics.h:37-39`) | nullptr ⇒ le GLSL `#version 120` ne compile pas |
| P8 | glfw délivre les événements souris **hors fenêtre, bouton enfoncé** (R-6) | rappels instrumentés, mode `--probe-drag` | aucun `cursor_pos` reçu hors cadre |

⚠ **P8 exige une main humaine** : il est derrière `--probe-drag` et **n'entre pas** dans le code de
sortie automatique, qui porte sur P1…P7. Sinon la sonde ne serait pas exécutable sans opérateur.

**Pourquoi un pixel et non une image** : `peles.md` §4 V5 — l'oracle image n'existe plus
(`5a87adb`, 2026-09-17) et n'était **pas reproductible** (jusqu'à 24 621 octets d'écart entre deux
exécutions du même binaire). Un pixel unique, face à plat, lumière fixe, sans anticrénelage, est
déterministe.

⚠ **La tolérance ±2/255 est une estimation, pas une mesure.** Repli déclaré : « le pixel central diffère
du pixel de coin d'au moins 32/255 sur au moins un canal ». Les deux valeurs brutes sont imprimées, pour
que la tolérance définitive se calibre sur des nombres réels.

**La sonde est bornée dans le temps** : au-delà de **3 jours** sans verdict clair, elle est déclarée non
concluante et le lot s'arrête. **Une sonde sans verdict n'est pas une sonde.**

---

## 4. Branches d'échec de la sonde — écrites avant de la lancer

| Échec | Diagnostic | Sortie |
|---|---|---|
| **P2 et P3 échouent** | le pilote ne délivre pas de profil de compatibilité | ⛔ **Arrêt du lot.** D-6 se rouvre : (b) `cgre2`+Vulkan ou (c) renoncer. Le verdict « figer cgre » de `debt_cgre.md` §5 est **confirmé par la mesure** au lieu d'être révisé |
| **P2 échoue, P3 passe** | le contexte par défaut est core sur ce pilote | ✅ Pas d'arrêt : l'hôte demande explicitement 3.3 + `COMPAT_PROFILE`, et fige ce choix avec la mesure en commentaire |
| **P4 ou P5 échoue, P2/P3 passés** | incohérence glad / pilote | Investigation `debugger` avant toute suite. Pas un arrêt de lot |
| **P6 échoue, P7 passe** | `cgre` dessine sous wx mais pas sous glfw | ⛔ **Arrêt temporaire.** Isoler la différence (format de pixel, `GLFW_DEPTH_BITS`). Ne pas passer à l'étape 2 |
| **P7 échoue seul** | le shader ne compile pas | Pas d'arrêt : `cgre::SetSurfaceShaderEnabled(false)` (`surface_program.h:184`) retombe sur le pipeline fixe. À consigner comme régression d'apparence assumée |

---

## 5. Réserves propres à l'exécution

| # | Réserve | Sens du biais | Condition de levée |
|---|---|---|---|
| **R-P1** | Tolérance ±2/255 de P6 | majorant | Première exécution sur les deux pilotes cibles |
| **R-P2** | **Le substitut `--dump-*` ne couvre PAS la parité caméra** avec `sinaia` (`camera project`, `camera show`) | **inconnu** | L0b — la console — dans un lot ultérieur. ⚠ **À savoir en entrant dans le lot, pas en sortant** |
| **R-P3** | `RmlUi_Renderer_GL2` modifie-t-il un état que `glPushAttrib` ne couvre pas ? | majorant du coût de l'étape 2 | ✅ **Levée le 2026-09-22, après correction de l'instrument.** Oui : les **trois matrices** (`BeginFrame` écrit `GL_PROJECTION` puis `glLoadIdentity()` sur `GL_TEXTURE` et `GL_MODELVIEW`). Escalade niveau (1) appliquée — elles sont empilées par la clôture **et** relevées. ⚠ La levée initiale était **prématurée** : le relevé ne contenait aucun état matriciel, la matrice de texture avait été trouvée **en lisant** `BeginFrame`, pas par l'instrument. Le relevé couvre désormais `GL_MATRIX_MODE`, les trois matrices (bit à bit), `GL_STENCIL_TEST` — **le seul état que RmlUi laisse franchement allumé derrière lui**, `EndFrame` étant vide — `GL_STENCIL_WRITEMASK`, `GL_ACTIVE_TEXTURE` et jusqu'à 8 unités de texture. Aucune copie de `RmlUi_Renderer_GL2.{cpp,h}` n'a été faite |
| **R-P4** | Plafonds `kMaxChildrenPerNode` et `kMaxFiles` — **estimations, pas mesures** | inconnu | ✅ **Levée le 2026-09-22, corrections de revue de l'étape 4.** Les deux sont **mesurés** et les trois valeurs ont changé : `kMaxChildrenPerNode` **2000 → 1000**, `kMaxFiles` **5000 → 1000**, et un plafond **global** `kMaxVisibleRows = 1000` a été **ajouté** — le plafond par nœud ne bornait pas la grandeur que la mesure fait varier. Instrument et chiffres en §6 |
| **R-P5** | Thread-safety de `cgmesh` et de ses importeurs : **non examinée** | majorant du coût d'un lot ultérieur | Audit des états globaux avant tout chargement asynchrone |
| **R-P6** | `FetchContent` sur RmlUi : **RmlUi ne documente pas cette voie** (0 occurrence dans sa doc de build). Son `CMakeLists.txt:13-15` définit `RMLUI_IS_ROOT_PROJECT`, donc l'inclusion en sous-répertoire est **prévue** — mais c'est **inféré** | inconnu | L'essai réel, étape 2 |
| **R-P7** | RmlUi appelle `find_package(Freetype)`, et une cible apportée par `FetchContent` **n'est pas trouvée** par `find_package` en mode MODULE | majorant du coût de l'étape 2 | ✅ **Levée le 2026-09-22 — par un mécanisme autre que le repli écrit d'avance.** Voir §6, ligne R-P7 |
| **R-P8** | ⚠ `ENABLE_PELES=ON` **rompt la propriété « configuration hors ligne »** que le dépôt tient ailleurs. Conséquence directe de D-7 | — | ✅ **Écrite dans `peles/CMakeLists.txt` (étape 2) et le preset existe depuis les corrections de revue de l'étape 4** : `ci-linux-peles`, hérité de `ci-linux`, `ENABLE_PELES=ON`, condition `hostSystemName == Linux`, plus le job du même nom dans `.github/workflows/build.yml`. ⚠ **Sa description dit ce qu'il ne prouve pas** : aucun rendu (pas de contexte GL sur un runner), et **pas** `tu_supported_formats`, qui exige wxWidgets. La réserve reste vraie — elle est désormais **isolée dans un job séparé**, dont la panne la plus probable est une panne de réseau |

**Ce que le lot 1 ne referme pas** : `peles` est la **quatrième** couche application pour un seul
`cgmesh` (`peles.md` §8), et il duplique nommément trois des six fonctions déjà triplées — chargement,
cadrage/trackball, picking. **La seule atténuation exécutable sans toucher à `sinaia`** est le catalogue
de formats détypé, accompagné d'un **test de cohérence** qui fait échouer la CI si les deux ensembles
d'extensions divergent. Ce n'est pas une préférence esthétique : c'est la seule atténuation de la
réserve que `camera_refacto.md` §2.1 a **déjà mesurée** (« deux corrections perdues, personne ne s'en
est aperçu »).

⚠ **Le catalogue détypé vit dans `peles/core/file_catalog.{h,cpp}`, et NON dans `src/cgmesh/` comme
ce paragraphe le prévoyait.** Motif de l'écart, décidé à l'étape 4 : **ajouter un fichier à une
bibliothèque partagée par quatre applications n'est pas une décision de ce lot.** `cgmesh` est lié par
`sinaia`, `sulina`, `vecna` et `maker` ; y poser un catalogue de formats engage leurs quatre listes de
formats et leur surface d'API, alors que rien dans le lot 1 ne mesure cet effet. Le poser dans
`peles/core/` n'ôte rien à l'atténuation, **parce que l'atténuation est le test, pas l'emplacement** :
`test/tu_supported_formats.cpp` compare les deux ensembles sous la même configuration de macros quel
que soit le répertoire d'où vient le catalogue. La remontée vers `cgmesh` reste ouverte, et elle aura
alors **un test déjà écrit** pour la couvrir.

⚠ **MAIS CE TEST NE TOURNE DANS AUCUNE CONFIGURATION OUTILLÉE, ET LE DIRE AUTREMENT SERAIT FAUX.**
Il exige `ENABLE_SINAIA=ON` **et** `ENABLE_PELES=ON` — le premier pour que `sinaia/SupportedFormats.h`
soit incluable, donc pour wxWidgets. `ci-linux` a `ENABLE_SINAIA=OFF` ; `ci-linux-peles` n'installe
pas wxWidgets sur le runner. **Mesuré** : `grep -c ENABLE_PELES CMakePresets.json
CMakeUserPresets.json` rendait `0` et `0` avant les corrections de revue de l'étape 4, et rend
`1`/`0` après — le preset existe, il n'active pas `ENABLE_SINAIA` pour autant. **La seule voie
d'exécution de ce test est une construction locale sous les deux options.**

Deux conséquences, traitées :

- **L'absence du test produit désormais un signal.** Hors de sa configuration, le fichier ne compile
  plus « à rien » : un `#else` rend un `GTEST_SKIP()` qui **nomme** la configuration manquante.
  Auparavant `ctest` était vert et rien ne distinguait « les deux catalogues concordent » de
  « personne ne les a comparés ».
- **La limite de couverture est dans le test, pas seulement dans un commentaire.** Le premier test
  **imprime les gardes ouvertes** de la construction courante. Mesuré sur la machine de référence :
  `gardes ouvertes dans cette construction : CG_HAS_ZLIB` — c'est-à-dire qu'une divergence portant sur
  `CG_HAS_OPENNURBS`, `CG_HAS_OCCT` ou `CG_HAS_LIB3MF` y est invisible **par construction**.

**Ce qui est sans objet, vérifié** : le défaut n°1 de `debt_sinaia.md` (`SceneSignature` non résistante
au remplacement de scène) **n'est pas hérité**. `m_matLevelSignature` n'a que trois sites
(`wxOpenGLCanvas.cpp:882, :884`, `.h:496`) et `UpdateCuttingMatLevel` n'a que deux appelants — `:1416`
(bloc tapis de coupe de `DrawGL`) et `GetCuttingMatLevel:2148-2152`. **Les deux sont du tapis de coupe,
hors lot 1.** D-9 ne se pose donc pas ici.

⚠ **L'ÉTAT FINAL DES HUIT RÉSERVES CI-DESSUS, PLUS R-4, R-5, R-6 ET D-5, EST EN §8.2** — le lot se
ferme à l'étape 7, et un état dispersé sur quatre sections ne se lit pas. Le tableau de cette
section reste celui de leur **énoncé** ; §8.2 porte leur **verdict**.

---

## 6. Journal de prédictions

Posées **avant** exécution. Résultat, écart et cause se remplissent à la fin de chaque étape.
**Liste fermée des causes** : 1 affirmation sur l'existant non vérifiée · 2 instrument biaisé ·
3 hypothèse d'environnement non vérifiée · 4 structure conçue sans charge · 5 critère non falsifiable ·
6 excès de pessimisme · 7 mécanisme de vérification neutralisé par le code.

| Étape | Prédiction | Instrument | Résultat | Écart | Cause |
|---|---|---|---|---|---|
| 1 | Le contexte glfw par défaut (sans hint) est de **compatibilité** sur la machine Windows de référence | `glGetIntegerv(GL_CONTEXT_PROFILE_MASK)` | ✅ **Vérifiée.** `GL_VERSION = "4.6.0 - Build 32.0.101.6078"` (Intel), masque `0x2` = COMPATIBILITY. Avec hints explicites 3.3 + `COMPAT_PROFILE` (P3) : masque `0x2` également | aucun | — |
| 1 | `cgre` dessine sous glfw **sans modification de `src/cgre/*`** | `glReadPixels`, 1 pixel, ±2/255 | ✅ **Vérifiée.** centre (256,256) = **(25,128,204)** ; coin (2,2) = (0,0,0) ; prédiction **(26,128,204)** ; contraste max 204. `src/cgre/*` non modifié | **1/255** sur un canal, sous la tolérance | — (règle de conversion du pilote ; ⚗ ci-dessous, le tramage est INFIRMÉ) |
| 1 | glfw délivre les événements souris hors fenêtre, bouton enfoncé (R-6) | rappels instrumentés, `--probe-drag` | ⏸ **NON MESURÉE.** Le chemin a été exercé jusqu'à son délai de 60 s sans opérateur : il ne plante pas, mais **personne n'a fait le geste**. Rendu `SKIP`, pas `PASS` | — | — |

**Verdict de l'étape 1, mesuré le 2026-09-22** : **le verrou V1 de `peles.md` est levé sur cette
machine.** 7/7 assertions P1…P7 passent, code de sortie 0, et **deux exécutions consécutives rendent une
sortie identique au caractère près** — la propriété que l'oracle image n'avait pas.

⚠ **Trois réserves que ce verdict ne lève pas** :
- **R-P1 n'est pas levée.** Une exécution, **un** pilote, n'est pas une calibration. La tolérance
  ±2/255 reste déclarée ESTIMÉE dans le code et dans le rapport, et les valeurs brutes sortent à chaque
  exécution. Condition de levée inchangée : un second pilote.
- **R-6 reste ouverte.** P8 est un `SKIP` et non un `PASS`. ⚠ La distinction est portée par le code :
  un appui reçu **sans aucun** `cursor_pos` hors cadre est un `FAIL` franc ; **aucun appui du tout** est
  un `SKIP`, qui ne compte ni pour ni contre. Confondre les deux ferait passer une absence d'opérateur
  pour un défaut de plateforme.
- **L'idempotence de `cg_add_vendored_glfw()` n'est établie que dans un ordre.** `ENABLE_CGGRAPH_BOILERPLATE`
  seul, puis les deux options ensemble : configurés **et bâtis**, un seul `glfw3.lib`. L'ordre inverse
  n'est pas atteignable avec le `CMakeLists.txt` racine actuel — le garde `if (TARGET glfw) return()` le
  couvre, mais **aucune exécution ne l'établit**.

**Le détecteur de P6 a été validé sur un cas négatif** (règle « valider le détecteur » de
`global-context`) : `Draw(meshId)` commenté → `P6 FAIL`, repli sur le contraste également infirmé
(contraste 0), `EXIT=1`. Fichier restauré et re-vérifié vert. **P6 n'est donc pas une assertion qui
passerait sans rien mesurer.**

### Corrections de revue de l'étape 1, mesurées le 2026-09-22

**1. La sonde ne rendait aucun verdict dans la branche qu'elle prétend diagnostiquer.**
`gladLoadGLUserPtr` abandonne avant tout chargeur par version si `glGetString` est nul
(`src/cgre/glad/gl.c:2371-2373`) : un chargement échoué laisse **tous** les pointeurs nuls. P5
appliquait la règle « test de pointeur, jamais d'appel » ; P2, P6 et P7 ne l'appliquaient pas.
**Mesuré** : avec `gladLoaderLoadGL()` neutralisé et les gardes retirées, le processus rend
`Segmentation fault`, `EXIT=139`, juste après P4 — **aucun** des quatre codes de sortie du contrat
d'en-tête. Avec les gardes : `P4/P2/P5/P6/P7 FAIL`, `P1/P3 PASS`, `RESULTAT : 2/7`, **`EXIT=1`**.
Le retour des deux `gladLoaderLoadGL()` de P3 est désormais contrôlé. Fichier restauré, re-vérifié
vert, sortie identique au caractère près à celle d'avant correction.

**2. `GL_DITHER` — hypothèse INFIRMÉE par la mesure.** `GL_DITHER` est actif par défaut et la
spécification autorise l'implémentation à modifier le **LSB** de la couleur écrite selon la position
du pixel : c'était le confondant le plus économique pour l'écart R = 25 au lieu de 26. `glDisable
(GL_DITHER)` ajouté → **le pixel central reste (25,128,204), écart max 1, inchangé**. Le tramage
n'était donc pas la cause. La ligne est conservée : elle retire un confondant réel de l'oracle, même
si elle n'explique pas cet écart-ci.

⚗ **Hypothèse de remplacement, à confirmer sur un second pilote.** La diffuse par défaut est
exactement `{0.1f, 0.5f, 0.8f}` (`src/cgre/material_renderer.cpp:766`). Une conversion
`round(f × 255)` donnerait (26,128,204) ; une conversion `min(255, floor(f × 256))` donne
(25,128,204) — **les trois canaux à la fois**. L'écart s'expliquerait donc par la règle de conversion
flottant → unorm du pilote, et non par un défaut de rendu. Ce qui la confirmerait : lire le pixel sur
deux ou trois couleurs de matériau supplémentaires et vérifier que `floor(f × 256)` prédit les trois
canaux à chaque fois. **R-P1 reste ouverte** ; la tolérance ±2/255 reste ESTIMÉE.

**3. Le préalable « surface de dessin exploitable » de P6 rend désormais `SKIP`, plus `FAIL`.** Une
fenêtre réduite ou une session distante déconnectée est étrangère au profil GL, or §4 fait de « P6
échoue, P7 passe » un ⛔ arrêt de lot : le texte nommait la bonne cause, le verdict la confondait.
⚠ **La réciproque est codée explicitement** : une entrée GL absente **est** un défaut de plateforme,
donc un `FAIL`. `SKIP` est réservé à ce qui manque **hors** de la plateforme (opérateur, surface).

**4. `main` passe de 269 à 47 lignes** (38 hors commentaires), une fonction par assertion. Vérifié par
`diff` : sortie **identique au caractère près**, `EXIT=0`, et deux exécutions consécutives identiques.
⚠ **Résidu déclaré** : `MeasureP6` fait 149 lignes (93 hors commentaires) et reste au-dessus du seuil
de 50 de la checklist C++. Il n'a pas été découpé — c'est le modèle qu'on vient de généraliser.

⚠ **Point de configuration à connaître** : `EXTERN_GOOGLETEST` est **obligatoire sur Windows** et sans
rapport avec `peles` — `add_subdirectory(test)` est inconditionnel dans le `CMakeLists.txt` racine, et
`test/CMakeLists.txt:159-166` retombe sur `find_package(GTest REQUIRED)` quand la variable est vide.
Sans elle, la configuration échoue **avant** d'atteindre `peles`.
| 2 | `FetchContent` sur RmlUi 6.3 **configure sans patch** (R-P6) | `cmake -B build -S .` | ✅ **Vérifiée.** `-- Configuring done (23.3s)`, aucun patch, aucun fichier amont modifié. Trois options suffisent : `RMLUI_SAMPLES=OFF`, `BUILD_SHARED_LIBS=OFF` (variable normale, CMP0077 est NEW chez RmlUi), `RMLUI_FONT_ENGINE=freetype` | aucun | — |
| 2 | FreeType par `FetchContent` est **trouvé** par le `find_package` de RmlUi, sans le repli (R-P7) | idem | ❌ **INFIRMÉE**, et **le repli écrit d'avance ne marchait pas non plus.** Mesuré : `-- Could NOT find Freetype (missing: FREETYPE_LIBRARY FREETYPE_INCLUDE_DIRS)`. Ce qui sauve la configuration est autre chose : RmlUi vérifie l'**existence de la cible** `Freetype::Freetype` (`CMake/Utilities.cmake:68-71`), pas le résultat du `find_package` — il déclare ses dépendances « souples » **pour que l'hôte les fournisse autrement**. Un `add_library(Freetype::Freetype ALIAS freetype)` posé avant `FetchContent_MakeAvailable(rmlui)` suffit : `-- Found Freetype::Freetype - Freetype font engine enabled` | **double** : la prédiction ET son repli | **1** — `CMakeLists.txt` de FreeType 2.13.3 `:456` crée la cible `freetype` et `:516-518` une interface dont l'`EXPORT_NAME` vaut `Freetype::Freetype`. Un `EXPORT_NAME` ne produit un nom qu'à l'**installation** : la cible n'existe pas dans l'arbre de construction. Et `FindFreetype` appelle `find_library`, qui exige que le `.lib` **existe déjà** — impossible pour une bibliothèque bâtie dans l'arbre. Les deux affirmations portaient sur l'existant et n'avaient pas été lues |
| 2 | `glPushAttrib`/`glPopAttrib` suffisent, **plus les matrices empilées à la main** : tous les états relevés identiques de part et d'autre des deux phases sous clôture (R-P3) | `peles --state-audit` | ✅ **Vérifiée — mais seulement après correction de l'instrument.** État final : `AUDIT OK : les 25 etats releves sont identiques avant et apres, sur les 3 image(s) rendue(s) et sur les deux phases`, `EXIT=0`. Tenu **aussi** avec `--debugger`, donc avec de la géométrie texturée réellement émise par `Context::Render()`. **Le code lui-même n'a été pris en défaut sur aucun état** : la clôture tenait déjà les cinq familles ajoutées. ⚠ **La première levée était portée par un instrument aveugle** : le relevé ne contenait ni matrice, ni `GL_STENCIL_TEST`, ni les unités de texture au-delà de 2, il n'encadrait qu'**une** des deux phases, et `GL_CURRENT_PROGRAM` ne pouvait structurellement pas diverger. **Aucune copie de `RmlUi_Renderer_GL2.{cpp,h}` n'a été faite** | **l'instrument, pas le code** | **2** — instrument biaisé, et **7** pour `GL_CURRENT_PROGRAM` : mécanisme de vérification neutralisé par le code |
| 2 | Aucune DLL nouvelle à côté de `peles.exe` (statique partout) | `ls` du répertoire de sortie | ✅ **Vérifiée.** Le répertoire de sortie ne contient que `peles.exe`, `peles_probe.exe`, les quatre `.lib` du pare-feu et `assets/`. `rmlui.lib` et `rmlui_debugger.lib` sont statiques | aucun | — |
| 2 | Le pare-feu d'inclusion est tenu par **le compilateur** et non par un relecteur | 4 inclusions fautives injectées, une par cible | ✅ **Vérifiée sur son cas négatif.** `gl_wrapper.h` dans `core/`, `platform/`, `ui/` et `<RmlUi/Core.h>` dans `view3d/` : **les quatre** rendent `error C1083 ... No such file or directory`. Fichiers restaurés, reconstruction verte | aucun | — |
| 3 | Le rectangle lu de `#viewport` est **exact au pixel** après `Context::Update()` | `--dump-layout` vs valeur RCSS calculée | ✅ **Vérifiée.** `--dump-layout 1400x900` rend `x=240 y=0 w=880 h=720`, soit **exactement** la valeur calculée à la main depuis la feuille : `1400 − 240 − 280 = 880`, `900 − 180 = 720`. `1401x900` rend `w=881`, `1400x901` rend `h=721` — **+1 exactement**, sur l'axe touché et lui seul | aucun | — |
| 3 | Le portage de `DrawGL` ne change **aucun** pixel par rapport à la sonde | `--readpixel` | ✅ **Vérifiée.** `peles --frames 1 --readpixel 620 490 test/data/obj/cube.obj` rend **(25,128,204)**, soit la valeur de `peles_probe` **au caractère près**, écart 1/255 de la valeur prédite (26,128,204). ⚠ **Sous réserve nommée par l'instrument lui-même** : `--readpixel` **épingle la pose de mesure** — rotation frontale et ombrage plat — sans quoi la prédiction ne s'applique à aucun pixel. Le reste du chemin est nominal : loi de cadrage, `set_camera`, boucle sur `VModels`, `MeshRenderer` | aucun | — |
| 4 | Un `data-for` de **1000 lignes** tient sous 16,6 ms d'`Update` (R-4) | `--dump-timing` | ✅ **Vérifiée, et sur les deux régimes.** Construction (1ʳᵉ image, modèle sali) : **7,4 / 8,9 / 9,4 ms** sur trois exécutions. Régime établi, rien de sali : **0,060 à 0,066 ms**. Avec un `Toggle` à **chaque** image (`--churn-tree 100`) : médiane d'`Update` **5,37 ms**, médiane d'image **7,09 ms**. ⚠ **À 5002 lignes, elle tombe** : construction **38,0 / 40,9 / 41,0 ms**, et `Toggle` à chaque image **29,9 ms** de médiane | aucun sur 1000 lignes | — |
| 4 | Rétrécir le vecteur lié **ne provoque pas** le comportement indéfini de §1.5 | `Toggle` ×100 sous ASan | ⏸ **PARTIELLEMENT mesurée, et l'écart est déclaré.** Le rétrécissement **a bien été exercé** : `--churn-tree 100` fait passer le vecteur lié de **1000 à 1 ligne et retour, cent fois**, pendant que RmlUi continue de dessiner — aucun plantage, `EXIT=0`, mêmes temps d'une exécution à l'autre. ⚠ **Mais PAS sous ASan** : la construction instrumentée n'a pas été faite, donc une écriture hors bornes non fatale ne serait pas vue. Ce qui est établi : le vecteur source change de taille sans que personne n'appelle `RemoveChild` — ce que §1.5 distingue | **l'instrument**, pas le résultat | 2 — instrument plus faible que celui qui était prévu |
| 4 | Le détypage du catalogue est **à ensemble constant** | `tu_supported_formats` | ✅ **Vérifiée, exécutée.** 2 tests, `PASSED`, sous `ENABLE_SINAIA=ON` + `ENABLE_PELES=ON` + `EXTERN_WXWIDGETS`. ⚠ **Portée réelle : les macros de LA configuration bâtie.** Celle-ci avait `CG_HAS_ZLIB` seule active — une divergence qui vivrait sous `CG_HAS_OCCT` y resterait invisible **par construction**, et ne se verrait que dans une configuration où la garde est ouverte | aucun | — |
| 5 | Une seule racine de binding suffit (R-5) | `--dump-timing` sur un fichier à 98 matériaux | | | |
| 5 | `data-style-background-color` remplace les 25 l. de GDI de `MakeColorSwatch` | absence de croissance mémoire sur 3 sélections | | | |
| 6 | `Context::GetHoverElement()` suffit à l'arbitrage, **sans** test de rectangle manuel | scénario d'événements scripté | ✅ **Vérifiée.** `--input-script` rend `#viewport` sur le point de la vue et `#panel-left` sur celui du panneau, **lus de `GetHoverElement()`**, et les sept étapes ont le propriétaire attendu. Aucun test de rectangle manuel n'a été écrit : la seule géométrie que le C++ lit est le rectangle de `#viewport`, et il sert à **convertir** les coordonnées, pas à décider du propriétaire. ⚠ **Une adaptation, pas un écart** : l'arbitre remonte la chaîne de parents au lieu de comparer l'identifiant du seul élément rendu — RmlUi rend le descendant **le plus jeune** sous le curseur, et `#viewport` recevra un contenu aux étapes suivantes | aucun | — |
| 6 | La conversion fenêtre→vue est correcte **du premier coup** | `--dump-pick`, produit vectoriel < 1e-5 | ✅ **Vérifiée.** `|rayon central × axe de vue| = 0.000000000` sur **trois** tailles de fenêtre (1400×900, 1000×800, 1920×1080), pour trois rectangles et trois `set_dimensions` différents, et le rayon central est **identique aux trois** : `(-0.813798, 0.469846, -0.342020)`. Aucune correction n'a été nécessaire après la première exécution | aucun | — |
| 7 | Un fichier illisible **ne fait pas sortir en erreur** et son statut nomme le fichier et la raison | `--dump-status` | ✅ **Vérifiée.** `EXIT=0`, `statut a l'ecran : "<chemin> : format inconnu, fichier illisible ou sans geometrie"`. ⚠ **L'instrument est neuf** : le statut est une chaîne du modèle lié, donc invisible de la console — sans lui, le critère ne se vérifiait qu'à l'œil | aucun | — |
| 7 | **Sous ASan, 60 images ouvrant deux fichiers ne rapportent aucune erreur** | `build/peles-asan`, `--frames 60 --open-second` | ✅ **Vérifiée.** **0 ligne `AddressSanitizer`**, `EXIT=0`. Étendue à sept autres exécutions, toutes à 0 : churn de **1001 lignes** ×100, `--reselect`, `--state-audit`, `--readpixel`, `--reopen`, `--focus-loss`, `--dump-copy` | aucun | — |
| 7 | Une **fuite volontaire** ferait échouer le critère 2 | 8 Mio par image, 60 images | ❌ **INFIRMÉE, et l'instrument est en cause, pas le code.** 480 Mio fuités : **aucune ligne `AddressSanitizer`**, `EXIT=0`. **L'ASan de MSVC n'embarque pas LeakSanitizer.** Le cas négatif valide sous MSVC est une écriture hors bornes : `p[16] = 'x'` → `heap-buffer-overflow`, `EXIT=1` | **le cas négatif prévu ne s'applique pas à la plateforme** | **2** — instrument dont la limite n'avait pas été lue |
| 7 | La ligne `Path` **n'est ni sélectionnable ni copiable**, et la refermer demande deux choses | lecture de RmlUi 6.3, puis `--dump-copy` | ⚗ **PARTIELLEMENT INFIRMÉE.** La **sélection** ne demandait rien : `WidgetTextInput` s'abonne à `Mousedown`/`Drag` et `ProcessMouseButtonDown` donne le focus — le routage **souris** de l'étape 6 suffisait. Restaient deux maillons, tous deux faits. ⚠ Le dernier, glfw → presse-papier du **système**, n'est **pas mesurable** dans cette session | **l'affirmation d'origine était une déduction, pas une mesure** | **1** — affirmation sur l'existant non vérifiée |
| 7 | `glPushAttrib` et le pare-feu tiennent encore après **deux découpages de fichiers** | 16 instruments, comparaison octet pour octet | ✅ **Vérifiée.** `stdout`, `stderr` et code de sortie **identiques** avant et après les onze unités de traduction. La seule différence du lot est une ligne retirée sciemment de `--dump-copy` | aucun | — |
| 7 | *(non posée d'avance)* Le rétrécissement d'un vecteur lié est **sans conséquence** | 12 exécutions par instrument | ❌ **INFIRMÉE, et c'est le défaut le plus coûteux du lot.** `--reopen` **5/10** et `--open-second` **5/10** en `EXIT=3`, sur le binaire de l'**étape 6**. Cause lue dans RmlUi : égalité de `sort_order` entre `DataViewFor` et `DataViewAttribute` sur le **même** élément, `std::sort` non stable. Corrigé, **0/48** | **total, et non déterministe** | **5** — critère relevé une seule fois là où le défaut tombe une fois sur deux |

### Verdict de l'étape 2, mesuré le 2026-09-22

**Les trois critères d'acceptation passent.**

| # | Critère | Mesure |
|---|---|---|
| 1 | `peles --assets <inexistant>` sort non nul en nommant le répertoire, **sans ouvrir de fenêtre** | `peles : --assets C:/chemin/inexistant : le repertoire n'existe pas`, `EXIT=3`. La ligne `GL_VERSION = "…"`, qui n'est imprimée qu'**après** la création de la fenêtre, est absente : c'est le marqueur observable de « aucune fenêtre ouverte ». ⚠ **Une quatrième branche rejoint ce critère depuis la revue** : document RML sans l'élément `#viewport` ⇒ même famille de panne, **même code 3** |
| 2 | `peles --state-audit` : tous les états relevés identiques, sur les deux phases, sur des images réellement rendues | `25 etat(s) releves, 3 image(s) rendue(s), 2 phases sous cloture par image` puis `AUDIT OK …`, `EXIT=0`. Tenu aussi avec `--debugger` |
| 3 | `peles --frames 120 --dump-timing` | **médiane 0,63 à 0,66 ms** sur quatre exécutions de **120 images effectivement rendues** après correction (0,656 / 0,629 / 0,651 / 0,641), maximum 4,6 à 6,4 ms, synchronisation verticale coupée, `EXIT=0` sur les quatre. Très en deçà du repère de 16,6 ms — qui reste **une estimation non normative**, et **R-4 reste ouverte** : le document de l'étape 2 est vide, donc cette mesure ne dit rien du coût d'un `data-for` de 1000 lignes |

**Deux mesures hors critères, prises pendant l'étape** (instrumentation temporaire, retirée depuis) :

- **La vue 3D dessine bien le cube, et `cgre` n'a rien perdu au passage.** `glReadPixels` du pixel
  central sous `peles` : **(25,128,204)**, soit **exactement** la valeur de `peles_probe`, identique
  sur trois images consécutives. Le pixel de coin vaut (31,31,36), la couleur d'effacement de
  **l'hôte** — donc la phase 4 efface et `cgre` n'a pas repeint le cadre. ⚗ Le critère `--readpixel`
  formel reste celui de l'étape 3.
- **La phase 3 lit réellement la géométrie de RmlUi.** `#viewport` doté d'une marge
  `50px 120px 30px 200px` sur un cadre 1280×800 → rectangle lu **(200, 50, 960, 720)**, GL
  **(200, 30, 960, 720)**. Les quatre bords et la conversion d'origine sont exacts. Sans cette
  mesure, un repli silencieux sur le plein cadre aurait donné le même rendu et la même image.

**Deux détecteurs ont été validés sur leur cas négatif** (règle de `global-context`) :

- **Critère 2.** Clôture `GlStateFence` retirée de la phase 7 → `AUDIT ECHOUE : 1 image(s) sur 3
  divergent. Etats fautifs : GL_BLEND`, `EXIT=1`. Fichier restauré, re-vérifié vert. Le relevé
  **nomme** bien l'état fautif, et il n'est pas un critère qui passerait sans rien mesurer.
- **Critère 1.** Trois branches distinctes exercées : répertoire inexistant (`le repertoire n'existe
  pas`), répertoire existant sans `peles.rml` (`le repertoire existe mais ne contient pas
  peles.rml`), et `PELES_ASSETS_DIR` errone (même traitement). ⚠ **Les niveaux (1) et (2) ne
  retombent pas sur les suivants, et c'est ce qui rend le critère falsifiable** : un `--assets` erroné
  qui glisserait vers l'arbre source rendrait l'application muette sur la faute.

**Trois décisions prises pendant l'étape, à connaître :**

- **Le contexte est demandé avec les hints explicites 3.3 + `GLFW_OPENGL_COMPAT_PROFILE`**, soit la
  variante P3 de la sonde et non la variante P1 « sans hint ». Les deux ont été mesurées
  équivalentes sur cette machine (masque `0x2` dans les deux cas) ; l'explicite est retenu parce
  qu'il est le seul **vérifiable** — un pilote qui délivrerait un contexte core rendrait un écran
  noir sans un mot. Conséquence visible : `peles` imprime `GL_VERSION = "3.3.0 - Build …"` là où
  `peles_probe` sans hint imprime `"4.6.0 - Build …"`. **Même pilote, même profil.**
- **La police est `LatoLatin-Regular.ttf`, SIL OFL 1.1**, versionnée sous `peles/assets/fonts/` avec
  son texte de licence complet à côté (`LatoLatin-Regular.LICENSE.txt`, 108 l.). Elle est chargée par
  `Rml::LoadFontFace`. ⚠ **Ce n'est pas une police du `Rml::Debugger`** : celui-ci charge les
  siennes depuis la mémoire, ce que le journal montre (`Loaded font face 'rmlui-debugger-font' …
  from 'memory'`). Le couplage refusé ne s'est pas introduit.
- **`Rml::Debugger` est derrière `--debugger`, éteint par défaut.** Deux raisons : le relevé d'état
  doit mesurer le chemin nominal, et le débogueur ne doit rien coûter à qui ne le demande pas.
  ⚠ **Bénéfice non cherché, et il renforce le critère 2** : avec `--debugger`, `Context::Render()`
  émet de la géométrie **texturée** réelle (barre du débogueur, atlas de police) et le relevé rend
  toujours `AUDIT OK`. Sans cela, le critère 2 aurait porté sur un `Render()` presque vide — le
  document de l'étape 2 n'a aucun texte.

### Corrections de revue de l'étape 2, mesurées le 2026-09-22

La revue a rendu **REQUEST CHANGES** : 0 critique, 3 HIGH, 7 medium, 6 low. ⚠ **Les trois HIGH étaient
des défauts d'instrument, pas de code** — deux des trois critères d'acceptation pouvaient rendre vert
**sans avoir rien mesuré**. Le code lui-même n'a été pris en défaut sur aucun état : la clôture tenait
déjà tout ce que le relevé ne regardait pas.

**1. Le repli muet quand `#viewport` est introuvable.** `ReadViewportRect` confondait « élément absent »
et « élément de taille nulle » derrière un seul `false`, et l'appelant repliait sur le **plein cadre** —
indiscernable du succès, puisque `#viewport` occupe déjà tout le cadre. Reproduit par la revue :
identifiant renommé dans la copie **déployée** du `.rml`, sans recompilation ⇒ `AUDIT OK`, `EXIT=0`,
**pas un mot**. Corrigé par `ViewportRectStatus` à trois valeurs (§1.3). **Mesuré sur le même cas
négatif** : `peles : aucun element d'identifiant "viewport" dans le document RML : la vue 3D n'a aucune
surface. Assets lus depuis <dir>.`, **`EXIT=3`**, fichier restauré et re-vérifié vert.

**2. Le relevé était aveugle à la moitié du contrat.** Cinq trous, tous fermés :
(a) aucun état **matriciel** — alors que la clôture les empile, et que c'est par **lecture** de
`BeginFrame`, pas par l'instrument, que la matrice de texture avait été trouvée ;
(b) `GL_STENCIL_TEST`, **le seul état que RmlUi laisse franchement allumé derrière lui** (`EndFrame`
est vide), n'était pas relevé ;
(c) la clôture de la **phase 5 n'était jamais mesurée** — les deux relevés encadraient la seule
phase 7, et la phase 6 renormalise à la main trois des états concernés juste avant ;
(d) **trois** unités de texture en dur, alors que le chemin PBR de `cgre` lie ses cartes sur les
unités **2 à 6** ; (e) `GL_CURRENT_PROGRAM` ne pouvait **structurellement** pas diverger.

**Chaque état ajouté a été vu ROUGE une fois**, par instrumentation temporaire (clôture non dépilée,
sélectionnable par phase, et salisseur d'un état à la fois), **retirée depuis** :

| État ajouté | Cas négatif | Sortie de l'instrument |
|---|---|---|
| `GL_MATRIX_MODE` | clôture 7 affaiblie + `glMatrixMode(GL_TEXTURE)` | `GL_MATRIX_MODE, GL_PROJECTION_MATRIX, GL_BLEND, GL_STENCIL_TEST` |
| `GL_PROJECTION_MATRIX` | clôture 7 affaiblie, **sans salisseur** | `GL_PROJECTION_MATRIX, GL_BLEND, GL_STENCIL_TEST` — RmlUi la réécrit lui-même |
| `GL_TEXTURE_MATRIX` | clôture 7 affaiblie + `glScalef` sur `GL_TEXTURE` | `…, GL_TEXTURE_MATRIX, …` |
| `GL_MODELVIEW_MATRIX` | clôture 7 affaiblie + `glTranslatef` | `…, GL_MODELVIEW_MATRIX, …` |
| `GL_STENCIL_TEST` | clôture 7 affaiblie, **sans salisseur** | présent dans **toutes** les exécutions à clôture affaiblie |
| `GL_STENCIL_WRITEMASK` | clôture 7 affaiblie + `glStencilMask(0x0F)` | `…, GL_STENCIL_WRITEMASK` |
| `GL_ACTIVE_TEXTURE` | clôture 7 affaiblie + `glActiveTexture(GL_TEXTURE3)` | `…, GL_ACTIVE_TEXTURE, …` |
| `GL_TEXTURE_BINDING_2D[3…7]` | clôture 7 affaiblie + liaisons sur les unités 3 à 7 | les **cinq** nommées une à une |
| `GL_CURRENT_PROGRAM` | **clôture INTACTE** + programme trivial laissé lié | `AVANT = 0`, `APRES = 4`, `3 image(s) sur 3 divergent`, `EXIT=1` |
| **clôture de la phase 5** | clôture 5 affaiblie, sans salisseur | `image n° 1, phase 5 (vue 3D, cgre). Etats fautifs : GL_PROJECTION_MATRIX, GL_MODELVIEW_MATRIX, GL_DEPTH_TEST, GL_LIGHTING, GL_SCISSOR_TEST` |

⚠ **Ce que la dernière ligne établit, et que l'ancien instrument ne pouvait pas voir** : `GL_LIGHTING`
et les deux matrices laissées par `cgre` auraient traversé la phase 6 — qui ne renormalise que
`GL_SCISSOR_TEST`, `GL_DEPTH_TEST` et `GL_VIEWPORT` — et se seraient retrouvées des **deux** côtés du
relevé de la phase 7, donc invisibles. **Le contrat pose deux clôtures ; le critère n'en vérifiait
qu'une.**

⚠ **`GL_CURRENT_PROGRAM` est le seul état relevé à un instant différent des autres**, et c'est la
condition de sa falsifiabilité : il est lu **avant** le `glUseProgram(0)` du destructeur de la clôture.
Le relevé imprimé le dit en toutes lettres — un point de mesure particulier qui ne serait pas annoncé
se lirait de travers.

**3. Une image non rendue était comptée comme une image auditée réussie.** `auditedFrames` comptait les
**tours de boucle**. Tampon de taille nulle, ou `#viewport` dégénéré : les phases 2 à 8 étaient sautées,
deux instantanés vierges — donc identiques — rendaient `AUDIT OK`, `EXIT=0`, et `--dump-timing` une
médiane de ~0 ms confortablement sous le repère. ⚠ **C'est la distinction que l'étape 1 avait été
corrigée pour faire** (`SKIP` ≠ `FAIL`) et qui n'avait pas été transportée — ici elle rendait `PASS`,
le seul des trois verdicts franchement faux. `RenderFrame` rend désormais `bool`.
**Mesuré sur son cas négatif** : `#viewport` forcé à `flex: 0 0 0px; height: 0px` dans la copie
déployée du `.rcss` ⇒ `peles : 3 image(s) demandee(s), 0 rendue(s)`, `AUDIT NON CONCLUANT : aucune
image n'a ete RENDUE`, `aucune image rendue : il n'y a pas de mediane a rendre.`, **`EXIT=1`**.
Fichier restauré, re-vérifié vert.

**4. Le décompte d'états est dérivé, plus recopié.** L'instrument annonçait « 12 états » et en imprimait
13 ; le chiffre était recopié à quatre endroits. `GlStateSnapshot::Entries()` est désormais l'unique
source de `Describe()`, `StateCount()` et `DifferingStates()`, et `--help` **ne cite aucun nombre** — il
dépend du contexte GL, qui n'existe pas à l'heure où `--help` s'imprime.

**5. La preuve imprimée est celle de la première image divergente.** Elle était celle de la dernière
image rendue, alors que le mode de panne normal est « divergent à la première, masqué ensuite » : la
preuve contredisait le verdict exactement dans le cas où l'on en a le plus besoin. **Mesuré** : clôture
7 affaiblie ⇒ `releve imprime : image n° 1, phase 7 (interface, RmlUi) (PREMIERE image divergente)`,
avec `GL_BLEND = 0` avant et `= 1` après. L'avertissement « ce compte n'est PAS un taux » est passé du
commentaire à **la ligne de verdict**.

**6. Dépendances CMake — les empreintes de commit, et une politique qui ne se voyait pas.**
`GIT_TAG VER-2-13-3` et `GIT_TAG 6.3` étaient des **étiquettes mutables** sans vérification
d'intégrité, pour du code qui entre dans le binaire, alors que le dépôt tient le standard opposé
partout ailleurs. Remplacées par les empreintes `42608f77…` (FreeType) et `ba95ffe8…` (RmlUi),
étiquettes conservées en commentaire. **Vérifié** : les scripts `*-gitclone.cmake` engendrés portent
les empreintes, et `git rev-parse HEAD` des deux arbres extraits leur est égal.

⚠ **Les neuf `set(… CACHE … FORCE)` sont devenus des variables normales de portée fonction — mais cela
exigeait une ligne que la justification existante ne couvrait pas.** La justification donnée pour
`BUILD_SHARED_LIBS` invoque CMP0077 en NEW, vrai chez RmlUi (`cmake_minimum_required(3.10…3.27)`).
**Elle est fausse chez FreeType**, qui déclare `3.0…3.5` : CMP0077 y serait **non définie**, donc OLD,
et son `option()` écraserait la variable normale. D'où `set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)`.
**Détecteur validé sur son cas positif** : la même ligne forcée en `OLD` ⇒ `Could NOT find ZLIB`,
`PNG`, `BZip2`, `BrotliDec`, avertissement HarfBuzz, et les cinq `FT_DISABLE_*` recréés en cache à
`OFF`. En `NEW` : aucun de ces messages, et **aucune** entrée `FT_DISABLE_*` ni `RMLUI_*` en cache —
`option()` n'a rien créé. Les cinq coupures sont donc bien effectives, et plus rien n'est forcé dans
le cache de l'utilisateur après `ENABLE_PELES=OFF`.

**7. `peles_add_rmlui()` : la garde de ré-entrée sortait sans rendre sa variable de sortie.**
**Reproduit** — premier appel dans une portée qui s'évapore, second appel dans `peles/` :
`CMake Error at peles/CMakeLists.txt:113 (add_library): Cannot find source file:` — un diagnostic qui
ne nomme ni RmlUi ni la fonction. Variable désormais rendue **avant** le `return()`, recalculée par
`FetchContent_GetProperties`. Même scénario après correction : `Configuring done`, `Generating done`.

**8. Visibilité des liens, et deux conventions dans le même répertoire.** `glfw` et `RmlUi::Debugger`
passent en `PRIVATE` sur `peles_platform` — seul `RmlUi::Core` doit rester public,
`rml_system_interface.h:3` en dérive ; `RmlUi::Core` en `PRIVATE` sur `peles_ui` ; `cgre` en `PRIVATE`
sur `peles_view3d`, l'exécutable déclarant désormais son propre `target_link_libraries(peles PRIVATE
cgre)`. `NOMINMAX` **et** `GLFW_INCLUDE_NONE` sont posés par `target_compile_definitions` sur les deux
cibles concernées : `NOMINMAX` était par fichier, et `asset_paths.cpp` incluait déjà `<windows.h>`
**sans** lui.

**9. Narration retirée du code.** Trois formes : relevés de mesure datés dans le source
(`window_glfw.cpp` inscrivait `GL_VERSION = "4.6.0 - Build 32.0.101.6078"` et un numéro de pilote Intel
qui sera faux à la prochaine mise à jour) ; renvois aux identifiants du plan (`R-P3`, `R-P7`,
`P1/P2/P3`, « étapes 3 à 5 ») ; alternatives rejetées. Le *pourquoi mécanique* est conservé partout.

**10. Divers, tous mesurés.** `Application::Run` passe de ~100 lignes à 67 (40 hors commentaires et lignes vides) (`ReportStateAudit` et
`ReportTiming` extraits, les compteurs regroupés dans `FrameAudit`, leur propriétaire naturel) — c'est
ce qui rendait les points 3 et 5 difficiles à voir. `frameSeconds` n'est plus collecté sans
`--dump-timing`. `Rml::Set*Interface` sont remis à `nullptr` si `Rml::Initialise()` échoue — le seul
chemin où `Rml::Shutdown()` ne le fait pas. `ParseCount` contrôle `errno == ERANGE` et borne à
`INT_MAX` : `--frames 4294967297` rendait **1 image** sous Linux et `INT_MAX` sous Windows, sans un
mot ; il rend maintenant `--frames : valeur invalide "4294967297" (attendu : entier de 1 a 2147483647).`
et `EXIT=2` sur les deux. `--assets` passe par le même analyseur que `--frames`.

**Ce que ce verdict ne lève pas :**

- **R-4 et R-5 restent entières.** La mesure du critère 3 porte sur un document **vide** : elle
  détecte un ordre de grandeur aberrant, elle ne qualifie rien.
- **R-P1 reste ouverte.** Le pixel central mesuré sous `peles` vaut **(25,128,204)**, identique au
  caractère près à celui de `peles_probe` — mais c'est le **même pilote**, donc ce n'est toujours pas
  une calibration.
- **Pas de HiDPI, et c'est assumé.** `Context::SetDimensions` reçoit la taille du framebuffer, aucune
  unité `dp` n'apparaît dans `peles.rcss`.
- **La propriété « configuration hors ligne » du dépôt est rompue sous `ENABLE_PELES=ON`** (R-P8).
  C'est écrit en tête de `peles/CMakeLists.txt`, là où on le lit quand la configuration échoue, **et
  désormais dans la chaîne d'aide de l'option elle-même** (`CMakeLists.txt` racine) — c'est ce que
  `cmake -LH` et cmake-gui affichent, et la chaîne disait encore « currently peles_probe only […]
  aucune interface, ni wxWidgets ni RmlUi », devenu faux à l'étape 2.

### Verdict de l'étape 3, mesuré le 2026-09-22

**Les quatre critères d'acceptation passent.** Binaire bâti par
`cmake --build build/peles-probe --config Release --target peles`, machine Windows de référence,
`GL_VERSION = "3.3.0 - Build 32.0.101.6078"`.

**La valeur attendue du rectangle, écrite ici pour qu'elle soit falsifiable.** Elle se calcule à la
main depuis `peles/assets/peles.rcss`, qui n'emploie que des `px` :

| Terme | Règle RCSS | Valeur |
|---|---|---|
| `#panel-left` | `flex: 0 0 240px` | 240 |
| `#panel-right` | `flex: 0 0 280px` | 280 |
| `#panel-bottom` | `flex: 0 0 180px` | 180 |
| `#viewport.x` | après le panneau gauche | **240** |
| `#viewport.y` | sous rien — la zone basse est en dessous | **0** |
| `#viewport.w` | `L − 240 − 280` | **L − 520** |
| `#viewport.h` | `H − 180` | **H − 180** |

Aucune marge, aucun remplissage, aucune bordure sur le chemin body → `#row` → `#viewport` : c'est ce
qui rend la valeur indépendante du modèle de boîte.

| # | Critère | Mesure |
|---|---|---|
| 1 | Rectangle exact | `--dump-layout 1400x900` → `RmlUi x=240 y=0 w=880 h=720`, `GL x=240 y=180 w=880 h=720`. La table ci-dessus prédit `880 × 720` en `x=240, y=0` |
| 2 | Variation au pixel | `1401x900` → `w=881 h=720` ; `1400x901` → `w=880 h=721`. **+1 exactement, sur l'axe touché et lui seul** |
| 3 | `GL_VIEWPORT` concordant | `GL_VIEWPORT concordant avec le rectangle lu sur 120 image(s).` La comparaison tourne **à chaque image et sans option** ; seul son verdict vert est réservé aux exécutions à `--frames`. Elle est prise **à l'intérieur** de la clôture, `GL_VIEWPORT` faisant partie de la pile d'attributs |
| 4 | Le portage n'a rien changé | `peles --frames 1 --readpixel 620 490 test/data/obj/cube.obj` → **(25,128,204)**, `EXIT=0`. C'est la valeur de `peles_probe` au caractère près |

**Les quatre détecteurs ont été validés sur leur cas négatif**, et les fichiers restaurés puis
re-vérifiés verts à chaque fois :

| Détecteur | Cas négatif injecté | Sortie |
|---|---|---|
| Critère 3 | `glViewport (x + 7, …)` dans la phase 5 | `GL_VIEWPORT DIVERGENT … Premiere divergence a l'image n° 1 : attendu (240, 180, 760, 620), releve (247, 180, 760, 620)`, **`EXIT=1`** |
| Critère 1 | `flex: 0 0 240px` → `200px` dans la copie **déployée** du `.rcss`, sans recompiler | `x=200 y=0 w=920 h=720` — plus la valeur attendue |
| Critère 4 | `renderer->Draw (id)` neutralisé | `pixel lu : (620, 490) = (31,31,36)` — la couleur d'effacement de l'hôte — `ecart max 168/255`, `PIXEL HORS TOLERANCE`, **`EXIT=1`** |
| Contrat de destruction §1.8 | `Document::AnnounceRelease` neutralisée | le destructeur de `SceneRenderer` trouve **2 maillages encore enregistrés** au lieu de **0**, donc deux `RemoveMesh` sur des `Mesh*` déjà détruits. Avec l'annonce : deux `OnMeshReleasing` reçus, **0** restant |
| Pare-feu de cibles | 4 inclusions fautives, une par cible | `gl_wrapper.h` depuis `core/`, `ui/`, `platform/` et `<RmlUi/Core.h>` depuis `view3d/` : **les quatre** rendent `error C1083` |

⚠ **Le critère 4 a un biais, et l'instrument le dit lui-même.** `--readpixel` **épingle la pose de
mesure** : rotation frontale et ombrage plat. Sans cela, la normale de la face vue au centre n'est
plus `(0,0,1)` et aucune couleur n'est prédictible — le critère deviendrait inécrivable, pas plus
sévère. L'épinglage est **imprimé à chaque exécution**, sur la ligne `pose de mesure`. Le reste du
chemin est nominal : loi de cadrage, `set_camera`, parcours de `VModels`, `MeshRenderer`. Un second
garde-fou ferme le mode de panne le plus probable : **document vide ⇒ `LECTURE NON CONCLUANTE` et
`EXIT=1`**, sans quoi « le fichier n'a pas été chargé » se lirait comme une mesure.

**Les critères de l'étape 2 restent verts, et le relevé est plus sévère qu'alors :**

- `peles --state-audit <maillage réel>` → `25 etat(s) releves, 3 image(s) rendue(s)`, `AUDIT OK`,
  `EXIT=0`. Tenu sur `test/data/rabbit.obj` (1 maillage, ~1 350 lignes) **et** sur
  `test/data/obj/cube-tex.obj` avec `--debugger`, donc avec de la géométrie **texturée** émise des
  deux côtés du pare-feu. Le `glEnable(GL_NORMALIZE)` que le portage laisse allumé est bien confiné :
  `GL_NORMALIZE = 0` des deux côtés du relevé.
- `peles --assets <inexistant>` → `EXIT=3`, sans fenêtre.
- `peles --frames 120 --dump-timing <maillage réel>` → **médiane 0,737 ms**, maximum 7,796 ms,
  120 images rendues, `EXIT=0`. ⚠ **R-4 reste ouverte** : le document RML est toujours vide de texte.
- `peles_probe` → **7/7**, `EXIT=0`.
- `ENABLE_CGGRAPH_BOILERPLATE=ON` **et** `ENABLE_PELES=ON` : configurent et bâtissent, **un seul**
  `glfw3.lib`.

**Une loi de cadrage, et une seule.** `view3d::Camera::Frame` **appelle** `OrbitCamera::Frame` au
lieu de réécrire `d = r/sin(fovY/2) × 1,2` : une `OrbitCamera` de travail applique la loi, son
résultat est publié au trackball. L'ordre que `debt_sinaia.md:156-159` signale comme non trivial est
tenu explicitement — `set_scene_spheres` **avant** `set_pivot`/`set_zoom`, puisque `set_zoom` borne
la distance contre le rayon de cadrage. `OrbitCamera::Frame`, qui avait **zéro appelant**, en a un.
⛔ `sinaia/FrameCamera` n'est pas touché : la deuxième copie subsiste **dans `sinaia`**, ce que le
périmètre impose.

**Quatre décisions prises pendant l'étape, à connaître :**

- **`InitGL` est RÉÉCRIT, pas « non porté ».** ⚠ **Formulation corrigée par la revue de l'étape 3** :
  `glDisable(GL_CULL_FACE)` et `glEnable(GL_DEPTH_TEST)` (`wxOpenGLCanvas.cpp:1906-1907`) **sont**
  portés, dans `SceneRenderer::Draw`. Ce qui est réécrit, c'est l'éclairage. ⚠ **Et la justification
  d'origine — « sinon le critère 4 est inécrivable » — était fausse** : elle faisait prescrire par
  l'oracle l'éclairage du produit. Corrigé : l'éclairage d'`InitGL` est porté terme pour terme et sert
  de **défaut**, l'éclairage déterministe de la sonde devient le **troisième terme de la pose de
  mesure**, posé par `--readpixel` avec la rotation frontale et l'ombrage plat. Voir §1.12.
- **Le repère et la grille ne sont pas portés** (`wxOpenGLCanvas.cpp:1384-1413`).
  `rendering_properties_init` laisse `display_repere` et `display_grid` à **0** et le lot n'a aucun
  contrôle pour les allumer : les porter ajouterait deux chemins que rien n'exécute. À reprendre avec
  la commande qui les allumera.
- **La pose de repos est la vue trois-quarts** (`Ctrackball::ResetTransformations`), pas l'identité.
  La pose frontale n'est posée que par `--readpixel`.
- **`core/unit_cube.{h,cpp}` a disparu.** Le cube en mémoire de l'étape 2 est remplacé par le fichier
  passé en argument ; `peles_probe` garde le sien, qui lui est propre.

**Ce que ce verdict ne lève pas :**

- **La BRANCHE UTILE d'`OnMeshReleasing` n'est exercée par aucun chemin nominal.** ⚠ **Formulation
  corrigée par la revue de l'étape 3** : dire « `OnMeshReleasing` n'est exercé par **aucun** chemin
  nominal » est inexact. `core/loader.cpp` appelle `Document::RemoveModel` sur échec de chargement,
  donc `AnnounceRelease` **est** appelée. Ce qui est mort, c'est la branche où le maillage figure dans
  `m_registered` — il faut qu'il ait été **dessiné** pour y être. Cette branche a été exercée par
  instrumentation temporaire (ci-dessus, puis à nouveau lors des corrections de revue) et **retirée
  depuis** ; elle redeviendra nominale à l'étape 4, quand une seconde ouverture remplacera la scène.
- **La surbrillance de survol est portée mais morte** : rien n'écrit `Document::Hovered` avant
  l'étape 6. Elle est portée parce que le portage l'exige, pas parce qu'elle est mesurée.
- **R-P1 reste ouverte.** Même pilote, même machine : (25,128,204) reproduit, mais ce n'est toujours
  pas une calibration. La tolérance ±2/255 reste **ESTIMÉE**.
- **R-P2 reste ouverte** : aucune parité caméra avec `sinaia` n'est mesurée. `--readpixel` ne compare
  pas une pose, il compare une couleur sous une pose épinglée.
- **Aucune interaction souris.** `Camera::SetViewSize` reçoit `(rect.w, rect.h)` — la moitié
  structurelle du piège de §1.7 est en place — mais la translation des coordonnées souris par
  l'origine du rectangle appartient à `input_router`, étape 6, et **n'est pas écrite**.

### Corrections de revue de l'étape 3, mesurées le 2026-09-22

Revue : **REQUEST CHANGES**, 0 critical, **2 HIGH**, 5 medium, 9 low. Les deux HIGH sont des défauts
**d'instrument et de frontière**, pas de portage : la revue juge le portage fidèle sur tous les points
prévus, le contrat de destruction tenu sur tous les chemins, la loi de cadrage correctement fusionnée
et le critère 3 bien à l'intérieur de la clôture.

**F1 — le critère 4 prescrivait l'éclairage du produit au lieu de le constater.**
`SceneRenderer::SetMeasurementLighting(bool)` posé à côté de `SetFlatShading` ; **défaut = éclairage
produit**, portage terme pour terme d'`InitGL:1930-1965`. `Application::CreateSceneAndDocument` pose
les **trois** termes de la pose de mesure ensemble. Détail des deux branches : §1.12. **Quatre pertes
que le plan ne consignait nulle part sont supprimées** : ambiante globale `0,12`, lampe d'appoint `0,28`
dir `(0,7 ; 0,25 ; 0,4)`, spéculaire du key blanc, `GL_LIGHT_MODEL_TWO_SIDE = GL_TRUE`.

| Mesure | Avant correction | Après correction |
|---|---|---|
| `--frames 1 --readpixel 620 490 test/data/obj/cube.obj` | `(25,128,204)`, écart **1/255**, `EXIT=0` | `(25,128,204)`, écart **1/255**, `EXIT=0` — **identique** |

⚠ **Le commutateur est validé sur son cas négatif, et c'est ce qui établit que la branche produit
n'est pas un alias de la branche mesure.** Avec `SetMeasurementLighting(true)` neutralisé — donc le
même binaire, sous l'éclairage **produit** — le pixel central rend **`(19,94,153)`, écart 51/255**,
`PIXEL HORS TOLERANCE`, **`EXIT=1`**. Fichier restauré, re-vérifié vert.
⚗ **La revue prédisait `(19,94,151)`, écart 53** : l'écart de 2 sur le bleu vient de la diffuse de la
lampe d'appoint, qui est `(0,28 ; 0,28 ; **0,30**)` et non `0,28` sur les trois canaux
(`wxOpenGLCanvas.cpp:1959`). Calcul à la main confirmant la mesure : facteur `R,G = 0,741`, `B = 0,750`
→ `(18,9 ; 94,5 ; 153,1)`.

**F2 — le répertoire d'inclusion racine rouvrait l'arête `view3d → app`.**
`target_include_directories(peles_view3d PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})` supprimé ; la racine est
donnée à l'**exécutable seul** (`target_include_directories(peles PRIVATE …)`) ;
`view3d/scene_renderer.cpp` écrit `#include "camera.h"` au lieu de `"view3d/camera.h"`. Voir §1.4.

⚠ **Contrôle POSITIF du détecteur — l'arête était réellement ouverte, c'est mesuré.** Avec la ligne
supprimée **remise en place**, `#include "app/application.h"` en tête de `view3d/scene_renderer.cpp`
**compile** et `peles_view3d.lib` **est produite** — zéro `C1083`. C'est la validation du détecteur sur
un cas où il doit être muet ; sans elle, les huit `OK` ci-dessous ne prouveraient rien.

**La batterie de cas négatifs du pare-feu passe de 4 à 8 cas, et les huit rendent `error C1083`** :

| # | Cas négatif injecté | Cible bâtie | Résultat |
|---|---|---|---|
| 1 | `#include "gl_wrapper.h"` depuis `core/` | `peles_core` | `error C1083` |
| 2 | `#include "gl_wrapper.h"` depuis `ui/` | `peles_ui` | `error C1083` |
| 3 | `#include "gl_wrapper.h"` depuis `platform/` | `peles_platform` | `error C1083` |
| 4 | `#include <RmlUi/Core.h>` depuis `view3d/` | `peles_view3d` | `error C1083` |
| **5** | `#include "app/application.h"` depuis `view3d/` | `peles_view3d` | `error C1083` |
| **6** | `#include "app/application.h"` depuis `core/` | `peles_core` | `error C1083` |
| **7** | `#include "app/application.h"` depuis `ui/` | `peles_ui` | `error C1083` |
| **8** | `#include "platform/rml_host.h"` depuis `view3d/` | `peles_view3d` | `error C1083` |

**Les cinq MEDIUM.**

- **M1** — les quatre pertes d'éclairage sont consignées (§1.12) et **supprimées** par F1. Aucune ne
  subsiste.
- **M2** — **`app/dump_modes.{h,cpp}` créé**, que §1.4 déclarait depuis l'origine sans qu'il existe.
  `application.cpp` passe de **672 à 376 lignes** ; `dump_modes.cpp` en fait 369 et `dump_modes.h` 175.
  Y ont émigré : `FrameAudit`+`Accumulate`+`ReportStateAudit`, `ViewportTracker` (ex-`CheckSceneViewport`
  + `ReportViewportCheck`), `PixelProbe` (ex-lecture inline + `ReportPixel`), `DumpLayout`,
  `ReportTiming`. ⚠ `dump_modes.h` suit la règle de `gl_state_fence.h` : **aucun type OpenGL dans la
  déclaration**.
- **M3** — `Camera::Frame` lit `m_trackball->camera().GetFovY()`, **en radians à la source**, au lieu de
  `get_fov_y_deg() * kDegreesToRadians` : le double passage rad → deg → rad ajoutait un arrondi que
  `get_pick_ray`, qui lit la même grandeur, ne fait pas (`examinator_trackball.h:116-120`).
  `kDegreesToRadians` a disparu de `view3d/camera.cpp`.
- **M4** — la précondition « **un contexte GL du groupe de partage courant** » est portée sur
  `Document::RemoveModel`, `Document::Clear` et `DocumentObserver::OnMeshReleasing`. À l'étape 4 ce
  chemin devient nominal et l'auteur lira `core/`, pas `view3d/`.
- **M5** — longueurs effectives (lignes non vides, hors commentaires, hors accolades seules) :
  `ParseArguments` extraite de `main` → **47** ; `Main` → **19** ; `main` → **8** ;
  `Application::Create` éclatée en `CreateWindowAndGl` / `CreateUi` / `CreateSceneAndDocument` →
  **9 / 13 / 19 / 16**. ⚠ **`RenderFrame` est laissée en un bloc** (41 effectives) : la lecture linéaire
  des huit phases a une valeur propre.

**Les neuf LOW.** 1 narration résiduelle dans le CMake (`(§1.10)` et « conservée au-delà de l'étape qui
l'a fait naître ») — retirée. 2 `new` nu dans `camera.cpp` et `application.cpp` → `std::make_unique` ;
le `new Application` de `Create` est **conservé** (constructeur privé) et le commentaire le dit.
3 `properties.light` n'était jamais lu : `glEnable(GL_LIGHTING)` suit désormais `properties.light` dans
`DrawModels`, comme `DrawGL:1374-1377` — **une seule décision d'éclairage** pour le pipeline fixe et
pour l'uniforme `uLighting`. 4 `--readpixel` exige que la coordonnée tombe **dans** le rectangle, et le
rectangle imprimé est désormais le **relevé**, plus l'attendu. 5 `ComputeBBox()` par image retiré de
`DrawHoverHighlight` (la boîte est calculée à l'import, la géométrie est statique ensuite). 6 `peles ""`
sort en **2** en nommant la faute. 7 `AnnounceRelease` itère sur une **copie** de `m_observers`.
8 `main` a un `catch` de dernier recours, code **1**. 9 correction factuelle portée ci-dessus.

**Mesures de non-régression, même machine, `GL_VERSION = "3.3.0 - Build 32.0.101.6078"` :**

| Critère | Mesure |
|---|---|
| `peles_probe` | **7/7**, `EXIT=0` |
| Étape 3, critère 1 | `--dump-layout 1400x900` → `RmlUi x=240 y=0 w=880 h=720`, `GL x=240 y=180 w=880 h=720` |
| Étape 3, critère 2 | `1401x900` → `w=881 h=720` ; `1400x901` → `w=880 h=721` |
| Étape 3, critère 3 | `GL_VIEWPORT concordant … sur 1 image(s)` et `sur 120 image(s)` |
| Étape 3, critère 4 | `(25,128,204)`, écart 1/255, `PIXEL CONFORME`, `EXIT=0` |
| Étape 2, (a) | `--assets C:/nowhere` → `EXIT=3`, sans fenêtre |
| Étape 2, (b) | `--state-audit test/data/rabbit.obj` → `25 etat(s) releves, 3 image(s) rendue(s)`, `AUDIT OK`, `EXIT=0`. Tenu aussi sur `test/data/obj/cube-tex.obj --debugger`, `GL_NORMALIZE = 0` des deux côtés |
| Étape 2, (c) | `--frames 120 --dump-timing` → médiane **0,226 ms**, maximum 15,504 ms, 120 images rendues, `EXIT=0` |
| Étape 1 | `ENABLE_CGGRAPH_BOILERPLATE=ON` **et** `ENABLE_PELES=ON` : configurent et bâtissent, un seul `glfw3.lib` |

**Les trois autres détecteurs de l'étape 3 ont été revalidés sur leur cas négatif**, les instruments
ayant été déplacés ou modifiés :

| Détecteur | Cas négatif injecté | Sortie |
|---|---|---|
| Critère 3 (déplacé dans `ViewportTracker`) | `Draw (…, view.x + 7, …)` | `GL_VIEWPORT DIVERGENT … attendu (240, 180, 760, 620), releve (247, 180, 760, 620)`, **`EXIT=1`** — et `ReportPixel` imprime bien `x=247 … (RELEVE)`, ce que LOW 4 corrige |
| Critère 4 (déplacé dans `PixelProbe`) | `renderer->Draw (id)` neutralisé | `(620, 490) = (31,31,36)`, écart 168/255, `PIXEL HORS TOLERANCE`, **`EXIT=1`** |
| Contrat de destruction §1.8 (`AnnounceRelease` modifiée par LOW 7) | itération sur une liste vide | **avec** annonce : `1 OnMeshReleasing recu`, `0 maillage(s) encore enregistre(s)` ; **sans** : `0 recu`, `1 restant` — donc un `RemoveMesh` sur un `Mesh*` déjà détruit |
| LOW 4, coordonnée hors cadre | `--readpixel 10 10` | `LECTURE NON CONCLUANTE : (10, 10) est HORS du rectangle de la vue 3D`, **`EXIT=1`** — auparavant : `PIXEL HORS TOLERANCE`, verdict juste et **diagnostic faux** |
| LOW 6, nom de fichier vide | `peles ""` | `peles : nom de fichier vide.`, **`EXIT=2`** — auparavant : chargement silencieusement sauté |

⚠ **Le contrat de destruction est exercé sur sa branche UTILE pour la première fois.** L'instrumentation
temporaire détruisait la scène (`Document::Clear`) **après une image rendue**, donc avec un maillage
réellement enregistré auprès de `MeshRenderer` — et non à la fermeture, où `~SceneRenderer` court
**avant** `~Document` et nettoie lui-même. Instrumentation retirée, fichiers restaurés.

**Ce que ces corrections ne lèvent pas :**

- **Aucune des quatre pertes d'éclairage ne subsiste**, mais **R-P2 reste ouverte** : aucune parité
  caméra ni aucune parité d'image avec `sinaia` n'est mesurée. `(19,94,153)` est une valeur **calculée
  et mesurée sur `peles`**, jamais comparée à un rendu de `sinaia`.
- **R-P1 reste ouverte** : même pilote, même machine. La tolérance ±2/255 reste **ESTIMéE**.
- **`ViewportTracker::LastActual` est le relevé de la DERNIÈRE image**, pas celui de l'image sur
  laquelle le pixel a été lu. Les deux coïncident tant que `--readpixel` implique `--frames 1`
  (c'est le cas par défaut), mais `--frames 120 --readpixel` les dissocierait si le rectangle bougeait
  en cours d'exécution.
- **Le contrôle de la coordonnée de `--readpixel` porte sur le rectangle RELEVÉ**, donc il ne détecte
  pas une coordonnée qui serait hors du rectangle **attendu** mais dans le relevé. C'est délibéré : le
  relevé est ce qui a servi à dessiner. La divergence des deux est déjà le critère 3.

### Verdict de l'étape 4, mesuré le 2026-09-22

**Les cinq critères d'acceptation passent.** Machine Windows de référence,
`GL_VERSION = "3.3.0 - Build 32.0.101.6078"`, binaire bâti par
`cmake --build build/peles-probe --config Release --target peles`.

**La fixture est versionnée et figée** : `test/data/peles/fixture/`. Elle contient quatre
sous-répertoires (`_under`, `alpha`, `Beta`, `gamma`), un sous-arbre à trois niveaux
(`alpha/inner/deeper`), sept fichiers au catalogue et **deux qui n'y sont pas** — `cube.mtl` et
`notes.txt`. ⚠ **Les noms ne sont pas décoratifs** : `_under` et `_first.obj` commencent par `_`
(0x5F), donc l'ordre insensible à la casse ASCII les place **avant** les lettres, là où l'ordre de
NTFS — collation par majuscules — les place **après**. C'est ce qui rend les critères 1 et 2
falsifiables sur un système de fichiers qui trie déjà.

| # | Critère | Mesure |
|---|---|---|
| 1 | `--dump-tree <fixture>` identique **octet pour octet** sur deux exécutions | `cmp` sur deux sorties consécutives : identiques. Idem pour `--dump-files`. ⚠ **La sortie des deux vidages est écrite en mode BINAIRE** (`_setmode(_O_BINARY)` sous Windows) : en mode texte, le CRT traduirait `\n` en `\r\n` et le même vidage différerait d'un octet par ligne entre Windows et Linux — « identique octet pour octet » cesserait d'être vrai d'une plateforme à l'autre |
| 2 | Identique au fichier attendu versionné | `cmp` avec `test/data/peles/expected_dump_tree.txt` : identique. ⚠ **Aucun chemin absolu dans la sortie** — seul le dernier composant est imprimé, sans quoi le fichier attendu serait propre à une machine. Vérifié aussi en passant le chemin **relatif** et **avec séparateur final** : mêmes octets. ⚠ **Un `test/data/peles/.gitattributes` marque les deux fichiers attendus `-text`** : le dépôt a `core.autocrlf=true`, donc sans cette ligne ils seraient extraits en CRLF sur tout clone Windows et la comparaison échouerait pour une raison sans rapport avec ce qu'elle mesure |
| 3 | `--dump-files` liste **exactement** les extensions du catalogue | 7 lignes : `_first.obj`, `cloud.ply`, `cube.obj`, `MODEL.OBJ`, `part.stl`, `scene.glb`, `tri.off`. **`cube.mtl` et `notes.txt` sont absents.** `MODEL.OBJ` établit au passage que la correspondance d'extension est insensible à la casse |
| 4 | `tu_supported_formats` passe | 2 tests, `PASSED`, sous `ENABLE_SINAIA=ON -DEXTERN_WXWIDGETS=C:/home/dev/extern/wxWidgets-3.3.2`. Le test **inclut** `sinaia/SupportedFormats.h` sans le modifier. Sous `ENABLE_SINAIA=OFF`, le fichier compile à rien et `TU` se bâtit — vérifié dans les deux sens |
| 5 | Mesure de R-4 et R-5 à 100 / 1000 / 5000 lignes | Table ci-dessous. **Consignée quelle qu'elle soit** : à 1000 lignes le budget tient, à 5000 il ne tient pas |

**R-4 et R-5 — les chiffres, trois exécutions de 120 images chacune, synchronisation verticale
coupée.** Instrument : `peles --frames 120 --dump-timing --browse <dir> [--browse-depth n]
test/data/obj/cube.obj`, qui mesure **la seule phase 2** (`Rml::Context::Update()`) en plus de
l'image entière.

| Lignes liées | `Update()` 1ʳᵉ image (construction) | `Update()` médiane (rien de sali) | Image médiane |
|---|---|---|---|
| 1 (témoin, arbre non ouvert) | 0,193 / 0,195 / 0,199 ms | 0,004–0,005 ms | 0,77–0,79 ms |
| 100 | 0,701 / 0,801 / 0,740 ms | 0,010–0,012 ms | 0,77–0,78 ms |
| 1000 | 8,928 / 9,396 / 7,377 ms | 0,060–0,066 ms | 0,76–0,81 ms |
| **2002** (un seul nœud **au plafond** : 2000 + la ligne de débordement) | 16,135 / 18,543 ms | 0,160–0,177 ms | 1,48–1,53 ms |
| 5002 | 38,060 / 40,938 / 37,994 ms | 0,438–0,456 ms | 3,14–3,23 ms |

**Et le régime que §1.5 nomme « l'opération réelle » — `Toggle` puis réaffichage — mesuré avec un
basculement à CHAQUE image** (`--churn-tree 100`) :

| Lignes | `Update()` médiane | Image médiane |
|---|---|---|
| 1000 | **5,37 ms** | 7,09 ms |
| 5002 | **29,88 ms** | 36,34 ms |

**Ce que ces chiffres disent, et ce qu'ils ne disent pas :**

- **R-4 est levée pour 1000 lignes, et elle ne l'est pas pour 5000.** À 1000 lignes, construction
  ≈ 9 ms et repli/dépli continu ≈ 5,4 ms : les deux tiennent sous le repère de 16,6 ms. À 5000, la
  construction coûte **2,3 à 2,5 images** et le repli/dépli continu **1,8 image**.
- **Le coût est linéaire en N, pas quadratique** : 100 → 0,75 ms, 1000 → 8,9 ms, 5000 → 39 ms, soit
  ≈ 7,7 µs par ligne construite sur les trois points. Aucune opération devenue quadratique.
- ⚠ **Le plafond de 2000 par nœud est MESURÉ, plus seulement estimé, sur son effet le plus
  défavorable** : un **seul** répertoire de 2500 sous-répertoires produit 2002 lignes et coûte
  **16,1 à 18,5 ms** à construire, c'est-à-dire **exactement le repère de 60 Hz**. Le plafond place
  donc le pire nœud unique **à la limite**, pas en deçà. Le faire descendre à 1000 ramènerait ce pire
  cas à ≈ 9 ms. **La décision n'est pas prise ici** : le plan dit qu'elle appartient au lot suivant,
  et le chiffre pour la prendre existe maintenant.
- **Le régime établi ne coûte presque rien** (0,44 ms d'`Update` à 5002 lignes) : le coût est celui
  des images où le modèle est **sali**, pas celui de l'affichage.
- ⚠ **Les mesures d'avant la correction de la mise en page sont NULLES ET NON AVENUES.** Elles ont été
  prises avec un `#row` de 18 826 px de haut (cf. §1.3) : ce n'est pas la mise en page du produit.

**Sept détecteurs validés sur leur cas négatif**, fichiers restaurés et re-vérifiés verts à chaque
fois :

| Détecteur | Cas négatif injecté | Sortie |
|---|---|---|
| Critères 1 et 2, **arbre** | `std::sort` de `DirectoryTree::Enumerate` neutralisé | `_under` passe de la 2ᵉ à la dernière position — l'ordre de NTFS, pas celui du comparateur. `diff` non vide |
| Critères 1 et 3, **liste** | `std::sort` de `FileList::Populate` neutralisé | `_first.obj` passe de la 1ʳᵉ à la dernière position. `diff` non vide. ⚠ **Sans `_first.obj` dans la fixture, ce détecteur était MUET** : sur les sept autres noms, l'ordre de NTFS coïncide avec celui du comparateur. Le fichier a été ajouté pour cette raison |
| Critère 3, **filtre** | `"txt"` ajouté à la famille OFF du catalogue de `peles` | `--dump-files` liste `notes.txt`, `fichiers : 8` au lieu de 7 : `diff` non vide |
| Critère 4, **anti-divergence** | le **même** ajout de `"txt"` | `tu_supported_formats` **ÉCHOUE** : `absentes de peles : (aucune) \| absentes de sinaia : txt`. ⚠ **Un seul cas négatif valide les critères 3 et 4 ensemble**, et c'est ce qui établit que le test n'est pas un test qui passerait sans rien comparer |
| **Lignes en erreur d'accès** | un sous-répertoire sous `icacls /deny` | ligne `d1 [!] secret`, **affichée et non ouvrable**, et le parent reste énumérable (`ok` est là). ⚠ **Ce cas a trouvé un VRAI DÉFAUT** — voir ci-dessous |
| **Contrat de destruction** | `Document::AnnounceRelease` neutralisée | `maillages enregistres avant : 1`, `apres : 1`, `CONTRAT DE DESTRUCTION ROMPU : 1 cle(s) pendante(s)`, **`EXIT=1`**. Avec l'annonce : `avant : 1`, `apres : 0`, `CONTRAT TENU`, `EXIT=0`. Et **sans fichier initial** : `REMPLACEMENT NON CONCLUANT`, `EXIT=1` — les **trois** verdicts sont atteignables |
| **Rétrécissement du vecteur lié** | répertoire **sans sous-répertoire** | `lignes, minimum : 1`, `maximum : 1`, `REPLI NON CONCLUANT`, **`EXIT=1`**. Sur `bench1000` : `de 1 a 1000 lignes`, `REPLI OK` |

⚠ **Le cas négatif des erreurs d'accès a trouvé un défaut réel, et c'est le plus instructif de
l'étape.** L'énumération passait `std::filesystem::directory_options::skip_permission_denied` —
un drapeau qui **fait rendre un itérateur vide et AUCUNE erreur** quand l'accès est refusé. Un
répertoire interdit s'affichait donc comme un répertoire **vide** : exactement le mode de panne que
la ligne en erreur existe pour empêcher, et que le plan interdit nommément. Mesuré avant correction :
`d1 [-] secret` (ouvert, zéro enfant) ; après : `d1 [!] secret`. **Le drapeau a été retiré des deux
énumérations** — arbre et liste.

⚠ **Le détecteur permanent de concordance `GL_VIEWPORT` a trouvé le second défaut, et il n'était
visible par aucun critère de l'étape 4.** Voir §1.3 : 1000 lignes dans la zone gauche faisaient
mesurer 18 826 px de haut à `#row`. `--dump-layout`, qui ne peuple aucun panneau, restait vert.
**Vérifié après correction** : avec 1000 puis 5002 lignes liées, `GL_VIEWPORT concordant`, rectangle
`x=240 y=180 w=760 h=620` **inchangé**, et `--readpixel 620 490` rend toujours **(25,128,204)**.

**La batterie de cas négatifs du pare-feu passe de 8 à 12 cas, et les douze rendent `error C1083` :**

| # | Cas négatif injecté | Cible bâtie |
|---|---|---|
| 1 | `#include "gl_wrapper.h"` depuis `core/file_catalog.cpp` | `peles_core` |
| 2 | `#include "gl_wrapper.h"` depuis `ui/ui_controller.cpp` | `peles_ui` |
| 3 | `#include "gl_wrapper.h"` depuis `platform/` | `peles_platform` |
| 4 | `#include <RmlUi/Core.h>` depuis `view3d/` | `peles_view3d` |
| 5 | `#include "app/application.h"` depuis `view3d/` | `peles_view3d` |
| 6 | `#include "app/application.h"` depuis `core/directory_tree.cpp` | `peles_core` |
| 7 | `#include "app/application.h"` depuis `ui/ui_controller.cpp` | `peles_ui` |
| 8 | `#include "platform/rml_host.h"` depuis `view3d/` | `peles_view3d` |
| **9** | `#include <RmlUi/Core.h>` depuis `core/directory_tree.cpp` | `peles_core` |
| **10** | `#include "ui_controller.h"` depuis `view3d/` | `peles_view3d` |
| **11** | `#include "scene_renderer.h"` depuis `ui/` | `peles_ui` |
| **12** | `#include "directory_tree.h"` depuis `platform/` | `peles_platform` |

**Six décisions prises pendant l'étape, à connaître :**

- **Le catalogue détypé vit dans `peles/core/`, pas dans `src/cgmesh/`.** Motif écrit en §5.
- **`RmlHost::Create` et `RmlHost::LoadDocument` sont deux étapes**, et le liage du modèle vit entre
  les deux. Contrainte de RmlUi, non documentée, trouvée par la mesure (§1.5).
- **Les identifiants de ligne sont ceux des NŒUDS, pas des rangs.** Le réservoir de nœuds ne relâche
  jamais une entrée : un identifiant gardé pendant un repli désigne encore le même répertoire après
  le dépli suivant. Le prix est la mémoire de ce qui a été ouvert, et il est borné par ce qui l'a été.
- **L'icône est une classe RCSS** (`kind-obj`, `kind-stl`, `kind-generic`), portée par
  `data-attr-class`. ⚠ **Conséquence qui n'est pas un détail : `data-attr-class` ÉCRASE l'attribut
  `class`.** Une ligne liée ne peut donc pas porter aussi une classe statique ; son style commun
  vient du sélecteur de son parent (`#dir-tree div`, `#file-list div`). Les trois bitmaps XPM, le
  `wxImageList` et sa mise à l'échelle n'ont aucun équivalent ici.
- **Quatre options nouvelles, toutes instrumentales sauf deux** : `--dump-tree` et `--dump-files`
  (oracles, **sans contexte GL ni assets**, traités par `main` avant toute création) ; `--browse` et
  `--browse-depth` (produit : où ouvrir l'arbre) ; `--reopen` et `--churn-tree` (instruments du
  contrat de destruction et du rétrécissement).
- **`--dump-tree` ouvre trois niveaux, et la borne est obligatoire.** Sans elle, `--dump-tree C:\`
  parcourrait le disque : un instrument doit terminer sur n'importe quelle entrée, pas seulement sur
  la fixture pour laquelle il a été écrit.
- **Les deux vidages sont dans `dump_modes.cpp`** (369 → 532 lignes), pas dans `application.cpp`.
  Celui-ci passe tout de même de 376 à 524 lignes — `PopulateBrowser`, `OpenModel` et les deux
  instruments de boucle — et `Run`, remontée à **73 lignes effectives**, a été redécoupée en
  `Run` / `Report` / `ChurnTreeOnce` / `ReopenIfRequested` : **32 / 29 / 8 / 9** effectives.
  `RenderFrame` reste en un bloc (45), la lecture linéaire des huit phases ayant sa valeur propre.

**Ce que ce verdict ne lève pas :**

- **La prédiction « sous ASan » n'a PAS été mesurée sous ASan.** Le rétrécissement du vecteur lié a
  été exercé cent fois, sans plantage, **sous le CRT de Release**. Une écriture hors bornes non
  fatale ne serait pas vue. La construction instrumentée reste à faire.
- **Le test de cohérence ne compare que ce que les macros de la configuration bâtie laissent voir.**
  Il a tourné avec `CG_HAS_ZLIB` seule active : une divergence sous `CG_HAS_OCCT`,
  `CG_HAS_OPENNURBS` ou `CG_HAS_LIB3MF` n'y serait pas visible **par construction**. La CI doit donc
  l'exécuter dans une configuration où ces gardes sont ouvertes, sinon la couverture annoncée est
  supérieure à la couverture réelle.
- **Le plafond de 2000 place le pire nœud unique à la limite des 16,6 ms**, mesuré. Le descendre est
  une décision du lot suivant.
- **Aucune interaction.** Rien ne clique : `Toggle` et `SelectDirectory` ne sont appelés que par
  l'hôte et par les instruments. Le routage d'entrées est l'étape 6, et **aucun `data-event-` n'a été
  posé** — il serait mort, `Context::ProcessMouse*` n'étant pas encore câblé.
- **Le contrat de destruction est tenu sur le chemin exercé**, pas sur tous : `--reopen` remplace la
  scène **une** fois, après **une** image. Une séquence longue d'ouvertures n'est pas mesurée.
- ⚠ **La branche « lecteur non prêt omis » n'est PAS exercée, et elle ne peut pas l'être ici** : la
  machine de référence n'a qu'un seul volume, `C:\`, et aucun lecteur optique ni amovible. Ce qui
  **est** mesuré, c'est que l'énumération suit les lecteurs réellement présents : un volume virtuel
  monté par `subst X:` fait passer l'arbre de 1 à 2 racines, et son retrait le ramène à 1. La
  bascule `SetThreadErrorMode(SEM_FAILCRITICALERRORS)` reste donc une **précaution non vérifiée
  expérimentalement** — elle est écrite pour la boîte de dialogue « Insérez un disque », que
  personne n'a vue ici.
- **R-P1 et R-P2 restent ouvertes**, inchangées.

**Mesures de non-régression, même machine :**

| Critère | Mesure |
|---|---|
| `peles_probe` | **7/7**, `EXIT=0` |
| `--assets <inexistant>` | `EXIT=3`, sans fenêtre |
| `--state-audit <maillage réel>` | `25 etat(s) releves, 3 image(s) rendue(s), 2 phases`, `AUDIT OK`, `EXIT=0`. Tenu aussi avec **1000 lignes liées** dans l'arbre |
| `--dump-layout 1400x900` | `x=240 y=0 w=880 h=720` ; `1401x900` → `w=881` ; `1400x901` → `h=721` |
| `--readpixel 620 490` | **(25,128,204)**, écart 1/255, `PIXEL CONFORME`, `EXIT=0`. Tenu **avec 1000 et 5002 lignes liées** |
| `ENABLE_CGGRAPH_BOILERPLATE=ON` | configure et bâtit `cggraph-boilerplate.exe`, **un seul** `glfw3.lib` |
| `TU` sous `ENABLE_SINAIA=OFF` | se bâtit ; `tu_supported_formats` compile à rien (0 test découvert) |

---

### Corrections de revue de l'étape 4, mesurées le 2026-09-22

**Verdict d'entrée : REQUEST CHANGES — 0 critical, 3 HIGH, 8 medium, 8 low.** Les cinq critères
d'acceptation passaient ; les trois HIGH portaient tous sur la même chose : **des gardes qui
n'existaient pas là où le code affirmait qu'elles existaient.**

Machine de référence, `GL_VERSION = "3.3.0 - Build 32.0.101.6078"`, binaire bâti par
`cmake --build build/peles-probe --config Release`.

#### Les trois HIGH

| # | Défaut | Correction | Cas négatif, mesuré |
|---|---|---|---|
| **F1** | Le test anti-divergence **ne tournait dans aucune configuration outillée**, alors que `file_catalog.h` écrivait « fait échouer la CI » et que §5 le nommait « la seule atténuation exécutable ». Sous `ENABLE_SINAIA=OFF` le fichier **disparaissait entièrement** : `ctest` vert, aucun signal | Preset `ci-linux-peles` + job CI du même nom, sur le modèle exact de `ci-linux-boilerplate` — **il bâtit `peles`/`peles_probe` et exécute `--dump-tree`/`--dump-files`, qui n'ouvrent aucun contexte GL ; il n'exécute NI rendu NI `tu_supported_formats`**, faute de wxWidgets sur un runner. Les deux commentaires disent désormais la vérité. `#else` + `GTEST_SKIP()` compilé **inconditionnellement** | Sous `ENABLE_SINAIA=OFF` : `[ SKIPPED ] SupportedFormats.MecanismeAntiDivergenceNonCompile`, message nommant les deux options. Sous `ENABLE_SINAIA=ON` : 2 tests `PASSED` |
| **F2** | Un `data-model` mal nommé rendait **quatre panneaux vides et `EXIT=0`**. RmlUi journalise et **poursuit** ; l'hôte ne lisait pas ce canal, alors qu'il traitait `#viewport` absent — même famille de faute — en `EXIT=3` | `SystemInterface` **compte** les messages ; `CreateUi` échoue en **`EXIT=3`** si le compteur est non nul, en citant le message de RmlUi. **Variante générale**, pas contrôle du seul attribut : les étapes 5 à 7 ajoutent du RML à chaque fois | `data-model="pelesx"` → **`EXIT=3`**, « Could not locate data model 'pelesx' ». `data-for="row : treeRowsX"` → **`EXIT=3`**, « Could not find variable name 'treeRowsX' ». **Faux positif exclu** : document correct, `--frames 2` et `--frames 2 --debugger` → `EXIT=0`, zéro message compté |
| **F3** | Deux instruments rendaient un verdict **vert** sur une exécution qui n'avait pas fait ce qui était demandé : `ReopenIfRequested` **jetait** le retour d'`OpenModel` (qui vide la scène **avant** de charger, donc un fichier illisible donne « avant = 1, après = 0 » — la signature exacte d'un contrat tenu) ; `ReportTreeChurn` ne comparait **jamais** les basculements au nombre demandé | `ReportSceneReplacement` prend `opened` **en premier terme** ; `ReportTreeChurn` prend le nombre **demandé** | `--frames 3 --reopen <inexistant> cube.obj` → `fichier de remplacement : NON OUVERT`, `REMPLACEMENT NON CONCLUANT`, **`EXIT=1`** ; avec un fichier valide → `CONTRAT DE DESTRUCTION TENU`, `EXIT=0`. `--frames 1 --churn-tree 100` → `REPLI NON CONCLUANT : 100 demandes, 1 effectues`, **`EXIT=1`** ; `--frames 100 --churn-tree 100` → `REPLI OK`, `EXIT=0` |

⚠ **Un écart assumé sur F2, et il est mesuré.** La revue prescrivait de ne compter que `LT_ERROR` /
`LT_ASSERT`. **Mesure du journal** : un `data-for` dont la source n'est pas liée produit
`Could not add data-for view to element` en **`LT_WARNING`**, pas en `LT_ERROR` — et son effet est un
panneau vide, exactement la panne visée. Le critère a donc été **élargi à `LT_WARNING`**, sur la
mesure du faux positif : sur le document du produit, `--frames 2` **et** `--frames 2 --debugger`
rendent **zéro** message de niveau `LT_WARNING` ou supérieur (seuls des `LT_INFO` de chargement de
police, débogueur compris). **Risque connu** : un futur RmlUi qui avertirait sur un document valide
rendrait la création fatale — le message cite alors la ligne de journal, donc le diagnostic est
immédiat.

#### Les plafonds — R-P4 levée, et les trois valeurs changent

**Instrument** : `peles --frames 120 --dump-timing --browse <dir> --browse-depth n`, qui mesure la
**seule phase 2** (`Rml::Context::Update()`) sur la première image — celle où les lignes sont
construites et mises en page.

| Grandeur | Avant | Après | Motif, mesuré |
|---|---|---|---|
| `kMaxChildrenPerNode` | 2000 | **1000** | 2002 lignes coûtent **16,1–18,5 ms**, soit **au-dessus** du repère de 16,6 ms. Un plafond dont le pire cas dépasse le budget ne borne rien d'utile |
| `kMaxFiles` | 5000 | **1000** | 5002 lignes coûtent **38–41 ms**, plus de deux périodes de 60 Hz. ⚠ **Statut : mesuré sur l'ARBRE, reporté ici par inférence** — l'instrument a été appliqué aux lignes d'arbre, pas de liste ; l'inférence tient à ce que les deux zones passent par le même `data-for` sur un vecteur lié, et ce qui la confirmerait est la même mesure sur un répertoire à plusieurs milliers de fichiers importables |
| `kMaxVisibleRows` | **n'existait pas** | **1000** | Le plafond par nœud **ne bornait pas la grandeur que la mesure fait varier** : le coût est linéaire dans le **total**, ≈ 7,7 µs/ligne |

**Mesures après correction, 3 exécutions de 120 images, synchronisation verticale coupée :**

| Cas | `Update()` 1ʳᵉ image | `Update()` médiane | Image médiane |
|---|---|---|---|
| 1001 lignes liées (plafond atteint) | **7,319 / 9,527 / 7,147 ms** | 0,066–0,070 ms | 0,788–0,800 ms |
| Régime `Toggle` à **chaque** image, plafond atteint (`--churn-tree 100`) | — | **7,299 ms** | **8,072 ms** |

Les deux tiennent sous le repère de 16,6 ms, ce qui n'était **pas** le cas à 2002 lignes.

⚠ **Le plafond global mord là où celui par nœud est aveugle, vérifié** : trois répertoires de
**400** sous-répertoires ouverts à la profondeur 2 — **aucun nœud n'atteint son propre plafond** —
produiraient 1204 lignes sans lui. Mesuré : `lignes : 1001`, dernière ligne
`d2     ... plafond de 1000 ligne(s) atteint, la suite n'est pas affichee`. Et `kMaxFiles` :
1200 fichiers `.obj` → `fichiers : 1000`, `non affiches : 200`.

#### Les autres medium et low

| # | Correction | Vérification |
|---|---|---|
| **M1a** | Le test de `ec` passe **entre l'incrément et la lecture suivante**, dans une boucle `while`. ⚠ **Ni « en tête de corps » ni « après la boucle » n'est correct** : libstdc++ remet l'itérateur à la fin dès qu'`advance` échoue (test en tête **jamais atteint**, erreur **perdue**), tandis que MSVC ne relâche l'implémentation que sur `_No_more_files` et laisse l'itérateur **différent de la fin** sur toute autre erreur (test après la boucle **jamais atteint non plus — boucle infinie**). Source MSVC lue : `<filesystem>`, `_Dir_enum_impl::_Advance_and_reset_if_no_more_files` | Les deux énumérations, arbre et liste |
| **M1b** | `entryEc` n'est plus ignoré : les entrées illisibles sont **comptées** et le compte devient une ligne (`... N entree(s) dont le statut n'a pas pu etre lu`) côté arbre, un message côté liste. ⚠ **Sans mettre le nœud en erreur** : le marquer le rendrait **non ouvrable**, donc masquerait les enfants qui, eux, se sont lus | — |
| **M1c** | `ec.message()` ajouté aux trois messages | `--dump-tree <inexistant>` → « (The system cannot find the file specified.) » ; `--dump-tree <sous icacls /deny du parent>` → « (Access is denied.) ». **Les deux cas ne rendent plus le même message** |
| **M2** | L'identité d'un fichier est un `std::filesystem::path` ; une chaîne n'est produite que pour **afficher**, en **UTF-8** (`core/path_text.{h,cpp}`). `FileList` porte `m_paths` indexé comme `m_rows` ; `DirectoryTree::Node::path`, `PathOf`, `SetRoot`, `FileList::Populate` et `UiController::ShowDirectory`/`SelectFile` prennent des `path` | ⚠ **Trois défauts fermés d'un coup** : les octets ne dépendent plus de la page de codes, la conversion n'est plus destructive pour l'identité, et elle **ne lève plus** — `PathToUtf8` rattrape et replie sur `'?'`. Fixture ASCII : **mêmes octets**, les deux oracles restent identiques |
| **M3** | Les **20** retours de `RegisterMember` et `Bind` sont agrégés ; un seul faux fait échouer le liage, et le modèle partiellement construit est **retiré** | Sans cela, `ui_dto.h:14-19` justifiait toute la structure des DTO par un refus que personne ne lisait |
| **M5** | Les deux « ESTIMATION, PAS MESURE » remplacés par le chiffre **et son instrument** | `directory_tree.h`, `file_list.h` |
| **M6** | Le raisonnement faux (« identiques **dans cette unité de traduction** ») corrigé : `sinaia::…` est `inline` et compilé **ici**, `peles::core::…` dans **`peles_core`** — ce qui tient l'égalité est l'héritage des définitions **`PUBLIC`** de `cgmesh`, qu'un `target_compile_definitions(peles_core PRIVATE …)` romprait **sans alerte**. La limite de macros est **dans le test** | Mesuré : `gardes ouvertes dans cette construction : CG_HAS_ZLIB` |
| **M7** | `#viewport { flex: 1 1 0px }` | La règle que `#row` énonce à `:59-60` |
| **M8** | `--dump-layout <taille> --browse <dir>` ajouté à la non-régression : `PopulateBrowser` court **avant** `Run`, donc la géométrie est mesurée **panneaux peuplés** | ✅ **Détecteur validé sur un contrôle positif.** Avec la forme historique (`#row { flex: 1 1 auto }`, `min-height` neutralisé) : `--dump-layout 1400x900 --browse <1200 sous-rép.>` rend **`h=18843`** là où `--dump-layout 1400x900` seul rend **`h=720`**. ⚠ **Et le cas que le tracker `GL_VIEWPORT` ne peut pas voir est reproduit** : à **800** lignes l'inflation vaut **15085 px**, donc **sous** `GL_MAX_VIEWPORT_DIMS` — le tracker rend `GL_VIEWPORT concordant` et **`EXIT=0`**, tandis que `--dump-layout --browse` rend `h=15085`. ⚠ **Neutraliser les neuf `min-*: 0px` seuls NE SUFFIT PAS** à faire enfler la mise en page : c'est `flex-basis: auto` sur `#row` qui est le terme nécessaire — les `min-*` sont une **condition, pas la condition** |
| **M9** | `test/tu_peles_core.cpp`, **hors** `CG_PELES_SINAIA_FORMATS` : 11 tests sous `ENABLE_PELES` seul. Les deux `cmp` d'oracle deviennent des **tests CMake** (`peles_dump_tree_oracle`, `peles_dump_files_oracle`) via `peles/cmake/CompareDumpOracle.cmake` — `cmake -E compare_files`, donc **octet pour octet** | ✅ **Les deux détecteurs validés sur leur cas négatif.** Départage de `NameLess` neutralisé → `DepartageParLesOctetsBrutsQuandLaCasseSeuleDiffere` **ÉCHOUE** (les 3 autres passent) ; un `zz.obj` ajouté à la fixture → `peles_dump_files_oracle` **ÉCHOUE** et nomme les deux fichiers. Fichiers restaurés, verts |
| **LOW** | `--dump-tree ""` refusé (`EXIT=2`, « valeur vide ») ; les deux vidages ensemble refusés (`EXIT=2`) — même traitement qu'un second fichier ; `std::make_unique` pour les deux interfaces RmlUi ; l'échec de `Rml::Debugger::Initialise` est **dit** ; la sélection de fichier vit dans `FileList::m_selected`, alignée sur `DirectoryTree` ; `ParseArguments` passe d'une chaîne de `else if` de **113 lignes** à une **table** `{nom, arité, action}` de 13 entrées, parcourue en boucle — `--help` reste hors table, étant la seule option qui **arrête** le programme | — |

**La batterie de cas négatifs du pare-feu passe de 12 à 14 cas, et les quatorze rendent `error C1083` :**

| # | Cas négatif injecté | Cible bâtie |
|---|---|---|
| **13** | `#include "rml_host.h"` depuis `ui/ui_controller.cpp` | `peles_ui` |
| **14** | `#include "view_model.h"` depuis `core/directory_tree.cpp` | `peles_core` |

⚠ **Le cas 13 est le plus tentant de tous** : `UiController::Bind` prend le `Rml::Context*` que seul
`RmlHost::Context()` fournit, et le raccourci naturel est de lui passer un `RmlHost&`.

**Mesures de non-régression, après correction :**

| Critère | Mesure |
|---|---|
| `peles_probe` | **7/7**, `EXIT=0` |
| `--assets <inexistant>` | `EXIT=3` |
| `--state-audit <maillage réel>` | `AUDIT OK`, **25 états**, 3 images, 2 phases, `EXIT=0` |
| `--dump-layout 1400x900` | `(240, 0, 880, 720)` ; `1401x900` → `w=881` ; `1400x901` → `h=721` |
| `--dump-layout 1400x900 --browse <fixture> --browse-depth 3` | **le même rectangle** |
| `--readpixel 620 490` | **(25,128,204)**, `PIXEL CONFORME`, `EXIT=0` |
| `--dump-tree` / `--dump-files` | **identiques octet pour octet** aux attendus versionnés, par `cmp` **et** par les deux tests CMake |
| `TU` sous `ENABLE_SINAIA=OFF` | se bâtit ; 11 `PelesCore*` **PASSED**, 1 `SupportedFormats` **SKIPPED** |
| `TU` sous `ENABLE_SINAIA=ON` | se bâtit ; **13 PASSED**, dont les 2 de `tu_supported_formats` |
| `ENABLE_CGGRAPH_BOILERPLATE=ON` | configure et bâtit `cggraph-boilerplate.exe` |

⚠ **Les deux fichiers attendus n'ont PAS été régénérés, et c'est un résultat, pas un oubli.** Ils sont
identiques **octet pour octet** après toutes les corrections : la fixture est très en deçà des
nouveaux plafonds, et elle est **ASCII pur**, donc le passage de la page de codes ANSI à l'UTF-8 ne
change aucun octet. C'est précisément ce que la limite ASCII de la fixture, désormais écrite dans
`tu_peles_core.cpp`, dit.

**Ce que ces corrections ne lèvent pas :**

- ⚠ **Le job `ci-linux-peles` n'a PAS été exécuté** : aucune machine Linux n'était disponible. Sa
  configuration YAML est validée (`yaml.safe_load`) et son preset aussi (`json.load` puis `cmake`),
  mais **la première exécution réelle reste à surveiller** — notamment le tirage réseau de RmlUi et
  de FreeType, et l'égalité octet pour octet des deux vidages produits par GCC.
- ⚠ **M1a est corrigé sur la base de la LECTURE des deux implémentations, pas d'un échec provoqué.**
  Faire échouer un `increment` en cours d'énumération demande de retirer le support pendant la
  lecture ; ni la branche libstdc++ ni la branche MSVC n'ont été exercées.
- ⚠ **M1b n'a pas été exercé non plus** : sous MSVC, `directory_entry` met en cache les attributs
  rendus par `FindFirstFile`, donc `is_directory` réussit même sur une entrée dont l'accès est
  refusé — le compteur `unreadable` reste à zéro sur tous les cas que la machine de référence sait
  construire.
- **La prédiction « sous ASan » n'est toujours pas mesurée sous ASan.**

---

---

### Verdict de l'étape 5, mesuré le 2026-09-22

**Les quatre critères d'acceptation passent**, avec **une réserve nommée sur le critère 2**.
Machine Windows de référence, `GL_VERSION = "3.3.0 - Build 32.0.101.6078"`, binaire bâti par
`cmake --build build/peles-probe --config Release`.

**Ce qui remplace `UpdatePropertiesGrid`** (`sinaia/SinaiaFrame.cpp:2172-2470`, 299 lignes) :
`core/model_info.{h,cpp}` (**513 l.**, dont environ la moitié de commentaire),
`core/topology_stats.{h,cpp}` (**79 l.**), **+29 lignes de RML** (45 → 74) et **+118 lignes de RCSS**
(251 → 369). **`MakeColorSwatch` (`:76-100`, 25 lignes de GDI) et la purge de la
`wxImageList` (`:2279-2282`) n'ont AUCUN équivalent** : la couleur est un champ `std::string swatch`
valant `"#rrggbb"`, consommé par `data-style-background-color`.

| # | Critère | Mesure |
|---|---|---|
| 1 | **Exactement 14 lignes**, dans l'ordre de `:2187-2201` + `Path`, **même sans fichier chargé** | `peles --dump-info` **sans argument** rend `proprietes : 14` et les quatorze libellés, valeurs vides. Test unitaire `PelesCoreModelInfo.UnModeleAbsentRendQuatorzeLignesDansLOrdre` : nombre **et** ordre comparés à une table littérale. Oracle CMake `peles_dump_info_oracle`, comparaison **octet pour octet** avec `test/data/peles/expected_dump_info.txt` |
| 2 | Valeurs numériques identiques à `sinaia` sur 3 fichiers de référence | ⚠ **NON COMPARÉ À `sinaia`** — voir la réserve ci-dessous. Les valeurs sont **dérivées de l'API `cgmesh`** et consignées dans `test/data/peles/expected_reference_info.txt` (125 lignes, trois fichiers) |
| 3 | Format des nombres identique (`%.4g`, `:2239, :2242`) | `PelesCoreModelInfo.LeFormatDesNombresEstCeluiDeLHoteHistorique` : `[ 0 , 1.157e+04 ]`, `1.157e+04`, `0.1235`. Relevé sur `Lantern.glb` : `Bounding box X : [ -3922 , 1.157e+04 ]` |
| 4 | Sur ≥ 50 matériaux, la mémoire ne croît pas sur trois sélections du même fichier | ⚠ **PASSE PARTIELLEMENT** — voir la réserve ci-dessous. `peles --reselect 3 test/data/peles/info/many_materials.obj` : `materiaux : 60`, `arbre des materiaux : complet`, **cinq entiers exacts stables** aux trois sélections — `cache 1`, `moteur 1`, **lignes liées `14/4/242`** — `AUCUNE ACCUMULATION INDEXEE PAR MAILLAGE NI PAR LIGNE LIEE`, `EXIT=0`. **Mais l'empreinte du processus CROÎT** (`+368 Kio` sur trois sélections, ≈ 100 Kio/sélection sur 22), sous une tolérance placée **au-dessus** d'elle : l'instrument imprime désormais `⚠ ECART NON RESOLU` et le dit lui-même |

#### ⚠ Le critère 4 ne passe que PARTIELLEMENT, et ce qui manque est nommé

**Correction de la revue de l'étape 5.** La version d'origine de cet instrument lisait `cacheEntries`
et `registeredMeshes` — **tous deux indexés par MAILLAGE** — plus l'empreinte à gros grain.
`many_materials.obj` porte **60 matériaux sur UN SEUL maillage** : les deux entiers valaient `1, 1, 1`
**quel que soit le nombre de matériaux**. **Le nombre « 50 » que le critère nomme n'entrait donc dans
aucune grandeur que le verdict lisait** ; il ne servait qu'à la garde de présence.

**Ce qui a été ajouté** : un **troisième étage exact, indexé par la grandeur du critère** — les tailles
des trois vecteurs liés (`infoRows`, `meshRows`, `materialRows`), relevées à chaque sélection et
comparées **sans tolérance**. Sur ce fichier, `materialRows` vaut **242**, donc il croît avec le nombre
de matériaux, ce qu'aucun des deux premiers ne faisait.

**Et l'ordre des deux étages a été inversé** : les cinq entiers exacts sont lus **avant** l'empreinte.
Dans l'ordre précédent, une accumulation **visible dans les compteurs** sortait sous
`EMPREINTE EN CROISSANCE : … sans que les deux registres indexés par maillage aient bougé —
l'accumulation est AILLEURS`, c'est-à-dire un **diagnostic faux** alors que l'instrument avait le bon
sous la main.

**Ce qui reste ouvert, et qui est un ÉCART NON RÉSOLU et non un bruit toléré** : la croissance de
l'empreinte, ≈ **100 Kio par sélection**, **indépendante du nombre de matériaux**, antérieure à
l'étape 5, de cause non établie. La tolérance (512 Kio/sélection) a été placée **au-dessus** d'elle :
elle la **tolère**, elle ne la nie pas. L'instrument l'imprime maintenant en clair à chaque exécution
verte. Ce qui la lèverait : une construction sous ASan, ou un profileur de tas.

#### ⚠ Le critère 2 n'a PAS été comparé à `sinaia`, et c'est dit ici parce que le plan l'exige

**Ce qui a été fait** : les valeurs sont dérivées de l'API `cgmesh`, et **deux des nombres sont
calculables à la main sur la fixture** — le test écrit la valeur littérale : `Borders = 9`
(tétraèdre fermé 0 + triangle isolé 3 + trois triangles sur une arête commune 6) et
`Non manifold edges = 1`. Le second garant est `expected_dump_info.txt`, dont **chacune des 14 valeurs
est recalculable** à partir du fichier de fixture.

⚠ **Correction de la revue de l'étape 5 : ce test n'est PAS un « recoupement indépendant », et
l'appeler ainsi était faux.** `ComputeTopologyStats` appelle `MeshDataManager::GetTopologicIssues`,
qui appelle `Mesh::GetTopologicIssues` — **la même fonction** que la boucle de contrôle du test. Ce
que la comparaison établit se réduit à la division par deux et à la somme sur les maillages. De même,
les onze comparaisons de valeurs qui suivent opposent `Text ("%u", x)` à `std::to_string (x)` :
**elles testent la mise en forme, pas la donnée.** C'est l'**étiquette** qui était fausse, pas la
méthode : ce qui porte réellement l'indépendance, ce sont les deux nombres calculés à la main et
l'oracle versionné. Le commentaire du test dit désormais cela.

**Ce qui n'a PAS été fait** : bâtir `sinaia` avec wxWidgets, l'ouvrir sur les trois fichiers et lire
les treize valeurs de son `wxPropertyGrid`. **Cela demande un opérateur** — le panneau est une
interface graphique, et rien dans `sinaia` ne l'imprime. La configuration `ENABLE_SINAIA=ON` a été
bâtie (elle l'est pour `tu_supported_formats`), mais **l'exécutable n'a pas été ouvert**.

**Conséquence exacte, sans l'atténuer** : ce qui est établi est que la mise en forme de `peles` ne
perd ni ne transforme les valeurs que `cgmesh` rend, et que le format des nombres est celui du code
de `sinaia` **lu ligne à ligne**. Ce qui n'est pas établi est que l'hote historique **affiche** bien
ces valeurs-là — un écart introduit par `sinaia` lui-même resterait invisible.
**Une divergence est d'ailleurs connue et documentée** : `GetNBorders`/`GetNNonManifoldEdges`
(`wxOpenGLCanvas.cpp:2172-2192`) somment sur **tous les modèles de la scène** alors que le panneau
décrit le modèle **sélectionné** ; `peles` calcule sur le modèle désigné. **Les deux coïncident tant
qu'un seul fichier est ouvert**, ce qui est le cas du lot 1, et divergent dès le second.

#### Les trois fichiers de référence

| Rôle | Fichier | Versionné ? |
|---|---|---|
| `.obj` texturé | `test/data/obj/cube-tex.obj` | oui |
| `.glb` PBR | `Lantern.glb`, à la racine | ⚠ **NON** — il est dans `.gitignore`/non suivi. `expected_reference_info.txt` conserve donc ses valeurs, mais aucun test automatisé ne peut les recalculer |
| multi-maillages | `test/data/obj/multi_objects.obj` | oui |

`Lantern.glb` exerce les cinq emplacements de carte PBR et la branche `MATERIAL_PBR` ;
`cube-tex.obj` la branche `MATERIAL_TEXTURE` avec son image réellement décodée (`Size: 320 x 320`) ;
`multi_objects.obj` le cas **sans aucun matériau** — l'arbre se réduit alors à sa seule racine.

#### Les deux fixtures de synthèse, et pourquoi elles existent

**Aucun modèle du dépôt ne porte 50 matériaux ou plus** — vérifié sur `test/data/obj/`,
`test/data/peles/fixture/` et les trois `.glb` de la racine, dont le plus riche (`Lantern.glb`) en
porte **3**. Deux fichiers ont donc été **engendrés et versionnés** :

- **`test/data/peles/info/panels.obj` + `.mtl`** — trois objets dont **aucun n'est décoratif** :
  un tétraèdre fermé (0 bord), un triangle isolé (3 bords), trois triangles sur une arête commune
  (1 arête non manifold, 6 bords). ⚠ **Sans le troisième, la ligne « Non manifold edges » vaudrait
  zéro quelle que soit l'implémentation du compteur** : elle serait affichée sans être vérifiée.
  C'est la fixture de l'oracle `--dump-info`.
- **`test/data/peles/info/many_materials.obj` + `.mtl`** — **60 matériaux sur un seul maillage**,
  couleurs sur une rampe pour qu'aucune pastille ne soit répétée. C'est la forme du cas que `sinaia`
  cite (« sur un modèle à 98 matériaux, ça se voit vite »), et **c'est celle qui fait grossir une
  liste d'images à chaque rechargement**. 242 lignes de matériaux.

#### ⚠ Le critère 4 n'est PAS jugé sur l'empreinte mémoire du processus, et c'est une mesure

**Premier instrument essayé** : `PrivateUsage` (Windows) relevé après chaque sélection. **Il ne peut
pas trancher, et le chiffre le dit** :

| Fichier | Matériaux | Croissance sur 22 sélections |
|---|---|---|
| `panels.obj` | 3 | **+2524 Kio** puis **+2276 Kio** |
| `many_materials.obj` | 60 | **+1508 Kio** puis **+2424 Kio** |

**Le fichier à 3 matériaux croît autant que celui à 60.** La croissance n'est donc pas dans le
panneau de matériaux, et deux exécutions de la **même** ligne de commande rendent 1508 puis
2424 Kio : **un verdict à tolérance serrée n'aurait mesuré que le hasard**. Cette croissance est
**antérieure à l'étape 5** ; sa cause n'a pas été établie, elle est déclarée ouverte ci-dessous.
Sur **3** sélections — le nombre que le critère nomme — elle vaut **0 à 560 Kio**, mesuré sur cinq
exécutions.

**Instrument retenu, en deux étages :**

1. **Deux entiers exacts, sans tolérance** — le nombre d'entrées vivantes dans les caches de
   `MeshDataManager` et le nombre de maillages enregistrés auprès de `MeshRenderer`. **C'est
   l'équivalent exact de la `wxImageList` que `sinaia` devait purger à la main.**
2. **Un garde-fou à gros grain sur l'empreinte**, `512 Kio × (nombre d'intervalles)`. ⚠ **Il existe
   parce que l'étage 1 est aveugle à tout ce qui n'est pas indexé par maillage**, et le cas négatif
   exigé par le plan — une allocation qui fuit par sélection — est exactement de cette forme.
   **Proportionnel et non fixe** : une valeur fixe deviendrait de plus en plus sévère à mesure que
   `--reselect` grandit, donc l'instrument se contredirait selon son propre argument.

⚠ **Ce que l'étage 2 ne vaut PAS, et il faut le lire avant de s'y fier.** Il y a **deux ordres de
grandeur** entre le bruit et une fuite franche, et la tolérance est placée entre les deux : il est
donc **aveugle à une fuite de quelques dizaines de kibioctets par sélection**.

⚠ **Le relevé est pris à l'image QUI SUIT la sélection, pas à celle qui la porte.** Un maillage n'est
enregistré auprès du moteur qu'à son **premier dessin** : relevé dans l'image du chargement, le
compteur valait **0 à chaque sélection** — identique d'une fois sur l'autre, donc vert, sans avoir
rien observé. C'est pourquoi `--reselect n` consomme `2n + 1` images.

#### ⚠ Un couplage de `cgmesh` fermé, et il n'était dans aucune liste

`MeshDataManager` est un **cache global indexé par `Mesh*` que personne n'invalidait** :
`Invalidate` avait **zéro appelant** dans tout le dépôt (`grep` sur `src/`, `sinaia/`, `peles/`,
`test/`). L'étape 5 est la première à peupler ce cache depuis `peles` — `topology_stats` l'interroge
— donc la première pour qui il fuit. `Document::AnnounceRelease` l'appelle désormais, à côté de
l'annonce aux observateurs.

⚠ **Ce que le cas négatif a montré, et qui est pire qu'une fuite** : `Invalidate` neutralisée, le
compte passe de `2` à `3` puis **cesse de croître** — l'allocateur réattribue les adresses des `Mesh`
détruits, et les nouvelles entrées **retombent sur les clés des anciennes**. Un maillage rechargé
hérite donc de la topologie de son prédécesseur, **silencieusement**. La fuite est le symptôme
bénin ; la collision de clés est le vrai défaut.

**`src/cgmesh/mesh_data_manager.h` reçoit un accesseur `CachedEntryCount()` const** — trois lignes,
purement additives. ⚠ **C'est le seul fichier hors de `peles/` que cette étape modifie**, et c'est
assumé : sans lui, la grandeur que le critère 4 mesure n'est pas observable depuis l'hôte.

#### Les six détecteurs, chacun validé sur son cas négatif

| Détecteur | Cas négatif injecté | Sortie |
|---|---|---|
| **Critère 1 — ligne retirée** | `rows.push_back (Property ("Size Z", …))` neutralisée | `UnModeleAbsentRendQuatorzeLignesDansLOrdre` **ÉCHOUE** (13 ≠ 14), et deux autres tests avec ; `peles_dump_info_oracle` **ÉCHOUE** en nommant les deux fichiers |
| **Critère 1 — ordre permuté** | `Faces` et `Meshes` échangées | `UnModeleAbsentRendQuatorzeLignesDansLOrdre` **ÉCHOUE** au rang 2. ⚠ **Le compte, lui, reste à 14** : c'est ce cas qui établit que le test n'est pas un test de cardinal |
| **Critère 1 — assertion CI** | même flux privé de la ligne `Size Z` | `grep -c '^[A-Z]'` rend **13**, le `test` de la CI échoue |
| **Critère 4 — accumulation indexée par maillage** | `MeshDataManager::Invalidate` neutralisée dans `Document::AnnounceRelease` | `cache 2 → 3 entree(s)`, `CACHE EN CROISSANCE`, **`EXIT=1`**. Restauré : `1, 1, 1`, `AUCUNE ACCUMULATION`, `EXIT=0` |
| **Critère 4 — fuite d'allocation** | `(void)new char[8 << 20];` par sélection, dans `ReselectOnce` | `empreinte : +16428 Kio (tolerance 1024 Kio)`, `EMPREINTE EN CROISSANCE`, **`EXIT=1`**. ⚠ **Les deux compteurs exacts, eux, restent à `1, 1, 1` : c'est ce cas qui établit que l'étage 1 seul ne suffisait pas** |
| **Journal RmlUi, à la création** | `data-for="row : materialRowsX"` dans le RML neuf | `EXIT=3`, « Could not find variable name 'materialRowsX' in data model » |
| **Journal RmlUi, à L'EXÉCUTION** | valeur par défaut de `InfoRow::swatch` ramenée à la chaîne vide | **122 avertissements**, `EXIT=3`. ⚠ **Avant correction : `EXIT=0`** — voir ci-dessous |
| **Pare-feu d'inclusion** | `#include "gl_wrapper.h"` depuis `core/model_info.cpp` ; `#include <RmlUi/Core.h>` depuis `core/topology_stats.cpp` | `error C1083` sur `peles_core`, les deux |

#### ⚠ Un trou du mécanisme de l'étape 4, trouvé par la mesure et bouché

Le compteur de messages RmlUi de `CreateUi` (F2 de l'étape 4) **ne voit que ce qui est refusé au
chargement du document**. Une valeur **liée** que RCSS ne sait pas analyser n'est refusée qu'à la
**première mise à jour**, donc **après** ce contrôle.

**Mesuré** : avec `InfoRow::swatch` vide, RmlUi journalise
`Syntax error parsing inline property declaration 'background-color: ;'` — **122 fois** — et le
programme sortait en **0**. `Application::Report` relit désormais le journal après la boucle et sort
en **3**, même code et même famille de cause que le contrôle de création.
**Mesure du faux positif** : sur le document du produit, `--frames 2`, `--frames 60`,
`--frames 2 --debugger` et `--frames 10 --churn-tree 5` rendent **zéro** message de niveau
`LT_WARNING` ou supérieur.

⚠ **C'est aussi la raison pour laquelle `InfoRow::swatch` vaut `"transparent"` et jamais la chaîne
vide**, contrairement à ce que §1.6 écrivait (« `"#rrggbb"` ou vide »). Le champ est lu par
`data-style-background-color` à chaque image ; une chaîne vide est une valeur que RCSS refuse.

#### `--dump-info` prend son fichier dans l'argument POSITIONNEL, pas dans une valeur d'option

⚠ **Écart au plan, et il est forcé par le critère 1 lui-même.** `--dump-info <fichier>` avec une
valeur obligatoire rend le cas « même sans fichier chargé » **inatteignable** : `ParseString` refuse
la valeur vide (correction LOW de l'étape 4, et elle est juste). Le mode est donc un **drapeau**, et
le fichier est l'argument positionnel que `peles` accepte déjà — optionnel par construction.
`peles --dump-info` seul rend les quatorze lignes vides et **`EXIT=0`** ; `peles --dump-info
<illisible>` rend **les quatorze lignes aussi**, puis `EXIT=2` et un message nommant le fichier.
Les trois vidages sont **mutuellement exclusifs** (`EXIT=2`), comme `--dump-tree` et `--dump-files`
l'étaient entre eux.

#### La géométrie des panneaux, mesurée — la seule preuve qu'ils ne sont pas vides

Une option `--layout-element <id>` a été ajoutée : `--dump-layout` imprime le rectangle résolu de
**n'importe quel** élément, et non plus du seul `#viewport`. ⚠ **Sans elle, rien dans le lot ne
distingue un panneau mis en page d'un `data-for` qui n'engendre rien** — l'étape 4 a montré qu'un
document vide rend tous les instruments verts.

| Élément | Sans modèle | Avec `many_materials.obj` |
|---|---|---|
| `#panel-right` | 280 × 720 | 280 × 720 |
| `#propgrid` | **280 × 234** | **280 × 234** |
| `#mesh-tree` | 280 × 4 | 280 × 70 |
| `#material-tree` | 280 × 4 | 280 × **4008** |

**Deux lectures de ces chiffres :**
- **234 px pour 14 lignes, soit 16,7 px par ligne, et la MÊME valeur avec et sans modèle.** C'est la
  mesure directe de l'invariant que `BuildEmptyModelInfo` existe pour tenir : **la disposition ne
  saute pas à la désélection**. Et 16,7 px par ligne établit que libellé et valeur sont sur la
  **même ligne** — deux colonnes, donc ; empilés, la table ferait le double.
- **4008 px de contenu dans une zone de 720 px**, et `--dump-layout 1400x900` rend toujours
  `(240, 0, 880, 720)`. C'est `min-height: 0px` + `overflow: hidden` de §1.3 qui tiennent, sur un
  débordement six fois supérieur à la hauteur de la zone.

⚠ **Conséquence assumée, non corrigée dans ce lot** : les lignes au-delà de 720 px sont **coupées et
inatteignables** — `#panel-right` est en `overflow: hidden` et le lot 1 n'a aucune barre de
défilement. Sur 60 matériaux, l'arbre n'est visible qu'à son début. **Le contenu est complet dans le
modèle** (242 lignes, `--dump-info` les imprime toutes) ; c'est son **affichage** qui est tronqué.

#### Trois décisions prises pendant l'étape

- **`table-layout: fixed` n'est PAS écrit dans le RCSS.** RmlUi ne connaît pas cette propriété — sa
  mise en page de table se comporte toujours comme la variante fixe de CSS. ⚠ **L'écrire produirait
  `Syntax error parsing property declaration` en `LT_WARNING`, donc `EXIT=3`** : la règle prescrite
  par §1.6 aurait fait échouer la création. Vérifié par lecture de
  `Source/Core/StyleSheetSpecification.cpp:307` puis par la mesure du journal.
- **La ligne `Path` est un `<textarea>` SANS attribut `readonly`.** ⚠ **RmlUi 6.3 n'implémente pas
  `readonly`** — seul `disabled` existe (`Source/Core/Elements/ElementFormControl.cpp:30`), et il
  retire le focus, donc la sélection avec lui. Écrire `readonly` aurait posé un attribut inerte.
  **La ligne est donc éditable localement**, et une édition est effacée à la republication suivante.
  ⚠ **Et la copie ne sort pas de RmlUi** : `SystemInterface::SetClipboardText` n'est pas surchargée,
  donc `Ctrl+C` écrit dans le presse-papier interne de RmlUi, pas dans celui du système. **Aucune
  entrée n'est d'ailleurs routée avant l'étape 6** : au terme de l'étape 5, cette ligne n'est
  **ni sélectionnable ni copiable en pratique**. La régression D-5 est donc **plus large que
  §1.6 ne l'annonçait**, et la refermer demande deux choses nommées — le routage clavier (étape 6)
  et une surcharge de `SetClipboardText`.
- **Les libellés de détail perdent l'indentation par espaces de `sinaia`** (`"  Diffuse: …"`) au
  profit du champ `depth` et des classes `.d0`…`.d4`. Le **format des nombres**, lui, est conservé à
  la lettre — `%.2f`, `%.3f`, `%.0f`, `%u x %u`. La comparaison textuelle exigée par le critère 3
  porte sur les valeurs, pas sur la gouttière.

#### Mesures de non-régression

| Critère | Mesure |
|---|---|
| `peles_probe` | **7/7**, `EXIT=0` |
| `--assets <inexistant>` | `EXIT=3` |
| `--state-audit <maillage réel>` | `AUDIT OK`, **25 états**, 3 images, 2 phases, `EXIT=0` |
| `--dump-layout 1400x900` | `(240, 0, 880, 720)` ; `1401x900` → `w=881` ; `1400x901` → `h=721` |
| `--dump-layout 1400x900 --browse <fixture> --browse-depth 3 <60 matériaux>` | **le même rectangle**, panneaux gauche, bas **et droit** peuplés |
| `--readpixel 620 490` | **(25,128,204)**, `PIXEL CONFORME`, `EXIT=0` |
| `--dump-tree` / `--dump-files` | identiques octet pour octet aux attendus, par `cmp` **et** par les tests CMake |
| `--dump-info` | identique octet pour octet sur deux exécutions, et à `expected_dump_info.txt` (test CMake `peles_dump_info_oracle`) |
| `--reopen` | `CONTRAT DE DESTRUCTION TENU`, `EXIT=0` |
| `--churn-tree 100` sur 100 images | `REPLI OK`, de 1 à 7 lignes, `EXIT=0` |
| `--dump-timing`, 120 images, **242 lignes de matériaux liées** | image médiane **0,679 ms**, `Update()` médiane **0,034 ms** |
| `TU` sous `ENABLE_SINAIA=OFF` | **1865 PASSED**, 1 SKIPPED (`SupportedFormats`) ; **19 `PelesCore*`** dont les 8 neufs |
| `TU` sous `ENABLE_SINAIA=ON` | **21 PASSED** sur le filtre `PelesCore*:SupportedFormats*`, dont les 2 de `tu_supported_formats` |
| `ENABLE_CGGRAPH_BOILERPLATE=ON` | configure et bâtit `cggraph-boilerplate.exe` **et** `peles`, un seul `glfw3.lib` |

#### Ce que ce verdict ne lève pas

- **Le critère 2 n'a pas été comparé à `sinaia`.** Réserve détaillée ci-dessus ; elle se lève en
  ouvrant `sinaia` sur les trois fichiers et en relevant les treize valeurs à l'écran.
- **La croissance de l'empreinte du processus sur sélections répétées est RÉELLE et sa cause n'est
  pas établie.** Mesurée à ≈ 100 Kio par sélection en moyenne sur 22 sélections, **indépendante du
  nombre de matériaux**, donc hors du périmètre de l'étape 5. Les deux registres indexés par maillage
  sont écartés — ils sont stables au relevé exact. Elle passe sous la tolérance à gros grain, ce qui
  veut dire que **le critère 4 la tolère au lieu de la nier**. Ce qui la lèverait : une construction
  sous ASan, ou un profileur de tas.
- ⚠ **Le garde-fou d'empreinte reste aveugle à une fuite de l'ordre du kibioctet par sélection**, et
  il le restera : deux ordres de grandeur séparent le bruit mesuré d'une fuite franche, et la
  tolérance est entre les deux. **Ce qui a changé après la revue** : cette bande n'est plus le seul
  filet. Les trois compteurs de lignes liées la couvrent pour tout ce qui passe par les vecteurs du
  modèle — **mesuré**, une accumulation de 4 lignes par sélection (+304 Kio sur trois, donc **dans**
  la bande aveugle) rend désormais rouge. Ce qui reste hors de portée : une allocation qui ne passe
  ni par un maillage, ni par une ligne liée, ni par un mébioctet.
- **L'arbre des matériaux déborde sa zone sans barre de défilement** — mesuré, 4008 px pour 720, soit
  **82 % inatteignable**. Désormais inscrit **en §1.6** (le cahier des charges du remplacement) **et**
  comme critère (5) d'acceptation de l'étape 6.
- **Aucune interaction.** Rien ne clique, rien ne tape : `ShowModelInfo` n'est appelé que par l'hôte.
  La ligne `Path` n'est donc **pas** sélectionnable au terme de cette étape.
- **Le job `ci-linux-peles` n'a toujours PAS été exécuté** : aucune machine Linux disponible. Les
  deux commandes `--dump-info` ajoutées y sont validées **par leur équivalent local** (`cmp` et
  `grep -c`, tous deux exercés y compris sur leur cas négatif), et le YAML est validé
  (`yaml.safe_load`). ⚠ **L'égalité octet pour octet de `expected_dump_info.txt` sous GCC reste à
  surveiller** : elle repose sur `%.4g`, dont la sortie est spécifiée par la bibliothèque C.
- **`expected_reference_info.txt` n'est comparé par aucune commande** : l'un de ses trois fichiers,
  `Lantern.glb`, n'est pas versionné. C'est une **mesure consignée**, pas un oracle exécuté.
- **R-P1 et R-P2 restent ouvertes**, inchangées.

---

### Corrections de la revue de l'étape 5, mesurées le 2026-09-22

Revue rendue **REQUEST CHANGES** : 0 critique, **3 HIGH**, 9 medium, 8 low. Même machine, même
binaire, `cmake --build build/peles-probe --config Release`.

#### Les trois HIGH

| # | Défaut | Correction | Cas négatif, **mesuré** |
|---|---|---|---|
| **F1** | Le critère 4 ne pouvait pas échouer pour le défaut qu'il nomme : ses deux entiers exacts sont indexés par **maillage**, et le fichier de test porte 60 matériaux sur **un** maillage | **Troisième étage exact**, indexé par la grandeur du critère : `infoRows`/`meshRows`/`materialRows`, comparés **sans tolérance**. Étage exact **avant** l'étage à gros grain. Verdict requalifié : `AUCUNE ACCUMULATION INDEXEE PAR MAILLAGE NI PAR LIGNE LIEE`, suivi d'un `⚠ ECART NON RESOLU` sur l'empreinte | **(a)** purge de `materialRows` omise + ajout au lieu de remplacement : `242 → 484 → 726 → 968`, `LIGNES DE MATERIAUX EN CROISSANCE`, **`EXIT=1`**. **(b)** même faute sur `meshRows`, **4 lignes par sélection** : empreinte `+304 Kio`, donc **sous la tolérance de 1024** — c'est-à-dire **dans la bande que l'ancien instrument ne voyait pas** — et pourtant `LIGNES DE MAILLAGES EN CROISSANCE : 8 → 12`, **`EXIT=1`** |
| **F2** | `BuildMeshTree` tronquait **en silence** : trois `break` nus, aucun `PushOverflow`, là où `BuildMaterialTree` en pose un. Le même mode de panne que le répertoire sous `icacls /deny` qui s'affichait *vide*, traité comme un vrai défaut à l'étape 4 | Symétrisé sur `BuildMaterialTree` ; `PushOverflow` prend l'unité en paramètre (`maillage(s)` / `materiau(x)`), son libellé n'est plus en dur. **Deux tests** construisent le modèle **en mémoire** pour atteindre le plafond — aucune fixture du dépôt ne l'approche | `PushOverflow` neutralisée dans `BuildMeshTree` : `LArbreDesMaillagesAnnonceSaTroncature` **ÉCHOUE** (999 ≠ 1000) |
| **F3** | `--dump-layout` court-circuitait le contrôle du journal RmlUi, alors qu'il appelle `Update()` — le moment exact où ces messages sont émis — et que `--layout-element` est **la seule preuve falsifiable qu'un panneau a du contenu mis en page** | `int Application::ReportRmlJournal () const` extrait, appelé sur **les deux** chemins de sortie de `Run()`. **Règle écrite** dans `application.h` : *aucun chemin de sortie de `Run()` ne rend un code sans avoir relu le journal* | `InfoRow::swatch` ramené à la chaîne vide. **Avant** : `--frames 2` → 122 avertissements, `EXIT=3` ; `--dump-layout --layout-element material-tree` → `280 x 4008`, **`EXIT=0`, pas un mot**. **Après**, même binaire : les **deux** rendent `122 avertissement(s)` et **`EXIT=3`**. Restauré : **`EXIT=0`** des deux côtés |

⚠ **Les trois autres vidages ne sont PAS concernés, et c'est vérifié par lecture** : `--dump-tree`,
`--dump-files` et `--dump-info` sont traités par `main` **avant toute création d'`Application`**
(`main.cpp:417-424`). Ils n'ouvrent aucun contexte RmlUi, n'appellent jamais `Update()`, et il n'y a
donc **pas de journal à relire**. C'est écrit à côté de la règle.

⚠ **`CarConcept.glb` et `CarbonFibre.glb` ont été passés à `--dump-info`**, comme la revue le
demandait. **Aucun n'approche le plafond** : `CarbonFibre.glb` rend `lignes maillages : 4`,
`lignes materiaux : 8` ; `Lantern.glb` rend `10` et `28`. **`CarConcept.glb` n'est pas lisible par
`cgmesh`** — `format inconnu, fichier illisible ou sans geometrie`, `EXIT=2`, les quatorze lignes
vides imprimées. C'est une **donnée nouvelle**, sans rapport avec l'étape 5. Elle confirme au passage
qu'aucun fichier du dépôt n'exerce le plafond, donc que la construction en mémoire était le seul
chemin.

#### Les neuf medium

| # | Correction |
|---|---|
| **M1** | **Étiquette corrigée, méthode inchangée.** Le « recoupement indépendant » du critère 2 n'en était pas un : `ComputeTopologyStats` → `MeshDataManager::GetTopologicIssues` → `Mesh::GetTopologicIssues`, c'est-à-dire la fonction que le test appelle lui-même. Ce qui porte l'indépendance, ce sont les **deux nombres calculés à la main** et `expected_dump_info.txt`. Corrigé dans le test **et** dans ce document |
| **M2** | **`Swatch` borne le FLOTTANT avant de convertir.** L'ancienne forme bornait l'entier **après** la conversion : `[conv.fpint]/1` en fait un comportement indéfini. **MESURÉ sur cette machine** (x86-64, MSVC) : l'ancienne forme rend `#000000` pour `RGB(nan, inf, -inf)` et `#000080` pour `RGB(1e30, -1e30, 0.5)` — l'infini sort **noir**. La nouvelle rend `#00ff00` et `#ff0080`. Test étendu à `NaN`, `±inf`, `±1e30f` ; il **échoue** sur l'ancienne forme |
| **M3** | **La classe de fautes `DirtyVariable` est supprimée, pas surveillée.** `DataModel::DirtyVariable` ne fait qu'un `RMLUI_ASSERTMSG`, **compilé à rien sous `NDEBUG`** : un écart entre le nom lié et le nom sali fige le panneau, sans erreur ni journal. Neuf `inline constexpr const char*` dans `ui/view_model.h`, employés par `view_model.cpp` **et** `ui_controller.cpp` : une faute de frappe cesse de compiler. Le commentaire d'`application.cpp` qui promettait une couverture générale nomme désormais **les deux familles qui lui échappent** |
| **M4** | **`ReadProcessFootprintKb` lit la même grandeur des deux côtés.** La branche Linux lisait le second champ de `/proc/self/statm`, c'est-à-dire le **RSS** — exactement ce que le commentaire de la branche Windows rejette. Elle lit maintenant **`VmData`** dans `/proc/self/status`, pendant Linux de la mémoire privée validée. Et `ProcessFootprintName ()` fait **nommer la grandeur dans la ligne de sortie** : `empreinte du processus (memoire privee validee, PrivateUsage)`. ⚠ **La branche Linux n'a PAS été exécutée** — aucune machine Linux — elle est corrigée par lecture |
| **M5** | **Le chemin de troncature de `BuildMaterialTree` est exercé.** `LArbreDesMateriauxAnnonceSaTroncatureSansDecalage` construit **501 matériaux** sur un maillage et vérifie la ligne de débordement **et** l'exactitude du nombre annoncé |
| **M6** | **La perte du défilement est entrée en §1.6** — le cahier des charges du remplacement — **et** comme critère (5) d'acceptation de l'étape 6, avec la mesure (4008 px dans 720, 82 % inatteignable) |
| **M7** | `debt_cgmesh.md` : les trois contradictions levées (voir ci-dessous) |
| **M8** | **Les indices magiques ont disparu.** `enum class InfoLine` dans `model_info.h`, table `kInfoLineSpec` indexée par lui, **trois `static_assert`** — cardinal, et consécutivité des trois axes dont la boucle de boîte englobante dépend. `BuildEmptyModelInfo` **boucle sur la table** au lieu de pousser quatorze lignes à la main : le contrat n'est plus tenu à deux endroits, il l'est à un seul |
| **M9** | **(a)** `LaDestructionDUnModeleRendSesClesAuCacheDeCgmesh` : `core::Document`, chargement, `CachedEntryCount () > 0`, `Clear ()`, retour à **0**. C'est le même chemin que `--reselect` — `AnnounceRelease` — **sans GL**, donc exécutable par CTest et par la CI. **(b)** La suite `PelesCoreModelInfo` est passée en `TEST_F` avec un `TearDown` qui appelle `MeshDataManager::Clear ()` : le binaire `TU` n'accumule plus de clés pendantes. ⚠ **Le placement d'`Invalidate` n'a pas été touché**, la revue le juge juste |

#### Les huit low

| # | Correction |
|---|---|
| **L1** | `src/cgmesh/mesh_data_manager.h` : le bloc français est passé en anglais, comme le reste du fichier |
| **L2** | `Text ()` ne tronque plus en silence : au-delà du tampon de 512 octets, la chaîne porte `...(tronque)`. Test `UnNomPlusLongQueLeTamponEstCoupeVISIBLEMENT` sur un chemin de texture de 604 octets ; **cas négatif** — marqueur retiré — le test **échoue** |
| **L3** | Décalage d'une unité supprimé : un matériau **dont la ligne de nom a été poussée** n'est plus compté parmi les absents. Exercé par le test de M5 ; l'ancienne forme y **échoue** (3 au lieu de 2) |
| **L4** | `--dump-layout` entre dans l'exclusion mutuelle des vidages : c'est aussi un mode « vider et sortir », et demandé avec un autre il n'était **jamais atteint** |
| **L5** | La ligne de débordement **tient dans** le plafond. `kMaxInfoContentRows = kMaxInfoRows - 1` lui **réserve sa place d'avance** : la retirer après coup rendrait le compte annoncé faux, la dernière ligne pouvant être le **nom** d'un élément compté comme affiché. Les deux tests de troncature vérifient `rows.size () == kMaxInfoRows` **exactement** |
| **L6** | `RmlHost::ResetCounts` et `SystemInterface::ResetCounts` **supprimées** — zéro appelant, le même anti-motif qu'`Invalidate`. Le commentaire de `FirstProblem` disait « depuis la dernière remise à zéro » : il dit maintenant « depuis la création de l'hôte », et la raison de ne **pas** remettre à zéro est écrite |
| **L7** | `model_info.h` : la référence `Model&` non `const` est attribuée à sa cause — `VMeshes::GetMeshes ()` n'a **pas de surcharge `const`** — contrainte héritée, pas un oubli |
| **L8** | `LoadModel` ne sélectionne que si rien n'est sélectionné : inscrit comme critère (6) de l'étape 6, avec le scénario qui le ferait échouer (ouvrir A puis B, la ligne `File` doit dire **B**) |

#### Mesures de non-régression après correction

| Critère | Mesure |
|---|---|
| `peles_probe` | **7/7**, `EXIT=0` |
| `--assets <inexistant>` | `EXIT=3` |
| `--state-audit` | `AUDIT OK`, **25 états**, 3 images, 2 phases, `EXIT=0` |
| `--dump-layout 1400x900` | RmlUi `(240, 0, 880, 720)` ; `1401x900` → `w=881` ; `1400x901` → `h=721` |
| `--dump-layout 1400x900 --browse <fixture> --browse-depth 3 <60 matériaux>` | **le même rectangle**, `EXIT=0` |
| `--readpixel 620 490 test/data/obj/cube.obj` | **(25,128,204)**, écart 1/255, `PIXEL CONFORME`, `EXIT=0` |
| Les trois oracles de vidage | `--dump-info`, `--dump-tree`, `--dump-files` **identiques octet pour octet** aux attendus versionnés, par `cmp`. ⚠ **Aucun n'a été régénéré** : la ligne de débordement n'apparaît que sur un arbre tronqué, et aucune fixture n'approche le plafond |
| `--reselect 3 many_materials.obj` | cinq entiers exacts stables, `AUCUNE ACCUMULATION INDEXEE PAR MAILLAGE NI PAR LIGNE LIEE`, `⚠ ECART NON RESOLU : +368 Kio`, `EXIT=0` |
| `TU` sous `ENABLE_SINAIA=OFF` | **1870 PASSED**, 1 SKIPPED — **+5 tests** (1865 auparavant) |
| `TU` sous `ENABLE_SINAIA=ON` | **26 PASSED** sur le filtre `PelesCore*:SupportedFormats*` (21 auparavant) |
| `--reopen` | `CONTRAT DE DESTRUCTION TENU`, 3 clés rendues, `EXIT=0` |
| `--churn-tree 100` sur 100 images | `REPLI OK`, de 1 à 5 lignes, `EXIT=0` |
| Exclusion mutuelle des vidages | `--dump-layout --dump-info <f>` → message nommant les **quatre** modes, `EXIT=2` |
| Les 3 oracles par CTest | `peles_dump_tree_oracle`, `peles_dump_files_oracle`, `peles_dump_info_oracle` — **3/3 Passed** |
| `ENABLE_CGGRAPH_BOILERPLATE=ON` | configure et bâtit `cggraph-boilerplate.exe` **et** `peles` |

⚠ **Ce que ces corrections ne lèvent toujours pas** : la branche Linux de `ReadProcessFootprintKb`
n'a pas été exécutée ; `ci-linux-peles` n'a toujours pas tourné ; le critère 2 n'est toujours pas
comparé à `sinaia` ; et la croissance d'empreinte de ≈ 100 Kio/sélection reste un **écart non
résolu**, désormais imprimé comme tel.

---

### Verdict de l'étape 6, mesuré le 2026-09-22

**Les six critères d'acceptation passent.** Machine Windows de référence,
`GL_VERSION = "3.3.0 - Build 32.0.101.6078"`, binaire bâti par
`cmake --build build/peles-probe --config Release --target peles`.

**Ce qui remplace `MyGLCanvas::OnMouse`** (`sinaia/wxOpenGLCanvas.cpp:1693-1831`, 139 lignes) :
`ui/input_router.{h,cpp}` (l'arbitre), `ui/input_event.h` + `ui/ui_command.h` (deux vocabulaires),
`view3d/picking.{h,cpp}` (le portage du calcul pur) et `Application::ApplyViewportGesture` (la
correspondance geste → effet). **Le `dynamic_cast<MyFrame*>` que `OnMouse` fait trois fois n'a
aucun équivalent** : la sélection et le survol produisent un état de `core::Document`, et la
sélection de fichier passe par une commande remise à l'hôte.

| # | Critère | Mesure |
|---|---|---|
| 1 | **La conversion de repère** | `--dump-pick 1400x900 680 360` → `rectangle x=240 y=0 w=880 h=720`, pixel (680,360) → **vue (440.0, 360.0)**, `\|rayon central × axe de vue\| = 0.000000000` (tolérance 1e-5), `EXIT=0` |
| 2 | `set_dimensions` reçoit `(rect.w, rect.h)` | Trois tailles : `1400x900` → `(880, 720)`, `1000x800` → `(480, 620)`, `1920x1080` → `(1400, 900)`. **Le rayon central est le même aux trois** : `(-0.813798, 0.469846, -0.342020)` |
| 3 | **Arbitrage scripté** | `--input-script` : `down@viewport → move@panneau → up@panneau` rendent **`vue` pour les trois**, le survol étant `#panel-left` dès le deuxième. Et le second versant — les mêmes gestes entièrement sur le panneau — rend **`interface` pour les trois**. `EXIT=0` |
| 4 | `Alt+clic` **sans surface** laisse le pivot inchangé **au bit près** | `--alt-pivot 250 10` : `surface NON`, avant et après `3f000000 3f000000 3f000000`, `IDENTIQUE AU BIT PRES`, `EXIT=0`. **Versant opposé** : `--alt-pivot 620 310` (centre de la vue) pose le pivot en `(0.999999881, 0.211324871, 0.710138321)` — **un point de la face `x = 1` du cube unité**, `EXIT=0` |
| 5 | **Défilement** | `--dump-scroll` sur `many_materials.obj` : contenu **4334 px** dans **620 px**, `barre de defilement : PRESENTE`, molette routée vers `interface`, **16 crans** portent le défilement de 0 à **3714 px (le maximum)**, et la **dernière** ligne passe de `y ∈ [4316, 4332]` à `y ∈ [602, 618]`, donc **dans la bande visible `[0, 620]`**. `EXIT=0` |
| 6 | **Sélection** | `--open-second test/data/peles/info/panels.obj test/data/obj/cube.obj` : `modeles dans la scene : 2`, `ligne "File" du panneau : panels.obj`, `EXIT=0` |

⚠ **Le critère 5 est mesuré sur une hauteur de contenu de 4334 px, et non 4008.** L'écart avec la
mesure de l'étape 5 est la **barre de défilement elle-même** : elle réduit la largeur utile de la
zone, donc quelques libellés longs passent sur deux lignes. La grandeur que le critère nomme — la
part inatteignable — passe de **82 % à 0 %**.

#### Les huit détecteurs, chacun validé sur son cas négatif

Fichiers restaurés et re-vérifiés verts à chaque fois.

| Détecteur | Cas négatif injecté | Sortie |
|---|---|---|
| **Critère 1** | `ui::WindowToView` privée de la soustraction (`viewX = windowX`) | pixel (680,360) → **vue (680.0, 360.0)**, `\|rayon central × axe\| = 0.266180009`, **`EXIT=1`** |
| **Critère 2** | `--dump-pick` appelant `SetViewSize (width, height)` au lieu de `(rect.width, rect.height)` | `1400x900` → écart **0.245505154**, `1000x800` → écart **0.274008632** : **non colinéaire ET différent d'une taille à l'autre**, **`EXIT=1`** |
| **Critère 3** | règle de capture retirée (les trois branches lisent `OwnerFromHover()`) | `move@panneau` et `up@panneau` rendent **`interface`** au lieu de `vue`, deux lignes marquées `<-- ECART`, **`EXIT=1`** |
| **Critère 3, versant PRODUIT** | ⚠ **le seul qui mesure ce que la CAMERA en a fait** — `move` privé du gel sous capture | `--drag 620 310 120 260` (arrivée sur le panneau) : rotation **0.0000 deg** au lieu de **79.2256 deg**, **`EXIT=1`** |
| **Critère 5** | `overflow: hidden` remis sur `#panel-right`, dans la copie **déployée** du `.rcss`, **sans recompiler** | `barre de defilement : ABSENTE`, `defilement atteint : 0 px`, dernière ligne à `y ∈ [4316, 4332]` hors de `[0, 620]`, **`EXIT=1`** |
| **Critère 6** | `core/loader.cpp` ramené à `if (document.Selected () == nullptr)` | `modeles dans la scene : 2`, `ligne "File" : cube.obj` — **le PREMIER**, **`EXIT=1`** |
| **Câblage `data-event-`** | `data-event-click="onFileRow(file.id)"` retiré de la copie **déployée** du `.rml` | `commandes produites : 0`, `AUCUNE COMMANDE : le clic n'a atteint aucun data-event-`, **`EXIT=1`** |
| **Critère 4** | *(deux versants, tous deux mesurés ci-dessus)* | sans surface : `IDENTIQUE AU BIT PRES` ; avec surface : `MODIFIE`. Un geste qui ne poserait **jamais** le pivot passerait le premier seul |

#### Le câblage, mesuré de bout en bout

⚠ **Aucun autre verdict du lot ne distingue « le clic ne fait rien » de ses trois causes** — attribut
mal nommé, rappel non lié, commande produite mais jamais appliquée. `--click` / `--double-click`
injectent l'événement, impriment le survol, **les commandes produites**, puis leur effet :

| Geste injecté | Mesure |
|---|---|
| `--click 60 700 --browse <fixture>` (liste de fichiers) | `SelectFile ligne 2`, `lignes de fichiers : 7`, `EXIT=0` |
| `--double-click 60 700 --browse <fixture>` | `SelectFile, OpenFile, SelectFile`, puis `statut a l'image de transition : Chargement de cube.obj ...`, `modeles dans la scene : 1`, `ligne "File" : cube.obj`, `EXIT=0` |
| `--click 60 40 --browse <fixture>` (racine de l'arbre) | `ToggleDirectory ligne 0`, l'arbre passe de **5 à 1 ligne**, `EXIT=0` |
| `--drag 620 310 520 260` (glisser interne) | rotation **21.1405 deg**, pivot **inchangé au bit près**, `EXIT=0` |
| `--drag 620 310 120 260` (glisser **sortant** vers le panneau) | rotation **79.2256 deg**, pivot inchangé, `EXIT=0` |

⚠ **L'image de transition est mesurée, pas supposée.** `--double-click` imprime le statut
`Chargement de cube.obj ...` **avant** la ligne `modele : cube.obj`, c'est-à-dire avant que
`VMeshesIO::load` ne soit appelée : le statut est posé en phase 1, une image complète le dessine, et
le chargement synchrone suit.

#### ⚠ R-6 est levée — **par lecture du code de glfw, pas par `--probe-drag`**

Le backend Win32 de glfw appelle `SetCapture(hWnd)` au **premier** bouton enfoncé et
`ReleaseCapture()` au **dernier** relâchement (`extern/glfw-3.3.10/src/win32_window.c:848-867`).
Sous capture Win32, les `WM_MOUSEMOVE` continuent d'arriver à la fenêtre **hors de sa zone client**,
avec des coordonnées qui sortent des bornes. **La règle de capture de l'arbitre repose donc sur une
propriété établie, et non sur une espérance.**

⚠ **Statut exact, et il n'est pas « mesuré »** : c'est une **lecture de source**, pas une exécution.
`--probe-drag` n'a toujours pas été lancé — il exige un opérateur — et P8 reste un `SKIP`. Ce qui
**est** mesuré à l'exécution est l'échelon au-dessus : `--drag 620 310 120 260` fait tourner la vue
de 79 degrés alors que l'arrivée est **sur le panneau gauche**, donc l'arbitre tient bien la capture
sur des coordonnées que RmlUi attribue à un autre élément. Ce que ni l'un ni l'autre n'établit : le
comportement **hors de la fenêtre entière**, et celui des backends X11 et Wayland — dont seul le
masque d'événements a été lu (`x11_window.c:533`, `:633`).

#### Sept décisions prises pendant l'étape, à connaître

- **L'arbitre remonte la chaîne de parents**, au lieu de comparer l'identifiant du seul élément
  rendu par `GetHoverElement()`. RmlUi rend le descendant **le plus jeune** sous le curseur :
  `#viewport` est vide aujourd'hui, mais §1.9 y met une image de transition et une comparaison
  directe rendrait alors la vue inerte. **Ce n'est pas un second calcul de géométrie** : la chaîne
  de parents est celle que RmlUi a construite, et le C++ n'y ajoute aucune largeur de panneau.
- **Le propriétaire se fige au PREMIER enfoncement**, et un second bouton enfoncé pendant un
  glisser ne ré-arbitre rien. La règle demandée est « jusqu'au relâchement de CE bouton » ; celle-ci
  est plus forte et ferme un état mixte — un bouton à la vue, un autre à l'interface — dont le
  relâchement irait au mauvais destinataire.
- **Ouvrir un fichier depuis la liste l'AJOUTE à la scène, il ne la remplace pas.** C'est ce qui
  rend le survol et la sélection à la souris utiles, et c'est le seul état où la sélection décrite
  par les panneaux peut être fausse — donc le seul où le critère 6 est falsifiable. ⚠ **Conséquence
  assumée : la scène ne se vide jamais par l'interface dans ce lot.** Il n'y a ni « Fermer » ni
  onglets (P-2, D-3).
  ⚠ **RÉSERVE, consignée à l'étape 6 : l'interface devient MONOTONE, et aucun instrument mémoire ne
  surveille ce chemin-là.** Dix double-clics mettent dix modèles dans la scène et rien ne les retire ;
  la mémoire GL et le cache `MeshDataManager` croissent sans borne. Or **`--reselect` passe par
  `OpenModel`, qui VIDE avant de charger**, et `--reopen` aussi : les deux instruments mesurent le
  chemin qui **remplace**, jamais celui que l'utilisateur emprunte. Fermer cette réserve demande soit
  un « Fermer » (hors périmètre du lot 1), soit un instrument qui répète `--open-second` et relève
  l'empreinte après chacune — à l'étape 7.
- **Un clic désigne, un double-clic ouvre**, comme `EVT_LIST_ITEM_ACTIVATED` de l'hôte historique.
  Un simple clic qui chargerait rendrait toute traversée de la liste aussi coûteuse que le plus
  lourd des fichiers traversés.
- **Un clic sur une ligne d'arbre ouvre ou ferme le nœud ET en fait le répertoire courant.** La
  feuille de style ne dessine aucun chevron : une ligne qui ne ferait que sélectionner laisserait
  l'arbre inouvrable, faute de zone où cliquer pour l'ouvrir.
- **Le défilement est INSTANTANÉ, pas animé** (`Context::SetDefaultScrollBehavior (Instant)`). Deux
  raisons, et la seconde est la plus forte : le défilement lisse de RmlUi s'intègre sur le **temps
  écoulé** entre deux `Update()`, et un chargement synchrone de plusieurs secondes tombe au milieu
  de l'animation ; et une position de défilement qui dépend du nombre d'images rendues **n'est pas
  mesurable**.
- ⚠ **`--alt-pivot` contient sa propre comparaison, au lieu d'être un `--dump-camera` que
  l'opérateur diffe.** Deux exécutions d'un vidage relues à l'œil ne distinguent pas deux flottants
  qui diffèrent au dernier bit : ce serait un critère qui ne peut pas échouer sur le mode de panne
  le plus probable. Les octets bruts sont imprimés à chaque exécution.

#### Deux vocabulaires d'entrée, et une vérification mécanique entre eux

`peles_platform` ne lie pas `peles_ui` — le pare-feu l'interdit — donc les deux cibles ont
nécessairement leur propre encodage des boutons et des modificateurs. ⚠ **`app/application.cpp` est
la seule unité qui voie les deux en-têtes**, et elle porte **six `static_assert`** qui les relient.
Sans eux, une divergence d'un seul bit ferait paner la vue au lieu de l'orbiter, ou poserait le
pivot sur un `Ctrl` + clic — **sans aucun diagnostic**.

**La batterie de cas négatifs du pare-feu passe de 14 à 18 cas, et les dix-huit rendent `error C1083` :**

| # | Cas négatif injecté | Cible bâtie |
|---|---|---|
| **15** | `#include "gl_wrapper.h"` depuis `ui/input_router.cpp` | `peles_ui` |
| **16** | `#include <RmlUi/Core.h>` depuis `view3d/picking.cpp` | `peles_view3d` |
| **17** | `#include "app/application.h"` depuis `ui/input_router.cpp` | `peles_ui` |
| **18** | `#include "window_glfw.h"` depuis `ui/input_router.cpp` | `peles_ui` |

⚠ **Le cas 18 est le plus tentant de cette étape** : l'arbitre consomme des événements que la
fenêtre produit, et le raccourci naturel est de lui faire lire directement les codes de glfw. C'est
exactement ce que les six `static_assert` rendent inutile.

#### Mesures de non-régression

| Critère | Mesure |
|---|---|
| `peles_probe` | **7/7**, `EXIT=0` |
| `--assets <inexistant>` | `EXIT=3` |
| `--state-audit test/data/rabbit.obj` | `AUDIT OK`, **25 états**, 3 images, 2 phases, `EXIT=0` |
| `--dump-layout 1400x900` | `(240, 0, 880, 720)` ; `1401x900` → `w=881` ; `1400x901` → `h=721` |
| `--dump-layout 1400x900 --browse <fixture> --browse-depth 3 <60 matériaux>` | **le même rectangle**. ⚠ **C'est la mesure qui établit que les barres de défilement ne déplacent pas la vue** : elles sont posées à l'intérieur de la boîte de la zone, dont la base flex reste fixe |
| `--readpixel 620 490 test/data/obj/cube.obj` | **(25,128,204)**, écart 1/255, `PIXEL CONFORME`, `EXIT=0` |
| Les trois oracles de vidage | `--dump-tree`, `--dump-files`, `--dump-info` **identiques octet pour octet** aux attendus, par `cmp` **et** par les trois tests CTest (3/3 Passed). ⚠ **Aucun n'a été régénéré** |
| `--reopen` | `CONTRAT DE DESTRUCTION TENU`, `EXIT=0` |
| `--reselect 3 many_materials.obj` | cinq entiers exacts stables (`1`, `1`, `14/4/242`), `AUCUNE ACCUMULATION`, `⚠ ECART NON RESOLU : +560 Kio`, `EXIT=0` |
| `--churn-tree 100` sur 100 images | `REPLI OK`, de 1 à 5 lignes, `EXIT=0` |
| `--frames 120 --dump-timing <60 matériaux>` | image médiane **0,765 ms**, `Update()` médiane **0,039 ms** |
| Journal RmlUi sur le document du produit | `--frames 2`, `--frames 60`, `--frames 2 --debugger`, `--frames 10 --churn-tree 5` : **zéro** message de niveau `LT_WARNING` ou supérieur, `EXIT=0` sur les quatre |
| `TU` sous `ENABLE_SINAIA=OFF` | **1870 PASSED**, 1 SKIPPED (`SupportedFormats`) — inchangé |
| `TU` sous `ENABLE_SINAIA=ON` | **26 PASSED** sur le filtre `PelesCore*:SupportedFormats*`, dont les 2 de `tu_supported_formats` |
| `ENABLE_CGGRAPH_BOILERPLATE=ON` **et** `ENABLE_PELES=ON` | configurent et bâtissent `cggraph-boilerplate.exe`, `peles.exe` et `peles_probe.exe`, **un seul** `glfw3.lib` |
| Avertissements du compilateur | **zéro** sur les fichiers de `peles/`, reconstruction **complète** (`--clean-first`). Les avertissements restants viennent de `src/cgmesh` et `extern/glutess`, et sont antérieurs |

#### Ce que ce verdict ne lève pas

- ⚠ **R-6 est levée par LECTURE, pas par mesure.** Voir ci-dessus : `--probe-drag` n'a toujours pas
  été lancé, P8 reste un `SKIP`, et seul le backend **Win32** a été lu.
- ⚠ **L'arbitrage est fait avec le rectangle de l'image PRÉCÉDENTE**, et l'ordre des huit phases
  l'impose : l'événement arrive avant l'`Update()` qui recalculera la mise en page. Un
  redimensionnement introduit un retard d'**une image** sur le picking. **Non mesuré** — il faudrait
  redimensionner la fenêtre pendant un glisser.
- ⚠ **Les événements de la toute première image sont écartés**, faute de rectangle lu. Une image à
  60 Hz ; la conséquence est qu'un clic dans les 16 premières millisecondes de la session est perdu.
- **La ligne `Path` n'est toujours ni copiable ni sélectionnable en pratique.** Le routage **souris**
  est là, mais **aucune entrée clavier n'est routée** : `ProcessKeyDown`, `ProcessTextInput` et la
  surcharge de `SetClipboardText` restent à faire. La régression D-5 n'est pas refermée, et elle
  appartient à l'étape 7.
- **Le survol 3D n'éclaire aucune ligne de panneau.** `Document::Hovered` est écrit et la boîte
  jaune est dessinée, mais le pendant de `HighlightModelRow` — mettre en évidence la ligne du
  fichier survolé — n'est pas câblé : il entrerait en conflit avec la classe de sélection, que
  `data-attr-class` écrase entièrement.
- **La scène ne se vide jamais par l'interface.** Ouvrir dix fichiers en met dix dans la scène et
  aucun chemin ne les retire. Le contrat de destruction est donc exercé par `--reopen` seul.
- ⚠ **Le seuil de 3 px entre clic et glisser est un PORTAGE, pas une mesure.** Il vient de
  `wxOpenGLCanvas.cpp:1765` ; rien ici ne l'a calibré.
- **R-P1 et R-P2 restent ouvertes**, inchangées. Aucune parité caméra avec `sinaia` n'est mesurée :
  `--drag` compare la caméra de `peles` à elle-même.
- ⚠ **Le défilement horizontal n'existe pas** (`overflow-x: hidden`). Un nom de fichier ou de
  matériau plus large que sa zone reste coupé, comme à l'étape 5.

---

### Corrections de la revue de l'étape 6, mesurées le 2026-09-22

Revue `REQUEST CHANGES` : 0 critical, **4 HIGH**, 9 medium, 6 low. Binaire bâti par
`cmake --build build/peles-sinaia --config Release --target peles`, machine Windows de référence,
`GL_VERSION = "3.3.0 - Build 32.0.101.6078"`. **Zéro avertissement du compilateur** sur les fichiers
de `peles/`, recompilation complète (tous les `.cpp` et `.h` de `peles/` touchés avant la passe).

#### 🟠 H1 — un second bouton relâché figeait la caméra. **Corrigé.**

Le gel du propriétaire fermait l'état mixte **côté arbitre** et le laissait ouvert **côté caméra** :
`ApplyViewportGesture` mappait chaque enfoncement et chaque relâchement sur un trackball à **un seul
slot d'état**, et `Ctrackball::mouse_press` pose `tb_state = RELEASED` **quel que soit le bouton**.

- `InputRouter::GestureButton()` mémorise le bouton qui a fait passer `m_heldButtons` de 0 à non nul.
  ⚠ **Il survit au dernier relâchement** : le relâchement est arbitré **après** la mise à jour du
  masque, et c'est lui qui a besoin de la valeur.
- `ApplyViewportGesture` est scindée en `BeginViewportGesture` / `EndViewportGesture` — **c'est là
  que le défaut se cachait** — et les deux **ignorent tout bouton qui n'a pas ouvert le geste**.
- ⛔ `src/cgre` **n'est pas touché**. Le commentaire de `Camera::EndDrag` énonce désormais la règle
  en vigueur : *l'hôte n'appelle `BeginDrag`/`EndDrag` que pour le bouton qui a ouvert le geste.*

**Détecteur neuf : `--drag-bump <left|right>`**, qui se compose avec `--drag`. C'est le seul
instrument du lot qui exerce un geste à **deux** boutons — `--input-script` mesure le propriétaire
retenu, jamais ce que la caméra en fait.

| Versant | Séquence injectée | Forme corrigée | **Forme fautive (garde retirée)** |
|---|---|---|---|
| `left` | move → **down L** → down R → up R → move → up L | rotation **79,2256 °**, œil déplacé, aucune sélection, `EXIT=0` | rotation **0,0000 °**, œil **0,000000**, **`EXIT=1`** |
| `right` | move → **down R** → down L → up L *(immobile)* → move → up R | œil déplacé de **0,684281**, `selection changee : non`, `EXIT=0` | `selection changee : **OUI**`, œil **0,000000**, **`EXIT=1`** |

⚠ **Le versant `right` reproduit littéralement la « variante pire » de la revue** : le clic gauche
immobile intercalé sélectionne un modèle au milieu d'un dolly.
⚠ **Garde de validité du versant `right`** : l'instrument remet la sélection à vide avant
l'injection. `SelectFromPixel` sort sans rien faire quand le modèle touché **est déjà** le modèle
sélectionné — or un fichier passé en argument est sélectionné par son chargement. Sans cette remise
à zéro, le détecteur aurait été vert quoi qu'il arrive.
⚠ **Aucun détecteur existant ne voyait H1** : sur la forme fautive, `--drag 620 310 120 260` rend
**79,2256 °** et `EXIT=0`.

#### 🟠 H2 — le critère 2 mesurait l'instrument. **`--dump-pick` passe par le produit.**

`--dump-pick` **ouvre la fenêtre à la taille demandée**, rend **une image complète**, puis relit :

- le rectangle que la **phase 3** a lu ;
- les dimensions que la **phase 5** a posées (`scene_renderer.cpp:310`), *relues à leur source — le
  trackball*.

L'instrument **ne pose plus aucune dimension**, et un **premier verdict** compare les deux avant le
verdict de colinéarité. ⚠ **Les deux verdicts ne sont pas redondants** : une hauteur fausse laisse le
rayon **central** colinéaire — le NDC du centre vaut (0,0) quelles que soient les dimensions — et ne
se voit que hors du centre. Le verdict de colinéarité seul **ne peut pas** échouer sur ce mode de
panne.

**Scénario de falsification de la revue, injecté dans le PRODUIT** (`scene_renderer.cpp:310`
remplacé par `camera.SetViewSize (width, height + 180)`) :

| Instrument | Sortie |
|---|---|
| **`--dump-pick 1400x900 680 360`** | `DIMENSIONS DE LA CAMERA FAUSSES : le produit a pose (880, 900) la ou le rectangle de la vue vaut (880, 720)`, **`EXIT=1`** |
| `--readpixel 620 490` | `(25,128,204)`, `PIXEL CONFORME`, `EXIT=0` |
| `--dump-layout 1400x900` | `(240, 0, 880, 720)`, `EXIT=0` |
| `--input-script` | `ARBITRAGE CONFORME`, `EXIT=0` |
| `--alt-pivot 620 310` | `PIVOT POSE SUR LA SURFACE`, `EXIT=0` |
| `--drag 620 310 120 260` | rotation 79,2256 °, `EXIT=0` |
| `--frames 2` (concordance `GL_VIEWPORT`) | concordant, `EXIT=0` |

**Le critère 1 et le critère 2, après correction :**

| Cadre demandé | Cadre **réel** | Rectangle lu | `set_dimensions` **posé par le produit** | Rayon central | Verdict |
|---|---|---|---|---|---|
| `1400x900` | 1400×900 | `x=240 y=0 w=880 h=720` | `(880, 720)` | `(-0.813798, 0.469846, -0.342020)` | écart `0.000000000`, `EXIT=0` |
| `1000x800` | 1000×800 | `x=240 y=0 w=480 h=620` | `(480, 620)` | *idem* | écart `0.000000000`, `EXIT=0` |
| `1920x1080` | **1920×1055** | `x=240 y=0 w=1400 h=**875**` | `(1400, 875)` | *idem* | écart `0.000000000`, `EXIT=0` |

⚠ **La troisième ligne diffère de l'étape 6, et c'est le passage par le produit qui la change** : le
gestionnaire de fenêtres **ne délivre pas** 1080 px de hauteur utile sur cette machine ; le tampon
fait 1055. L'ancienne mesure `(1400, 900)` était obtenue sur un cadre **imposé à RmlUi**, que rien
n'ouvrait. **Le rayon central reste identique aux trois tailles**, ce que le critère demande.

#### 🟠 H3 / 🟡 M5 — `--click` ne vérifiait ni le genre ni l'effet. **`ReportClick`.**

Le verdict `commands.empty () ? 1 : 0` est remplacé par `app::ReportClick`, qui lit **le genre** des
commandes **et** le nombre de modèles **avant / après** :

- sous `--double-click` : au moins une `OpenFile` **et** `modelsAfter > modelsBefore` ;
- sous `--click` simple : **absence** d'`OpenFile` et scène de taille inchangée ;
- `--click-expect select|toggle|open` compare le genre. Sans elle, seul le genre **impliqué** par
  `--double-click` est exigé.

| Cas négatif injecté (copie **déployée** du `.rml`, **sans recompiler**) | Sortie |
|---|---|
| `data-event-dblclick` retiré, `--double-click 60 700` | `commandes produites : 2` (deux `SelectFile`), `modeles dans la scene : 0 -> 0`, `GENRE DE COMMANDE FAUX : aucune OpenFile parmi les 2 commande(s) produites`, **`EXIT=1`** — c'est exactement le cas qui rendait `CLIC ROUTE` et `EXIT=0` |
| `kOnTreeRow` / `kOnFileRow` **intervertis**, `--click 60 700 --click-expect select` | `ToggleDirectory ligne 2`, `genre attendu : SelectFile`, **`EXIT=1`** |
| les mêmes rappels intervertis, **sans** `--click-expect` | `EXIT=0` — *le verdict d'avant M5, reproduit* |
| `data-event-click="onFileOpen(file.id)"` sur la ligne de fichier, `--click 60 700` | `OpenFile ligne 2`, `modeles : 0 -> 1`, `CLIC SIMPLE QUI OUVRE`, **`EXIT=1`** |

**Après correction** : `--click 60 700 --click-expect select` → `SelectFile ligne 2`, `0 -> 0`,
`EXIT=0` · `--click 60 40 --click-expect toggle` → `ToggleDirectory ligne 0`, `EXIT=0` ·
`--double-click 60 700` → `SelectFile, OpenFile, SelectFile`, `statut a l'image de transition :
Chargement de cube.obj ...`, `modeles dans la scene : 0 -> 1`, `ligne "File" : cube.obj`, `EXIT=0`.

#### 🟠 H4 — `--alt-pivot` rendait `EXIT=0` sans que le geste atteigne la vue. **Corrigé.**

`m_altPivotFoundSurface` (booléen) devient **`m_altPivotStatus`**, qui porte les **quatre** issues de
`PivotPickStatus`, et `m_lastPressOwner` relève le propriétaire **retenu par l'arbitre** — dans le
chemin du produit, sans seconde lecture du survol. `ReportAltPivot` lit les deux.

| Pixel | Avant correction | Après correction |
|---|---|---|
| `60 700` (**panneau bas**) | `surface NON / IDENTIQUE AU BIT PRES / PIVOT INCHANGE`, **`EXIT=0`** | `proprietaire retenu : PAS LA VUE`, `issue du geste : (le geste n'a pas couru)`, `MESURE NON CONCLUANTE`, **`EXIT=1`** |
| `250 10` (dans la vue, **rien dessous**) | `EXIT=0` | `proprietaire : vue`, `issue : NoHit`, `PIVOT INCHANGE`, `EXIT=0` — **inchangé** |
| `620 310` (centre de la vue) | `EXIT=0` | `issue : Ok`, pivot `(0.999999881, 0.211324871, 0.710138321)`, octets `3f7ffffe 3e58658c 3f35cba0`, `EXIT=0` — **inchangé au bit près** |

`OutOfViewport` et `NoView` rendent également `MESURE NON CONCLUANTE` + code 1.

#### 🟡 Les quatre MEDIUM exigés avant l'étape 7

- **M1 — la sortie de cadre est MISE EN FILE.** `MouseEvent::Kind::Leave` existe, le rappel glfw ne
  fait plus qu'`push_back`, et `InputRouter::Route` la traite comme les quatre autres genres —
  `OnWindowLeave()` disparaît. ⚠ **Elle échappe seule à la garde de rectangle** : elle ne porte
  aucune coordonnée, donc il n'y a rien à convertir. **Mesuré** : `--input-script` porte deux étapes
  de plus, `leave` → survol `(aucun)`, propriétaire `aucun`, puis `move@panneau` → survol
  `#panel-left`, propriétaire `interface`. *Statut : l'acheminement est **mesuré** ; l'**ordre**
  relatif au `move` du même `glfwPollEvents` est **structurel** — les deux passent par la même file,
  drainée dans l'ordre d'arrivée — et non mesuré, faute d'un moyen d'injecter un rappel glfw.*
- **M2 — le rectangle est lu dès `CreateUi`.** Un `m_rml->Update()` sur un document **sans aucune
  ligne** (les vecteurs liés sont peuplés par `PopulateBrowser`, qui court après), puis
  `ReadViewportRect` + `SetViewportRect`. ⚠ **La conséquence corrigée est plus large que « 16 ms »** :
  tant qu'aucune image n'était **rendue** — fenêtre créée réduite, vue dégénérée — **toutes** les
  entrées étaient jetées sans trace. La garde `!m_hasViewRect` subsiste et ne couvre plus que les
  états où aucune mise en page valide n'existe ; son commentaire le dit.
- **M3 — les dimensions sont posées AVANT l'arbitrage.** `GetFramebufferSize` + `SetDimensions`
  passent devant `ProcessInput`, **et le bloc ne dépend d'aucun événement**. RmlUi n'arbitre plus son
  survol sur des dimensions périmées. ⚠ **La garde « pas de surface » reste APRÈS l'arbitrage** : les
  événements sont donc drainés même fenêtre réduite, la file ne grossit pas et aucune commande
  d'interface n'est perdue — rien de ce qui court là n'appelle GL. La formulation circulaire est
  corrigée dans `application.h` : le retard d'une image est une **conséquence de l'ordre choisi**, pas
  une contrainte qui s'impose à lui, et il ne porte **que** sur le rectangle.
- **M7 — le terme Y a désormais un détecteur.** `assets/layout_probe.rml` + option `--document`. Voir
  §1.7, corrigé : `viewY` neutralisé laisse le document du produit à **0.000000000 / `EXIT=0`** et
  rend **0.150858477 / `EXIT=1`** sur la mise en page d'épreuve — où `--input-script`, `--readpixel`,
  `--dump-layout` et `--dump-scroll` restent tous les quatre verts. ⚠ **La mise en page d'épreuve ne
  porte AUCUN attribut `data-`** : elle ne peut donc pas dériver du modèle de données du produit.
  Ce qu'elle fixe est une **géométrie**, et rien d'autre : `x=240 y=140 w=1160 h=760` pour un cadre
  `1400x900`.

#### 🟡 Les cinq autres MEDIUM

- **M4 — non corrigé dans le code, et la RÈGLE ÉCRITE est alignée sur le code.** Restreindre la remise
  des `move` à `m_heldButtons == 0` **détruirait la garde d'inutilité de `--input-script`** (`i == 2`,
  « au déplacement sur le panneau, RmlUi survole encore la vue »), c'est-à-dire le seul détecteur
  d'arbitrage du lot. §1.7 énonce désormais les deux raisons et **consigne le défaut produit** :
  `#dir-tree div:hover` et `#file-list div:hover` (`peles.rcss:247, :281`) allument les lignes
  traversées pendant une orbite. ⚠ **C'est un arbitrage explicite, pas un oubli.**
- **M5** — fait, avec H3.
- **M6 — `--drag` LIT `arrivalInView`.** Une arrivée **dans** la vue rend désormais
  `⚠ ARRIVEE DANS LA VUE : la regle de capture n'a rien eu a tenir, ce glisser ne la mesure pas`
  (`EXIT=0`, mais la mesure ne prétend plus porter sur la capture) ; une arrivée **hors** de la vue
  rend `ARRIVEE HORS DE LA VUE : la capture a tenu jusqu'au relachement`. Mesuré : `--drag 620 310 520
  260` → 21,1405 ° / interne · `--drag 620 310 120 260` → 79,2256 ° / externe.
- **M8 — quatre des cinq fonctions passent sous 50 lignes effectives** (lignes non vides, hors
  commentaires et hors accolades seules) :

  | Fonction | Avant | Après |
  |---|---|---|
  | `Run` | 82 | **4** (+ `RunDumpMode` 38, `RunLoop` 32) |
  | `ApplyViewportGesture` | 69 | **19** (+ `BeginViewportGesture` 19, `EndViewportGesture` 12) |
  | `RenderFrame` | 63 | **50** |
  | `CreateWindowAndGl` | 54 | **47** |
  | `RunClick` | 73 | **41** (+ `QueueClickSequence` 14) |

  ⚠ **`dump_modes.cpp` n'est PAS coupé, et c'est une décision** : il passe de 1530 à 1814 lignes. La
  coupe est mécanique mais elle triplerait la surface de ce correctif et en rendrait la relecture
  beaucoup plus coûteuse. **Reste dû**, à faire seul.
- **M9 — la narration de `core/loader.cpp` est retirée.** Le bloc tient en deux phrases et énonce la
  règle en vigueur. Les cinq fichiers neufs étaient propres ; la régression était isolée à celui-là.

#### 🔵 LOW

- **L1 — la borne de molette est posée AVANT tout usage.** `kMaxWheelNotches` et `ClampNotches`
  vivent dans `ui/input_event.h`, **un seul site**, et les **deux** destinataires l'appliquent —
  l'arbitre avant `ProcessMouseWheel`, `ApplyViewportGesture` avant `ZoomStep`.
- **L2** — `MouseButton::count` supprimé : aucun lecteur, vérifié par lecture de la déclaration.
- **L3 — le commentaire disait faux d'une des deux frontières.** Il énonce maintenant que seule
  `platform → ui` est vérifiée par le compilateur (six `static_assert` dans `app/application.cpp`),
  et que `ui → RmlUi` repose sur la documentation de RmlUi — ses interfaces ne typent pas les boutons.
- **L4 — `InputRouter` porte `m_hasRect`.** Tant que `SetViewportRect` n'a pas été appelé, `Route`
  rend `Owner::None` et ne remet l'événement à personne. ⚠ **L'absence ne se déduisait pas du
  contenu** : un rectangle par défaut a l'air valide, son origine vaut (0,0).
- **L5 — NON CORRIGÉ, et l'énoncé est consigné.** glfw synthétise un `GLFW_RELEASE` par bouton à la
  perte de focus — aucune capture ne reste coincée — mais le relâchement porte la **dernière**
  position, donc `dx = dy = 0 ≤ 9` et un `Alt+Tab` pendant un appui immobile **sélectionne le
  modèle**. Le refermer demande un rappel de focus dans `MouseCallbacks` qui **annule** le geste au
  lieu de le terminer. **Étape 7.**
- **L6 — la citation datée de `window_glfw.h` est retirée.** La propriété (`SetCapture` /
  `ReleaseCapture` au premier et au dernier bouton, backend Win32) est nommée sans numéros de ligne ni
  numéro de version.

#### Les deux corrections de documentation

1. **R-6 est ré-énoncée** en §1.7 : *levée sur **Win32** par lecture de source ; supposée vraie sous
   X11 et Wayland **par le protocole** (grab implicite du serveur / de `wl_pointer`), **non lue, non
   mesurée**. `--probe-drag` non lancé, **P8 reste un `SKIP`**.* ⚠ Et il est écrit que **glfw ne pose
   aucun `XGrabPointer` sur enfoncement** — `captureCursor()` n'y sert que pour
   `GLFW_CURSOR_DISABLED` : la conclusion est juste, la preuve citée ne la portait pas.
2. **`ui/ui_command.h` est reformulé sur les deux raisons qui tiennent** : (a) `RequestOpen` doit
   **rendre la main** pour qu'une image dessine le statut avant le chargement synchrone ; (b)
   `OpenModel` → `AnnounceRelease` → `RemoveMesh` émet de **vrais appels GL**, qui depuis l'intérieur
   de `ProcessMouseButtonDown` seraient **hors des deux clôtures** et hors de tout relevé
   `--state-audit`. ⚠ Il est écrit **explicitement** que la limite §1.5 **ne mord pas ici** : un appel
   direct ne ferait que `DirtyVariable`, et la reconstruction a lieu dans `Update()`, jamais réentré
   depuis un rappel souris.
3. **La réserve de la décision n°3 est consignée** (monotonie de l'interface, croissance non bornée,
   et le fait qu'aucun instrument mémoire ne surveille le chemin réellement offert).

#### Non-régression — 24 exécutions, 24 conformes

| Critère | Mesure |
|---|---|
| `peles_probe` | **7/7**, `EXIT=0` (P8 `SKIP`, inchangé) |
| `--assets <inexistant>` | `EXIT=3` |
| `--state-audit test/data/rabbit.obj` | `AUDIT OK`, **25 états**, 3 images, **2 phases**, `EXIT=0` |
| `--dump-layout 1400x900` | `(240, 0, 880, 720)` ; `1401x900` → `w=881` ; `1400x901` → `h=721` ; **et avec `--browse <fixture> --browse-depth 3`** : le **même** rectangle |
| `--readpixel 620 490 test/data/obj/cube.obj` | **(25,128,204)**, `PIXEL CONFORME`, `EXIT=0` |
| Les **trois** oracles | `--dump-tree`, `--dump-files`, `--dump-info` (chemin **relatif** `test/data/peles/info/panels.obj` depuis la racine) **identiques octet pour octet** par `cmp` **et** par les 3 tests CTest (3/3 Passed). **Aucun régénéré** |
| `TU --gtest_filter=PelesCore*:SupportedFormats*` | **26 PASSED** |
| `--reopen` | `maillages enregistres : 1 → 0`, `CONTRAT DE DESTRUCTION TENU`, `EXIT=0` |
| `--reselect 3 many_materials.obj` | `1`, `1`, `14/4/242` **stables sur les trois**, `materiaux : 60`, arbre `complet`, `EXIT=0` |
| `--churn-tree 100` sur 100 images | `REPLI OK`, de 1 à 5 lignes, `EXIT=0` |
| **Critère 1** | `--dump-pick 1400x900 680 360` → vue `(440.0, 360.0)`, écart `0.000000000`, `EXIT=0` |
| **Critère 2** | trois tailles, `set_dimensions` **posé par le produit** = rectangle lu, rayon central identique aux trois |
| **Critère 3** | `--input-script` : `vue` pour les trois sous capture, `interface` pour les trois sur le panneau, plus les deux étapes de sortie de cadre, `EXIT=0` |
| **Critère 4** | `--alt-pivot` sur les **trois** pixels ci-dessus |
| **Critère 5** | `--dump-scroll many_materials.obj` : barre `PRESENTE`, **16 crans**, défilement **3714 px (maximum)**, dernière ligne `y ∈ [602, 618]` dans `[0, 620]`, `EXIT=0` |
| **Critère 6** | `--open-second panels.obj cube.obj` : `modeles dans la scene : 2`, `ligne "File" : panels.obj`, `EXIT=0` |
| `--frames 120 --dump-timing <60 matériaux>` | image médiane **0,562 ms**, `Update()` médiane **0,025 ms** |
| Journal RmlUi | `--frames 60` : **zéro** message `LT_WARNING` ou supérieur, `EXIT=0` |
| `--drag-bump left` **sans** `--drag` | `EXIT=2` (refus d'argument) |
| `ENABLE_CGGRAPH_BOILERPLATE=ON` **et** `ENABLE_PELES=ON` | `peles.exe`, `peles_probe.exe` et `cggraph-boilerplate.exe` bâtis, **un seul** `glfw3.lib` |
| Avertissements du compilateur | **zéro** sur `peles/`, recompilation complète |

⚠ **Ce que cette passe ne lève pas** : L5 (perte de focus), la coupe de `dump_modes.cpp`, la réserve
de monotonie de la décision n°3, le défaut de survol M4 — les quatre sont nommés ci-dessus et
appartiennent à l'étape 7. **R-P1, R-P2 et P8 restent inchangées.**

---

### Verdict de l'étape 7, mesuré le 2026-09-22

**Les deux critères d'acceptation passent.** Machine Windows de référence,
`GL_VERSION = "3.3.0 - Build 32.0.101.6078"`.

⚠ **Deux arbres de construction, et il faut savoir lequel a mesuré quoi.**
`build/peles-sinaia` (Release, `ENABLE_PELES=ON` + `ENABLE_SINAIA=ON`) porte **tout sauf** le
critère 2 ; `build/peles-asan` (`CG_ENABLE_SANITIZERS=ON`, `ENABLE_SINAIA=OFF`) a été **créé pour
cette étape** et ne porte **que** les exécutions sous ASan — la bascule des sanitizers change les
options de compilation de l'arbre entier, donc elle ne peut pas cohabiter avec lui.
`build/peles-boilerplate` a été reconfiguré pour la seule non-régression `ENABLE_CGGRAPH_BOILERPLATE`.

| # | Critère | Mesure |
|---|---|---|
| 1 | `peles --frames 3 <fichier illisible>` sort **avec 0**, et le statut nomme le fichier **et** la raison | `peles --frames 3 --dump-status test/data/peles/info/nonexistent.obj` → `statut a l'ecran : "test/data/peles/info/nonexistent.obj : format inconnu, fichier illisible ou sans geometrie"`, `nom attendu … -> present`, `raison attendue … -> presente`, `STATUT CONFORME`, **`EXIT=0`** |
| 2 | Sous ASan, **60 images ouvrant deux fichiers**, aucune erreur | `peles --frames 60 --open-second panels.obj cube.obj` sous `build/peles-asan` : **0 ligne `AddressSanitizer`**, `SELECTION CONFORME`, **`EXIT=0`** |

#### ⚠ La liste des erreurs ASan connues, établie AVANT de lancer

Elle est tirée de la **lecture** de `debt_cgre.md` et `debt_cgmesh.md`. **Aucun relevé ASan de
`cgre` n'existait dans le dépôt avant cette étape** — `debt_cgre.md` le dit lui-même : *« Aucun
profilage, aucune exécution. […] Les affirmations sur le comportement à l'exécution — fuites GPU,
shader qui ne compile pas — sont déduites de la lecture. »* Il n'y avait donc **aucune erreur
mesurée** à exempter, et toute erreur rapportée par cette exécution aurait été **neuve**.

| # | Ce qui était attendu | Source | Verdict |
|---|---|---|---|
| A1 | `cgre` : **8 singletons** alloués par `new` à l'initialisation statique et jamais détruits — `background_manager.cpp:5`, `capabilities_manager.cpp:9`, `console.cpp:159`, `light_renderer.cpp:10`, `material_renderer.cpp:3`, `mesh_renderer.cpp:433`, `shaders_manager.cpp:20`, `textures_manager.cpp:7`. Fuite bornée à une par processus | `debt_cgre.md`, tableau des singletons | **invisible ici** — voir B |
| A2 | `cgmesh` : les fuites du §2 de `debt_cgmesh.md`, toutes établies **par lecture** (réserve H3 : « quantification réelle des fuites » reste due) | `debt_cgmesh.md` §2, H3 | **invisible ici** — voir B |
| A3 | `cgre` : `vboInfo::pMesh` pendant (gardé, `mesh_renderer.cpp:505-506`) et `MaterialRenderer::m_pMaterials` accumulant des `Material*` pendants **comparés par adresse** | `debt_cgre.md` §3 | **hors de portée d'ASan** : ni l'un ni l'autre n'est déréférencé, seule la valeur du pointeur est lue |
| A4 | `cgre` : `offscreen_rendering_factory.cpp`, `memcpy` lisant **4× la taille allouée** (`:412`) | `debt_cgre.md` annexe | **non exercé** : `peles` ne fait aucun rendu hors écran |

⚠ **B — LIMITE DE L'INSTRUMENT, DÉCLARÉE AVANT L'EXÉCUTION ET CONFIRMÉE PAR LA MESURE :
l'AddressSanitizer de MSVC n'embarque PAS LeakSanitizer.** A1 et A2 y sont donc structurellement
invisibles. Le dépôt le savait déjà sans l'écrire ici : son exécution ASan de `cgre` passe
`ASAN_OPTIONS=…detect_leaks=0…` (`debt_cgre.md`, ligne « Debug + `CG_ENABLE_SANITIZERS=ON` »).

⚠ **Conséquence directe sur le cas négatif que le plan demandait.** « Le critère 2 doit échouer si
on introduit une fuite volontaire » **ne peut pas être vérifié sur cette plateforme**, et le dire
autrement serait faux. **Mesuré** : `(void)new char[8 << 20];` par image, soit **480 Mio fuités sur
60 images** → **aucune ligne `AddressSanitizer`, `EXIT=0`**. Le cas négatif valide sous MSVC est une
**écriture hors bornes**, et il a été exercé : `char* p = new char[16]; p[16] = 'x';` →
`==13840==ERROR: AddressSanitizer: heap-buffer-overflow`, `WRITE of size 1`, **`EXIT=1`**. Fichier
restauré, re-vérifié vert.

#### Ce que l'exécution sous ASan a mesuré, en entier

Toutes ces exécutions sont sur les sources **livrées**, après les deux coupes de fichiers.

| Exécution | `EXIT` | Lignes `AddressSanitizer` |
|---|---|---|
| **Critère 2** — 60 images, deux fichiers ouverts | 0 | **0** |
| `--churn-tree 100` sur **1001 lignes liées** (plafond atteint), 100 images | 0 | **0** |
| `--reselect 3` sur 60 matériaux | 0 | **0** |
| `--state-audit` | 0 | **0** |
| `--readpixel 620 490` → `(25,128,204)` | 0 | **0** |
| `--reopen` (contrat de destruction) | 0 | **0** |
| `--focus-loss`, `--dump-copy`, `--dump-status` | 0 | **0** |

✅ **La réserve « le rétrécissement du vecteur lié n'a PAS été mesuré sous ASan », ouverte depuis
l'étape 4, est LEVÉE.** Le vecteur passe de **1001 à 1 ligne et retour, cent fois**, sous
instrumentation, sans une erreur. C'est exactement la mesure que le journal de l'étape 4 déclarait
manquante — et c'est **plus fort que la prédiction d'origine**, qui portait sur 1000 lignes exercées
sous le CRT de Release.

⚠ **Ce que cette levée ne couvre toujours pas** : les **fuites**, pour la raison B. La réserve H3 de
`debt_cgmesh.md` (« quantification réelle des fuites ») reste entière, et la croissance de ≈ 100 Kio
par sélection mesurée à l'étape 5 reste un **écart non résolu** — sous ASan elle vaut **+864 Kio sur
3 sélections**, l'allocateur instrumenté étant plus gourmand.

#### Les huit points de l'étape, un par un

**1. Les erreurs de chargement arrivent dans la zone de statut.** Trois chemins chargent un fichier
— argument de ligne de commande, remplacement de scène, ajout depuis la liste — et **deux d'entre
eux n'écrivaient que sur `stderr`** : dans l'application, l'échec était **invisible**.
`Application::ReportLoadFailure` est le site unique ; les trois l'appellent.
⚠ **L'instrument est neuf, parce que le statut est une chaîne du modèle lié, donc invisible de la
console.** `--dump-status` l'imprime après la boucle et exige les **deux** sous-chaînes séparément
— le nom **et** la raison — en nommant celle qui manque. Il vérifie aussi le cas **qui réussit** :
le statut doit alors être **vide**.

**2. Les lignes de plafond — les quatre sont là, et une cinquième est INATTEIGNABLE.** Audit
mesuré, pas relu :

| Plafond | Ligne | Mesure |
|---|---|---|
| Arbre, global `kMaxVisibleRows` | `... plafond de 1000 ligne(s) atteint, la suite n'est pas affichee` | ✅ 1200 sous-répertoires → `lignes : 1001`, la ligne est la dernière. Et sur **trois** répertoires de 400 à profondeur 2, où aucun nœud n'atteint son propre plafond : même ligne |
| Liste de fichiers, `kMaxFiles` | `... N entree(s) non affichee(s)` (dans le `.rml`) | ✅ 1200 fichiers `.obj` → `fichiers : 1000`, `non affiches : 200`, et l'élément est **mis en page** : `--dump-layout --layout-element files-overflow` rend `1390 x 16` |
| Arbre des maillages, `kMaxInfoRows` | `... plafond de N ligne(s) atteint : …` | ✅ couvert par `LArbreDesMaillagesAnnonceSaTroncature` (étape 5), re-exécuté |
| Arbre des matériaux, `kMaxInfoRows` | idem | ✅ couvert par `LArbreDesMateriauxAnnonceSaTroncatureSansDecalage`, re-exécuté |
| **Arbre, par nœud `kMaxChildrenPerNode`** | `... N entree(s) non affichee(s)` | ⚠ **INATTEIGNABLE, et c'est mesuré** — voir ci-dessous |

⚠ **La cinquième ligne ne peut pas sortir tant que `kMaxChildrenPerNode == kMaxVisibleRows == 1000`.**
Un nœud qui dépasse son plafond porte au moins 1001 enfants, donc le **total** franchit le plafond
global **avant** que sa ligne de débordement ne soit posée. **Mesuré** : `--dump-tree` sur 1200
sous-répertoires rend **1** ligne « plafond de 1000 ligne(s) atteint » et **0** ligne
« non affichee(s) ». Ce n'est **pas** une ligne oubliée : elle redevient atteignable dès que
`kMaxChildrenPerNode` passe **sous** `kMaxVisibleRows`, et ce réglage appartient au lot qui reprendra
les plafonds. C'est inscrit dans `core/directory_tree.h`, à côté des deux chiffres.

⚠ **Le détecteur de la ligne de liste est falsifiable dans les deux sens** : plafond atteint →
`1390 x 16`, `EXIT=0` ; plafond **non** atteint (la fixture) → `l'element "files-overflow" n'occupe
aucune surface pour ce cadre`, **`EXIT=1`**. L'identifiant a été ajouté pour cela : c'est la **seule**
des cinq lignes à vivre dans le document plutôt que dans un vecteur lié, donc la seule qu'aucun
oracle textuel ne couvre.

**3. Icône de fenêtre — `glfwSetWindowIcon` SEUL, et le `.rc` n'est pas repris.** Trois raisons,
dans l'ordre de poids :
- **Une ressource Win32 est MSVC seule.** `ci-linux-peles` existe, et le lot vise les deux
  plateformes : un `.rc` laisserait Linux sans icône, sans aucun message.
- **Les deux ne désignent pas le même objet.** Une ressource `.rc` habille le **fichier
  exécutable** (Explorateur, raccourcis) ; `glfwSetWindowIcon` habille la **fenêtre** (barre de
  titre, barre des tâches, par `WM_SETICON`). Prendre l'un pour l'autre est l'erreur courante.
- **`sinaia/sinaia.rc` n'est pas réutilisable sans le copier**, et `sinaia/` est en lecture seule.
  Dépendre d'un asset d'une autre application serait un couplage que tout le lot a évité.

Deux images sont versionnées, **16×16 et 32×32** (`peles/assets/icon-16.png`, `icon-32.png`) : Win32
demande une petite icône pour la barre de titre et une grande pour la barre des tâches, et glfw
choisit dans le tableau celle dont la taille est la plus proche. Elles sont décodées par
**`Img::load_from_memory`**, qui reconnaît le format **au contenu** — et non par `Img::load`, dont
le chemin PNG est derrière `CG_HAS_ZLIB` : l'icône ne dépend d'aucune option de construction.
Le décodage vit dans `app/`, **pas** dans `peles_platform`, qui ne lie ni `cgimg` ni `cgre` ;
`Window::SetIcon` ne voit que des octets RGBA.
⚠ **Un échec est COSMÉTIQUE et ne change pas le code de sortie** — contrairement au document RML et à
la police, sans lesquels il n'y a rien à l'écran. Il est **nommé** sur `stderr` : `peles : icone
illisible ou absente : <chemin>`. **Mesuré sur ses deux versants**, dans la copie **déployée**
et sans recompiler : les deux fichiers présents ⇒ **zéro** ligne « icone », `EXIT=0` ;
`icon-16.png` retirée ⇒ `peles : icone illisible ou absente : <chemin>`, la fenêtre s'ouvre,
`EXIT=0`. Restauré, re-vérifié muet.
⚠ **Ce qui n'est pas vérifiable, et qui est donc déclaré** : que l'icône soit **à l'écran**. glfw ne
rend rien, et sous Wayland l'appel est un `GLFW_FEATURE_UNAVAILABLE`. Le retour de `Window::SetIcon`
signifie « la demande a été faite », pas « l'icône est visible » ; le commentaire le dit.

**4. `Rml::Shutdown` avant la destruction des interfaces — TIENT ENCORE.** Vérifié **par lecture**,
et le contrat est porté par deux mécanismes distincts : le corps de `~RmlHost` appelle
`Rml::Shutdown()`, et `m_system`/`m_render` sont des **membres**, donc détruits **après** ce corps,
dans l'ordre inverse de leur déclaration (`platform/rml_host.h`, « ⚠ ORDRE CRITIQUE »). Le seul
chemin où `Rml::Shutdown()` ne court pas — échec d'`Rml::Initialise()` — reprend les deux pointeurs
par `Rml::Set*Interface (nullptr)`. ⚠ **Statut exact : lecture, pas mesure.** Ce qui s'en approche le
plus est indirect : aucune des neuf exécutions sous ASan ne rapporte de `use-after-free`, et toutes
détruisent l'hôte.

**5. L5 — la perte de focus. FERMÉE.** glfw appelle le rappel de focus **avant** de synthétiser les
relâchements (`extern/glfw-3.3.10/src/window.c:45-68`, lu) : une annulation mise en file à cet
instant précède donc les relâchements. `ui::MouseEvent::Kind::Cancel` est ajouté ; l'arbitre retient
le propriétaire, ferme le geste, et **avale** les relâchements des boutons annulés — sans quoi RmlUi
recevrait un `ButtonUp` sans `ButtonDown`. `Application::CancelViewportGesture` relâche le trackball
**sans consulter le seuil clic/glisser**, ce qui est toute la différence avec `EndViewportGesture`.
⚠ **Détecteur neuf, `--focus-loss <x> <y>`, et sa MOITIÉ UTILE est le témoin.** « Rien n'a été
sélectionné » est vrai aussi d'un pixel qui ne désigne aucun modèle, d'un arbitrage cassé ou d'un
picking mort : l'instrument exerce donc **d'abord** la séquence normale sur le même pixel, qui
**doit** sélectionner.

| | Mesure |
|---|---|
| Forme corrigée | `temoin : a selectionne`, `avec annulation : n'a rien selectionne`, `bouton encore enfonce : non`, `GESTE ANNULE`, **`EXIT=0`** |
| **Cas négatif** — branche `Cancel` de l'arbitre neutralisée, c'est-à-dire l'état d'avant la correction | `temoin : a selectionne`, `avec annulation : **A SELECTIONNE**`, `GESTE TERMINE AU LIEU D'ETRE ANNULE`, **`EXIT=1`** |

**6. La coupe de `dump_modes.cpp` — faite, et elle en a entraîné une seconde.**
`dump_modes.cpp` faisait **1814 lignes** ; `application.cpp`, à force de recevoir les finitions de
cette étape, en faisait **1808**. Fermer la première dette en ouvrant la même à côté n'aurait rien
fermé du tout : les deux sont coupées.

| Avant | Après | Ligne de coupe |
|---|---|---|
| `dump_modes.cpp` **1814** | `dump_state` 361 · `dump_geometry` 202 · `dump_gestures` 616 · `dump_effects` 451 · `dump_session` 421 · `dump_oracles` 229 | **ce que la mesure observe**, et cela se lit dans les inclusions : `dump_state` est la **seule** à inclure `gl_wrapper.h`, `dump_oracles` la **seule** à n'inclure ni RmlUi ni GL — la propriété exacte qui rend `--dump-tree`/`--dump-files`/`--dump-info` exécutables en CI |
| `application.cpp` **1808** | `application` 325 · `application_create` 408 · `application_scene` 417 · `application_loop` 294 · `application_modes` 535 | **le rôle** : la création, l'ordre des huit phases et la boucle · la scène et les entrées · les instruments qui agissent depuis la boucle · les modes « vider et sortir » |

`app/dump_modes.h` (624 l.) reste **l'unique** en-tête de déclaration des six unités ; `app/
application.h` (634 l.) celui des cinq autres. Un `app/application_internal.h` de **30 lignes**
porte le seul symbole réellement partagé, `kViewportElementId` — tout ce qui n'a qu'un lecteur est
resté dans l'espace anonyme de son unité.

⚠ **Le détecteur de la coupe est la CONSERVATION DES LIGNES, pas le fait que ça compile.** Un
découpage par index de texte emporte ses voisins sans qu'aucun diagnostic ne sorte. Le détecteur
compare le multi-ensemble des lignes non vides avant et après :
- `dump_modes.cpp` : **1756 lignes de corps, 0 perdue**, 169 ajoutées (les six en-têtes) ;
- `application.cpp` : **1560 lignes, 1 « perdue »** — `kViewportElementId`, la seule ligne
  délibérément réécrite, devenue `inline constexpr` dans l'en-tête interne.

⚠ **Le détecteur est validé sur son cas POSITIF** : la même comparaison, une unité omise du calcul,
rend **307 lignes perdues**. Sans cette validation, « 0 perdue » ne voudrait rien dire.

⚠ **Et la preuve de comportement est indépendante de lui** : les **16 instruments** rendent une
sortie **identique octet pour octet** — `stdout`, `stderr` **et** code de sortie — avant et après les
deux coupes. La seule différence de tout le lot est une ligne que j'ai retirée sciemment de
`--dump-copy` (voir le point 8).

⚠ **Reste dû, et nommé** : `peles/probe/main.cpp` fait **1001 lignes**. Il date de l'étape 1, il n'est
pas dans le périmètre de celle-ci, et il est le dernier fichier de `peles/` au-dessus du seuil.

**7. Narration hors périmètre — retirée.** Les deux commentaires de `main.cpp` (`:125` et `:530`)
énoncent désormais la règle en vigueur au lieu de l'historique de sa découverte. ⚠ **Vérifié sur tout
`peles/`** : une recherche des formes de la même famille (`auparavant`, `avant correction`,
`laissait`, renvois aux numéros d'étape, renvois aux identifiants `R-P*`) ne rend **aucune** autre
occurrence.

**8. D-5 — la ligne `Path`. INSTRUITE, TRANCHÉE, ET REFERMÉE, sous une réserve nommée.**

*Ce que l'instruction a établi, par lecture de RmlUi 6.3 :*
- `readonly` **n'existe pas** — confirmé ; `disabled` pose `Focus::None` et retire donc la sélection.
- **La sélection à la souris ne demandait RIEN** : `WidgetTextInput` s'abonne à `Mousedown` et
  `Drag` (`Source/Core/Elements/WidgetTextInput.cpp:172-178`), et `Context::ProcessMouseButtonDown`
  donne le focus à l'élément survolé (`Source/Core/Context.cpp:628-641`). Le routage souris de
  l'étape 6 suffisait. **L'affirmation « la ligne n'est pas sélectionnable » était une déduction, pas
  une mesure.**
- `Ctrl+C` passe par `KI_C` + `KM_CTRL` → `CopySelection()` → `GetSystemInterface()->
  SetClipboardText` (`WidgetTextInput.cpp:597-601`, `:787-792`). Il manquait donc **deux** choses :
  que la touche arrive, et que l'écriture sorte du processus.

*Ce qui est faisable dans ce lot, et qui est fait :*
- **Routage clavier**, `Context::ProcessKeyDown`/`ProcessKeyUp`, sur **dix touches seulement** —
  A, C, ←, →, ↑, ↓, Origine, Fin, Page haut, Page bas. ⚠ **C'est cette liste qui tient la ligne non
  modifiable**, faute de `readonly` : `ProcessTextInput` n'est appelé nulle part, et ni `V`, ni `X`,
  ni `Retour arrière`, ni `Suppr` n'entrent. **Une entrée ajoutée à `ui::Key` rend la ligne
  modifiable** ; c'est écrit à côté de l'énumération.
- **Surcharge de `SetClipboardText` ET de `GetClipboardText`** vers `glfwSetClipboardString` /
  `glfwGetClipboardString`. Les deux ensemble : surcharger la seule écriture laisserait la lecture
  puiser dans le tampon interne de RmlUi, qui ne recevrait plus rien.

*Ce qui n'est PAS faisable ici, et qui est donc déclaré :*
⚠ **Le dernier maillon — glfw vers le presse-papier du système — n'est pas mesuré sur cette
machine.** La session n'a pas de bureau interactif : `OpenClipboard` y échoue **pour tout le monde**,
et pas seulement pour `peles` — vérifié indépendamment, `powershell Set-Clipboard` rend
*« Échec de l'opération du Presse-papiers demandée »*.

`--dump-copy` rend donc un verdict **en deux étages**, et c'est ce qui le rend utilisable quand même :

| Étage | Ce qu'il établit | Mesure |
|---|---|---|
| 1 — **toujours vérifié** | le clic donne le **focus**, les touches sont **routées**, la **sélection** est non vide, et c'est **notre** `SetClipboardText` que RmlUi appelle | `element focalise : textarea`, `copies demandees a l'interface : 1`, `dernier texte copie : "test/data/obj/cube.obj"` — la valeur **affichée**, relue sur l'élément |
| 2 — **sauté ici, et nommé** | glfw écrit bien dans le presse-papier du **système** | `presse-papier du systeme : INACCESSIBLE DANS CETTE SESSION`, et le verdict final le dit en toutes lettres |

⚠ **Une sentinelle est semée avant le geste, et elle est semée PAR LA FENÊTRE, pas par l'interface
de RmlUi.** Sans elle, un presse-papier contenant déjà la valeur attendue rendrait le verdict vert
sans que le geste ait rien fait ; en passant par l'objet qu'on mesure, la mesure serait tautologique.

⚠ **L'instrument a été pris en défaut une fois, et c'est instructif.** Sa première forme appelait
`InputRouter::RouteKey` **directement**, donc sautait le drainage de la file par l'hôte — le premier
des deux maillons du routage clavier. **Mesuré** : drainage neutralisé, le verdict restait **VERT**.
Il passe désormais par la file de l'hôte (`KeySink`), et le même cas négatif rend
`copies demandees a l'interface : 0`, **`EXIT=1`**.

**Les deux cas négatifs de D-5, mesurés :**

| Cas négatif injecté | Sortie |
|---|---|
| Le drainage clavier de l'hôte est neutralisé | `element focalise : textarea`, `copies demandees : 0`, `LIGNE NI SELECTIONNABLE NI COPIABLE`, **`EXIT=1`** |
| `SetClipboardText`/`GetClipboardText` ne sont plus des surcharges | idem, `copies demandees : 0`, **`EXIT=1`** — RmlUi appelle alors l'implémentation de base, qui écrit dans une chaîne membre |

#### ⚠ Le défaut trouvé pendant l'étape, et qui n'était dans aucune liste

**Un rétrécissement de vecteur lié faisait sortir le processus en 3, une fois sur deux.** Il est
décrit en §1.5 (septième contrainte). Ce qu'il faut retenir ici :

- **Il est antérieur à cette étape** : reproduit à l'identique sur le binaire de l'étape 6, conservé
  avant la première modification.
- **Il est non déterministe**, donc six revues l'ont traversé : `--reopen` **5/10**, `--open-second`
  **5/10**, `--churn-tree 100` **1/10**, `--reselect 3` **0/10**.
- **Sa cause est lue, pas devinée** : égalité de `sort_order` entre `DataViewFor` et
  `DataViewAttribute` posés sur le **même** élément, et `std::sort` n'est pas stable.
- **La correction est mesurée sur les deux versants** : 5/10 → **0/48**, et les seize instruments
  rendent une sortie **identique octet pour octet**.
- ⚠ **C'est aussi une leçon sur le tableau de non-régression** : une ligne « `--reopen` → `EXIT=0` »
  relevée **une fois** ne dit rien d'un défaut qui tombe une fois sur deux. Les instruments qui
  rétrécissent un vecteur lié ont été relancés **dix à douze fois chacun**.

#### Un piège de construction, rencontré et à connaître

⚠ **`mv fichier.orig fichier` NE FAIT PAS RECOMPILER SOUS MSBuild.** `mv` conserve la date du
fichier déplacé, qui est antérieure au `.obj` produit par la construction du cas négatif : la
restauration passe alors inaperçue et **le binaire mesuré contient encore l'injection**. Constaté
ici : deux instruments rendaient `EXIT=1` sur des sources pourtant restaurées. **Règle** : après
toute restauration par `mv`, `touch` les fichiers avant de reconstruire — et relire le verdict.

#### Mesures de non-régression — 30 exécutions, 30 conformes

| Critère | Mesure |
|---|---|
| `peles_probe` | **7/7**, `EXIT=0` (P8 `SKIP`, inchangé) |
| `--assets <inexistant>` | `le repertoire n'existe pas`, **`EXIT=3`**, sans fenêtre |
| `--state-audit test/data/rabbit.obj` | `AUDIT OK`, **25 états**, 3 images, **2 phases**, `EXIT=0` |
| `--dump-layout 1400x900` | `(240, 0, 880, 720)` ; `1401x900` → `w=881` ; `1400x901` → `h=721` |
| `--dump-layout 1400x900 --browse <fixture> --browse-depth 3 <60 matériaux>` | **le même rectangle** |
| `--dump-pick` sur **trois** tailles | rayon central `(-0.813798, 0.469846, -0.342020)` aux trois, écart `0.000000000`, `EXIT=0` |
| `--dump-pick --document layout_probe.rml` | même rayon, écart `0.000000000`, `EXIT=0` |
| `--readpixel 620 490 test/data/obj/cube.obj` | **(25,128,204)**, `PIXEL CONFORME`, `EXIT=0` |
| Les **trois** oracles | `--dump-tree`, `--dump-files`, `--dump-info` (chemin **relatif** `test/data/peles/info/panels.obj`) **identiques octet pour octet** par `cmp` **et** par les 3 tests CTest — **3/3 Passed**. **Aucun régénéré** |
| `TU --gtest_filter=PelesCore*:SupportedFormats*` | **26 PASSED** |
| `TU` complet, `ENABLE_SINAIA=ON` | **1872 PASSED**, 0 échec |
| `--reopen` | `CONTRAT DE DESTRUCTION TENU`, 3 clés rendues, `EXIT=0` — **12 exécutions sur 12** |
| `--reselect 3 many_materials.obj` | `1`, `1`, `14/4/242` stables, `AUCUNE ACCUMULATION`, `EXIT=0` |
| `--churn-tree 100` sur 100 images | `REPLI OK`, de 1 à 5 lignes, `EXIT=0` — **12 sur 12** |
| `--dump-scroll many_materials.obj` | barre **PRESENTE**, **16 crans**, défilement **3714 px**, dernière ligne dans la bande visible, `EXIT=0` |
| `--open-second panels.obj cube.obj` | `modeles dans la scene : 2`, `ligne "File" : panels.obj`, `EXIT=0` — **12 sur 12** |
| `--input-script` | `ARBITRAGE CONFORME` sur les deux versants, `EXIT=0` |
| `--click 60 700 --click-expect select` | `CLIC ROUTE`, `EXIT=0` |
| `--click 60 40 --click-expect toggle` | `CLIC ROUTE`, `EXIT=0` |
| `--double-click 60 700` | `DOUBLE-CLIC ROUTE`, la scène grandit, `EXIT=0` |
| `--drag 620 310 520 260` (interne) | rotation **21,1405 °**, ⚠ `ARRIVEE DANS LA VUE`, `EXIT=0` |
| `--drag 620 310 120 260` (sortant) | rotation **79,2256 °**, `ARRIVEE HORS DE LA VUE`, `EXIT=0` |
| `--drag-bump left` | rotation **79,2256 °**, œil déplacé de **3,462963**, `GESTE INTACT`, `EXIT=0` |
| `--drag-bump right` | œil déplacé de **0,475308**, aucune sélection, `GESTE INTACT`, `EXIT=0` |
| `--alt-pivot 250 10` / `620 310` | `IDENTIQUE AU BIT PRES` / `PIVOT POSE SUR LA SURFACE`, `EXIT=0` |
| **`--focus-loss 620 310`** | `GESTE ANNULE`, témoin vert, `EXIT=0` |
| **`--dump-copy`** | `copies demandees : 1`, valeur affichée copiée, `EXIT=0` sous réserve nommée |
| **`--dump-status`** sur illisible / sur lisible | `STATUT CONFORME` dans les deux sens, `EXIT=0` |
| `--frames 120 --dump-timing <60 matériaux>` | image médiane **0,622 ms**, `Update()` médiane **0,031 ms**, 120 images rendues |
| Journal RmlUi sur le document du produit | `--frames 2`, `--frames 60`, `--frames 2 --debugger`, `--frames 10 --churn-tree 5` : **zéro** message `LT_WARNING` ou supérieur, `EXIT=0` sur les quatre |
| `ENABLE_CGGRAPH_BOILERPLATE=ON` **et** `ENABLE_PELES=ON` | configurent et bâtissent `cggraph-boilerplate.exe`, `peles.exe` et `peles_probe.exe`, **un seul** `glfw3.lib` |
| Avertissements du compilateur | **zéro** sur `peles/`, reconstruction **complète** (`--clean-first`). Les restants viennent de `src/cgmesh` et `extern/glutess`, et sont antérieurs |
| Pare-feu d'inclusion | ⚠ **SONDAGE, PAS LA BATTERIE COMPLÈTE.** Deux des dix-huit cas négatifs ont été re-exercés ici — `#include "gl_wrapper.h"` depuis `core/loader.cpp` et `#include "app/application.h"` depuis `view3d/picking.cpp` — et les **deux** rendent `error C1083`. Les seize autres reposent sur le relevé de l'étape 6 : les chemins d'inclusion des cinq cibles n'ont pas changé, et les onze unités neuves sont toutes dans l'exécutable |
| Icône de fenêtre | les deux images présentes ⇒ **zéro** message ; `icon-16.png` retirée de la copie déployée ⇒ `peles : icone illisible ou absente : <chemin>`, fenêtre ouverte, `EXIT=0` |

### Passe de clôture, mesurée le 2026-09-22

**Revue de clôture : APPROVE avec commentaires — 0 critical, 0 HIGH, 7 medium, 8 low.** Cette passe
ne change **aucun comportement** : elle corrige des **affirmations écrites devenues fausses**, pose
deux mécanismes à la place de deux phrases, et fournit **la mesure qui manquait**. Arbre :
`build/peles-sinaia` (Release, `ENABLE_PELES=ON` + `ENABLE_SINAIA=ON`),
`GL_VERSION = "3.3.0 - Build 32.0.101.6078"`.

#### ⚠ M6 — le cas négatif de `--dump-status`, qui manquait

C'était **le seul instrument neuf de l'étape 7 sans cas négatif injecté**. Il en a désormais
**deux**, un par versant du verdict.

| Injection | Fichier mesuré | Sortie | `EXIT` |
|---|---|---|---|
| `m_ui->SetStatus (name + " : " + reason)` **neutralisé** dans `ReportLoadFailure` (`app/application_scene.cpp:65`) | `test/data/peles/info/nonexistent.obj` | `statut a l'ecran : ""`, `nom attendu … -> ABSENT`, `raison attendue … -> ABSENTE`, **`STATUT INCOMPLET : … Les deux manquent.`** | **1** |
| `m_ui->SetStatus ("CAS NEGATIF INJECTE")` **ajouté** après un chargement réussi (`app/application_create.cpp`, fin de `CreateSceneAndDocument`) | `test/data/peles/info/panels.obj` | `statut a l'ecran : "CAS NEGATIF INJECTE"`, **`STATUT INATTENDU : le chargement a REUSSI et la zone de statut porte …`** | **1** |

**Les deux fichiers ont été restaurés, `touch`és, reconstruits, et re-vérifiés verts** :
`STATUT CONFORME : le fichier et la raison sont nommes.` → `EXIT=0`, et
`STATUT CONFORME : chargement reussi, zone de statut vide.` → `EXIT=0`.
⚠ Le piège de `mv` consigné plus haut a été évité : `touch` avant chaque reconstruction, et
`grep -c "CAS NEGATIF INJECTE"` rend **0** sur les deux fichiers après restauration.

#### Les deux mécanismes posés à la place d'une phrase

| Ce qui était écrit | Ce qui le tient maintenant | Cas positif |
|---|---|---|
| `window_glfw.h` : *« la correspondance des dix `kKey*` EST VÉRIFIÉE PAR LE COMPILATEUR »* — **faux**, les `static_assert` étaient **six** et aucune sur les touches ; le pont est un cast nu | **Dix `static_assert` ajoutés** dans `app/application.cpp` (soit **seize**) | `kKeyEnd`/`kKeyPageUp` permutés dans `window_glfw.h` → `error C2338: 'Fin n'a pas le meme code des deux cotes'` **et** `'PagePrec n'a pas le meme code des deux cotes'`. Restauré, re-vérifié vert |
| `debt_peles.md` : *« cinq plafonds, cinq lignes de débordement, **chacune** mesurée »* — **faux**, la cinquième est inatteignable (§7 du même document) | `DirectoryTree::SetMaxChildrenPerNode(int)`, **défaut inchangé**, + **3 tests** dans `test/tu_peles_core.cpp` | `node.hidden = 0` injecté dans `directory_tree.cpp` → `PelesCoreDirectoryTree.UnPlafondParNoeudAbaisseRendLaLigneDeDebordement` **FAILED** (4 lignes au lieu de 5). Restauré, re-vérifié vert |

⛔ **Les deux plafonds n'ont PAS été rendus distincts.** Les chiffres sont mesurés (étape 4,
≈ 7,7 µs/ligne) ; les changer pour rendre une ligne atteignable aurait été l'inversion que §1.12 a
refusée pour l'éclairage. Le plafond est **injecté par le test**, pas déplacé dans le produit —
et `debt_peles.md` §7 dit maintenant que **le binaire livré ne produit toujours pas cette ligne**.

#### Ce que la passe a mesuré au-delà de ce que la revue avait compté

⚠ **Trois chiffres de la revue étaient des minorants.** Ils sont corrigés à leur instrument.

| Sujet | Revue | Mesuré ici | Instrument |
|---|---|---|---|
| Longueur des fonctions au-dessus de 50 lignes | **3** (`Route`, `DumpCopy`, `ReportSelectionMemory`) | **24** — en tête `DumpScroll` 107, `ParseArguments` 104, `Route` 97, `DumpCopy` 96 | script Python sur `peles/**/*.{cpp,h}`, lignes **de code** du corps ; biais : **minore** (rate une fonction non indentée selon la convention) |
| Sorties console non ASCII | **4 sites** | **10 sites** — 5 dans `dump_effects.cpp`, 1 dans `dump_session.cpp`, 4 dans `dump_state.cpp` (`«` `»`, `⚠`, `n°`) | `awk` sur `[^\x00-\x7F]` hors lignes de commentaire, sur **tout** `peles/` ; après correction : **0** |
| Unités tirant `<windows.h>` | **5** | **9** — 7 par `gl_wrapper.h`, 2 directement (`asset_paths.cpp`, `dump_session.cpp`) | `grep -l` sur `gl_wrapper.h` et `windows.h` |

Et une divergence de moins : `<cstdlib>` est **aussi** inutile dans `dump_effects.cpp`, que la revue
n'avait pas listé. En revanche `dump_state.cpp` **utilise** `<cstdlib>` (`std::abs(int)`, `:280`) et
`dump_session.cpp` a besoin de `std::size_t` — remplacé par `<cstddef>`, pas supprimé.

#### Corrections d'écriture, sans changement de comportement

| # | Site | Ce qui était faux |
|---|---|---|
| **M5** | `peles/assets/peles.rcss:357-365` | *« La ligne reste donc **EDITABLE** localement »* — contredisait la garantie D-5. Le seul des quatre sites sur cette ligne à le faire, et **celui où l'on va pour casser la garde**. Remplacé par la règle en vigueur : la ligne **n'est pas modifiable**, et ce n'est pas `readonly` (absent de RmlUi 6.3) mais la **liste de dix touches** de `ui/input_event.h` |
| **M1** | `peles.rml:24-26`, `peles.rcss:224-229`, `debt_peles.md` | La règle ne nommait que `data-attr-*` et `data-style-*`. **Mesuré sur RmlUi 6.3** : `DataView::DataView` pose `sort_order = biais + 1000 + 2000 × profondeur` (`Source/Core/DataView.cpp:27`) et le biais vaut **0** pour `DataViewFor` **comme pour** `DataViewAttribute`, `DataViewStyle`, `DataViewClass`, `DataViewIf`, `DataViewVisible`, `DataViewRml`, `DataViewAttributeIf`, `DataViewAlias` (`sort_offset = 0` par défaut, `DataViewDefault.h:16`). La règle porte donc sur **toute vue `data-*`**. Les `data-event-*` en sont la seule exception : **contrôleurs**, pas vues (`Factory.cpp:252`). ⚗ Et le `{{ … }}` non plus : `DataViewText` s'attache au **nœud de texte** (`DataViewDefault.cpp:274`), donc un niveau plus bas — vérifié, pas supposé |
| **M7** | `debt_peles.md` « Ce qui est sain » | *« cinq lignes de débordement, chacune mesurée »* → **quatre sur cinq** |
| **M2** | `app/application.cpp:44,46` | `kDataModelName` et `LogToStderr` dupliqués depuis `application_create.cpp` et **inutilisés** ici. Sous `/W3`, `C4505` est de **niveau 4** : aucun diagnostic. Supprimés — `application_internal.h` disait déjà que ces deux-là appartiennent à l'espace anonyme d'**une seule** unité |
| **M3** | les six `dump_*.cpp` | Bloc `<algorithm> <cmath> <cstdio> <cstdlib> <cstring>` recopié verbatim. Réduit à ses inclusions réelles ; `application.cpp` réduit de six inclusions à `<memory>` seul |
| **L1** | `peles/CMakeLists.txt:248-262`, `probe/main.cpp:857` | Attribuait à `application.cpp` `cgre::SetLogSink` et le décodage de l'icône : les deux vivent dans `application_create.cpp` depuis la coupe. Et *« l'étape 2 ajoute `--state-audit` ici même »* — §1.2 a corrigé cela : le mode est porté par `peles`, la sonde ne liant pas RmlUi |
| **L3** | `ui/input_router.cpp:39-42` | *« LE COMPILATEUR LE VÉRIFIE »* : `C4062` est de niveau 4, `-Wswitch` est attaché à `-Wall`, et la racine ne pose ni l'un ni l'autre hors `ENABLE_COVERAGE` (`CMakeLists.txt:254`). La phrase dit maintenant que l'ajout d'une entrée à `ui::Key` est **silencieux** |
| **L4** | `app/application_create.cpp` | Seule ressource non liée à un objet du périmètre : un `std::FILE*` nu traversant une allocation `std::vector` qui peut lever. Devenu `std::unique_ptr<std::FILE, FileCloser>` — foncteur plutôt que `&std::fclose`, dont l'adresse n'est pas garantie par `[namespace.std]` |
| **L7** | 10 sites | Sortie console en ASCII. ⚠ **Les trois oracles comparés par ctest ne sont pas touchés** : ils sortent de `dump_oracles.cpp`, qui était déjà propre — les trois `cmp` et les trois tests CTest le confirment. La chaîne du verdict `--drag` perd son `⚠` : `ORBITE EFFECTUEE, PIVOT INCHANGE AU BIT PRES. ARRIVEE DANS LA VUE : …` |
| **L6 / L8 / L5** | `debt_peles.md` §7, §12, §13 | **Inscrits, pas corrigés.** L8 est **réfuté comme défaut** — `Camera::EndDrag` passe `x, y` à `Ctrackball::mouse_press`, qui sur `RELEASED` exécute `else tb_state = RELEASED;` (`src/cgre/examinator_trackball.cpp:129-130`) et **ne les lit pas** — seule la phrase d'`application_scene.cpp:299-300` est fausse, pour `Leave` |

#### Non-régression — la totale, relancée

| Critère | Mesure |
|---|---|
| `peles_probe` | **7/7**, `EXIT=0`, P8 `SKIP` (inchangé) |
| `--assets <inexistant>` | `le repertoire n'existe pas`, **`EXIT=3`** |
| `--state-audit test/data/rabbit.obj` | `AUDIT OK`, **25 états**, 3 images, **2 phases**, `EXIT=0` |
| `--dump-layout` ×3 | `1400x900` → `x=240 y=0 w=880 h=720` ; `1401x900` → `w=881` ; `1400x901` → `h=721` |
| `--dump-layout 1400x900 --browse <fixture> --browse-depth 3` | **le même rectangle** |
| `--dump-pick` ×3 tailles | `1400x900`, `1000x700`, `1600x1000` : `RAYON CENTRAL COLINEAIRE A L'AXE DE VUE`, `EXIT=0` |
| `--dump-pick --document layout_probe.rml` | idem, `EXIT=0` |
| `--readpixel 620 490 cube.obj` | **`(620, 490) = (25,128,204)`**, `PIXEL CONFORME`, `EXIT=0` |
| Les **trois** oracles | `cmp` : **identiques octet pour octet** ; CTest : `peles_dump_tree_oracle`, `peles_dump_files_oracle`, `peles_dump_info_oracle` — **3/3 Passed**. Aucun régénéré |
| `TU --gtest_filter=PelesCore*:SupportedFormats*` | **29 PASSED** (26 + les **3** nouveaux) |
| `TU` complet, `ENABLE_SINAIA=ON` | **1875 PASSED**, 0 échec (1872 + 3) |
| **`--reopen`** | `CONTRAT DE DESTRUCTION TENU`, 3 clés rendues — **12 exécutions sur 12**, `EXIT=0` |
| **`--open-second`** | `modeles dans la scene : 2` — **12 sur 12**, `EXIT=0` |
| **`--churn-tree 100`** sur 100 images | `REPLI OK`, de 1 à 7 lignes — **12 sur 12**, `EXIT=0` |
| `--dump-scroll many_materials.obj` | barre **PRESENTE**, **16 crans**, `DERNIERE LIGNE ATTEIGNABLE`, `EXIT=0` |
| `--input-script` | `ARBITRAGE CONFORME`, `EXIT=0` |
| `--click 60 700 --click-expect select` / `60 40 --click-expect toggle` | `CLIC ROUTE`, `EXIT=0` sur les deux |
| `--double-click 60 700` | `DOUBLE-CLIC ROUTE`, la scène grandit, `EXIT=0` |
| `--drag` interne / sortant | **21,1405 °** `ARRIVEE DANS LA VUE` / **79,2256 °** `ARRIVEE HORS DE LA VUE`, `EXIT=0` |
| `--drag-bump left` / `right` | **79,2256 °**, œil **3,462963**, `GESTE INTACT` / rotation **0,0000 °**, œil **0,475308**, `GESTE INTACT` |
| `--alt-pivot 250 10` / `620 310` | `IDENTIQUE AU BIT PRES` / `PIVOT POSE SUR LA SURFACE`, `EXIT=0` |
| `--focus-loss 620 310` | `GESTE ANNULE`, `EXIT=0` |
| `--dump-copy` | `copies demandees a l'interface : 1`, `LIGNE SELECTIONNABLE ET COPIABLE, SOUS UNE RESERVE NOMMEE`, `EXIT=0` |
| `--dump-status` illisible / lisible | `STATUT CONFORME` dans les deux sens, `EXIT=0` |
| `--reselect 3 many_materials.obj` | `3/3`, `14/4/242` stables, `AUCUNE ACCUMULATION`, **+48 Kio** (tolérance 1024 Kio), `EXIT=0` |
| Journal RmlUi sur le document du produit | `--frames 2`, `--frames 60`, `--frames 2 --debugger`, `--frames 10 --churn-tree 5` : **zéro** message `LT_WARNING` ou supérieur, `EXIT=0` sur les quatre. ⚠ C'est ce qui établit que les commentaires ajoutés dans `peles.rml` n'ont rien cassé : RmlUi **parse** ce fichier |
| `ENABLE_CGGRAPH_BOILERPLATE=ON` **et** `ENABLE_PELES=ON` | `cggraph-boilerplate.exe`, `peles.exe`, `peles_probe.exe` bâtis dans `build/peles-boilerplate`, **un seul** `glfw3.lib` |
| Icône de fenêtre, **après le passage en `unique_ptr`** | deux images ⇒ **zéro** message, `EXIT=0` ; `icon-16.png` retirée ⇒ `peles : icone illisible ou absente : <chemin>`, fenêtre ouverte, `EXIT=0` |
| Avertissements du compilateur | **zéro** sur `peles/`, après `touch` de **tous** les `.cpp`/`.h` de `peles/` et reconstruction de `peles` et `peles_probe` |
| Pare-feu d'inclusion | ⚠ **SONDAGE, comme à l'étape 7.** Les deux mêmes cas négatifs re-exercés après la réduction des inclusions : `#include "gl_wrapper.h"` depuis `core/loader.cpp` et `#include "app/application.h"` depuis `view3d/picking.cpp` → **`error C1083`** tous les deux. Les seize autres reposent sur le relevé de l'étape 6 |

⚠ **Ce que cette passe N'A PAS mesuré** : rien sous ASan (l'arbre `build/peles-asan` n'a pas été
reconstruit — aucune des modifications n'est un changement de comportement), rien sous UBSan (voir
§8.3), `--probe-drag` toujours jamais lancé, `ci-linux-peles` toujours jamais exécuté.

---

## 8. Bilan du lot 1

### 8.1 Les sept étapes

| # | Contenu | Verdict | Mesures clés |
|---|---|---|---|
| **1** | La sonde `peles_probe` | ✅ **7/7**, `EXIT=0`, P8 `SKIP` | `GL_VERSION "4.6.0"` sans hint, masque `0x2` = COMPATIBILITY ; pixel central **(25,128,204)** contre (26,128,204) prédits ; deux exécutions **identiques au caractère près** |
| **2** | L'hôte RmlUi, les 8 phases, `GlStateFence` | ✅ **3/3 critères**, après correction de l'instrument | `AUDIT OK` sur **25 états** × **2 phases** ; `--assets` → `EXIT=3` sans fenêtre ; médiane d'image **0,63–0,66 ms** sur 120 images |
| **3** | La vue 3D dans son rectangle | ✅ **4/4 critères** | `--dump-layout 1400x900` → `(240, 0, 880, 720)`, **+1 px exactement** sur l'axe touché ; `--readpixel` → **(25,128,204)**, la valeur de la sonde au caractère près |
| **4** | Arbre + liste, catalogue détypé, oracles | ✅ **5/5 critères** | Les deux vidages **identiques octet pour octet** ; coût **linéaire**, ≈ 7,7 µs/ligne ; plafonds abaissés à **1000/1000/1000** sur mesure |
| **5** | Les panneaux d'informations | ✅ **4/4**, dont **un partiellement** | **14 lignes** même sans fichier ; `%.4g` conservé ; `--reselect` → cinq entiers exacts stables. ⚠ Critère 2 **non comparé à `sinaia`** ; critère 4 laisse un **écart non résolu** de ≈ 100 Kio/sélection |
| **6** | L'interaction | ✅ **6/6 critères** | Rayon central **colinéaire à 0,000000000** sur trois tailles ; arbitrage `vue` pour les trois sous capture ; défilement **82 % → 0 %** inatteignable ; `--drag-bump` sur ses deux versants |
| **7** | Les finitions | ✅ **2/2 critères** | Statut nommant fichier **et** raison, `EXIT=0` ; **60 images + 2 fichiers sous ASan, 0 erreur**. Plus : D-5 refermée, L5 fermée, deux fichiers de 1800 lignes coupés en onze, **et un défaut non déterministe antérieur trouvé et corrigé** |

**Ce que les sept étapes ont coûté en détecteurs** : **19 HIGH** trouvés par les six revues, *presque
tous des instruments incapables d'échouer* — et l'étape 7 en a trouvé un **de plus dans son propre
outillage** (`--dump-copy` sautant le drainage clavier). La règle qui a le mieux servi est celle de
`global-context` : *un détecteur qui ne trouve rien n'est probant que s'il a d'abord trouvé quelque
chose.*

**La revue de clôture rend APPROVE avec commentaires : 0 critical, 0 HIGH, 7 medium, 8 low** — elle
a cherché un vingtième HIGH et ne l'a pas trouvé. ⚠ **Six des sept MEDIUM étaient des affirmations
écrites devenues fausses**, dont aucune n'était visible par un détecteur : *aucun outil ne lit les
commentaires*. Le septième était la mesure manquante de `--dump-status`, faite depuis (§6, « Passe
de clôture »). **Deux de ces phrases sont devenues des mécanismes** — dix `static_assert` et un
plafond injectable avec ses trois tests — plutôt que des phrases corrigées.

### 8.2 L'état de TOUTES les réserves

| # | Réserve | État au terme du lot 1 |
|---|---|---|
| **R-P1** | Tolérance **±2/255** de P6 | ⏸ **OUVERTE.** Toujours **un seul pilote** (Intel, `3.3.0 - Build 32.0.101.6078`). La valeur **(25,128,204)** est reproduite par `peles_probe` et par `peles`, en Release **et sous ASan** — mais c'est la même machine, donc ce n'est pas une calibration. La tolérance reste déclarée **ESTIMÉE** dans le code. ⚗ Hypothèse de l'écart de 1/255 (`min(255, floor(f × 256))` du pilote) **non confirmée** : elle demande deux ou trois couleurs de matériau supplémentaires |
| **R-P2** | **Aucune parité caméra avec `sinaia`** | ⏸ **OUVERTE, INCHANGÉE.** Aucune mesure n'a comparé une pose de `peles` à une pose de `sinaia`. `--drag`, `--alt-pivot` et `--dump-pick` comparent la caméra de `peles` **à elle-même**. Condition de levée inchangée : la console (L0b), dans un lot ultérieur |
| **R-P3** | `RmlUi_Renderer_GL2` salit-il un état hors `glPushAttrib` ? | ✅ **LEVÉE** (étape 2, après correction de l'instrument). Oui, trois matrices ; escalade niveau (1) appliquée. **Aucune copie de `RmlUi_Renderer_GL2.{cpp,h}` n'existe.** Tenu sur toutes les étapes suivantes : **25 états**, 2 phases, `AUDIT OK` |
| **R-P4** | Plafonds, estimations | ✅ **LEVÉE** (étape 4). Les trois valeurs sont **mesurées** : `kMaxChildrenPerNode` 2000 → **1000**, `kMaxFiles` 5000 → **1000**, `kMaxVisibleRows` **ajouté** à 1000. ⚠ **Effet de bord constaté à l'étape 7** : l'égalité des deux premiers rend la ligne de débordement **par nœud** inatteignable |
| **R-P5** | Thread-safety de `cgmesh` : **non examinée** | ⏸ **OUVERTE, INCHANGÉE.** Le lot est entièrement synchrone, donc la réserve n'a pas mordu — et elle n'a pas été instruite non plus |
| **R-P6** | `FetchContent` sur RmlUi | ✅ **LEVÉE** (étape 2). Configure sans patch, empreintes de commit figées |
| **R-P7** | `find_package(Freetype)` sur une cible `FetchContent` | ✅ **LEVÉE** (étape 2), **par un mécanisme autre que le repli écrit d'avance** : RmlUi vérifie l'existence de la **cible**, pas le résultat du `find_package` |
| **R-P8** | `ENABLE_PELES=ON` rompt la **configuration hors ligne** | ⏸ **OUVERTE PAR CONSTRUCTION, et isolée.** Le preset `ci-linux-peles` et son job existent ; la propriété reste rompue sous cette option. ⚠ **`ci-linux-peles` n'a JAMAIS été exécuté** — aucune machine Linux disponible sur les sept étapes |
| **R-4** | Coût d'un `data-for` de 1000 lignes | ✅ **LEVÉE pour 1000 lignes** (étape 4), et **la moitié qui manquait est levée à l'étape 7** : le rétrécissement 1001 → 1 → 1001, **cent fois, sous ASan**, ne rapporte aucune erreur. ⛔ **PAS levée pour 5000 lignes** : construction 38–41 ms, soit plus de deux périodes de 60 Hz — c'est la raison des plafonds à 1000 |
| **R-5** | Une seule racine de binding suffit-elle ? | ✅ **LEVÉE de fait.** Avec 242 lignes de matériaux liées, `Update()` médiane **0,031 ms** et image médiane **0,622 ms**. Le découpage en trois racines, prévu comme repli, n'a jamais été nécessaire |
| **R-6** | glfw délivre-t-il les événements hors fenêtre, bouton enfoncé ? | ⏸ **LEVÉE SUR WIN32 SEULEMENT, ET PAR LECTURE.** `SetCapture`/`ReleaseCapture` au premier et au dernier bouton (`win32_window.c:848-867`). **Supposée vraie sous X11 et Wayland par le PROTOCOLE** — grab implicite du serveur / de `wl_pointer` — **non lue, non mesurée** ; glfw n'y pose aucun `XGrabPointer`. **`--probe-drag` n'a jamais été lancé et P8 reste un `SKIP`** |
| **D-5** | La ligne `Path` ni sélectionnable ni copiable | ✅ **REFERMÉE à l'étape 7, sous une réserve nommée.** Sélection, focus, routage clavier et appel de `SetClipboardText` sont **mesurés** ; le dernier maillon vers le presse-papier du **système** n'est **pas mesurable dans cette session** et l'instrument le dit à chaque exécution. ⚠ Ce qui tient la ligne **non modifiable** n'est pas `readonly`, absent de RmlUi 6.3, mais la **liste de dix touches routées** |

### 8.3 Ce que le lot 1 ne livre PAS

Rassemblé, sans atténuation.

**Fonctionnalités absentes, par décision :**
- **Pas de docking flottant** (D-3) : quatre zones figées par un flex RCSS.
- **Pas de perspectives** ni d'onglets (P-2, D-3) — ce qui est aussi ce qui fait que *la pire
  limitation documentée de RmlUi ne mord pas sur ce lot*.
- **Pas de panneau Treatments.**
- **Pas de HiDPI** : `Context::SetDimensions` reçoit la taille du *framebuffer*, aucune unité `dp`
  n'apparaît dans `peles.rcss`.
- **Pas de défilement horizontal** (`overflow-x: hidden`) : un nom plus large que sa zone reste coupé.
- **Pas d'accessibilité**, **pas d'impression**.
- **Console distante absente** : `RemoteConsole` est hors périmètre, et `cgnet` n'est bâti que sous
  `WIN32`. C'est ce qui a rendu les modes `--dump-*` nécessaires.

**Comportements incomplets, mesurés :**
- ⚠ **L'interface est MONOTONE : ouvrir ajoute, rien ne retire.** Dix double-clics mettent dix
  modèles dans la scène ; il n'y a ni « Fermer » ni onglet. La mémoire GL et le cache
  `MeshDataManager` croissent sans borne sur ce chemin. ⚠ **Et aucun instrument mémoire ne le
  surveille** : `--reselect` et `--reopen` passent tous deux par le chemin qui **vide avant de
  charger**, jamais par celui que l'utilisateur emprunte. L'étape 7 n'a pas fermé cette réserve ;
  elle l'a seulement exercée sous ASan sur deux ouvertures (critère 2), ce qui ne borne rien.
- **La ligne `Path` n'est pas copiable de bout en bout ici.** Voir D-5 : trois maillons sur quatre
  sont mesurés.
- **Le survol 3D n'éclaire aucune ligne de panneau** : `Document::Hovered` est écrit, la boîte jaune
  est dessinée, mais le pendant de `HighlightModelRow` n'est pas câblé — il entrerait en conflit avec
  la classe de sélection, que `data-attr-class` écrase entièrement.
- ⚠ **Les règles `:hover` des panneaux allument les lignes traversées PENDANT une orbite.** Défaut
  produit **assumé** : le refermer détruirait la garde d'inutilité de `--input-script`, seul
  détecteur d'arbitrage du lot.
- **L'arbitrage utilise le rectangle de l'image PRÉCÉDENTE**, conséquence de l'ordre des huit phases.
  Un redimensionnement introduit un retard d'une image sur le picking. **Non mesuré.**
- **Le seuil de 3 px entre clic et glisser est un PORTAGE, pas une mesure.**
- **La ligne de débordement par nœud de l'arbre est inatteignable** tant que les deux plafonds sont
  égaux.

**Ce qui n'a jamais été exécuté :**
- ⚠ **`ci-linux-peles` n'a JAMAIS tourné.** Sa configuration YAML est validée (`yaml.safe_load`),
  son preset aussi, mais la première exécution réelle reste à surveiller : tirage réseau de RmlUi et
  de FreeType, et **égalité octet pour octet des trois vidages produits par GCC** — celle de
  `expected_dump_info.txt` repose sur `%.4g`, dont la sortie est spécifiée par la bibliothèque C.
- ⚠ **`--probe-drag` (P8) n'a jamais été lancé** : il exige un opérateur.
- ⚠ **`sinaia` n'a jamais été ouvert** sur les trois fichiers de référence : le critère 2 de
  l'étape 5 n'est **pas** comparé à l'hôte historique.
- ⚠ **La branche Linux de `ReadProcessFootprintKb` n'a jamais couru** ; elle est corrigée par lecture.
- ⚠ **`tu_supported_formats` ne tourne dans aucune configuration outillée** : il exige
  `ENABLE_SINAIA=ON` **et** `ENABLE_PELES=ON`, donc wxWidgets. Hors de cette configuration il rend un
  `GTEST_SKIP()` qui **nomme** ce qui manque. Et dans la configuration bâtie, la seule garde de
  macro ouverte est `CG_HAS_ZLIB` : une divergence sous `CG_HAS_OCCT`, `CG_HAS_OPENNURBS` ou
  `CG_HAS_LIB3MF` y est invisible **par construction**.
- ⚠ **Les fuites ne sont mesurées nulle part** : l'ASan de MSVC n'a pas de LeakSanitizer, et le job
  ASan Linux du dépôt tourne avec `detect_leaks=0`.
- ⚠ **`peles` n'a JAMAIS tourné sous UBSan, et la mesure du critère 2 s'est donc faite dans la
  configuration la PLUS PAUVRE des deux que le dépôt sait produire.** `CG_ENABLE_SANITIZERS=ON` a
  deux branches : celle de MSVC ne pose que `/fsanitize=address` (`CMakeLists.txt:149-151`) — *«
  UBSan n'existe pas chez MSVC »*, dit le commentaire — et celle qui ajoute
  `-fsanitize=address,undefined -fno-sanitize-recover=all` (`:156-159`) n'est atteinte que sous
  GCC/Clang, donc par `ci-linux-peles`, **jamais exécuté** (voir R-P8 ci-dessus).

  **Ce que le critère 2 couvre donc réellement** : débordements de tas, de pile et de zone globale ;
  `use-after-free`, `use-after-return`, `use-after-scope` ; double `free` ; `free` invalide.
  **Ce qu'il ne couvre PAS** : les fuites (pas de LeakSanitizer sous MSVC, ci-dessus) **et tout
  comportement indéfini non mémoire** — débordement d'entier signé, décalage hors bornes,
  déréférencement désaligné, conversion flottant → entier hors domaine, appel par un pointeur de
  fonction de type incompatible. `peles` fait de l'arithmétique de pixels et des conversions
  `float`/`int` sur tous ses chemins de conversion de repère : c'est exactement le domaine
  d'UBSan, et il n'a jamais été exercé.

### 8.4 Les dettes créées, et où elles vont

⚠ **Elles vont dans `debt_peles.md`, pas ici.** Ce document est l'exécution du lot 1 : il se ferme.
Une dette qui vivra plus longtemps que lui a besoin d'un document qui ne se ferme pas — c'est la
convention des cinq `debt_*.md` de la racine.

| Dette | Gravité | Destination |
|---|---|---|
| L'interface est monotone, et aucun instrument mémoire ne surveille ce chemin | 🔴 | `debt_peles.md` |
| `peles` est la **quatrième** couche application pour un seul `cgmesh`, et duplique chargement, cadrage/trackball et picking | 🟠 | `debt_peles.md` — et c'est la réserve que `camera_refacto.md` §2.1 a déjà mesurée |
| `tu_supported_formats` n'est exécuté par aucune configuration outillée | 🟠 | `debt_peles.md` |
| `probe/main.cpp` : 1001 lignes, au-dessus du seuil de 800 | 🟡 | `debt_peles.md` |
| Les `:hover` allument les lignes traversées pendant une orbite | 🟡 | `debt_peles.md` |
| La ligne de débordement par nœud est inatteignable (plafonds égaux) | 🟡 | `debt_peles.md` |
| Le seuil clic/glisser de 3 px n'est pas calibré | 🟡 | `debt_peles.md` |
| Retard d'une image sur le picking après redimensionnement | 🟡 | `debt_peles.md` |
| Croissance d'empreinte de ≈ 100 Kio par sélection, cause non établie | 🟠 | `debt_peles.md` — l'étape 5 l'a nommée, l'étape 7 n'a pas pu la lever faute de LeakSanitizer |
| R-P8 : `ENABLE_PELES=ON` rompt la configuration hors ligne | 🟠 | reste dans **ce** document (c'est une propriété du lot, pas du code livré) |

---

## 9. Handoff

- Conception par le sous-agent `architect`, implémentation par le sous-agent `developer`, **`/code-review`
  obligatoire** à chaque étape (cf. `/dev-task`).
- **Les commits sont gérés par l'utilisateur.** Ne jamais commiter ni créer de branche.
- ⛔ **`sinaia/` n'est modifié à aucune étape** (D-3 du périmètre). Il est **lu** par le test de
  cohérence de l'étape 4, ce qui n'est pas le modifier.
- Documents liés : `peles.md` (décision), `debt_sinaia.md`, `debt_cgre.md` §5, `camera_refacto.md` §2.1.

---

## 10. Après le lot 1

### 10.1 Arbre de répertoires : chevron et icônes, mesuré le 2026-09-22

**Livré.** Chaque ligne de `#dir-tree` porte un chevron replié/déplié (12 px), puis une icône
(dossier fermé, dossier ouvert, lecteur pour les racines de `SetSystemRoots`), puis le libellé.
Le dossier ouvert est réservé aux lignes `expandable` : un répertoire énuméré vide bascule
`open`/`closed` à chaque clic (`UiController::ActivateDirectoryRow` ouvre puis sélectionne), son
icône reste le dossier fermé. Choix de feuille de style — `.closed, .open` partagent une règle —,
sans nouvelle combinaison ni changement de code. La couleur du texte (`.open` blanc) suit encore
l'état : non traité.
Les lignes en erreur et de débordement n'ont ni l'un ni l'autre, mais gardent les deux
emplacements : les libellés d'un même niveau restent alignés.

**Heuristique « dépliable »** (`TreeRow::expandable`, `directory_tree.cpp`) : chevron sur tout
nœud **non encore énuméré** ; après énumération, chevron seulement s'il reste une ligne à montrer
dessous (enfant ou entrées illisibles). Un répertoire énuméré vide perd son chevron à sa première
ouverture — comportement de l'explorateur Windows. Rien n'est énuméré en plus : savoir d'avance si
un enfant a des sous-répertoires coûterait une ouverture de répertoire par ligne affichée.

**Trois décisions, chacune mesurée ou lue :**

- **Images, pas glyphes.** `LatoLatin-Regular.ttf` n'a ni U+25B6, U+25B8, U+25BC ni U+25BE [MESURÉ
  sur la table `cmap` format 4 : 393 points de code].
- **TGA 32 bits non compressés, pas PNG.** `RenderInterface_GL2::LoadTexture` ne décode que ce
  format [LU, `Backends/RmlUi_Renderer_GL2.cpp:205-247`]. Les cinq images de `peles/assets/icons/`
  sont engendrées par `peles/tools/make_tree_icons.py` (rastérisation suréchantillonnée, aucune
  ressource externe).
- **Décorateurs sur la ligne, pas éléments.** [MESURÉ] `--frames 120 --dump-timing --browse <1200
  sous-répertoires> --browse-depth 0`, 1001 lignes liées, `Update()` de la première image, 10
  exécutions par variante :

  | Variante | `Update()`, 1re image |
  |---|---|
  | Sans icône (référence) | 13,7 – 17,0 ms |
  | Deux `span` d'icône + un `span` de libellé par ligne | 23,2 – 25,6 ms |
  | **Deux décorateurs `image(... scale-none <x>px center)` sur la ligne (livré)** | **13,3 – 14,6 ms** |

  La variante à éléments dépassait le repère de 16,6 ms sur lequel `kMaxVisibleRows` est réglé.
  ⚠ La référence est plus lente que les ≈ 9 ms de l'étape 4 : même machine, autre jour — seule la
  comparaison dans la même session est probante.

**Vérifications [MESURÉ] :**

- `ctest -R "peles|Peles"` : 36/36 (`peles-sinaia`), 35/35 (`peles-boilerplate`) ; cinq tests
  nouveaux dans `tu_peles_core.cpp`. Deux mutants exécutés, puis retirés : `expandable` forcé à
  `!node.error` fait échouer « répertoire vide sans chevron » ; `(node.expanded &&
  !node.children.empty ())` fait échouer « énuméré puis replié garde son chevron », et lui seul.
- Les trois oracles sont **inchangés** : `--dump-tree` n'affiche pas `expandable`.
- **Chaque texture est réellement demandée** : retirer l'une d'elles de la copie d'assets fait
  sortir en **3** (« Could not load texture »), et seulement dans la vue qui l'emploie — vue par
  défaut : chevron fermé + lecteur, et **pas** dossier fermé, ce qui établit que
  `.drive.expandable.closed` l'emporte sur `.expandable.closed` ; `--browse` sur la fixture :
  les deux chevrons et les deux dossiers, pas le lecteur.
- **Position à l'écran** par `--readpixel` sur la fixture (cadre 1280×800, `#dir-tree` en y=22,
  h=144 = 7 lignes de 20 px) : 7 pixels prédits depuis les coordonnées du générateur, 7 conformes
  (dossier ouvert (238,197,108) sur `fixture`, dossier fermé (226,181,90) sur `deeper` **et** sur
  `_under` et `Beta` — vides, état `open` —, chevron (168,168,174), emplacement vide = fond
  (38,38,40)).
- **Sélection survolée** : `.selected:hover` (id + 2 classes, 1 200 000) l'emporte sur `div:hover`
  (id + balise + pseudo-classe, 1 110 000), dans `#dir-tree` comme dans `#file-list` [LU, poids de
  `Source/Core/StyleSheetSelector.h:13-20`]. Non mesuré au pixel : `--click` sort avant tout rendu,
  donc ne se combine pas avec `--readpixel`.
- `--churn-tree`, `--reopen`, `--open-second` : 40 exécutions, puis 20 après les corrections de
  revue, 0 sortie non nulle, 0 avertissement RmlUi. `--frames 5`, `--dump-tree peles`, `--browse peles --browse-depth 2 --frames 30` : sortie
  0, aucun avertissement, dans les deux arbres de construction.

**Non vérifié / limites :**

- l'aspect d'ensemble à l'écran : aucune capture d'image complète n'est disponible, seuls des
  pixels isolés sont lus ;
- la sélection survolée (cf. ci-dessus) ;
- une ligne en erreur réelle : aucun accès refusé dans la fixture, seule la règle de classes la
  couvre ;
- la branche `node.unreadable > 0` du chevron : aucune fixture ne produit une entrée dont le statut
  ne se lit pas, et aucune n'a été fabriquée pour elle ;
- `LesRacinesSystemeSontDesLecteurs` dépend des lecteurs de la machine qui l'exécute
  (`SetSystemRoots` interroge chaque lettre) : il suppose au moins un lecteur prêt, et peut être
  lent si un lecteur réseau déconnecté reste monté.

Aucune dette créée.

### 10.2 Redimensionnement de la fenêtre : rendu pendant le geste, mesuré le 2026-09-23

**Symptôme rapporté** : « le resize de la fenêtre n'est pas géré de manière dynamique ».

**Cause racine.** Sous Win32, tirer un bord de la fenêtre fait entrer le système dans une boucle
modale de redimensionnement. `glfwPollEvents` (`app/application.cpp:136`, phase 1) ne rend la main
qu'au relâchement. Pendant le geste, seuls `WM_SIZE` et `WM_PAINT` sont distribués, et `WM_PAINT`
n'aboutit qu'au rappel de rafraîchissement [LU, glfw 3.3.10 `win32_window.c:1135-1138`,
`window.c:125-128`]. Or `peles` n'en posait aucun : ni `glfwSetWindowRefreshCallback` ni
`glfwSetFramebufferSizeCallback` dans `platform/window_glfw.cpp`. Résultat : aucune image n'est
rendue avant le relâchement.

**Hypothèse écartée par la mesure — la propagation des dimensions.** Sans le correctif (cas négatif
ci-dessous), l'image que la boucle rend après un `glfwSetWindowSize` a déjà un tampon, un contexte
RmlUi et un rectangle `#viewport` à la nouvelle taille. `SetDimensions`, `glViewport`,
`Camera::SetViewSize` et la relecture du rectangle étaient donc corrects, puisqu'ils sont refaits
à chaque image.

**Correctif.**

- `Window::SetRefreshCallback` (`platform/window_glfw.cpp`). La table de rappels pointée par la
  fenêtre porte désormais ce rappel à côté des rappels d'entrée, et elle est posée dès `Create`.
- `RenderFrame` est découpée en trois : phase 1 et garde de tampon nul, `SyncFramebufferSize` (le
  bloc des dimensions, inchangé), `DrawFrame` (phases 2 à 8, dans l'ordre et d'un seul tenant).
- `RenderRefreshFrame` = dimensions puis `DrawFrame`, **sans** `PollEvents` (glfw l'interdit depuis
  un rappel) et sans arbitrage ni commandes (les événements en file restent à l'image qui les a
  reçus). Une garde empêche la réentrance. Elle ne dessine **que si la taille du tampon diffère de
  celle de la dernière image effectivement dessinée** (`m_drawnWidth/Height`, posés après
  l'échange). Sans ce filtre, un `WM_PAINT` de restauration ou de fenêtre découverte, ou un
  `Expose` X11 (non filtré sur son compte, glfw `x11_window.c:1854-1857`), coûterait une image et
  un échange de plus que celle de la boucle.
- Le rappel n'est posé **que dans `RunLoop`, après sa première image**. Pas dans les modes « vider
  et sortir », où une image intercalée courrait entre deux événements injectés. Pas avant l'image 1,
  où l'image du rappel paierait la première mise en page que « `Update()`, 1re image » mesure.
- Le rappel est retiré par une garde RAII locale à `RunLoop` : une exception qui en sort ne laisse
  pas une capture de `this` posée pendant la destruction des membres.
- `--dump-timing` écarte toute image qui a contenu une image du rappel. Libellés : « images
  mesurees » (au lieu de « images rendues ») et « ecartees (image rappel) : N ». Aucun oracle ni
  test ne lit ces lignes (`grep` sur `peles/` et `test/`).

**Instrument : `--resize <l>x<h>`.** Après la première image, il appelle `glfwSetWindowSize` puis
relève l'image suivante. Les critères portent sur la taille **obtenue** :
- contexte RmlUi = tampon ;
- rectangle changé et contenu dans le tampon ;
- au moins une image du rappel entre la demande et l'image suivante, **quelle que soit sa taille** ;
- cette image a la taille et le rectangle de l'image suivante.

Un défaut sur l'un de ces critères donne `ECHOUE`. Si tout est conforme mais que le tampon diffère
de la demande (écran trop petit, facteur d'échelle ≠ 1), le verdict est `NON CONCLUANT` et nomme
la taille obtenue. Une demande égale à la taille courante ne redimensionne rien : `NON CONCLUANT`
explicite. ⚠ **Ce n'est pas un
redimensionnement au bord** : `SetWindowPos` n'entre pas dans la boucle modale. Il exerce les mêmes
`WM_SIZE` puis `WM_PAINT` que chaque pas de cette boucle.

**Mesures [MESURÉ] :**

| Exécution | Résultat |
|---|---|
| `--resize 900x600` | rectangle `x=240 y=0 l=380 h=420`, identique au rappel et après ; 1 image du rappel ; `EXIT=0` |
| `--resize 1500x950` | `x=240 y=0 l=980 h=770` ; 1 image du rappel ; `EXIT=0` |
| Oracle indépendant `--dump-layout 900x600` / `1500x950` | `x=240 y=0 w=380 h=420` / `w=980 h=770` — **identiques** |
| **Cas négatif 1** : rappel non posé | tampon, contexte et rectangle suivent, **0** image du rappel → `REDIMENSIONNEMENT ECHOUE`, `EXIT=1` |
| **Cas négatif 2** : le rappel dessine sans remettre les dimensions à RmlUi | rectangle du rappel `l=760 h=620` ≠ `l=380 h=420` → `ECHOUE`, `EXIT=1` |

**Après la revue de code** (I1, m1, m2, m4, m5), mesuré sur `peles-boilerplate`. ⚠ Le binaire
`peles-sinaia` n'a pas pu être relié (LNK1104 : une instance interactive de l'opérateur le tenait
ouvert). Ses objets compilent, et son ctest (36/36) a tourné avec l'ancien `peles.exe` pour les
oracles.

| Exécution | Résultat |
|---|---|
| `--resize 900x600` / `1500x950` | `OK`, 1 image du rappel à la taille obtenue, `EXIT=0` |
| `--resize 20000x20000` | tampon **1924x1055**, rappel à 1924x1055, rectangle `l=1404 h=875` identique → `NON CONCLUANT : tampon obtenu 1924x1055 au lieu de 20000x20000`, `EXIT=1` (auparavant : « aucune image rendue par le rappel ») |
| `--resize 1280x800` (taille courante) | `NON CONCLUANT : la demande 1280x800 est la taille courante du tampon`, `EXIT=1` |
| Cas négatif 1 rejoué (rappel non posé) | `0` image du rappel → `ECHOUE`, `EXIT=1` ; code remis → `OK`, `EXIT=0` |
| `--frames 60 --dump-timing` | `images mesurees : 60`, `ecartees (image rappel): 0` |
| ctest `[Pp]eles` | 35/35 (`peles-boilerplate`), 36/36 (`peles-sinaia`, voir ⚠ ci-dessus) |
| Batterie : `--frames 5`, `--frames 60 --dump-timing`, `--churn-tree 20`, `--browse … --frames 30`, `--reopen` ×4, `--open-second` ×4, les deux `--resize`, `--resize 1000x700 --state-audit --frames 4`, `--readpixel 620 490 cube.obj`, `--dump-pick`, `--input-script`, `--dump-copy` | 19/19 `EXIT=0`, 0 avertissement |

⚠ **Le filtre de taille (I1) n'est mesuré que dans sa branche « taille changée »** : aucun instrument
ne produit de `WM_PAINT` à taille constante.

Non-régression, sur `peles-sinaia` **et** `peles-boilerplate` :

- `ctest -R "[Pp]eles"` : 36/36 et 35/35.
- 22 exécutions par build, toutes `EXIT=0` et sans aucun avertissement : `--frames 5` ;
  `--browse peles --browse-depth 2 --frames 30` ; `--churn-tree 20` ; `--reopen` ×5 ;
  `--open-second` ×5 ; les deux `--resize` ; `--resize --state-audit` ;
  `--frames 1 --readpixel 620 490 cube.obj` → **(25,128,204)**, écart 1/255, inchangé ;
  `--dump-pick` ; `--input-script` ; `--dump-copy` ; `--frames 60 --dump-timing`.

**Non vérifié :**

- **le redimensionnement à la souris lui-même** : la boucle modale ne s'automatise pas. Qu'elle
  distribue un `WM_PAINT` par pas est [LU] (classe de fenêtre `CS_HREDRAW | CS_VREDRAW`,
  `win32_window.c:1428`), pas mesuré ;
- le déplacement de la fenêtre (même boucle modale, sans `WM_PAINT`) : la boucle d'images y reste
  suspendue, sans effet visible ;
- un facteur d'échelle ≠ 1 : le critère « tampon = demande » le suppose.

### 10.3 Panneaux redimensionnables : trois séparateurs, mesuré le 2026-09-23

**Demande** : « Est-il possible de resizer les panneaux arborescence et liste de fichiers ? »

**Livré.** Trois séparateurs de 6 px : entre l'arbre et la vue (largeur de `#panel-left`), entre la
vue et le panneau droit (largeur de `#panel-right`), entre la ligne haute et la liste de fichiers
(hauteur de `#panel-bottom`). Le panneau droit est venu sans coût propre : une ligne de table
(`ui::PanelSplitters::Specs`) et un élément RML. Curseur de redimensionnement au survol et pendant
le geste, surbrillance `:hover` / `:active`, double-clic = taille par défaut. **Pas de persistance
entre sessions** (non demandée) : c'est la suite naturelle, et elle tiendrait dans
`PanelSplitters` — l'étendue demandée par panneau est déjà l'état à sauver.

**Cinq décisions, chacune justifiée :**

- **Le glisser est celui de RmlUi, pas un geste de l'hôte.** `drag: drag` en RCSS ; RmlUi émet
  `dragstart` / `drag` / `dragend` sur le séparateur, `ui::PanelSplitters` les écoute
  (`Rml::EventListener`). L'enfoncement sur un séparateur est arbitré `Owner::Ui` par
  `InputRouter` **sans aucune règle nouvelle** : la capture du §1.7 fige ce propriétaire jusqu'au
  relâchement, donc un glisser commencé sur un séparateur ne peut pas orbiter, même au-dessus de la
  vue ; un glisser commencé dans la vue n'est jamais remis à RmlUi, qui ne voit aucun `dragstart`.
  Un geste de l'hôte aurait demandé un troisième propriétaire dans l'arbitre — le seul module du lot
  dont chaque ligne a été éprouvée par un cas négatif. [LU] `Context::ProcessMouseButtonDown`
  choisit l'élément tiré en remontant la chaîne de survol jusqu'au premier `drag` non nul ;
  `UpdateHoverChain` émet `drag` à chaque déplacement et garde le **curseur de l'élément tiré**
  pendant tout le geste (`Source/Core/Context.cpp`, RmlUi 6.3).
- **La taille par défaut reste dans `peles.rcss`, seule source.** Un glisser pose `flex-basis` **en
  ligne** sur le panneau (`Element::SetProperty(PropertyId::FlexBasis, …px)`), qui l'emporte sur la
  règle `flex: 0 0 240px` ; le double-clic appelle `RemoveProperty` et la règle reprend. Un
  `data-style-flex-basis` aurait obligé à recopier 240/280/180 en C++ et à ne jamais lier une valeur
  vide (refusée en `LT_WARNING`, §1.6). Le C++ ne connaît toujours **aucune largeur de panneau** :
  il lit l'étendue résolue au `dragstart`, ajoute le déplacement du pointeur, borne. L'écouteur pose
  la propriété dans le rappel : `SetProperty` ne fait que salir le style, aucun appel GL n'est émis
  — la raison qui fait différer les `data-event-*` (§1.9, `ui/ui_command.h`) ne s'applique pas.
- **Les séparateurs n'occupent aucune place, et ne recouvrent aucune barre de défilement.** Base
  flex 6 px et UNE marge de −6 px : boîte de marge nulle, zone de saisie entièrement sur un seul
  voisin. Les séparateurs gauche et droit recouvrent la **vue** (240–246 et 994–1000 au cadre
  1280×800) ; le séparateur bas recouvre le **titre** de `#panel-bottom` (620–626), qui est inerte
  — déborder vers le haut aurait recouvert le bas des barres des panneaux latéraux. La barre de
  `#panel-bottom` est décalée d'autant (`margin-top: 6px; scrollbar-margin: 6px` sur
  `scrollbarvertical`, pris en compte par `ElementScroll::FormatScrollbars` [LU]).
  `position: relative; z-index: 1`, sans quoi `#viewport` — qui suit `#split-left` dans le document
  — serait testé par-dessus lui (mesuré, cas N2 ci-dessous). Conséquence assumée : une bande de
  6 px de la vue contre chaque séparateur vertical n'orbite plus.
- **Les bornes, dans `core/splitter_bounds.{h,cpp}`, sans GL ni RmlUi.** Panneau ≥ 120 px
  (`kMinPanelExtent`), vue ≥ 200 px sur l'axe du séparateur (`kMinViewportExtent`), sur l'étendue que
  panneau et vue se **partagent**, lue dans la mise en page. **Quand les deux minimums ne tiennent
  pas ensemble, la vue passe avant** : le panneau descend sous 120, jusqu'à 0, plutôt que de laisser
  la vue sans surface.
- **L'étendue DEMANDÉE est conservée, et bornée à chaque image — pas l'étendue affichée.**
  `PanelSplitters::Enforce`, appelée par `DrawFrame` juste après `Update()`, rétrécit un panneau
  redimensionné si la fenêtre ne laisse plus son minimum à la vue, puis refait `Update()` **dans la
  même image** (la phase 3 ne lit jamais un rectangle que la correction va changer). Rendue à sa
  taille, la fenêtre rend au panneau l'étendue choisie. Un panneau et une vue en débordement (somme
  des bases > conteneur, vue à 0) ne partagent pas « panneau + vue » : `SharedExtent` retranche le
  débordement mesuré sur les boîtes de marge des enfants du conteneur (`core::SharedAxisExtent`).
  **Tous les panneaux d'un axe sont bornés ensemble, en une passe** (`core::ClampAxisExtents`) : le
  manque est pris sur les panneaux au-dessus de leur minimum, dans l'ordre fixe gauche puis droit,
  et aucun ne descend sous 120 px tant que la somme des minimums et de la vue minimale tient. Un
  panneau à sa taille par défaut ne cède rien. Sans panneau redimensionné,
  `Enforce` rend `false` d'emblée : `Update(), médiane` de `--frames 60 --dump-timing` reste
  **0,017 ms**.

**Curseur, rendu visible par l'hôte.** `Rml::SystemInterface::SetMouseCursor` n'était pas surchargée
[LU, `platform/rml_system_interface.h` avant ce paragraphe] : toute propriété `cursor` était sans
effet. `SystemInterface` la transmet à un puits (`RmlHost::Config::cursor`), qu'`Application` relie à
`Window::SetCursor` : nom RCSS → curseur standard glfw 3.3 (`ew-resize`/`col-resize` →
`GLFW_HRESIZE_CURSOR`, `ns-resize`/`row-resize` → `GLFW_VRESIZE_CURSOR`, `text`, `pointer`,
`crosshair` ; tout autre nom → flèche). Curseurs créés à la première demande, détruits avant la
fenêtre.

**Faute de configuration.** Dans le document du produit, un séparateur absent sort en **3**, comme
`#viewport` absent : mesuré en renommant `split-right` dans la copie d'assets → « 2 separateur(s)
de panneau sur 3 … », `EXIT=3`. Une mise en page d'épreuve (`--document`) n'en exige aucun ;
`--document layout_probe.rml --drag-splitter left 150` rend `NON CONCLUANT`, `EXIT=1`.

**Instrument : `--drag-splitter <left|right|bottom> <delta>`**, mode « vider et sortir », par la file
d'événements, l'arbitre et des images réellement rendues (`app/application_splitters.cpp`, classe
imbriquée `Application::SplitterProbe`) :

0. **témoin** : glisser parti de 30 px dans la vue, fini 60 px dans le panneau. Il **doit** faire
   tourner la caméra (≥ 1°, sinon `NON CONCLUANT`) et ne rien redimensionner ;
1. **glisser** du séparateur, depuis son milieu lu dans la géométrie : survol = le séparateur,
   curseur glfw posé, propriétaire `interface`, étendue = attendue, vue qui suit (origine et
   taille), rectangle de la phase 3 = géométrie du document, axe **et** pivot de la caméra
   inchangés **au bit près** depuis le témoin ;
2. **fenêtre réduite** de (vue − 200 + 40) px puis **rendue** : panneau borné, vue ≥ 200, puis
   étendue demandée restituée. `NON CONCLUANT` si le tampon n'a pas la taille demandée ;
3. **double-clic suivi d'une secousse d'un pixel bouton tenu**, dans le même lot d'évènements :
   panneau et vue reviennent au rectangle initial, demande effacée.

`pair <delta>` : les panneaux gauche **et** droit sont tirés de `delta` (le droit sondé après le
glisser du gauche), puis la fenêtre est réduite en trois paliers — manque absorbé par le gauche
seul, gauche à son minimum et droit entamé, manque supérieur à la somme des deux marges — puis
rendue. Le glisser exige au moins un évènement `drag` traité ; le déplacement est borné à
±100 000 px par l'analyse de la ligne de commande.

L'étendue attendue est **réécrite depuis les deux minimums** dans l'instrument, sans appeler
`ClampPanelExtent` : reprendre la fonction du produit rendrait l'instrument d'accord avec elle
quelle qu'elle soit.

**Mesures [MESURÉ]**, `peles-boilerplate`, cadre 1280×800 (`left 150`, `right -150`,
`bottom -150`, `left 2000` et `bottom 2000` rejouées sur `peles-sinaia` dans la batterie, toutes
`CONFORME`) :

| Exécution | Panneau | Vue après glisser | Fenêtre réduite → panneau / vue | Verdict |
|---|---|---|---|---|
| `left 150` | 240 → 390 (libre) | x=390 w=610 | 830×800 → 350 / 200 | `CONFORME`, `EXIT=0` |
| `right 150` | 280 → 130 (libre) | x=240 w=910 | 530×800 → 90 / 200 | `CONFORME` |
| `right -150` | 280 → 430 (libre) | x=240 w=610 | 830×800 → 390 / 200 | `CONFORME` |
| `bottom -150` | 180 → 330 (libre) | h=470 | 1280×490 → 290 / 200 | `CONFORME` |
| `left 2000` | 240 → **800 (borne)** | x=800 w=200 | 1240×800 → 760 / 200 | `CONFORME` |
| `left -2000` | 240 → **120 (borne)** | x=120 w=880 | 560×800 → **80** / 200 (la vue passe avant) | `CONFORME` |
| `bottom 2000` | 180 → **120 (borne)** | h=680 | 1280×280 → 80 / 200 | `CONFORME` |
| `bottom -2000` | 180 → **600 (borne)** | h=200 | 1280×760 → 560 / 200 | `CONFORME` |

Témoin : rotation **7,6317°** / **7,6316°** (séparateurs gauche / droit), **11,9090°** (séparateur
bas), panneau et vue inchangés. Double-clic : retour à 240 / 280 / 180 et à `x=240 y=0 w=760 h=620` dans les huit cas.

**Cas négatifs, chacun injecté dans le PRODUIT puis retiré** (le verdict revient à `CONFORME`) :

| Faute injectée | Critères rouges | `EXIT` |
|---|---|---|
| N1 `ApplyExtent` sans effet | étendue ; bornes à la réduction ; vue sous son minimum | 1 |
| N2 RCSS sans `position`/`z-index` | survol (`#viewport`) ; curseur ; propriétaire ; étendue ; **caméra bougée** au glisser **et** au double-clic ; bornes | 1 |
| N3 `Enforce` rend toujours `false` | bornes à la réduction (330 au lieu de 290) ; vue 160 < 200 | 1 |
| N4 RCSS sans `drag: drag` | étendue (0 évènement `drag`) ; bornes | 1 |
| N5 `OnReset` sans effet | double-clic, et lui seul | 1 |
| N6 RCSS sans `cursor: ew-resize` | curseur, et lui seul | 1 |
| N7 `Enforce` écrase la demande | fenêtre rendue : panneau 350 au lieu de 390, et lui seul | 1 |
| Mutant `ClampPanelExtent` (minimum du panneau prioritaire) | tests `QuandLesDeuxMinimumsNeTiennentPasLaVuePasseAvant`, `LEtendueNEstJamaisNegative` | — |

Le premier essai de l'instrument est lui-même tombé sur sa garde : le témoin partait du côté du
panneau, et l'instrument a rendu `NON CONCLUANT` (rotation 0°) au lieu d'un vert vide.

**Non-régression [MESURÉ]** :

- `--dump-layout` sur `viewport`, `panel-left`, `panel-right`, `panel-bottom`, `row`, aux cadres
  1280×800, 900×600, 1500×950 : **15 sorties identiques octet pour octet** à celles du binaire
  d'avant ce paragraphe (`cmp`), sur les deux builds. Séparateurs : `x=240 w=6`, `x=994 w=6`,
  `y=620 h=6`.
- `--input-script` et `--drag 620 300 100 300 panels.obj` : sorties **identiques** à l'avant.
  `--readpixel 620 490 cube.obj` → **(25,128,204)**, `PIXEL CONFORME`.
- `ctest -R "[Pp]eles"` : **48/48** (`peles-boilerplate`), **49/49** (`peles-sinaia`) — treize
  tests nouveaux `PelesCoreSplitter.*` et `PelesCoreSplitterAxis.*` dans `test/tu_peles_core.cpp`.
- Batterie de 32 exécutions par build, **32/32** `EXIT=0` et **0** message RmlUi de niveau
  avertissement ou plus : `--frames 5` (avec et sans fichier), `--browse peles --browse-depth 2
  --frames 30`, `--churn-tree 20 --frames 25`, `--reopen` ×4, `--open-second` ×4, `--resize
  900x600`, `--resize 1500x950`, `--resize 1000x700 --state-audit --frames 4`, `--input-script`,
  `--drag`, `--drag … --drag-bump left`, `--dump-layout 1280x800`, `--dump-pick 1400x900 680 360`,
  `--readpixel`, `--dump-copy`, `--document layout_probe.rml --frames 3`, `--frames 60
  --dump-timing`, six `--drag-splitter` et deux `--drag-splitter pair` (200, 400).
- `peles-sinaia` a été relié normalement (aucune instance ouverte).

**Non vérifié :**

- **le geste à la souris** et **l'aspect du curseur à l'écran** : l'instrument injecte des
  évènements dans la file de l'hôte, comme `--drag` ; il établit que glfw a reçu la forme demandée
  (`Window::CurrentCursor`), pas que le système l'affiche ;
- la surbrillance `:hover` / `:active` du séparateur (aucune capture d'image complète) ;
- un facteur d'échelle ≠ 1 : le temps 2 le suppose, et rend `NON CONCLUANT` sinon ;
- le redimensionnement au bord **pendant** qu'un panneau est borné : `Enforce` court dans
  `DrawFrame`, donc aussi dans l'image du rappel de rafraîchissement (§10.2), mais seul
  `glfwSetWindowSize` a été exercé.

**Comportements assumés, consignés** (`debt_peles.md`, §16) : une perte de focus pendant le glisser
garde la dernière étendue (RmlUi reçoit un relâchement, pas une annulation) ; un glisser commencé
dans le même lot d'évènements qu'un double-clic est ignoré ; quand gauche et droit sont tous deux
bornés, le gauche cède le premier, mais seulement jusqu'à son minimum tant que le droit a de la
marge ; les panneaux à leur
taille par défaut ne sont **pas** bornés — sous 720 px de large (240 + 280 + 200), la vue passe sous
son minimum, et sous 520 px elle est de largeur nulle, comme avant ce paragraphe.

#### Corrections de la revue de code, mesurées le 2026-09-23

Verdict de revue : 0 bloquant, WARNING mergeable. Huit points, tous corrigés dans le code — les
paragraphes ci-dessus décrivent l'état **après** correction.

| # | Constat | Correction | Mesure |
|---|---|---|---|
| 1 | `Enforce` bornait chaque panneau contre « lui + vue » : gauche 400 / droit 400, fenêtre 700 → gauche **100** (< 120), droit 400 | `core::ClampAxisExtents` : tous les panneaux d'un axe en une passe, manque pris au-dessus des minimums (gauche puis droit) avant tout passage sous 120 ; `core::SharedAxisExtent` porte la correction de débordement ; `PanelSplitters::EnforceAxis` | 7 tests `PelesCoreSplitterAxis` (deux panneaux, un, aucun, manque > marges, panneau non réglable, restitution, débordement vue à 0). `pair 200` : 1080 → 400/480, 760 → **120/440**, 400 → 80/120, vue 200 aux trois paliers |
| 2 | Double-clic puis secousse de 1 px dans le même lot : `OnDragStart` lisait l'étendue d'avant la remise | `Slot::resetPending`, posé par le double-clic, levé par `Enforce` (appelée après chaque `Update()`) ; un `dragstart` pendant ce temps est ignoré | temps 3 de l'instrument : secousse de 1 px → panneau 240 / 280 / 180, `CONFORME` |
| 3 | La zone de saisie gauche recouvrait 3 px de la barre de `#panel-left`, la zone basse le haut de celle de `#panel-bottom` | zones d'un seul côté (vue, ou titre du panneau bas) ; barre de `#panel-bottom` décalée de 6 px | `--dump-layout` : 15/15 identiques ; `--click 237 300` survole `#split-left` avec les anciennes marges, une ligne de l'arbre (`div`) avec les nouvelles ; `--click 1275 628` (arbre et liste sur `test/data`) survole `sliderbar`, `--click 1275 622` le séparateur |
| 4 | Débordement signé pour un delta proche de `INT_MAX` | `ParseBoundedInt` reçoit un maximum ; `--drag-splitter` borné à ±100 000 | `100000` → `EXIT=0` ; `100001`, `-100001`, `2147483000` → `EXIT=2`, « entier de -100000 a 100000 » |
| 5 | `cursors[6]`, `kEvents[]` en tableaux C | `std::array<GLFWcursor*, kCursorShapeCount>` + `static_assert` sur `CursorShape::Crosshair + 1` ; `std::array<Rml::EventId, 4>` ; `axisChanged[2]` a disparu avec le point 1 | compilation |
| 6 | `drags` affiché, non exigé | échec si `drags <= 0` | cas N4 : « aucun evenement `drag` n'a ete traite » |
| 7 | `SplitterProbe::Drag` : 69 lignes de code | `DragGesture` (geste et mesure, 38) + `ReportDrag` (rapport et verdict) | plus aucune fonction de l'unité au-dessus de 50 lignes de code (`ShrinkPair` : 50) |
| 8 | `main.cpp` proche du seuil de 800 lignes | aucun code ; consigné dans `debt_peles.md` §10 | 786 lignes (`wc -l`) |

**Cas négatifs des corrections, injectés dans le produit puis retirés :**

| Faute | Rouge | `EXIT` |
|---|---|---|
| N8 phase 1 de `ClampAxisExtents` supprimée | `pair 200` : 760 → gauche **80** / droit 480 au lieu de 120 / 440, 400 → droit 200 au lieu de 120 ; tests `DeuxPanneauxCedentAuDessusDeLeurMinimumAvantDeDescendre`, `UnManqueSuperieurAuxMargesPasseSousLesMinimums` | 1 |
| N9 garde `resetPending` supprimée | double-clic + secousse → panneau **391** (gauche) / **329** (bas) au lieu du défaut | 1 |
| N4 rejoué (sans `drag: drag`) | « aucun evenement `drag` n'a ete traite », plus l'étendue et les bornes | 1 |
| N3 rejoué (`Enforce` rend toujours `false`) | `bottom -150` : 330 au lieu de 290, vue 160 ; `pair 200` : les trois paliers, vue 160 | 1 |
| N7 rejoué (`EnforceAxis` écrase la demande) | fenêtre rendue : `left 150` 350 au lieu de 390 ; `pair 200` 120 / 120 au lieu de 440 / 480 | 1 |

L'instrument, restructuré, a été relancé en entier : les huit exécutions du tableau et les deux
`pair` sont `CONFORME`.
