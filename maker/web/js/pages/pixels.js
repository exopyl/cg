// ===========================================================================
//  Page « Blocs pixelisés » : quantification, pixelisation au vote majoritaire,
//  segmentation en blocs connexes puis extrusion (cf. cgmesh/image_pixel_blocks.h).
// ===========================================================================

import { createShell, runPage } from "../shell.js";
import { saveBlob, safeName } from "../exporters.js";

runPage(async () => {
  const ctx = await createShell({
    title: "Blocs pixelisés",
    subtitle: "pixelisation · segmentation · pièces séparables",
  });

  const imageInput  = document.getElementById("imageInput");
  const previewBox  = document.getElementById("imagePreviewBox");
  const preview     = document.getElementById("imagePreview");
  const previewInfo = document.getElementById("imagePreviewInfo");

  // --------------------------------------------------------------------------
  // Aperçu de l'image source — la référence à laquelle comparer la pixelisation.
  // --------------------------------------------------------------------------
  let previewUrl = null;

  function showPreview(file) {
    clearPreview();
    previewUrl = URL.createObjectURL(file);
    preview.src = previewUrl;
    previewInfo.textContent = file.name;
    preview.onload = () => {
      previewInfo.textContent =
        `${file.name} — ${preview.naturalWidth}×${preview.naturalHeight} px`;
    };
    previewBox.hidden = false;
  }

  function clearPreview() {
    // Libérer le blob : sans revoke, chaque import fuite l'image précédente.
    if (previewUrl) URL.revokeObjectURL(previewUrl);
    previewUrl = null;
    preview.removeAttribute("src");
    previewInfo.textContent = "";
    previewBox.hidden = true;
  }

  ctx.registerFileInput(imageInput, async (file) => {
    // Aperçu AVANT le travail lourd : la chaîne est synchrone et gèle le thread,
    // c'est le seul moment où le navigateur peut encore peindre.
    showPreview(file);
    ctx.setStatus("Pixelisation en cours…");
    // Laisse le navigateur peindre le message avant le blocage.
    //
    // setTimeout et PAS requestAnimationFrame : rAF ne se déclenche pas dans un
    // onglet masqué ou occulté, ce qui bloquerait l'import indéfiniment (champs
    // désactivés, message d'attente figé) dès que la fenêtre perd le premier plan.
    await new Promise((resolve) => setTimeout(resolve, 0));

    // Octets bruts : une image est binaire, la convertir en texte la détruirait.
    const bytes = new Uint8Array(await file.arrayBuffer());
    ctx.destroyCurrent();
    const id = ctx.Module.createImagePixelBlocks(bytes, file.name);
    if (id < 0) {
      clearPreview();   // rien n'a été produit : pas d'aperçu trompeur
      ctx.setStatus("Image illisible ou sans région");
      return;
    }
    // bootstrap : recadrage caméra fiable sur un gros saut d'échelle.
    ctx.setShape(id, file.name.replace(/\.[^.]+$/, ""), { bootstrap: true });
  });

  // --------------------------------------------------------------------------
  // Export par bloc : UN fichier OBJ portant un groupe `o block_NNNN_color_NN`
  // par bloc connexe. Tout slicer sait les séparer, et ça n'ajoute aucune
  // dépendance zip côté navigateur.
  // --------------------------------------------------------------------------
  const exportBtn = document.createElement("button");
  exportBtn.textContent = "OBJ par blocs";
  exportBtn.addEventListener("click", () => {
    const id = ctx.currentId();
    if (id < 0) return;
    ctx.setStatus("Découpe en blocs…");
    // Comme l'import : la chaîne est rejouée en synchrone côté WASM, on laisse le
    // navigateur peindre le message avant le gel.
    setTimeout(() => {
      try {
        const text = ctx.Module.exportPixelBlocksObj(id);
        if (!text) { ctx.setStatus("Export par blocs : forme non pixelisée"); return; }
        const nBlocks = (text.match(/^o /gm) || []).length;
        saveBlob(new Blob([text], { type: "text/plain" }),
                 safeName(document.getElementById("filename").value + "_blocks", ".obj"));
        ctx.setStatus(`${nBlocks} blocs exportés`);
      } catch (e) {
        ctx.banner.report("Export par blocs a échoué", e);
      }
    }, 0);
  });
  ctx.footerExtras.appendChild(exportBtn);

  ctx.setStatus("Importe une image pour commencer.");
});
