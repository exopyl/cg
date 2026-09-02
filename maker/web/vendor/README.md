# vendor/ — provenance des bundles

## `o3dv-three.module.js` (le bundle courant)

Bundle **ES module** d'[Online3DViewer](https://github.com/kovacsv/Online3DViewer)
et de three.js, qui exporte les deux : `OV` et `THREE`.

### Pourquoi un bundle maison plutôt que le fichier publié

La build publiée `build/engine/o3dv.min.js` (UMD) embarque three.js **sans rien
en exposer** : ses 262 exports ne contiennent aucune classe three, et le
minifieur en a renommé les classes. Impossible, donc, d'obtenir une
`THREE.Texture` pour poser un `material.map` — et sans texture, un maillage
d'image ne peut montrer que des aplats.

La build publiée `build/engine/o3dv.module.js` (ESM) importe bien `three` en
externe, mais aussi sept loaders `three/examples/jsm` et `fflate` ; un import map
manuel devrait donc vendorer toute la cascade, jusqu'à `chevrotain` que
`VRMLLoader` tire.

D'où ce bundle : **un seul graphe de modules, donc un seul three**. Celui
qu'o3dv importe est exactement celui qu'on réexporte — c'est vérifiable, la
marque `isDataTexture` n'apparaît qu'une fois dans le fichier.

### Comment le régénérer

Les deux fichiers `o3dv-three.entry.js` et `o3dv-three.package.json` sont la
recette complète. Dans un répertoire de travail **hors du dépôt** :

```sh
cp o3dv-three.package.json <tmp>/package.json
cp o3dv-three.entry.js     <tmp>/entry.js
cd <tmp> && npm install
npx esbuild entry.js --bundle --format=esm --minify --legal-comments=none \
    --outfile=o3dv-three.module.js
```

puis recopier le résultat ici. `node_modules` n'entre jamais dans le dépôt : on
ne versionne que l'artefact et sa recette, comme avant.

Versions épinglées : `online-3d-viewer@0.18.0`, `three@0.176.0` (la version dont
o3dv dépend — ne pas la faire diverger, o3dv ne déclare pas three en `peer`).

## `o3dv.min.js` (l'ancien, conservé)

La build UMD utilisée jusqu'au passage à l'ESM, dans une version antérieure non
identifiée (262 exports, ne correspond à aucune release 0.12 → 0.18). Gardée
pour que le retour arrière reste trivial ; plus aucune page ne la charge.
