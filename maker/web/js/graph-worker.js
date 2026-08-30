// ===========================================================================
//  Worker du graphe nodal -- il possede le CALCUL et le RENDU (D27).
// ===========================================================================
//
// L'OffscreenCanvas lui est TRANSFERE par graph-host.js ; a partir de la, le
// thread UI n'a plus aucun moyen de dessiner dessus. C'est ce qui rend la
// propriete verifiable plutot que promise.
//
// AUCUNE GEOMETRIE NE TRAVERSE LA FRONTIERE. Ce fichier lit les sommets et les
// triangles par graphMeshView(), qui rend des vues typees sur le tas du module
// -- dans CE thread -- et les televerse directement dans WebGL2. Les messages
// renvoyes a la page ne portent que des comptes et des diagnostics. Un maillage,
// un tampon de sommets ou une image dans un postMessage refuterait D27.
//
// INSTANCE PROPRE (D17) : ce worker charge sa propre instance de maker.js. Les
// six pages de formes gardent la leur sur le thread UI, inchangees, et les deux
// documents sont disjoints -- des formes parametrees d'un cote, un DAG de
// l'autre.
//
// MONO-FIL : graphEvaluate() est SYNCHRONE. Pendant qu'il tourne, ce worker ne
// traite aucun message -- c'est le gel assume de l'option (a). La page, elle,
// reste vivante ; c'est tout ce que (a) promet.
//
// ---------------------------------------------------------------------------
//  L'EDITEUR NODAL VIT ICI, et la cadence lui vient du thread UI
// ---------------------------------------------------------------------------
//
// LA CADENCE VIENT DU THREAD UI : un tick par requestAnimationFrame.
//
// ⚠ CE N'EST PAS PARCE QUE rAF MANQUERAIT ICI. On lit souvent que
// requestAnimationFrame n'existe pas dans un Worker ; MESURE sur cette machine
// (Chrome, sonde maker/probes/editor.js, verdict E0), c'est FAUX : le symbole
// existe dans un DedicatedWorker ET il est rappele -- 130 fois par seconde,
// autant que sur le thread UI. La raison du choix est donc ailleurs, et il faut
// qu'elle soit dite juste :
//
//  1. LE TICK PORTE DEJA LES ENTREES. Les evenements de souris et de clavier
//     n'existent que sur le thread UI : quelque chose doit traverser a chaque
//     frame de toute facon. Les faire voyager AVEC la demande de frame donne un
//     message par frame et par sens. Une horloge dans le worker n'aurait rien
//     supprime -- elle aurait ajoute une seconde source a cote d'un flux
//     d'entrees qui, lui, resterait ;
//  2. PORTABILITE. Le rAF de worker est mesure sur UN navigateur, UNE version.
//     Celui du thread UI est universel. Un editeur dont la boucle depend d'une
//     API recente s'arreterait ailleurs sans rien dire ;
//  3. le tick ATTEND SA REPONSE (bridge.call, non bridge.send). Pendant un
//     calcul long, le worker ne repond pas, donc le thread UI ne poste pas de
//     nouveau tick : le retard ne s'accumule pas.
//
// ⚠ NON MESURE, et donc NON INVOQUE : ce que devient le rAF d'un worker quand
// l'onglet est cache ou occulte. C'est l'argument qui trancherait le mieux, et
// il n'a pas ete pris.
//
// ⚠ ET LE POMPAGE DE FRAME NE MARCHE PAS SUR LES PIEDS DE CETTE CADENCE.
// Les deux ne produisent pas la meme chose : le TICK est le seul a construire
// une frame ImGui (NewFrame / Draw / Render) ; le POMPAGE, appele depuis
// l'interieur d'un calcul, ne fait que REEMETTRE la liste de dessin deja
// construite, par-dessus un apercu 3D redessine. Un producteur, un
// re-emetteur ; ils ne peuvent pas se croiser, le calcul ayant lieu HORS frame
// (graphEditorPump).
//
import createMakerModule from "../maker.js";

