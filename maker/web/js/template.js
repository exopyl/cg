// ===========================================================================
//  Page GABARIT : une page de maker entièrement décrite par un fichier JSON
// ===========================================================================
//
// Ce fichier ne nomme AUCUN nœud, aucun paramètre, aucun domaine. Tout ce qui
// distingue « Texte 3D » d'une autre page vit dans data/templates/<nom>.json :
// le graphe à évaluer, les paramètres à exposer, les sources de fichier. Ajouter
// une page ne doit toucher que ce dossier.
//
// POURQUOI CETTE PAGE ÉVALUE SUR LE THREAD UI, et non dans le Worker du canvas
// nodal. wasm.js et graph-worker.js chargent le MÊME artefact maker.js : les
// liaisons graph* existent donc des deux côtés. L'arrangement Worker +
// OffscreenCanvas (D17/D27) sert le CANVAS ImGui, pas l'évaluation. Rester sur
// le thread UI permet de garder Online3DViewer — donc les matériaux, le
// recadrage, l'export — là où le renderer du worker est un aplat à teinte
// unique. C'est le seul choix qui rende cette page équivalente à celle qu'elle
// remplace.
//
// LIMITE À CONNAÎTRE : graphExportObj rend un OBJ MINIMAL, sans mtllib — même
// contrainte que regenerate() pour les formes, o3dv chercherait un .mtl absent
// de sa FileList. Les matériaux ne traversent donc pas. Sans conséquence pour un
// solide à matériau unique (le texte) ; rédhibitoire pour un relief coloré, dont
// les couleurs SONT le résultat. Une page gabarit sur img.relief demandera
// d'abord un chemin qui les porte.
// ===========================================================================

import { loadModule } from "./wasm.js";
import { createViewer } from "./viewer.js";
import { saveBlob, safeName } from "./exporters.js";

const el = (id) => document.getElementById(id);

// Écrit une valeur de paramètre dans le graphe. Le TYPE vient du gabarit, pas
// d'une inspection : c'est lui qui décide du setter, donc une erreur de type se
// voit à l'écriture et non trois appels plus loin.
function writeParam(Module, spec, value) {
  const { node, param, type } = spec;
  switch (type) {
    case "int":
    case "enum":  return Module.graphSetInt(node, param, value | 0);
    case "float": return Module.graphSetFloat(node, param, +value);
    case "bool":  return Module.graphSetBool(node, param, !!value);
    default:      return Module.graphSetString(node, param, String(value));
  }
}

// Valeur initiale d'un paramètre, LUE DANS LE GRAPHE et non dans le gabarit :
// le document fait foi, sans quoi le panneau afficherait des valeurs que le
// moteur n'a pas.
function readParam(Module, spec) {
  const info = JSON.parse(Module.graphNodeInfo(spec.node));
  const found = (info.params || []).find((p) => p.name === spec.param);
  return found ? found.value : null;
}

// Type que le GRAPHE donne au paramètre, à confronter à celui du gabarit.
function graphType(Module, spec) {
  const info = JSON.parse(Module.graphNodeInfo(spec.node));
  const found = (info.params || []).find((p) => p.name === spec.param);
  return found ? found.type : null;
}

// Le type déclaré par le gabarit décide du SETTER. S'il ne correspond pas à
// celui du graphe, l'écriture RETYPE le paramètre — et l'adaptateur, qui lit par
// type exact (`GetInt` exige ParamType::Int), retombe alors sur son défaut. Le
// curseur bouge, et rien ne change.
//
// C'est une panne muette, et elle est facile à écrire : un entier déclaré
// « float » parce qu'on lui a mis un curseur. D'où cette vérification au
// chargement plutôt qu'une relecture attentive des gabarits.
const SETTER_TYPE = { int: "int", enum: "int", float: "float", bool: "bool", string: "string" };

function checkType(Module, spec, warn) {
  const actual = graphType(Module, spec);
  const expected = SETTER_TYPE[spec.type] || "string";
  if (actual && actual !== expected)
    warn(`gabarit : ${spec.node}.${spec.param} est « ${actual} » dans le graphe, `
       + `déclaré « ${spec.type} » (écrit du ${expected}) — le réglage serait sans effet`);
}

