// ===========================================================================
//  Page « Extrusion SVG » : contours d'un fichier SVG tesseles puis extrudes
//  (cf. import_svg.h + extrude_contours.h).
// ===========================================================================

import { createShell, runPage } from "../shell.js";

runPage(async () => {
  const ctx = await createShell({
    title: "Extrusion SVG",
    subtitle: "contours vectoriels · tessellation · extrusion",
  });

  const svgInput = document.getElementById("svgInput");

  // Le SVG arrive en TEXTE (contrairement a l'image, binaire) : le WASM l'ecrit en
  // MEMFS car l'importeur natif travaille par chemin de fichier.
  ctx.registerFileInput(svgInput, async (file) => {
    const text = await file.text();
    ctx.destroyCurrent();
    const id = ctx.Module.createSvgExtrusion(text);
    if (id < 0) { ctx.setStatus("SVG illisible"); return; }
    // bootstrap : recadrage camera fiable sur un gros saut d'echelle.
    ctx.setShape(id, file.name.replace(/\.svg$/i, ""), { bootstrap: true });
  });

  ctx.setStatus("Importe un fichier SVG pour commencer.");
});
