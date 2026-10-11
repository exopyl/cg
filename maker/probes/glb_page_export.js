// ===========================================================================
//  Sonde : le GLB que livre svg.html, relu par un lecteur TIERS
// ===========================================================================
//
// Un GLB se verifie mecaniquement dans ctest (tu_cgmesh_io_gltf.cpp). Trois
// proprietes echappent a cette verification, parce qu'elles ne se voient qu'a
// l'arrivee, chez un lecteur qui n'est pas le notre :
//
//   P2  COULEURS JUSTES, ni delavees ni assombries ;
//   P3  ECHELLE : la piece mesure ce que le document dit ;
//   P4  ORIENTATION : elle est a plat, du bon cote.
//
// LE LECTEUR TIERS EST o3dv, et le choix n'est pas d'opportunite. Son importeur
// glTF est une implementation INDEPENDANTE de tinygltf : ecrire et relire avec
// tinygltf ne detecterait aucune erreur commune aux deux.
//
// L'ORACLE DE COULEUR EST UN ALLER-RETOUR, et c'est ce qui le rend probant.
// L'importeur glTF d'o3dv traite baseColorFactor comme LINEAIRE et le reconvertit
// en sRGB 8 bits (`LinearToSRGB`, bundle vendorise). Son importeur OBJ, lui, lit
// `Kd` comme du sRGB et le multiplie par 255. Donc :
//
//     si notre ecrivain lineraise correctement, les couleurs relues du GLB
//     EGALENT celles relues de l'OBJ, materiau par materiau.
//
// Une conversion oubliee ferait ressortir le GLB plus CLAIR que l'OBJ ; une
// conversion appliquee a l'envers, plus SOMBRE. Les deux se lisent dans le meme
// chiffre. C'est la reserve R4, mesuree.
//
// LES DEUX FICHIERS VIENNENT DES BOUTONS de la page, pas du pont : c'est le
// cablage de G5 qui est eprouve en meme temps que l'ecrivain de G4.
//
// RESSOURCE : data/tiger.svg, non versionne (cf. probes/data/.gitignore) --
//     cp test/data/svg/Ghostscript_Tiger.svg maker/probes/data/tiger.svg
//
import { OV, isAvailable } from "../web/js/o3dv.js";

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

// Lecteur d'archive STORED : le ZipManager du moteur natif n'ecrit que des
// entrees non compressees, donc aucun decodeur n'est necessaire.
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

// Charge une FileList dans un viseur neuf et rend ce que l'IMPORT a produit :
// materiaux avec leurs couleurs, et boite englobante en coordonnees MONDE
// (transformations de noeud appliquees -- c'est precisement ce qu'on mesure).
function load(containerId, files) {
  return new Promise((resolve) => {
    const container = document.getElementById(containerId);
    let done = false;
    const finish = (status) => {
      if (done) return;
      done = true;
      const model = viewer.GetModel();
      const materials = [];
      if (model)
        for (let i = 0; i < model.MaterialCount(); ++i) {
          const m = model.GetMaterial(i);
          materials.push({
            name: m.name,
            color: m.color ? [m.color.r, m.color.g, m.color.b] : null,
          });
        }
      let box = null;
      try {
        const b = viewer.GetViewer().GetBoundingBox(() => true);
        if (b && isFinite(b.min.x))
          box = { min: [b.min.x, b.min.y, b.min.z], max: [b.max.x, b.max.y, b.max.z] };
      } catch (e) { box = null; }
      resolve({
        status,
        materialCount: model ? model.MaterialCount() : 0,
        meshCount: model ? model.MeshCount() : 0,
        materials,
        box,
        extent: box ? [0, 1, 2].map((k) => box.max[k] - box.min[k]) : null,
      });
    };
    const viewer = new OV.EmbeddedViewer(container, {
      backgroundColor: new OV.RGBAColor(255, 255, 255, 255),
      defaultColor: new OV.RGBColor(180, 190, 200),
      onModelLoaded: () => finish("charge"),
      onModelLoadFailed: () => finish("refuse"),
    });
    viewer.LoadModelFromFileList(files);
    setTimeout(() => finish("aucune reponse"), 60000);
  });
}

// Clique un bouton de la page et intercepte le Blob a URL.createObjectURL :
// c'est l'objet EXACT que le navigateur aurait ecrit sur le disque.
async function clickAndCapture(win, doc, buttonId) {
  let captured = null;
  const real = win.URL.createObjectURL.bind(win.URL);
  win.URL.createObjectURL = (blob) => { captured = blob; return real(blob); };
  const btn = doc().getElementById(buttonId);
  if (!btn) { win.URL.createObjectURL = real; throw new Error(buttonId + " absent de la page"); }
  btn.click();
  const blob = await until(() => captured, 120000);
  win.URL.createObjectURL = real;
  if (!blob) throw new Error(buttonId + " n'a rien produit");
  return blob;
}

const ratio = (a, b) => (b === 0 ? null : a / b);

