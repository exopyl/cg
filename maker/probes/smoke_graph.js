// Fumée sur la page livrée : graph.html, son contrôleur, sa palette, son
// témoin de vivacité. Elle n'assert que ce qu'un utilisateur verrait.
const report = { started: new Date().toISOString(), page: "web/graph.html" };
const frame = document.getElementById("page");

const wait = (ms) => new Promise((r) => setTimeout(r, ms));

async function until(predicate, timeoutMs) {
  const deadline = performance.now() + timeoutMs;
  while (performance.now() < deadline) {
    if (predicate()) return true;
    await wait(100);
  }
  return false;
}

async function run() {
  await new Promise((r) => frame.addEventListener("load", r, { once: true }));
  const doc = frame.contentDocument;

  const ready = await until(
    () => (doc.getElementById("log").textContent || "").includes("worker prêt"),
    60000
  );
  report.workerReady = ready;
  report.log = doc.getElementById("log").textContent.trim().split("\n")[0] || "";
  report.paletteButtons = doc.getElementById("palette").querySelectorAll("button").length;

  // Le témoin de vivacité doit AVANCER : une valeur non nulle relevée une seule
  // fois ne dirait pas s'il bat encore.
  const first = +doc.getElementById("beat").textContent;
  await wait(500);
  const second = +doc.getElementById("beat").textContent;
  report.beatAdvanced = second > first;
  report.beats = [first, second];

  // Un geste réel : cliquer le premier type de la palette, puis vérifier que la
  // liste des nœuds et l'inspecteur s'en sont peuplés.
  doc.getElementById("palette").querySelector("button").click();
  report.nodeAppeared = await until(
    () => doc.getElementById("nodes").querySelectorAll("button").length === 1,
    15000
  );
  report.inspectorFields = doc.getElementById("inspector").querySelectorAll("input").length;

  report.ok =
    ready && report.paletteButtons === 6 && report.beatAdvanced && report.nodeAppeared &&
    report.inspectorFields > 0;
}

run()
  .catch((e) => {
    report.ok = false;
    report.error = String((e && e.stack) || e);
  })
  .finally(async () => {
    document.getElementById("out").textContent = JSON.stringify(report, null, 2);
    try {
      await fetch("/result", { method: "POST", body: JSON.stringify(report, null, 2) });
    } catch (e) {
      /* rapport à l'écran */
    }
  });
