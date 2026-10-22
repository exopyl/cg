// ===========================================================================
//  Sonde : page microarch.html (microarchitecture gothique), dans un vrai
//  navigateur, et absence de la microarchitecture dans gothic.html.
// ===========================================================================
//
//   python maker/probes/run.py --page microarch_page.html
//
// Oracle : la ligne d'etat du shell (« N sommets · M faces », ecrite apres
// meshData), la banniere, l'etat des boutons et les Blobs telecharges -- pas des pixels.
//
//   control : microarch.html sans import. Titre de la page, pas de catalogue, invitation a
//             importer dans le viseur, boutons inactifs, ligne d'etat « — ».
//   ready   : premier import D4 segment, viseur jamais amorce : regenerate rend "", une
//             mutation de la ligne d'etat ecrit « — » (amorce d'un triangle, M40), puis
//             2 176 faces. Exports OBJ, GLB et STL du pied de page refuses en
//             banniere, aucun Blob. Second import : en place, sans « — ». Fichier de
//             4 Mio + 1 : json_too_large avant lecture, microarchitecture gardee. Import
//             refuse (M42) : etat garde (boutons, solide, detail), le modele reste affiche,
//             « Tube sides » le remaille. Nom « a b/é » : ab_network.json (le JSON du reseau
//             d'une instance de reference), ab_curves.obj (lignes l, aucune f),
//             ab_tubes.obj (lignes f).
//   cold    : second import des que le premier a lance l'amorcage o3dv (« — ») : un seul
//             chargement o3dv a la fois, la derniere forme s'affiche quand meme. Puis une
//             specification a profil non monotone : « chargée avec avertissements ».
//   empty   : shapes.html sur un L-system vide (systeme 5, iteration 0 ; M37, M40). L'OBJ
//             d'amorcage n'a pas de face : amorce d'un triangle (« — »), puis meshData
//             (« 0 sommets · 0 faces · … ms ») ; GLB -> banniere « rien à exporter », aucun
//             Blob ; iteration 2 -> des faces, en place.
//   gothic  : gothic.html et js/pages/gothic.js (textes servis) sans aucune trace de la
//             microarchitecture ; la page chargee n'a aucun element microarch* ni solid*, et
//             ses champs d'imprimante sont remplis (js/printer_panel.js).
//   control : en plus, un premier import refuse n'amorce pas le viseur (invitation gardee) ;
//             un import refuse pendant l'amorce d'un triangle finit sur le modele (2 176 faces).
//   home    : index.html relie microarch.html.
//   printer : en dernier, sur microarch.html et gothic.html ouvertes APRES avoir pose
//             l'imprimante dans localStorage : les champs la refletent ; un change sur
//             pcMinWall de microarch.html met la cle a jour, et gothic.html la recoit par
//             l'evenement storage, sans reecrire la cle. Puis, ecrits par cette page : une
//             valeur refusee ({"schema":"x"}) -> « refusée » en banniere, champs inchanges ;
//             la cle retiree -> champs inchanges, aucune banniere.
//   solide (dans ready, J7-d) : bouton « Solide STL » actif apres import ; rayon 50 permil,
//           q = 3, 2 mm par unite -> banniere « Solide STL imprimable », un Blob STL de
//           84 + 50 x 47 632 octets ; q = 1 -> banniere « refusé », solid_invalid_parameter,
//           aucun Blob ; 0,001 mm par unite -> « NON imprimable, STL téléchargé quand même »,
//           echelle minimale donnee, un Blob ; bouton desactive apres un import refuse.
//           La ligne d'etat « construction et vérification… » doit etre lue par un rappel
//           d'observation AVANT la fin du calcul (peinte avant lui). Le telechargement est
//           intercepte (URL.createObjectURL et clic de l'ancre), pas ecrit sur disque.
//   Chaque attente est bornee a 60 s ; une attente depassee est nommee dans
//   report.timeouts et rend le verdict faux.
//
const D4 = `{
  "schema": "gothic-microarchitecture/v1",
  "meta": { "id": "d4-segment" },
  "symmetry": { "center": [0, 0], "rotation_order": 4, "reflection_axis_deg": 90 },
  "levels": [ { "id": 0, "base": 0 } ],
  "points": [
    { "id": "A", "xy": [4, 1], "ground": true },
    { "id": "B", "xy": [1, 5], "height": 10 }
  ],
  "curves": [ { "id": "l1", "type": "segment", "start": "A", "end": "B", "level": 0 } ]
}`;