let Module = null;
let canvas = null;
let gl = null;
let program = null;
let vao = null;
let posBuffer = null;
let idxBuffer = null;
let indexCount = 0;
let uniforms = {};
let center = [0, 0, 0];
let radius = 1;
let spin = 0;

// --------------------------------------------------------------------------
//  WebGL2 : un programme, un VAO, pas de bibliotheque.
// --------------------------------------------------------------------------
const VERT = `#version 300 es
precision highp float;
layout(location = 0) in vec3 aPos;
uniform mat4 uMvp;
out vec3 vWorld;
void main() {
  vWorld = aPos;
  gl_Position = uMvp * vec4(aPos, 1.0);
}`;

// Normale par DERIVEE d'ecran : le maillage n'a pas a porter de normales, et le
// pont n'a donc pas a en transporter.
const FRAG = `#version 300 es
precision highp float;
in vec3 vWorld;
uniform vec3 uTint;
out vec4 fragColor;
void main() {
  vec3 n = normalize(cross(dFdx(vWorld), dFdy(vWorld)));
  float lambert = clamp(abs(dot(n, normalize(vec3(0.4, 0.6, 1.0)))), 0.0, 1.0);
  fragColor = vec4(uTint * (0.25 + 0.75 * lambert), 1.0);
}`;

function compile(type, src) {
  const s = gl.createShader(type);
  gl.shaderSource(s, src);
  gl.compileShader(s);
  if (!gl.getShaderParameter(s, gl.COMPILE_STATUS))
    throw new Error("shader: " + gl.getShaderInfoLog(s));
  return s;
}

function initGL(offscreen) {
  canvas = offscreen;

  // ⚠ PUBLIE AVANT d'ouvrir le contexte. graph_editor.cpp le retrouve ici pour
  // le declarer a emscripten_webgl_create_context via specialHTMLTargets ; sans
  // ce depot, le C++ n'a aucun moyen de designer un canvas dans un worker --
  // document.querySelector n'y existe pas.
  globalThis.__makerCanvas = offscreen;

  // UN SEUL contexte pour les deux dessinateurs. getContext() rend le MEME
  // objet pour un meme identifiant sur un meme canvas : le C++ retrouvera
  // celui-ci, et non un second que le navigateur refuserait.
  gl = canvas.getContext("webgl2", { antialias: true });
  if (!gl) throw new Error("webgl2 indisponible dans le worker");

  program = gl.createProgram();
  gl.attachShader(program, compile(gl.VERTEX_SHADER, VERT));
  gl.attachShader(program, compile(gl.FRAGMENT_SHADER, FRAG));
  gl.linkProgram(program);
  if (!gl.getProgramParameter(program, gl.LINK_STATUS))
    throw new Error("link: " + gl.getProgramInfoLog(program));
  uniforms.mvp = gl.getUniformLocation(program, "uMvp");
  uniforms.tint = gl.getUniformLocation(program, "uTint");

  vao = gl.createVertexArray();
  gl.bindVertexArray(vao);
  posBuffer = gl.createBuffer();
  gl.bindBuffer(gl.ARRAY_BUFFER, posBuffer);
  gl.enableVertexAttribArray(0);
  gl.vertexAttribPointer(0, 3, gl.FLOAT, false, 0, 0);
  idxBuffer = gl.createBuffer();
  gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, idxBuffer);
  gl.bindVertexArray(null);

  gl.enable(gl.DEPTH_TEST);
  gl.clearColor(0.09, 0.10, 0.12, 1.0);
  return gl.getParameter(gl.VERSION);
}

// RENDERER masque par defaut depuis que la chaine sert au fingerprinting.
// L'extension le demasque ; sans elle, la reserve de nodal.md 9.3 -- "mesure en
// rendu logiciel" -- ne serait ni confirmee ni infirmee.
function rendererName() {
  const info = gl.getExtension("WEBGL_debug_renderer_info");
  if (info) return gl.getParameter(info.UNMASKED_RENDERER_WEBGL);
  return gl.getParameter(gl.RENDERER);
}

