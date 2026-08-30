// ===========================================================================
//  Page du graphe -- controleur, cote THREAD UI.
// ===========================================================================
//
// Il ne dessine rien et ne calcule rien. Il fait trois choses :
//
//  1. il CADENCE : un tick par requestAnimationFrame. ⚠ Non parce que rAF
//     manquerait dans un Worker -- mesure faite, il y existe et il y sert --,
//     mais parce que le tick porte deja les entrees et qu'un rAF de thread UI
//     est universel. Le motif est ecrit au long dans graph-worker.js ;
//  2. il ROUTE LES ENTREES. Le canvas est dessine dans le worker, mais
//     l'element <canvas> reste dans le DOM et continue de recevoir les
//     evenements : transferControlToOffscreen cede le RENDU, pas l'element.
//     Les gestes sont accumules dans l'ordre et partent GROUPES avec le tick ;
//  3. il affiche ce que le worker renvoie -- des COMPTES et des diagnostics,
//     jamais de geometrie.
//
// Le transfert du canvas et le protocole vivent dans graph-bridge.js, que les
// sondes de maker/probes/ importent aussi : la propriete de D27 se lit a UN
// seul endroit.
//
import { startWorker, call } from "./graph-bridge.js";

const $ = (id) => document.getElementById(id);

function log(text) {
  const el = $("log");
  el.textContent = text + "\n" + el.textContent;
}

function humanBytes(n) {
  return (n / (1024 * 1024)).toFixed(1) + " Mio";
}

// --------------------------------------------------------------------------
//  Entrees : accumulation ordonnee, vidangee a chaque tick
// --------------------------------------------------------------------------
//
// ⚠ POURQUOI ACCUMULER PLUTOT QU'EMETTRE. Un pointermove arrive plus souvent
// qu'une frame ; un message par geste ferait un trafic non borne, et pendant un
// calcul long il s'empilerait dans la file du worker pour etre rejoue d'un bloc.
// Ici la file est bornee par la frame : au pire un tick porte tous les gestes
// d'un intervalle, dans l'ordre ou ils sont arrives -- ce dont ImGui a besoin
// pour qu'un enfoncement suivi d'un relachement reste un clic.
const pending = [];

function push(event) {
  // Deux deplacements consecutifs se REMPLACENT : ImGui ne lit que la derniere
  // position de la frame, et garder les intermediaires n'ajoute rien qu'un
  // message plus gros.
  if (event.k === "m" && pending.length && pending[pending.length - 1].k === "m")
    pending[pending.length - 1] = event;
  else pending.push(event);
}

function drain() {
  if (!pending.length) return null;
  return pending.splice(0, pending.length);
}

// Les curseurs d'ImGui, dans l'ordre de son enumeration ImGuiMouseCursor_.
// Le canvas etant dans le worker, le navigateur ne sait rien de ce qu'ImGui
// survole : sans ce report, redimensionner une fenetre ou entrer dans un champ
// ne donnerait aucun retour visuel.
const CURSORS = ["default", "text", "move", "ns-resize", "ew-resize", "nesw-resize",
                 "nwse-resize", "pointer", "not-allowed"];

