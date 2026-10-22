// Controles du pont embind de la microarchitecture gothique, sous node.
// Lance par microarch_bridge.py :
//   node microarch_bridge.cjs <maker_node.js> <d4.json> <dense.json | ""> [sha256 natif | ""] [seuil attendu]
const crypto = require("crypto");
const fs = require("fs");
const path = require("path");

const [modulePath, specPath, densePath, nativeSha, thresholdArg] = process.argv.slice(2);
// Seuil de verification attendu : 2^22 au defaut (M36), ou celui que le pont a injecte.
const expectedThreshold = thresholdArg ? Number(thresholdArg) : 4194304;
const createMakerModule = require(path.resolve(modulePath));

let failures = 0;
function check(name, cond, detail) {
  console.log(`${cond ? "PASS" : "FAIL"} ${name}${detail !== undefined ? " : " + detail : ""}`);
  if (!cond) failures++;
}
const bytesOf = (r) => new Uint8Array(r.bytes);
const faces = (Module, id) => new Uint32Array(Module.meshData(id).indices).length / 3;
const codes = (r) => JSON.parse(r.report).entries.map((e) => e.code);
const errorPaths = (r) => JSON.parse(r.report).entries.filter((e) => e.severity === "error").map((e) => e.parameter);
const heapMB = (M) => (M.HEAPU8 ? M.HEAPU8.buffer.byteLength / 1048576 : NaN);

// STL binaire : nombre de triangles, sommets (9 floats par triangle), boite.
function readStl(bytes) {
  const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  const count = bytes.length >= 84 ? view.getUint32(80, true) : -1;
  const lo = [Infinity, Infinity, Infinity], hi = [-Infinity, -Infinity, -Infinity];
  const v = new Float32Array(9 * Math.max(count, 0));
  if (bytes.length !== 84 + 50 * count) return { count, sizeOk: false, v, lo, hi };
  for (let f = 0; f < count; f++)
    for (let k = 0; k < 9; k++) {
      const x = view.getFloat32(84 + 50 * f + 12 + 4 * k, true);
      v[9 * f + k] = x;
      lo[k % 3] = Math.min(lo[k % 3], x);
      hi[k % 3] = Math.max(hi[k % 3], x);
    }
  return { count, sizeOk: true, v, lo, hi };
}

// Relecture soudee par egalite exacte des floats : chaque arete orientee une fois, et son
// opposee une fois (ferme, 2-variete aux aretes, oriente) ; volume signe.
function closedAndOutward(stl) {
  const id = new Map();
  const vid = (f, c) => {
    const k = `${stl.v[9 * f + 3 * c]},${stl.v[9 * f + 3 * c + 1]},${stl.v[9 * f + 3 * c + 2]}`;
    if (!id.has(k)) id.set(k, id.size);
    return id.get(k);
  };
  const directed = new Map();
  let volume = 0;
  for (let f = 0; f < stl.count; f++) {
    const t = [vid(f, 0), vid(f, 1), vid(f, 2)];
    for (let k = 0; k < 3; k++) {
      const e = `${t[k]}>${t[(k + 1) % 3]}`;
      directed.set(e, (directed.get(e) || 0) + 1);
    }
    const a = stl.v.subarray(9 * f, 9 * f + 3), b = stl.v.subarray(9 * f + 3, 9 * f + 6), c = stl.v.subarray(9 * f + 6, 9 * f + 9);
    volume += (a[0] * (b[1] * c[2] - b[2] * c[1]) - a[1] * (b[0] * c[2] - b[2] * c[0]) + a[2] * (b[0] * c[1] - b[1] * c[0])) / 6;
  }
  let bad = 0;
  for (const [e, n] of directed) {
    const [a, b] = e.split(">");
    if (n !== 1 || directed.get(`${b}>${a}`) !== 1) bad++;
  }
  return { bad, volume, vertices: id.size };
}

