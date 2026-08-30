// ===========================================================================
//  Sondes de l'etape 7 -- criteres 7.2, 7.3, 7.5, 7.6, 7.9, 7.10.
// ===========================================================================
//
// Elles tournent dans un navigateur, contre l'artefact reellement construit
// (maker/web/maker.js). Chaque sonde rend un verdict et le CHIFFRE qui le
// porte ; le tout part en POST /result, ce qui la rend rejouable sans oeil
// humain.
//
// Parametres d'URL :
//   ?obj=<url>          fichier de maillage a charger (defaut ./data/big.obj)
//   ?smooth=<n>         nombre de noeuds de lissage (defaut 7, soit 8 noeuds)
//   ?iterations=<n>     iterations par noeud de lissage (defaut 1)
//   ?budget=<octets>    budget de cache impose au worker (0 = celui de l'hote)
//   ?pages=1            charge AUSSI une instance sur le thread UI (D17)
//   ?control=<url>      CONTROLE NEGATIF de 7.2 : rejoue le meme graphe dans
//                       une instance du THREAD UI et compte les memes tours de
//                       setTimeout(0). Sans ce controle, "34 tours servis" ne
//                       prouverait rien -- il faut montrer que le meme
//                       instrument rend ZERO quand le module vit ici.
//
const params = new URLSearchParams(location.search);
const objUrl = params.get("obj") || "./data/big.obj";
const smoothCount = +(params.get("smooth") || 7);
const iterations = +(params.get("iterations") || 1);
const forcedBudget = +(params.get("budget") || 0);
const withPages = params.get("pages") === "1";
const controlObj = params.get("control") || "";
// SABOTAGE de 7.8, a la demande : charge le document du worker DANS l'instance
// des pages, ce qui donne deux copies editables du meme document. C'est le seul
// chemin qui existe -- deux instances wasm ne partagent aucune memoire --, et
// c'est celui que R-deux-instances nomme.
const shareDocument = params.get("share") === "1";

const report = { started: new Date().toISOString(), probes: {} };
const out = document.getElementById("out");

function say(text) {
  out.textContent += "\n" + text;
}

// --------------------------------------------------------------------------
//  7.10 -- inventaire des messages de la frontiere.
//
//  On remplace le constructeur Worker AVANT d'importer le pont : tout message
//  emis ou recu passe donc par ce filtre, y compris ceux que le produit envoie
//  sans que la sonde les connaisse.
// --------------------------------------------------------------------------
const traffic = { out: [], in: [] };

function describe(value, depth) {
  if (value == null) return { kind: typeof value };
  if (ArrayBuffer.isView(value))
    return { kind: value.constructor.name, bytes: value.byteLength, binary: true };
  if (value instanceof ArrayBuffer) return { kind: "ArrayBuffer", bytes: value.byteLength, binary: true };
  if (typeof ImageBitmap !== "undefined" && value instanceof ImageBitmap)
    return { kind: "ImageBitmap", binary: true };
  if (typeof OffscreenCanvas !== "undefined" && value instanceof OffscreenCanvas)
    return { kind: "OffscreenCanvas", canvas: true };
  if (typeof Blob !== "undefined" && value instanceof Blob)
    return { kind: value.constructor.name, bytes: value.size, byReference: true };
  if (Array.isArray(value)) return { kind: "Array", length: value.length };
  if (typeof value === "object" && depth < 4) {
    const fields = {};
    for (const k of Object.keys(value)) fields[k] = describe(value[k], depth + 1);
    return { kind: "object", fields };
  }
  return { kind: typeof value };
}

function record(bucket, message, transfer) {
  bucket.push({
    type: message && message.type,
    payload: describe(message, 0),
    transfer: (transfer || []).map((t) => describe(t, 0)),
    at: performance.now(),
  });
}

const NativeWorker = self.Worker;
self.Worker = class ProbedWorker extends NativeWorker {
  constructor(url, options) {
    super(url, options);
    const nativePost = NativeWorker.prototype.postMessage.bind(this);
    this.postMessage = (message, transfer) => {
      record(traffic.out, message, transfer);
      return nativePost(message, transfer);
    };
    this.addEventListener("message", (event) => record(traffic.in, event.data, []));
  }
};