// Televerse la sortie courante du graphe. La geometrie vient du tas du module,
// dans ce meme thread : rien ne passe par un message.
function uploadCurrentMesh() {
  const view = Module.graphMeshView(0);
  indexCount = view.indices.length;
  if (indexCount === 0) return { nv: 0, nf: 0 };

  gl.bindVertexArray(vao);
  gl.bindBuffer(gl.ARRAY_BUFFER, posBuffer);
  gl.bufferData(gl.ARRAY_BUFFER, view.positions, gl.STATIC_DRAW);
  gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, idxBuffer);
  gl.bufferData(gl.ELEMENT_ARRAY_BUFFER, view.indices, gl.STATIC_DRAW);
  gl.bindVertexArray(null);

  const p = view.positions;
  const lo = [Infinity, Infinity, Infinity], hi = [-Infinity, -Infinity, -Infinity];
  for (let i = 0; i < p.length; i += 3)
    for (let k = 0; k < 3; ++k) {
      if (p[i + k] < lo[k]) lo[k] = p[i + k];
      if (p[i + k] > hi[k]) hi[k] = p[i + k];
    }
  center = [(lo[0] + hi[0]) / 2, (lo[1] + hi[1]) / 2, (lo[2] + hi[2]) / 2];
  radius = Math.max(1e-4, Math.hypot(hi[0] - lo[0], hi[1] - lo[1], hi[2] - lo[2]) / 2);
  return { nv: view.nv, nf: view.nf };
}

// Colonne-majeur, convention WebGL. mul(a, b) rend a*b.
function mul(a, b) {
  const o = new Float32Array(16);
  for (let c = 0; c < 4; ++c)
    for (let r = 0; r < 4; ++r) {
      let v = 0;
      for (let k = 0; k < 4; ++k) v += a[k * 4 + r] * b[c * 4 + k];
      o[c * 4 + r] = v;
    }
  return o;
}

function mvp(aspect, angle) {
  const d = radius * 3.2;
  const zn = Math.max(radius * 0.01, d - radius * 2.5);
  const zf = d + radius * 2.5;
  const f = 1 / Math.tan(0.5);
  const proj = new Float32Array([
    f / aspect, 0, 0, 0,
    0, f, 0, 0,
    0, 0, (zf + zn) / (zn - zf), -1,
    0, 0, (2 * zf * zn) / (zn - zf), 0,
  ]);
  const c = Math.cos(angle), s = Math.sin(angle);
  const rot = new Float32Array([c, 0, -s, 0, 0, 1, 0, 0, s, 0, c, 0, 0, 0, 0, 1]);
  const toOrigin = new Float32Array([
    1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, -center[0], -center[1], -center[2], 1,
  ]);
  const back = new Float32Array([1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, -d, 1]);
  return mul(proj, mul(back, mul(rot, toOrigin)));
}

// Une frame. Appelee par le bouton, et surtout par le POMPAGE : le collecteur
// de progression installe cote hote (graphSetFramePump) l'appelle depuis le
// calcul. cggraph ne connait pas cette fonction et n'a aucun moyen de
// l'atteindre -- il appelle ctx.Progress, et c'est graph_api.cpp qui traduit.
let pumpedFrames = 0;
function drawFrame(tint) {
  if (!gl) return;
  gl.viewport(0, 0, canvas.width, canvas.height);
  gl.clear(gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT);
  if (indexCount === 0) return;
  gl.useProgram(program);
  gl.bindVertexArray(vao);
  gl.uniformMatrix4fv(uniforms.mvp, false, mvp(canvas.width / canvas.height, spin));
  gl.uniform3fv(uniforms.tint, tint || [0.75, 0.78, 0.85]);
  gl.drawElements(gl.TRIANGLES, indexCount, gl.UNSIGNED_INT, 0);
  gl.bindVertexArray(null);
}