(async () => {
  const M = await createMakerModule();
  const d4 = fs.readFileSync(specPath, "utf8");

  // --- chargement --------------------------------------------------------------
  const r = M.createMicroarchFromJson(d4);
  check("load accepted", r.id >= 0, `id=${r.id}`);
  check("no failure", r.failure === undefined);
  check("network JSON schema", JSON.parse(r.network).schema === "gothic-microarchitecture-network/v1");
  if (nativeSha) {
    const sha = crypto.createHash("sha256").update(Buffer.from(r.network, "utf8")).digest("hex");
    check("network JSON = native", sha === nativeSha, sha);
  }
  check("load report without error", JSON.parse(r.report).entries.every((e) => e.severity !== "error"));

  // --- affichage : regenerate sans OBJ texte, meshData seul ---------------------
  check("regenerate gives no OBJ text", M.regenerate(r.id) === "");
  check("meshData shows the tubes", faces(M, r.id) === 2176, faces(M, r.id));
  check("tube report empty", JSON.parse(M.microarchTubeReport(r.id)).entries.length === 0);
  const o = M.microarchOrigin(r.id);
  check("origin is [x,y,z]", Array.isArray(o) && o.length === 3, JSON.stringify(o));

  // --- exports propres --------------------------------------------------------------
  const curves = M.exportMicroarchObj(r.id, "my d4", "curves");
  check("curves obj", bytesOf(curves).length > 0 && curves.name === "myd4_curves", `${curves.name} ${bytesOf(curves).length} bytes`);
  const tubes = M.exportMicroarchObj(r.id, "", "tubes");
  const tt = Buffer.from(bytesOf(tubes)).toString("utf8");
  const nv = (tt.match(/^v /gm) || []).length, nf = (tt.match(/^f /gm) || []).length;
  check("tubes obj counts", nv === 1104 && nf === 2176 && tubes.name === "microarchitecture_tubes", `${tubes.name} v=${nv} f=${nf}`);
  const bad = M.exportMicroarchObj(r.id, "x", "glb");
  check("unknown kind refused", bytesOf(bad).length === 0 && bad.name === "");

  // --- garde des exports generiques -------------------------------------------------
  const eo = M.exportObj(r.id, "x");
  check("exportObj refused", eo.refused !== undefined && bytesOf(eo).length === 0, eo.refused);
  const eg = M.exportGlb(r.id, "x");
  check("exportGlb refused", eg.refused !== undefined && bytesOf(eg).length === 0);
  check("gothicPrintReport refused (STL guard)", M.gothicPrintReport(r.id).refused !== undefined);

  // --- curseurs ---------------------------------------------------------------------
  check("setParam sides 3", M.setParam(r.id, "Tube sides", 3));
  check("remesh nf 1072", faces(M, r.id) === 1072);
  M.setParam(r.id, "Tube sides", 2);
  const empty = faces(M, r.id);
  const tr = JSON.parse(M.microarchTubeReport(r.id));
  check("out of range reported", empty === 0 && tr.entries.length === 1 && tr.entries[0].code === "tube_invalid_parameter",
        JSON.stringify(tr.entries[0]));
  check("origin null without mesh", M.microarchOrigin(r.id) === null);
  check("curves still exported", bytesOf(M.exportMicroarchObj(r.id, "x", "curves")).length > 0);
  const refusedTubes = M.exportMicroarchObj(r.id, "x", "tubes");
  const rt = JSON.parse(refusedTubes.report);
  check("tubes export gives the tube refusal", bytesOf(refusedTubes).length === 0 &&
        rt.entries.length === 1 && rt.entries[0].code === "tube_invalid_parameter", refusedTubes.report);
  M.setParam(r.id, "Tube sides", 6);
  check("valid value clears the refusal", faces(M, r.id) === 2176 && JSON.parse(M.microarchTubeReport(r.id)).entries.length === 0);

  // --- solide imprimable (J7-d) --------------------------------------------------------
  // D4 segment, rayon 50 permil, q = 3 : F = 47 632 (natif, journal §14.11).
  const printerBefore = M.printConstraints();
  const originBefore = JSON.stringify(M.microarchOrigin(r.id));
  const s2 = M.exportMicroarchStl(r.id, "my d4", 50, 3, 2);
  const stl2 = readStl(s2.bytes);
  check("solid STL printable", s2.verdict === "printable" && s2.triangles === 47632 && s2.name === "myd4_solid",
        `${s2.verdict} F=${s2.triangles} ${s2.name} ${s2.failure || ""}`);
  check("solid STL size = 84 + 50 F", stl2.sizeOk && stl2.count === s2.triangles, `${s2.bytes.length} bytes, count ${stl2.count}`);
  const cw = closedAndOutward(stl2);
  check("solid STL reread: closed, manifold edges, outward", cw.bad === 0 && cw.volume > 0, JSON.stringify(cw));
  const centre = [0.5 * (stl2.lo[0] + stl2.hi[0]), 0.5 * (stl2.lo[1] + stl2.hi[1])];
  check("solid STL centred in x, y, base at z = 0",
        Math.abs(centre[0]) < 1e-3 && Math.abs(centre[1]) < 1e-3 && Math.abs(stl2.lo[2]) < 1e-4, `centre ${centre} zmin ${stl2.lo[2]}`);
  const ext2 = [0, 1, 2].map((k) => stl2.hi[k] - stl2.lo[k]);
  check("solid STL extent = extentMm", ext2.every((x, k) => Math.abs(x - s2.extentMm[k]) < 1e-3), `${ext2} / ${s2.extentMm}`);
  const stl1 = readStl(M.exportMicroarchStl(r.id, "x", 50, 3, 1).bytes);
  const ext1 = [0, 1, 2].map((k) => stl1.hi[k] - stl1.lo[k]);
  check("solid STL in millimetres: scale 2 doubles the extent", ext1.every((x, k) => Math.abs(2 * x - ext2[k]) < 1e-3 * ext2[k]),
        `${ext1} -> ${ext2}`);
  // Avant tout meshData (qui reconstruirait des tubes oublies) : le maillage et son origine sont la.
  const originAfter = JSON.stringify(M.microarchOrigin(r.id));
  check("tubes untouched by the solid export", originAfter === originBefore && originAfter !== "null" &&
        faces(M, r.id) === 2176 && JSON.parse(M.microarchTubeReport(r.id)).entries.length === 0, `${originBefore} -> ${originAfter}`);
  const thick = JSON.parse(printerBefore);
  thick.minWallMm = 100;
  M.setPrintConstraints(JSON.stringify(thick));
  const np = M.exportMicroarchStl(r.id, "x", 50, 3, 2);
  const wallMessage = (JSON.parse(np.report).entries.find((e) => e.code === "solid_wall_too_thin") || {}).message || "";
  const minScale = Number((/minimum scale ([0-9.eE+-]+) mm per drawing unit/.exec(wallMessage) || [])[1]);
  // 2r = 2 x 0,05 x D ; echelle minimale = minWallMm / 2r, avec minWallMm = 100.
  const twoR = 2 * 0.05 * JSON.parse(r.network).scale;
  check("solid not printable: wall reported with the minimum scale, STL still exported", np.verdict === "not_printable" &&
        codes(np).includes("solid_wall_too_thin") && np.bytes.length === 84 + 50 * np.triangles &&
        Math.abs(minScale - 100 / twoR) < 1e-6 * minScale, `${np.verdict} ${wallMessage}`);
  M.setPrintConstraints(printerBefore);
  const badQ = M.exportMicroarchStl(r.id, "x", 50, 2.5, 2);
  check("solid q not an integer: refused, not rounded", badQ.verdict === "refused" && badQ.bytes.length === 0 &&
        codes(badQ).join() === "solid_invalid_parameter" && errorPaths(badQ).join() === "solid.cells_per_radius", badQ.report);
  // Nom du fichier : filtre [A-Za-z0-9_-], « microarchitecture » s'il ne reste rien.
  const names = [["a/b c", "abc_solid"], ["../x", "x_solid"], [".", "microarchitecture_solid"],
                 ["..", "microarchitecture_solid"], ["  ", "microarchitecture_solid"], ["", "microarchitecture_solid"]]
    .map(([base, want]) => [base, M.exportMicroarchStl(r.id, base, 50, 3, 2).name, want]);
  check("solid STL name filtered, default when empty", names.every(([, got, want]) => got === want), JSON.stringify(names));
  const badScale = M.exportMicroarchStl(r.id, "x", 50, 3, 0);
  check("solid scale 0: refused", badScale.verdict === "refused" && badScale.bytes.length === 0 &&
        errorPaths(badScale).join() === "solid.mm_per_unit", badScale.report);

  // --- refus de chargement ----------------------------------------------------------
  const notJson = M.createMicroarchFromJson("{ not json");
  check("non JSON refused", notJson.id === -1 && notJson.network === "" &&
        JSON.parse(notJson.report).entries[0].code === "json_parse");
  const limit = 4 * 1024 * 1024;
  const big = M.createMicroarchFromJson(d4 + " ".repeat(limit - Buffer.byteLength(d4) + 1));
  const bigR = JSON.parse(big.report);
  check("json_too_large", big.id === -1 && bigR.entries.length === 1 && bigR.entries[0].code === "json_too_large");
  const exact = M.createMicroarchFromJson(d4 + " ".repeat(limit - Buffer.byteLength(d4)));
  check("exactly 4 MiB accepted", exact.id >= 0);
  M.destroyShape(exact.id);

  // --- une autre forme n'est pas concernee -------------------------------------------
  const torus = M.createShape("Torus");
  check("Torus in the catalogue", torus >= 0);
  if (torus >= 0) {
    check("other shape: microarch functions are inert", M.microarchTubeReport(torus) === "" &&
          M.microarchOrigin(torus) === null && bytesOf(M.exportMicroarchObj(torus, "x", "tubes")).length === 0);
    check("other shape: exportObj not refused", M.exportObj(torus, "t").refused === undefined);
    check("other shape: regenerate gives OBJ text", /^v /m.test(M.regenerate(torus)));
    const ts = M.exportMicroarchStl(torus, "x", 50, 3, 1);
    check("other shape: solid export refused", ts.verdict === "refused" && ts.bytes.length === 0 && ts.name === "");
    M.destroyShape(torus);
  }

  // --- seuil de verification (M33) et refus au plafond exact (M32 b), reseau dense ---------
  if (densePath) {
    const dense = M.createMicroarchFromJson(fs.readFileSync(densePath, "utf8"));
    check("dense network loaded", dense.id >= 0, dense.report.slice(0, 200));
    // Rayon 8 permil, q = 3 : F = 3 492 944 (journal §14.12). Sous le seuil par defaut (2^22,
    // M36) : verdict complet ; au-dessus d'un seuil injecte : non verifie.
    const heaps = [];
    for (let round = 0; round < 2; round++) {
      const t0 = Date.now();
      const u = M.exportMicroarchStl(dense.id, "dense", 8, 3, 1);
      heaps.push(heapMB(M));
      if (round === 0) {
        const detail = `${u.verdict} F=${u.triangles} threshold=${u.threshold} ${((Date.now() - t0) / 1000).toFixed(1)} s heap ${heaps[0]} MB`;
        check("dense: threshold is the expected one", u.threshold === expectedThreshold, detail);
        if (u.triangles > expectedThreshold)
          check("dense beyond the threshold: unverified, STL exported", u.verdict === "unverified" &&
                codes(u).includes("solid_unverified") && u.bytes.length === 84 + 50 * u.triangles, detail);
        else
          check("dense within the threshold: verified, STL exported", (u.verdict === "printable" || u.verdict === "not_printable") &&
                !codes(u).includes("solid_unverified") && u.triangles > 0 && u.bytes.length === 84 + 50 * u.triangles, detail);
      }
    }
    check("heap stable over two dense exports (nothing kept)", heaps[1] === heaps[0], `${heaps.join(" -> ")} MB`);
    const t1 = Date.now();
    const cap = M.exportMicroarchStl(dense.id, "dense", 5, 3, 1);
    const afterCap = heapMB(M);
    check("refusal at the exact cap: solid_limit_exceeded on solid.triangles, no bytes", cap.verdict === "refused" &&
          cap.bytes.length === 0 && cap.triangles === 0 && codes(cap).includes("solid_limit_exceeded") &&
          errorPaths(cap).join() === "solid.triangles",
          `${((Date.now() - t1) / 1000).toFixed(1)} s heap ${afterCap} MB ${cap.report.slice(0, 160)}`);
    const again = M.exportMicroarchStl(r.id, "x", 50, 3, 2);
    check("usable after the refusal: export again, heap unchanged", again.verdict === "printable" &&
          again.bytes.length === s2.bytes.length && heapMB(M) === afterCap && afterCap <= heaps[0], `${heapMB(M)} MB`);
    M.destroyShape(dense.id);
  }

  M.destroyShape(r.id);
  check("destroyed", M.microarchTubeReport(r.id) === "");
  console.log(failures ? `${failures} FAILURE(S)` : "ALL PASS");
  process.exit(failures ? 1 : 0);
})();