async function run() {
  if (!isAvailable) throw new Error("bundle vendor o3dv absent");

  const win = frame.contentWindow;
  const doc = () => frame.contentDocument;

  const handle = await until(() => win.maker, 180000);
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
  }, 180000);
  if (!status) throw new Error("le Tigre ne s'est pas evalue");
  report.probes.P1 = { status };
  say("P1 : " + status);

  // ---- les deux fichiers, par les BOUTONS de la page --------------------
  const zipBlob = await clickAndCapture(win, doc, "downloadObjBtn");
  const glbBlob = await clickAndCapture(win, doc, "downloadGlbBtn");

  const entries = readStoredZip(await zipBlob.arrayBuffer());
  if (!entries) throw new Error("l'export OBJ n'est pas une archive lisible");
  const objName = Object.keys(entries).find((n) => n.endsWith(".obj"));
  const mtlName = Object.keys(entries).find((n) => n.endsWith(".mtl"));
  if (!objName || !mtlName) throw new Error("archive OBJ incomplete");

  const glbBytes = new Uint8Array(await glbBlob.arrayBuffer());
  report.probes.fichiers = {
    glbType: glbBlob.type,
    glbBytes: glbBytes.length,
    glbMagic: String.fromCharCode(...glbBytes.subarray(0, 4)),
    zipBytes: zipBlob.size,
    zipEntries: Object.keys(entries),
  };
  say("fichiers : " + JSON.stringify(report.probes.fichiers));

  // ---- relecture par le lecteur tiers -----------------------------------
  const objRes = await load("stageObj", [
    new File([entries[objName]], objName, { type: "text/plain" }),
    new File([entries[mtlName]], mtlName, { type: "text/plain" }),
  ]);
  const glbRes = await load("stageGlb", [
    new File([glbBytes], "model.glb", { type: "model/gltf-binary" }),
  ]);
  say("OBJ : " + JSON.stringify({ ...objRes, materials: objRes.materials.length }));
  say("GLB : " + JSON.stringify({ ...glbRes, materials: glbRes.materials.length }));

  // ---- P2 : les couleurs ------------------------------------------------
  // Materiau par materiau, par NOM : les indices pourraient se renumeroter
  // d'un format a l'autre sans que rien ne soit faux.
  const objByName = new Map(objRes.materials.map((m) => [m.name, m.color]));
  const compared = [];
  let worst = 0, worstName = null;
  for (const m of glbRes.materials) {
    const ref = objByName.get(m.name);
    if (!ref || !m.color) continue;
    const d = Math.max(...[0, 1, 2].map((k) => Math.abs(m.color[k] - ref[k])));
    compared.push({ name: m.name, glb: m.color, obj: ref, ecart: d });
    if (d > worst) { worst = d; worstName = m.name; }
  }
  const distinctGlb = new Set(glbRes.materials.map((m) => (m.color || []).join(","))).size;
  report.probes.P2_couleurs = {
    compares: compared.length,
    ecartMax: worst,
    pire: worstName,
    couleursDistinctes: distinctGlb,
    echantillon: compared.slice(0, 5),
  };
  say("P2 couleurs : " + JSON.stringify(report.probes.P2_couleurs));

  // ---- P3 : l'echelle ---------------------------------------------------
  // Le GLB est en METRES (noeud a 0,001), l'OBJ en millimetres bruts. Les
  // etendues TRIEES eliminent la question de l'axe, qui est celle de P4.
  const sortDesc = (a) => [...a].sort((x, y) => y - x);
  const eObj = objRes.extent ? sortDesc(objRes.extent) : null;
  const eGlb = glbRes.extent ? sortDesc(glbRes.extent) : null;
  const ratios = eObj && eGlb ? [0, 1, 2].map((k) => ratio(eGlb[k], eObj[k])) : null;
  report.probes.P3_echelle = {
    extentObj: objRes.extent,
    extentGlb: glbRes.extent,
    rapportsTries: ratios,
    attendu: 0.001,
  };
  say("P3 echelle : " + JSON.stringify(report.probes.P3_echelle));

  // ---- P4 : l'orientation ----------------------------------------------
  // La piece est une PLAQUE : sa plus petite etendue est son EPAISSEUR, portee
  // par Z dans le document et bornee a [0, profondeur] -- jamais negative.
  // La rotation de -90 degres autour de X la fait passer sur l'axe VERTICAL du
  // lecteur ; une rotation de +90 l'y mettrait aussi, mais a l'envers, donc avec
  // une etendue NEGATIVE. C'est le signe qui distingue les deux, pas l'axe.
  const axisNames = ["x", "y", "z"];
  const thinObj = objRes.extent ? objRes.extent.indexOf(Math.min(...objRes.extent)) : -1;
  const thinGlb = glbRes.extent ? glbRes.extent.indexOf(Math.min(...glbRes.extent)) : -1;
  report.probes.P4_orientation = {
    axeEpaisseurObj: axisNames[thinObj],
    axeEpaisseurGlb: axisNames[thinGlb],
    bornesEpaisseurGlb: glbRes.box ? [glbRes.box.min[thinGlb], glbRes.box.max[thinGlb]] : null,
    // Le GLB doit avoir CHANGE d'axe d'epaisseur par rapport a l'OBJ : c'est la
    // signature de la rotation. S'ils coincident, aucune rotation n'a ete lue.
    rotationLue: thinObj !== thinGlb,
    node: { scale: 0.001, rotation: "-90 deg autour de X" },
  };
  say("P4 orientation : " + JSON.stringify(report.probes.P4_orientation));

  // ---- verdicts ---------------------------------------------------------
  const echelleOk = ratios !== null
    && ratios.every((r) => r !== null && Math.abs(r - 0.001) < 0.001 * 0.02);
  const epaisseurPositive = report.probes.P4_orientation.bornesEpaisseurGlb
    ? report.probes.P4_orientation.bornesEpaisseurGlb[0] > -1e-9
    : false;

  report.verdicts = {
    glb_charge_chez_un_lecteur_tiers: glbRes.status === "charge" && glbRes.meshCount > 0,
    palette_complete: glbRes.materialCount >= 30 && distinctGlb >= 30,
    // R4 : la conversion sRGB, mesuree bout en bout. 1/255 est la resolution du
    // canal, donc la plus petite difference qui ait un sens.
    couleurs_identiques_a_l_OBJ: compared.length >= 30 && worst <= 1,
    echelle_mille: echelleOk,
    rotation_appliquee: report.probes.P4_orientation.rotationLue,
    epaisseur_du_bon_cote: epaisseurPositive,
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