// ---------------------------------------------------------------------------
//  LA COEXISTENCE APERCU 3D / INTERFACE -- un canvas, un contexte, un ordre
// ---------------------------------------------------------------------------
//
// La scene d'ABORD (test de profondeur actif), l'interface PAR-DESSUS (melange
// alpha, profondeur desactivee par le backend d'ImGui, qui restaure l'etat
// qu'il a trouve). C'est le motif habituel, et il tient ici parce que les deux
// dessinateurs partagent le meme contexte WebGL2 -- voir initGL.
//
// L'apercu n'est pas perdu derriere les panneaux : leur fond est
// semi-transparent (regle par l'hote, graph_editor.cpp), et le basculement
// d'overlay le rend en plein cadre.
function composite(tint) {
  drawFrame(tint);
  if (Module && Module.graphEditorReady && Module.graphEditorReady())
    Module.graphEditorRenderDrawData();
}

// Combien de pixels du cadre central diffèrent du fond. C'est la seule facon,
// DANS le worker, d'etablir qu'une frame a bien ete rendue -- et un pixel
// unique ne suffit pas : le centre d'un tore est du fond, si bien qu'un seul
// echantillon dirait "rien dessine" d'une image correcte.
//
// La PRESENTATION du canvas placeholder, elle, appartient au compositeur du
// navigateur et n'est PAS observable d'ici.
const CLEAR_RGB = [23, 25, 31];
function drawnPixels() {
  const w = Math.min(64, canvas.width), h = Math.min(64, canvas.height);
  const px = new Uint8Array(w * h * 4);
  gl.readPixels((canvas.width - w) >> 1, (canvas.height - h) >> 1, w, h, gl.RGBA,
                gl.UNSIGNED_BYTE, px);
  let drawn = 0;
  for (let i = 0; i < px.length; i += 4) {
    if (Math.abs(px[i] - CLEAR_RGB[0]) > 2 || Math.abs(px[i + 1] - CLEAR_RGB[1]) > 2 ||
        Math.abs(px[i + 2] - CLEAR_RGB[2]) > 2)
      ++drawn;
  }
  return { sampled: w * h, drawn };
}

// Le pompage de frame -- installe par L'HOTE, jamais par le moteur. cggraph
// appelle ctx.Progress ; graph_api.cpp traduit en appel JS ; cette fonction-ci
// decide que cela veut dire "dessine". Les trois etages sont separables, et
// c'est ce que le critere 7.4 demande.
function installFramePump() {
  Module.graphSetFramePump(() => {
    ++pumpedFrames;
    spin += 0.06;
    // REEMISSION, pas construction : la liste de dessin d'ImGui est celle du
    // dernier tick, et elle reste valide jusqu'au NewFrame suivant. Construire
    // une frame ici serait rentrer dans ImGui depuis l'interieur d'un calcul
    // lance par cette meme frame.
    composite([0.95, 0.65, 0.25]);
  });
}

// ---------------------------------------------------------------------------
//  Entrees -- appliquees DANS L'ORDRE, groupees par tick
// ---------------------------------------------------------------------------
//
// Chaque geste est un petit objet : un caractere de genre et deux ou trois
// nombres. Rien de binaire, rien de geometrique -- c'est ce que la sonde de
// 7.10 verifie, et elle le verifie sur le trafic REEL, pas sur cette phrase.
//
// Ils voyagent GROUPES dans le tick plutot qu'un par message : un pointermove
// arrive plus vite qu'une frame, et ImGui n'a besoin que de la derniere
// position par frame. Le groupage borne le trafic a un message par frame et
// preserve l'ordre, ce dont ImGui a besoin pour qu'un clic reste un clic --
// l'enfoncement et le relachement d'une meme frame se suivent dans la file.
function applyEvents(events) {
  if (!events || !events.length) return;
  for (const e of events) {
    switch (e.k) {
      case "m": Module.graphEditorMouseMove(e.x, e.y); break;
      case "b": Module.graphEditorMouseButton(e.b, e.d); break;
      case "w": Module.graphEditorMouseWheel(e.x, e.y); break;
      case "l": Module.graphEditorMouseLeave(); break;
      case "k": Module.graphEditorKey(e.c, e.d, !!e.ctrl, !!e.shift, !!e.alt, !!e.meta); break;
      case "t": Module.graphEditorText(e.cp); break;
      case "f": Module.graphEditorFocus(e.d); break;
      default: break;
    }
  }
}

