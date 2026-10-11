// ===========================================================================
//  Sonde : ce que coute chaque etape du pas de curseur d'une page GABARIT
// ===========================================================================
//
// La page svg.html rejoue trois choses a chaque mouvement de curseur :
// graphEvaluate, puis la charge utile de rendu (graphMeshData), puis -- et
// c'est ce qui se mesure ici -- une SERIALISATION OBJ complete du maillage,
// dont le resultat n'est consomme qu'a l'amorcage du viewer et au clic sur
// « Telecharger ». La sonde chiffre les trois separement, pour que le gain
// d'un deplacement de la serialisation sous le bouton se lise sur une mesure
// et non sur une impression.
//
// Elle sert AUSSI de reference de taille : longueur de la chaine OBJ, nombre de
// sommets et de triangles du Tigre.
//
// RESSOURCE : data/tiger.svg. `maker/probes/data/` n'est pas versionne (voir son
// .gitignore) ; reinstaller le fichier avec
//     cp test/data/svg/Ghostscript_Tiger.svg maker/probes/data/tiger.svg
//
// Le graphe evalue est CELUI DE LA PAGE, lu dans son gabarit : mesurer un graphe
// ecrit ici mesurerait autre chose que ce que l'utilisateur declenche.
//
import { loadModule } from "../web/js/wasm.js";

const report = { started: new Date().toISOString(), probes: {} };
const out = document.getElementById("out");
const say = (t) => (out.textContent += t + "\n");

const TIGER = "data/tiger.svg";
const TEMPLATE = "../web/data/templates/svg.json";

// Mediane sur `n` tirages, en millisecondes. Mediane et non moyenne : un tirage
// unique attrape le cout de chauffe, et une moyenne se laisse tirer par lui.
function median(fn, n) {
  const t = [];
  for (let i = 0; i < n; ++i) {
    const t0 = performance.now();
    fn();
    t.push(performance.now() - t0);
  }
  t.sort((a, b) => a - b);
  return { median: +t[t.length >> 1].toFixed(2), all: t.map((v) => +v.toFixed(2)) };
}

async function run() {
  const { Module, staleMessage } = await loadModule();
  if (staleMessage) report.stale = staleMessage;

  const tpl = await (await fetch(TEMPLATE, { cache: "no-store" })).json();
  const error = Module.graphFromJson(JSON.stringify(tpl.graph));
  if (error) throw new Error("graphe refuse : " + error);

  // file.ref designe un CHEMIN : la ressource doit exister dans le systeme de
  // fichiers du module, au chemin que le document nomme. Meme geste que
  // template.js:placeFile.
  const spec = tpl.sources[0];
  const bytes = new Uint8Array(await (await fetch(TIGER)).arrayBuffer());
  Module.FS.writeFile("/tiger.svg", bytes);
  Module.graphSetString(spec.node, "path", "/tiger.svg");

  const first = JSON.parse(Module.graphEvaluate(tpl.output));
  if (first.status !== "ok") throw new Error("evaluation refusee : " + JSON.stringify(first));
  report.mesh = { nv: first.nv, nf: first.nf, svg: TIGER, bytes: bytes.length };
  say(`maillage : ${first.nv} sommets, ${first.nf} triangles`);

  // Un pas de curseur = une evaluation. La ré-évaluation d'un graphe inchangé
  // peut etre servie par le cache de noeuds : c'est justement le cas mesure ici,
  // celui ou la serialisation OBJ domine tout le reste.
  report.probes.graphEvaluate = median(() => Module.graphEvaluate(tpl.output), 5);
  report.probes.graphMeshData = median(() => Module.graphMeshData(0), 5);

  let obj = "";
  report.probes.graphExportObj = median(() => { obj = Module.graphExportObj(0); }, 5);
  report.probes.graphExportObj.length = obj.length;
  report.probes.graphExportObj.hasMtllib = obj.startsWith("mtllib") || obj.includes("\nmtllib ");
  report.probes.graphExportObj.usemtl = (obj.match(/^usemtl /gm) || []).length;

  // L'archive ZIP OBJ+MTL du chemin GABARIT, quand le binding existe. Absent
  // d'un artefact d'avant ce chantier : la sonde reste alors utilisable.
  if (typeof Module.graphExportObjZip === "function") {
    let zip = null;
    report.probes.graphExportObjZip = median(() => { zip = Module.graphExportObjZip(0, "tigre"); }, 5);
    report.probes.graphExportObjZip.bytes = zip ? zip.bytes.length : 0;
  }

  say("mesures (ms, mediane de 5) : " + JSON.stringify(report.probes, null, 2));
  report.ok = true;
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
