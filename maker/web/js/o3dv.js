// ===========================================================================
//  Point d'entree UNIQUE vers Online3DViewer et three.js
// ===========================================================================
//
// Le bundle vendor exporte les DEUX -- `OV` et `THREE` -- issus du meme graphe
// de modules, donc d'un seul three. Voir vendor/README.md pour la raison et la
// recette : la build UMD publiee embarquait three sans rien en exposer, et il
// n'existait aucun moyen d'obtenir une THREE.Texture pour poser un
// `material.map`. Sans texture, un maillage d'image ne montre que des aplats.
//
// DEGRADATION : le vendor est un artefact volumineux, absent d'un depot fraichement
// cloné selon la façon dont il a été récupéré. La page doit rester utilisable
// sans lui -- generation et telechargements marchent, seule la vue 3D manque.
// C'est ce que faisait `onerror` sur la balise <script> ; un import statique,
// lui, casserait tout le module. D'ou l'import DYNAMIQUE sous try/catch.
//
// `await` au niveau du module : le chargement du module importateur attend, ce
// qui est exactement le comportement de l'ancienne balise <script> synchrone
// placee avant le module de la page.

let mod = null;
try {
	mod = await import("../vendor/o3dv-three.module.js");
} catch (e) {
	console.error("vendor/o3dv-three.module.js introuvable ou illisible :", e);
}

export const OV = mod ? mod.OV : null;
export const THREE = mod ? mod.THREE : null;
export const isAvailable = mod !== null;
