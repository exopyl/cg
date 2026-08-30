// ===========================================================================
//  Sonde de l'editeur nodal dans le worker
// ===========================================================================
//
// Tout ce que ce portage ajoute vit sur une cible que `TU` ne lie pas : le
// contexte GL partage, la frame ImGui, le routage d'evenements. Cette sonde est
// donc HORS CI, et c'est dit ici plutot que suppose. Elle etablit, contre
// l'artefact reellement construit :
//
//   E1  le contexte GL du C++ est CELUI du JS -- un seul contexte, un canvas ;
//   E2  une frame ImGui est rendue, et des pixels sont ecrits ;
//   E3  un geste de souris atteint ImGui (WantCaptureMouse) ;
//   E4  un CLIC atteint un bouton de la palette et cree un noeud -- la chaine
//       entiere, de l'evenement DOM a EditorModel::AddNode ;
//   E5  le clavier atteint un champ de texte (WantTextInput apres un clic
//       dedans, puis le caractere saisi) ;
//   E6  7.10 tient : le trafic de la frontiere, ticks et entrees compris, ne
//       porte aucune geometrie. L'instrument est VALIDE sur un cas positif
//       avant de rendre son zero (?positive=1).
//
// Parametres d'URL :
//   ?positive=1   valide l'instrument de 7.10 : envoie deliberement un
//                 ArrayBuffer par la frontiere, et la sonde doit le VOIR.
//   ?frames=<n>   nombre de frames pompees avant les gestes (defaut 8).
//
const params = new URLSearchParams(location.search);
const positive = params.get("positive") === "1";
const warmFrames = +(params.get("frames") || 8);

const report = { started: new Date().toISOString(), probes: {} };
const out = document.getElementById("out");
const say = (text) => (out.textContent += "\n" + text);

