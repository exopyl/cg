// ===========================================================================
//  Export du maillage courant : OBJ (produit par le WASM) et STL (binaire, JS).
// ===========================================================================
//  L'OBJ est serialise cote C++ : la serialisation d'un maillage appartient a
//  au moteur natif, pas a la couche navigateur. Ce module ne garde que ce qui est
//  reellement une preoccupation navigateur -- nommer le fichier et declencher le
//  telechargement.
//
//  Le STL reste assemble ici, faute d'un point d'entree WASM ; c'est un candidat
//  au meme traitement que l'OBJ (MeshIO::export_stl_binary existe deja).
//
//  DEUX CHEMINS le consomment, et ils ne designent pas le maillage de la meme
//  facon : les pages de FORMES tiennent un objet natif identifie (meshData(id)),
//  une page pilotee par un GRAPHE tient un port de sortie (graphMeshData(port)).
//  D'ou deux points d'entree pour un seul encodeur -- c'est la difference que
//  shell.js et template.js portent partout ailleurs.
// ===========================================================================

// Nom de fichier : base saisie par l'utilisateur (nettoyee) + extension.
export function safeName(base, ext) {
  let b = (base || "maker").replace(/[\\/:*?"<>|]+/g, "_").trim();
  b = b.replace(/\.(obj|mtl|stl)$/i, ""); // evite Torus.obj.obj
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
export function downloadObj(Module, id, base) {
  const res = Module.exportObj(id, base || "model");
  const bytes = new Uint8Array(res.bytes);   // copie hors du tas WASM
  if (!bytes.length) return;
  saveBlob(new Blob([bytes], { type: "application/zip" }), res.name + ".zip");
}

export function downloadStl(Module, id, base) {
  const d = Module.meshData(id);
  const positions = new Float32Array(d.positions); // copie hors du tas WASM
  const indices = new Uint32Array(d.indices);
  saveBlob(new Blob([buildStl(positions, indices)], { type: "model/stl" }),
           safeName(base, ".stl"));
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
