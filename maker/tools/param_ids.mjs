// Contrôle de la forme du JSON de getParams(id) : chaque entrée porte un `id`
// chaîne non vide (Parameter::Id, clé de setParam et des liens profonds), un
// `name` chaîne (libellé), et les `id` d'une même forme sont distincts.
//
// Fonction pure, sans module : elle se vérifie sur des objets simulés
// (`node maker/tools/param_ids.mjs --self-test`).

import { pathToFileURL } from "node:url";

export function paramIdFailures(shapeName, params) {
  if (!Array.isArray(params)) return [`${shapeName} : getParams ne rend pas un tableau`];
  const failures = [];
  const seen = new Set();
  params.forEach((p, i) => {
    const where = `${shapeName} : paramètre ${i}`;
    if (p === null || typeof p !== "object") {
      failures.push(`${where} n'est pas un objet`);
      return;
    }
    if (typeof p.id !== "string" || p.id.length === 0) {
      failures.push(`${where} (« ${p.name} ») sans \`id\` chaîne non vide`);
      return;
    }
    if (typeof p.name !== "string") failures.push(`${where} (« ${p.id} ») sans \`name\` chaîne`);
    if (seen.has(p.id)) failures.push(`${shapeName} : \`id\` en double « ${p.id} »`);
    seen.add(p.id);
  });
  return failures;
}

function selfTest() {
  const cases = [
    ["conforme", [{ id: "Height", name: "Height (mm)" }, { id: "Count", name: "Count" }], 0],
    ["sans id (module d'avant Q3)", [{ name: "Height" }], 1],
    ["id non chaîne", [{ id: 3, name: "Height" }], 1],
    ["id vide", [{ id: "", name: "Height" }], 1],
    ["id en double", [{ id: "a", name: "A" }, { id: "a", name: "B" }], 1],
    ["sans name", [{ id: "a" }], 1],
    ["pas un tableau", {}, 1],
  ];
  let bad = 0;
  for (const [label, params, expected] of cases) {
    const got = paramIdFailures("Simulée", params).length;
    const ok = got === expected;
    if (!ok) bad += 1;
    process.stdout.write(`${ok ? "ok  " : "FAUX"} ${label} : ${got} échec(s), ${expected} attendu(s)\n`);
  }
  process.exit(bad === 0 ? 0 : 1);
}

const runDirectly = process.argv[1] && pathToFileURL(process.argv[1]).href === import.meta.url;
if (runDirectly && process.argv[2] === "--self-test") selfTest();