// Meme segment, B a 2 de haut : k = 0,4 < 1, avertissement profile_not_monotonic
// (tu_cgarchi_gothic_micro_curve.cpp, oneSegment(symmetry(4, 90), 4, 1, 1, 5, 2)).
const D4_LOW = D4.replace('"height": 10', '"height": 2');

const report = { started: new Date().toISOString(), probes: {} };
const out = document.getElementById("out");
const say = (t) => (out.textContent += t + "\n");
const wait = (ms) => new Promise((r) => setTimeout(r, ms));

const WAIT_MAX_MS = 60000;
report.timeouts = [];

// Attend que fn rende une valeur vraie, au plus WAIT_MAX_MS ; sinon null, et l'attente
// `name` est relevee dans report.timeouts (verdict faux).
async function until(fn, name, step = 100) {
  const t0 = performance.now();
  while (performance.now() - t0 < WAIT_MAX_MS) {
    let v = null;
    try { v = fn(); } catch { /* page pas encore la */ }
    if (v) return v;
    await wait(step);
  }
  report.timeouts.push(name);
  return null;
}

function importSpec(frame, name, text = D4) {
  const doc = frame.contentDocument;
  const input = doc.getElementById("microarchJsonInput");
  const dt = new frame.contentWindow.DataTransfer();
  dt.items.add(new frame.contentWindow.File([text], name, { type: "application/json" }));
  input.files = dt.files;
  input.dispatchEvent(new frame.contentWindow.Event("change"));
}

// Historique des textes de la ligne d'etat, pour voir passer l'amorce « — ».
function watchStats(frame) {
  const stats = frame.contentDocument.getElementById("stats");
  const seen = [stats.textContent];
  new frame.contentWindow.MutationObserver(() => seen.push(stats.textContent))
    .observe(stats, { childList: true, characterData: true, subtree: true });
  return seen;
}

// Index du premier « — » ecrit par une mutation (seen[0] est le texte avant l'import) :
// l'amorce d'un triangle. setStatus(« chargement… ») et « — » sont ecrits dans la meme
// tache, l'observateur ne voit que le second ; -1 sinon.
function placeholderAfterLoad(seen) {
  return seen.indexOf("—", 1);
}

const banner = (doc) => doc.querySelector(".banner");
const bannerText = (doc) => (banner(doc).hidden ? "" : banner(doc).textContent);
const statsText = (doc) => doc.getElementById("stats").textContent;
const shows2176 = (doc) => (/2176 faces · .* ms$/.test(statsText(doc)) ? statsText(doc) : null);
const microButtonsEnabled = (doc) =>
  ["microarchNetworkBtn", "microarchCurvesBtn", "microarchTubesBtn"].map((b) => !doc.getElementById(b).disabled);
const solidEnabled = (doc) => !doc.getElementById("microarchSolidBtn").disabled;
const moduleLoaded = (doc) =>
  doc.getElementById("microarchJsonInput") && !/Chargement du module/.test(doc.getElementById("viewerHint").textContent);

