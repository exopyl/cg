// ===========================================================================
//  Garde d'impression d'une Gothic Window : lecture du rapport rendu par le
//  WASM, resume pour la banniere et la ligne d'etat, document de l'imprimante.
// ===========================================================================
//  Le controle est calcule cote C++ (CheckGothicExportAndPrintAtWidth) ; ce
//  module ne fait que le LIRE. Deux documents, jamais melanges :
//    - print-report/v1 : le verdict, rendu par exportObj / exportGlb /
//      gothicPrintReport dans le champ `print` (texte JSON) ;
//    - print-constraints/v1 : l'imprimante (seuils, enveloppe), gardee dans
//      localStorage. L'echelle n'y est jamais : elle vient de la largeur
//      imprimee de la piece, tenue par la page.
//
//  La garde AVERTIT : rien ici ne peut empecher un telechargement. Toute
//  lecture qui echoue devient un motif affiche, jamais une exception.
// ===========================================================================

export const PRINT_CONSTRAINTS_KEY = "maker.printConstraints.v1";

// Champs de garde d'un resultat WASM -> verdict { refused?, warning?, print?,
// printFailure? }, `print` deja lu ; null quand il n'y a rien a dire (forme
// non gothique).
export function guardVerdict(res) {
  if (!res) return null;
  const v = {};
  if (res.refused !== undefined) v.refused = res.refused;
  if (res.warning !== undefined) v.warning = res.warning;
  if (res.printFailure !== undefined) v.printFailure = res.printFailure;
  if (res.print !== undefined) {
    try {
      v.print = JSON.parse(res.print);
    } catch (e) {
      v.printFailure = `rapport d'impression illisible (${e.message})`;
    }
  }
  return Object.keys(v).length ? v : null;
}

// Informations du rapport, comptees mais jamais un defaut (IsPrintInformation).
const INFORMATION = new Set(["blunted_void_corner", "scale_unset"]);

const DEFECT_LABELS = {
  thin_wall: "paroi mince",
  thin_island: "îlot mince",
  fragile_tip: "pointe fragile",
  closed_opening: "ouverture bouchée",
  narrow_opening: "ouverture étroite",
  thin_slab: "plaque mince",
  detail_too_small: "détail trop fin",
  envelope_exceeded: "hors du volume utile",
};

const SCOPE_LABELS = {
  profile_not_checked: "relief non vérifié",
  not_run: "contrôle non effectué",
};

const fmt = (x, digits) => x.toLocaleString("fr-FR", { maximumFractionDigits: digits });

function sizeText(p) {
  const s = p.printedSizeMm;
  return Array.isArray(s) && s.length === 3 ? `${s.map((v) => fmt(v, 1)).join(" × ")} mm` : "";
}

// Le fichier est ecrit a 1 mm par unite (GLB declare, convention OBJ / STL) :
// une echelle de controle differente se reporte dans le slicer.
function slicerText(p) {
  if (typeof p.mmPerUnit !== "number" || Math.abs(p.mmPerUnit - 1) < 1e-9) return "";
  return `échelle ${fmt(p.mmPerUnit * 100, 2)} % dans le slicer`;
}

// Defauts du rapport, par categorie : displayCounts (maximum par tranche), sans
// les informations.
export function printDefects(p) {
  return Object.entries(p.displayCounts || {})
    .filter(([k, n]) => n > 0 && !INFORMATION.has(k))
    .map(([k, n]) => ({ kind: k, label: DEFECT_LABELS[k] || k, count: n }));
}

// Resume d'un rapport print-report/v1 : { alert, text }. `alert` vrai : la
// banniere (defaut, portee partielle, erreur, controle interrompu) ; faux : la
// ligne d'etat (echelle non fixee, piece imprimable).
export function describePrint(p) {
  if (p.error) return { alert: true, text: `impression non vérifiée — ${p.error}` };
  if (typeof p.mmPerUnit !== "number")
    return { alert: false, text: "impression non vérifiée : échelle non fixée" };
  const where = [sizeText(p), slicerText(p)].filter(Boolean).join(", ");
  if (p.scope === "not_run")
    return { alert: true, text: `impression non vérifiée — ${p.reason || "contrôle interrompu sans motif"}` };
  // Banniere courte : categories et portee ; le motif de la portee est au
  // detail du controle a la demande.
  const parts = printDefects(p).map((d) => `${d.label} ×${d.count}`);
  if (p.scope !== "complete") parts.push(SCOPE_LABELS[p.scope] || p.scope);
  if (!parts.length) return { alert: false, text: `imprimable à ${where}` };
  return { alert: true, text: `impression à ${where} : ${parts.join(", ")}` };
}

// --- document de l'imprimante -----------------------------------------------

// localStorage, ou null : son simple acces peut lever (stockage refuse par le
// navigateur, page en file://).
export function browserStorage() {
  try {
    return globalThis.localStorage || null;
  } catch {
    return null;
  }
}

export function readStoredConstraints(storage) {
  if (!storage) return null;
  try {
    return storage.getItem(PRINT_CONSTRAINTS_KEY);
  } catch (e) {
    console.warn("Contraintes d'imprimante non relues :", e);
    return null;
  }
}

// Rend false si le document n'a pas pu etre garde ; la page reste utilisable,
// l'imprimante vaut pour la session.
export function writeStoredConstraints(storage, text) {
  if (!storage) return false;
  try {
    storage.setItem(PRINT_CONSTRAINTS_KEY, text);
    return true;
  } catch (e) {
    console.warn("Contraintes d'imprimante non enregistrées :", e);
    return false;
  }
}

// Pose un document print-constraints/v1 cote WASM : { accepted, errors,
// ignored, constraints }. Un binding absent ou en exception est un refus nomme.
export function applyPrintConstraints(Module, text) {
  try {
    return JSON.parse(Module.setPrintConstraints(text));
  } catch (e) {
    return { accepted: false, errors: [{ path: "", message: e.message || String(e) }], ignored: [], constraints: null };
  }
}

// Premier refus d'un rapport de lecture, nomme par son chemin JSON.
export function constraintsRefusal(report) {
  const e = report.errors && report.errors[0];
  if (!e) return "contraintes refusées";
  return e.path ? `${e.path} : ${e.message}` : e.message;
}