function buildWidget(Module, spec, onChange, warn) {
  const wrap = document.createElement("div");
  wrap.className = "param " + spec.type;

  const initial = readParam(Module, spec);
  if (initial === null) {
    // Le gabarit désigne un paramètre que le graphe n'a pas. On le DIT : un
    // widget muet qui n'écrit nulle part est le pire des deux mondes.
    warn(`gabarit : ${spec.node}.${spec.param} n'existe pas dans le graphe`);
    return wrap;
  }
  checkType(Module, spec, warn);

  const row = document.createElement("div");
  row.className = "row";
  const label = document.createElement("label");
  label.textContent = spec.label || spec.param;
  if (spec.hint) label.title = spec.hint;

  let input;
  if (spec.type === "bool") {
    input = document.createElement("input");
    input.type = "checkbox";
    input.checked = !!initial;
    input.addEventListener("change", () => {
      writeParam(Module, spec, input.checked);
      onChange();
    });
  } else if (spec.type === "enum") {
    input = document.createElement("select");
    (spec.choices || []).forEach((choice, i) => {
      const option = document.createElement("option");
      option.value = String(i);
      option.textContent = choice;
      input.appendChild(option);
    });
    input.value = String(initial | 0);
    input.addEventListener("change", () => {
      writeParam(Module, spec, Number(input.value));
      onChange();
    });
  } else if (spec.type === "string") {
    // Une ZONE DE TEXTE quand le gabarit le demande. Ce n'est pas un détail de
    // présentation : un <input type="text"> ne peut pas contenir de retour à la
    // ligne, donc sans cela le paramètre « Alignement » juste au-dessus serait
    // inutilisable — il ne déplace que des lignes qu'on ne pourrait pas saisir.
    if (spec.multiline) {
      input = document.createElement("textarea");
      input.rows = spec.rows || 3;
    } else {
      input = document.createElement("input");
      input.type = "text";
    }
    input.value = String(initial);
    // À la frappe : c'est le seul réglage dont on veut le retour immédiat, et
    // c'est ce que faisait déjà la page qu'on remplace.
    input.addEventListener("input", () => {
      writeParam(Module, spec, input.value);
      onChange();
    });
  } else {
    input = document.createElement("input");
    input.type = "range";
    input.min = spec.min !== undefined ? spec.min : 0;
    input.max = spec.max !== undefined ? spec.max : 1;
    input.step = spec.step !== undefined ? spec.step : 0.01;
    input.value = initial;
    const readout = document.createElement("span");
    readout.className = "readout";
    // Un entier s'affiche sans décimales : « 16 », pas « 16.000 ».
    const show = (v) => (spec.type === "int" ? String(Math.round(v)) : Number(v).toFixed(3));
    readout.textContent = show(initial);
    input.addEventListener("input", () => {
      readout.textContent = show(input.value);
      writeParam(Module, spec, Number(input.value));
      onChange();
    });
    row.appendChild(label);
    row.appendChild(readout);
    wrap.appendChild(row);
    wrap.appendChild(input);
    return wrap;
  }

  row.appendChild(label);
  row.appendChild(input);
  wrap.appendChild(row);
  return wrap;
}