function attachInput(canvas) {
  const local = (event) => {
    const rect = canvas.getBoundingClientRect();
    return {
      x: (event.clientX - rect.left) * (canvas.width / rect.width),
      y: (event.clientY - rect.top) * (canvas.height / rect.height),
    };
  };

  // Pointer events plutot que mouse events : une seule famille couvre souris,
  // stylet et tactile, et setPointerCapture garde le glissement vivant quand le
  // curseur sort du cadre -- un lien qu'on tire hors du canvas ne doit pas se
  // rompre.
  canvas.addEventListener("pointerdown", (event) => {
    canvas.focus();
    canvas.setPointerCapture(event.pointerId);
    const p = local(event);
    push({ k: "m", x: p.x, y: p.y });
    push({ k: "b", b: event.button, d: true });
    event.preventDefault();
  });

  canvas.addEventListener("pointermove", (event) => {
    const p = local(event);
    push({ k: "m", x: p.x, y: p.y });
  });

  const release = (event) => {
    const p = local(event);
    push({ k: "m", x: p.x, y: p.y });
    push({ k: "b", b: event.button, d: false });
  };
  canvas.addEventListener("pointerup", release);
  canvas.addEventListener("pointercancel", release);

  canvas.addEventListener("pointerleave", () => push({ k: "l" }));

  // deltaMode : 0 pixels, 1 lignes, 2 pages. ImGui compte en CRANS ; diviser
  // par une constante de pixels arbitraire donnerait un zoom qui ne ressemble a
  // rien d'un navigateur a l'autre.
  canvas.addEventListener(
    "wheel",
    (event) => {
      const unit = event.deltaMode === 0 ? 100 : event.deltaMode === 1 ? 3 : 1;
      push({ k: "w", x: -event.deltaX / unit, y: -event.deltaY / unit });
      event.preventDefault();
    },
    { passive: false }
  );

  // Le menu contextuel du navigateur mangerait le clic droit, qui est le geste
  // de navigation de l'editeur de noeuds.
  canvas.addEventListener("contextmenu", (event) => event.preventDefault());

  // ⚠ LE CLAVIER EST DEUX CHOSES, PAS UNE. `event.code` designe une POSITION
  // sur le clavier -- c'est ce dont ImGui a besoin pour les raccourcis et la
  // navigation. `event.key` porte le CARACTERE, qui depend de la disposition et
  // des modificateurs -- c'est ce dont les champs de l'inspecteur ont besoin.
  // Router seulement le premier donnerait un editeur ou l'on ne peut rien
  // taper ; seulement le second, un editeur ou Suppr n'efface rien.
  const modifiers = (event) => ({
    ctrl: event.ctrlKey,
    shift: event.shiftKey,
    alt: event.altKey,
    meta: event.metaKey,
  });

  canvas.addEventListener("keydown", (event) => {
    push(Object.assign({ k: "k", c: event.code, d: true }, modifiers(event)));

    // Un seul point de code : c'est ce que rend `key` pour une touche
    // imprimable, quelle que soit la disposition. Les touches nommees
    // ("Enter", "ArrowLeft") ont une chaine plus longue et ne passent pas ici.
    if (!event.ctrlKey && !event.metaKey && event.key.length === 1)
      push({ k: "t", cp: event.key.codePointAt(0) });

    // Tab, Espace et les fleches feraient defiler ou changer le focus.
    if (event.key !== "F5" && event.key !== "F12") event.preventDefault();
  });

  canvas.addEventListener("keyup", (event) => {
    push(Object.assign({ k: "k", c: event.code, d: false }, modifiers(event)));
  });

  // ⚠ Sans ce relachement, une touche maintenue au moment ou l'on change
  // d'onglet reste enfoncee pour toujours du point de vue d'ImGui : le
  // navigateur n'envoie pas le keyup d'une fenetre qui n'a plus le focus.
  canvas.addEventListener("blur", () => push({ k: "f", d: false }));
  canvas.addEventListener("focus", () => push({ k: "f", d: true }));
  window.addEventListener("blur", () => push({ k: "f", d: false }));
}

// --------------------------------------------------------------------------
//  Panneau lateral -- ce qui reste en HTML
// --------------------------------------------------------------------------
//
// La palette, l'inspecteur et le cablage sont passes DANS le canvas : c'est
// tout l'objet de ce portage, et les garder en double aurait donne deux
// interfaces sur un meme document. Ce qui reste ici est ce que le canvas ne
// sait pas faire : ouvrir un fichier local, et dire ce qui se passe.
async function refreshDocument() {
  const { json } = await call({ type: "document" });
  const doc = JSON.parse(json);
  $("summary").textContent = `${doc.nodes.length} nœud(s), ${(doc.links || []).length} lien(s)`;
}

