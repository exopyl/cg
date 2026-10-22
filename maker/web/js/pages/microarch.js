// ===========================================================================
//  Page « Micro architecture gothique » : import d'une specification
//  gothic-microarchitecture/v1, reseau de courbes, tubes d'affichage, exports
//  du reseau et solide imprimable (STL).
// ===========================================================================
//  Le resultat est le RESEAU de courbes (JSON gothic-microarchitecture-network/v1,
//  copie en chaine JS au chargement, et OBJ des courbes) ; le maillage n'est que
//  son affichage tubulaire. Les exports du pied de page sont refuses par le WASM
//  (microarchExportGuard). L'objet est hors catalogue : la page n'a pas de liste
//  de formes, elle s'ouvre sans modele et attend un import.
// ===========================================================================

import { createShell, runPage } from "../shell.js";
import { saveBlob } from "../exporters.js";
import { describeEntry, describeSolid } from "../microarch_solid.js";
import { mountPrinterControls } from "../printer_panel.js";

const MICROARCH_MAX_BYTES = 4 * 1024 * 1024;   // SpecLimits::max_text_bytes
const MICROARCH_BINDINGS = ["createMicroarchFromJson", "microarchTubeReport", "microarchOrigin", "exportMicroarchObj"];

runPage(async () => {
  const ctx = await createShell({
    title: "Micro architecture gothique",
    subtitle: "réseau de courbes · schéma gothic-microarchitecture/v1",
  });

  const el = (id) => document.getElementById(id);
  const microGroup   = el("microarchGroup");
  const microInput   = el("microarchJsonInput");
  const microDetails = el("microarchDetails");
  const microBtns    = ["microarchNetworkBtn", "microarchCurvesBtn", "microarchTubesBtn"].map(el);
  const solidGroup   = el("solidGroup");
  const solidBtn     = el("microarchSolidBtn");
  const solidInputs  = ["solidRadius", "solidCells", "solidScale"].map(el);
  const printGroup   = el("printGroup");
  const hintEl       = el("viewerHint");
  microDetails.style.whiteSpace = "pre-line";

  const hasMicroarch = MICROARCH_BINDINGS.every((f) => typeof ctx.Module[f] === "function");
  const hasSolid     = hasMicroarch && typeof ctx.Module.exportMicroarchStl === "function";

  // L'imprimante ne sert qu'au solide : paroi minimale et volume utile.
  if (!hasSolid || !mountPrinterControls(ctx, printGroup, { scaleSource: "l'échelle du solide" }))
    printGroup.hidden = true;
  if (!hasSolid) solidGroup.hidden = true;

  if (!hasMicroarch) {
    microGroup.hidden = true;
    ctx.banner.report("Module WASM sans microarchitecture gothique : reconstruire maker.wasm");
    return;
  }

  // Le viseur reste vide jusqu'au premier import : il n'y a pas de modele par defaut.
  if (ctx.viewer.available && !hintEl.hidden)
    hintEl.textContent = "Importer une microarchitecture (JSON) depuis le panneau.";

  // Etat de la microarchitecture courante : id -1 quand il n'y en a pas.
  const micro = { id: -1, name: "", network: "", loadText: "", shownText: null };

  function parseReport(text) {
    try { return JSON.parse(text); } catch { return null; }
  }

  function firstError(report) {
    const e = report && report.entries.find((x) => x.severity === "error");
    return e ? `${e.code} — ${describeEntry(e)}` : "";
  }

  // Avertissements groupes par code, chacun avec ses chemins (8 au plus).
  function describeWarnings(report) {
    if (!report) return "";
    const byCode = new Map();
    for (const e of report.entries.filter((x) => x.severity === "warning")) {
      if (!byCode.has(e.code)) byCode.set(e.code, []);
      byCode.get(e.code).push(e.parameter || e.message);
    }
    const parts = [];
    for (const [code, paths] of byCode) {
      const shown = paths.slice(0, 8).join(", ") + (paths.length > 8 ? `, … (${paths.length})` : "");
      parts.push(`${code} : ${shown}`);
    }
    return parts.join(" ; ");
  }

  function forgetMicroarch() {
    const wasShown = micro.shownText;
    Object.assign(micro, { id: -1, name: "", network: "", loadText: "", shownText: null });
    microDetails.textContent = "";
    for (const b of microBtns) b.disabled = true;
    solidBtn.disabled = true;
    if (wasShown) ctx.banner.clear();   // la banniere de la microarchitecture ne lui survit pas
  }

  // Nom de base des exports, filtre comme cote C++ (exportMicroarchObj).
  function microBase() {
    const raw = el("filename").value || micro.name;
    return raw.replace(/[^A-Za-z0-9_-]/g, "") || "microarchitecture";
  }

  // Apres chaque affichage : rapport des tubes (refus ou avertissement) et rapport du
  // chargement, en banniere tant qu'ils ont quelque chose a dire.
  function showMicroarchReports(id) {
    if (id < 0 || id !== micro.id) return;
    const tubes = parseReport(ctx.Module.microarchTubeReport(id));
    const err = firstError(tubes);
    const warn = describeWarnings(tubes);
    const tubeText = err ? `Tubes non construits, réseau conservé — ${err}` : warn ? `Tubes : ${warn}` : "";
    const origin = ctx.Module.microarchOrigin(id);
    microDetails.textContent = [
      micro.loadText ? `Réseau : ${micro.loadText}` : "Réseau : sans avertissement",
      tubeText || "Tubes : construits",
      origin ? `Origine des sommets affichés : ${origin.map((x) => x.toPrecision(10)).join(", ")}` : "",
    ].filter(Boolean).join("\n");
    const text = [micro.loadText && `Microarchitecture chargée avec avertissements — ${micro.loadText}`, tubeText]
      .filter(Boolean).join(" ; ");
    if (text === micro.shownText) return;
    micro.shownText = text;
    if (text) ctx.banner.report(text);
    else ctx.banner.clear();
  }

  ctx.onShapeShown(showMicroarchReports);

  ctx.registerFileInput(microInput, async (file) => {
    // Avant la lecture : un texte trop long n'est pas copie dans le tas WASM. Le
    // controle natif (json_too_large) reste celui qui fait foi.
    if (file.size > MICROARCH_MAX_BYTES) {
      ctx.banner.report(`Microarchitecture « ${file.name} » refusée — json_too_large : ` +
                        `${file.size} octets, au plus ${MICROARCH_MAX_BYTES}`);
      return;
    }
    const text = await file.text();
    // Un import refuse garde le modele precedent (M42) : la nouvelle microarchitecture
    // est creee AVANT toute destruction, et un refus ne touche qu'a la banniere. Le tas
    // porte donc un instant l'ancien objet et le nouveau, tubes compris (Load les
    // construit).
    ctx.setStatus("Microarchitecture : chargement…");
    const r = ctx.Module.createMicroarchFromJson(text);
    if (r.failure !== undefined) {
      ctx.banner.report(`Microarchitecture « ${file.name} » : échec — ${r.failure}`);
      return;
    }
    const load = parseReport(r.report);
    if (r.id < 0) {
      ctx.banner.report(`Microarchitecture « ${file.name} » refusée — ${firstError(load) || "rapport illisible"}`);
      return;
    }
    // Accepte : l'etat de l'ancienne part, setShape detruit l'ancien objet.
    forgetMicroarch();
    micro.id = r.id;
    micro.name = file.name.replace(/\.json$/i, "");
    micro.network = r.network;
    micro.loadText = describeWarnings(load);
    for (const b of microBtns) b.disabled = false;
    solidBtn.disabled = !hasSolid;
    // Jamais d'amorcage o3dv par OBJ texte : regenerate() rend "" pour une
    // microarchitecture (l'OBJ du maillage entier porterait le tas WASM au-dela de
    // 1 Gio au plafond de triangles). Viseur pas encore amorce : le shell l'amorce
    // sur un triangle, puis affiche par meshData. Viseur amorce : meshData en
    // place, recadre sur la boite.
    ctx.setShape(r.id, micro.name, { bootstrap: false });
  });

  microBtns[0].addEventListener("click", () => {
    if (micro.id < 0 || !micro.network) return;
    saveBlob(new Blob([micro.network], { type: "application/json" }), `${microBase()}_network.json`);
  });
  for (const [btn, kind] of [[microBtns[1], "curves"], [microBtns[2], "tubes"]]) {
    btn.addEventListener("click", () => {
      if (micro.id < 0) return;
      let r;
      try {
        r = ctx.Module.exportMicroarchObj(micro.id, microBase(), kind);
      } catch (e) {
        ctx.banner.report(`Export ${kind} impossible`, e);
        return;
      }
      const bytes = new Uint8Array(r.bytes);   // copie hors du tas WASM
      if (!bytes.length) {
        ctx.banner.report(`Export ${kind} refusé — ${firstError(parseReport(r.report)) || "pas de géométrie"}`);
        return;
      }
      saveBlob(new Blob([bytes], { type: "model/obj" }), `${r.name}.obj`);
      ctx.setStatus(`Export ${kind} : ${r.name}.obj`);
    });
  }

  // Solide imprimable : construit au clic par le WASM, verifie jusqu'au seuil de M33,
  // serialise en STL puis libere ; rien n'est garde ni affiche. Verdict en banniere.
  solidBtn.addEventListener("click", async () => {
    if (micro.id < 0 || solidBtn.disabled) return;
    const [radius, cells, scale] = solidInputs.map((i) => (i.value.trim() === "" ? NaN : Number(i.value)));
    solidBtn.disabled = true;
    ctx.setStatus("Solide : construction et vérification…");
    // Deux images : la ligne d'etat est peinte avant le calcul, qui bloque la page.
    await new Promise((resolve) => requestAnimationFrame(() => requestAnimationFrame(resolve)));
    let r;
    try {
      r = ctx.Module.exportMicroarchStl(micro.id, microBase(), radius, cells, scale);
    } catch (e) {
      ctx.banner.report("Solide STL impossible", e);
      return;
    } finally {
      solidBtn.disabled = micro.id < 0;
    }
    const text = describeSolid(r);
    if (r.bytes.length) saveBlob(new Blob([r.bytes], { type: "model/stl" }), `${r.name}.stl`);
    ctx.banner.report(text);
  });
});
