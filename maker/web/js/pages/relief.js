// ===========================================================================
//  Page « Image to puzzle » : quantification des couleurs, vectorisation
//  des regions puis extrusion (cf. image_relief.h).
// ===========================================================================

import { createShell, runPage } from "../shell.js";

runPage(async () => {
  const ctx = await createShell({
    title: "Image to puzzle",
    subtitle: "quantification · vectorisation · extrusion",
  });

  const imageInput      = document.getElementById("imageInput");
  const previewBox      = document.getElementById("imagePreviewBox");
  const preview         = document.getElementById("imagePreview");
  const previewInfo     = document.getElementById("imagePreviewInfo");

  // --------------------------------------------------------------------------
  // Apercu de l'image source
  // --------------------------------------------------------------------------
  // Affiche l'image importee a cote de ses parametres : c'est la reference a
  // laquelle on compare la segmentation, et les dimensions en pixels donnent
  // l'echelle de « Min region area » (exprime en px de la source).
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
    // Liberer le blob : sans revoke, chaque import fuite l'image precedente.
    if (previewUrl) URL.revokeObjectURL(previewUrl);
    previewUrl = null;
    preview.removeAttribute("src");
    previewInfo.textContent = "";
    previewBox.hidden = true;
  }

  // Les octets sont passes en Uint8Array : une image est binaire, la convertir en
  // texte la detruirait.
  const importImage = ctx.registerFileInput(imageInput, async (file) => {
    // Affiche l'apercu AVANT le travail lourd : la chaine est synchrone et gele le
    // thread, donc c'est le seul moment ou le navigateur peut encore peindre.
    showPreview(file);
    ctx.setStatus("Vectorisation en cours…");
    // Laisse le navigateur peindre le message avant le blocage : la chaine
    // complete (load + quantification + vectorisation) tourne en synchrone dans le
    // WASM et gele le thread principal plusieurs secondes sur une grande image.
    //
    // setTimeout et PAS requestAnimationFrame : rAF ne se declenche pas dans un
    // onglet masque ou occulte, ce qui bloquerait l'import indefiniment (champs
    // desactives, message d'attente fige) des que la fenetre perd le premier plan.
    await new Promise((resolve) => setTimeout(resolve, 0));

    const bytes = new Uint8Array(await file.arrayBuffer());
    ctx.destroyCurrent();
    const id = ctx.Module.createImageRelief(bytes, file.name);
    if (id < 0) {
      clearPreview();   // rien n'a ete produit : pas d'apercu trompeur
      ctx.setStatus("Image illisible ou sans région");
      return;
    }
    // bootstrap : recadrage camera fiable sur un gros saut d'echelle.
    ctx.setShape(id, file.name.replace(/\.[^.]+$/, ""), { bootstrap: true });
  });

  // --------------------------------------------------------------------------
  // Image par defaut
  // --------------------------------------------------------------------------
  // La page ouvre sur une image plutot que sur un viewer vide. Le fichier est
  // copie de test/data/jpg/ par le build (cf. maker/CMakeLists.txt).
  //
  // On passe par importImage() et non par le handler nu : c'est le meme enrobage
  // que la selection manuelle (desactivation des champs pendant le calcul,
  // banniere d'erreur). fetch -> Blob -> File, parce que la chaine attend un File
  // (.arrayBuffer(), .name, et createObjectURL pour l'apercu).
  //
  // Tout echec est silencieux et laisse la page utilisable : image absente parce
  // que le build n'a pas tourne, ou serveur qui ne sert pas data/.
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
