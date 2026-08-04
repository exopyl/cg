// ===========================================================================
//  Page « Formes paramétriques » : catalogue des surfaces analytiques, voxels,
//  fractales et L-systemes de cgmesh.
// ===========================================================================

import { createShell, runPage } from "../shell.js";

runPage(async () => {
  const ctx = await createShell({
    title: "Formes paramétriques",
    subtitle: "surfaces, voxels, fractales, L-systèmes",
  });

  const catalogEl = document.getElementById("catalog");

  // Le catalogue vient du C++ (listShapes("parametric")) : pas de liste de noms
  // dupliquee ici, ajouter une forme ne demande d'editer que wasm_api.cpp. Chaque
  // entree porte son groupe, qui devient un <optgroup> -- les entrees consecutives
  // de meme groupe partagent le leur, l'ordre du catalogue fait donc l'ordre des
  // sections.
  const entries = JSON.parse(ctx.Module.listShapes("parametric"));
  const shapes = entries.map((e) => e.name);
  let group = null;
  let target = catalogEl;
  for (const e of entries) {
    if (e.group !== group) {
      group = e.group;
      if (group) {
        target = document.createElement("optgroup");
        target.label = group;
        catalogEl.appendChild(target);
      } else {
        target = catalogEl;
      }
    }
    target.appendChild(new Option(e.name, e.name));
  }

  function selectShape(name, opts) {
    ctx.destroyCurrent();
    const id = ctx.Module.createShape(name);
    if (id < 0) { ctx.setStatus("forme inconnue"); return; }
    ctx.setShape(id, name, opts);
  }

  catalogEl.addEventListener("change", () => selectShape(catalogEl.value));

  // Deep-link optionnel : ?shape=Torus selectionne une forme au chargement, et
  // toute autre cle nommant un parametre de cette forme est appliquee avant le
  // premier rendu (deepLink) -- utile pour les captures automatisees.
  const wanted = new URLSearchParams(location.search).get("shape");
  const initial = shapes.includes(wanted) ? wanted : shapes[0];
  if (initial) {
    catalogEl.value = initial;
    selectShape(initial, { deepLink: true });
  }
});