// --------------------------------------------------------------------------
//  Inventaire du trafic -- meme principe que la sonde de l'etape 7 : on
//  remplace le constructeur Worker AVANT d'importer le pont, si bien que tout
//  message emis ou recu passe par ce filtre, y compris ceux que le produit
//  envoie sans que la sonde les connaisse.
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
  if (Array.isArray(value))
    return { kind: "Array", length: value.length, items: value.slice(0, 3).map((v) => describe(v, depth + 1)) };
  if (typeof value === "object" && depth < 5) {
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

function binaryVerdict() {
  const flat = [];
  const walk = (d, path) => {
    if (!d) return;
    if (d.binary) flat.push({ path, kind: d.kind, bytes: d.bytes || 0 });
    if (d.fields) for (const k of Object.keys(d.fields)) walk(d.fields[k], path + "." + k);
    if (d.items) d.items.forEach((it, i) => walk(it, path + "[" + i + "]"));
  };
  const scan = (bucket, direction) => {
    for (const m of bucket) {
      walk(m.payload, direction + ":" + m.type);
      for (const t of m.transfer) walk(t, direction + ":" + m.type + ":transfer");
    }
  };
  scan(traffic.out, "ui->worker");
  scan(traffic.in, "worker->ui");
  const canvases = traffic.out.flatMap((m) => m.transfer.filter((t) => t.canvas).map(() => m.type));
  const byType = {};
  for (const m of traffic.out) byType["ui->worker:" + m.type] = (byType["ui->worker:" + m.type] || 0) + 1;
  for (const m of traffic.in) byType["worker->ui:" + m.type] = (byType["worker->ui:" + m.type] || 0) + 1;
  return {
    messagesOut: traffic.out.length,
    messagesIn: traffic.in.length,
    byType,
    binaryPayloads: flat,
    binaryBytesTotal: flat.reduce((a, b) => a + (b.bytes || 0), 0),
    offscreenCanvasTransfers: canvases,
  };
}

// --------------------------------------------------------------------------
//  E0 -- la premisse de la cadence, MESUREE et non citee.
//
//  Le choix « le thread UI poste un tick a chaque rAF » repose sur une
//  affirmation : requestAnimationFrame n'existe pas dans un Worker. Une
//  affirmation negative ne se constate pas par lecture de specification ; on la
//  mesure, dans un worker jetable, et on la compare a ce que le meme test rend
//  sur ce thread-ci -- sans ce second terme, « undefined » ne distinguerait pas
//  l'absence de l'API d'une erreur de la sonde.
// --------------------------------------------------------------------------
function probeWorkerGlobals() {
  // ⚠ « LE SYMBOLE EXISTE » N'EST PAS « LA FONCTION SERT ». Un rAF declare mais
  // jamais rappele donnerait une boucle qui ne tourne pas -- et le premier jet
  // de cette sonde s'arretait a typeof, ce qui aurait fait conclure a l'inverse
  // de la verite. On COMPTE donc les rappels, sur une duree connue, et on donne
  // le meme test a setTimeout comme temoin : si les deux rendent zero, c'est la
  // sonde qui est fausse, pas l'API.
  const source =
    "let raf = 0, timer = 0, done = false;" +
    "const stop = () => { if (done) return; done = true;" +
    "  self.postMessage({ rAF: 'function', rafCalls: raf, timerCalls: timer," +
    "    document: typeof document, OffscreenCanvas: typeof OffscreenCanvas }); };" +
    "if (typeof requestAnimationFrame === 'function') {" +
    "  const step = () => { ++raf; requestAnimationFrame(step); };" +
    "  requestAnimationFrame(step);" +
    "} else {" +
    "  self.postMessage({ rAF: 'undefined', rafCalls: 0, timerCalls: 0," +
    "    document: typeof document, OffscreenCanvas: typeof OffscreenCanvas });" +
    "  done = true;" +
    "}" +
    "const tick = () => { ++timer; setTimeout(tick, 16); };" +
    "setTimeout(tick, 16);" +
    "setTimeout(stop, 1000);";
  const url = URL.createObjectURL(new Blob([source], { type: "text/javascript" }));
  return new Promise((resolve) => {
    // NativeWorker : le constructeur instrumente plus haut compterait ce worker
    // jetable dans le trafic de 7.10, ce qui fausserait l'inventaire.
    const w = new NativeWorker(url);
    w.onmessage = (e) => {
      w.terminate();
      URL.revokeObjectURL(url);
      resolve(e.data);
    };
  });
}

// --------------------------------------------------------------------------
async function run() {
  const inWorker = await probeWorkerGlobals();
  // Le meme comptage sur CE thread, comme terme de comparaison. Sans lui,
  // « n rappels dans le worker » ne se compare a rien.
  let pageCalls = 0;
  await new Promise((resolve) => {
    const step = () => { ++pageCalls; requestAnimationFrame(step); };
    requestAnimationFrame(step);
    setTimeout(resolve, 1000);
  });
  const inPage = { rAF: typeof requestAnimationFrame, rafCalls: pageCalls,
                   document: typeof document, OffscreenCanvas: typeof OffscreenCanvas };
  report.probes.E0 = {
    inWorker,
    inPage,
    verdict:
      inWorker.rAF !== "function"
        ? "rAF ABSENT du worker : la cadence ne peut venir que du thread UI"
        : inWorker.rafCalls === 0
          ? "rAF DECLARE dans le worker mais JAMAIS RAPPELE : le symbole ment"
          : "rAF EXISTE ET SERT dans le worker (" + inWorker.rafCalls + " rappels/s) -- "
            + "la premisse « rAF n'existe pas dans un Worker » est FAUSSE ici",
  };
  say("cadence : rAF page=" + pageCalls + "/s, worker=" + inWorker.rafCalls + "/s"
      + " (setTimeout worker=" + inWorker.timerCalls + "/s)");

  const bridge = await import("../web/js/graph-bridge.js");
  const canvas = document.getElementById("view");
  canvas.width = 1200;
  canvas.height = 800;

  const ready = await bridge.startWorker(canvas);
  if (ready.type === "error") throw new Error("worker : " + ready.error);
  await bridge.call({ type: "resize", width: 1200, height: 800 });

  report.probes.E1 = {
    glVersion: ready.glVersion,
    renderer: ready.renderer,
    editorInit: ready.editor,
    catalogSize: ready.catalog.length,
    verdict: ready.editor === "ok" ? "un contexte, deux dessinateurs" : "ECHEC : " + ready.editor,
  };
  say("editeur : " + ready.editor + " / " + ready.glVersion + " / " + ready.renderer);
  if (ready.editor !== "ok") throw new Error("graphEditorInit : " + ready.editor);

  const tick = (events) => bridge.call({ type: "tick", dt: 1 / 60, events: events || null });

  // ---- E2 : une frame est rendue -----------------------------------------
  let last = null;
  for (let i = 0; i < warmFrames; ++i) last = await tick();
  const pixels = (await bridge.call({ type: "pixels" })).pixels;
  report.probes.E2 = {
    frames: warmFrames,
    lastState: last && last.state,
    pixels,
    verdict: pixels.drawn > 0 ? "des pixels sont ecrits" : "CADRE VIDE",
  };
  say(`frames : ${warmFrames} — pixels ecrits ${pixels.drawn}/${pixels.sampled}`);

  // ---- E3 : un deplacement atteint ImGui ---------------------------------
  // (600, 3) est AU-DESSUS de toutes les fenetres : la disposition de premier
  // usage du canvas les pose a y = 10. Le premier point choisi -- (1190, 790) --
  // etait DANS l'inspecteur, et la sonde rendait donc vrai des deux cotes : un
  // controle mal place ne distingue rien, et il le disait.
  const outside = await tick([{ k: "m", x: 600, y: 3 }]);
  const inside = await tick([{ k: "m", x: 150, y: 60 }]);
  report.probes.E3 = {
    wantMouseOutsideAnyWindow: outside.state.wantMouse,
    wantMouseOverPalette: inside.state.wantMouse,
    verdict:
      inside.state.wantMouse && !outside.state.wantMouse
        ? "le pointeur atteint ImGui, et son etat DISTINGUE les deux positions"
        : "le pointeur n'atteint pas ImGui, ou l'etat ne distingue rien",
  };
  say(
    `pointeur : hors fenetre wantMouse=${outside.state.wantMouse}, ` +
      `sur la palette wantMouse=${inside.state.wantMouse}`
  );

  // ---- E4 : un CLIC cree un noeud ----------------------------------------
  //
  // La position des boutons n'est pas connue de l'exterieur -- c'est ImGui qui
  // dispose. On BALAYE donc la colonne de la palette, un clic par point, et on
  // s'arrete au premier qui fait grandir le document. Trouver un y qui marche
  // est la mesure ; ne rien trouver refuterait le routage.
  let created = 0;
  let clickedAt = null;
  for (let y = 40; y < 700 && created === 0; y += 6) {
    await tick([{ k: "m", x: 150, y }, { k: "b", b: 0, d: true }]);
    const up = await tick([{ k: "b", b: 0, d: false }]);
    if (up.state.nodes > 0) {
      created = up.state.nodes;
      clickedAt = y;
    }
  }
  const afterClick = await tick();
  report.probes.E4 = {
    clickedAt,
    nodes: afterClick.state.nodes,
    selection: afterClick.state.selection,
    verdict: created > 0 ? "le clic atteint un bouton de la palette" : "AUCUN CLIC N'A RIEN CREE",
  };
  say(`clic : y=${clickedAt} -> ${afterClick.state.nodes} noeud(s), selection #${afterClick.state.selection}`);

  // ---- E5 : le clavier et le TEXTE ---------------------------------------
  //
  // L'inspecteur du noeud selectionne porte des champs. On balaye sa colonne a
  // la recherche d'un champ de texte : WantTextInput ne devient vrai que
  // lorsqu'ImGui a effectivement pris le focus dans un InputText.
  let textAt = null;
  for (let y = 40; y < 700 && textAt === null; y += 6) {
    await tick([{ k: "m", x: 1150, y }, { k: "b", b: 0, d: true }]);
    const up = await tick([{ k: "b", b: 0, d: false }]);
    if (up.state.wantText) textAt = y;
  }
  let typed = null;
  if (textAt !== null) {
    // Un caractere, puis Backspace : le premier passe par AddInputCharacter, le
    // second par la touche. Les deux chemins sont distincts, et c'est pour cela
    // qu'ils sont eprouves tous les deux.
    await tick([
      { k: "t", cp: "Z".codePointAt(0) },
      { k: "k", c: "KeyZ", d: true },
      { k: "k", c: "KeyZ", d: false },
    ]);
    const back = await tick([
      { k: "k", c: "Backspace", d: true },
      { k: "k", c: "Backspace", d: false },
    ]);
    typed = { wantKeyboard: back.state.wantKeyboard, wantText: back.state.wantText };
  }
  report.probes.E5 = {
    textFieldFoundAt: textAt,
    afterTyping: typed,
    verdict:
      textAt !== null && typed && typed.wantText
        ? "un champ de texte a le focus et garde le clavier"
        : "AUCUN CHAMP DE TEXTE N'A PRIS LE FOCUS",
  };
  say(`clavier : champ de texte a y=${textAt}, apres saisie ${JSON.stringify(typed)}`);

  // ---- E7 : le calcul passe par la BOUCLE DE FRAMES, et le pompage tourne --
  //
  // Le noeud cree au clic alimente un lissage assez long pour que le collecteur
  // de progression soit appele : c'est la seule facon d'observer que le pompage
  // reemet des frames PENDANT un calcul. La chaine est batie par la facade
  // procedurale -- meme document, c'est tout l'interet de graph_host.h.
  const shape = afterClick.state.selection;
  const smooth = (await bridge.call({ type: "addNode", nodeType: "mesh.smooth.laplacian" })).node;
  await bridge.call({ type: "setParam", node: smooth, name: "iterations", valueType: "int", value: 40 });
  const wired = (await bridge.call({ type: "connect", from: shape, fromPort: 0, to: smooth, toPort: 0 })).status;

  const before = await tick();
  await bridge.call({ type: "request", node: smooth });

  // La fenetre de coalescence vaut 150 ms : les premiers ticks ne servent rien,
  // et c'est le comportement voulu. On tourne jusqu'a ce que la revision bouge.
  let served = null;
  for (let i = 0; i < 300 && served === null; ++i) {
    const answer = await tick();
    if (answer.revision !== before.revision) served = answer;
  }
  report.probes.E7 = {
    connect: wired,
    revisionBefore: before.revision,
    revisionAfter: served && served.revision,
    pumpedFramesBefore: before.pumpedFrames,
    pumpedFramesAfter: served && served.pumpedFrames,
    uploaded: served && served.uploaded,
    verdict:
      served && served.pumpedFrames > before.pumpedFrames
        ? "le calcul passe par la boucle de frames, et le pompage a reemis des frames"
        : served
          ? "le calcul est passe, mais AUCUNE frame n'a ete pompee"
          : "AUCUN RESULTAT N'EST ARRIVE PAR LA BOUCLE",
  };
  say(
    `boucle : revision ${before.revision} -> ${served && served.revision}, ` +
      `frames pompees ${before.pumpedFrames} -> ${served && served.pumpedFrames}`
  );

  // Quelques frames de plus : l'apercu televerse doit se voir.
  for (let i = 0; i < 4; ++i) await tick();
  const afterEval = (await bridge.call({ type: "pixels" })).pixels;
  report.probes.preview = {
    pixels: afterEval,
    heap: (await bridge.call({ type: "stats" })).heap,
    verdict: afterEval.drawn > 0 ? "la scene et l'interface cohabitent" : "CADRE VIDE",
  };

  // ---- Validation de l'instrument de 7.10, sur un cas POSITIF ------------
  if (positive) {
    // Un message deliberement binaire. S'il n'apparait PAS dans le verdict,
    // c'est l'instrument qui est faux, et son zero ne vaut rien.
    await bridge.call({ type: "inconnu-sonde", payload: new Uint8Array(4096) });
  }

  report.probes.E6 = binaryVerdict();
  report.probes.E6.positiveControl = positive;
  report.probes.E6.verdict =
    report.probes.E6.binaryPayloads.length === 0
      ? "aucune geometrie, aucun binaire sur la frontiere"
      : "binaire vu : " + JSON.stringify(report.probes.E6.binaryPayloads);
  say("frontiere : " + report.probes.E6.verdict);
  say(`messages : ${report.probes.E6.messagesOut} sortants, ${report.probes.E6.messagesIn} entrants`);
}

run()
  .then(() => (report.status = "ok"))
  .catch((e) => {
    report.status = "echec";
    report.error = String((e && e.stack) || e);
    say("ECHEC : " + report.error);
  })
  .finally(() => {
    fetch("/result", { method: "POST", body: JSON.stringify(report, null, 2) });
  });
