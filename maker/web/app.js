// ===========================================================================
//  maker — logique de la page : panneau de parametres + Online3DViewer
// ===========================================================================
//  Charge le module WASM (createMakerModule), peuple le catalogue, construit le
//  panneau depuis getParams(), et sur chaque edition : setParam -> regenerate
//  (OBJ) -> (re)chargement dans Online3DViewer. Voir maker/ANALYSE.md sec.5/6.
// ===========================================================================

import createMakerModule from "./maker.js";

const el = (id) => document.getElementById(id);
const catalogEl = el("catalog");
const paramsEl  = el("params");
const statsEl   = el("stats");
const viewerEl  = el("viewer");
const hintEl    = el("viewerHint");
const bannerEl  = el("banner");
const svgInput  = el("svgInput");
const downloadBtn = el("download");
const downloadGlbBtn = el("downloadGlbBtn");
const downloadStlBtn = el("downloadStlBtn");
const filenameInput = el("filename");
const modelColorInput = el("modelColor");
const bgColorInput = el("bgColor");
const wireframeInput = el("wireframe");
const resetViewBtn = el("resetView");

let Module = null;
let currentId = -1;        // id de l'objet cgmesh courant
let currentName = "maker"; // nom de la forme courante (nom de fichier suggere)
let viewer = null;         // instance Online3DViewer (ou null si absent)

// Reference vers le maillage three.js affiche + constructeurs three (captures
// depuis o3dv apres le premier chargement). Servent a mettre a jour la
// geometrie EN PLACE pendant l'edition, sans repasser par le reload de fichier
// d'o3dv (qui vide la scene / cache le canvas / recadre la camera = flickering).
let meshRef = null, GeomCtor = null, AttrCtor = null, IdxAttrCtor = null;

// --------------------------------------------------------------------------
// Online3DViewer : init + chargement d'un OBJ en memoire. Degrade proprement
// si le global OV est absent (fichier vendor/o3dv.min.js non fourni).
// --------------------------------------------------------------------------
function initViewer() {
  if (window.__o3dvMissing || typeof window.OV === "undefined") {
    bannerEl.hidden = false;
    bannerEl.innerHTML =
      "Online3DViewer introuvable. Place <code>o3dv.min.js</code> dans " +
      "<code>maker/web/vendor/</code> pour activer le rendu 3D. " +
      "Le panneau et la génération restent fonctionnels (télécharge l'OBJ).";
    hintEl.textContent = "Rendu 3D indisponible (o3dv.min.js manquant).";
    return false;
  }
  try {
    viewer = new OV.EmbeddedViewer(viewerEl, {
      backgroundColor: new OV.RGBAColor(20, 22, 26, 255),
      defaultColor: new OV.RGBColor(180, 190, 200),
      onModelLoaded: captureMesh,
    });
    // Rotation libre (trackball) au lieu de l'orbite a vecteur up fixe, qui
    // "verrouille" la rotation autour de l'axe vertical.
    viewer.GetViewer().SetNavigationMode(OV.NavigationMode.FreeOrbit);
    return true;
  } catch (e) {
    console.error(e);
    hintEl.textContent = "Erreur d'initialisation du viewer : " + e.message;
    return false;
  }
}

// Chargement complet (via le pipeline o3dv) : utilise UNIQUEMENT au changement
// de forme / import SVG. Recadre la camera sur la nouvelle geometrie. C'est la
// seule voie qui "flicke", mais c'est une action ponctuelle, pas le drag.
function bootstrapView(objText) {
  downloadBtn.disabled = false;
  downloadGlbBtn.disabled = false;
  downloadStlBtn.disabled = false;
  if (!viewer) return;
  try {
    const file = new File([objText], "model.obj", { type: "text/plain" });
    viewer.LoadModelFromFileList([file]); // captureMesh() appele via onModelLoaded
    hintEl.hidden = true;
  } catch (e) {
    console.error(e);
    hintEl.hidden = false;
    hintEl.textContent = "Erreur de chargement du modèle : " + e.message;
  }
}

