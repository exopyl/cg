pragma Singleton
import QtQuick

// Design tokens ported from the "Dock complet — Détaillé" mockup
// (Design/Dock complet - Détaillé.html). Single source of truth for
// colours, radii, sizes, fonts, icon paths and the analysis-tool
// metadata. Only the dark theme is baked in (the mockup's light/density
// variants are out of scope for the reskin).
QtObject {
    id: theme

    // -- theme state -----------------------------------------------
    // Day / night switch (toggled by the chrome's sun/moon button). All
    // colour tokens below are bindings on `dark`, so flipping it restyles
    // the whole UI. The orange accent and the ok/warn/danger semantics are
    // shared by both themes.
    property bool dark: true

    // -- palette (dark ⇄ light) ------------------------------------
    // Translucent "glass" surfaces are alpha-first #AARRGGBB. Qt has no
    // cheap backdrop-blur over a Vulkan item, so the frosted look is
    // approximated with semi-opaque fills. Light values from the mockup's
    // [data-theme="light"].
    readonly property color bg:           dark ? "#17181B" : "#E9E7E1"  // window / chrome base
    readonly property color viewport:     dark ? "#141519" : "#DAD8D0"  // 3D viewport clear
    readonly property color panel:        dark ? "#1B1D22" : "#F4F2ED"  // solid side panels

    readonly property color surface:      dark ? "#DB222429" : "#E6FCFBF8"
    readonly property color surface2:     dark ? "#E62C2F35" : "#F2F4F2ED"
    readonly property color surfaceSolid: dark ? "#212329"   : "#FBFAF7"

    readonly property color stroke:       dark ? "#1AFFFFFF" : "#1F1E1C18"
    readonly property color stroke2:      dark ? "#12FFFFFF" : "#141E1C18"
    readonly property color strokeStrong: dark ? "#2EFFFFFF" : "#381E1C18"

    readonly property color text:         dark ? "#ECEAE5" : "#23211C"
    readonly property color textSoft:     dark ? "#A3A097" : "#6B675E"
    readonly property color textFaint:    dark ? "#6E6C66" : "#A29D92"

    // accent + semantics: identical in both themes
    readonly property color accent:       "#FF7A1A"
    readonly property color accentSoft:   "#29FF7A1A"  // rgba(255,122,26,.16)
    readonly property color accentGlow:   "#66FF7A1A"  // rgba(255,122,26,.40)

    readonly property color ok:           "#33B07A"
    readonly property color warn:         "#E8B23A"
    readonly property color danger:       "#E94034"

    // faint HUD grid: light lines on the dark viewport, dark lines on the light one
    readonly property color gridLine:     dark ? "#16FFFFFF" : "#12000000"

    // -- geometry --------------------------------------------------
    readonly property int rLg: 16
    readonly property int rMd: 11
    readonly property int rSm: 8

    readonly property int dockPad:     8
    readonly property int dockBtn:     46
    readonly property int dockIcon:    23
    readonly property int panelWidth:  312
    readonly property int panelPad:    18
    readonly property int fieldGap:    16

    // -- fonts -----------------------------------------------------
    // IBM Plex isn't bundled; Qt falls back gracefully when it's not
    // installed. Install IBM Plex Sans/Mono to match the mockup exactly.
    readonly property string fontSans: "IBM Plex Sans"
    readonly property string fontMono: "IBM Plex Mono"

    // -- icons (24px line paths, from Design/hifi-icons.jsx) -------
    readonly property var icons: ({
        "fit":      "M4 9V5a1 1 0 0 1 1-1h4M20 9V5a1 1 0 0 0-1-1h-4M4 15v4a1 1 0 0 0 1 1h4M20 15v4a1 1 0 0 0-1 1h-4",
        "orbit":    "M12 4a8 8 0 1 0 8 8M12 4a8 8 0 0 1 8 8M3 12c0 2 4 3.2 9 3.2S21 14 21 12M12 4v16",
        "grid":     "M3 9h18M3 15h18M9 3v18M15 3v18",
        "ruler":    "M3 8h18v8H3zM7 8v3.5M11 8v5M15 8v3.5M19 8v5",
        "curvature":"M3 16c2.6 0 2.6-9 5.2-9S10.8 16 13.4 16 16 7 18.6 7 21 16 21 16",
        "compare":  "M5 6 9 6 7 4M19 6h-4l2-2M5 6c0 5 14 5 14 0M5 18l4 0-2 2M19 18h-4l2 2M5 18c0-5 14-5 14 0",
        "measure":  "M3.5 14.4 14.4 3.5l6.1 6.1L9.6 20.5zM7.2 10.7l1.7 1.7M10.1 7.8l1.7 1.7M13 4.9l1.7 1.7",
        "thickness":"M4 5h16M4 19h16M9 9.2v5.6M12 8v8M15 9.2v5.6M9 9.2 12 8l3 1.2M9 14.8 12 16l3-1.2",
        "section":  "M4 8 12 4l8 4v8l-8 4-8-4zM4 8l8 4m0 0 8-4m-8 4v8M8 14l8-4",
        "home":     "M4 11 12 4l8 7M6.5 9.6V20h11V9.6",
        "rotate":   "M20 12a8 8 0 1 1-2.3-5.6M20 4v3.5h-3.5",
        "export":   "M12 15V4m0 0-4 4m4-4 4 4M5 15v3.5a1.5 1.5 0 0 0 1.5 1.5h11a1.5 1.5 0 0 0 1.5-1.5V15",
        "settings": "M12 9.5a2.5 2.5 0 1 0 0 5 2.5 2.5 0 0 0 0-5zM12 3v2.5M12 18.5V21M3 12h2.5M18.5 12H21M5.6 5.6l1.8 1.8M16.6 16.6l1.8 1.8M18.4 5.6l-1.8 1.8M7.4 16.6l-1.8 1.8",
        "share":    "M7 12a2.5 2.5 0 1 0 0-.1M17 6.5a2.5 2.5 0 1 0 0-.1M17 17.5a2.5 2.5 0 1 0 0-.1M9.2 10.8l5.6-3.1M9.2 13.2l5.6 3.1",
        "chevron":  "M8 10l4 4 4-4",
        "close":    "M6 6l12 12M18 6 6 18",
        "warning":  "M12 4 2.5 20h19zM12 10v5M12 17.6v.1",
        "check":    "M5 13l4 4 10-11",
        "layers":   "M12 3l9 5-9 5-9-5zM3 12l9 5 9-5M3 15.5l9 5 9-5",
        "reset":    "M4 12a8 8 0 1 1 2.4 5.7M4 17v-4h4",
        "target":   "M12 4v3M12 17v3M4 12h3M17 12h3M12 9.5a2.5 2.5 0 1 0 0 5 2.5 2.5 0 0 0 0-5z",
        "open":     "M3 7a1 1 0 0 1 1-1h5l2 2h8a1 1 0 0 1 1 1v9a1 1 0 0 1-1 1H4a1 1 0 0 1-1-1z",
        "spec":     "M7 3h7l3.5 3.5V20a1 1 0 0 1-1 1H7a1 1 0 0 1-1-1V4a1 1 0 0 1 1-1zM14 3v4h3.5M9 12.5h6M9 16h6",
        "sun":      "M12 7.5a4.5 4.5 0 1 0 0 9 4.5 4.5 0 0 0 0-9zM12 2v3M12 19v3M2 12h3M19 12h3M4.9 4.9l2.1 2.1M17 17l2.1 2.1M19.1 4.9 17 7M7 17l-2.1 2.1",
        "moon":     "M20 14.5A8 8 0 0 1 9.5 4 7 7 0 1 0 20 14.5z"
    })

    function iconPath(name) {
        return icons[name] !== undefined ? icons[name] : icons["target"];
    }

    // -- analysis tools (metadata only — no cgre2 backend yet) -----
    // Ported from Design/hifi-app.jsx HTOOLS. These drive the dock
    // buttons, the reactive parameter panel and the legend card. The
    // controls are a faithful, non-functional preview of future cgre2
    // analysis features.
    readonly property var tools: [
        {
            id: "spec", icon: "spec", label: "Fiche technique", key: "F",
            kind: "spec",   // read-only mesh data sheet (no parameter controls)
            blurb: "Caractéristiques du maillage actuellement chargé.",
            legend: null,
            params: [],
            result: null
        },
        {
            id: "curvature", icon: "curvature", label: "Courbure", key: "C",
            evaluable: true,   // calcul réel (méthode Desbrun) via CgreQuickItem.evaluateCurvature
            blurb: "Cartographie convexité et concavité (méthode Desbrun) pour juger les transitions de forme et repérer les arêtes vives.",
            legend: { kind: "scale", title: "Courbure", unit: "1/mm",
                      stops: ["#2f6df6", "#34c266", "#f4c020", "#e2483d"],
                      left: "Faible", mid: "", right: "Forte" },
            params: [
                { k: "type",        t: "select", label: "Type de courbure", opts: ["Gaussienne", "Moyenne", "Minimale", "Maximale"], def: "Moyenne" },
                { k: "sensitivity", t: "slider", label: "Sensibilité", min: 10, max: 100, def: 62, unit: "%" },
                { k: "smoothing",   t: "slider", label: "Lissage", min: 0, max: 100, def: 35, unit: "%" }
            ],
            result: { state: "ok", text: "Cliquez sur « Évaluer » pour calculer la courbure sur le modèle." }
        },
        {
            id: "compare", icon: "compare", label: "Comparaison", key: "D",
            blurb: "Mesure l'écart point-à-point avec un maillage de référence après alignement.",
            legend: { kind: "scale", title: "Écart au modèle", unit: "mm",
                      stops: ["#2f6df6", "#7fa8d8", "#eef0ee", "#e8a07a", "#e2483d"],
                      left: "−2,0", mid: "0", right: "+2,0" },
            params: [
                { k: "ref",   t: "file",   label: "Maillage de référence", def: "reference_v2.stl" },
                { k: "align", t: "select", label: "Alignement", opts: ["Automatique", "Manuel", "Par repères"], def: "Automatique" },
                { k: "tol",   t: "slider", label: "Tolérance ±", min: 0.1, max: 2, step: 0.1, def: 0.5, unit: "mm" }
            ],
            result: { state: "warn", text: "Écart max +1,8 mm · 96 % dans la tolérance" }
        },
        {
            id: "measure", icon: "measure", label: "Mesures", key: "M",
            blurb: "Mode « Point » : survolez la surface pour lire les coordonnées du point sous le curseur. Les autres modes (distance, angle, rayon) sont des aperçus.",
            legend: { kind: "readout", title: "Distance", value: "42,7", unit: "mm", sub: "2 points · arête aimantée" },
            params: [
                { k: "mode",  t: "seg",    label: "Mode", opts: ["Point", "Distance", "Angle", "Rayon"], def: "Point" },
                { k: "units", t: "select", label: "Unités", opts: ["mm", "cm", "pouces"], def: "mm" },
                { k: "snap",  t: "toggle", label: "Aimanter aux arêtes", def: true }
            ],
            result: null
        },
        {
            id: "thickness", icon: "thickness", label: "Épaisseur", key: "T",
            blurb: "Colore la paroi selon son épaisseur pour repérer les zones fragiles avant impression.",
            legend: { kind: "scale", title: "Épaisseur de paroi", unit: "mm",
                      stops: ["#5b2d8e", "#c8447e", "#f4922f", "#f4d63b"],
                      left: "0,8", mid: "3,0", right: "6,0" },
            params: [
                { k: "method", t: "select", label: "Méthode", opts: ["Sphère roulante", "Rayon normal"], def: "Sphère roulante" },
                { k: "tmin",   t: "slider", label: "Épaisseur min", min: 0.6, max: 3, step: 0.1, def: 1.2, unit: "mm" },
                { k: "tmax",   t: "slider", label: "Épaisseur max", min: 3, max: 8, step: 0.5, def: 5, unit: "mm" }
            ],
            result: { state: "warn", text: "3 zones sous l'épaisseur min — risque d'impression" }
        },
        {
            id: "section", icon: "section", label: "Coupe", key: "S",
            blurb: "Tranche le modèle par un plan pour inspecter l'intérieur et relever des cotes internes.",
            legend: { kind: "readout", title: "Position du plan", value: "18,5", unit: "mm", sub: "Plan Y · contour visible" },
            params: [
                { k: "plan",     t: "seg",    label: "Plan de coupe", opts: ["X", "Y", "Z"], def: "Y" },
                { k: "position", t: "slider", label: "Position", min: 5, max: 95, def: 48, unit: "%" },
                { k: "contour",  t: "toggle", label: "Afficher le contour", def: true }
            ],
            result: null
        }
    ]

    function toolById(id) {
        for (var i = 0; i < tools.length; ++i)
            if (tools[i].id === id)
                return tools[i];
        return null;
    }
}
