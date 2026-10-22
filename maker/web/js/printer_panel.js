// ===========================================================================
//  Controles de l'imprimante (document print-constraints/v1) : seuils, volume
//  utile, import et export. Communs aux pages qui en dependent : la fenetre
//  gothique (controle d'impression) et la microarchitecture (solide STL).
// ===========================================================================
//  L'imprimante appartient a l'utilisateur, pas a la piece : un seul document,
//  garde dans le navigateur sous une seule cle, donc le meme sur toutes les
//  pages. Chaque page a sa propre instance WASM : elle le repose au demarrage,
//  puis a chaque modification faite par une autre page ouverte (evenement storage).
//
//  `root` contient les champs .pc (data-field = cle du document), les trois
//  .pcEnv (x, y, z), #printConstraintsInput et #printConstraintsExportBtn.
// ===========================================================================

import { saveBlob } from "./exporters.js";
import {
  PRINT_CONSTRAINTS_KEY, applyPrintConstraints, browserStorage, constraintsRefusal, readStoredConstraints,
  writeStoredConstraints,
} from "./print.js";

export const PRINTER_BINDINGS = ["setPrintConstraints", "printConstraints"];

// Monte les controles sur `root`. Rend false, sans rien monter, si le module ne
// porte pas les bindings de l'imprimante : la page masque alors le groupe.
// `scaleSource` nomme ce qui fixe l'echelle sur la page : un champ d'echelle du
// document n'est jamais lu, la ligne d'etat le dit.
export function mountPrinterControls(ctx, root, { scaleSource = "la largeur imprimée" } = {}) {
  if (!PRINTER_BINDINGS.every((f) => typeof ctx.Module[f] === "function")) return false;

  const thresholdInputs = [...root.querySelectorAll(".pc")];
  const envelopeInputs  = [...root.querySelectorAll(".pcEnv")];
  const storage = browserStorage();

  function currentPrinter() {
    try { return JSON.parse(ctx.Module.printConstraints()); } catch { return null; }
  }

  // Champs = imprimante en vigueur (defauts compris).
  function showPrinter() {
    const doc = currentPrinter();
    if (!doc) return;
    for (const i of thresholdInputs) i.value = doc[i.dataset.field] ?? "";
    envelopeInputs.forEach((i, k) => { i.value = doc.envelopeMm ? doc.envelopeMm[k] : ""; });
  }

  // Pose un document d'imprimante. Accepte : garde dans le navigateur. Refuse :
  // motif en banniere, imprimante en vigueur inchangee -- l'export n'en depend
  // jamais.
  function setPrinter(text, origin) {
    const r = applyPrintConstraints(ctx.Module, text);
    if (!r.accepted) {
      ctx.banner.report(`${origin} refusée — ${constraintsRefusal(r)} ; imprimante en vigueur inchangée`);
      return false;
    }
    writeStoredConstraints(storage, ctx.Module.printConstraints());
    showPrinter();
    if (r.ignored && r.ignored.length)
      ctx.setStatus(`${origin} : ${r.ignored.join(", ")} ignoré, l'échelle vient de ${scaleSource}`);
    return true;
  }

  // Document des champs : seuil vide -> absent (sa valeur par defaut) ; volume
  // entierement vide -> absent ; un cote vide ou illisible part tel quel, et la
  // lecture stricte le refuse en le nommant (envelopeMm[1]).
  function printerFromFields() {
    const doc = currentPrinter() || { schema: "print-constraints/v1" };
    for (const i of thresholdInputs) {
      const v = i.value.trim();
      if (v === "") delete doc[i.dataset.field];
      else doc[i.dataset.field] = Number(v);
    }
    const env = envelopeInputs.map((i) => i.value.trim());
    if (env.every((v) => v === "")) delete doc.envelopeMm;
    else doc.envelopeMm = env.map((v) => (v === "" ? null : Number(v)));
    return JSON.stringify(doc);
  }

  const stored = readStoredConstraints(storage);
  if (stored !== null) {
    const r = applyPrintConstraints(ctx.Module, stored);
    if (!r.accepted)
      ctx.banner.report(`Imprimante enregistrée refusée — ${constraintsRefusal(r)} ; seuils par défaut`);
  }
  showPrinter();

  // Une autre page du meme navigateur a change l'imprimante : la poser ici aussi, sans
  // reecrire la cle (l'evenement ne parvient qu'aux autres documents, jamais a celui qui
  // ecrit). Cle retiree : l'imprimante en vigueur reste.
  globalThis.addEventListener("storage", (e) => {
    if (e.key !== PRINT_CONSTRAINTS_KEY || e.newValue === null) return;
    const r = applyPrintConstraints(ctx.Module, e.newValue);
    if (!r.accepted) {
      ctx.banner.report(`Imprimante modifiée par une autre page refusée — ${constraintsRefusal(r)} ; ` +
                        "imprimante en vigueur inchangée");
      return;
    }
    showPrinter();
  });

  for (const i of [...thresholdInputs, ...envelopeInputs])
    i.addEventListener("change", () => setPrinter(printerFromFields(), "Imprimante"));
  ctx.registerFileInput(root.querySelector("#printConstraintsInput"), async (file) => {
    if (setPrinter(await file.text(), `Imprimante « ${file.name} »`))
      ctx.setStatus(`Imprimante « ${file.name} » chargée`);
  });
  root.querySelector("#printConstraintsExportBtn").addEventListener("click", () => {
    saveBlob(new Blob([ctx.Module.printConstraints()], { type: "application/json" }), "print-constraints.json");
  });
  return true;
}