// Capture le maillage three.js affiche + les constructeurs three, apres qu'o3dv
// a charge le modele. Permet ensuite les mises a jour en place.
function captureMesh() {
  if (!viewer) return;
  try {
    // o3dv (esbuild) ne mangle pas les noms de champs : .scene est la THREE.Scene.
    const scene = viewer.GetViewer().scene;
    meshRef = null;
    scene.traverse((o) => { if (o.isMesh && !meshRef) meshRef = o; });
    if (meshRef) {
      GeomCtor = meshRef.geometry.constructor;                            // THREE.BufferGeometry
      AttrCtor = meshRef.geometry.getAttribute("position").constructor;   // THREE.Float32BufferAttribute
      // Classe de base generique (THREE.BufferAttribute) pour l'INDEX : sinon
      // Float32BufferAttribute convertirait les index en float -> index GL_FLOAT
      // -> "glDrawElements: Invalid enum" et rien ne s'affiche.
      IdxAttrCtor = Object.getPrototypeOf(AttrCtor.prototype).constructor;
    }
    // Re-applique les reglages de vue courants au materiau (frais au bootstrap).
    applyModelColor();
    applyWireframe();
    // Bootstrap : o3dv importe l'OBJ en SOUDANT les sommets coincidents -> les
    // sommets distincts caps/parois du maillage sont refusionnes, et o3dv
    // recalcule des normales lissees a travers les aretes vives -> faces plates
    // facettees/degradees. On bascule aussitot sur la geometrie meshData (non
    // soudee) + computeVertexNormals pour un rendu correct des le 1er affichage.
    if (meshRef && GeomCtor && IdxAttrCtor && currentId >= 0)
      updateInPlace(currentId, true);
  } catch (e) { console.error(e); }
}

// --- Controles de vue ------------------------------------------------------
function materialsOf(mesh) {
  if (!mesh || !mesh.material) return [];
  return Array.isArray(mesh.material) ? mesh.material : [mesh.material];
}

function applyModelColor() {
  if (!meshRef) return;
  for (const m of materialsOf(meshRef)) if (m.color) m.color.set(modelColorInput.value);
  if (viewer) viewer.GetViewer().Render();
}

function applyWireframe() {
  if (!meshRef) return;
  for (const m of materialsOf(meshRef)) m.wireframe = wireframeInput.checked;
  if (viewer) viewer.GetViewer().Render();
}

function applyBackground() {
  if (!viewer) return;
  const h = bgColorInput.value.replace("#", "");
  const r = parseInt(h.slice(0, 2), 16), g = parseInt(h.slice(2, 4), 16), b = parseInt(h.slice(4, 6), 16);
  viewer.GetViewer().SetBackgroundColor(new OV.RGBAColor(r, g, b, 255));
}

function resetView() {
  if (!viewer || !meshRef) return;
  const V = viewer.GetViewer();
  meshRef.geometry.computeBoundingSphere();
  const s = meshRef.geometry.boundingSphere;
  if (s) { V.AdjustClippingPlanesToSphere(s); V.FitSphereToWindow(s, true); }
}

// Mise a jour EN PLACE : recupere positions/indices du WASM (vues typees sur le
// tas, copiees aussitot), reconstruit la BufferGeometry, la substitue au mesh
// existant et re-rend. Pas de Clear, pas de reload, camera inchangee.
function updateInPlace(id, refit = false) {
  const t0 = performance.now();
  const d = Module.meshData(id);
  const positions = new Float32Array(d.positions); // copie hors du tas WASM
  // Type d'index minimal : Uint16 (UNSIGNED_SHORT, universel) tant que < 65536
  // sommets ; sinon Uint32 (UNSIGNED_INT, WebGL2). Uint32 systematique faisait
  // "GL_INVALID_ENUM: glDrawElements" sur certains contextes.
  const IndexArray = d.nv > 65535 ? Uint32Array : Uint16Array;
  const indices = new IndexArray(d.indices);

  const g = new GeomCtor();
  g.setAttribute("position", new AttrCtor(positions, 3));
  g.setIndex(new IdxAttrCtor(indices, 1)); // BufferAttribute generique (garde le type entier)
  g.addGroup(0, indices.length, 0); // o3dv attend un material[] -> un groupe
  g.computeVertexNormals();         // normales lissees (bonus: moins facette)

  const old = meshRef.geometry;
  meshRef.geometry = g;
  if (old && old.dispose) old.dispose();

  const V = viewer.GetViewer();
  if (refit) {
    // Changement de forme : recadrage ANIME sur la nouvelle geometrie, sans
    // flicker (contrairement au reload de fichier). AdjustClippingPlanesToSphere
    // evite le clipping quand l'echelle change beaucoup.
    g.computeBoundingSphere();
    const sphere = g.boundingSphere; // THREE.Sphere : {center:{x,y,z}, radius}
    if (sphere) {
      V.AdjustClippingPlanesToSphere(sphere);
      V.FitSphereToWindow(sphere, true); // true = transition animee (variante C)
    }
  }
  V.Render();

  const ms = performance.now() - t0;
  statsEl.textContent = `${d.nv} sommets · ${d.nf} faces · ${ms.toFixed(1)} ms`;
}

