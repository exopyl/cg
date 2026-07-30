// ===========================================================================
//  Chargement du module WASM, partage par les quatre pages de generation.
// ===========================================================================
//  Un SEUL maker.wasm sert toutes les pages (cf. ANALYSE.md sec.15) : le noyau
//  geometrique commun domine le binaire, le decouper en un module par page le
//  dupliquerait autant de fois.
// ===========================================================================

import createMakerModule from "../maker.js";

// Bindings Embind dont l'UI depend. Verifies au demarrage : si le navigateur
// execute un maker.js/maker.wasm d'avant la derniere reconstruction, un binding
// recent est `undefined` et l'echec se produirait plus tard, silencieusement, au
// premier clic. Mieux vaut le NOMMER tout de suite.
const REQUIRED_BINDINGS = [
  "listShapes", "createShape", "createSvgExtrusion", "createImageRelief",
  "createGothicFromJson", "exportGothicJson", "getParams", "setParam",
  "regenerate", "meshData", "destroyShape",
  "createImagePixelBlocks", "exportPixelBlocksObj",
];

// Renvoie { Module, staleMessage }. `staleMessage` est non nul quand le module
// charge est plus ancien que le code JS : l'appelant l'affiche en banniere.
export async function loadModule() {
  const Module = await createMakerModule();
  const missing = REQUIRED_BINDINGS.filter((f) => typeof Module[f] !== "function");
  const staleMessage = missing.length
    ? `Module WASM obsolète (cache navigateur) : ${missing.join(", ")} absent(s). ` +
      `Rechargez avec Ctrl+Shift+R, ou relancez maker\\build.ps1.`
    : null;
  return { Module, staleMessage };
}
