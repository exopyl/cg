// Entrée du bundle vendor de maker.
//
// Les DEUX espaces de noms sortent du MÊME graphe de modules, donc esbuild
// n'inclut qu'un seul three : celui qu'o3dv importe est exactement celui qu'on
// réexporte. C'est tout l'objet de ce fichier — la build UMD d'o3dv embarquait
// three sans rien en exposer, et il n'existait aucun moyen d'obtenir une
// THREE.Texture pour poser un `material.map`.
export * as OV from 'online-3d-viewer';
export * as THREE from 'three';