// --------------------------------------------------------------------------
// Panneau de parametres, construit depuis getParams() (JSON typé).
// --------------------------------------------------------------------------
function buildPanel(id) {
  paramsEl.innerHTML = "";
  const params = JSON.parse(Module.getParams(id));
  for (const p of params) {
    paramsEl.appendChild(buildParamWidget(id, p));
  }
}

function buildParamWidget(id, p) {
  const wrap = document.createElement("div");
  wrap.className = "param " + p.type;

  if (p.type === "bool") {
    const cb = document.createElement("input");
    cb.type = "checkbox";
    cb.checked = !!p.value;
    cb.id = "p_" + p.name;
    const row = document.createElement("div");
    row.className = "row";
    const lab = document.createElement("label");
    lab.className = "name";
    lab.htmlFor = cb.id;
    lab.textContent = p.name;
    row.append(cb, lab);
    wrap.appendChild(row);
    cb.addEventListener("change", () => {
      Module.setParam(id, p.name, cb.checked ? 1 : 0);
      scheduleUpdate(id);
    });
    return wrap;
  }

  if (p.type === "enum") {
    const row = document.createElement("div");
    row.className = "row";
    const name = document.createElement("span");
    name.className = "name";
    name.textContent = p.name;
    row.appendChild(name);
    wrap.appendChild(row);
    const sel = document.createElement("select");
    p.choices.forEach((c, i) => {
      const o = document.createElement("option");
      o.value = i; o.textContent = c;
      if (i === p.value) o.selected = true;
      sel.appendChild(o);
    });
    sel.addEventListener("change", () => {
      Module.setParam(id, p.name, Number(sel.value));
      scheduleUpdate(id);
    });
    wrap.appendChild(sel);
    return wrap;
  }

  // int / float -> slider + valeur
  const isInt = p.type === "int";
  const row = document.createElement("div");
  row.className = "row";
  const name = document.createElement("span");
  name.className = "name";
  name.textContent = p.name;
  const val = document.createElement("span");
  val.className = "val";
  val.textContent = fmt(p.value, isInt);
  row.append(name, val);
  wrap.appendChild(row);

  const range = document.createElement("input");
  range.type = "range";
  range.min = p.min;
  range.max = p.max;
  range.step = isInt ? 1 : (p.max - p.min) / 200 || 0.001;
  range.value = p.value;
  range.addEventListener("input", () => {
    const v = Number(range.value);
    val.textContent = fmt(v, isInt);
    Module.setParam(id, p.name, v);
    scheduleUpdate(id);
  });
  wrap.appendChild(range);
  return wrap;
}

function fmt(v, isInt) {
  return isInt ? String(Math.round(v)) : (Math.round(v * 1000) / 1000).toString();
}

// --------------------------------------------------------------------------
// Mise a jour pendant l'edition : coalescee sur requestAnimationFrame (au plus
// une par frame). En place si le mesh est deja capture, sinon bootstrap.
// --------------------------------------------------------------------------
let rafPending = false, rafId = -1;
function scheduleUpdate(id) {
  rafId = id;
  if (rafPending) return;
  rafPending = true;
  requestAnimationFrame(() => {
    rafPending = false;
    if (meshRef && GeomCtor && IdxAttrCtor) updateInPlace(rafId, false); // drag : pas de recadrage
    else firstRender(rafId);
  });
}

// Affiche la forme `id` : en place (avec recadrage anime) si le viewer est deja
// amorce -> aucun flicker au changement de forme ; sinon amorcage initial (un
// unique reload de fichier au demarrage, pour capturer le mesh three.js).
function applyShape(id) {
  if (meshRef && GeomCtor && IdxAttrCtor) updateInPlace(id, true);
  else firstRender(id);
}

// --------------------------------------------------------------------------
// Selection d'une forme du catalogue / import SVG.
// --------------------------------------------------------------------------
function selectShape(name) {
  if (currentId !== -1) { Module.destroyShape(currentId); currentId = -1; }
  const id = Module.createShape(name);
  if (id < 0) { statsEl.textContent = "forme inconnue"; return; }
  currentId = id;
  currentName = name;
  filenameInput.value = name;
  buildPanel(id);
  applyShape(id);
}

async function importSvg(file) {
  const text = await file.text();
  if (currentId !== -1) { Module.destroyShape(currentId); currentId = -1; }
  const id = Module.createSvgExtrusion(text);
  if (id < 0) { statsEl.textContent = "SVG illisible"; return; }
  currentId = id;
  currentName = file.name.replace(/\.svg$/i, "");
  filenameInput.value = currentName;
  // Reflète la forme active dans le catalogue.
  const opt = new Option(`SVG : ${file.name}`, "__svg__", true, true);
  catalogEl.appendChild(opt);
  buildPanel(id);
  applyShape(id);
}

