// ===========================================================================
//  Online3DViewer : init, chargement d'un OBJ en memoire, et mise a jour EN
//  PLACE de la geometrie three.js pendant l'edition.
// ===========================================================================
//  Degrade proprement si le global OV est absent (vendor/o3dv.min.js non
//  fourni) : la generation et les telechargements restent fonctionnels.
// ===========================================================================

// Cree le viewer. Les accesseurs passes en options evitent que ce module
// connaisse le DOM du panneau (couleur, fil de fer) ou l'id de la forme courante.
//
//   container      : <main> qui recoit le canvas
//   hintEl         : message centre affiche tant qu'aucun modele n'est charge
//   getModelColor  : () -> "#rrggbb" du selecteur de couleur
//   getWireframe   : () -> bool
//   onVertexColors : (bool) -> void, pour griser le selecteur quand le maillage
//                    porte ses propres couleurs (relief d'image)
//   onModelReady   : () -> void, appele apres qu'o3dv a charge un modele ; le
//                    shell y rebascule sur la geometrie non soudee (voir plus bas)
export function createViewer({
  container, hintEl, getModelColor, getWireframe, onVertexColors, onModelReady,
}) {
  let viewer = null;

  // Reference vers le maillage three.js affiche + constructeurs three (captures
  // depuis o3dv apres le premier chargement). Servent a mettre a jour la
  // geometrie EN PLACE pendant l'edition, sans repasser par le reload de fichier
  // d'o3dv (qui vide la scene / cache le canvas / recadre la camera = flickering).
  let meshRef = null, GeomCtor = null, AttrCtor = null, IdxAttrCtor = null;

  // Le maillage courant porte-t-il ses propres couleurs par sommet ? Vrai pour un
  // relief d'image (multi-materiaux) ; faux pour les formes parametriques, qui
  // restent pilotees par le color picker.
  let hasVertexColors = false;

  function materialsOf(mesh) {
    if (!mesh || !mesh.material) return [];
    return Array.isArray(mesh.material) ? mesh.material : [mesh.material];
  }

  function applyModelColor() {
    if (!meshRef) return;
    // En mode vertexColors la couleur du materiau MODULE les couleurs de sommet :
    // il faut du blanc pour restituer la palette telle quelle. Le picker n'a donc
    // pas d'effet sur un relief colore (et son champ est grise via onVertexColors).
    const c = hasVertexColors ? "#ffffff" : getModelColor();
    for (const m of materialsOf(meshRef)) if (m.color) m.color.set(c);
    onVertexColors(hasVertexColors);
    if (viewer) viewer.GetViewer().Render();
  }

  function applyWireframe() {
    if (!meshRef) return;
    for (const m of materialsOf(meshRef)) m.wireframe = getWireframe();
    if (viewer) viewer.GetViewer().Render();
  }

  function applyBackground(hex) {
    if (!viewer) return;
    const h = hex.replace("#", "");
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
      // facettees/degradees. Le shell rebascule aussitot sur la geometrie meshData
      // (non soudee) + computeVertexNormals pour un rendu correct des le 1er affichage.
      if (isReady()) onModelReady();
    } catch (e) { console.error(e); }
  }

  // Vrai des que la mise a jour en place est possible.
  function isReady() {
    return !!(meshRef && GeomCtor && IdxAttrCtor);
  }

  // Chargement complet (via le pipeline o3dv) : utilise UNIQUEMENT au premier
  // affichage et aux imports de fichier. Recadre la camera sur la nouvelle
  // geometrie. C'est la seule voie qui "flicke", mais c'est une action
  // ponctuelle, pas le drag.
  function bootstrap(objText) {
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

  // Mise a jour EN PLACE : recupere positions/indices du WASM (vues typees sur le
  // tas, copiees aussitot), reconstruit la BufferGeometry, la substitue au mesh
  // existant et re-rend. Pas de Clear, pas de reload, camera inchangee.
  // Renvoie { nv, nf, ms } pour l'affichage des stats par l'appelant.
  function updateInPlace(Module, id, refit = false) {
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

    // Couleurs par sommet : non vides uniquement pour les maillages multi-materiaux
    // (relief d'image = une couleur par region quantifiee). Le materiau bascule en
    // vertexColors et sa couleur de base doit repasser en BLANC, sinon three.js
    // multiplie les deux et le gris du picker ternit toutes les regions.
    hasVertexColors = d.colors.length === positions.length && positions.length > 0;
    if (hasVertexColors)
      g.setAttribute("color", new AttrCtor(new Float32Array(d.colors), 3));

    g.computeVertexNormals();         // normales lissees (bonus: moins facette)

    const old = meshRef.geometry;
    meshRef.geometry = g;
    if (old && old.dispose) old.dispose();

    for (const m of materialsOf(meshRef)) {
      m.vertexColors = hasVertexColors;
      m.needsUpdate = true;
    }
    applyModelColor();

    const V = viewer.GetViewer();
    if (refit) {
      // Changement de forme : recadrage ANIME sur la nouvelle geometrie, sans
      // flicker (contrairement au reload de fichier). AdjustClippingPlanesToSphere
      // evite le clipping quand l'echelle change beaucoup.
      g.computeBoundingSphere();
      const sphere = g.boundingSphere; // THREE.Sphere : {center:{x,y,z}, radius}
      if (sphere) {
        V.AdjustClippingPlanesToSphere(sphere);
        V.FitSphereToWindow(sphere, true); // true = transition animee
      }
    }
    V.Render();

    return { nv: d.nv, nf: d.nf, ms: performance.now() - t0 };
  }

  // --- initialisation ------------------------------------------------------
  let available = false;
  if (window.__o3dvMissing || typeof window.OV === "undefined") {
    hintEl.textContent = "Rendu 3D indisponible (o3dv.min.js manquant).";
  } else {
    try {
      viewer = new OV.EmbeddedViewer(container, {
        backgroundColor: new OV.RGBAColor(20, 22, 26, 255),
        defaultColor: new OV.RGBColor(180, 190, 200),
        onModelLoaded: captureMesh,
      });
      // Rotation libre (trackball) au lieu de l'orbite a vecteur up fixe, qui
      // "verrouille" la rotation autour de l'axe vertical.
      viewer.GetViewer().SetNavigationMode(OV.NavigationMode.FreeOrbit);
      available = true;
    } catch (e) {
      console.error(e);
      hintEl.textContent = "Erreur d'initialisation du viewer : " + e.message;
    }
  }

  return {
    available,
    isReady,
    bootstrap,
    updateInPlace,
    applyModelColor,
    applyWireframe,
    applyBackground,
    resetView,
  };
}
