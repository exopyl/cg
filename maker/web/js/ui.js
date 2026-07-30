// ===========================================================================
//  Surfaces d'interaction communes : remontee d'erreurs et <input type=file>.
// ===========================================================================

// --------------------------------------------------------------------------
// Remontee d'erreurs
// --------------------------------------------------------------------------
// Une erreur doit se VOIR. Sans surface d'erreur, une exception dans une
// fonction async appelee depuis un addEventListener finit en unhandled promise
// rejection : elle part dans la console et nulle part ailleurs, et l'UI reste
// figee sur son message d'attente ("rien ne se passe").
export function createBanner(bannerEl, setStatus) {
  // Avertissement d'environnement pose au demarrage (o3dv absent, module WASM
  // obsolete) : il reste valable toute la session, donc clear() ne doit PAS
  // l'effacer au premier import reussi.
  let sticky = false;

  return {
    report(msg, err) {
      if (err) console.error(err);
      setStatus(msg);
      bannerEl.hidden = false;
      bannerEl.textContent = err ? `${msg} — ${err.message || err}` : msg;
    },
    // Variante HTML, pour les messages qui contiennent du <code>.
    reportHtml(html) {
      bannerEl.hidden = false;
      bannerEl.innerHTML = html;
    },
    clear() {
      if (sticky) return;
      bannerEl.hidden = true;
      bannerEl.textContent = "";
    },
    // A appeler une fois l'amorcage termine : fige l'etat courant de la banniere.
    seal() { sticky = !bannerEl.hidden; },
  };
}

// --------------------------------------------------------------------------
// Imports fichier
// --------------------------------------------------------------------------
// Enveloppe un <input type=file> : erreurs remontees, double import empeche, et
// re-selection du MEME fichier possible (sans reset de .value, 'change' ne
// re-declenche pas et l'utilisateur croit l'UI morte).
//
// `setEnabled` desactive TOUS les inputs enregistres pendant l'import : la chaine
// complete tourne en SYNCHRONE dans le WASM, deux imports concurrents n'auraient
// pas de sens et masqueraient l'origine d'une erreur.
export function createFileInputs(banner) {
  const inputs = [];

  function setEnabled(on) {
    for (const i of inputs) i.disabled = !on;
  }

  return {
    register(input, handler) {
      inputs.push(input);
      input.addEventListener("change", async () => {
        if (!input.files.length) return;
        const file = input.files[0];
        input.value = "";
        setEnabled(false);
        try {
          banner.clear();
          await handler(file);
        } catch (e) {
          banner.report(`Import « ${file.name} » a échoué`, e);
        } finally {
          setEnabled(true);
        }
      });
    },
    setEnabled,
  };
}
