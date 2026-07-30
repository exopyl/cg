// ===========================================================================
//  Export du maillage courant : OBJ (texte, produit par le WASM), GLB et STL
//  (binaires, construits ici depuis meshData).
// ===========================================================================
//  Le GLB est assemble en JS : plus compact que l'OBJ, avec normales lissees, et
//  sans dependance (ni tinygltf cote WASM, ni GLTFExporter cote three).
// ===========================================================================

// Nom de fichier : base saisie par l'utilisateur (nettoyee) + extension.
export function safeName(base, ext) {
  let b = (base || "maker").replace(/[\\/:*?"<>|]+/g, "_").trim();
  b = b.replace(/\.(obj|glb|stl)$/i, ""); // evite Torus.obj.obj si l'utilisateur tape l'extension
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

export function downloadObj(Module, id, base) {
  const obj = Module.regenerate(id); // OBJ courant a la demande
  saveBlob(new Blob([obj], { type: "text/plain" }), safeName(base, ".obj"));
}

export function downloadGlb(Module, id, base) {
  const d = Module.meshData(id);
  const positions = new Float32Array(d.positions); // copie hors du tas WASM
  const indices = new Uint32Array(d.indices);
  const normals = computeNormals(positions, indices);
  saveBlob(new Blob([buildGlb(positions, normals, indices)], { type: "model/gltf-binary" }),
           safeName(base, ".glb"));
}

export function downloadStl(Module, id, base) {
  const d = Module.meshData(id);
  const positions = new Float32Array(d.positions);
  const indices = new Uint32Array(d.indices);
  saveBlob(new Blob([buildStl(positions, indices)], { type: "model/stl" }), safeName(base, ".stl"));
}

// --- GLB (glTF binaire) ----------------------------------------------------

function computeNormals(positions, indices) {
  const normals = new Float32Array(positions.length);
  for (let i = 0; i < indices.length; i += 3) {
    const a = indices[i] * 3, b = indices[i + 1] * 3, c = indices[i + 2] * 3;
    const ux = positions[b] - positions[a], uy = positions[b + 1] - positions[a + 1], uz = positions[b + 2] - positions[a + 2];
    const vx = positions[c] - positions[a], vy = positions[c + 1] - positions[a + 1], vz = positions[c + 2] - positions[a + 2];
    const nx = uy * vz - uz * vy, ny = uz * vx - ux * vz, nz = ux * vy - uy * vx;
    normals[a] += nx; normals[a + 1] += ny; normals[a + 2] += nz;
    normals[b] += nx; normals[b + 1] += ny; normals[b + 2] += nz;
    normals[c] += nx; normals[c + 1] += ny; normals[c + 2] += nz;
  }
  for (let i = 0; i < normals.length; i += 3) {
    const l = Math.hypot(normals[i], normals[i + 1], normals[i + 2]) || 1;
    normals[i] /= l; normals[i + 1] /= l; normals[i + 2] /= l;
  }
  return normals;
}

function buildGlb(positions, normals, indices) {
  const align4 = (x) => (x + 3) & ~3;
  const min = [Infinity, Infinity, Infinity], max = [-Infinity, -Infinity, -Infinity];
  for (let i = 0; i < positions.length; i += 3)
    for (let k = 0; k < 3; k++) { const v = positions[i + k]; if (v < min[k]) min[k] = v; if (v > max[k]) max[k] = v; }

  const posB = positions.byteLength, nrmB = normals.byteLength, idxB = indices.byteLength;
  const posOff = 0, nrmOff = align4(posOff + posB), idxOff = align4(nrmOff + nrmB);
  const binLen = align4(idxOff + idxB);

  const gltf = {
    asset: { version: "2.0", generator: "maker (cgmesh)" },
    buffers: [{ byteLength: binLen }],
    bufferViews: [
      { buffer: 0, byteOffset: posOff, byteLength: posB, target: 34962 },
      { buffer: 0, byteOffset: nrmOff, byteLength: nrmB, target: 34962 },
      { buffer: 0, byteOffset: idxOff, byteLength: idxB, target: 34963 },
    ],
    accessors: [
      { bufferView: 0, componentType: 5126, count: positions.length / 3, type: "VEC3", min, max }, // POSITION
      { bufferView: 1, componentType: 5126, count: normals.length / 3, type: "VEC3" },             // NORMAL
      { bufferView: 2, componentType: 5125, count: indices.length, type: "SCALAR" },               // indices (u32)
    ],
    meshes: [{ primitives: [{ attributes: { POSITION: 0, NORMAL: 1 }, indices: 2, mode: 4 }] }],
    nodes: [{ mesh: 0 }],
    scenes: [{ nodes: [0] }],
    scene: 0,
  };

  const jsonBytes = new TextEncoder().encode(JSON.stringify(gltf));
  const jsonChunkLen = align4(jsonBytes.length);
  const bin = new Uint8Array(binLen);
  bin.set(new Uint8Array(positions.buffer, positions.byteOffset, posB), posOff);
  bin.set(new Uint8Array(normals.buffer, normals.byteOffset, nrmB), nrmOff);
  bin.set(new Uint8Array(indices.buffer, indices.byteOffset, idxB), idxOff);

  const total = 12 + 8 + jsonChunkLen + 8 + binLen;
  const out = new ArrayBuffer(total);
  const dv = new DataView(out), u8 = new Uint8Array(out);
  let o = 0;
  dv.setUint32(o, 0x46546c67, true); o += 4; // magic 'glTF'
  dv.setUint32(o, 2, true); o += 4;          // version
  dv.setUint32(o, total, true); o += 4;      // length
  dv.setUint32(o, jsonChunkLen, true); o += 4;
  dv.setUint32(o, 0x4e4f534a, true); o += 4; // 'JSON'
  u8.set(jsonBytes, o); o += jsonBytes.length;
  while (o < 20 + jsonChunkLen) u8[o++] = 0x20; // padding espaces
  dv.setUint32(o, binLen, true); o += 4;
  dv.setUint32(o, 0x004e4942, true); o += 4; // 'BIN\0'
  u8.set(bin, o);
  return out;
}

// --- STL binaire : standard de l'impression 3D -----------------------------
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
