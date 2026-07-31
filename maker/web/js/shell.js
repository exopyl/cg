// ===========================================================================
//  Chrome commun aux pages de generation : panneau, viewer, cycle de vie de la
//  forme courante.
// ===========================================================================
//  Chaque page (formes / gothique / relief / SVG) ne declare dans son HTML que
//  ses propres controles, dans <div id="pageControls">. Le shell injecte tout le
//  reste AUTOUR : en-tete, banniere, panneau de parametres, section Vue, footer
//  d'export. Sans bundler ni templating, c'est ce qui evite de dupliquer 40
//  lignes de HTML dans quatre fichiers.
// ===========================================================================

import { loadModule } from "./wasm.js";
import { createBanner, createFileInputs } from "./ui.js";
import { createViewer } from "./viewer.js";
import { buildPanel } from "./panel.js";
import { downloadObj, downloadStl } from "./exporters.js";

const el = (id) => document.getElementById(id);

// Cree le shell : charge le module WASM, monte le panneau, initialise le viewer.
// Renvoie un contexte imperatif que la page utilise pour ses propres controles.
export async function createShell({ title, subtitle }) {
  const panel = el("panel");
  const pageControls = el("pageControls");

  // --- injection du chrome -------------------------------------------------
  const header = document.createElement("header");
  header.innerHTML =
    `<a class="back" href="./index.html">← maker</a>` +
    `<h1></h1><p class="sub"></p>`;
  header.querySelector("h1").textContent = title;
  header.querySelector(".sub").textContent = subtitle;

  const banner = document.createElement("div");
  banner.className = "banner";
  banner.hidden = true;

  const paramsSection = document.createElement("section");
  paramsSection.className = "group";
  paramsSection.innerHTML = `<div class="lbl">Paramètres</div><div id="params"></div>`;

  const viewSection = document.createElement("section");
  viewSection.className = "group";
  viewSection.innerHTML = `
    <div class="lbl">Vue</div>
    <div class="ctrlrow"><span>Couleur du modèle</span><input type="color" id="modelColor" value="#b4bec8"></div>
    <div class="ctrlrow"><span>Couleur du fond</span><input type="color" id="bgColor" value="#14161a"></div>
    <div class="ctrlrow"><label for="wireframe">Fil de fer</label><input type="checkbox" id="wireframe"></div>
    <button id="resetView">Recentrer la vue</button>`;

  const footer = document.createElement("footer");
  footer.innerHTML = `
    <div id="stats" class="stats">—</div>
    <div class="lbl">Export</div>
    <input type="text" id="filename" value="maker" placeholder="nom du fichier" spellcheck="false">
    <div class="btnrow">
      <button id="downloadObjBtn" disabled>OBJ</button>
      <button id="downloadStlBtn" disabled>STL</button>
    </div>
    <div id="footerExtras"></div>`;

  // En-tete et banniere AVANT les controles de la page, parametres / vue / export
  // APRES : l'ordre visuel du panneau est fixe par le shell, pas par chaque page.
  panel.prepend(banner);
  panel.prepend(header);
  pageControls.after(paramsSection);
  paramsSection.after(viewSection);
  panel.append(footer);

  // --- references ---------------------------------------------------------
  const paramsEl        = el("params");
  const statsEl         = el("stats");
  const filenameInput   = el("filename");
  const modelColorInput = el("modelColor");
  const bgColorInput    = el("bgColor");
  const wireframeInput  = el("wireframe");
  const hintEl          = el("viewerHint");
  const exportBtns      = [el("downloadObjBtn"), el("downloadStlBtn")];

  const setStatus = (text) => { statsEl.textContent = text; };
  const bnr   = createBanner(banner, setStatus);
  const files = createFileInputs(bnr);

  // --- etat ---------------------------------------------------------------
  let Module = null;
  let currentId = -1;   // id de l'objet cgmesh courant

  // --- viewer -------------------------------------------------------------
  const viewer = createViewer({
    container: el("viewer"),
    hintEl,
    getModelColor: () => modelColorInput.value,
    getWireframe:  () => wireframeInput.checked,
    // Grise le picker quand il ne peut rien changer (maillage multi-couleurs).
    onVertexColors: (has) => {
      modelColorInput.disabled = has;
      modelColorInput.title = has
        ? "Le modèle porte ses propres couleurs (une par région)"
        : "";
    },
    // o3dv importe l'OBJ en SOUDANT les sommets coincidents (normales lissees a
    // travers les aretes vives = faces plates facettees). On rebascule aussitot
    // sur la geometrie meshData, non soudee.
    onModelReady: () => { if (currentId >= 0) showStats(viewer.updateInPlace(Module, currentId, true)); },
  });

  if (!viewer.available) {
    bnr.reportHtml(
      "Online3DViewer introuvable. Place <code>o3dv.min.js</code> dans " +
      "<code>maker/web/vendor/</code> pour activer le rendu 3D. " +
      "Le panneau et la génération restent fonctionnels (télécharge l'OBJ).");
  }

  function showStats(s) {
    if (s) statsEl.textContent = `${s.nv} sommets · ${s.nf} faces · ${s.ms.toFixed(1)} ms`;
  }

  // --------------------------------------------------------------------------
  // Mise a jour pendant l'edition : coalescee sur requestAnimationFrame (au plus
  // une par frame). En place si le mesh est deja capture, sinon amorcage.
  // --------------------------------------------------------------------------
  let rafPending = false, rafFallback = 0;
  function scheduleUpdate() {
    if (rafPending) return;
    rafPending = true;

    const run = () => {
      if (!rafPending) return;        // deja execute par l'autre voie
      rafPending = false;
      clearTimeout(rafFallback);
      if (currentId < 0) return;
      if (viewer.isReady()) showStats(viewer.updateInPlace(Module, currentId, false)); // drag : pas de recadrage
      else firstRender(currentId);
    };

    requestAnimationFrame(run);
    // Filet de securite : rAF ne se declenche PAS dans un onglet masque ou
    // occulte. Sans lui, un seul rAF avale son tour, rafPending reste true pour
    // toujours et TOUTE mise a jour ulterieure est silencieusement ignoree -- l'UI
    // repond aux sliders mais le modele ne bouge plus jamais.
    rafFallback = setTimeout(run, 250);
  }

  // Amorcage (premier affichage, ou import) : reload complet via o3dv, qui
  // recadre la camera et permet a captureMesh() de recuperer le mesh three.js +
  // ses constructeurs. Ensuite tout passe par updateInPlace().
  function firstRender(id) {
    const obj = Module.regenerate(id);
    const v = (obj.match(/^v /gm) || []).length;
    const f = (obj.match(/^f /gm) || []).length;
    statsEl.textContent = `${v} sommets · ${f} faces`;
    viewer.bootstrap(obj);
  }

  // Affiche la forme `id` : en place (avec recadrage anime) si le viewer est deja
  // amorce -> aucun flicker au changement de forme ; sinon amorcage initial.
  //
  // bootstrap : imports (JSON/SVG/image) -> reload o3dv complet, qui recadre la
  // camera de facon fiable meme sur un gros saut d'echelle (le refit anime de
  // updateInPlace ne fit pas correctement cube~1 -> fenetre~500).
  function applyShape(id, bootstrap) {
    if (!bootstrap && viewer.isReady()) showStats(viewer.updateInPlace(Module, id, true));
    else firstRender(id);
  }

  // --- chargement du module ----------------------------------------------
  const loaded = await loadModule();
  Module = loaded.Module;
  // Un module obsolete est plus actionnable que l'avis o3dv : il a le dernier mot.
  if (loaded.staleMessage) bnr.report(loaded.staleMessage);
  bnr.seal();

  // --- cablage des controles communs -------------------------------------
  modelColorInput.addEventListener("input", viewer.applyModelColor);
  bgColorInput.addEventListener("input", () => viewer.applyBackground(bgColorInput.value));
  wireframeInput.addEventListener("change", viewer.applyWireframe);
  el("resetView").addEventListener("click", viewer.resetView);

  const withShape = (fn) => () => { if (currentId >= 0) fn(Module, currentId, filenameInput.value); };
  el("downloadObjBtn").addEventListener("click", withShape(downloadObj));
  el("downloadStlBtn").addEventListener("click", withShape(downloadStl));

  // Deep-link des parametres : toute cle de la query string dont le nom correspond
  // a un parametre de la forme est appliquee via setParam (ex.
  // ?shape=Gothic%20Window&Rosette%20foil%20count=18&Offset%20outer=40). Utile pour
  // les captures automatisees.
  //
  // N'ecrit QUE les valeurs -- pas de reconstruction de panneau ni de rendu. C'est
  // setShape qui enchaine, de sorte que la forme est affichee UNE seule fois, deja
  // aux valeurs demandees : la version precedente rendait d'abord les valeurs par
  // defaut puis re-declenchait un chargement o3dv par-dessus, et le recadrage
  // camera du second chargement partait de la geometrie du premier.
  function applyQueryParams(id) {
    const qs = new URLSearchParams(location.search);
    if (![...qs.keys()].length) return;
    const names = new Set(JSON.parse(Module.getParams(id)).map((p) => p.name));
    for (const [k, v] of qs.entries()) {
      if (k === "shape" || !names.has(k)) continue;
      Module.setParam(id, k, Number(v));
    }
  }

  // ------------------------------------------------------------------------
  // Contexte rendu a la page.
  // ------------------------------------------------------------------------
  const ctx = {
    Module,
    viewer,
    banner: bnr,
    setStatus,
    footerExtras: el("footerExtras"),

    currentId: () => currentId,

    // Libere la forme courante. Les pages l'appellent AVANT de creer la suivante :
    // sur un relief d'image, garder les deux en memoire le temps de la generation
    // doublerait la pointe d'occupation du tas.
    destroyCurrent() {
      if (currentId !== -1) { Module.destroyShape(currentId); currentId = -1; }
    },

    // Adopte la forme `id` : nom de fichier suggere, panneau reconstruit,
    // affichage, boutons d'export actifs.
    //
    //   bootstrap : force le rechargement complet o3dv (imports de fichier)
    //   deepLink  : applique d'abord les parametres de la query string
    setShape(id, name, { bootstrap = false, deepLink = false } = {}) {
      if (currentId !== -1 && currentId !== id) Module.destroyShape(currentId);
      currentId = id;
      filenameInput.value = name;
      if (deepLink) applyQueryParams(id);
      buildPanel(Module, paramsEl, id, scheduleUpdate);
      for (const b of exportBtns) b.disabled = false;
      applyShape(id, bootstrap);
    },

    registerFileInput(input, handler) { files.register(input, handler); },
  };

  return ctx;
}

// Enveloppe l'amorcage d'une page : toute exception part dans la zone d'attente
// du viewer au lieu de finir en unhandled rejection invisible.
export function runPage(main) {
  main().catch((e) => {
    console.error(e);
    const hint = el("viewerHint");
    if (hint) { hint.hidden = false; hint.textContent = "Erreur : " + e.message; }
  });
}