// Verdict : est binaire tout message dont la charge ou la liste de transfert
// porte un ArrayBuffer, une vue typee ou un ImageBitmap. L'OffscreenCanvas du
// bootstrap est nomme a part -- c'est le transfert que D27 EXIGE, pas celui
// qu'elle refuse.
function binaryVerdict() {
  const flat = [];
  const walk = (d, path) => {
    if (!d) return;
    if (d.binary) flat.push({ path, kind: d.kind, bytes: d.bytes || 0 });
    if (d.fields) for (const k of Object.keys(d.fields)) walk(d.fields[k], path + "." + k);
  };
  const scan = (bucket, direction) => {
    for (const m of bucket) {
      walk(m.payload, direction + ":" + m.type);
      for (const t of m.transfer) walk(t, direction + ":" + m.type + ":transfer");
    }
  };
  scan(traffic.out, "ui->worker");
  scan(traffic.in, "worker->ui");
  const canvases = traffic.out
    .flatMap((m) => m.transfer.filter((t) => t.canvas).map(() => m.type));
  return {
    messagesOut: traffic.out.length,
    messagesIn: traffic.in.length,
    binaryPayloads: flat,
    binaryBytesTotal: flat.reduce((a, b) => a + (b.bytes || 0), 0),
    offscreenCanvasTransfers: canvases,
  };
}

