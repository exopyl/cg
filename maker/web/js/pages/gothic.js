// ===========================================================================
//  Page « Fenêtre gothique » : remplage parametrique (Gothic Window) et bloc de
//  maconnerie (Gothic Block), plus l'aller-retour JSON du descripteur, et la
//  garde d'impression resine (largeur imprimee, imprimante, controle).
// ===========================================================================

import { createShell, runPage } from "../shell.js";
import { saveBlob, safeName } from "../exporters.js";
import { describePrint, guardVerdict, printDefects } from "../print.js";
import { mountPrinterControls } from "../printer_panel.js";

runPage(async () => {
  const ctx = await createShell({
    title: "Fenêtre gothique",
    subtitle: "remplage paramétrique · schéma gothic-window-v2",
  });

  const catalogEl = document.getElementById("catalog");
  const jsonInput = document.getElementById("gothicJsonInput");

  // listShapes renvoie des objets {name, group} ; les deux formes gothiques n'ont
  // pas de groupe, la liste reste plate.
  const shapes = JSON.parse(ctx.Module.listShapes("gothic")).map((e) => e.name);
  for (const s of shapes) catalogEl.appendChild(new Option(s, s));

  // --- impression resine -----------------------------------------------------
  // La largeur imprimee appartient a la PIECE : tenue par objet cote WASM
  // (setPrintedWidthMm), effacee a chaque nouvelle forme. L'imprimante (seuils,
  // volume utile) appartient a l'utilisateur : document print-constraints/v1,
  // garde dans le navigateur, valable pour toutes les fenetres.
  const printGroup  = document.getElementById("printGroup");
  const widthInput  = document.getElementById("printedWidth");
  const checkBtn    = document.getElementById("printCheckBtn");
  const detailsEl   = document.getElementById("printDetails");
  detailsEl.style.whiteSpace = "pre-line";
  const hasPrint = ["setPrintConstraints", "printConstraints", "setPrintedWidthMm", "gothicPrintReport", "gothicReport"]
    .every((f) => typeof ctx.Module[f] === "function");

  function applyPrintedWidth() {
    const id = ctx.currentId();
    if (id < 0 || !hasPrint) return;
    const v = widthInput.value.trim();
    const mm = v === "" ? NaN : Number(v);
    ctx.Module.setPrintedWidthMm(id, mm);
    detailsEl.textContent = "";
    if (v === "") ctx.setStatus("Largeur imprimée effacée : échelle non fixée");
    else if (!(Number.isFinite(mm) && mm > 0))
      ctx.setStatus(`Largeur imprimée invalide (${v}) : impression non vérifiée, export non bloqué`);
    else ctx.setStatus(`Largeur imprimée : ${mm} mm`);
  }

  // Detail du dernier controle : portee, taille, echelle, comptes affiches
  // (displayCounts, maximum par tranche).
  function showDetails(v) {
    detailsEl.textContent = "";
    if (!v || !v.print) return;
    const p = v.print;
    const lines = [`Portée : ${p.scope}${p.reason ? ` (${p.reason})` : ""}`];
    if (Array.isArray(p.printedSizeMm))
      lines.push(`Taille imprimée : ${p.printedSizeMm.map((x) => x.toFixed(2)).join(" × ")} mm`);
    if (typeof p.mmPerUnit === "number") lines.push(`Échelle : ${p.mmPerUnit.toPrecision(4)} mm par unité`);
    for (const d of printDefects(p)) lines.push(`${d.label} : ${d.count}`);
    if (p.printable) lines.push("Imprimable");
    detailsEl.textContent = lines.join("\n");
  }

  function reportCheck(v) {
    if (!v) { ctx.setStatus("Contrôle : forme non gothique"); return; }
    if (v.refused !== undefined) { ctx.banner.report(`Contrôle : géométrie refusée — ${v.refused}`); return; }
    if (v.printFailure !== undefined) { ctx.banner.report(`Contrôle d'impression indisponible — ${v.printFailure}`); return; }
    const d = describePrint(v.print);
    const strictness = v.warning !== undefined ? ` ; maillage non strict — ${v.warning}` : "";
    if (d.alert || strictness) ctx.banner.report(`Contrôle : ${d.text}${strictness}`);
    else { ctx.banner.clear(); ctx.setStatus(`Contrôle : ${d.text}`); }
  }

  if (hasPrint) {
    mountPrinterControls(ctx, printGroup);
    widthInput.addEventListener("change", applyPrintedWidth);
    checkBtn.addEventListener("click", () => {
      const id = ctx.currentId();
      if (id < 0) return;
      let v;
      try {
        v = guardVerdict(ctx.Module.gothicPrintReport(id));
      } catch (e) {
        ctx.banner.report("Contrôle d'impression impossible", e);
        return;
      }
      showDetails(v);
      reportCheck(v);
    });
  } else {
    printGroup.hidden = true;
  }

  // Option de provenance du catalogue (import JSON, modele par defaut) : une seule
  // par valeur, selectionnee.
  function setProvenance(label, value) {
    for (const o of [...catalogEl.options]) if (o.value === value) o.remove();
    catalogEl.appendChild(new Option(label, value, true, true));
  }

  // Adopte la forme `id` (setShape) : nouvelle piece, largeur imprimee non
  // fixee ; controle propose aux seules Gothic Window.
  function adopt(id, name, opts) {
    ctx.setShape(id, name, opts);
    widthInput.value = "";
    detailsEl.textContent = "";
    const isWindow = hasPrint && ctx.Module.gothicReport(id) !== "";
    widthInput.disabled = !isWindow;
    checkBtn.disabled = !isWindow;
  }

  function selectShape(name, opts) {
    ctx.destroyCurrent();
    const id = ctx.Module.createShape(name);
    if (id < 0) { ctx.setStatus("forme inconnue"); return; }
    adopt(id, name, opts);
  }

  // La banniere decrit le chargement precedent : elle ne survit pas au changement
  // de forme.
  catalogEl.addEventListener("change", () => {
    ctx.banner.clear();
    selectShape(catalogEl.value);
  });

  // Rapports du chargement JSON (GothicBuildReportToJson : compteurs + entrees
  // {severity, code, parameter, message}). Un module d'avant ces bindings charge
  // sans les afficher.
  const hasReports = typeof ctx.Module.gothicReport === "function";

  function describeEntry(e) {
    return e.parameter ? `${e.parameter} : ${e.message}` : e.message;
  }

  // Refus : la premiere erreur, nommee par son chemin JSON.
  function refusedMessage() {
    if (!hasReports) return "JSON illisible";
    const report = JSON.parse(ctx.Module.lastGothicLoadReport());
    const first = report.entries.find((e) => e.severity === "error");
    return first ? `JSON refusé — ${describeEntry(first)}` : "JSON refusé";
  }

  // Succes : champs ignores ou convertis ; "" s'il n'y en a pas.
  function loadWarnings(id) {
    if (!hasReports) return "";
    const { load } = JSON.parse(ctx.Module.gothicReport(id));
    const count = Object.values(load.warnings).reduce((a, b) => a + b, 0);
    if (!count) return "";
    const paths = load.entries.filter((e) => e.severity === "warning").map((e) => e.parameter);
    return `JSON chargé avec ${count} avertissement(s), champs ignorés ou convertis : ${paths.join(", ")}`;
  }

  // Import d'une Gothic Window depuis un fichier de description JSON (schema
  // gothic-window-v2, ou v1 converti) : parse cote natif, la geometrie est
  // generee par l'engine. Le panneau reflete les valeurs chargees (getParams lit
  // les membres remplis).
  ctx.registerFileInput(jsonInput, async (file) => {
    const text = await file.text();
    ctx.destroyCurrent();
    const id = ctx.Module.createGothicFromJson(text);
    if (id < 0) { ctx.banner.report(refusedMessage()); return; }
    // Reflete la provenance dans le catalogue, et bootstrap : le recadrage camera
    // doit rester fiable sur un gros saut d'echelle (bloc ~1 -> fenetre ~500).
    setProvenance(`JSON : ${file.name}`, "__gothicjson__");
    adopt(id, file.name.replace(/\.json$/i, ""), { bootstrap: true });
    const warnings = loadWarnings(id);
    if (warnings) ctx.banner.report(warnings);
  });

  // Export du descripteur JSON refletant les valeurs courantes de l'UI.
  const exportBtn = document.createElement("button");
  exportBtn.textContent = "Export Gothic Window (JSON)";
  exportBtn.addEventListener("click", () => {
    const id = ctx.currentId();
    if (id < 0) return;
    const text = ctx.Module.exportGothicJson(id);
    // Vide si la forme active n'est pas une Gothic Window (ex. Gothic Block).
    if (!text) { ctx.setStatus("Export JSON : forme non gothique"); return; }
    saveBlob(new Blob([text], { type: "application/json" }),
             safeName(document.getElementById("filename").value, ".json"));
  });
  ctx.footerExtras.appendChild(exportBtn);

  // Charge le descripteur servi comme modele par defaut. Copie de
  // test/data/gothic-window-instance.json par le build (cf. maker/CMakeLists.txt) :
  // la page ouvre donc sur une fenetre complete -- deux lancettes, rosette a six
  // foils -- plutot que sur les valeurs par defaut de la classe.
  //
  // Renvoie true si le modele a pu etre installe. Tout echec (fichier absent parce
  // que le build n a pas tourne, JSON refuse par le parseur natif) laisse la place
  // au catalogue : une page qui affiche quelque chose vaut mieux qu une page vide.
  async function selectDefaultDescriptor() {
    let text;
    try {
      const res = await fetch("data/gothic-window-instance.json", { cache: "no-store" });
      if (!res.ok) return false;
      text = await res.text();
    } catch { return false; }

    ctx.destroyCurrent();
    const id = ctx.Module.createGothicFromJson(text);
    // Le descripteur par defaut n'affiche pas de banniere : son rapport part en
    // console, la banniere reste aux imports de l'utilisateur.
    if (id < 0) { console.warn(refusedMessage()); return false; }
    setProvenance("Modele par defaut (JSON)", "__gothicdefault__");
    // bootstrap : le descripteur travaille en centaines d unites, le recadrage
    // camera doit repartir de zero (meme raison que pour un import manuel).
    adopt(id, "gothic-window-instance", { bootstrap: true });
    const warnings = loadWarnings(id);
    if (warnings) console.info(warnings);
    return true;
  }

  // Deep-link : ?shape=Gothic%20Window&Rosette%20foil%20count=18&Offset%20outer=40
  // Un ?shape= explicite l emporte sur le descripteur par defaut -- c est une
  // demande nommee, et les captures automatisees en dependent.
  const wanted = new URLSearchParams(location.search).get("shape");
  if (shapes.includes(wanted)) {
    catalogEl.value = wanted;
    selectShape(wanted, { deepLink: true });
  } else if (!(await selectDefaultDescriptor()) && shapes.length) {
    catalogEl.value = shapes[0];
    selectShape(shapes[0], { deepLink: true });
  }
});
