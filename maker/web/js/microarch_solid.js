// ===========================================================================
//  Verdict du solide imprimable de la microarchitecture gothique (J7-d) : texte
//  de la banniere, a partir du resultat de exportMicroarchStl. Fonction pure,
//  sans DOM, testee sous node (maker/probes/microarch_solid_test.mjs).
// ===========================================================================

// Etat en tete de banniere. Un STL non imprimable ou non verifie est tout de meme
// telecharge : l'utilisateur peut le vouloir, la banniere le dit.
export const SOLID_VERDICTS = {
  printable: "imprimable",
  not_printable: "NON imprimable, STL téléchargé quand même : à ne pas imprimer tel quel",
  unverified: "NON VÉRIFIÉ, STL téléchargé sans verdict",
  refused: "refusé, aucun fichier",
};

// Entree du rapport : chemin et message (qui porte la position).
export function describeEntry(e) {
  return e.parameter ? `${e.parameter} : ${e.message}` : e.message;
}

// r : resultat de exportMicroarchStl. Etat, puis F, seuil si non verifie, fichier et
// cotes, puis les entrees du rapport (erreurs d'abord, 12 au plus) avec leur code.
export function describeSolid(r) {
  let report = null;
  try { report = JSON.parse(r.report); } catch { /* rapport absent */ }
  const entries = report && Array.isArray(report.entries) ? [...report.entries] : [];
  entries.sort((a, b) => (b.severity === "error") - (a.severity === "error"));
  const head = [`Solide STL ${SOLID_VERDICTS[r.verdict] || r.verdict}`];
  if (r.failure !== undefined) head.push(`échec — ${r.failure}`);
  if (r.triangles) head.push(`${r.triangles} triangles`);
  if (r.verdict === "unverified") head.push(`au-delà du seuil de vérification (${r.threshold} triangles)`);
  if (r.bytes && r.bytes.length && Array.isArray(r.extentMm))
    head.push(`${r.name}.stl, ${r.extentMm.map((x) => x.toFixed(2)).join(" × ")} mm`);
  const shown = entries.slice(0, 12).map((e) => `${e.code} — ${describeEntry(e)}`);
  if (entries.length > 12) shown.push(`… (${entries.length} entrées)`);
  return [head.join(", "), ...shown].join(" ; ");
}
