// ===========================================================================
//  Page « Blocs pixelisés » : quantification, pixelisation au vote majoritaire,
//  segmentation en blocs connexes puis extrusion (cf. image_pixel_blocks.h).
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

  const importImage = ctx.registerFileInput(imageInput, async (file) => {
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
  // Export par bloc : un OBJ portant un groupe `o block_NNNN_color_NN` par bloc
  // connexe, plus son .mtl compagnon — les deux dans une seule archive ZIP, donc
  // un seul téléchargement.
  //
  // Sérialisé côté natif (MeshIO::export_obj_zip_bytes) sur les blocs fusionnés.
  // C'est le C++ qui nomme les entrées internes, puisqu'il dérive la ligne
  // `mtllib` du chemin de sortie ; on reprend donc le nom qu'il renvoie.
  // --------------------------------------------------------------------------
  const exportBtn = document.createElement("button");
  exportBtn.textContent = "ZIP par blocs (OBJ+MTL)";
  exportBtn.addEventListener("click", () => {
    const id = ctx.currentId();
    if (id < 0) return;
    ctx.setStatus("Découpe en blocs…");
    // Comme l'import : la chaîne est rejouée en synchrone côté WASM, on laisse le
    // navigateur peindre le message avant le gel.
    setTimeout(() => {
      try {
        const base = document.getElementById("filename").value + "_blocks";
        const res = ctx.Module.exportPixelBlocks(id, base);
        // Vue typée sur le tas WASM : la copie est obligatoire.
        const bytes = new Uint8Array(res.bytes);
        if (!bytes.length) { ctx.setStatus("Export par blocs : forme non pixelisée"); return; }
        saveBlob(new Blob([bytes], { type: "application/zip" }), res.name + ".zip");
        ctx.setStatus(`${res.blocks} blocs exportés (ZIP)`);
      } catch (e) {
        ctx.banner.report("Export par blocs a échoué", e);
      }
    }, 0);
  });
  ctx.footerExtras.appendChild(exportBtn);

  // --------------------------------------------------------------------------
  // Image par défaut
  // --------------------------------------------------------------------------
  // La page ouvre sur une image plutôt que sur un viewer vide. Le fichier est
  // copié de test/data/jpg/ par le build (cf. maker/CMakeLists.txt).
  //
  // On passe par importImage() et non par le handler nu : c'est le même enrobage
  // que la sélection manuelle (désactivation des champs pendant le calcul,
  // bannière d'erreur). fetch -> Blob -> File, parce que la chaîne attend un File
  // (.arrayBuffer(), .name, et createObjectURL pour l'aperçu).
  //
  // Tout échec est silencieux et laisse la page utilisable : image absente parce
  // que le build n'a pas tourné, ou serveur qui ne sert pas data/.
  async function loadDefaultImage() {
    try {
      const res = await fetch("data/joconde.jpg", { cache: "no-store" });
      if (!res.ok) return false;
      const blob = await res.blob();
      await importImage(new File([blob], "joconde.jpg", { type: "image/jpeg" }));
      return true;
    } catch { return false; }
  }

  if (!await loadDefaultImage())
    ctx.setStatus("Importe une image pour commencer.");
});