export async function runTemplate(name) {
  const status = el("status");
  const setStatus = (t) => { if (status) status.textContent = t; };
  const warnings = [];
  const warn = (m) => { warnings.push(m); console.warn(m); };

  // ---- gabarit -----------------------------------------------------------
  const response = await fetch(`data/templates/${name}.json`, { cache: "no-store" });
  if (!response.ok) { setStatus(`gabarit ${name} introuvable`); return; }
  const tpl = await response.json();

  el("title").textContent = tpl.title || name;
  document.title = `${tpl.title || name} — maker`;
  if (tpl.subtitle) el("subtitle").textContent = tpl.subtitle;

  // ---- module ------------------------------------------------------------
  const { Module, staleMessage } = await loadModule();
  if (staleMessage) warn(staleMessage);

  const error = Module.graphFromJson(JSON.stringify(tpl.graph));
  if (error) { setStatus(`graphe refusé — ${error}`); return; }

  // ---- viewer ------------------------------------------------------------
  const modelColor = el("modelColor");
  const wireframe = el("wireframe");
  const viewer = createViewer({
    container: el("viewer"),
    hintEl: el("hint"),
    getModelColor: () => modelColor.value,
    getWireframe: () => wireframe.checked,
    // Grise le sélecteur de couleur quand le maillage porte les siennes.
    onVertexColors: (has) => {
      modelColor.disabled = has;
      modelColor.title = has
        ? "Le modèle porte ses propres couleurs (une par région)"
        : "";
    },
    // o3dv importe l'OBJ en SOUDANT les sommets coïncidents, ce qui lisse les
    // normales à travers les arêtes vives. On rebascule aussitôt sur la géométrie
    // non soudée de graphMeshData — qui porte aussi les couleurs, absentes de
    // l'OBJ minimal servi à l'amorçage.
    onModelReady: () => {
      if (Module) viewer.updateInPlace(() => Module.graphMeshData(0), 0, true);
    },
  });
  modelColor.addEventListener("input", viewer.applyModelColor);
  wireframe.addEventListener("change", viewer.applyWireframe);
  el("resetView").addEventListener("click", viewer.resetView);

  // ---- évaluation --------------------------------------------------------
  //
  // Coalescée : un curseur émet des dizaines d'événements par seconde, et
  // l'évaluation est SYNCHRONE (pilote en ligne). Sans cela, la page évaluerait
  // un graphe par pixel de déplacement.
  //
  // setTimeout et PAS requestAnimationFrame — c'est la même leçon que celle
  // déjà écrite dans pixels.js et relief.js, et je l'ai revérifiée ici à mes
  // dépens : rAF ne se déclenche PAS dans un onglet masqué ou occulté. Un
  // changement de police déclenché pendant que l'onglet est en arrière-plan
  // restait alors indéfiniment sur « chargement… », et rien ne repartait au
  // retour au premier plan.
  let queued = false;
  let lastObj = "";
  // Recadrage de la caméra : au premier rendu, et après un changement de SOURCE
  // (une autre police, une autre image) — le saut d'échelle y est arbitraire.
  // Pas sur un déplacement de curseur, où la vue doit rester où l'utilisateur l'a
  // mise.
  let refit = true;

  function evaluate() {
    if (queued) return;
    queued = true;
    setTimeout(() => {
      queued = false;
      const result = JSON.parse(Module.graphEvaluate(tpl.output));
      if (result.status !== "ok") {
        setStatus(`calcul : ${result.status}${result.detail ? " — " + result.detail : ""}`);
        return;
      }
      lastObj = Module.graphExportObj(0);
      if (!lastObj) { setStatus("le nœud de sortie n'a pas rendu de maillage"); return; }

      // AMORÇAGE UNE SEULE FOIS, puis mise à jour EN PLACE — c'est la discipline
      // de shell.js, et elle n'est pas cosmétique : bootstrap() est un rechargement
      // de fichier o3dv complet (scène vidée, canvas caché, caméra recadrée), donc
      // un scintillement à chaque déplacement de curseur.
      //
      // Et la mise à jour en place apporte ce que l'OBJ ne peut pas : les COULEURS
      // PAR SOMMET. graphExportObj rend un OBJ minimal sans mtllib ; graphMeshData
      // rend en plus l'attribut `color`, vide sur un maillage mono-matériau — le
      // sélecteur de couleur reprend alors la main, comme pour une forme.
      if (!viewer.isReady()) viewer.bootstrap(lastObj);
      else viewer.updateInPlace(() => Module.graphMeshData(0), 0, refit);
      refit = false;

      setStatus(`${result.nv} sommets · ${result.nf} triangles · ${result.ms} ms`);
    });
  }

  // ---- panneau -----------------------------------------------------------
  const params = el("params");
  params.innerHTML = "";
  for (const spec of tpl.expose || [])
    params.appendChild(buildWidget(Module, spec, evaluate, warn));

  // ---- sources de fichier ------------------------------------------------
  //
  // Une source par entrée d'octets déclarée. Les octets vont DIRECTEMENT dans le
  // nœud (graphSetBytes) : pas de détour par MEMFS, donc pas de fichier
  // temporaire à garder vivant pour la durée de la page.
  const sources = el("sources");
  sources.innerHTML = "";

  // DEUX RÉGIMES DE SOURCE, choisis par le nœud lui-même et non par le gabarit :
  //
  //   accepte des octets (text.font.load, img.io.load)  -> graphSetBytes ;
  //   sinon (file.ref)                                  -> la ressource est
  //       déposée en MEMFS AU CHEMIN QU'ELLE PORTE, puis le chemin est écrit
  //       dans le paramètre `path`. C'est ce qui rend le document reproductible :
  //       il nomme "data/fonts/Cinzel.ttf", et l'hôte fait exister ce chemin-là.
  //
  // Le C++ répond à la question (graphAcceptsBytes, un dynamic_cast vers
  // ByteSource) : aucune liste de types de nœuds n'est tenue ici.
  const acceptsBytes = new Map();

  function nodeAcceptsBytes(node) {
    if (!acceptsBytes.has(node)) acceptsBytes.set(node, !!Module.graphAcceptsBytes(node));
    return acceptsBytes.get(node);
  }

  // Fait exister `path` dans le système de fichiers du module.
  //
  // ⚠ N'ÉCRIT QUE SI LE CHEMIN EST ABSENT. Le mtime d'un fichier MEMFS est celui
  // de son écriture, et file.ref verse un STAT à sa signature : réécrire la même
  // ressource lui donnerait une identité neuve à chaque fois, et le cache
  // manquerait à tous les coups.
  async function placeFile(path, source) {
    try {
      const stat = Module.FS.stat(path);
      if (stat && stat.size > 0) return true;   // déjà là : on n'y touche pas
    } catch { /* absent : on écrit */ }

    // FS.writeFile ne crée pas les répertoires intermédiaires.
    const parts = path.split("/").filter(Boolean);
    let dir = path.startsWith("/") ? "" : ".";
    for (let i = 0; i < parts.length - 1; ++i) {
      dir += "/" + parts[i];
      try { Module.FS.mkdir(dir); } catch { /* existe déjà */ }
    }

    try {
      let bytes;
      if (source instanceof Blob) {
        bytes = new Uint8Array(await source.arrayBuffer());
      } else {
        // Le STATUT compte : sans ce test, la page d'erreur 404 du serveur serait
        // écrite dans le fichier, et l'échec surviendrait bien plus loin — dans un
        // décodeur annonçant « police illisible ».
        const res = await fetch(source, { cache: "force-cache" });
        if (!res.ok) return false;
        bytes = new Uint8Array(await res.arrayBuffer());
      }
      if (!bytes.length) return false;
      Module.FS.writeFile(path, bytes);
      return true;
    } catch { return false; }
  }

  // `bytes` sert le régime octets ; `path` et `source` servent le régime fichier.
  async function feed(spec, bytes, label, path, source) {
    if (nodeAcceptsBytes(spec.node))
      return Module.graphSetBytes(spec.node, bytes, label);

    if (!(await placeFile(path, source))) {
      setStatus(`${label} : dépôt impossible en ${path}`);
      return false;
    }
    return Module.graphSetString(spec.node, "path", path);
  }

  // Charge une ressource du catalogue par son URL et la pousse dans le nœud.
  // Une ressource SERVIE : elle est déposée au chemin qu'elle porte déjà, si
  // bien que le document enregistré la nomme d'une adresse qui vaut ailleurs.
  async function feedUrl(spec, url) {
    if (!nodeAcceptsBytes(spec.node))
      return feed(spec, null, url.split("/").pop(), url, url);

    const res = await fetch(url, { cache: "force-cache" });
    if (!res.ok) { setStatus(`${url} introuvable`); return false; }
    const bytes = new Uint8Array(await res.arrayBuffer());
    return feed(spec, bytes, url.split("/").pop(), url, url);
  }

  // CATALOGUE d'une source : une liste de ressources prêtes, servie à côté du
  // choix d'un fichier à soi. Sa STRUCTURE est décrite par le gabarit — noms des
  // clés du manifeste — et non connue d'ici : un futur catalogue de presets
  // d'images, aux clés différentes, marchera sans toucher à ce fichier.
  //
  // Tolérant à l'échec de bout en bout : le manifeste peut manquer parce que le
  // build n'a pas tourné, ou parce que le serveur ne sert pas data/. Le
  // sélecteur disparaît alors et l'import de fichier reste — la page demeure
  // utilisable.
  async function buildCatalogue(spec, wrap) {
    const c = spec.catalogue;
    if (!c || !c.url) return null;

    let manifest;
    try {
      // `no-store` sur le MANIFESTE seul : il change à chaque build, alors que
      // les ressources qu'il liste gagnent à rester dans le cache du navigateur
      // — resélectionner une police de 2 Mo ne doit pas la retélécharger.
      const res = await fetch(c.url, { cache: "no-store" });
      if (!res.ok) return null;
      manifest = await res.json();
    } catch { return null; }

    const groupsKey = c.groupsKey || "groups";
    const itemsKey  = c.itemsKey  || "items";
    const fileKey   = c.fileKey   || "file";
    const labelKey  = c.labelKey  || "label";
    const noteKey   = c.noteKey   || "note";
    const dir       = c.dir ? c.dir.replace(/\/+$/, "") + "/" : "";

    const select = document.createElement("select");
    const notes = new Map();
    let count = 0;

    const addItem = (item, parent) => {
      const file = item[fileKey];
      if (!file) return;
      const option = document.createElement("option");
      option.value = dir + file;
      option.textContent = item[labelKey] || file;
      if (item[noteKey]) { option.title = item[noteKey]; notes.set(option.value, item[noteKey]); }
      parent.appendChild(option);
      ++count;
    };

    for (const group of manifest[groupsKey] || []) {
      const holder = document.createElement("optgroup");
      holder.label = group[labelKey] || "";
      for (const item of group[itemsKey] || []) addItem(item, holder);
      select.appendChild(holder);
    }
    // Manifeste plat : les entrées peuvent aussi vivre à la racine.
    for (const item of manifest[itemsKey] || []) addItem(item, select);

    if (count === 0) return null;

    const note = document.createElement("p");
    note.className = "hint-sm";

    const label = document.createElement("label");
    label.textContent = c.label || "Catalogue";
    wrap.appendChild(label);
    wrap.appendChild(select);
    wrap.appendChild(note);

    select.addEventListener("change", async () => {
      note.textContent = notes.get(select.value) || "";
      setStatus(`chargement de ${select.value.split("/").pop()}…`);
      refit = true;   // autre ressource, autre échelle : la vue se recadre
      if (await feedUrl(spec, select.value)) evaluate();
    });

    // Sélection initiale : le `default` du gabarit s'il désigne une entrée du
    // catalogue, sinon celui que le manifeste déclare, sinon la première.
    const declared = manifest[c.defaultKey || "default"];
    const wanted = [spec.default, declared ? dir + declared : null]
      .find((v) => v && [...select.options].some((o) => o.value === v));
    if (wanted) select.value = wanted;
    note.textContent = notes.get(select.value) || "";
    return select;
  }

  for (const spec of tpl.sources || []) {
    const wrap = document.createElement("div");
    wrap.className = "param source";

    const catalogue = await buildCatalogue(spec, wrap);

    const label = document.createElement("label");
    label.textContent = catalogue
      ? (spec.ownLabel || "…ou un fichier à toi")
      : (spec.label || `source #${spec.node}`);
    if (spec.hint) label.title = spec.hint;
    const input = document.createElement("input");
    input.type = "file";
    if (spec.accept) input.accept = spec.accept;
    input.addEventListener("change", async () => {
      const file = input.files[0];
      if (!file) return;
      setStatus(`chargement de ${file.name}…`);
      refit = true;
      // Pas d'adresse durable pour un fichier choisi à la souris : /tmp. Le
      // document ne le retrouvera pas au chargement suivant, mais il le NOMME —
      // ce qui vaut mieux qu'une ressource muette.
      const bytes = nodeAcceptsBytes(spec.node)
        ? new Uint8Array(await file.arrayBuffer())
        : null;
      if (await feed(spec, bytes, file.name, "/tmp/" + file.name, file)) evaluate();
    });
    wrap.appendChild(label);
    wrap.appendChild(input);
    sources.appendChild(wrap);

    // Ressource de départ : la page ouvre sur un objet, pas sur un viseur vide.
    // Le catalogue a le dernier mot quand il existe, sa sélection initiale étant
    // déjà arbitrée ci-dessus.
    const initial = catalogue ? catalogue.value : spec.default;
    if (initial) {
      try { await feedUrl(spec, initial); }
      catch { /* page utilisable sans ressource de départ */ }
    }
  }

  // ---- export ------------------------------------------------------------
  el("downloadObjBtn").addEventListener("click", () => {
    if (!lastObj) return;
    saveBlob(new Blob([lastObj], { type: "text/plain" }),
             safeName(el("filename").value || name, ".obj"));
  });

  // ---- lien vers l'éditeur ----------------------------------------------
  //
  // Le graphe de cette page est un DOCUMENT ordinaire : il s'ouvre dans
  // l'éditeur nodal, où il se modifie. C'est tout l'intérêt d'avoir templaté la
  // page plutôt que d'avoir écrit une seconde fois la chaîne en C++.
  const openInEditor = el("openInEditor");
  if (openInEditor) {
    openInEditor.addEventListener("click", () => {
      const doc = Module.graphToJson();
      saveBlob(new Blob([doc], { type: "application/json" }), safeName(name, ".json"));
      setStatus("document enregistré — ouvre-le dans l'éditeur nodal (graph.html)");
    });
  }

  if (warnings.length) setStatus(warnings[0]);
  evaluate();
}