// Releve les Blobs passes a URL.createObjectURL et neutralise le clic de l'ancre de
// telechargement ; rend la fonction qui retablit les deux.
function interceptBlobs(win, blobs, downloads = []) {
  const realUrl = win.URL.createObjectURL.bind(win.URL);
  const realClick = win.HTMLAnchorElement.prototype.click;
  win.URL.createObjectURL = (blob) => { blobs.push({ size: blob.size, type: blob.type, blob }); return realUrl(blob); };
  win.HTMLAnchorElement.prototype.click = function () {
    if (this.download) downloads.push(this.download);
    else realClick.call(this);
  };
  return () => {
    win.URL.createObjectURL = realUrl;
    win.HTMLAnchorElement.prototype.click = realClick;
  };
}

// Regle les champs du solide, clique « Solide STL », attend une nouvelle banniere du solide.
async function clickSolid(frame, fields) {
  const win = frame.contentWindow, doc = frame.contentDocument;
  for (const [id, value] of Object.entries(fields)) doc.getElementById(id).value = value;
  const before = bannerText(doc);
  const blobs = [];
  // Textes de la ligne d'etat lus a chaque rappel d'observation : sans attente avant le calcul,
  // le premier rappel ne survient qu'apres lui et ne voit que le texte final.
  const stats = doc.getElementById("stats");
  const states = [];
  const observer = new win.MutationObserver(() => states.push(stats.textContent));
  observer.observe(stats, { childList: true, characterData: true, subtree: true });
  const restore = interceptBlobs(win, blobs);
  try {
    doc.getElementById("microarchSolidBtn").click();
    const text = await until(() => {
      const t = bannerText(doc);
      return t !== before && /^Solide STL/.test(t) ? t : null;
    }, "solid banner");
    return { banner: text, blobs, states };
  } finally {
    observer.disconnect();
    restore();
  }
}

// Clique un bouton d'export du pied de page, banniere masquee avant : texte de la nouvelle
// banniere et nombre de Blobs produits.
async function clickFooterExport(frame, id) {
  const win = frame.contentWindow, doc = frame.contentDocument;
  banner(doc).hidden = true;
  banner(doc).textContent = "";
  const blobs = [];
  const restore = interceptBlobs(win, blobs);
  try {
    doc.getElementById(id).click();
    const text = await until(() => bannerText(doc) || null, `footer ${id}`);
    return { banner: text || "", blobs: blobs.length };
  } finally {
    restore();
  }
}

