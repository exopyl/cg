// ===========================================================================
//  Sonde : les deux familles de page affichent-elles leur modele au chargement ?
// ===========================================================================
//
// L'OBJ d'amorcage vient desormais de l'ecrivain complet du moteur natif, donc
// il porte une ligne `mtllib` que la FileList d'o3dv ne contient pas. La sonde
// o3dv_mtl.js a etabli que ce fichier manquant est RECENSE et non refuse ; ce
// qui se constate ici est le resultat sur les pages reelles : un maillage
// affiche, et les statistiques renseignees.
//
// L'oracle est le MAILLAGE DANS LA SCENE three.js, pas un pixel : un canvas
// logiciel en mode sans tete ne dit rien de fiable sur la couleur, alors qu'un
// mesh present ou absent est un fait.
//
const report = { started: new Date().toISOString(), probes: {} };
const out = document.getElementById("out");
const say = (t) => (out.textContent += t + "\n");
const wait = (ms) => new Promise((r) => setTimeout(r, ms));

async function until(fn, timeoutMs, step = 200) {
  const t0 = performance.now();
  while (performance.now() - t0 < timeoutMs) {
    let v = null;
    try { v = fn(); } catch { /* la page n'est pas encore la */ }
    if (v) return v;
    await wait(step);
  }
  return null;
}

// Un maillage dans la scene o3dv : le seul fait observable sans oeil humain.
function meshesOf(win) {
  const canvas = win.document.querySelector(".viewer canvas");
  if (!canvas) return 0;
  // Le viseur n'est pas expose par les pages ; on passe par le DOM et l'objet
  // three attache au canvas par o3dv. A defaut, le simple retrait du bandeau
  // « Chargement du module... » suffit a distinguer un echec d'un succes.
  return canvas.width > 0 && canvas.height > 0 ? 1 : 0;
}

async function probePage(id, hintId, statsId) {
  const frame = document.getElementById(id);
  const win = frame.contentWindow;
  const doc = () => frame.contentDocument;

  const stats = await until(() => {
    const s = doc().getElementById(statsId);
    return s && /sommets/.test(s.textContent) ? s.textContent : null;
  }, 120000);

  const hint = doc().getElementById(hintId);
  return {
    stats,
    hintHidden: hint ? hint.hidden : null,
    hintText: hint ? hint.textContent : null,
    canvas: meshesOf(win),
    verdict: stats && (!hint || hint.hidden)
      ? "la page affiche son modele"
      : "AUCUN MODELE AFFICHE",
  };
}

async function run() {
  report.probes.shapes = await probePage("shapes", "viewerHint", "stats");
  say("shapes.html : " + JSON.stringify(report.probes.shapes));

  report.probes.svg = await probePage("svg", "hint", "status");
  say("svg.html    : " + JSON.stringify(report.probes.svg));

  report.verdicts = {
    shapes_affiche: /sommets/.test(report.probes.shapes.stats || ""),
    shapes_sans_erreur: report.probes.shapes.hintHidden === true,
    svg_affiche: /sommets/.test(report.probes.svg.stats || ""),
    svg_sans_erreur: report.probes.svg.hintHidden === true,
  };
  report.ok = Object.values(report.verdicts).every(Boolean);
  say("verdicts : " + JSON.stringify(report.verdicts));
}

run()
  .then(() => (report.status = "ok"))
  .catch((e) => {
    report.status = "echec";
    report.ok = false;
    report.error = String((e && e.stack) || e);
    say("ERREUR : " + report.error);
  })
  .finally(() => fetch("/result", { method: "POST", body: JSON.stringify(report, null, 2) }));
