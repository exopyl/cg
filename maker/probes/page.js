// ===========================================================================
//  Sonde du CONTROLEUR de la page -- graph-host.js, pas le worker
// ===========================================================================
//
// La sonde editor.js parle au worker en direct : elle n'execute pas une ligne
// de graph-host.js. Or c'est la que vivent la cadence (un tick par rAF, jamais
// deux en vol) et le routage d'entrees. Sans cette sonde-ci, une faute dans le
// controleur ne se verrait qu'a l'oeil, sur la page.
//
// Ce qu'elle etablit :
//   P1  la page demarre : le worker repond, l'editeur s'initialise ;
//   P2  la CADENCE tourne : le compteur de frames avance, et il avance a une
//       cadence PLAUSIBLE -- ni zero, ni un ordre de grandeur au-dessus du
//       rafraichissement de l'ecran, ce qui trahirait un tick auto-cadence ;
//   P3  un evenement DOM REEL -- pas un message fabrique -- traverse : un clic
//       synthetise sur l'element <canvas> fait apparaitre un noeud.
//
import "../web/js/graph-host.js";

const report = { started: new Date().toISOString(), probes: {} };
const $ = (id) => document.getElementById(id);
const wait = (ms) => new Promise((r) => setTimeout(r, ms));

// Un evenement de pointeur COMPLET : graph-host.js appelle setPointerCapture,
// qui exige un pointerId connu du navigateur. isPrimary et pointerType sont
// necessaires pour que Chrome accepte la capture.
function pointer(type, x, y, button) {
  const canvas = $("view");
  const rect = canvas.getBoundingClientRect();
  canvas.dispatchEvent(
    new PointerEvent(type, {
      pointerId: 1,
      pointerType: "mouse",
      isPrimary: true,
      bubbles: true,
      cancelable: true,
      clientX: rect.left + x,
      clientY: rect.top + y,
      button: button === undefined ? 0 : button,
      buttons: type === "pointerdown" ? 1 : 0,
    })
  );
}

async function run() {
  // Le demarrage est asynchrone (chargement du module dans le worker).
  for (let i = 0; i < 100 && !$("log").textContent; ++i) await wait(100);

  report.probes.P1 = {
    log: $("log").textContent.slice(0, 400),
    verdict: /worker prêt/.test($("log").textContent) ? "la page a demarre" : "AUCUN DEMARRAGE",
  };

  // ---- P2 : la cadence ---------------------------------------------------
  const t0 = performance.now();
  const framesBefore = +$("frames").textContent;
  const beatsBefore = +$("beat").textContent;
  await wait(2000);
  const elapsed = (performance.now() - t0) / 1000;
  const drawn = +$("frames").textContent - framesBefore;
  report.probes.P2 = {
    seconds: +elapsed.toFixed(2),
    frames: drawn,
    fps: +(drawn / elapsed).toFixed(1),
    beats: +$("beat").textContent - beatsBefore,
    verdict:
      drawn > 0 && drawn / elapsed <= 240
        ? "la cadence tourne, et elle reste dans l'ordre de grandeur d'un rAF"
        : drawn === 0
          ? "AUCUNE FRAME : la cadence ne tourne pas"
          : "CADENCE ABERRANTE : au-dela de ce qu'un rAF peut produire",
  };

  // ---- P3 : un evenement DOM reel atteint l'editeur -----------------------
  //
  // Meme balayage que la sonde de l'editeur, mais par des PointerEvent poses
  // sur l'element : tout le chemin est parcouru -- ecouteur, accumulation,
  // groupage dans le tick, postMessage, ImGuiIO, bouton de la palette.
  const summaryBefore = $("summary").textContent;
  let clickedAt = null;
  for (let y = 40; y < 700 && clickedAt === null; y += 6) {
    pointer("pointermove", 150, y);
    pointer("pointerdown", 150, y);
    await wait(40);
    pointer("pointerup", 150, y);
    await wait(40);
    if (/[1-9]\d* nœud/.test($("summary").textContent)) clickedAt = y;
  }
  report.probes.P3 = {
    summaryBefore,
    summaryAfter: $("summary").textContent,
    clickedAt,
    verdict: clickedAt !== null
      ? "un PointerEvent du DOM cree un noeud dans le graphe"
      : "AUCUN EVENEMENT DOM N'A RIEN CREE",
  };
}

run()
  .then(() => (report.status = "ok"))
  .catch((e) => {
    report.status = "echec";
    report.error = String((e && e.stack) || e);
  })
  .finally(() => fetch("/result", { method: "POST", body: JSON.stringify(report, null, 2) }));
