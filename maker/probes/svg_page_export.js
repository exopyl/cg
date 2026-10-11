// ===========================================================================
//  Sonde : ce que la page svg.html telecharge, et ce qu'elle serialise
// ===========================================================================
//
// Quatre constats, tous pris sur la PAGE REELLE chargee en iframe :
//
//   P1  le Tigre depose par l'entree de fichier s'evalue et s'affiche ;
//   P2  UN PAS DE CURSEUR NE SERIALISE RIEN : le compteur d'appels a
//       graphExportObj reste a zero pendant N deplacements ;
//   P3  le bouton OBJ produit une ARCHIVE : elle est interceptee a
//       URL.createObjectURL, lue comme un vrai lecteur ZIP, et son .mtl est
//       compte ;
//   P4  les noms `usemtl` du .obj et `newmtl` du .mtl s'alignent.
//
// La page publie `window.maker = { Module, template, stats }` : c'est par la
// que les compteurs s'installent, sur l'INSTANCE que la page utilise et non sur
// un module charge une seconde fois.
//
// RESSOURCE : data/tiger.svg, non versionne (cf. probes/data/.gitignore) --
//     cp test/data/svg/Ghostscript_Tiger.svg maker/probes/data/tiger.svg
//
const report = { started: new Date().toISOString(), probes: {} };
const out = document.getElementById("out");
const say = (t) => (out.textContent += t + "\n");
const wait = (ms) => new Promise((r) => setTimeout(r, ms));

const frame = document.getElementById("page");

async function until(predicate, timeoutMs, step = 100) {
  const t0 = performance.now();
  while (performance.now() - t0 < timeoutMs) {
    const v = predicate();
    if (v) return v;
    await wait(step);
  }
  return null;
}

// Lecteur d'archive STORED : parcours du repertoire central, extraction. Le
// ZipManager du moteur natif n'ecrit que des entrees non compressees.
function readStoredZip(buf) {
  const u8 = new Uint8Array(buf);
  const dv = new DataView(buf);
  const dec = new TextDecoder("utf-8");
  let eocd = -1;
  for (let i = u8.length - 22; i >= 0; --i)
    if (dv.getUint32(i, true) === 0x06054b50) { eocd = i; break; }
  if (eocd < 0) return null;

  const n = dv.getUint16(eocd + 10, true);
  let p = dv.getUint32(eocd + 16, true);
  const entries = {};
  for (let k = 0; k < n; ++k) {
    if (dv.getUint32(p, true) !== 0x02014b50) return null;
    const usize = dv.getUint32(p + 24, true);
    const nlen = dv.getUint16(p + 28, true);
    const lho = dv.getUint32(p + 42, true);
    const name = dec.decode(u8.subarray(p + 46, p + 46 + nlen));
    const start = lho + 30 + dv.getUint16(lho + 26, true) + dv.getUint16(lho + 28, true);
    entries[name] = dec.decode(u8.subarray(start, start + usize));
    p += 46 + nlen + dv.getUint16(p + 30, true) + dv.getUint16(p + 32, true);
  }
  return entries;
}

const directiveArgs = (text, keyword) =>
  (text.match(new RegExp("^" + keyword + " (.*)$", "gm")) || [])
    .map((l) => l.slice(keyword.length + 1).trim());

