// ===========================================================================
//  Page « Cinq ordres » : colonnes de l'architecture classique d'apres Vignole
//  (Classical Column), donnees des cinq ordres embarquees dans le module.
// ===========================================================================

import { createShell, runPage } from "../shell.js";

runPage(async () => {
  const ctx = await createShell({
    title: "Cinq ordres",
    subtitle: "colonnes et colonnades d'après Vignole · toscan à composite",
  });

  const catalogEl = document.getElementById("catalog");

  // Catalogue de la categorie "classical" (classical_parametric_catalog.cpp) :
  // Classical Column et Classical Colonnade, liste plate, sans groupe.
  const shapes = JSON.parse(ctx.Module.listShapes("classical")).map((e) => e.name);
  for (const s of shapes) catalogEl.appendChild(new Option(s, s));

  function selectShape(name, opts) {
    ctx.destroyCurrent();
    const id = ctx.Module.createShape(name);
    if (id < 0) { ctx.setStatus("forme inconnue"); return; }
    ctx.setShape(id, name, opts);
  }

  catalogEl.addEventListener("change", () => selectShape(catalogEl.value));

  // Avertissement de taille d'une colonnade (shapeWarnings, wasm_api.cpp),
  // relu apres chaque affichage. Un module anterieur au binding n'en a pas.
  if (typeof ctx.Module.shapeWarnings === "function") {
    ctx.onShapeShown((id) => {
      if (id < 0) return;
      const warning = ctx.Module.shapeWarnings(id);
      if (warning) ctx.banner.report(`Maillage lourd : ${warning}`);
      else ctx.banner.clear();
    });
  }

  // Deep-link : ?shape=Classical%20Column&Order=3 (Order : indice dans
  // Tuscan, Doric, Ionic, Corinthian, Composite).
  const wanted = new URLSearchParams(location.search).get("shape");
  const initial = shapes.includes(wanted) ? wanted : shapes[0];
  if (initial) {
    catalogEl.value = initial;
    selectShape(initial, { deepLink: true });
  } else {
    ctx.setStatus("catalogue classique vide : module WASM antérieur à la page ?");
  }
});