// Un tour de boucle complet, et l'ORDRE des quatre temps porte tout le montage.
let lastRevision = 0;
function tick(dt, events) {
  applyEvents(events);

  //  1. la frame ImGui est CONSTRUITE -- aucun appel GL n'en sort ;
  Module.graphEditorFrame(dt);
  //  2. elle est COMPOSEE par-dessus l'apercu ;
  composite();
  //  3. le calcul a lieu HORS FRAME. Le collecteur de progression peut donc
  //     redessiner sans se retrouver au milieu d'une frame commencee : il
  //     appelle composite(), qui reemet la liste du temps 1 ;
  Module.graphEditorPump();
  //  4. un resultat neuf se televerse et se recompose. La revision est le seul
  //     moyen de l'apprendre : c'est le canvas qui appelle Poll, pas nous.
  const revision = Module.graphEditorResultRevision();
  let uploaded = null;
  if (revision !== lastRevision) {
    lastRevision = revision;
    uploaded = uploadCurrentMesh();
    composite();
  }
  return { revision, uploaded };
}

// --------------------------------------------------------------------------
//  Protocole de messages. Rien d'autre que des scalaires, des chaines et des
//  diagnostics ne le traverse.
// --------------------------------------------------------------------------
function reply(id, payload) {
  self.postMessage(Object.assign({ id }, payload));
}