async function probeReady() {
  const frame = document.getElementById("ready");
  const doc = () => frame.contentDocument;
  const r = {};
  await until(() => moduleLoaded(doc()), "ready module");
  const seen = watchStats(frame);
  importSpec(frame, "d4.json");
  r.stats = await until(() => shows2176(doc()), "ready import");
  r.statsHistory = seen.slice(0, 12);
  r.placeholderAfterLoad = placeholderAfterLoad(seen) >= 0;
  r.hintHidden = doc().getElementById("viewerHint").hidden;
  r.buttonsAfterImport = microButtonsEnabled(doc());
  r.details = doc().getElementById("microarchDetails").textContent;

  r.footer = {};
  for (const id of ["downloadObjBtn", "downloadGlbBtn", "downloadStlBtn"]) r.footer[id] = await clickFooterExport(frame, id);

  r.solidEnabled = solidEnabled(doc());
  r.solid = await clickSolid(frame, { solidRadius: "50", solidCells: "3", solidScale: "2" });
  r.solidRefused = await clickSolid(frame, { solidRadius: "50", solidCells: "1", solidScale: "2" });
  r.solidThin = await clickSolid(frame, { solidRadius: "50", solidCells: "3", solidScale: "0.001" });
  r.solidEnabledAfter = solidEnabled(doc());

  // Second import, viseur amorce : meshData en place, sans amorce.
  const seen2 = watchStats(frame);
  importSpec(frame, "d4.json");
  await until(() => seen2.length > 1 && shows2176(doc()), "ready second import");
  r.secondHistory = seen2.slice(0, 12);
  r.secondInPlace = placeholderAfterLoad(seen2) < 0 && !!shows2176(doc());

  // Plus de 4 Mio : refus avant lecture, la microarchitecture courante reste.
  importSpec(frame, "big.json", " ".repeat(4 * 1024 * 1024 + 1));
  r.tooLarge = await until(() => (/json_too_large/.test(bannerText(doc())) ? bannerText(doc()) : null), "too large banner");
  r.buttonsAfterTooLarge = microButtonsEnabled(doc());

  // Import refuse (M42) : rien ne change hors de la banniere.
  importSpec(frame, "bad.json", "{}");
  r.refused = await until(() => (/refusée/.test(bannerText(doc())) ? bannerText(doc()) : null), "refused banner");
  r.buttonsAfterRefusal = microButtonsEnabled(doc());
  r.solidEnabledAfterRefusal = solidEnabled(doc());
  r.detailsAfterRefusal = doc().getElementById("microarchDetails").textContent;
  r.filenameAfterRefusal = doc().getElementById("filename").value;

  // Le modele garde reagit aux curseurs : 8 cotes au lieu de 6.
  const sides = sliderOf(doc(), "Tube sides");
  r.sidesInput = !!sides;
  if (sides) {
    sides.value = "8";
    sides.dispatchEvent(new frame.contentWindow.Event("input", { bubbles: true }));
    r.afterSides = await until(() => {
      const m = statsText(doc()).match(/^([0-9]+) sommets · ([0-9]+) faces · .* ms$/);
      return m && m[2] !== "2176" && m[2] !== "0" ? statsText(doc()) : null;
    }, "sides after refusal");
  }

  // Exports du panneau, nom filtre comme cote C++.
  doc().getElementById("filename").value = "a b/é";
  r.exports = {};
  for (const [id, kind] of [["microarchNetworkBtn", "network"], ["microarchCurvesBtn", "curves"],
                            ["microarchTubesBtn", "tubes"]]) {
    const blobs = [], downloads = [];
    const restore = interceptBlobs(frame.contentWindow, blobs, downloads);
    try {
      doc().getElementById(id).click();
      await until(() => downloads.length > 0, `export ${kind}`);
    } finally {
      restore();
    }
    r.exports[kind] = { downloads, count: blobs.length, text: blobs.length ? await blobs[0].blob.text() : "" };
  }
  return r;
}

// Curseur du panneau (panel.js) : <span class="name">nom</span> dans la ligne qui le precede.
function sliderOf(doc, label) {
  const name = [...doc.querySelectorAll("span.name")].find((n) => n.textContent === label);
  return name && name.parentElement.parentElement.querySelector('input[type="range"]');
}

// JSON du reseau de D4 par une instance du module propre a la sonde : l'oracle du Blob.
async function referenceNetwork() {
  const { default: createMakerModule } = await import("../web/maker.js");
  const M = await createMakerModule();
  const res = M.createMicroarchFromJson(D4);
  const network = res.network;
  if (res.id >= 0) M.destroyShape(res.id);
  return { network, printer: M.printConstraints() };
}

// Second import des que le premier a lance l'amorcage o3dv : un seul chargement a la fois.
async function probeCold() {
  const frame = document.getElementById("cold");
  const doc = () => frame.contentDocument;
  const r = {};
  await until(() => moduleLoaded(doc()), "cold module", 10);
  const seen = watchStats(frame);
  importSpec(frame, "d4.json");
  await until(() => placeholderAfterLoad(seen) >= 0, "cold first placeholder", 1);
  importSpec(frame, "d4b.json");
  r.stats = await until(() => (doc().getElementById("filename").value === "d4b" && shows2176(doc())), "cold import");
  r.statsHistory = seen.slice(0, 12);
  r.placeholderPath = placeholderAfterLoad(seen) >= 0;
  // Course gagnee : le second import a trouve le viseur pas encore pret (seconde amorce « — »).
  r.raced = seen.filter((t) => t === "—").length >= 3;

  importSpec(frame, "low.json", D4_LOW);
  r.warningBanner = await until(() => (/chargée avec avertissements/.test(bannerText(doc())) ? bannerText(doc()) : null),
                                "warning banner");
  r.warningDetails = doc().getElementById("microarchDetails").textContent;
  return r;
}

