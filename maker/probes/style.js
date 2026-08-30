// ===========================================================================
//  Sonde de RENDU du canvas nodal -- une capture, pas un verdict
// ===========================================================================
//
// ⚠ CETTE SONDE NE CONCLUT RIEN TOUTE SEULE, et c'est dit ici plutot que
// suppose. Le rendu d'un editeur ne se verifie pas par une assertion : un
// canvas peut ecrire tous ses pixels et rester illisible. Ce que cette sonde
// fait, c'est BATIR un graphe qui contient tous les cas que le dessin doit
// distinguer, puis rendre l'image -- le juge est un oeil humain, et il n'y en a
// pas dans un navigateur sans tete.
//
// Elle est HORS CI, comme editor.js et pour la meme raison : elle s'execute
// contre l'artefact WebAssembly, que `TU` ne lie pas.
//
// Le graphe bati contient, deliberement :
//
//   - cinq CATEGORIES du catalogue (Formes, Profil, Maillage, Texte, Flux),
//     donc cinq teintes d'en-tete ;
//   - trois TYPES sur les liens (cgmesh.Mesh, cgmesh.Profile2D.splay,
//     cgmath.Font), donc trois couleurs de trait ;
//   - un port OPTIONNEL alimente et un port OPTIONNEL libre sur le meme noeud
//     (les deux profils de la fenetre gothique) ;
//   - un port OBLIGATOIRE libre (l'entree de l'enregistrement), donc un noeud
//     qui n'est pas pret.
//
const report = { started: new Date().toISOString(), probes: {} };
const out = document.getElementById("out");
const say = (text) => (out.textContent += "\n" + text);

// Positions en coordonnees de l'EDITEUR, non de l'ecran : c'est ce que
// graphAddNode attend, et c'est ce que le document serialise.
//
// Depuis la correction de dimensionnement, le graphe est le FOND de
// l'affichage : son origine coincide avec celle du cadre, et l'abscisse d'un
// noeud est donc son abscisse a l'ecran (zoom 1, vue non deplacee). La palette
// et l'inspecteur FLOTTENT au-dessus -- 10..290 a gauche, 1290..1590 a droite
// pour un cadre de 1600 --, et la mise en place se tient entre les deux pour
// que la capture montre les noeuds plutot que leur cachette.
const LAYOUT = [
  { key: "chamfer", type: "profile.chamfer", x: 330, y: 45 },
  { key: "gothic", type: "shape.gothic.window", x: 330, y: 135 },
  { key: "smooth", type: "mesh.smooth.laplacian", x: 330, y: 290 },
  { key: "repeat", type: "flow.repeat", x: 560, y: 290 },
  { key: "font", type: "text.font.load", x: 330, y: 420 },
  { key: "extrude", type: "text.extrude", x: 560, y: 420 },
  { key: "save", type: "mesh.io.save", x: 330, y: 540 },
];

// from -> to. Ce qui n'est PAS cable compte autant : l'entree de `save` reste
// libre, et le second profil de `gothic` aussi.
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

  // ⚠ CE QUE CETTE MISE EN PLACE SUPPOSE. La zone de dessin de l'editeur
  // couvrait 435 px pour 744 px de fenetre disponibles -- mesure a la colonne
  // de pixels, grille de x = 309 a x = 743, fond de fenetre au-dela. Elle
  // couvre desormais le cadre entier, et les positions ci-dessus sont ecrites
  // pour cette disposition-la : elles sont a la fois des coordonnees d'editeur
  // et des coordonnees d'ecran.
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

  const wired = [];
  for (const [from, fromPort, to, toPort] of WIRES) {
    const answer = await bridge.call({
      type: "connect",
      from: id[from],
      fromPort,
      to: id[to],
      toPort,
    });
    wired.push(`${from}:${fromPort} -> ${to}:${toPort} = ${answer.status}`);
  }
  say(wired.join("\n"));

  // Les categories du catalogue, telles que l'en-tete les teinte. On les rend
  // dans le rapport pour que la lecture de l'image sache ce qu'elle regarde.
  const catalog = ready.catalog;
  const byType = {};
  for (const entry of catalog) byType[entry.type] = entry;
  report.probes.graph = {
    nodes: LAYOUT.map((n) => ({
      key: n.key,
      type: n.type,
      label: byType[n.type] && byType[n.type].label,
      category: byType[n.type] && byType[n.type].category,
      id: id[n.key],
    })),
    wires: wired,
  };

  for (let i = 0; i < 12; ++i) await tick();

  // Un clic sur l'en-tete d'un noeud : il fait apparaitre a la fois la bordure
  // de selection et le panneau de l'inspecteur, donc deux etats de plus dans la
  // meme image. La position est CALCULEE puis VERIFIEE -- le graphe occupant le
  // cadre depuis (0, 0), la coordonnee d'editeur EST la coordonnee d'ecran, et
  // il ne reste qu'a viser l'interieur du bandeau de titre du noeud.
  const smooth = LAYOUT.find((n) => n.key === "smooth");
  const target = { x: smooth.x + 55, y: smooth.y + 10 };
  await tick([{ k: "m", x: target.x, y: target.y }, { k: "b", b: 0, d: true }]);
  const clicked = await tick([{ k: "b", b: 0, d: false }]);
  for (let i = 0; i < 6; ++i) await tick();

  const last = await tick();
  report.probes.state = {
    clickedAt: target,
    selection: last.state.selection,
    selectionIsSmooth: last.state.selection === id.smooth,
    nodes: last.state.nodes,
  };
  say(`selection apres clic : #${last.state.selection} (smooth = #${id.smooth})`);

  const counted = (await bridge.call({ type: "pixels" })).pixels;
  const shot = await bridge.call({ type: "shot" });
  report.probes.render = {
    pixels: counted,
    width: shot.width,
    height: shot.height,
    pngBytes: Math.round(((shot.dataUrl.length - 22) * 3) / 4),
  };
  report.png = shot.dataUrl;
  say(`capture : ${shot.width}x${shot.height}, ${report.probes.render.pngBytes} octets PNG`);
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
