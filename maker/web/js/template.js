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
import { saveBlob, safeName, downloadStlFromGraph, meshExtent } from "./exporters.js";

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

// MESURES du dernier calcul d'un nœud (Node::PublishStats). Ce ne sont pas des
// paramètres : rien ne les règle, elles se constatent — d'où une section à part
// dans graphNodeInfo.
function readStats(Module, node) {
  const info = JSON.parse(Module.graphNodeInfo(node));
  return info && info.stats ? info.stats : {};
}

// Les premières mesures étaient toutes des COMPTES — des morceaux, des déliés
// mangés — et s'écrivaient telles quelles. L'entraxe de la fixation est une
// LONGUEUR : brute, elle s'afficherait « 92.36000061035156 », ce qui est exact
// et illisible. Un entier reste un entier ; le reste est arrondi au dixième de
// millimètre, qui est déjà plus fin que ce qu'une perceuse tient.
function fmtStat(value) {
  return Number.isInteger(value)
    ? String(value)
    : value.toLocaleString("fr-FR", { maximumFractionDigits: 1 });
}

// Un avertissement du gabarit est vrai QUAND la branche qu'il surveille est
// celle qui sert. Sans cette garde, la page crierait « ta silhouette est en
// morceaux » alors que le socle choisi est rectangulaire — un avertissement
// exact et hors sujet, donc un avertissement qu'on apprend à ignorer.
function watchApplies(Module, spec) {
  if (!spec.onlyIf) return true;
  // Une condition, ou plusieurs qui doivent TOUTES tenir : la silhouette ne se
  // signale que si le socle est demande ET que la forme choisie est bien elle.
  const conditions = Array.isArray(spec.onlyIf) ? spec.onlyIf : [spec.onlyIf];
  return conditions.every((c) =>
    Number(readParam(Module, { node: c.node, param: c.param })) === Number(c.equals));
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
const SETTER_TYPE = {
  int: "int", enum: "int", float: "float", bool: "bool",
  string: "string",
  // La couleur est une chaîne côté graphe : c'est le type qui la rend lisible
  // dans le document, et le seul dont ParamSet dispose pour « #rrggbb ».
  color: "string",
};

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
  } else if (spec.type === "enum" && (spec.choices || []).length <= 3) {
    // `values` : la valeur écrite pour la position i, quand elle n'est pas i.
    // C'est ce qui permet à une page de N'OFFRIR QU'UNE PARTIE d'une
    // énumération — sauter une position que le nœud accepte mais qui ne peut
    // rien produire d'utile — sans toucher au C++, où la retirer casserait les
    // documents qui la portent. Un document qui l'utilise reste lisible : aucune
    // position n'est alors marquée, et c'est vrai.
    const values = spec.values || (spec.choices || []).map((_, i) => i);
    // SEGMENTÉ pour deux ou trois positions. Un menu déroulant cache ce qu'on
    // POURRAIT choisir et demande deux clics ; côte à côte, les positions se
    // lisent et se changent d'un seul. Au-delà de trois, le menu reprend
    // l'avantage — c'est pourquoi le seuil est ici et pas dans le gabarit.
    const seg = document.createElement("div");
    seg.className = "seg";
    const buttons = [];
    const select = (index) => {
      buttons.forEach((b, i) => b.setAttribute("aria-pressed", i === index ? "true" : "false"));
      writeParam(Module, spec, values[index]);
      onChange();
    };
    (spec.choices || []).forEach((choice, i) => {
      const b = document.createElement("button");
      b.type = "button";
      b.textContent = choice;
      // Le libellé complet en infobulle : une position peut être tronquée,
      // l'ellipse ne doit pas emporter le sens avec elle.
      b.title = choice;
      b.setAttribute("aria-pressed", values[i] === (initial | 0) ? "true" : "false");
      b.addEventListener("click", () => select(i));
      buttons.push(b);
      seg.appendChild(b);
    });
    row.appendChild(label);
    wrap.appendChild(row);
    wrap.appendChild(seg);
    return wrap;
  } else if (spec.type === "enum") {
    const values = spec.values || (spec.choices || []).map((_, i) => i);
    input = document.createElement("select");
    (spec.choices || []).forEach((choice, i) => {
      const option = document.createElement("option");
      option.value = String(values[i]);
      option.textContent = choice;
      input.appendChild(option);
    });
    input.value = String(initial | 0);
    input.addEventListener("change", () => {
      writeParam(Module, spec, Number(input.value));
      onChange();
    });
  } else if (spec.type === "color") {
    // La couleur s'écrit « #rrggbb » — le format de <input type="color"> — et
    // part dans un paramètre de type CHAÎNE. Un entier aurait fait la même
    // chose, mais le document enregistré ne se lirait plus : « #b4bec8 » se
    // reconnaît, « 11845832 » se décode.
    input = document.createElement("input");
    input.type = "color";
    input.value = String(initial);
    input.addEventListener("input", () => {
      writeParam(Module, spec, input.value);
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
    // Une zone MULTILIGNE prend toute la largeur, sous son libellé : à côté de
    // lui elle n'a plus la place d'afficher la ligne qu'on y tape, ce qui est
    // tout ce qu'on lui demande.
    if (spec.multiline) {
      row.appendChild(label);
      wrap.appendChild(row);
      wrap.appendChild(input);
      return wrap;
    }
  } else {
    input = document.createElement("input");
    input.type = "range";
    input.min = spec.min !== undefined ? spec.min : 0;
    input.max = spec.max !== undefined ? spec.max : 1;
    input.step = spec.step !== undefined ? spec.step : 0.01;
    input.value = initial;
    const readout = document.createElement("span");
    readout.className = "readout";
    // La précision de l'AFFICHAGE suit celle du PAS : à 0,5 de pas, « 30.000 »
    // annonce trois décimales que le curseur ne sait pas atteindre. Les zéros de
    // queue tombent ensuite, pour que 30 se lise « 30 » et 0,05 « 0.05 ».
    const step = String(spec.step !== undefined ? spec.step : 0.01);
    const dot = step.indexOf(".");
    const decimals = dot < 0 ? 0 : step.length - dot - 1;
    const show = (v) => {
      if (spec.type === "int") return String(Math.round(v));
      const fixed = Number(v).toFixed(decimals);
      return decimals > 0 ? fixed.replace(/\.?0+$/, "") : fixed;
    };
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

// Cote en millimetres : deux decimales au plus, et pas de zeros inutiles --
// « 124,98 » et « 30 », pas « 124,9800 » ni « 30,00 ».
function fmtMm(v) {
  return (Math.round(v * 100) / 100).toLocaleString("fr-FR", { maximumFractionDigits: 2 });
}

export async function runTemplate(name) {
  const status = el("status");
  const setStatus = (t) => { if (status) status.textContent = t; };
  // Optionnels : un gabarit qui ne montre ni cotes ni avertissements n'a pas ces
  // éléments. Deux notions de « warning », à ne pas confondre : le tableau
  // `warnings` ci-dessus porte les fautes de GABARIT, relevées une fois au
  // chargement ; `warnBox` porte ce que le dernier CALCUL a coûté, et change à
  // chaque réglage.
  const dims = el("dims");
  const warnBox = el("warnings");
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

  // POIGNÉE DE DIAGNOSTIC. Cette page est un module ES : sans elle, la console
  // du navigateur n'atteint ni le module WASM ni le gabarit chargé, et vérifier
  // ce que la page vient de produire — sa palette, les mesures d'un nœud —
  // obligerait à instancier un SECOND module, donc à mesurer autre chose que ce
  // qui est à l'écran. Lecture seule par usage, pas par contrat.
  window.maker = { Module, template: tpl, stats: (node) => readStats(Module, node) };

  // ---- viewer ------------------------------------------------------------
  // OPTIONNELLE désormais : quand le document porte lui-même sa couleur
  // (nœud `mesh.color`), la page n'a plus de pastille à offrir — la couleur
  // n'est plus un réglage de vue. template.html en garde une ; text.html non.
  const modelColor = el("modelColor");
  const wireframe = el("wireframe");
  const viewer = createViewer({
    container: el("viewer"),
    hintEl: el("hint"),
    // Sans pastille, la teinte de repli ne sert qu'aux maillages qui ne portent
    // aucun matériau — ceux qui en portent un l'emportent de toute façon.
    getModelColor: () => (modelColor ? modelColor.value : "#b4bec8"),
    getWireframe: () => wireframe.checked,
    // Grise le sélecteur de couleur quand le maillage porte les siennes.
    onVertexColors: (has) => {
      if (!modelColor) return;
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
  if (modelColor) modelColor.addEventListener("input", viewer.applyModelColor);
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
        // NOMMER LE NŒUD FAUTIF. `graphEvaluate` rend déjà son identifiant —
        // l'évaluateur le porte pour cette raison — mais la page n'en faisait
        // rien, si bien qu'un refus parfaitement localisé s'affichait
        // « compute-failed », c'est-à-dire rien. Et quand le gabarit sait ce que
        // ce nœud reproche d'ordinaire, il le dit : un cul-de-sac devient un
        // conseil.
        let who = "";
        try {
          const info = JSON.parse(Module.graphNodeInfo(result.node));
          if (info && info.type) who = ` · ${info.type}`;
        } catch { /* le nœud a pu disparaître : le statut seul reste vrai */ }
        const hint = (tpl.failureHints || {})[String(result.node)];
        setStatus(`calcul refusé${who}${result.detail ? " — " + result.detail : ""}`);
        if (warnBox) {
          warnBox.textContent = hint || "";
          warnBox.hidden = !hint;
        }
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
      // UN SEUL appel a graphMeshData, consomme deux fois : les cotes le lisent,
      // le viewer le recoit. La charge utile est une VUE TYPEE sur le tas WASM,
      // valide jusqu'au prochain appel au Module -- d'ou l'ordre strict ici, et
      // le `() => payload` passe au viewer plutot qu'un second appel.
      const payload = Module.graphMeshData(0);
      const extent = meshExtent(payload.positions);

      if (!viewer.isReady()) viewer.bootstrap(lastObj);
      else viewer.updateInPlace(() => payload, 0, refit);
      refit = false;

      setStatus(`${result.nv} sommets · ${result.nf} triangles · ${result.ms} ms`);
      // COTES : ce que l'utilisateur regarde avant d'imprimer, et la seule
      // verification que le reglage veut bien dire ce qu'il annonce. Les unites
      // monde du document SONT des millimetres (cf. le gabarit).
      if (dims)
        dims.textContent = extent
          ? `${fmtMm(extent.x)} × ${fmtMm(extent.y)} × ${fmtMm(extent.z)} mm`
          : "—";

      // AVERTISSEMENTS. Plusieurs nœuds comptent ce que le réglage a coûté — les
      // morceaux d'une silhouette qui ne s'est pas refermée, les déliés qu'une
      // arête a mangés. Ces compteurs ne servaient à rien tant qu'ils
      // n'atteignaient pas l'écran : un garde-fou invisible n'en est pas un.
      if (warnBox) {
        const said = [];
        for (const spec of tpl.watch || []) {
          if (!watchApplies(Module, spec)) continue;
          // SANS `stat`, la garde `onlyIf` suffit : l'avertissement ne porte
          // pas sur ce que le calcul a coûté mais sur un RÉGLAGE qui est un
          // piège en lui-même — un renfort qui avale les lettres, par exemple.
          // Rien à mesurer, tout à dire.
          if (!spec.stat) { said.push(String(spec.message)); continue; }
          const value = readStats(Module, spec.node)[spec.stat];
          if (typeof value !== "number") continue;
          if (value > (spec.warnAbove || 0))
            said.push(String(spec.message).replace("{value}", fmtStat(value)));
        }
        warnBox.textContent = said.join(" · ");
        warnBox.hidden = said.length === 0;
      }

      applyGates();
    });
  }

  // RÉGLAGES QUE LE RÉSULTAT REND INOPÉRANTS. Un widget qui n'a plus d'effet
  // n'est pas une faute de gabarit — c'est une conséquence de ce que le calcul a
  // produit, et elle change d'un fichier à l'autre. Il ne peut donc se lire que
  // sur une MESURE publiée par un nœud (Node::PublishStats), jamais sur une
  // convention recopiée dans le gabarit, qui serait vraie le jour où on l'écrit
  // et fausse ensuite.
  //
  // Grisé et NON masqué : un réglage qui disparaît laisse croire qu'il n'existe
  // pas. L'infobulle porte alors la raison, sans quoi le gris est une énigme.
  const gated = [];

  function applyGates() {
    for (const gate of gated) {
      const value = readStats(Module, gate.rule.node)[gate.rule.stat];
      const off = typeof value === "number" && value === Number(gate.rule.equals);
      gate.wrap.classList.toggle("off", off);
      for (const control of gate.wrap.querySelectorAll("input, select, textarea, button"))
        control.disabled = off;
      const label = gate.wrap.querySelector("label");
      if (label) label.title = off ? (gate.rule.reason || "") : gate.hint;
    }
  }

  // ---- panneau -----------------------------------------------------------
  const params = el("params");
  params.innerHTML = "";
  // BLOCS. Un intertitre ne suffisait pas : il flottait dans la même colonne que
  // les réglages et se confondait avec les libellés de section de la page. Un
  // bloc encadré et REPLIABLE fait deux choses de plus — il borne visiblement ce
  // qui va ensemble, et il permet de ranger ce qu'on ne touche qu'une fois.
  //
  // TOUS REPLIÉS d'avance. Le panneau s'ouvre alors sur une TABLE DES MATIÈRES
  // — cinq ou six en-têtes qu'on lit d'un coup d'œil — au lieu d'une colonne de
  // vingt-cinq réglages qu'il faut parcourir pour savoir ce qu'elle contient. On
  // déplie ce qu'on vient régler.
  //
  // `tpl.groups` est optionnel : un groupe y demande `"open": true` pour faire
  // exception. Un gabarit qui ne déclare rien les replie tous, et un gabarit qui
  // ne groupe rien du tout retombe sur la liste simple.
  const groupMeta = new Map();
  for (const g of tpl.groups || []) groupMeta.set(g.name, g);

  let currentGroup = null;
  let body = params;
  for (const spec of tpl.expose || []) {
    if (spec.group && spec.group !== currentGroup) {
      currentGroup = spec.group;
      const meta = groupMeta.get(currentGroup) || {};
      const block = document.createElement("details");
      block.className = "block";
      block.open = meta.open === true;
      const summary = document.createElement("summary");
      summary.textContent = currentGroup;
      if (meta.hint) summary.title = meta.hint;
      block.appendChild(summary);
      body = document.createElement("div");
      body.className = "body";
      block.appendChild(body);
      params.appendChild(block);
    }
    const widget = buildWidget(Module, spec, evaluate, warn);
    if (spec.disableWhen)
      gated.push({ rule: spec.disableWhen, wrap: widget, hint: spec.hint || "" });
    body.appendChild(widget);
  }

  // ---- sources de fichier ------------------------------------------------
  //
  // Une source par entrée d'octets déclarée. Les octets vont DIRECTEMENT dans le
  // nœud (graphSetBytes) : pas de détour par MEMFS, donc pas de fichier
  // temporaire à garder vivant pour la durée de la page.
  const sources = el("sources");
  sources.innerHTML = "";
  // Le titre du bloc appartient au DOCUMENT : « Police » pour ce gabarit,
  // « Image » pour un autre. Sans lui, la page annonce « Sources », qui ne dit
  // rien de ce qu'on y dépose.
  const sourcesTitle = el("sourcesTitle");
  if (sourcesTitle) sourcesTitle.textContent = tpl.sourcesGroup || "Sources";

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

  // STL BINAIRE -- le format de l'impression 3D, et la raison d'etre de cette
  // page. Optionnel dans le gabarit : un relief colore n'a rien a faire d'un
  // format qui ne porte pas la couleur, donc le bouton n'existe que la ou il a
  // un sens.
  const stlBtn = el("downloadStlBtn");
  if (stlBtn) {
    stlBtn.addEventListener("click", () => {
      if (!downloadStlFromGraph(Module, 0, el("filename").value || name))
        setStatus("rien a exporter — le nœud de sortie n'a pas rendu de maillage");
    });
  }

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