async function probeControl() {
  const frame = document.getElementById("control");
  const doc = () => frame.contentDocument;
  const r = {};
  r.hint = await until(() => {
    const h = doc().getElementById("viewerHint");
    return h && /Importer une microarchitecture/.test(h.textContent) && !h.hidden ? h.textContent : null;
  }, "control hint");
  r.title = doc().querySelector("header h1").textContent;
  r.documentTitle = doc().title;
  r.catalog = !!doc().getElementById("catalog");
  r.stats = statsText(doc());
  r.buttons = microButtonsEnabled(doc());
  r.solidEnabled = solidEnabled(doc());
  r.printGroupShown = !doc().getElementById("printGroup").hidden;
  r.pcMinWall = doc().getElementById("pcMinWall").value;
  r.bannerHidden = banner(doc()).hidden;

  importSpec(frame, "bad.json", "{}");
  r.firstRefused = await until(() => (/refusée/.test(bannerText(doc())) ? bannerText(doc()) : null), "control refused");
  await wait(500);
  const h = doc().getElementById("viewerHint");
  r.hintAfterRefusal = h.hidden ? null : h.textContent;
  r.buttonsAfterRefusal = microButtonsEnabled(doc());

  // Import refuse pendant l'amorce d'un triangle lancee par un import accepte : le
  // chargement o3dv se termine sur la microarchitecture gardee, pas sur le triangle.
  const seen = watchStats(frame);
  importSpec(frame, "d4.json");
  await until(() => placeholderAfterLoad(seen) >= 0, "control placeholder", 1);
  importSpec(frame, "bad2.json", "{}");
  r.refusedDuringBootstrap = await until(() => shows2176(doc()), "control refused during bootstrap");
  r.statsHistory = seen.slice(0, 12);
  return r;
}

// Imprimante partagee par les deux pages. Les cadres sont crees APRES l'ecriture de la cle.
async function probePrinter(referencePrinter) {
  const r = {};
  const KEY = "maker.printConstraints.v1";
  const doc0 = JSON.parse(referencePrinter);
  doc0.minWallMm = 0.55;
  doc0.minHoleMm = 0.77;
  localStorage.setItem(KEY, JSON.stringify(doc0));
  const frames = {};
  for (const [name, src] of [["micro", "../web/microarch.html"], ["gothic", "../web/gothic.html?shape=Gothic%20Block"]]) {
    const f = document.createElement("iframe");
    f.id = "printer_" + name;
    f.src = src;
    document.body.insertBefore(f, out);
    frames[name] = f;
  }
  const field = (name, id) => frames[name].contentDocument.getElementById(id);
  for (const name of Object.keys(frames)) {
    r[`${name}Stored`] = await until(() => {
      const hole = field(name, "pcMinHole"), wall = field(name, "pcMinWall");
      return hole && hole.value === "0.77" && wall.value === "0.55" ? hole.value : null;
    }, `printer ${name} stored`);
  }
  // Ecritures de la cle par gothic.html, comptees a partir d'ici.
  const gwin = frames.gothic.contentWindow;
  let gothicWrites = 0;
  const realSet = gwin.Storage.prototype.setItem;
  gwin.Storage.prototype.setItem = function (k, v) { if (k === KEY) gothicWrites++; return realSet.call(this, k, v); };

  const wall = field("micro", "pcMinWall");
  wall.value = "0.9";
  wall.dispatchEvent(new frames.micro.contentWindow.Event("change"));
  r.keyAfterChange = await until(() => {
    const v = localStorage.getItem(KEY);
    return v && JSON.parse(v).minWallMm === 0.9 ? v : null;
  }, "printer key updated");
  r.gothicSynced = await until(() => (field("gothic", "pcMinWall").value === "0.9" ? "0.9" : null), "printer storage event");
  await wait(300);
  r.gothicWrites = gothicWrites;
  gwin.Storage.prototype.setItem = realSet;

  // Ecritures par un autre document (cette page) : valeur refusee, puis cle retiree. Dans les
  // deux cadres, les champs ne bougent pas ; seul le refus va en banniere.
  const fieldsOf = (name) => ["pcMinWall", "pcMinHole"].map((id) => field(name, id).value).join("/");
  const bannerOf = (name) => bannerText(frames[name].contentDocument);
  const hideBanners = () => { for (const name of Object.keys(frames)) banner(frames[name].contentDocument).hidden = true; };
  const before = { micro: fieldsOf("micro"), gothic: fieldsOf("gothic") };
  hideBanners();
  localStorage.setItem(KEY, '{"schema":"x"}');
  r.badValue = {};
  for (const name of Object.keys(frames)) {
    const text = await until(() => (/refusée/.test(bannerOf(name)) ? bannerOf(name) : null), `printer ${name} bad value`);
    r.badValue[name] = { banner: text, unchanged: fieldsOf(name) === before[name] };
  }
  hideBanners();
  localStorage.removeItem(KEY);
  await wait(500);
  r.removed = {};
  for (const name of Object.keys(frames))
    r.removed[name] = { banner: bannerOf(name), unchanged: fieldsOf(name) === before[name] };
  r.fieldsBefore = before;
  return r;
}

