// ===========================================================================
//  Rejeu des sondes COOP/COEP de nodal.md §9 -- et cette fois l'instrument
//  reste dans le depot.
// ===========================================================================
//
// Les chiffres du 2026-08-14 ont ete pris par des scripts jetables qui n'ont
// pas ete conserves : la RESERVE R-audit porte exactement la-dessus, et sa
// condition de levee est de redonner a ces chiffres l'instrument qui les
// produit. Les six sondes ci-dessous sont les memes, dans le meme ordre.
//
// A lancer DEUX fois -- avec et sans --coop-coep --, le controle negatif
// comptant autant que la mesure positive.
//
const report = { started: new Date().toISOString(), probes: {} };

function probe(name, fn) {
  try {
    report.probes[name] = fn();
  } catch (e) {
    report.probes[name] = { error: String((e && e.message) || e) };
  }
}

probe("crossOriginIsolated", () => ({ value: self.crossOriginIsolated }));

probe("SharedArrayBuffer", () => {
  if (typeof SharedArrayBuffer === "undefined") return { value: "is not defined" };
  return { value: typeof SharedArrayBuffer, alloc1KiO: new SharedArrayBuffer(1024).byteLength };
});

probe("WebAssembly.Memory shared", () => {
  const memory = new WebAssembly.Memory({ initial: 1, maximum: 16, shared: true });
  return { value: "ok", buffer: memory.buffer.constructor.name, bytes: memory.buffer.byteLength };
});

probe("Atomics.waitAsync", () => ({
  value: typeof Atomics !== "undefined" && typeof Atomics.waitAsync,
}));

probe("headers", () => ({
  // Les en-tetes ne sont pas lisibles depuis la page ; ce qui l'est, c'est leur
  // EFFET. On le note pour que le rapport dise contre quel serveur il a tourne.
  isolated: self.crossOriginIsolated,
}));

// Worker + OffscreenCanvas + WebGL2 : le montage de l'option (a). Il ne depend
// d'AUCUN en-tete -- c'est ce que le controle negatif doit montrer.
const source = `
self.onmessage = (e) => {
  const canvas = e.data;
  const out = { gotCanvas: !!canvas, webgl2: false, drew: false, error: 0, renderer: "" };
  try {
    const gl = canvas.getContext("webgl2");
    out.webgl2 = !!gl;
    if (gl) {
      out.renderer = gl.getParameter(gl.RENDERER);
      gl.clearColor(0.2, 0.4, 0.6, 1.0);
      gl.clear(gl.COLOR_BUFFER_BIT);
      out.error = gl.getError();
      out.drew = out.error === 0;
    }
  } catch (err) {
    out.exception = String(err && err.message ? err.message : err);
  }
  self.postMessage(out);
};`;

const blob = new Blob([source], { type: "text/javascript" });
const worker = new Worker(URL.createObjectURL(blob));

const finish = async () => {
  document.getElementById("out").textContent = JSON.stringify(report, null, 2);
  try {
    await fetch("/result", { method: "POST", body: JSON.stringify(report, null, 2) });
  } catch (e) {
    /* rapport a l'ecran */
  }
};

worker.onmessage = (event) => {
  report.probes["Worker + OffscreenCanvas + WebGL2"] = event.data;
  report.ok = true;
  finish();
};

setTimeout(() => {
  if (!report.ok) {
    report.probes["Worker + OffscreenCanvas + WebGL2"] = { error: "aucune reponse du worker" };
    report.ok = false;
    finish();
  }
}, 15000);

const canvas = document.getElementById("probe");
const offscreen = canvas.transferControlToOffscreen();
worker.postMessage(offscreen, [offscreen]);
