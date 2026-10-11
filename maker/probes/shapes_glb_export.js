// ===========================================================================
//  Sonde : le bouton GLB d'une page de FORMES (shell.js -> exportGlb)
// ===========================================================================
//
// La sonde glb_page_export.js couvre le chemin GABARIT (template.js ->
// graphExportGlb). Celui-ci est l'AUTRE : une forme parametrique identifiee,
// servie par shell.js. Les deux boutons ont ete ajoutes ensemble ; en verifier
// un et supposer l'autre serait tenir le cablage pour acquis.
//
// Le maillage d'une forme parametrique ne porte AUCUN materiau : l'oracle n'est
// donc pas la palette mais le conteneur -- un GLB valide, avec sa primitive
// unique et son materiau de remplacement explicite.
//
const report = { started: new Date().toISOString(), probes: {} };
const out = document.getElementById("out");
const say = (t) => (out.textContent += t + "\n");
const wait = (ms) => new Promise((r) => setTimeout(r, ms));
const frame = document.getElementById("page");

async function until(p, timeoutMs, step = 100) {
  const t0 = performance.now();
  while (performance.now() - t0 < timeoutMs) { const v = p(); if (v) return v; await wait(step); }
  return null;
}

// Relit le conteneur GLB comme le ferait un lecteur : magic, version, longueur
// declaree, chunks alignes sur 4.
function splitGlb(buf) {
  const dv = new DataView(buf);
  const u8 = new Uint8Array(buf);
  if (u8.length < 12) return null;
  if (String.fromCharCode(...u8.subarray(0, 4)) !== "glTF") return null;
  if (dv.getUint32(4, true) !== 2) return null;
  if (dv.getUint32(8, true) !== u8.length) return null;
  let p = 12, json = null, bin = 0;
  while (p + 8 <= u8.length) {
    const len = dv.getUint32(p, true), type = dv.getUint32(p + 4, true);
    if (len % 4 !== 0) return null;
    if (p + 8 + len > u8.length) return null;
    if (type === 0x4e4f534a) json = new TextDecoder().decode(u8.subarray(p + 8, p + 8 + len));
    else if (type === 0x004e4942) bin = len;
    else return null;
    p += 8 + len;
  }
  return json ? { json: JSON.parse(json), bin } : null;
}

async function run() {
  const win = frame.contentWindow;
  const doc = () => frame.contentDocument;

  const ready = await until(() => {
    const t = (doc().getElementById("stats") || {}).textContent || "";
    return /sommets/.test(t) ? t : null;
  }, 180000);
  if (!ready) throw new Error("shapes.html n'a pas affiche de modele");
  report.probes.P1 = { stats: ready };
  say("P1 : " + ready);

  let captured = null;
  const real = win.URL.createObjectURL.bind(win.URL);
  win.URL.createObjectURL = (b) => { captured = b; return real(b); };
  const btn = doc().getElementById("downloadGlbBtn");
  if (!btn) throw new Error("downloadGlbBtn absent du pied de shell.js");
  if (btn.disabled) throw new Error("downloadGlbBtn reste desactive");
  btn.click();
  const blob = await until(() => captured, 120000);
  win.URL.createObjectURL = real;
  if (!blob) throw new Error("le bouton GLB n'a rien produit");

  const g = splitGlb(await blob.arrayBuffer());
  if (!g) throw new Error("l'objet telecharge n'est pas un GLB lisible");
  const prims = g.json.meshes[0].primitives;
  report.probes.P2 = {
    type: blob.type,
    bytes: blob.size,
    assetVersion: g.json.asset.version,
    binBytes: g.bin,
    primitives: prims.length,
    materials: g.json.materials.length,
    attributs: Object.keys(prims[0].attributes),
    // Un maillage NON PEINT ne doit pas porter COLOR_0 : le gris 0,5 par defaut
    // assombrirait baseColorFactor de moitie.
    color0: prims.some((p) => "COLOR_0" in p.attributes),
    metallic: g.json.materials[0].pbrMetallicRoughness.metallicFactor,
    node: g.json.nodes[0],
  };
  say("P2 : " + JSON.stringify(report.probes.P2));

  report.verdicts = {
    bouton_present_et_actif: true,
    conteneur_valide: g.json.asset.version === "2.0" && g.bin > 0,
    une_primitive_par_plage: prims.length >= 1 && g.json.materials.length >= 1,
    materiau_non_metallique: g.json.materials[0].pbrMetallicRoughness.metallicFactor === 0,
    pas_de_color0_sur_un_maillage_non_peint: report.probes.P2.color0 === false,
    noeud_en_metres: g.json.nodes[0].scale[0] === 0.001,
  };
  report.ok = Object.values(report.verdicts).every(Boolean);
  say("verdicts : " + JSON.stringify(report.verdicts));
}

run()
  .then(() => (report.status = "ok"))
  .catch((e) => { report.status = "echec"; report.ok = false; report.error = String((e && e.stack) || e); say("ERREUR : " + report.error); })
  .finally(() => fetch("/result", { method: "POST", body: JSON.stringify(report, null, 2) }));
