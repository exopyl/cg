// ===========================================================================
//  Sonde de RECADRAGE a l'ouverture d'un document -- cinq captures
// ===========================================================================
//
// ⚠ CETTE SONDE NE CONCLUT RIEN TOUTE SEULE, comme style.js et pour la meme
// raison : le juge d'un rendu est un oeil humain, et il n'y en a pas dans un
// navigateur sans tete. Ce qu'elle fait, c'est produire les cinq etats que le
// comportement doit distinguer, et les rendre en PNG.
//
// Elle est HORS CI : elle s'execute contre l'artefact WebAssembly, que `TU` ne
// lie pas.
//
// Les cinq captures, dans l'ordre :
//
//   avant   le graphe est BATI noeud par noeud, aux positions d'un document
//           ecrit sous l'ancienne disposition -- x entre 20 et 300, donc sous
//           la palette qui flotte au-dessus. Aucun chargement n'a eu lieu,
//           donc aucun recadrage : c'est le comportement d'avant ce chantier ;
//   zoom    la meme vue zoomee A LA MOLETTE. Temoin : il dit ce que le zoom
//           seul fait au rendu, pour qu'un defaut de zoom ne soit pas impute
//           au recadrage ;
//   apres   le MEME document, exporte puis relu par "loadDocument". C'est le
//           seul geste qui declenche RequestFitToContent ;
//   ajout   un noeud de plus, ajoute apres le recadrage. La vue ne doit PAS
//           bouger -- un ajout n'est pas un chargement ;
//   vide    un document sans noeud, relu par le meme chemin. Le declencheur
//           est pose et reste sans effet.
//
const report = { started: new Date().toISOString(), probes: {} };
const out = document.getElementById("out");
const say = (text) => (out.textContent += "\n" + text);

// Positions en coordonnees de l'EDITEUR. Elles sont deliberement toutes vers
// l'abscisse zero : c'est le cas qui motive le chantier, et sans recadrage la
// palette les recouvre.
const LAYOUT = [
  { key: "chamfer", type: "profile.chamfer", x: 20, y: 40 },
  { key: "gothic", type: "shape.gothic.window", x: 20, y: 130 },
  { key: "smooth", type: "mesh.smooth.laplacian", x: 20, y: 280 },
  { key: "repeat", type: "flow.repeat", x: 200, y: 280 },
  { key: "font", type: "text.font.load", x: 20, y: 410 },
  { key: "extrude", type: "text.extrude", x: 200, y: 410 },
  { key: "save", type: "mesh.io.save", x: 20, y: 530 },
];

const WIRES = [
  ["chamfer", 0, "gothic", 0],
  ["gothic", 0, "smooth", 0],
  ["smooth", 0, "repeat", 0],
  ["font", 0, "extrude", 0],
];

async function run() {
  const bridge = await import("../web/js/graph-bridge.js");
  const canvas = document.getElementById("view");

  const ready = await bridge.startWorker(canvas);
  if (ready.type === "error") throw new Error("worker : " + ready.error);
  if (ready.editor !== "ok") throw new Error("graphEditorInit : " + ready.editor);
  await bridge.call({ type: "resize", width: canvas.width, height: canvas.height });
  say("editeur : " + ready.editor + " / " + ready.renderer);

  const tick = (events) => bridge.call({ type: "tick", dt: 1 / 60, events: events || null });
  const settle = async (n) => {
    for (let i = 0; i < (n || 12); ++i) await tick();
  };
  const capture = async (name) => {
    const shot = await bridge.call({ type: "shot" });
    report.probes[name] = {
      width: shot.width,
      height: shot.height,
      pngBytes: Math.round(((shot.dataUrl.length - 22) * 3) / 4),
      state: (await tick()).state,
    };
    report[name + "Png"] = shot.dataUrl;
    say(`capture ${name} : ${shot.width}x${shot.height}, ${report.probes[name].pngBytes} octets`);
  };

  // ---- avant : le graphe bati sur place, sans aucun chargement -----------
  const id = {};
  for (const node of LAYOUT) {
    const answer = await bridge.call({
      type: "addNode",
      nodeType: node.type,
      x: node.x,
      y: node.y,
    });
    if (!answer.node) throw new Error("type absent du catalogue : " + node.type);
    id[node.key] = answer.node;
  }
  for (const [from, fromPort, to, toPort] of WIRES)
    await bridge.call({ type: "connect", from: id[from], fromPort, to: id[to], toPort });

  await settle();
  await capture("avant");

  // ---- zoom : la MEME vue, zoomee a la molette, sans aucun chargement -----
  //
  // Temoin. Le recadrage change le zoom (c est assume) ; ce temoin dit ce que
  // le zoom SEUL fait au rendu, pour qu un defaut de zoom ne soit pas impute
  // au recadrage.
  await tick([{ k: "m", x: 800, y: 500 }]);
  for (let i = 0; i < 3; ++i) await tick([{ k: "w", x: 0, y: 1 }]);
  await settle(30);
  await capture("zoom");

  // ---- apres : le meme document, exporte puis RELU ------------------------
  const peuple = (await bridge.call({ type: "document" })).json;
  report.probes.document = { bytes: peuple.length };

  const chargement = await bridge.call({ type: "loadDocument", json: peuple });
  if (chargement.error) throw new Error("loadDocument : " + chargement.error);
  await settle();
  await capture("apres");

  // ---- ajout : un noeud de plus APRES le recadrage ------------------------
  //
  // Le canvas voit exactement ce qu'il verrait d'un clic dans la palette : un
  // noeud de plus dans le graphe, sans demande de cadrage. La vue doit rester
  // ou le chargement l'a mise.
  await bridge.call({ type: "addNode", nodeType: "profile.chamfer", x: 640, y: 640 });
  await settle();
  await capture("ajout");

  // ---- vide : le meme chemin sur un document sans noeud -------------------
  await bridge.call({ type: "reset" });
  const nul = (await bridge.call({ type: "document" })).json;
  report.probes.documentVide = { bytes: nul.length, json: nul };

  const chargementVide = await bridge.call({ type: "loadDocument", json: nul });
  if (chargementVide.error) throw new Error("loadDocument (vide) : " + chargementVide.error);
  await settle();
  await capture("vide");
}

run()
  .then(() => (report.ok = true))
  .catch((e) => {
    report.ok = false;
    report.error = String((e && e.stack) || e);
    say("ECHEC : " + report.error);
  })
  .finally(() => {
    fetch("/result", { method: "POST", body: JSON.stringify(report) });
  });