// --------------------------------------------------------------------------
async function run() {
  const bridge = await import("../web/js/graph-bridge.js");
  const canvas = document.getElementById("view");

  // ---- 7.9 : le canvas est TRANSFERE ------------------------------------
  const ready = await bridge.startWorker(canvas);
  if (ready.type === "error") throw new Error("worker : " + ready.error);

  // Apres un transfert, le thread UI ne peut plus obtenir de contexte : le
  // navigateur leve InvalidStateError. Un canvas encore dessinable ici
  // refuterait D27.
  let uiContext = "obtenu";
  try {
    uiContext = canvas.getContext("2d") ? "obtenu" : "null";
  } catch (e) {
    uiContext = e.name;
  }
  report.probes["7.9"] = {
    transferControlToOffscreen: true,
    handleInTransferList: binaryVerdict().offscreenCanvasTransfers,
    uiThreadContextAfterTransfer: uiContext,
    verdict: uiContext !== "obtenu" ? "transfere" : "NON TRANSFERE",
  };

  // ---- 7.3 : OffscreenCanvas + WebGL2 dans le worker ---------------------
  report.probes["7.3"] = {
    glVersion: ready.glVersion,
    renderer: ready.renderer,
    verdict: /WebGL 2/.test(ready.glVersion || "") ? "webgl2 dans le worker" : "ECHEC",
    note: /SwiftShader|ANGLE \(Google/.test(ready.renderer || "")
      ? "rendu logiciel -- la reserve de nodal.md 9.3 tient"
      : "renderer non logiciel",
  };
  say("worker : " + ready.glVersion + " / " + ready.renderer);

  if (forcedBudget > 0) await bridge.call({ type: "budget", bytes: forcedBudget });

  // ---- Instance des PAGES sur le thread UI (D17) -------------------------
  let pagesHeap = 0;
  let pagesInstance = null;
  let pagesShapeId = -1;
  if (withPages) {
    const createMakerModule = (await import("../web/maker.js")).default;
    const pages = await createMakerModule();
    // Une action representative d'une page de formes : instancier et generer.
    const id = pages.createShape("Sphere");
    pages.setParam(id, "Nu", 200);
    pages.setParam(id, "Nv", 200);
    pages.meshData(id);
    pagesHeap = pages.heapBytes();
    pagesInstance = pages;
    pagesShapeId = id;
    say("instance des pages (thread UI) : " + (pagesHeap / 1048576).toFixed(1) + " Mio");
  }

  // ---- Construction du graphe de 8 noeuds --------------------------------
  const t0 = performance.now();
  // URL ABSOLUE : c'est le worker qui fetch, et son URL de base est
  // /web/js/graph-worker.js, pas celle de cette page.
  const absoluteObj = new URL(objUrl, location.href).href;
  const file = await bridge.call({ type: "fetchFile", url: absoluteObj, path: "/tmp/big.obj" });
  if (file.type === "error") throw new Error(file.error);
  say(`${absoluteObj} : ${(file.bytes / 1048576).toFixed(1)} Mio dans le MEMFS du worker`);

  const load = (await bridge.call({ type: "addNode", nodeType: "mesh.io.load" })).node;
  await bridge.call({ type: "setParam", node: load, name: "path", valueType: "string", value: "/tmp/big.obj" });

  let previous = load;
  const chain = [load];
  for (let i = 0; i < smoothCount; ++i) {
    const smooth = (await bridge.call({ type: "addNode", nodeType: "mesh.smooth.laplacian" })).node;
    await bridge.call({ type: "setParam", node: smooth, name: "iterations", valueType: "int", value: iterations });
    // lambda distinct par noeud : deux noeuds identiques auraient la MEME
    // signature, donc une seule entree de cache -- la mesure porterait alors
    // sur un seul maillage vivant, et elle s'ignorerait.
    await bridge.call({ type: "setParam", node: smooth, name: "lambda", valueType: "float", value: 0.1 + 0.05 * i });
    const status = (await bridge.call({ type: "connect", from: previous, fromPort: 0, to: smooth, toPort: 0 })).status;
    if (status !== "ok") throw new Error(`connexion ${previous}->${smooth} : ${status}`);
    previous = smooth;
    chain.push(smooth);
  }

  // ---- 7.2 : le module ne vit pas sur le thread UI -----------------------
  // Un setTimeout de 0 ms qui se replante. On compte ses tours PENDANT
  // l'evaluation : zero tour signifierait que le calcul bloque cette boucle.
  let ticks = 0;
  let ticking = true;
  const tick = () => {
    if (!ticking) return;
    ++ticks;
    setTimeout(tick, 0);
  };
  tick();

  const before = performance.now();
  const answer = await bridge.call({ type: "evaluate", node: previous });
  const wall = performance.now() - before;
  ticking = false;
  if (answer.type === "error") throw new Error(answer.error);

  report.probes["7.2"] = {
    evaluationMs: Math.round(wall),
    uiTicksDuringEvaluation: ticks,
    verdict: ticks > 0 ? "thread UI servi pendant le calcul" : "THREAD UI BLOQUE",
  };

  // CONTROLE NEGATIF : le meme graphe, la meme mesure, mais dans une instance
  // qui vit sur CE thread. Si l'instrument y rendait lui aussi des tours, il ne
  // mesurerait pas ce qu'il annonce.
  if (controlObj) {
    const createMakerModule = (await import("../web/maker.js")).default;
    const here = await createMakerModule();
    const bytes = new Uint8Array(await (await fetch(controlObj)).arrayBuffer());
    here.FS.writeFile("/tmp/control.obj", bytes);
    const source = here.graphAddNode("mesh.io.load", 0, 0);
    here.graphSetString(source, "path", "/tmp/control.obj");
    let last = source;
    for (let i = 0; i < smoothCount; ++i) {
      const smooth = here.graphAddNode("mesh.smooth.laplacian", 0, 0);
      here.graphSetFloat(smooth, "lambda", 0.1 + 0.05 * i);
      here.graphConnect(last, 0, smooth, 0);
      last = smooth;
    }
    let controlTicks = 0;
    let controlTicking = true;
    const controlTick = () => { if (controlTicking) { ++controlTicks; setTimeout(controlTick, 0); } };
    controlTick();
    // Laisse la boucle d'evenements demarrer le compteur avant de la bloquer.
    await new Promise((r) => setTimeout(r, 50));
    const startedAt = controlTicks;
    const t = performance.now();
    const controlResult = JSON.parse(here.graphEvaluate(last));
    const controlMs = performance.now() - t;
    controlTicking = false;
    report.probes["7.2-controle"] = {
      obj: controlObj,
      evaluationMs: Math.round(controlMs),
      uiTicksDuringEvaluation: controlTicks - startedAt,
      status: controlResult.status,
      verdict:
        controlTicks - startedAt === 0
          ? "instrument valide : sur le thread UI il rend ZERO"
          : "INSTRUMENT DECORATIF : il rend des tours meme quand le module vit ici",
    };
    say(`controle : ${Math.round(controlMs)} ms sur le thread UI, ${controlTicks - startedAt} tours`);
  }
  say(`evaluation ${Math.round(wall)} ms, ${ticks} tours de setTimeout(0) sur le thread UI`);

  // ---- 7.4 : pompage de frame -------------------------------------------
  report.probes["7.4"] = {
    progressTicks: answer.result.ticks,
    pumpedFrames: answer.pumpedFrames,
    pixelsAfterFrame: answer.pixels,
    verdict:
      answer.pumpedFrames > 0 && answer.pixels.drawn > 0
        ? "frames pompees depuis progress(), et le rendu a bien ecrit des pixels"
        : answer.pumpedFrames > 0
          ? "frames pompees, mais AUCUN PIXEL ecrit"
          : "AUCUNE FRAME POMPEE",
  };

  // ---- 7.5 : empreinte ---------------------------------------------------
  const stats = await bridge.call({ type: "stats" });
  report.probes["7.5"] = {
    nodes: chain.length,
    triangles: answer.nf,
    vertices: answer.nv,
    graphHeapBytes: stats.heap,
    pagesHeapBytes: pagesHeap,
    totalHeapBytes: stats.heap + pagesHeap,
    memfsFileBytes: file.bytes,
    cache: stats.cache,
    evaluationMs: Math.round(wall),
    buildMs: Math.round(performance.now() - t0),
  };
  say(
    `tas du graphe ${(stats.heap / 1048576).toFixed(1)} Mio, ` +
      `cache ${(stats.cache.bytes / 1048576).toFixed(1)}/${(stats.cache.budget / 1048576).toFixed(1)} Mio, ` +
      `${stats.cache.entries} entrees, ${stats.cache.evictions} evictions`
  );

  // ---- 7.6 : budget de cache --------------------------------------------
  const engineDefault = 256 * 1024 * 1024;
  report.probes["7.6"] = {
    hostBudgetBytes: stats.cache.budget,
    engineDefaultBytes: engineDefault,
    cacheBytes: stats.cache.bytes,
    entries: stats.cache.entries,
    evictions: stats.cache.evictions,
    verdict:
      forcedBudget > 0
        ? "budget impose par la sonde -- ne dit rien de l'hote"
        : stats.cache.budget !== engineDefault
          ? "budget pose PAR L'HOTE, distinct du defaut du moteur"
          : "L'HOTE N'A RIEN POSE : le defaut du moteur passe tel quel",
  };

  // ---- 7.8 : deux instances, aucun document commun (D17) -----------------
  // Falsifiable : si les deux instances designaient le meme objet editable, le
  // graphe construit dans le worker apparaitrait dans le document de
  // l'instance des pages. C'est le seul chemin par lequel elles pourraient se
  // rencontrer, l'API du module etant la seule surface partagee.
  if (pagesInstance) {
    const workerJson = (await bridge.call({ type: "document" })).json;
    if (shareDocument) pagesInstance.graphFromJson(workerJson);
    const workerDoc = JSON.parse(workerJson);
    const pagesDoc = JSON.parse(pagesInstance.graphToJson());
    // Et le symetrique, pris dans l'autre sens pour qu'aucune coincidence
    // d'identifiants ne le rende vrai par hasard : le dernier noeud du graphe
    // du worker interroge dans l'instance des pages.
    const lastNodeSeenByPages = JSON.parse(pagesInstance.graphNodeInfo(previous));
    report.probes["7.8"] = {
      workerDocumentNodes: workerDoc.nodes.length,
      pagesDocumentNodes: pagesDoc.nodes.length,
      pagesShapeId,
      sabotageShareDocument: shareDocument,
      workerLastNodeVisibleFromPages: lastNodeSeenByPages !== null,
      verdict:
        workerDoc.nodes.length > 0 && pagesDoc.nodes.length === 0 && lastNodeSeenByPages === null
          ? "deux documents disjoints -- la premisse de R-deux-instances tient"
          : "DOCUMENT COMMUN : la premisse de R-deux-instances est refutee",
    };
    say(`7.8 : ${workerDoc.nodes.length} noeuds cote worker, ${pagesDoc.nodes.length} cote pages`);
  }

  // ---- 7.10 : rien de binaire sur la frontiere ---------------------------
  const verdict = binaryVerdict();
  report.probes["7.10"] = Object.assign(verdict, {
    verdict: verdict.binaryPayloads.length === 0 ? "aucune geometrie sur la frontiere" : "GEOMETRIE SUR LA FRONTIERE",
  });

  report.ok = true;
}

run()
  .catch((e) => {
    report.ok = false;
    report.error = String((e && e.stack) || e);
    say("ECHEC : " + report.error);
  })
  .finally(async () => {
    out.textContent = JSON.stringify(report, null, 2);
    try {
      await fetch("/result", { method: "POST", body: JSON.stringify(report, null, 2) });
    } catch (e) {
      /* pas de serveur de sonde : le rapport reste a l'ecran */
    }
  });