async function main() {
  const canvas = $("view");
  const resize = async () => {
    const width = Math.max(1, Math.round(canvas.clientWidth));
    const height = Math.max(1, Math.round(canvas.clientHeight));
    await call({ type: "resize", width, height });
  };

  canvas.width = Math.max(1, canvas.clientWidth);
  canvas.height = Math.max(1, canvas.clientHeight);

  const ready = await startWorker(canvas);
  if (ready.type === "error") {
    log("échec du worker : " + ready.error);
    return;
  }
  await resize();
  log(`worker prêt — ${ready.renderer} — tas ${humanBytes(ready.heap)} — budget de cache ${humanBytes(ready.budget)}`);
  if (ready.editor !== "ok") log("éditeur nodal indisponible : " + ready.editor);
  else log(`éditeur nodal dans le worker — ${ready.catalog.length} types de nœuds dans la palette`);

  attachInput(canvas);
  new ResizeObserver(() => resize()).observe(canvas);

  // ---- La cadence -------------------------------------------------------
  //
  // Un tick par rAF, et JAMAIS deux en vol. Le tick ATTEND sa reponse : pendant
  // un calcul long le worker ne repond pas, donc rien n'est poste, donc rien ne
  // s'accumule. C'est la difference entre « l'interface gele pendant le calcul »
  // -- ce que l'option (a) assume -- et « l'interface rejoue trois cents frames
  // de retard apres le calcul », ce que personne n'assume.
  //
  // Et c'est ce meme tick qui remet les gestes accumules : une seule source de
  // frames, un seul message par frame. Le pompage de frame du worker, lui, ne
  // CONSTRUIT pas de frame -- il reemet la derniere. Les deux ne se croisent
  // donc pas.
  let inFlight = false;
  let previous = performance.now();
  let frames = 0;
  const frame = (now) => {
    requestAnimationFrame(frame);
    if (inFlight) return;
    const dt = Math.min(0.25, (now - previous) / 1000);
    previous = now;
    inFlight = true;
    call({ type: "tick", dt, events: drain() })
      .then((answer) => {
        inFlight = false;
        if (!answer || !answer.ready) return;
        ++frames;
        $("frames").textContent = frames;
        canvas.style.cursor = CURSORS[answer.cursor] || "default";
        if (answer.state)
          $("summary").textContent =
            `${answer.state.nodes} nœud(s), ${answer.state.links} lien(s)` +
            (answer.state.selection ? ` — sélection #${answer.state.selection}` : "");
        if (answer.uploaded)
          log(`aperçu : ${answer.uploaded.nv} sommets, ${answer.uploaded.nf} triangles`);
      })
      .catch(() => {
        inFlight = false;
      });
  };
  requestAnimationFrame(frame);

  // ---- Ce que le canvas ne sait pas faire -------------------------------
  $("file").addEventListener("change", async () => {
    const file = $("file").files[0];
    if (!file) return;
    const path = "/tmp/" + file.name;
    // Le File part par REFERENCE : ses octets ne traversent pas, c'est le
    // worker qui les lit.
    await call({ type: "loadFile", path, file });
    log(`fichier ${path} — ${humanBytes(file.size)} dans le MEMFS du worker`);
    log("→ colle ce chemin dans le champ « path » d'un nœud mesh.io.load");
  });

  // OUVRIR UN DOCUMENT. C'est le seul geste de cette page qui soit un
  // « chargement » au sens du canvas, et c'est donc le seul qui declenche un
  // recadrage -- le worker s'en charge, en meme temps qu'il reconstruit ce que
  // la relecture emporte. Le texte traverse, pas des octets de geometrie : un
  // document de graphe est un diagnostic, pas une scene (D27).
  $("open").addEventListener("change", async () => {
    const file = $("open").files[0];
    if (!file) return;
    const { error } = await call({ type: "loadDocument", json: await file.text() });
    $("open").value = "";
    if (error) {
      log(`document refusé : ${error}`);
      return;
    }
    await refreshDocument();
    log(`document ${file.name} ouvert — vue recadrée sur les nœuds`);
  });

  $("overlay").addEventListener("click", async () => {
    const { on } = await call({ type: "overlay", on: $("overlay").dataset.on !== "1" });
    $("overlay").dataset.on = on ? "1" : "0";
    $("overlay").textContent = on ? "masquer l'éditeur" : "afficher l'éditeur";
  });
  $("overlay").dataset.on = "1";

  $("reset").addEventListener("click", async () => {
    await call({ type: "reset" });
    await refreshDocument();
    log("graphe remis à vide");
  });

  $("stats").addEventListener("click", async () => {
    const s = await call({ type: "stats" });
    log(
      `tas ${humanBytes(s.heap)} — cache ${humanBytes(s.cache.bytes)}/${humanBytes(s.cache.budget)}` +
        ` — ${s.cache.entries} entrée(s), ${s.cache.hits} succès, ${s.cache.evictions} éviction(s)`
    );
  });

  $("export").addEventListener("click", async () => {
    const { json } = await call({ type: "document" });
    log(json);
  });

  // Temoin de vivacite : un setTimeout de 0 ms qui se replante. S'il cesse de
  // battre pendant une evaluation, c'est que le module vit sur ce thread.
  let beats = 0;
  const beat = () => {
    ++beats;
    $("beat").textContent = beats;
    setTimeout(beat, 0);
  };
  beat();

  canvas.tabIndex = 0;
  canvas.focus();
  await refreshDocument();
}

main();
