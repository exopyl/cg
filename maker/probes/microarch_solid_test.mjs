// Texte de la banniere du solide imprimable (maker/web/js/microarch_solid.js), sous node :
//   node maker/probes/microarch_solid_test.mjs
// Sortie 0 si tous les controles passent.
import { describeSolid } from "../web/js/microarch_solid.js";

let failures = 0;
function check(name, cond, detail) {
  console.log(`${cond ? "PASS" : "FAIL"} ${name}${detail !== undefined ? " : " + detail : ""}`);
  if (!cond) failures++;
}

const report = (entries) => JSON.stringify({ entries });
const stl = new Uint8Array(84 + 50 * 3);

// Seuil injecte (2^21) : au seuil de Maker, 2^22 = plafond de triangles (M36), aucun solide
// accepte n'est non verifie ; le libelle reste teste par une valeur injectee (M38 Q-A6).
const unverified = describeSolid({
  verdict: "unverified", triangles: 3492944, threshold: 2097152, name: "dense_solid", bytes: stl, extentMm: [10, 20, 30],
  report: report([{ code: "solid_unverified", parameter: "solid.verdict", message: "3492944 triangles > 2097152", severity: "warning" }]),
});
check("unverified: state first", unverified.startsWith("Solide STL NON VÉRIFIÉ, STL téléchargé sans verdict, 3492944 triangles"), unverified);
check("unverified: threshold given", unverified.includes("au-delà du seuil de vérification (2097152 triangles)"), unverified);
check("unverified: file and code listed", unverified.includes("dense_solid.stl, 10.00 × 20.00 × 30.00 mm") &&
      unverified.includes("solid_unverified — solid.verdict : 3492944 triangles > 2097152"), unverified);

const wall = "tube diameter 0.064 mm < minWallMm 0.4 mm ; minimum scale 6.25 mm per drawing unit (minWallMm / 2r), or a larger radius";
const thin = describeSolid({
  verdict: "not_printable", triangles: 478640, threshold: 2097152, name: "d4_solid", bytes: stl, extentMm: [1, 1, 1],
  report: report([{ code: "solid_wall_too_thin", parameter: "solid.radius_ratio", message: wall, severity: "warning" }]),
});
check("not printable: says the STL is downloaded anyway",
      thin.startsWith("Solide STL NON imprimable, STL téléchargé quand même : à ne pas imprimer tel quel"), thin);
check("not printable: minimum scale shown", thin.includes("minimum scale 6.25 mm per drawing unit"), thin);
check("not printable: no threshold", !thin.includes("seuil"), thin);

const ok = describeSolid({ verdict: "printable", triangles: 47632, threshold: 2097152, name: "d4_solid", bytes: stl, extentMm: [1, 2, 3],
                           report: report([]) });
check("printable", ok === "Solide STL imprimable, 47632 triangles, d4_solid.stl, 1.00 × 2.00 × 3.00 mm", ok);

const many = Array.from({ length: 14 }, (_, k) => ({ code: `w${k}`, message: `m${k}`, severity: "warning" }));
many.push({ code: "solid_limit_exceeded", parameter: "solid.triangles", message: "more than 4194304 triangles", severity: "error" });
const refused = describeSolid({ verdict: "refused", triangles: 0, threshold: 2097152, name: "", bytes: new Uint8Array(0), report: report(many) });
check("refused: errors first, no file", refused.startsWith("Solide STL refusé, aucun fichier ; solid_limit_exceeded — solid.triangles"), refused);
check("refused: 12 entries at most", refused.includes("… (15 entrées)") && !refused.includes("w11 —"), refused);

console.log(failures ? `${failures} FAILURE(S)` : "ALL PASS");
process.exit(failures ? 1 : 0);
