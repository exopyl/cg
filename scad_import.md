# Import OpenSCAD (`.scad`) dans sinaia — voie A : l'exécutable OpenSCAD

**Ouvert le** 2026-10-19. **Décision d'origine** : utilisateur, 2026-10-19 — l'évaluation du langage
est déléguée à l'exécutable OpenSCAD (« voie A ») ; aucun interpréteur OpenSCAD n'est écrit dans le
dépôt.

Statuts : **[V]** vérifié par lecture, **[M]** mesuré (instrument nommé), **[E]** estimé ou
calculé, **[H]** hypothèse (avec ce qui la trancherait).

---

## 1. Ce qui est livré

    openscad -o <temp>/model.off [-D nom=valeur]... <fichier.scad>      (cwd = répertoire du .scad)
        -> MeshIO::import_off -> Mesh::Triangulate -> VMeshes -> sinaia

| Élément | Fichier |
|---|---|
| Lanceur de processus sans shell (tableau d'arguments, cwd, délai, annulation, sortie capturée, répertoire temporaire RAII) | `src/cgmesh/external_process.{h,cpp}` |
| Recherche de l'exécutable, appel d'OpenSCAD, lecture, classement des échecs | `src/cgmesh/import_scad.{h,cpp}` |
| Lecteur OFF : polygones N ≥ 3, lectures vérifiées, en-têtes menteurs refusés ; écrivain OFF conforme | `src/cgmesh/mesh_io.cpp` (`import_off`, `export_off`) |
| Point de passage unique des ouvertures de sinaia, réglage persistant, dialogue de progression annulable | `sinaia/ModelFileLoader.{h,cpp}` |
| Catalogue des formats et filtre OpenSCAD, sans wx (testé dans TU) | `sinaia/FormatCatalog.h` |
| Chaînes wx du catalogue (Open, Add, panneau Files) | `sinaia/SupportedFormats.h` |
| Réglage « répertoire d'OpenSCAD » et son état | `sinaia/SettingsDialog.h` (File > Settings > Import) |
| Validité du répertoire, conversion de l'ancien réglage | `src/cgmesh/import_scad.{h,cpp}` (`CheckOpenScadDirectory`, `MigrateOpenScadSetting`) |
| Chemin connu de la configuration (`EXTERN_OPENSCAD`), pour les tests seulement | `CMakeLists.txt`, `test/CMakeLists.txt` |
| Tests (25 + 5) et fixtures | `test/tu_cgmesh_import_scad.cpp`, `test/tu_sinaia_scad_setting.cpp`, `test/data/scad/` |

Les chemins d'ouverture couverts **[V]** : File > Open, File > Add, glisser-déposer (les deux
passent par `OpenDocument` / `AppendModels`), double-clic du panneau Files, rafraîchissement d'un
modèle (`ReloadModel` : éditer le .scad dans un éditeur externe puis rafraîchir relance OpenSCAD),
commandes `open` et `append` de la console distante. Tous aboutissent à
`sinaia::LoadModelFile`, seul appelant de `VMeshesIO::load` pour un fichier utilisateur
(`CuttingMat.cpp` lit son propre actif OBJ, hors périmètre).

---

## 2. Décisions

### D1. Le module vit dans cgmesh, hors build WebAssembly

- **Pourquoi pas sinaia** : `wxExecute` attacherait la conversion à wxWidgets, donc ni testable dans
  `TU` (qui ne lie pas wx) ni réutilisable par sulina, vecna ou un outil en ligne de commande.
- **Pourquoi c'est acceptable dans cgmesh** : la dépendance au lancement de processus est isolée dans
  une unité sans géométrie (`external_process.cpp`), elle-même sans dépendance hors bibliothèque
  standard et API système. cgmesh compte déjà des unités qui incluent `windows.h` (`vmeshes_io.cpp`).
- **Hors WASM sans garde dans le code** **[V]** : la liste EMSCRIPTEN de `src/cgmesh/CMakeLists.txt`
  est explicite, le natif est un `GLOB`. Les deux nouvelles unités n'y figurent pas, et
  `mesh_io.cpp` (seul fichier existant touché côté cgmesh) en est déjà exclu.
- **Le réglage d'interface reste dans sinaia** : cgmesh reçoit un `ExecutableSearch` explicite,
  sans état global ni lecture de configuration.

### D2. Recherche de l'exécutable, sans chemin caché

⚠ **Pour sinaia, remplacé par D6 (2026-10-20)** : sinaia ne consulte plus que le répertoire réglé.
La recherche ci-dessous reste celle de `cgmesh::scad::FindOpenScad`, employée par les tests et par
tout autre appelant qui ne fournit pas de chemin explicite.

Ordre (`cgmesh::scad::FindOpenScad`) :

1. chemin explicite `ExecutableSearch::configured` (sinaia y passe `<répertoire réglé>/openscad.exe`,
   cf. D6), **exclusif** : un chemin explicite qui ne mène à rien est signalé, pas contourné ;
2. variable d'environnement `CG_OPENSCAD`, même règle ;
3. `PATH` (Windows : `openscad.exe` puis `openscad.com` ; ailleurs `openscad`) ;
4. Windows : `%ProgramFiles%\OpenSCAD` — emplacement de l'installeur officiel **[H]** (non vérifié,
   aucune installation par l'installeur sur cette machine) ;