// Toute trace de la microarchitecture dans un texte de la page gothique.
const MICROARCH_TRACE =
  /micro[\s-]*architect|microarch|solidRadius|solidCells|solidScale|exportMicroarch|createMicroarch|microarch_solid/i;

async function probeGothic() {
  const r = { texts: {} };
  for (const path of ["../web/gothic.html", "../web/js/pages/gothic.js"]) {
    const res = await fetch(path, { cache: "no-store" });
    const text = res.ok ? await res.text() : "";
    const m = text.match(MICROARCH_TRACE);
    r.texts[path] = { bytes: text.length, trace: m ? m[0] : null };
  }
  const frame = document.getElementById("gothic");
  const doc = () => frame.contentDocument;
  r.stats = await until(() => (/ ms$/.test(statsText(doc())) ? statsText(doc()) : null), "gothic shape");
  r.domTraces = [...doc().querySelectorAll("[id]")].map((e) => e.id).filter((id) => /^(microarch|solid)/i.test(id));
  r.catalog = [...doc().getElementById("catalog").options].map((o) => o.value);
  r.printGroupShown = !doc().getElementById("printGroup").hidden;
  r.pcMinWall = doc().getElementById("pcMinWall").value;
  return r;
}

async function probeHome() {
  const text = await (await fetch("../web/index.html", { cache: "no-store" })).text();
  return { card: /<a class="card" href="\.\/microarch\.html">\s*<h2>Micro architecture gothique<\/h2>/.test(text) };
}

// Forme vide (M40) : amorcage sur un triangle puis meshData, GLB « rien à exporter ».
async function probeEmpty() {
  const frame = document.getElementById("empty");
  const doc = () => frame.contentDocument;
  const r = {};
  await until(() => doc().getElementById("stats"), "empty page", 10);
  const seen = watchStats(frame);
  r.stats = await until(() => {
    const t = statsText(doc());
    return /^0 sommets · 0 faces · .* ms$/.test(t) ? t : null;
  }, "empty shown by meshData");
  r.statsHistory = seen.slice(0, 12);
  // « — » est aussi le texte initial de la page : l'amorce d'un triangle se lit a
  // l'absence du texte que l'amorcage par l'OBJ vide ecrirait (sans « ms »).
  r.placeholderPath = seen.includes("—") && !seen.includes("0 sommets · 0 faces");

  const blobs = [];
  const restore = interceptBlobs(frame.contentWindow, blobs);
  try {
    doc().getElementById("downloadGlbBtn").click();
    r.glbBanner = await until(() => bannerText(doc()) || null, "empty GLB banner");
    r.glbBlobs = blobs.length;
  } finally {
    restore();
  }

  // Iteration 2 : le viseur, amorce, montre des faces en place.
  const it = sliderOf(doc(), "Iterations");
  r.iterationsInput = !!it;
  if (it) {
    it.value = "2";
    it.dispatchEvent(new frame.contentWindow.Event("input", { bubbles: true }));
    r.afterIteration = await until(() => {
      const t = statsText(doc());
      return /^[1-9][0-9]* sommets · [1-9][0-9]* faces · .* ms$/.test(t) ? t : null;
    }, "empty then iteration 2");
  }
  return r;
}

