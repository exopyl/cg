// Vérification du module maker (maker.js + maker.wasm) sous Node.
//
// Usage : node maker/tools/check_module.mjs <répertoire de maker.js> <nb formes> <nb gothiques>
//
// Contrôles, tous requis pour un code de sortie 0 :
//   - le module se charge et s'instancie ;
//   - listShapes("") rend exactement <nb formes> noms, distincts ;
//   - listShapes("gothic") rend exactement <nb gothiques> noms, tous présents dans le catalogue ;
//   - chaque forme du catalogue se crée et rend un maillage non vide (nv > 0, nf > 0) ;
//   - le module n'écrit aucune ligne sur stderr pendant ces opérations ;
//   - le détecteur de stderr est lui-même vu fonctionner (contrôle positif).
//
// Les deux nombres attendus sont des arguments OBLIGATOIRES : un défaut égal au
// nombre observé rendrait le contrôle de cardinalité incapable d'échouer.
//
// Codes de sortie : 0 succès, 1 contrôle en échec ou module inutilisable, 2 usage.
// Le rapport JSON va sur stdout ; les diagnostics du harnais vont sur
// process.stderr, jamais par console.error, que le harnais intercepte.

import { readFileSync } from "node:fs";
import { pathToFileURL } from "node:url";
import path from "node:path";

function usage(message) {
  process.stderr.write(`${message}\n`);
  process.stderr.write("usage : node check_module.mjs <répertoire de maker.js> <nb formes> <nb gothiques>\n");
  process.exit(2);
}

function parseCount(text, label) {
  if (text === undefined) usage(`argument manquant : ${label}`);
  if (!/^\d+$/.test(text)) usage(`${label} doit être un entier positif, reçu « ${text} »`);
  return Number(text);
}

const [webDir, totalArg, gothicArg] = process.argv.slice(2);
if (!webDir) usage("argument manquant : répertoire de maker.js");
const expectedTotal = parseCount(totalArg, "nb formes");
const expectedGothic = parseCount(gothicArg, "nb gothiques");

const fail = (message) => {
  process.stderr.write(`check_module : ${message}\n`);
  process.exit(1);
};

// Le module est construit avec -sENVIRONMENT=web,worker : il ne lit ni
// `wasmBinary` ni `printErr` passés en argument. Il lie sa sortie d'erreur à
// console.error à sa création et ne consulte que le crochet `instantiateWasm`.
// D'où l'interception de console.error AVANT l'appel à la fabrique, et
// l'instanciation à partir des octets lus ici.
const stderrLines = [];
console.error = (...args) => stderrLines.push(args.join(" "));

let Module;
try {
  const { default: createMakerModule } = await import(pathToFileURL(path.resolve(webDir, "maker.js")).href);
  const bytes = readFileSync(path.resolve(webDir, "maker.wasm"));
  Module = await new Promise((resolve, reject) => {
    createMakerModule({
      instantiateWasm(imports, done) {
        WebAssembly.instantiate(bytes, imports)
          .then((r) => done(r.instance, r.module))
          .catch(reject);
        return {};
      },
    }).then(resolve, reject);
  });
} catch (e) {
  // La sortie d'erreur du module est interceptée : la restituer, elle porte
  // souvent la cause (abort, import manquant).
  const captured = stderrLines.length ? `\nstderr du module :\n${stderrLines.join("\n")}` : "";
  fail(`chargement du module impossible : ${e && e.stack ? e.stack : e}${captured}`);
}

// Contrôle positif : une écriture sur /dev/stderr par le système de fichiers du
// module emprunte le même chemin (fd 2 -> sortie d'erreur du module) que
// std::cerr côté C++. Si elle n'est pas vue, une absence de ligne stderr plus
// bas ne prouverait rien.
const PROBE = "check_module-stderr-probe";
{
  const stream = Module.FS.open("/dev/stderr", "w");
  const payload = new TextEncoder().encode(`${PROBE}\n`);
  Module.FS.write(stream, payload, 0, payload.length);
  Module.FS.close(stream);
}
const probeDetected = stderrLines.includes(PROBE);
stderrLines.length = 0;

const all = JSON.parse(Module.listShapes("")).map((e) => e.name);
const gothic = JSON.parse(Module.listShapes("gothic")).map((e) => e.name);

const failures = [];
if (all.length !== expectedTotal) failures.push(`catalogue : ${all.length} formes, ${expectedTotal} attendues`);
if (new Set(all).size !== all.length) failures.push("catalogue : noms en double");
if (gothic.length !== expectedGothic) failures.push(`gothiques : ${gothic.length} formes, ${expectedGothic} attendues`);
for (const name of gothic) {
  if (!all.includes(name)) failures.push(`gothique absente du catalogue : ${name}`);
}

const counts = {};
for (const name of all) {
  const id = Module.createShape(name);
  if (id < 0) {
    failures.push(`${name} : createShape a rendu ${id}`);
    continue;
  }
  const mesh = Module.meshData(id);
  counts[name] = [mesh.nv, mesh.nf];
  if (!(mesh.nv > 0 && mesh.nf > 0)) failures.push(`${name} : maillage vide (nv=${mesh.nv}, nf=${mesh.nf})`);
  Module.destroyShape(id);
}

if (!probeDetected) failures.push("détecteur stderr inopérant : la sonde n'a pas été vue");
if (stderrLines.length !== 0) failures.push(`${stderrLines.length} ligne(s) écrite(s) sur stderr par le module`);

const report = {
  total: all.length,
  expectedTotal,
  gothic,
  expectedGothic,
  counts,
  stderrLines,
  stderrProbeDetected: probeDetected,
  failures,
};
process.stdout.write(`${JSON.stringify(report, null, 2)}\n`);

if (failures.length !== 0) {
  for (const f of failures) process.stderr.write(`ÉCHEC : ${f}\n`);
  process.exit(1);
}
process.stderr.write(`check_module : ${all.length} formes, dont ${gothic.length} gothiques, maillages non vides, stderr muet\n`);