async function run() {
  const win = frame.contentWindow;
  const doc = () => frame.contentDocument;

  // La page publie sa poignee de diagnostic une fois le module charge.
  const handle = await until(() => win.maker, 120000);
  if (!handle) throw new Error("window.maker absent : la page n'a pas demarre");

  // ---- P1 : le Tigre, depose par l'entree de fichier de la page ----------
  const svg = await (await fetch("data/tiger.svg")).text();
  const input = doc().querySelector("#sources input[type=file]");
  if (!input) throw new Error("entree de fichier introuvable");
  const dt = new win.DataTransfer();
  dt.items.add(new win.File([svg], "tiger.svg", { type: "image/svg+xml" }));
  input.files = dt.files;
  input.dispatchEvent(new win.Event("change"));

  const status = await until(() => {
    const t = doc().getElementById("status").textContent;
    return /sommets/.test(t) ? t : null;
  }, 120000);
  report.probes.P1 = { status, verdict: status ? "le Tigre est evalue" : "AUCUNE EVALUATION" };
  say("P1 : " + status);
  if (!status) throw new Error("le Tigre ne s'est pas evalue");

  // ---- P2 : un pas de curseur ne serialise pas -------------------------
  // Les compteurs sont poses APRES l'amorcage : c'est la serialisation du
  // chemin d'EVALUATION qu'on compte, pas celle du premier affichage.
  let nObj = 0, nZip = 0;
  const realObj = handle.Module.graphExportObj.bind(handle.Module);
  const realZip = handle.Module.graphExportObjZip.bind(handle.Module);
  handle.Module.graphExportObj = (...a) => { ++nObj; return realObj(...a); };
  handle.Module.graphExportObjZip = (...a) => { ++nZip; return realZip(...a); };

  // « Profondeur (mm) » : un curseur ordinaire de la page. Le nom n'importe
  // pas -- ce qui compte est qu'un reglage change et que la page reevalue.
  const sliders = [...doc().querySelectorAll('#params input[type=range]')];
  const slider = sliders.find((s) => (s.previousElementSibling || {}).textContent
    ? /Profondeur/.test(s.closest(".param").textContent) : false) || sliders[0];
  if (!slider) throw new Error("aucun curseur dans le panneau");

  const steps = 5;
  for (let i = 0; i < steps; ++i) {
    slider.value = String(+slider.value + (+slider.step || 0.1));
    slider.dispatchEvent(new win.Event("input", { bubbles: true }));
    await wait(400);   // l'evaluation est coalescee par setTimeout
  }

  // AUCUN CHRONOMETRE ICI : la cadence de cette boucle est celle de son propre
  // `wait`, pas celle de la page. Le cout d'un pas se mesure sur le Module,
  // fonction par fonction -- c'est la sonde graph_obj_cost.js.
  report.probes.P2 = {
    steps,
    graphExportObjCalls: nObj,
    verdict: nObj === 0
      ? "aucune serialisation OBJ sur le chemin d'evaluation"
      : `${nObj} SERIALISATIONS pour ${steps} pas`,
  };
  say("P2 : " + JSON.stringify(report.probes.P2));

  // ---- P3/P4 : le bouton, et ce qu'il livre ----------------------------
  // Le Blob est intercepte a URL.createObjectURL : c'est l'objet EXACT que le
  // navigateur aurait ecrit sur le disque.
  let captured = null;
  const realCreate = win.URL.createObjectURL.bind(win.URL);
  win.URL.createObjectURL = (blob) => { captured = blob; return realCreate(blob); };

  doc().getElementById("downloadObjBtn").click();
  const blob = await until(() => captured, 60000);
  win.URL.createObjectURL = realCreate;
  if (!blob) throw new Error("le bouton OBJ n'a rien produit");

  const entries = readStoredZip(await blob.arrayBuffer());
  if (!entries) throw new Error("l'objet telecharge n'est pas une archive lisible");
  const names = Object.keys(entries);
  const objName = names.find((n) => n.endsWith(".obj"));
  const mtlName = names.find((n) => n.endsWith(".mtl"));

  const declared = mtlName ? directiveArgs(entries[mtlName], "newmtl") : [];
  const used = objName ? directiveArgs(entries[objName], "usemtl") : [];
  const usedDistinct = [...new Set(used)];
  const orphans = usedDistinct.filter((u) => !declared.includes(u));

  report.probes.P3 = {
    graphExportObjZipCalls: nZip,
    type: blob.type,
    bytes: blob.size,
    entries: names,
    newmtl: declared.length,
    usemtlDistinct: usedDistinct.length,
    mtllibResolves: objName && mtlName
      ? entries[objName].includes("mtllib " + mtlName) : false,
  };
  report.probes.P4 = {
    orphans,
    verdict: orphans.length === 0
      ? "chaque usemtl a son newmtl"
      : `${orphans.length} usemtl sans newmtl`,
  };
  say("P3 : " + JSON.stringify(report.probes.P3));
  say("P4 : " + JSON.stringify(report.probes.P4));

  report.verdicts = {
    aucune_serialisation_par_pas: nObj === 0,
    archive_a_deux_entrees: names.length === 2 && !!objName && !!mtlName,
    palette_complete: declared.length >= 30 && usedDistinct.length >= 30,
    noms_alignes: orphans.length === 0 && report.probes.P3.mtllibResolves,
    bouton_serialise_une_fois: nZip === 1,
  };
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
