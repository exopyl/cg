// ===========================================================================
//  Sonde o3dv : que fait le lecteur OBJ d'un `mtllib` et d'un .mtl compagnon ?
// ===========================================================================
//
// Trois cas, dans l'ordre de ce qu'ils etablissent :
//
//   P1  paire   FileList = [ .obj avec mtllib, .mtl ] -> le .mtl est-il RESOLU,
//               et le modele porte-t-il ses materiaux ?
//   P2  orphelin FileList = [ .obj avec mtllib ] SEUL -> le chargement est-il un
//               ECHEC, ou un succes avec un fichier manquant recense ?
//   P3  nu      FileList = [ .obj sans mtllib ] -> TEMOIN. Il fixe le nombre de
//               materiaux d'un modele qui ne declare aucun compagnon, auquel P1
//               se compare : sans lui, les deux materiaux de P1 pourraient etre
//               le defaut du lecteur plutot que ceux du .mtl.
//
// Chaque cas passe par OV.EmbeddedViewer, le MEME point d'entree que
// web/js/viewer.js -- une sonde sur un chemin voisin n'etablirait rien sur le
// chemin de production. `missingFiles` se lit sur l'importeur du chargeur, seul
// endroit ou il survit a l'import (EmbeddedViewer ne retient que le modele).
//
import { OV, isAvailable } from "../web/js/o3dv.js";

const report = { started: new Date().toISOString(), probes: {} };
const out = document.getElementById("out");
const say = (t) => (out.textContent += t + "\n");

// Deux quads disjoints, un materiau chacun. Meme forme que le maillage temoin
// des tests C++ d'export : rouge et bleu, distinguables d'un defaut gris.
const OBJ_BODY =
  "v 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 0\n" +
  "v 2 0 0\nv 3 0 0\nv 3 1 0\nv 2 1 0\n" +
  "usemtl rouge_vif\nf 1 2 3 4\n" +
  "usemtl bleu_nuit\nf 5 6 7 8\n";

const OBJ_WITH_MTLLIB = "mtllib sonde.mtl\n" + OBJ_BODY;
const OBJ_BARE = OBJ_BODY;

const MTL =
  "newmtl rouge_vif\nKa 0.2 0.2 0.2\nKd 1.000000 0.000000 0.000000\n" +
  "Ks 0 0 0\nNs 0\nd 1.000000\nillum 1\n\n" +
  "newmtl bleu_nuit\nKa 0.2 0.2 0.2\nKd 0.000000 0.000000 1.000000\n" +
  "Ks 0 0 0\nNs 0\nd 1.000000\nillum 1\n";

function fileOf(text, name) {
  return new File([text], name, { type: "text/plain" });
}

// Charge une FileList dans un EmbeddedViewer neuf et rend ce que l'import a
// REELLEMENT produit. Le timeout compte : un echec silencieux -- ni
// onModelLoaded ni onModelLoadFailed -- doit se distinguer d'un refus explicite.
function load(containerId, files) {
  return new Promise((resolve) => {
    const container = document.getElementById(containerId);
    let done = false;
    const finish = (status) => {
      if (done) return;
      done = true;
      const importer = viewer.modelLoader ? viewer.modelLoader.GetImporter() : null;
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
      resolve({
        status,
        missingFiles: importer ? importer.missingFiles.slice() : null,
        usedFiles: importer ? importer.usedFiles.slice() : null,
        materialCount: model ? model.MaterialCount() : 0,
        meshCount: model ? model.MeshCount() : 0,
        materials,
      });
    };

    const viewer = new OV.EmbeddedViewer(container, {
      backgroundColor: new OV.RGBAColor(255, 255, 255, 255),
      defaultColor: new OV.RGBColor(180, 190, 200),
      onModelLoaded: () => finish("charge"),
      onModelLoadFailed: () => finish("refuse"),
    });
    viewer.LoadModelFromFileList(files);
    setTimeout(() => finish("aucune reponse"), 15000);
  });
}

async function run() {
  if (!isAvailable) throw new Error("bundle vendor o3dv absent");

  report.probes.P1_paire = await load("stagePair", [
    fileOf(OBJ_WITH_MTLLIB, "sonde.obj"),
    fileOf(MTL, "sonde.mtl"),
  ]);
  say("P1 paire     : " + JSON.stringify(report.probes.P1_paire));

  report.probes.P2_orphelin = await load("stageOrphan", [
    fileOf(OBJ_WITH_MTLLIB, "sonde.obj"),
  ]);
  say("P2 orphelin  : " + JSON.stringify(report.probes.P2_orphelin));

  report.probes.P3_nu = await load("stageBare", [fileOf(OBJ_BARE, "nu.obj")]);
  say("P3 nu        : " + JSON.stringify(report.probes.P3_nu));

  const p1 = report.probes.P1_paire;
  const p2 = report.probes.P2_orphelin;
  const p3 = report.probes.P3_nu;

  report.verdicts = {
    // R1-a : une FileList a deux entrees resout le .mtl.
    paire_resout_le_mtl:
      p1.status === "charge" && p1.missingFiles.length === 0 && p1.materialCount >= 2,
    // R1-b : un mtllib orphelin n'est pas une erreur de chargement.
    mtllib_orphelin_tolere:
      p2.status === "charge" && p2.missingFiles.length === 1,
    // Contre-epreuve : sans .mtl, les materiaux ne sont PAS la. Sans elle,
    // P1 pourrait passer sur un modele qui compte deux materiaux par defaut.
    sans_mtl_pas_de_couleurs: p3.status === "charge" && p1.materialCount > p3.materialCount,
  };
  report.ok = Object.values(report.verdicts).every(Boolean);
  say("verdicts     : " + JSON.stringify(report.verdicts));
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