self.onmessage = async (event) => {
  const msg = event.data || {};
  const id = msg.id;
  try {
    switch (msg.type) {
      case "init": {
        const version = initGL(msg.canvas);
        // UNE instance, et une seule : c'est celle du graphe (D17).
        Module = await createMakerModule();
        // Le pompage de frame est installe ICI, par l'hote. C'est la moitie
        // que cggraph n'a pas le droit de connaitre.
        installFramePump();
        // L'editeur ImGui : contexte GL partage avec celui d'au-dessus, contexte
        // ImGui, canvas commun aux deux hotes. Un refus est RENDU, jamais tu --
        // une page qui afficherait un cadre noir sans rien dire serait pire.
        const editorError = Module.graphEditorInit(canvas.width, canvas.height);
        reply(id, {
          type: "ready",
          glVersion: version,
          renderer: rendererName(),
          catalog: JSON.parse(Module.graphCatalog()),
          budget: Module.graphGetMemoryBudget(),
          heap: Module.heapBytes(),
          editor: editorError === "" ? "ok" : editorError,
        });
        break;
      }

      case "tick": {
        if (!Module || !Module.graphEditorReady()) {
          reply(id, { type: "ticked", ready: false });
          break;
        }
        const outcome = tick(msg.dt || 0, msg.events);
        reply(id, {
          type: "ticked",
          ready: true,
          cursor: Module.graphEditorCursor(),
          state: JSON.parse(Module.graphEditorState()),
          revision: outcome.revision,
          uploaded: outcome.uploaded,
          pumpedFrames,
        });
        break;
      }

      // Demande une evaluation par la BOUCLE DE FRAMES, la ou "evaluate" la fait
      // sur place. Le pompage de frame n'a de sens que sur ce chemin-la.
      case "request":
        Module.graphEditorRequest(msg.node);
        reply(id, { type: "requested", node: msg.node });
        break;

      case "overlay":
        Module.graphEditorSetOverlay(!!msg.on);
        reply(id, { type: "overlay", on: Module.graphEditorGetOverlay() });
        break;

      case "resize":
        canvas.width = msg.width;
        canvas.height = msg.height;
        Module.graphEditorResize(canvas.width, canvas.height);
        composite();
        reply(id, { type: "resized", width: canvas.width, height: canvas.height });
        break;

      case "reset":
        Module.graphReset();
        // graphReset() reconstruit l'hote, donc perd le collecteur.
        installFramePump();
        // ... et il remet les identifiants de noeuds a zero, ce que le canvas
        // doit apprendre : sa trace de placement les designe encore.
        Module.graphEditorReset();
        indexCount = 0;
        lastRevision = Module.graphEditorResultRevision();
        composite();
        reply(id, { type: "reset" });
        break;

      // CHARGEMENT D'UN DOCUMENT. graphFromJson() remet l'hote a vide avant de
      // relire : les deux gestes de "reset" s'imposent donc ici aussi -- le
      // collecteur de progression est perdu avec l'hote, et la trace de
      // placement du canvas designe des identifiants de noeuds qui repartent.
      //
      // Le cadrage vient EN DERNIER, sur le canvas neuf : graphEditorReset()
      // en construit un autre, et une demande posee avant serait detruite avec
      // le precedent. Il n'est pas demande si la relecture a echoue -- cadrer
      // sur le graphe vide que l'echec laisse ne montrerait rien.
      case "loadDocument": {
        const error = Module.graphFromJson(msg.json);
        installFramePump();
        Module.graphEditorReset();
        indexCount = 0;
        lastRevision = Module.graphEditorResultRevision();
        if (!error) Module.graphEditorFitToContent();
        composite();
        reply(id, { type: "loaded", error });
        break;
      }

      case "addNode":
        reply(id, { type: "node", node: Module.graphAddNode(msg.nodeType, msg.x || 0, msg.y || 0) });
        break;

      case "connect":
        reply(id, {
          type: "connected",
          status: Module.graphConnect(msg.from, msg.fromPort, msg.to, msg.toPort),
        });
        break;

      case "setParam": {
        let ok = false;
        if (msg.valueType === "int") ok = Module.graphSetInt(msg.node, msg.name, msg.value | 0);
        else if (msg.valueType === "float") ok = Module.graphSetFloat(msg.node, msg.name, +msg.value);
        else if (msg.valueType === "bool") ok = Module.graphSetBool(msg.node, msg.name, !!msg.value);
        else ok = Module.graphSetString(msg.node, msg.name, String(msg.value));
        reply(id, { type: "param", ok });
        break;
      }

      case "nodeInfo":
        reply(id, { type: "nodeInfo", info: JSON.parse(Module.graphNodeInfo(msg.node)) });
        break;

      // Deux entrees de fichier, et AUCUNE des deux ne fait traverser d'octets.
      //
      //  loadFile  la page envoie le File choisi par l'utilisateur -- une
      //            REFERENCE de blob, pas son contenu. C'est ce worker qui lit
      //            les octets ;
      //  fetchFile ce worker va chercher l'URL lui-meme.
      //
      // Envoyer un ArrayBuffer d'OBJ conviendrait au sens strict de D27 (une
      // entree ponctuelle n'est pas la geometrie par frame de la variante
      // ecartee), mais la sonde de 7.10 n'aurait alors plus rien de net a
      // mesurer : mieux vaut que la frontiere soit vide.
      case "loadFile": {
        const bytes = new Uint8Array(await msg.file.arrayBuffer());
        Module.FS.writeFile(msg.path, bytes);
        reply(id, { type: "file", path: msg.path, bytes: bytes.length });
        break;
      }

      case "fetchFile": {
        const response = await fetch(msg.url);
        if (!response.ok) throw new Error(`fetch ${msg.url} : ${response.status}`);
        const bytes = new Uint8Array(await response.arrayBuffer());
        Module.FS.writeFile(msg.path, bytes);
        reply(id, { type: "file", path: msg.path, bytes: bytes.length });
        break;
      }

      case "evaluate": {
        pumpedFrames = 0;
        const t0 = performance.now();
        const result = JSON.parse(Module.graphEvaluate(msg.node));
        const counts = result.status === "ok" ? uploadCurrentMesh() : { nv: 0, nf: 0 };
        lastRevision = Module.graphEditorResultRevision();
        spin += 0.35;
        composite();
        reply(id, {
          type: "evaluated",
          result,
          wallMs: Math.round(performance.now() - t0),
          pumpedFrames,
          pixels: drawnPixels(),
          heap: Module.heapBytes(),
          cache: JSON.parse(Module.graphCacheStats()),
          nv: counts.nv,
          nf: counts.nf,
        });
        break;
      }

      case "budget":
        if (typeof msg.bytes === "number") Module.graphSetMemoryBudget(msg.bytes);
        reply(id, { type: "budget", budget: Module.graphGetMemoryBudget() });
        break;

      // Combien de pixels du cadre central ont ete ecrits. C'est l'instrument
      // dont une sonde a besoin pour etablir qu'une frame a bien ete rendue --
      // il n'y a pas d'oeil humain dans un navigateur sans tete.
      case "pixels":
        reply(id, { type: "pixels", pixels: drawnPixels() });
        break;

      // La MEME question que "pixels", mais rendue en image plutot qu'en
      // comptage. Elle existe pour ce que le comptage ne sait pas dire : un
      // rendu peut etre entierement ecrit et entierement laid, et le seul juge
      // en est un oeil humain, qui n'est pas dans le navigateur sans tete.
      //
      // Rendue en CHAINE -- une URL de donnees --, jamais en octets : la
      // frontiere de D27 ne porte que des scalaires, des chaines et des
      // diagnostics, et une sonde ne doit pas etre ce qui l'entame.
      case "shot": {
        const w = canvas.width;
        const h = canvas.height;

        // ⚠ LA FRAME EST REEMISE ICI, ET IL LE FAUT. Le tampon de dessin d'un
        // contexte WebGL sans preserveDrawingBuffer est vide des que la tache
        // qui l'a rempli se termine : relire les pixels dans un message
        // ULTERIEUR rend un cadre noir. Mesure faite -- la premiere version de
        // cette capture rendait un PNG entierement noir, que drawnPixels ()
        // annoncait pourtant « ecrit a 4096/4096 », faute de distinguer le noir
        // du fond.
        composite();
        const px = new Uint8ClampedArray(w * h * 4);
        gl.readPixels(0, 0, w, h, gl.RGBA, gl.UNSIGNED_BYTE, px);

        // readPixels rend l'image a l'endroit d'OpenGL, origine en bas ; une
        // ImageData la lit origine en haut. Sans ce retournement la capture
        // sortirait a l'envers, ce qu'un comptage de pixels n'aurait pas vu.
        const rows = new Uint8ClampedArray(w * h * 4);
        for (let y = 0; y < h; ++y)
          rows.set(px.subarray((h - 1 - y) * w * 4, (h - y) * w * 4), y * w * 4);

        // Le canal ALPHA du cadre est celui que le compositeur du navigateur a
        // laisse, non celui d'une image : il vaut zero sur tout ce que la scene
        // n'a pas ecrit. Le garder rendrait un PNG entierement transparent --
        // et un comptage de pixels ne l'aurait pas vu, lui qui ne lit que le
        // rouge, le vert et le bleu.
        for (let i = 3; i < rows.length; i += 4) rows[i] = 255;

        const shot = new OffscreenCanvas(w, h);
        shot.getContext("2d").putImageData(new ImageData(rows, w, h), 0, 0);
        const bytes = new Uint8Array(
          await (await shot.convertToBlob({ type: "image/png" })).arrayBuffer()
        );

        let base64 = "";
        for (let i = 0; i < bytes.length; i += 8192)
          base64 += String.fromCharCode.apply(null, bytes.subarray(i, i + 8192));
        reply(id, {
          type: "shot",
          width: w,
          height: h,
          dataUrl: "data:image/png;base64," + btoa(base64),
        });
        break;
      }

      case "stats":
        reply(id, {
          type: "stats",
          heap: Module.heapBytes(),
          cache: JSON.parse(Module.graphCacheStats()),
        });
        break;

      case "document":
        reply(id, { type: "document", json: Module.graphToJson() });
        break;

      default:
        reply(id, { type: "error", error: "message inconnu: " + msg.type });
    }
  } catch (e) {
    reply(id, { type: "error", error: String(e && e.message ? e.message : e) });
  }
};