async function run() {
  // En parallele : cold doit importer pendant son premier amorcage, et l'observateur de
  // empty doit etre pose avant son premier affichage.
  [report.probes.control, report.probes.cold, report.probes.empty, report.probes.gothic, report.probes.home] =
    await Promise.all([probeControl(), probeCold(), probeEmpty(), probeGothic(), probeHome()]);
  for (const k of ["control", "cold", "empty", "gothic", "home"]) say(`${k} : ` + JSON.stringify(report.probes[k]));
  report.probes.ready = await probeReady();
  say("ready : " + JSON.stringify(report.probes.ready));
  const ref = await referenceNetwork();
  report.probes.printer = await probePrinter(ref.printer);
  say("printer : " + JSON.stringify(report.probes.printer));
  const x = report.probes.ready.exports, pr = report.probes.printer;
  const c = report.probes.control, k = report.probes.cold, r = report.probes.ready, g = report.probes.gothic;
  const e = report.probes.empty;
  const refusedFooter = (f) => /^Export refusé — display mesh only/.test(f.banner) && f.blobs === 0;
  report.verdicts = {
    page_titre: c.title === "Micro architecture gothique" && /^Micro architecture gothique/.test(c.documentTitle),
    page_sans_catalogue: !c.catalog,
    page_invite_a_importer: !!c.hint && c.stats === "—" && c.bannerHidden,
    page_rien_d_actif_sans_import: c.buttons.every((b) => !b) && !c.solidEnabled,
    page_imprimante_montee: c.printGroupShown && c.pcMinWall !== "",
    accueil_relie_la_page: report.probes.home.card,
    gothique_textes_sans_microarchitecture: Object.values(g.texts).every((t) => t.bytes > 0 && !t.trace),
    gothique_dom_sans_microarchitecture: !!g.stats && g.domTraces.length === 0 &&
                                          !g.catalog.some((v) => /micro/i.test(v)),
    gothique_imprimante_montee: g.printGroupShown && g.pcMinWall !== "",
    premier_import_amorce_sans_obj: r.placeholderAfterLoad && !!r.stats,
    premier_import_masque_l_invite: r.hintHidden,
    boutons_actifs: r.buttonsAfterImport.every(Boolean),
    detail_des_rapports: /^Réseau : /.test(r.details) && /Tubes : construits/.test(r.details),
    pied_obj_refuse: refusedFooter(r.footer.downloadObjBtn),
    pied_glb_refuse: refusedFooter(r.footer.downloadGlbBtn),
    pied_stl_refuse: refusedFooter(r.footer.downloadStlBtn),
    second_import_en_place: r.secondInPlace,
    // Le message du controle JS (taille du fichier), distinct de celui du controle natif
    // (json_too_large — chemin : message) : seul signe visible que le texte n'a pas ete lu.
    trop_grand_avant_lecture: /json_too_large : 4194305 octets, au plus 4194304/.test(r.tooLarge || "") &&
                              r.buttonsAfterTooLarge.every(Boolean),
    refus_garde_l_etat: !!r.refused && r.buttonsAfterRefusal.every(Boolean) && r.solidEnabledAfterRefusal &&
                        /^Réseau : /.test(r.detailsAfterRefusal) && r.filenameAfterRefusal === "d4",
    refus_garde_le_modele_qui_reagit: r.sidesInput && !!r.afterSides,
    refus_pendant_l_amorce_affiche_le_modele: !!c.refusedDuringBootstrap,
    refus_initial_sans_amorce: !!c.firstRefused && /Importer une microarchitecture/.test(c.hintAfterRefusal || "") &&
                               c.buttonsAfterRefusal.every((b) => !b),
    export_noms_filtres: x.network.downloads[0] === "ab_network.json" && x.curves.downloads[0] === "ab_curves.obj" &&
                         x.tubes.downloads[0] === "ab_tubes.obj",
    export_reseau_egal_au_json: !!ref.network && x.network.text === ref.network,
    export_courbes_lignes: /^l /m.test(x.curves.text) && !/^f /m.test(x.curves.text),
    export_tubes_faces: /^f /m.test(x.tubes.text),
    avertissement_en_banniere: !!k.warningBanner && /profile_not_monotonic/.test(k.warningBanner) &&
                               /profile_not_monotonic/.test(k.warningDetails),
    imprimante_relue_microarch: !!pr.microStored,
    imprimante_relue_gothique: !!pr.gothicStored,
    imprimante_change_ecrit_la_cle: !!pr.keyAfterChange,
    imprimante_synchronisee_sans_reecriture: !!pr.gothicSynced && pr.gothicWrites === 0,
    imprimante_valeur_refusee_en_banniere: Object.values(pr.badValue).every((v) => !!v.banner && v.unchanged),
    imprimante_cle_retiree_sans_effet: Object.values(pr.removed).every((v) => v.banner === "" && v.unchanged),
    cold_affiche_par_meshData: !!k.stats,
    cold_amorce_sans_obj: k.placeholderPath,
    solide_bouton_actif: r.solidEnabled && r.solidEnabledAfter,
    solide_banniere_imprimable: !!r.solid.banner && /^Solide STL imprimable, 47632 triangles/.test(r.solid.banner),
    solide_blob_stl: r.solid.blobs.length === 1 && r.solid.blobs[0].size === 84 + 50 * 47632 &&
                     r.solid.blobs[0].type === "model/stl",
    solide_refus_en_banniere: !!r.solidRefused.banner && /refusé/.test(r.solidRefused.banner) &&
                              /solid_invalid_parameter/.test(r.solidRefused.banner) && r.solidRefused.blobs.length === 0,
    solide_non_imprimable_dit: !!r.solidThin.banner && /^Solide STL NON imprimable, STL téléchargé quand même/.test(r.solidThin.banner) &&
                               /minimum scale [0-9.]+ mm per drawing unit/.test(r.solidThin.banner) && r.solidThin.blobs.length === 1,
    solide_etat_avant_calcul: r.solid.states.some((t) => /construction et vérification/.test(t)),
    vide_amorce_par_triangle: e.placeholderPath,
    vide_affiche_par_meshData: !!e.stats,
    vide_glb_rien_a_exporter: /rien à exporter/.test(e.glbBanner || "") && e.glbBlobs === 0,
    vide_puis_iteration_en_place: !!e.afterIteration,
    attentes_bornees: report.timeouts.length === 0,
  };
  // Information : la course du cas cold n'est pas garantie (depend de la duree de l'amorcage).
  report.cold_race_won = k.raced;
  report.ok = Object.values(report.verdicts).every(Boolean);
  say("verdicts : " + JSON.stringify(report.verdicts));
}

run()
  .then(() => (report.status = "ok"))
  .catch((e) => {
    report.status = "echec";
    report.ok = false;
    report.error = String((e && e.stack) || e);
    say("ERREUR : " + report.error);
  })
  .finally(() => fetch("/result", { method: "POST", body: JSON.stringify(report, null, 2) }));
