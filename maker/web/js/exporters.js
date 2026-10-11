// ===========================================================================
//  Export du maillage courant : OBJ et GLB (produits par le WASM), STL (JS).
// ===========================================================================
//  L'OBJ et le GLB sont serialises cote C++ : la serialisation d'un maillage --
//  materiaux compris -- appartient au moteur natif. Ce module ne garde que ce qui
//  est une preoccupation navigateur : nommer le fichier et declencher le
//  telechargement.
//
//  Le STL reste assemble ici, faute d'un point d'entree WASM ; c'est un candidat
//  au meme traitement que l'OBJ (MeshIO::export_stl_binary existe deja). Il
//  passe par la MEME garde qu'OBJ et GLB (gothicPrintReport) : refus sur erreur
//  de construction, avertissement d'impression.
//
//  DEUX CHEMINS le consomment, et ils ne designent pas le maillage de la meme
//  facon : les pages de FORMES tiennent un objet natif identifie (meshData(id)),
//  une page pilotee par un GRAPHE tient un port de sortie (graphMeshData(port)).
//  D'ou deux points d'entree pour un seul encodeur -- c'est la difference que
//  shell.js et template.js portent partout ailleurs.
// ===========================================================================

import { guardVerdict, describePrint } from "./print.js";

// Nom de fichier : base saisie par l'utilisateur (nettoyee) + extension.
export function safeName(base, ext) {
  let b = (base || "maker").replace(/[\\/:*?"<>|]+/g, "_").trim();
  b = b.replace(/\.(obj|mtl|stl|glb)$/i, ""); // evite Torus.obj.obj
  return (b || "maker") + ext;
}

// Telechargement. Si le navigateur a "Toujours demander ou enregistrer",
// il ouvrira sa propre boite "Enregistrer sous" avec ce nom pre-rempli.
export function saveBlob(blob, name) {
  const a = document.createElement("a");
  a.href = URL.createObjectURL(blob);
  a.download = name;
  a.click();
  URL.revokeObjectURL(a.href);
}

// Un OBJ ne peut pas embarquer ses materiaux : il les reference par `mtllib`, donc
// un export colore compte forcement deux fichiers. Le moteur natif les livre zippes
// (MeshIO::export_obj_zip_bytes), ce qui ramene l'operation a UN telechargement.
//
// Les octets arrivent en vue typee sur le tas WASM, valide seulement jusqu'au
// prochain appel : la copie est obligatoire, pas une precaution.
//
// Rend le verdict de la garde d'export d'une fenetre gothique (exportVerdict).
// Un binding en exception n'a rien produit : refus, motif rendu (exportFailed).
export function downloadObj(Module, id, base) {
  let res;
  try {
    res = Module.exportObj(id, base || "model");
  } catch (e) {
    return exportFailed(e);
  }
  const bytes = new Uint8Array(res.bytes);   // copie hors du tas WASM
  if (bytes.length)
    saveBlob(new Blob([bytes], { type: "application/zip" }), res.name + ".zip");
  return exportVerdict(res);
}

// Verdict de la garde d'export d'une Gothic Window (gothicExportGuard, cote
// WASM) : { refused } quand le rapport de construction porte une erreur (rien
// n'est telecharge), { warning } quand le maillage exporte n'est pas strict,
// { print } le rapport d'impression (print-report/v1), { printFailure } quand
// il n'a pas pu etre produit ou lu ; null pour toute autre forme.
export function exportVerdict(res) {
  return guardVerdict(res);
}

// Binding d'export ou de garde en exception : rien n'est telecharge, comme pour
// un refus. Le motif reste neutre : l'exception peut venir de la garde comme de
// l'ecriture des octets, et de toute forme ; une geometrie non jugee par la
// garde arrive, elle, en refus nomme du WASM (guardVerdict).
function exportFailed(e) {
  console.error(e);
  return { refused: `export impossible (${(e && e.message) || e})` };
}

// Affiche un verdict d'export : refus, maillage non strict, defaut d'impression,
// portee partielle ou erreur en banniere (createBanner, ui.js) ; sinon la
// banniere est effacee -- sauf un avertissement d'environnement scelle (seal),
// que clear() preserve -- et l'etat de l'impression (echelle non fixee, piece
// imprimable) va dans la ligne d'etat.
export function reportExportVerdict(banner, verdict, setStatus) {
  if (verdict && verdict.refused !== undefined) {
    banner.report(`Export refusé — ${verdict.refused}`);
    return;
  }
  const alerts = [], notes = [];
  if (verdict && verdict.warning !== undefined) alerts.push(`maillage non strict — ${verdict.warning}`);
  if (verdict && verdict.printFailure !== undefined)
    alerts.push(`contrôle d'impression indisponible — ${verdict.printFailure}`);
  else if (verdict && verdict.print) {
    const d = describePrint(verdict.print);
    (d.alert ? alerts : notes).push(d.text);
  }
  if (alerts.length) {
    banner.report(`Export effectué, ${alerts.join(" ; ")}`);
    return;
  }
  banner.clear();
  if (notes.length && setStatus) setStatus(`Export effectué · ${notes.join(" · ")}`);
}

// Meme archive, source nodale : `port` est un port de sortie du graphe, pas un
// identifiant d'objet. Rend false quand le port ne porte pas de maillage --
// l'appelant a alors quelque chose a dire a l'utilisateur, ce qu'un export muet
// ne permettrait pas.
export function downloadObjFromGraph(Module, port, base) {
  const res = Module.graphExportObjZip(port, base || "model");
  const bytes = new Uint8Array(res.bytes);   // copie hors du tas WASM
  if (!bytes.length) return false;
  saveBlob(new Blob([bytes], { type: "application/zip" }), res.name + ".zip");
  return true;
}

// GLB (glTF binaire) : UN SEUL fichier, materiaux compris, la ou l'OBJ reference
// son .mtl et impose une archive. Il porte aussi sa conversion d'unite et
// d'orientation, sur son noeud de scene.
//
// Meme regle de copie que l'OBJ : la vue typee rendue par le WASM n'est valide
// que jusqu'au prochain appel. Meme verdict rendu.
export function downloadGlb(Module, id, base) {
  let res;
  try {
    res = Module.exportGlb(id, base || "model");
  } catch (e) {
    return exportFailed(e);
  }
  const bytes = new Uint8Array(res.bytes);   // copie hors du tas WASM
  if (bytes.length)
    saveBlob(new Blob([bytes], { type: "model/gltf-binary" }), res.name + ".glb");
  return exportVerdict(res);
}

// Meme fichier, source nodale : `port` est un port de sortie du graphe, pas un
// identifiant d'objet. Rend false quand le port ne porte pas de maillage.
export function downloadGlbFromGraph(Module, port, base) {
  const res = Module.graphExportGlb(port, base || "model");
  const bytes = new Uint8Array(res.bytes);   // copie hors du tas WASM
  if (!bytes.length) return false;
  saveBlob(new Blob([bytes], { type: "model/gltf-binary" }), res.name + ".glb");
  return true;
}

// Garde d'abord (gothicPrintReport : Regenerate puis verdict, sans octets),
// comme OBJ et GLB : un refus ne telecharge rien, une garde en exception non
// plus (exportFailed). L'echec du seul controle d'impression arrive en
// printFailure et laisse partir le STL. Rend le verdict (exportVerdict).
export function downloadStl(Module, id, base) {
  let verdict;
  try {
    verdict = guardVerdict(Module.gothicPrintReport(id));
  } catch (e) {
    return exportFailed(e);
  }
  if (verdict && verdict.refused !== undefined) return verdict;
  const d = Module.meshData(id);
  const positions = new Float32Array(d.positions); // copie hors du tas WASM
  const indices = new Uint32Array(d.indices);
  saveBlob(new Blob([buildStl(positions, indices)], { type: "model/stl" }),
           safeName(base, ".stl"));
  return verdict;
}

// Meme encodeur, source nodale : `port` est un port de sortie du graphe, pas un
// identifiant d'objet. Rend false quand le port ne porte pas de maillage --
// l'appelant a alors quelque chose a dire a l'utilisateur, ce qu'un export muet
// ne permettrait pas.
export function downloadStlFromGraph(Module, port, base) {
  const d = Module.graphMeshData(port);
  const positions = new Float32Array(d.positions); // copie hors du tas WASM
  const indices = new Uint32Array(d.indices);
  if (!indices.length) return false;
  saveBlob(new Blob([buildStl(positions, indices)], { type: "model/stl" }),
           safeName(base, ".stl"));
  return true;
}

// Encombrement du maillage, dans les UNITES MONDE du document -- donc en
// millimetres pour cette page (cf. le gabarit). `positions` est un tableau plat
// x,y,z : la fonction ne fait AUCUN appel au Module, elle peut donc lire une vue
// typee sur le tas WASM sans la copier.
export function meshExtent(positions) {
  if (!positions || positions.length < 3) return null;
  const lo = [Infinity, Infinity, Infinity];
  const hi = [-Infinity, -Infinity, -Infinity];
  for (let i = 0; i + 2 < positions.length; i += 3)
    for (let a = 0; a < 3; a++) {
      const v = positions[i + a];
      if (v < lo[a]) lo[a] = v;
      if (v > hi[a]) hi[a] = v;
    }
  return { x: hi[0] - lo[0], y: hi[1] - lo[1], z: hi[2] - lo[2] };
}

// --- STL binaire : standard de l'impression 3D -----------------------------
// Le format ne porte pas de couleur : rien a exporter de la palette.
function buildStl(positions, indices) {
  const nTri = indices.length / 3;
  const buf = new ArrayBuffer(84 + nTri * 50); // 80o header + u32 count + 50o/triangle
  const dv = new DataView(buf);
  dv.setUint32(80, nTri, true);
  let o = 84;
  const put = (x, y, z) => { dv.setFloat32(o, x, true); dv.setFloat32(o + 4, y, true); dv.setFloat32(o + 8, z, true); o += 12; };
  for (let i = 0; i < indices.length; i += 3) {
    const a = indices[i] * 3, b = indices[i + 1] * 3, c = indices[i + 2] * 3;
    const ax = positions[a], ay = positions[a + 1], az = positions[a + 2];
    const bx = positions[b], by = positions[b + 1], bz = positions[b + 2];
    const cx = positions[c], cy = positions[c + 1], cz = positions[c + 2];
    let nx = (by - ay) * (cz - az) - (bz - az) * (cy - ay);
    let ny = (bz - az) * (cx - ax) - (bx - ax) * (cz - az);
    let nz = (bx - ax) * (cy - ay) - (by - ay) * (cx - ax);
    const l = Math.hypot(nx, ny, nz) || 1; nx /= l; ny /= l; nz /= l;
    put(nx, ny, nz); put(ax, ay, az); put(bx, by, bz); put(cx, cy, cz);
    dv.setUint16(o, 0, true); o += 2;
  }
  return buf;
}
