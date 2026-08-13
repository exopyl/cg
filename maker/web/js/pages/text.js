// ===========================================================================
//  Page « Texte 3D » : une police a contours + une chaine -> solide extrude
//  (cf. cgmath/font.h, cgmath/text_layout.h, cgmesh/text_extrude.h).
// ===========================================================================
//
// La page n'a qu'UN controle propre : le choix de la police. Le texte, lui, est
// un parametre de la forme (Parameter::STRING), donc un widget du panneau
// generique construit par panel.js -- au meme titre que la profondeur ou le
// corps, et il se suit a la frappe. La police est le seul reglage qui n'y ait
// pas sa place : c'est un fichier, et la changer reconstruit l'objet.

import { createShell, runPage } from "../shell.js";

runPage(async () => {
  const ctx = await createShell({
    title: "Texte 3D",
    subtitle: "police · mise en page · extrusion",
  });

  const fontInput = document.getElementById("fontInput");
  const fontInfo  = document.getElementById("fontInfo");

  // Texte de DEPART seulement : ensuite c'est le panneau qui le pilote. La
  // valeur par defaut cote C++ ("Text") n'est pas reprise ici, une page qui
  // ouvre sur son propre titre se lit mieux.
  const kInitialText = "Texte 3D";

  function build(bytes, name) {
    ctx.destroyCurrent();
    // Police en Uint8Array (binaire), texte en std::string (embind le convertit
    // en UTF-8, ce que la mise en page attend). Cf. createTextExtrusion.
    const id = ctx.Module.createTextExtrusion(bytes, name, kInitialText);
    if (id < 0) {
      ctx.setStatus("Police illisible, ou aucun glyphe a extruder");
      return;
    }
    // Le nom de fichier suggere vient de la POLICE : le texte va changer a
    // chaque frappe, un nom qui le suivrait serait instable.
    // bootstrap : recadrage camera fiable sur un gros saut d'echelle.
    ctx.setShape(id, name.replace(/\.[^.]+$/, ""), { bootstrap: true });
  }

  // Une police est BINAIRE : Uint8Array, jamais de texte -- la conversion UTF-8
  // en detruirait les octets.
  ctx.registerFileInput(fontInput, async (file) => {
    const bytes = new Uint8Array(await file.arrayBuffer());
    fontInfo.textContent = `${file.name} — ${bytes.length} octets`;
    build(bytes, file.name);
  });

  // --------------------------------------------------------------------------
  // Police par defaut
  // --------------------------------------------------------------------------
  // La page ouvre sur un resultat plutot que sur un viewer vide. Le fichier est
  // copie de test/data/fonts/ par le build (cf. maker/CMakeLists.txt) ; il est du
  // domaine public, d'ou sa redistribution. Tout echec est silencieux et laisse
  // la page utilisable : fichier absent parce que le build n'a pas tourne, ou
  // serveur qui ne sert pas data/.
  async function loadDefaultFont() {
    try {
      const res = await fetch("data/BloomingGrove.otf", { cache: "no-store" });
      if (!res.ok) return false;
      const bytes = new Uint8Array(await res.arrayBuffer());
      fontInfo.textContent = `BloomingGrove.otf (par défaut) — ${bytes.length} octets`;
      build(bytes, "BloomingGrove.otf");
      return true;
    } catch { return false; }
  }

  if (!await loadDefaultFont())
    ctx.setStatus("Choisis une police pour commencer.");
});
