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

// L'UNITE SUIT LA TAILLE. Le tas et le budget de cache se comptent en mébioctets,
// mais un document de graphe fait quelques kibioctets : les rendre en Mio les
// affichait tous « 0.0 Mio », c'est-à-dire rien.
function humanBytes(n) {
  if (!(n >= 1024)) return `${Math.round(n)} o`;
  if (n < 1024 * 1024) return `${(n / 1024).toFixed(1)} Kio`;
  return `${(n / (1024 * 1024)).toFixed(1)} Mio`;
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
  // Sélecteur de fichier des demandes de l'inspecteur. Il ne lit AUCUNE
  // sélection : le canvas nomme lui-même le nœud visé, et il est la seule
  // autorité sur ce qui est sélectionné. C'est ce qui a permis de retirer
  // l'import global du panneau, qui devait deviner la cible.
  const picker = document.createElement("input");
  picker.type = "file";
  picker.style.display = "none";
  document.body.appendChild(picker);
  let pickerTarget = null;

  picker.addEventListener("change", async () => {
    const file = picker.files[0];
    const target = pickerTarget;
    picker.value = "";
    pickerTarget = null;
    if (!file || !target) return;

    // DEUX DESTINATIONS, et c'est le canvas qui a tranché : un nom de paramètre
    // veut dire « écris-y un chemin », son absence veut dire « verse les octets ».
    // La distinction vient du nœud lui-même — un paramètre `path` pour file.ref
    // et mesh.io.load, l'interface ByteSource pour img.io.load et
    // text.font.load — et non d'une liste tenue ici.
    if (!target.param) {
      // Le File part par RÉFÉRENCE : ses octets ne traversent pas le
      // postMessage, c'est le worker qui les lit.
      const answer = await call({ type: "setBytes", node: target.node, file });
      if (answer.ok) log(`${file.name} — ${humanBytes(file.size)} chargés dans le nœud #${target.node}`);
      else log(`${file.name} refusé par le nœud #${target.node} (fichier vide ?)`);
      await refreshDocument();
      return;
    }

    // /tmp : un fichier choisi à la souris n'a pas d'adresse durable. Le
    // document ne le retrouvera pas au chargement suivant, mais il le NOMME.
    const path = "/tmp/" + file.name;
    const placed = await call({ type: "placeFile", path, file });
    if (placed.error) { log(`${file.name} : ${placed.error}`); return; }
    await call({ type: "setParam", node: target.node, name: target.param,
                 valueType: "string", value: path });
    log(`${file.name} — ${humanBytes(file.size)} → ${target.param} du nœud #${target.node}`);
    await refreshDocument();
  });
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
        if (answer.state) {
          // Le canvas ne peut pas ouvrir de sélecteur : il DEMANDE, on ouvre.
          // La demande est effacée à la lecture côté C++, donc elle n'arrive
          // qu'une fois par clic — pas de garde à tenir ici.
          if (answer.state.fileRequest) {
            pickerTarget = answer.state.fileRequest;
            picker.click();
          }
          // Les compteurs portent sur le document RACINE, même quand l'œil est
          // descendu dans un sous-graphe : la façade procédurale continue de
          // désigner la racine, et descendre est un geste de lecture. La
          // mention l'annonce, sans quoi la barre semblerait contredire ce que
          // le canvas affiche.
          const d = answer.state.descent;
          $("summary").textContent =
            `${answer.state.nodes} nœud(s), ${answer.state.links} lien(s)` +
            (answer.state.selection ? ` — sélection #${answer.state.selection}` : "") +
            (d ? ` — vue : ${d.reference} (lecture seule)` : "");
        }
        if (answer.uploaded)
          log(`aperçu : ${answer.uploaded.nv} sommets, ${answer.uploaded.nf} triangles`);
      })
      .catch(() => {
        inFlight = false;
      });
  };
  requestAnimationFrame(frame);

  // ---- Ce que le canvas ne sait pas faire -------------------------------
  //
  // Il ne reste QU'UN geste de fichier ici : ouvrir un document. Le chargement
  // d'une RESSOURCE est parti dans l'inspecteur, sur le noeud concerne, et la
  // dispersion en deux endroits a disparu avec lui -- une entree en haut de
  // page qui ne disait pas a quel noeud elle s'adressait, plus un bouton
  // « Parcourir… » qui faisait la moitié du travail.
  //
  // Ce que la page savait faire et que le canvas ne savait pas -- ouvrir un
  // sélecteur de fichier -- lui reste : il DEMANDE (fileRequest), on ouvre.

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
    lastDocumentName = file.name;
    await refreshDocument();
    log(`document ${file.name} ouvert — vue recadrée sur les nœuds`);
  });

  $("overlay").addEventListener("click", async () => {
    const { on } = await call({ type: "overlay", on: $("overlay").dataset.on !== "1" });
    $("overlay").dataset.on = on ? "1" : "0";
    $("overlay").textContent = on ? "masquer l'éditeur" : "afficher l'éditeur";
    // Éditeur masqué : la scène prend tout le cadre, donc la poignée ne sépare
    // plus rien. La laisser afficherait un trait au milieu d'une vue pleine, et
    // la glisser ne montrerait aucun effet.
    $("split").hidden = !on;
  });
  $("overlay").dataset.on = "1";

  // ---- SEPARATEUR ------------------------------------------------------
  //
  // La poignee ne connait qu'une FRACTION ; c'est le worker qui en tire le
  // viewport de la scene, et le C++ la disposition de ses panneaux. Une seule
  // valeur, deux lecteurs — la faire calculer des deux cotes les aurait
  // decales d'un pixel.
  //
  // La position est gardee dans localStorage : rouvrir la page sur un volet
  // droit ecrase par mégarde serait un piège, et c'est un réglage par poste,
  // pas une propriété du document.
  // Nom du dernier document OUVERT, propose a l'enregistrement : reexporter ce
  // qu'on vient de lire ne doit pas obliger a retaper son nom.
  let lastDocumentName = "";

  const splitter = $("split");
  const stage = $("stage");
  let splitFraction = 0.5;
  try {
    const kept = parseFloat(localStorage.getItem("maker.graph.split"));
    if (kept > 0.05 && kept < 0.95) splitFraction = kept;
  } catch { /* stockage refusé (navigation privée) : le défaut suffit */ }

  const placeSplitter = () => {
    splitter.style.left = `${splitFraction * 100}%`;
  };
  const applySplit = async (fraction) => {
    // BORNES : sous 5 % l'éditeur n'est plus utilisable, au-dessus de 95 % la
    // vue 3D disparaît sans que rien ne le dise. Pour les faire disparaître
    // franchement il y a « masquer l'éditeur », qui est explicite.
    splitFraction = Math.min(0.95, Math.max(0.05, fraction));
    placeSplitter();
    try { localStorage.setItem("maker.graph.split", String(splitFraction)); } catch { /* idem */ }
    await call({ type: "split", fraction: splitFraction });
  };

  // L'ETAT DE GLISSEMENT EST UN BOOLEEN A NOUS, et non hasPointerCapture : la
  // capture est un CONFORT -- elle garde les evenements quand le pointeur sort
  // de la poignee de 9 px --, pas la condition du geste. S'en servir comme
  // drapeau lie le glissement a la reussite d'une capture, qui peut echouer
  // (pointeur deja capture ailleurs, evenement synthetique) et laisser une
  // poignee morte sans que rien ne le dise.
  let dragging = false;

  splitter.addEventListener("pointerdown", (event) => {
    dragging = true;
    try { splitter.setPointerCapture(event.pointerId); } catch { /* confort seul */ }
    splitter.classList.add("drag");
    event.preventDefault();
  });
  splitter.addEventListener("pointermove", (event) => {
    if (!dragging) return;
    const rect = stage.getBoundingClientRect();
    if (rect.width > 0) applySplit((event.clientX - rect.left) / rect.width);
  });
  const endDrag = (event) => {
    if (!dragging) return;
    dragging = false;
    try { splitter.releasePointerCapture(event.pointerId); } catch { /* idem */ }
    splitter.classList.remove("drag");
  };
  splitter.addEventListener("pointerup", endDrag);
  splitter.addEventListener("pointercancel", endDrag);
  // Double-clic : retour au partage égal, geste habituel d'une poignée.
  splitter.addEventListener("dblclick", () => applySplit(0.5));

  placeSplitter();
  await applySplit(splitFraction);

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

  // ---- EXPORT ----------------------------------------------------------
  //
  // Le bouton ECRIT UN FICHIER, la ou il se contentait de deverser le document
  // dans le journal — utile pour lire, inutilisable pour garder.
  //
  // ⚠ LE SELECTEUR D'ABORD, LA SERIALISATION ENSUITE. showSaveFilePicker exige
  // une activation utilisateur, et un aller-retour vers le worker la consomme :
  // ouvrir la boite pendant que le clic est encore « frais » est la seule
  // sequence qui tienne quand le document est gros.
  $("export").addEventListener("click", async () => {
    const suggested = lastDocumentName || "graphe.json";

    let handle = null;
    if (typeof window.showSaveFilePicker === "function") {
      try {
        handle = await window.showSaveFilePicker({
          suggestedName: suggested,
          types: [{ description: "Document cggraph",
                    accept: { "application/json": [".json"] } }],
        });
      } catch (error) {
        // ANNULATION ET REFUS NE SE CONFONDENT PAS : la première est un choix
        // de l'utilisateur, qu'on respecte en ne faisant rien ; le second veut
        // dire que l'API n'est pas utilisable ici (navigateur sans elle,
        // contexte non sécurisé), et il faut alors le téléchargement.
        if (error && error.name === "AbortError") { log("export annulé"); return; }
        handle = null;
      }
    }

    const { json } = await call({ type: "document" });
    // Un Blob plutôt que la chaîne : sa taille est en OCTETS, et un document
    // accentué en compte plus qu'il n'a de caractères. C'est aussi ce que les
    // deux chemins d'écriture attendent.
    const blob = new Blob([json], { type: "application/json" });

    if (handle) {
      const stream = await handle.createWritable();
      await stream.write(blob);
      await stream.close();
      log(`document écrit dans ${handle.name} (${humanBytes(blob.size)})`);
      return;
    }

    // REPLI : téléchargement sous le nom suggéré. Firefox et Safari n'ont pas
    // showSaveFilePicker ; sans ce chemin, l'export n'existerait pas chez eux.
    const url = URL.createObjectURL(blob);
    const link = document.createElement("a");
    link.href = url;
    link.download = suggested;
    link.click();
    URL.revokeObjectURL(url);
    log(`document téléchargé sous ${suggested} (${humanBytes(blob.size)})`);
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
