// ===========================================================================
//  Pont thread UI <-> worker du graphe.
// ===========================================================================
//
// Ce fichier ne dessine rien et ne calcule rien. Il fait trois choses :
//
//  1. il TRANSFERE l'OffscreenCanvas au worker -- transferControlToOffscreen()
//     puis passage du handle en SECOND argument de postMessage. Apres cet
//     appel, le thread UI n'a plus de contexte de dessin sur ce canvas : la
//     propriete de D27 est portee par le navigateur, pas par une convention ;
//  2. il numerote les messages et apparie les reponses ;
//  3. il n'emet et ne recoit que des scalaires, des chaines et des
//     diagnostics. JAMAIS de geometrie -- une geometrie dans un postMessage
//     est la signature de la variante ecartee "rendu sur le thread UI", que
//     D27 refuse.
//
// Il est importe par la page (graph-host.js) ET par les sondes de
// maker/probes/ : ainsi la propriete de D27 se lit et se sabote a UN seul
// endroit.
//
let worker = null;
let nextId = 1;
const pending = new Map();

export function startWorker(canvas) {
  worker = new Worker(new URL("./graph-worker.js", import.meta.url), { type: "module" });
  worker.onmessage = (event) => {
    const msg = event.data || {};
    const resolve = pending.get(msg.id);
    if (resolve) {
      pending.delete(msg.id);
      resolve(msg);
    }
  };

  // LE transfert. Le second argument de postMessage est la liste de transfert ;
  // sans lui, le handle serait clone et le canvas resterait au thread UI.
  const offscreen = canvas.transferControlToOffscreen();
  return call({ type: "init", canvas: offscreen }, [offscreen]);
}

// Un message SANS reponse. Il existe pour ce qui n'en attend pas -- et il n'est
// PAS le chemin des evenements d'entree : ceux-la voyagent groupes dans le tick,
// qui, lui, attend sa reponse. C'est cette attente qui empeche un backlog de
// s'accumuler pendant qu'un calcul tient le worker.
export function send(message, transfer) {
  worker.postMessage(message, transfer || []);
}

export function call(message, transfer) {
  const id = nextId++;
  return new Promise((resolve) => {
    pending.set(id, resolve);
    worker.postMessage(Object.assign({ id }, message), transfer || []);
  });
}