5. `EXTERN_OPENSCAD` de la configuration CMake, dernier recours (ignoré s'il n'existe pas à
   l'exécution).

Chaque entrée peut désigner l'exécutable ou son répertoire. En cas d'échec, le message liste les
emplacements essayés et dit où régler le chemin.

Sous Windows, **seuls les programmes `.exe` et `.com` sont admis** : un `.bat` ou un `.cmd` serait
exécuté par `cmd.exe`, dont l'échappement n'est pas celui de `CommandLineToArgvW` (le tableau
d'arguments ne serait plus sûr). Un réglage qui désigne un script est refusé avec la raison
(« rejected: only .exe and .com programs are accepted ») — test
`windows_scripts_are_not_accepted_as_the_executable`, avec un témoin `.exe` accepté.

`EXTERN_OPENSCAD` suit la convention des autres `EXTERN_*` (variable de cache, repli sur la
variable d'environnement du même nom). Les archives `cg_binaries` ne contiennent **pas** OpenSCAD :
**[M]** `unzip -l` des archives 1.0.0 Windows et Linux (OCCT et openNURBS seulement) ; l'archive
1.1.0 consommée par la CI n'est pas présente localement, mais son commentaire dans
`build.yml:13` annonce le seul ajout de lib3mf **[V]**. La CI n'a donc pas OpenSCAD : les 11 tests
qui l'exigent y sont sautés (§4).

### D3. Lancement sûr

- **Aucun shell** : Windows `CreateProcessW` avec une ligne de commande construite selon les règles de
  `CommandLineToArgvW` (`QuoteWindowsArgument`, testée par aller-retour sur 12 arguments piégés) ;
  POSIX `fork` + `execv`, entre les deux des fonctions async-signal-safe plus `syscall (SYS_close_range)` (hors liste POSIX,
  stub brut de la glibc), échec d'`execv`
  remonté par un tube `FD_CLOEXEC`.
- **Descripteurs (POSIX)** : tube de contrôle par `pipe2(O_CLOEXEC)` (repli `pipe` + `fcntl` hors
  Linux/BSD), fichier de sortie ouvert `O_CLOEXEC` ; dans l'enfant, les descripteurs utiles sont
  d'abord déplacés au-dessus de 2 (`F_DUPFD_CLOEXEC`) — si l'appelant avait fermé 0, 1 ou 2, ils
  pouvaient y avoir atterri —, puis tout descripteur hérité ≥ 3 est fermé (`close_range` sous Linux,
  boucle jusqu'à `_SC_OPEN_MAX` sinon). **[M]** harnais ASan/UBSan : un descripteur 50 ouvert sans
  `FD_CLOEXEC` dans le parent n'apparaît pas dans `/proc/self/fd` de l'enfant, alors que le même
  détecteur le voit à travers `popen` (témoin) ; parent avec 0-2 fermés : sortie et code corrects.
- **`posix_spawn` écarté** : changer de répertoire de travail y demande
  `posix_spawn_file_actions_addchdir_np`, non portable (glibc ≥ 2.29).
- **Handles hérités restreints** (Windows, `PROC_THREAD_ATTRIBUTE_HANDLE_LIST`) : l'enfant ne reçoit
  que NUL et le fichier de sortie, même si l'application a d'autres handles héritables.
- **Descendants** : `openscad.com` est un lanceur qui crée `openscad.exe` **[M]** (`Get-CimInstance
  Win32_Process` : `openscad.exe` a pour parent `openscad.com`). D'où un job
  `KILL_ON_JOB_CLOSE` sous Windows et un groupe de processus tué par `kill(-pid)` sous POSIX ;
  après annulation, on attend que le job soit vide avant de supprimer le répertoire temporaire.
- **`openscad.exe` préféré à `openscad.com`** : `openscad.exe` est en sous-système GUI (2),
  `openscad.com` en console (3) **[M]** (en-tête PE). Lancé directement avec des handles redirigés,
  `openscad.exe` écrit bien son diagnostic sur stderr **[M]** (`--version` : 0 octet sur stdout,
  26 sur stderr). Le `.com` reste accepté s'il est configuré.
- **Sortie** : stdout et stderr réunis dans un fichier du répertoire temporaire (un fichier ne se
  remplit pas comme un tube : pas de blocage possible). Deux plafonds : sur disque,
  `maxCapturedBytes` (64 Mio par défaut) — la taille du fichier est surveillée à chaque tour
  d'attente et le programme est tué au-delà (`OutputLimitExceeded`, test
  `output_limit_kills_a_chatty_process` : boucle d'`echo` tuée à 64 Kio) ; en mémoire,
  `maxOutputBytes` (1 Mio), seule la fin étant rendue.
- **Délai et annulation** : `timeout` (0 = aucun) et `shouldCancel`, interrogé toutes les 50 ms sur
  le thread appelant. sinaia n'impose pas de délai (un rendu CGAL légitime peut durer des minutes)
  mais affiche après 500 ms un `wxProgressDialog` avec bouton d'annulation.
- **Deux répertoires temporaires uniques** par import : `cg-scad-<64 bits aléatoires>` (le
  `model.off` écrit par OpenSCAD) et `cg-process-<…>` (la sortie capturée), création exclusive,
  droits 0700 sous POSIX, supprimés par RAII dans tous les cas — succès, erreur, annulation, délai
  **[M]**. Leur racine est réglable (`ImportOptions::temporaryRoot`, `ProcessRequest::temporaryRoot`) :
  les tests vérifient le nettoyage dans une racine qui leur est propre, et non dans le répertoire
  temporaire du système, partagé avec `ctest -j` ou un import concurrent.
- **Répertoire de travail = celui du .scad** : `include <parts/block.scad>` relatif résolu **[M]**
  (test `relative_include_resolves_next_to_the_source`).

### D4. Format intermédiaire : OFF

| Mesuré sur `cube_minus_sphere.scad` (CGAL) | OFF | STL ASCII | 3MF | AMF |
|---|---|---|---|---|
| Taille du fichier | 24 994 o | 177 771 o | 12 074 o | 148 251 o |
| Sommets lus par cgmesh / faces | 472 / 960 | 2 880 / 960 | — | — |
| Lecteur cgmesh | sans dépendance | sans dépendance | lib3mf (`CG_HAS_LIB3MF`) | aucun |
| Précision écrite par OpenSCAD 2021.01 | 6 chiffres significatifs (`6.50301`) | idem | 6 décimales (`%f`) | 6 chiffres sign. |

Instruments **[M]** : `openscad.exe -o out.<ext>` puis `ls -la` ; sonde gtest temporaire
(`VMeshesIO::load` + `ValidateMesh`, retirée après mesure). Sur `slow.scad` (minkowski, 6 654
faces) : OFF 203 540 o, 3 329 sommets, lecture 5,0 à 6,1 ms ; STL 1 342 464 o, 19 956 sommets,
lecture 26,9 à 37,6 ms (deux passes, Release). Le STL de ce modèle a 6 652 faces contre 6 654 en
OFF : OpenSCAD en a écarté deux à l'écriture **[M]**, cause non instruite.

Durée d'un appel complet (lancement d'OpenSCAD compris) : ≈ 1,0 s pour les quatre formats sur
`cube_minus_sphere.scad` **[M]** (`Measure-Command`, `Start-Process -Wait`), dont 0,48 s de rendu
CGAL annoncés par OpenSCAD. Le format ne pèse rien devant le démarrage d'OpenSCAD.

OFF est retenu : indexé (maillage fermé sans soudure à la lecture : 0 arête de bord **[M]**), lu
sans dépendance, 7 fois plus compact que le STL. 3MF serait plus précis pour les grandes
coordonnées, mais dépend de lib3mf, optionnelle (absente de `build/peles-boilerplate`).

**Constat qui a imposé une correction du lecteur OFF** **[M]** : pour un objet qui ne passe pas par
CGAL (primitive seule, `polyhedron()`), OpenSCAD 2021.01 écrit des **polygones** — `4 0 1 2 3`
pour `cube(5)`, octogones pour `cylinder($fn=8)`, et des hexagones **concaves** tels que donnés à
`polyhedron()`. L'ancien `import_off` refusait toute face non triangulaire (« illegal file »). Le
lecteur accepte désormais N ≥ 3, et `ImportScad` triangule (`Mesh::Triangulate`, qui passe par
glutess pour les faces concaves). Prédiction P5/P6 démentie sur ce point (§6).

### D5. Paramètres `-D`

API prête, sans interface : `ImportOptions::defines`, liste de `{nom, valeur}`. Le nom est validé
(identifiant OpenSCAD, `$` initial admis). Passer `nom=valeur` comme argument distinct empêche
l'injection d'**options**, pas l'injection de **code** : OpenSCAD concatène `nom=valeur;` au source.
**[M]** `openscad -o x.off -D "size=1; translate([100,0,0]) cube(3)" parametric.scad` → boîte
jusqu'à x = 103, un cube a été ajouté ; `import()` ou `surface()` y seraient tout aussi possibles.

La valeur doit donc être un **littéral** (`cgmesh::scad::IsLiteralValue`), sinon l'import est refusé
(`InvalidInput`, message nommant le paramètre) :

    valeur  := nombre | true | false | undef | chaîne | vecteur
    nombre  := [+-]? (chiffres [. chiffres?] | . chiffres) ([eE] [+-]? chiffres)?
    chaîne  := " (caractère imprimable | \ caractère imprimable)* "
    vecteur := [ ] | [ valeur (, valeur)* ]          profondeur ≤ 32

Tests : 15 littéraux acceptés et 20 refusés, dont la valeur d'injection mesurée, une chaîne non
fermée, une chaîne contenant un retour à la ligne, `1+1`, `[0:10]`, `import("x.stl")` et 40
niveaux d'imbrication ; avec OpenSCAD, `size=[2, 3, 4]` donne la boîte (2, 3, 4) et
`label="a; translate([100,0,0]) cube(3)"` reste une chaîne inerte (boîte inchangée,
**[M]** aussi en direct : x max = 5). **Mutation [M]** : chaîne non fermée acceptée et profondeur
portée à 1000 → `define_values_must_be_literals` échoue sur ces deux cas ; validateur neutralisé
(toujours vrai) → le même test échoue. Test `defines_override_top_level_variables` : boîte
5 → 7,5.

---

### D6. sinaia : le répertoire réglé est la seule source (décision utilisateur du 2026-10-20)

Décision : **OpenSCAD n'est pas copié** dans le répertoire de build. Dans File > Settings > Import,
on renseigne **le répertoire qui contient `openscad.exe`**. Quand ce réglage est valide, l'import des
`.scad` est possible ; sinon il ne l'est pas. Interprétation appliquée :

1. **Réglage = répertoire** (`wxDirPickerCtrl` avec zone de texte, clé wxConfig
   `/External/OpenSCADDirectory`). **Valide** = répertoire existant contenant `openscad.exe`, fichier
   régulier (Windows), ou `openscad` avec un bit d'exécution (POSIX) —
   `cgmesh::scad::CheckOpenScadDirectory`, sans lancer OpenSCAD. États : vide, inexistant, pas un
   répertoire, sans exécutable, valide ; affichés sous le champ à chaque saisie (vert / rouge / gris).
   `openscad.com` seul ne rend **pas** le répertoire valide.
   **Migration** : à défaut de la nouvelle clé, l'ancienne `/External/OpenSCAD` (chemin de
   l'exécutable) est convertie en son répertoire s'il est valide, ignorée sinon
   (`MigrateOpenScadSetting`). Rien n'est écrit à la lecture : la conversion est enregistrée à la
   validation des réglages, qui retire alors l'ancienne clé.
2. **Seule source dans sinaia** : `SINAIA_OPENSCAD_HINT` et sa définition sont supprimés (la dette
   CWE-427 est soldée) ; sinaia ne consulte plus `CG_OPENSCAD`, le `PATH` ni `%ProgramFiles%` : il
   passe à cgmesh le chemin explicite `<répertoire>/openscad.exe`, et un réglage explicite y est
   exclusif. `EXTERN_OPENSCAD` reste dans le `CMakeLists.txt` racine pour la **seule** suite de
   tests (`CG_TEST_OPENSCAD_HINT`), qui saute ses cas OpenSCAD sans lui ; rien n'est copié.
3. **Réglage vide ou invalide** : « OpenSCAD » disparaît du filtre des dialogues File > Open et
   File > Add, agrégat « All supported formats » compris (`BuildOpenWildcardText (false)`). Le
   panneau Files continue de **montrer** les `.scad`, pour que leur ouverture explique la marche à
   suivre. Un `.scad` ouvert (panneau Files, glisser-déposer, File > Open avec « All files »),
   ajouté (Shift + dépôt, File > Add), rafraîchi ou demandé par la console est refusé avec un
   message qui renvoie à File > Settings > Import : boîte de dialogue dans les gestes interactifs
   (une par lot pour un ajout), journal pour la console (`open` journalise et n'ouvre pas d'onglet,
   `append` rend `ERR`). Le rafraîchissement refusé **conserve** le modèle affiché. Réglage valide :
   comportement inchangé.
4. **Sans redémarrage** : le réglage est relu à chaque ouverture, à chaque construction du filtre
   et à chaque rafraîchissement.
5. **Tests sans interface** (`test/tu_sinaia_scad_setting.cpp`, 5 tests, TU) : les cinq états, un
   répertoire nommé comme l'exécutable (témoin), `openscad.com` seul (Windows) ou fichier sans bit
   d'exécution (POSIX), conversion de l'ancien réglage (valide, invalide, disparu, casse, répertoire
   déjà au nouveau format), filtre avec et sans OpenSCAD (le détecteur compte trois `*.scad` quand
   OpenSCAD est disponible), catalogue du panneau Files, messages de refus. **Mutations [M]**, toutes
   tuées : répertoire existant suffisant, répertoire au nom de l'exécutable accepté, filtre qui ignore
   l'exigence OpenSCAD, conversion sans contrôle de validité. Les tests ont été écrits avec la
   règle ; ils ne sont pas passés par un état rouge avant elle, mais les mutations établissent qu'ils
   peuvent échouer.

**Non vérifié à l'écran** (sinaia n'a pas été lancé, pour ne rien écrire dans le registre réel ;
tout ce qui suit est **compilé, non exécuté**) : le sélecteur de répertoire, la mise à jour de l'état
à la frappe (`wxEVT_TEXT` de la zone de texte et `wxEVT_DIRPICKER_CHANGED`) et ses couleurs,
l'enregistrement et la migration des clés wxConfig, le filtre réel des dialogues Open / Add, les
boîtes de refus (ouverture, ajout, rafraîchissement), l'absence d'onglet, la prise d'effet sans
redémarrage. La logique de décision qu'ils emploient est, elle, testée dans TU.

---

### D7. Revue de D6 (2026-10-20) : corrections

1. **Chemin relatif refusé** : `CheckOpenScadDirectory` rend `Relative` pour tout chemin non absolu
   (« . » dépendait du répertoire courant de sinaia) ; affiché « Invalid: the path must be absolute. ».
2. **POSIX** : l'exécutable est jugé par `access (X_OK)` et non par « un bit x quelconque » : un
   `others_exec` seul ne valide plus le répertoire pour son propriétaire. Le test du cas
   `others_exec` est sauté sous root, que `access` laisse passer dès qu'un bit x est posé.
3. **Migration sans perte** : `MigrateOpenScadSetting` rend `{répertoire, converti, note}`. Tout
   chemin absolu non répertoire (nom `openscad*` ou non) donne son parent s'il est valide ; sinon
   rien n'est retenu et la **note** dit pourquoi (affichée dans Settings sous l'état, tant que le
   champ n'est pas modifié). Un relatif n'est pas converti, note à l'appui. À l'enregistrement,
   l'ancienne clé n'est effacée que si la nouvelle valeur est valide **ou** si le champ a été
   modifié (`ShouldDeleteLegacyOpenScadSetting`) : un ancien exécutable momentanément inaccessible
   ne disparaît plus sur un simple OK.
4. **`ReloadModel`** : le refus d'un `.scad` sans OpenSCAD est vérifié **avant** de détacher les
   maillages du renderer ; l'affichage courant est réellement conservé.
5. **Console `open`** : un `.scad` refusé ou en échec répond `ERR open failed: <chemin> -- <motif>`
   (motif sur une ligne), aucun onglet n'étant ouvert. L'échec de lecture des **autres** formats
   ouvre toujours un onglet et répond `opened` : laissé en dette (le corriger changerait leur
   comportement).
6. Documentation (D2 point 1, §5 marqué historique) ; texte de cache d'`EXTERN_OPENSCAD`
   (« used by the test suite only »).
7. **Saisie** : la validation du champ, qui touche le disque, est différée de 300 ms après la
   dernière frappe (`wxTimer`) au lieu de suivre chaque caractère ; la validation initiale, à
   l'ouverture du dialogue, reste synchrone (dette `debt_sinaia.md` §6).

Tests ajoutés ou modifiés [M] : `Relative` (« . », chemin relatif), `others_exec` seul (POSIX,
non exécuté faute de Docker, voir §10), migration (6 cas dont un nom non `openscad*`, un chemin
disparu, un relatif — note obligatoire), règle d'effacement (4 cas). **Mutations tuées** [M] :
suppression du refus des relatifs, effacement inconditionnel de l'ancienne clé, note vidée, chemin
non convertible retenu tel quel.

### D8. CI Linux : OpenSCAD installé et exigé (2026-10-20)

- `.github/workflows/build.yml` : `openscad` installé dans les deux jobs qui lancent ctest —
  `ci-linux` et `ci-linux-asan`. `ci-linux-boilerplate` ne lance aucun test **[V]**. `sq.yml`
  (couverture) lance TU par `coverage.sh` **[V]** : non modifié, hors demande ; ses tests `.scad` y
  restent sautés.
- **Ligne apt séparée** : `sudo apt-get install -y --no-install-recommends openscad`, pour borner
  l'option à ce seul paquet sans toucher la liste existante. Mesuré sur `ubuntu:24.04` nue **[M]**
  (`apt-cache policy`, `apt-get install --assume-no`) : version **2021.01-6build4**, la même
  famille que les binaires Windows et Debian testés ; 245 paquets, 137 Mo téléchargés, 593 Mo
  installés avec les recommandations (`openscad-mcad`), contre 138 paquets, 84,3 Mo et 331 Mo sans.
  Sur un runner déjà garni, le coût réel est moindre **[H]** (non mesuré : pas d'accès aux logs).
  Export OFF vérifié sans les recommandations (`openscad -o c.off` : `OFF 8 6 0`).
- **`CG_REQUIRE_OPENSCAD: "1"`** dans l'env de l'étape de test des deux jobs. Règle
  (`test/scad_test_support.h`, seul endroit) : OpenSCAD est **exigé** dès que la variable est **non
  vide et différente de `0`** — une valeur inattendue (`true`, `yes`) exige plutôt que de laisser
  une CI sauter en silence. Exigé et introuvable : les tests **échouent** avec les emplacements
  essayés et la variable ; sinon `GTEST_SKIP` comme avant. Décision pure `scad_test::Decide`,
  testée (`ScadTestSupport.*`) ; **mutations tuées** [M] : « 0 » traité comme exigé, échec changé
  en saut, OpenSCAD trouvé ignoré. `tu_sinaia_scad_setting.cpp` ne lance jamais OpenSCAD et ne
  saute rien : non concerné.

### D9. SonarCloud sur la PR #3 (2026-10-20)

La PR #3 (commit `d2b7803`) passe ci-linux, ci-linux-asan, Build et ci-linux-boilerplate ; la
Quality Gate SonarCloud échoue sur deux points.

**1. Couverture du code nouveau : 66,3 % (< 80 %).** Chiffres SonarCloud transmis : `import_scad.cpp`
62,4 % (165 lignes non couvertes), `external_process.cpp` 67,0 % (104), `mesh_io.cpp` 90,6 % (6),
`import_scad.h` 0 % (1). Cause [V par lecture] : `sq.yml` lance TU sous couverture llvm
(`coverage.sh`, depuis `build_coverage/test/test-run`) sans installer OpenSCAD, donc les 12 tests
`.scad` y étaient sautés.

- `sq.yml` : même ligne apt séparée que `build.yml` (`--no-install-recommends openscad`,
  `openscad --version`) et `CG_REQUIRE_OPENSCAD: "1"` sur l'étape « Run coverage ». TU y trouve
  OpenSCAD par le PATH.
- Le code Windows n'est pas compté sous Linux [V par lecture] : `convert_lcov.py` ne transmet à Sonar
  que les lignes `DA:` de `llvm-cov export`, soit les lignes compilées et instrumentées.
- Ce qui restera non couvert même avec OpenSCAD [E, par lecture] : dans `external_process.cpp`,
  environ 29 lignes de l'enfant entre `fork` et `execv` (le profil de l'enfant n'est jamais écrit :
  il fait `execv` ou `_exit`), 9 de `CloseInheritedDescriptors` (enfant aussi), 6 du repli
  `pipe` + `fcntl` (Linux a `pipe2`), environ 17 de branches d'erreur système (échec de `open`,
  `pipe`, `fork`, `waitpid`, `mkdtemp`, exception) — soit environ 61 sur 315 ; dans `import_scad.cpp`,
  environ 15 (exception, sortie trop longue côté import, `%ProgramFiles%` non compilé) ;
  `mesh_io.cpp`, 6.
- Tests ajoutés, sans OpenSCAD : répertoire de travail inexistant ; fichier qui n'est pas un programme
  (`LaunchFailed`, chemin du tube de contrôle côté parent) ; sortie tronquée en gardant sa fin ;
  noms de toutes les issues et de tous les statuts ; recherche générique variable → PATH → recours
  (variables posées et rendues le temps du test, `%ProgramFiles%` neutralisé) ; exécutable trouvé
  mais qui ne démarre pas (`LaunchFailed`). POSIX seulement : un faux OpenSCAD (script shell)
  produit « pas de fichier », un OFF illisible (`ReadFailed`), un OFF vide (`EmptyResult`), un
  code non nul avec et sans diagnostic. **[M]** Windows : 6 tests de plus, TU 2 312 / 2 312.
  **Le test POSIX n'a été ni compilé ni exécuté** : Docker était en panne.
- **Estimation [E]** : environ 82 lignes non couvertes sur environ 819, soit **≈ 90 %** (fourchette
  85-92 %), au-dessus du seuil de 80 %. Ce qui confirmerait : la prochaine analyse SonarCloud.

**2. Vulnérabilité cpp:S5443** (« publicly writable directories », `TemporaryDirectory`).
Risque réel avant correctif [V par lecture] : `create_directory` est un `mkdir(2)` exclusif — un
lien symbolique pré-placé donne EEXIST et le nom est abandonné, aucun répertoire étranger n'est
adopté ; le nom porte 64 bits aléatoires. Mais entre `mkdir` (mode `0777 & ~umask`) et le `chmod
0700` qui suivait, le répertoire était lisible, et avec un umask permissif inscriptible, par les
autres utilisateurs : l'un d'eux pouvait y placer un lien `output.log`, que l'`open (O_CREAT |
O_TRUNC)` aurait suivi. Correctif : sous POSIX, **`mkdtemp`**, qui crée atomiquement en 0700 sous
un nom imprévisible, sans fenêtre. Sous Windows, `%TEMP%` est propre à l'utilisateur et
`CreateDirectory` exclusif : inchangé. API et préfixe inchangés, tests existants inchangés.

La règle restera **probablement levée** [H] : elle signale l'usage même du répertoire temporaire du
système (`fs::temp_directory_path ()`), qui reste nécessaire. Aucun commentaire `NOSONAR` n'a été
ajouté. Justification proposée, à saisir dans SonarCloud (statut « Safe ») :

> Temporary directory created with mkdtemp(3) on POSIX: atomic creation, mode 0700, unpredictable
> name, so no other user can read, write or pre-place anything inside it. On Windows the location
> is the per-user %TEMP% and CreateDirectory is exclusive (an existing name is never adopted). The
> directory holds only the child process output and the OpenSCAD mesh, and is removed by RAII.

---

## 3. Corrections hors module, imposées par la mesure

- **`MeshIO::import_off`** : polygones, lectures vérifiées (`fscanf` contrôlés), en-tête menteur
  refusé avant allocation (6 octets minimum par sommet, 8 par face), indice hors bornes refusé,
  maillage vidé en cas d'échec. Les entiers sont lus en `long long` (`long` fait 32 bits sous MSVC).
- **`MeshIO::export_off`** écrivait `"%ud %ud %d"` — l'en-tête sortait en `8d 12d 0` — et des faces
  sans leur nombre de sommets ; il rendait `false` (donc 0, « succès ») sur échec d'ouverture. Il
  écrit maintenant de l'OFF conforme, faces polygonales et trous de `RemoveFace` compris.
- **`test/data/cube.off`** n'était pas de l'OFF (faces `0 3 2` sans arité, troisième compte
  `-858993460`). **`TEST_cgmesh_io.off` passait à vide** : l'ancien lecteur échouait dès la première
  face (`n = 0`, retour −1) mais laissait 8 sommets et 12 faces vides, que le test comptait. La
  fixture est corrigée et le test vérifie désormais le code de retour, une face, et la relecture de
  l'export.

---

## 4. Tests

`test/tu_cgmesh_import_scad.cpp`, 25 tests sous Windows (24 sous Linux : quoting et scripts `.bat`
propres à Windows, arguments verbatim propre à POSIX) **[M]** (Release MSVC,
`build/peles-boilerplate/test/test-run`) :

| Groupe | Tests | Exige OpenSCAD |
|---|---|---|
| `ExternalProcess` | introuvable, sortie + code + cwd à accents et espaces, délai et annulation **de l'arbre** (le petit-enfant `ping`/`sleep` lancé par le shell meurt ; le détecteur le voit vivant avant l'annulation), borne de sortie, quoting Windows (ou arguments verbatim sous POSIX), répertoire temporaire visible puis supprimé dans une racine dédiée | non |
| `OffReader` | polygones et couleurs de face, 6 en-têtes ou corps faux, témoin négatif de triangulation en éventail | non |
| `ImportScad` | exécutable configuré absent, entrées invalides, littéraux `-D`, scripts `.bat`/`.cmd` refusés (Windows) | non |
| `ImportScad` | cube − sphère (boîte ±10, 0 arête de bord ni non-manifold, volume dans ]1000, 8000[), témoin négatif de fermeture, cube primitif en quadrilatères (12 triangles, `IsValid`, volume 125), faces concaves (aire 208, volume 128), `-D`, include relatif, chemin à espaces/accents/`&`/`;`, erreur de syntaxe (« Parser error », sortie intacte, nettoyage), objet vide, objet 2D (« not a 3D object »), annulation et délai sur `slow.scad` (+ nettoyage), vecteur et chaîne à `;` en `-D` | oui (12 tests) |

Sans OpenSCAD, ces 12 tests sont **sautés** (`GTEST_SKIP`) **[M]** : avec
`CG_OPENSCAD=C:\nowhere\openscad.exe` (une variable explicite invalide masque toutes les autres
voies), 13 réussis et 12 sautés, total 25 ; sous Linux avec `PATH=/nonexistent`, 3 `ImportScad`
réussis et 12 sautés.

**Témoins négatifs** (détecteurs validés sur un cas qui doit échouer) :

- fermeture : face retirée → `BoundaryEdge > 0` ;
- triangulation : le volume ne peut **pas** détecter un éventail faux — l'aire signée d'un éventail
  est toujours celle du polygone (**[E]** calcul à la main sur le L : −50 −20 +18 −12 = −64). Seule
  l'aire non signée discrimine : 280 en éventail contre 208 **[M]**. Le test concave vérifie donc
  l'aire, et le témoin prouve que le détecteur échoue sur l'éventail ;
- nettoyage : le détecteur voit un répertoire `cg-scad-*` vivant, puis plus après destruction.

---

## 5. Vérification dans sinaia

⚠ **Historique, antérieur à D6 (première livraison, 2026-10-19).** Les mesures ci-dessous ont été
prises quand sinaia cherchait encore OpenSCAD par `CG_OPENSCAD`, le PATH et un chemin compilé, et
qu'un échec laissait un onglet vide. Ce comportement a disparu (passe de correction, puis D6) ; la
seule vérification de sinaia postérieure est la compilation.

**[M]** console distante (port 7777), `build/peles-sinaia`, Release :

- `open cube_minus_sphere.scad` → `meshes 1, vertices 472, faces 960, bbox -10…10` ;
- `append uses_include.scad` → `models 2` ;
- `open syntax_error.scad` → onglet vide (`meshes 0`), échec journalisé ;
- `append empty.scad` → `ERR append failed` ;
- sinaia lancé avec `CG_OPENSCAD=C:\nowhere\openscad.exe` → `open` laisse un onglet vide, `append`
  rend `ERR` ;
- capture d'écran différée : le cube évidé est affiché. Une capture prise **juste après** `open` est
  noire pour un `.scad` comme pour `cube.obj` **[M]** : artefact de synchronisation de
  l'instrument, pas du format.

**Non vérifié dans l'interface** (aucune commande console ne les exerce) : le dialogue File > Open
et son filtre, le sélecteur de File > Settings > Import et sa persistance, la boîte d'erreur de
`OpenDocument` (différée par `CallAfter`), le dialogue de progression et son bouton d'annulation.
L'annulation elle-même est testée au niveau de cgmesh.

**Passe de correction (2026-10-19, après revue)** — comportement de sinaia modifié, **compilé mais
non exécuté** : sinaia n'a pas été relancé, car toute ouverture réécrit le réglage persistant
`/General/LastDir` (registre), ce qui est désormais exclu ; l'état décrit ci-dessus (onglet vide
sur échec) est donc celui d'avant la passe. Désormais **[V]** par lecture :

- un `.scad` qui échoue ou est annulé n'ouvre **plus d'onglet** (`OpenDocument`) ; les autres
  formats gardent le leur ;
- une **annulation** n'affiche aucune boîte d'erreur et se journalise « Chargement annule » ;
- le **rafraîchissement** journalise un seul échec (« Echec du rafraichissement »), plus deux ;
- pour un `.scad`, `OpenDocument` ne crée plus de `wxBusyInfo` : sa fenêtre est créée
  `wxSTAY_ON_TOP` **[H]** (souvenir de l'implémentation wx 3.x ; source de `busyinfo.cpp` absente
  de la machine) et centrée comme le `wxProgressDialog`, qu'elle pouvait donc couvrir. Le curseur
  d'attente reste. Non vérifié à l'écran.

---

## 6. Journal de prédictions

Écrites avant mesure (`scratchpad/scad/predictions.md`) :

| # | Prédiction | Résultat |
|---|---|---|
| P1 | `.exe` GUI, `.com` console | ✅ sous-systèmes 2 et 3 |
| P2 | `.com` lance `.exe` en enfant | ✅ |
| P3 | `.exe` écrit sur un stderr redirigé | ✅ |
| P4 | exports STL, OFF, AMF, 3MF | ✅ (`--help`, et les quatre produits) |
| P5 | OFF indexé, ~6 chiffres | ✅ pour CGAL ; ❌ **non triangulé** hors CGAL — non prévu |
| P6 | maillage fermé après lecture OFF | ✅ après correction du lecteur (il refusait les polygones) |
| P7 | syntaxe : code ≠ 0, « Parser error » | ✅ code 1 |
| P8 | objet vide : « Current top level object is empty. », code ≠ 0 | ✅ code 1, aucun fichier écrit |
| P9 | `cg_binaries` sans OpenSCAD | ✅ pour 1.0.0 ; 1.1.0 non présente localement |
| P10 | OFF par `Mesh::load`, 3MF sous `CG_HAS_LIB3MF` seulement | ✅ |

---

## 7. Non-régression

État après la passe de correction (les chiffres de la première livraison sont remplacés).

**Après D6 (2026-10-20)** **[M]** : TU 2 304 / 2 304 (2 299 + 5 `ScadSetting`), TU_faults 5 / 5 ;
sinaia reconstruit (`build/peles-sinaia`, Release), `sinaia.exe` plus récent que toutes les sources
touchées, aucune occurrence de `SINAIA_OPENSCAD_HINT` dans ses lignes de compilation ; GCC 14
(`-Wall -Wextra -Wshadow -Wconversion`) : 0 avertissement propre sur `import_scad.cpp`,
`external_process.cpp`, les deux fichiers de test, `ModelFileLoader.cpp`, `SettingsDialog.h`,
`SupportedFormats.h`, `FormatCatalog.h` ; maker.wasm : sha256 `7ad1f688…5b35`, non réécrit.
Linux (Docker `gcc:14`, OpenSCAD 2021.01 Debian) : 29 / 29 tests ciblés, dont les 5 `ScadSetting`
(branche POSIX du bit d'exécution) ; suite complète 2 297 tests, 2 294 réussis, 3 sautés préexistants.
Les chiffres ci-dessous sont ceux de la passe de correction.

- **TU** (passe de correction) : 2 299 / 2 299 **[M]** (2 274 + 25) — après D7/D8 : 2 306, §10 ; **TU_faults** : 5 / 5 **[M]** ; sinaia construit
  (MSVC Release).
- **Tests rouges avant correctif [M]** : API ajoutée avec des implémentations neutres, puis
  exécution — `temporary_directory_is_visible_then_removed_with_content`,
  `define_values_must_be_literals` et `windows_scripts_are_not_accepted_as_the_executable`
  échouaient, `output_limit_kills_a_chatty_process` ne terminait pas (tué par `timeout 300`) ;
  verts après correctif. Le test de l'arbre de processus passait déjà sous Windows (job).
- **maker.wasm** : sha256 `7ad1f688…5b35` avant la livraison, après elle et après la passe de
  correction, fichier non réécrit (horodatage 20:49 inchangé) **[M]** (`maker/build.ps1`, trois fois).
- **MSVC `/W4`** **[M]** (`cl` direct, Release) : `external_process.cpp`, `import_scad.cpp`,
  `tu_cgmesh_import_scad.cpp`, `ModelFileLoader.cpp` : 0 avertissement sur leurs propres lignes ;
  `mesh_io.cpp` : 58 → 54 lignes d'avertissement, aucune nouvelle (comparaison triée contre
  `cb3ec80`, les deux `fscanf ("%*[^\n]")` du lecteur remplacés par une lecture caractère par
  caractère pour ne pas ajouter de C4996).
- **GCC 14.4** (Docker `gcc:14`, `-Wall -Wextra -Wshadow -Wconversion`) **[M]** :
  `external_process.cpp`, `import_scad.cpp` (après `[[maybe_unused]]` sur `note`, inutilisé hors
  Windows), `tu_cgmesh_import_scad.cpp`, `ModelFileLoader.cpp`, `SettingsDialog.h`,
  `SupportedFormats.h` : 0 ; `mesh_io.cpp` : 31 → 27, aucun nouveau.
- **Branche POSIX exécutée** **[M]** : harnais de 21 vérifications sous ASan + UBSan, 0 échec —
  arguments verbatim, droits 0700, descripteur hérité fermé (avec témoin `popen`), parent à 0-2
  fermés, délai et annulation d'un groupe, borne de sortie, échecs de `chdir` et d'`execv`, mort par
  signal (137).
- **Construction Linux complète** **[M]** (Docker `gcc:14`, CMake + Ninja, Release, OpenSCAD
  2021.01 Debian) : 25 / 25 tests ciblés (24 du fichier + `TEST_cgmesh_io.off`) ; avec
  `PATH=/nonexistent`, 3 réussis et 12 sautés ; suite complète 2 292 tests, 2 289 réussis,
  3 sautés préexistants (police, deux tests de locale), 0 échec.
- **Fichiers sinaia sous GCC** : `wxOpenGLCanvas.cpp` et `SinaiaFrame.cpp` ne compilent pas sous
  GCC/wxGTK pour des raisons préexistantes (WGL non gardé) : non vérifiables, dette
  `debt_sinaia.md` §6.

---

## 8. Limites connues

- **Ombrage** : l'OFF est indexé, sinaia moyenne les normales par sommet, donc les arêtes vives d'un
  modèle CAO sont lissées (visible sur la capture). Dette `debt_sinaia.md` §6.
- **Précision** : 6 chiffres significatifs (OpenSCAD 2021.01, OFF) — un sommet à 1234,5678 est écrit
  1234,57 **[E]**. Négligeable pour l'affichage, pas pour une mesure fine.
- **Durée** : chaque ouverture relance OpenSCAD (≈ 1 s de démarrage) ; pas de cache.
- **Couleurs** (`color()`) perdues : OFF 2021.01 n'en porte pas.
- **Classement des échecs** : `EmptyResult` se reconnaît au texte « top level object is empty »,
  propre à la version d'OpenSCAD ; le message affiché, lui, vient toujours de la sortie réelle.
- **Encodage** (Windows) : le fichier OFF temporaire est relu par `fopen` en encodage étroit ; un
  répertoire temporaire hors page de code ANSI ferait échouer la lecture (`ReadFailed`) **[E]**.
- **Sécurité : un `.scad` est un programme.** Ouvrir un `.scad` exécute du code fourni par son
  auteur : il peut lire tout fichier accessible à l'utilisateur (`import()`, `surface()`,
  `include`) et faire tourner OpenSCAD indéfiniment. OpenSCAD 2021.01 date de janvier 2021 ; des
  défauts de ses importeurs ont été corrigés dans des versions ultérieures **[H]** — à vérifier
  dans le journal des versions d'OpenSCAD avant de citer un identifiant de vulnérabilité, ce qui
  n'a pas été fait ici. Recommandation : n'ouvrir que des `.scad` de confiance, et utiliser une
  version récente d'OpenSCAD (§9).
- **Descendants à la fin normale (POSIX)** : le groupe de processus n'est tué qu'en cas
  d'annulation, de délai ou de dépassement de sortie ; un descendant détaché qui survit à la fin
  normale d'OpenSCAD n'est pas tué (Windows : le job le tue à sa fermeture). Dette
  `debt_cgmesh.md`.
- ~~Recours `SINAIA_OPENSCAD_HINT`~~ : supprimé le 2026-10-20 (D6).

---

## 9. Suite possible

1. **Paramètres `-D` dans l'interface** : l'API est prête ; OpenSCAD ≥ 2019 sait exporter le
   customizer (`-p`/`-P`, fichier JSON) — un panneau généré depuis les variables de premier niveau.
2. **OpenSCAD récent avec Manifold** (2024+, `--backend=manifold`) : rendus des minutes → secondes
   sur les booléens lourds ; vérifier alors la précision et la triangulation de l'OFF (et `-O`
   pour les options d'export), et l'ajouter à `cg_binaries` pour que la CI exécute les 12 tests.
3. **Arêtes vives** : séparation des normales par angle de pli pour les imports CAO.
4. **maker** : hors voie A (aucun processus dans un navigateur) ; la voie possible est
   OpenSCAD compilé en WASM (openscad-wasm), à instruire séparément.

---

## 10. Mesures de la passe CI et de la revue (2026-10-20)

- **Windows** **[M]** : TU 2 306 / 2 306, TU_faults 5 / 5 ; `CG_REQUIRE_OPENSCAD=1` avec OpenSCAD
  présent : 17 / 17 `ImportScad` + `ScadTestSupport` ; témoin `CG_REQUIRE_OPENSCAD=1` et
  `CG_OPENSCAD` invalide : 12 échecs, message « OpenSCAD introuvable alors que
  CG_REQUIRE_OPENSCAD=1 l'exige ; emplacements essayés : … » ; `CG_REQUIRE_OPENSCAD=0` : sautés.
  sinaia reconstruit, plus récent que toutes les sources touchées. MSVC `/W4` : 0 avertissement
  propre sur `import_scad.cpp`, les deux fichiers de test, `ModelFileLoader.cpp`,
  `SettingsDialog.h`, `SupportedFormats.h`, `FormatCatalog.h`.
- **Linux, Docker `gcc:14` + `openscad` Debian, Release** **[M]**, sur l'état **antérieur** aux
  corrections de la revue (copie des sources prise avant elles) : `CG_REQUIRE_OPENSCAD=1 ctest -j 16`
  → 2 303 / 2 303 réussis, **aucun test `.scad` sauté** (les 3 sauts sont préexistants : police,
  deux tests de locale), 15,9 s. Marges sous charge (-j 16) : `timeout_and_cancellation_…` 2,35 s
  (borne 15 s), `long_render_is_cancelled_…` 1,03 s (borne 15 s), `output_limit_…` 0,07 s (borne
  10 s), `cube_minus_sphere…` 0,56 s.
- **Répertoire de build** : `build/peles-boilerplate` a disparu le 2026-10-20 vers 21:20
  (`build/` modifié à cette heure, avec `peles-asan`, `peles-probe` et `sinaia`), sans action de
  ma part ; la passe D9 a construit et testé dans `build/scad-check` (même configuration).
- **Non mesuré** : le témoin Linux `PATH=/nonexistent` (l'instrument était faux : il masquait
  `ctest` lui-même, code 127) ; la suite **ASan** (la construction a fait tomber le moteur Docker
  Desktop — erreur d'E/S puis API en 500 —, VM de 8 Go, `-j 8`) ; GCC 14 et le test POSIX
  `others_exec` sur le code de la revue. Docker Desktop n'a pas été redémarré : d'autres conteneurs
  de l'utilisateur en dépendent.
