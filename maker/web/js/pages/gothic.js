// ===========================================================================
//  Page « Fenêtre gothique » : remplage parametrique (Gothic Window) et bloc de
//  maconnerie (Gothic Block), plus l'aller-retour JSON du descripteur.
// ===========================================================================

import { createShell, runPage } from "../shell.js";
import { saveBlob, safeName } from "../exporters.js";

runPage(async () => {
  const ctx = await createShell({
    title: "Fenêtre gothique",
    subtitle: "remplage paramétrique · schéma gothic-window-v2",
  });

  const catalogEl = document.getElementById("catalog");
  const jsonInput = document.getElementById("gothicJsonInput");

  const shapes = JSON.parse(ctx.Module.listShapes("gothic"));
  for (const s of shapes) catalogEl.appendChild(new Option(s, s));

  function selectShape(name, opts) {
    ctx.destroyCurrent();
    const id = ctx.Module.createShape(name);
    if (id < 0) { ctx.setStatus("forme inconnue"); return; }
    ctx.setShape(id, name, opts);
  }

  catalogEl.addEventListener("change", () => selectShape(catalogEl.value));

  // Import d'une Gothic Window depuis un fichier de description JSON (schema
  // gothic-window-v2) : parse cote cgmesh, la geometrie est generee par l'engine.
  // Le panneau reflete les valeurs chargees (getParams lit les membres remplis).
  ctx.registerFileInput(jsonInput, async (file) => {
    const text = await file.text();
    ctx.destroyCurrent();
    const id = ctx.Module.createGothicFromJson(text);
    if (id < 0) { ctx.setStatus("JSON illisible"); return; }
    // Reflete la provenance dans le catalogue, et bootstrap : le recadrage camera
    // doit rester fiable sur un gros saut d'echelle (bloc ~1 -> fenetre ~500).
    catalogEl.appendChild(new Option(`JSON : ${file.name}`, "__gothicjson__", true, true));
    ctx.setShape(id, file.name.replace(/\.json$/i, ""), { bootstrap: true });
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

  // Deep-link : ?shape=Gothic%20Window&Rosette%20foil%20count=18&Offset%20outer=40
  const wanted = new URLSearchParams(location.search).get("shape");
  const initial = shapes.includes(wanted) ? wanted : shapes[0];
  if (initial) {
    catalogEl.value = initial;
    selectShape(initial, { deepLink: true });
  }
});