// Amorcage initial UNIQUEMENT (premier affichage) : reload complet via o3dv, qui
// recadre la camera et permet a captureMesh() (onModelLoaded) de recuperer le
// mesh three.js + ses constructeurs. Ensuite tout passe par updateInPlace().
function firstRender(id) {
  const obj = Module.regenerate(id);
  const v = (obj.match(/^v /gm) || []).length;
  const f = (obj.match(/^f /gm) || []).length;
  statsEl.textContent = `${v} sommets · ${f} faces`;
  bootstrapView(obj);
}

function downloadObj() {
  if (currentId < 0) return;
  const obj = Module.regenerate(currentId); // OBJ courant a la demande
  saveBlob(new Blob([obj], { type: "text/plain" }), safeName(".obj"));
}

// --- Export GLB (glTF binaire) --------------------------------------------
// Construit le GLB en JS a partir de la geometrie courante (meshData) : plus
// compact que l'OBJ, avec normales lissees. Aucune dependance (ni tinygltf cote
// WASM, ni GLTFExporter cote three).

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

function downloadGlb() {
  if (currentId < 0) return;
  const d = Module.meshData(currentId);
  const positions = new Float32Array(d.positions); // copie hors du tas WASM
  const indices = new Uint32Array(d.indices);
  const normals = computeNormals(positions, indices);
  saveBlob(new Blob([buildGlb(positions, normals, indices)], { type: "model/gltf-binary" }),
           safeName(".glb"));
}

// --- Export STL (binaire) : standard de l'impression 3D --------------------
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

function downloadStl() {
  if (currentId < 0) return;
  const d = Module.meshData(currentId);
  const positions = new Float32Array(d.positions);
  const indices = new Uint32Array(d.indices);
  saveBlob(new Blob([buildStl(positions, indices)], { type: "model/stl" }), safeName(".stl"));
}

// Nom de fichier : valeur saisie dans le champ (nettoyee) + extension.
function safeName(ext) {
  let base = (filenameInput.value || "maker").replace(/[\\/:*?"<>|]+/g, "_").trim();
  base = base.replace(/\.(obj|glb)$/i, ""); // evite Torus.obj.obj si l'utilisateur tape l'extension
  return (base || "maker") + ext;
}

// Telechargement. Si le navigateur a "Toujours demander ou enregistrer",
// il ouvrira sa propre boite "Enregistrer sous" avec ce nom pre-rempli.
function saveBlob(blob, name) {
  const a = document.createElement("a");
  a.href = URL.createObjectURL(blob);
  a.download = name;
  a.click();
  URL.revokeObjectURL(a.href);
}

// --------------------------------------------------------------------------
// Amorçage.
// --------------------------------------------------------------------------
async function main() {
  Module = await createMakerModule();
  initViewer();

  const shapes = JSON.parse(Module.listShapes());
  for (const s of shapes) catalogEl.appendChild(new Option(s, s));
  catalogEl.addEventListener("change", () => selectShape(catalogEl.value));
  svgInput.addEventListener("change", () => {
    if (svgInput.files.length) importSvg(svgInput.files[0]);
  });
  downloadBtn.addEventListener("click", downloadObj);
  downloadGlbBtn.addEventListener("click", downloadGlb);
  downloadStlBtn.addEventListener("click", downloadStl);
  modelColorInput.addEventListener("input", applyModelColor);
  bgColorInput.addEventListener("input", applyBackground);
  wireframeInput.addEventListener("change", applyWireframe);
  resetViewBtn.addEventListener("click", resetView);

  // Deep-link optionnel : ?shape=Torus selectionne une forme au chargement.
  const qs = new URLSearchParams(location.search);
  const wanted = qs.get("shape");
  const initial = shapes.includes(wanted) ? wanted : shapes[0];
  if (initial) { catalogEl.value = initial; selectShape(initial); }

  // Deep-link des parametres : toute autre cle de la query string dont le nom
  // correspond a un parametre de la forme est appliquee via setParam (ex.
  // ?shape=Gothic%20Window&Rosette%20foil%20count=18&Offset%20outer=40). Utile
  // pour les captures automatisees. Un seul re-render a la fin.
  if (currentId !== -1) {
    let touched = false;
    const params = JSON.parse(Module.getParams(currentId));
    const names = new Set(params.map((p) => p.name));
    for (const [k, v] of qs.entries()) {
      if (k === "shape" || !names.has(k)) continue;
      Module.setParam(currentId, k, Number(v));
      touched = true;
    }
    if (touched) { buildPanel(currentId); applyShape(currentId); }
  }
}

main().catch((e) => {
  console.error(e);
  hintEl.textContent = "Erreur : " + e.message;
});
