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

  // listShapes renvoie des objets {name, group} ; les deux formes gothiques n'ont
  // pas de groupe, la liste reste plate.
  const shapes = JSON.parse(ctx.Module.listShapes("gothic")).map((e) => e.name);
  for (const s of shapes) catalogEl.appendChild(new Option(s, s));

  function selectShape(name, opts) {
    ctx.destroyCurrent();
    const id = ctx.Module.createShape(name);
    if (id < 0) { ctx.setStatus("forme inconnue"); return; }
    ctx.setShape(id, name, opts);
  }

  catalogEl.addEventListener("change", () => selectShape(catalogEl.value));

  // Import d'une Gothic Window depuis un fichier de description JSON (schema
  // gothic-window-v2) : parse cote natif, la geometrie est generee par l'engine.
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
    if (id < 0) return false;
    catalogEl.appendChild(
      new Option("Modele par defaut (JSON)", "__gothicdefault__", true, true));
    // bootstrap : le descripteur travaille en centaines d unites, le recadrage
    // camera doit repartir de zero (meme raison que pour un import manuel).
    ctx.setShape(id, "gothic-window-instance", { bootstrap: true });
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
