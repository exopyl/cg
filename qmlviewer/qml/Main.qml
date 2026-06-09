import QtQuick
import QtQuick.Controls
import QtQuick.Window
import QtQuick.Dialogs
import Qt.labs.folderlistmodel
import QtQml.Models
import QmlViewer

ApplicationWindow {
    id: root
    width: 1280
    height: 800
    visible: true
    title: "QmlViewer — Vulkan / cgre2"
    color: Theme.bg

    // Frameless: we draw our own dark title bar (see `titleBar` below)
    // instead of stacking the QML menu bar under the native Windows
    // chrome. Native move/resize/snap are restored via WindowController
    // (startSystemMove / startSystemResize).
    flags: Qt.Window | Qt.FramelessWindowHint

    readonly property int chromeBarHeight: 38

    // -- analysis-tool UI state (preview only; no cgre2 backend) ---
    // Active analysis tool id ("" = none), the viewport HUD grid toggle,
    // and per-tool parameter values keyed by tool id.
    // The right tool pane is always open; "spec" (Fiche technique) is the
    // default/home tool. activeTool is never empty.
    property string activeTool: "spec"
    property bool   gridOn: true
    property var    toolState: ({})

    // True when the Mesure tool is in "Point" mode → enables surface picking
    // and the hovered-coordinate readout.
    readonly property bool measurePoint: activeTool === "measure"
        && toolState["measure"] !== undefined
        && toolState["measure"].mode === "Point"

    // True when the Mesure tool is in "Distance" mode → click anchors a first
    // point and the legend shows the live distance to the hovered point.
    readonly property bool measureDistance: activeTool === "measure"
        && toolState["measure"] !== undefined
        && toolState["measure"].mode === "Distance"

    Component.onCompleted: initToolState()

    function initToolState() {
        var s = {};
        var tools = Theme.tools;
        for (var i = 0; i < tools.length; ++i) {
            var t = tools[i];
            var o = {};
            for (var j = 0; j < t.params.length; ++j)
                o[t.params[j].k] = t.params[j].def;
            s[t.id] = o;
        }
        toolState = s;
    }
    function setParam(id, k, v) {
        var s = Object.assign({}, toolState);
        s[id] = Object.assign({}, s[id]);
        s[id][k] = v;
        toolState = s;
    }
    function resetToolParams(id) {
        var t = Theme.toolById(id);
        if (!t) return;
        var s = Object.assign({}, toolState);
        var o = {};
        for (var j = 0; j < t.params.length; ++j)
            o[t.params[j].k] = t.params[j].def;
        s[id] = o;
        toolState = s;
    }
    function toggleTool(id) {
        // Clicking the active tool again returns to the data sheet (home);
        // the pane is always showing something.
        activeTool = (activeTool === id && id !== "spec") ? "spec" : id;
    }

    function vec3str(v) {
        return v.x.toFixed(2) + ", " + v.y.toFixed(2) + ", " + v.z.toFixed(2);
    }

    // Rows for the "Fiche technique" tool — live mesh data sheet.
    readonly property var meshSpec: meshModel.loaded
        ? [
            { label: "Nom",            value: meshModel.name },
            { label: "Source",         value: meshModel.source },
            { label: "Sous-maillages", value: meshModel.meshCount + "" },
            { label: "Sommets",        value: meshModel.vertexCount + "" },
            { label: "Faces",          value: meshModel.faceCount + "" },
            { label: "Triangulé",      value: meshModel.isTriangleMesh ? "oui" : "non" },
            { label: "Diagonale",      value: meshModel.bboxDiagonal.toFixed(4) },
            { label: "BBox min",       value: root.vec3str(meshModel.bboxMin) },
            { label: "BBox max",       value: root.vec3str(meshModel.bboxMax) },
            { label: "Centre",         value: root.vec3str(meshModel.bboxCenter) },
            { label: "Chargement",     value: meshModel.loadTimeMs + " ms" }
          ]
        : meshModel.lastError.length > 0
            ? [ { label: "Erreur", value: meshModel.lastError } ]
            : [ { label: "État", value: "Aucun maillage chargé" } ]

    function toggleMaximize() {
        if (root.visibility === Window.Maximized)
            root.showNormal()
        else
            root.showMaximized()
    }

    MeshModel { id: meshModel }

    // Shared "current folder" — drives both the left panel (folder tree)
    // and the bottom strip (mesh-file thumbnails).
    property url currentFolder: "file:///" + dataDir

    // Keyboard shortcuts mirroring the mockup: each tool key toggles its
    // panel; Escape closes the active one.
    Shortcut { sequences: ["F"]; onActivated: root.toggleTool("spec") }
    Shortcut { sequences: ["C"]; onActivated: root.toggleTool("curvature") }
    Shortcut { sequences: ["D"]; onActivated: root.toggleTool("compare") }
    Shortcut { sequences: ["M"]; onActivated: root.toggleTool("measure") }
    Shortcut { sequences: ["T"]; onActivated: root.toggleTool("thickness") }
    Shortcut { sequences: ["S"]; onActivated: root.toggleTool("section") }
    Shortcut { sequences: ["Escape"]; onActivated: root.activeTool = "spec" }

    FileDialog {
        id: fileDialog
        title: "Open a mesh file"
        nameFilters: [
            "Mesh files (*.obj *.ply *.stl *.off *.3ds *.glb)",
            "All files (*)"
        ]
        currentFolder: "file:///" + dataDir
        onAccepted: meshModel.loadUrl(selectedFile)
    }

    // Map mesh extension → swatch color for the bottom-strip tile badges.
    function colorForExt(ext) {
        var e = ext.toLowerCase()
        if (e === "obj")                       return "#3182ce"
        if (e === "stl")                       return "#d97706"
        if (e === "glb" || e === "gltf")       return "#10b981"
        if (e === "ply")                       return "#8b5cf6"
        if (e === "3ds")                       return "#eab308"
        if (e === "off")                       return "#6b7280"
        return "#4a5568"
    }

    // -- Custom title bar (frameless chrome) -----------------------

    Rectangle {
        id: titleBar
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: root.chromeBarHeight
        color: Theme.panel

        Rectangle {  // hairline under the bar
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            height: 1
            color: Theme.stroke2
        }

        // Empty areas of the bar drag the window (native, with Aero-snap);
        // the File button and window controls consume their own presses.
        DragHandler {
            target: null
            onActiveChanged: if (active) winCtrl.startSystemMove()
        }
        TapHandler {
            gesturePolicy: TapHandler.DragThreshold
            onDoubleTapped: root.toggleMaximize()
        }

        // Left: app mark + File menu
        Row {
            id: leftCluster
            anchors.left: parent.left
            anchors.leftMargin: 12
            anchors.verticalCenter: parent.verticalCenter
            spacing: 2

            Row {
                anchors.verticalCenter: parent.verticalCenter
                spacing: 6
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: "◆"
                    color: Theme.accent
                    font.pixelSize: 13
                }
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: "QmlViewer"
                    color: Theme.text
                    font.family: Theme.fontSans
                    font.pixelSize: 13
                    font.weight: Font.DemiBold
                }
            }

            Item { width: 14; height: 1 }   // gap before the File menu

            Rectangle {
                id: fileBtn
                width: fileLabel.implicitWidth + 22
                height: root.chromeBarHeight
                anchors.verticalCenter: parent.verticalCenter
                color: (fileMouse.containsMouse || fileMenu.opened) ? Theme.surface2 : "transparent"

                Text {
                    id: fileLabel
                    anchors.centerIn: parent
                    text: qsTr("File")
                    color: Theme.text
                    font.family: Theme.fontSans
                    font.pixelSize: 13
                }
                MouseArea {
                    id: fileMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: fileMenu.open()
                }
                Menu {
                    id: fileMenu
                    y: root.chromeBarHeight
                    Action {
                        text: qsTr("Open…")
                        shortcut: StandardKey.Open
                        onTriggered: fileDialog.open()
                    }
                    MenuSeparator {}
                    Action {
                        text: qsTr("Quit")
                        shortcut: StandardKey.Quit
                        onTriggered: Qt.quit()
                    }
                }
            }
        }

        // Center: window title (purely decorative).
        Text {
            anchors.left: leftCluster.right
            anchors.right: winButtons.left
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            anchors.verticalCenter: parent.verticalCenter
            text: root.title
            color: Theme.textFaint
            font.family: Theme.fontSans
            font.pixelSize: 12
            elide: Text.ElideRight
            horizontalAlignment: Text.AlignHCenter
        }

        // Right: minimize / maximize-restore / close.
        Row {
            id: winButtons
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            spacing: 0

            // Segoe MDL2 Assets glyph codepoints: minimize E921,
            // maximize E922, restore E923, close E8BB.
            WinButton {
                code: 0xE921
                onClicked: root.showMinimized()
            }
            WinButton {
                code: root.visibility === Window.Maximized ? 0xE923 : 0xE922
                onClicked: root.toggleMaximize()
            }
            WinButton {
                code: 0xE8BB
                hoverColor: Theme.danger
                onClicked: root.close()
            }
        }
    }

    // -- Layout ----------------------------------------------------

    // Outer vertical split: [top area] / [bottom file strip], both resizable.
    SplitView {
        anchors.top: titleBar.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        orientation: Qt.Vertical

        handle: Rectangle {
            implicitWidth: 6
            implicitHeight: 6
            color: SplitHandle.pressed ? Theme.accent
                 : SplitHandle.hovered ? Theme.strokeStrong
                 : Theme.stroke
        }

        // Top area — inner horizontal split: [left folder tree] / [viewport].
        SplitView {
            SplitView.fillHeight: true
            SplitView.minimumHeight: 150
            orientation: Qt.Horizontal

            handle: Rectangle {
                implicitWidth: 6
                implicitHeight: 6
                color: SplitHandle.pressed ? Theme.accent
                     : SplitHandle.hovered ? Theme.strokeStrong
                     : Theme.stroke
            }

        // Left panel: folder navigator
        Rectangle {
            SplitView.preferredWidth: 240
            SplitView.minimumWidth: 120
            color: Theme.panel
            border.color: Theme.stroke
            border.width: 1

            Column {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 6

                // Filesystem tree — drives root.currentFolder; mesh files of
                // the selected folder show up in the bottom strip.
                TreeView {
                    id: dirTree
                    width: parent.width
                    height: parent.parent.height - y - 24
                    clip: true

                    model: fsModel
                    selectionModel: ItemSelectionModel { model: fsModel }

                    selectionBehavior: TableView.SelectRows
                    selectionMode: TableView.SingleSelection

                    columnWidthProvider: function(column) {
                        return column === 0 ? dirTree.width : 0
                    }
                    onWidthChanged: forceLayout()

                    Component.onCompleted: {
                        var idx = fsModel.index(dataDir, 0)
                        if (idx.row >= 0)
                            dirTree.expandToIndex(idx)
                    }

                    Connections {
                        target: dirTree.selectionModel
                        function onCurrentChanged(currentIdx) {
                            // FilePathRole = Qt::UserRole + 1.
                            var path = fsModel.data(currentIdx, Qt.UserRole + 1)
                            if (path && path.length > 0)
                                root.currentFolder = "file:///" + path
                        }
                    }

                    Connections {
                        target: fsModel
                        function onDirectoryLoaded(path) {
                            dirTree.forceLayout()
                        }
                    }

                    delegate: TreeViewDelegate {
                        id: dirDelegate
                        indentation: 16
                        implicitHeight: 26
                        rightPadding: 4
                        leftPadding: dirDelegate.depth * dirDelegate.indentation + 18

                        indicator: Item {
                            implicitWidth: 16
                            implicitHeight: 16
                            x: dirDelegate.leftPadding - implicitWidth
                            y: (dirDelegate.height - implicitHeight) / 2
                            Text {
                                anchors.right: parent.right
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.verticalCenterOffset: 2
                                text: dirDelegate.expanded ? "▾" : "▸"
                                color: dirDelegate.hasChildren
                                       ? (dirDelegate.hovered ? Theme.text : Theme.textSoft)
                                       : "transparent"
                                font.pixelSize: 16
                            }
                        }

                        background: Rectangle {
                            radius: 6
                            color: dirDelegate.highlighted ? Theme.accentSoft
                                 : dirDelegate.hovered     ? Theme.surface2
                                 : "transparent"
                            border.width: dirDelegate.highlighted ? 1 : 0
                            border.color: Theme.accentSoft
                        }

                        contentItem: Text {
                            text: (dirDelegate.expanded ? "📂 " : "📁 ") + model.display
                            color: dirDelegate.highlighted ? Theme.text : Theme.text
                            font.family: Theme.fontSans
                            font.pixelSize: 12
                            verticalAlignment: Text.AlignVCenter
                            elide: Text.ElideRight
                        }
                    }

                    ScrollBar.vertical: ThinScrollBar {}
                }
            }
        }

        // Center: cgre2 viewport + floating chrome
        Item {
            id: viewportArea
            SplitView.fillWidth: true
            SplitView.minimumWidth: 250
            clip: true

            // cgre2-driven Vulkan render (bottom layer; handles mouse).
            CgreQuickItem {
                id: cgreView
                anchors.fill: parent
                meshModel: meshModel
                pickingEnabled: root.measurePoint || root.measureDistance
            }

            // HUD grid + vignette overlay. No pointer handlers → mouse falls
            // through to the viewport. Grid is faded out in the centre so it
            // doesn't clutter the model.
            Canvas {
                id: vpFx
                anchors.fill: parent
                onPaint: {
                    var ctx = getContext("2d");
                    ctx.clearRect(0, 0, width, height);
                    var cx = width / 2, cy = height * 0.42;

                    if (root.gridOn) {
                        ctx.strokeStyle = Theme.dark ? Qt.rgba(1, 1, 1, 0.06)
                                                     : Qt.rgba(0, 0, 0, 0.07);
                        ctx.lineWidth = 1;
                        ctx.beginPath();
                        for (var x = (width % 40) / 2; x < width; x += 40) {
                            ctx.moveTo(Math.floor(x) + 0.5, 0);
                            ctx.lineTo(Math.floor(x) + 0.5, height);
                        }
                        for (var y = (height % 40) / 2; y < height; y += 40) {
                            ctx.moveTo(0, Math.floor(y) + 0.5);
                            ctx.lineTo(width, Math.floor(y) + 0.5);
                        }
                        ctx.stroke();

                        // erase the grid around the centre so the mesh stays clean
                        ctx.globalCompositeOperation = "destination-out";
                        var maxR = Math.max(width, height) * 0.55;
                        var er = ctx.createRadialGradient(cx, cy, 0, cx, cy, maxR);
                        er.addColorStop(0.0, "rgba(0,0,0,1)");
                        er.addColorStop(0.45, "rgba(0,0,0,0.85)");
                        er.addColorStop(1.0, "rgba(0,0,0,0)");
                        ctx.fillStyle = er;
                        ctx.fillRect(0, 0, width, height);
                        ctx.globalCompositeOperation = "source-over";
                    }

                    // vignette — transparent centre, darker edges (softer in day mode)
                    var vigA = Theme.dark ? 0.42 : 0.18;
                    var vg = ctx.createRadialGradient(cx, cy, Math.min(width, height) * 0.25,
                                                       cx, cy, Math.max(width, height) * 0.75);
                    vg.addColorStop(0.0, "rgba(0,0,0,0)");
                    vg.addColorStop(1.0, "rgba(0,0,0," + vigA + ")");
                    ctx.fillStyle = vg;
                    ctx.fillRect(0, 0, width, height);
                }
                onWidthChanged: requestPaint()
                onHeightChanged: requestPaint()
                Connections {
                    target: root
                    function onGridOnChanged() { vpFx.requestPaint() }
                }
                Connections {
                    target: Theme
                    function onDarkChanged() { vpFx.requestPaint() }
                }
            }

            // Distance-measurement overlay: the anchored first point, the live
            // hovered point, and the dashed segment joining them. No pointer
            // handlers, so the mouse still reaches the viewport below.
            Canvas {
                id: measureFx
                anchors.fill: parent
                visible: root.measureDistance

                readonly property bool  aOn: cgreView.anchorValid
                readonly property point a:   cgreView.anchorScreen
                readonly property bool  bOn: cgreView.hoverValid
                readonly property point b:   cgreView.hoverScreen
                readonly property color accent: Theme.accent

                onAOnChanged: requestPaint()
                onAChanged: requestPaint()
                onBOnChanged: requestPaint()
                onBChanged: requestPaint()
                onVisibleChanged: requestPaint()
                Connections { target: Theme; function onDarkChanged() { measureFx.requestPaint() } }

                function drawDot(ctx, x, y, filled) {
                    ctx.lineWidth = 2;
                    ctx.strokeStyle = accent;
                    ctx.fillStyle = filled ? accent : (Theme.dark ? "#15191f" : "#ffffff");
                    ctx.beginPath();
                    ctx.arc(x, y, 5, 0, 2 * Math.PI);
                    ctx.fill();
                    ctx.stroke();
                    ctx.globalAlpha = 0.35;          // soft halo ring
                    ctx.beginPath();
                    ctx.arc(x, y, 8, 0, 2 * Math.PI);
                    ctx.stroke();
                    ctx.globalAlpha = 1.0;
                }

                onPaint: {
                    var ctx = getContext("2d");
                    ctx.clearRect(0, 0, width, height);
                    if (!visible) return;

                    var haveA = aOn && a.x >= 0;
                    var haveB = bOn && b.x >= 0;

                    if (haveA && haveB) {            // dashed connecting segment
                        ctx.strokeStyle = accent;
                        ctx.lineWidth = 1.5;
                        ctx.setLineDash([5, 4]);
                        ctx.beginPath();
                        ctx.moveTo(a.x, a.y);
                        ctx.lineTo(b.x, b.y);
                        ctx.stroke();
                        ctx.setLineDash([]);
                    }

                    if (haveA) drawDot(ctx, a.x, a.y, true);   // first point
                    if (haveB) drawDot(ctx, b.x, b.y, false);  // live point
                }
            }

            // axis gizmo, bottom-left — spins to follow the model orientation
            // (camera × trackball), projecting the world X/Y/Z axes to 2D.
            Canvas {
                id: gizmo
                width: 64; height: 64
                opacity: 0.92
                anchors.left: parent.left
                anchors.bottom: parent.bottom
                anchors.leftMargin: 20
                anchors.bottomMargin: 18

                property matrix4x4 m: cgreView.axisTransform
                onMChanged: requestPaint()

                onPaint: {
                    var ctx = getContext("2d");
                    ctx.clearRect(0, 0, width, height);
                    var cx = width / 2, cy = height / 2, L = 21;
                    var mm = gizmo.m;
                    // world axis → eye-space direction (upper 3×3 of axisTransform)
                    function dir(ex, ey, ez) {
                        return Qt.vector3d(
                            mm.m11 * ex + mm.m12 * ey + mm.m13 * ez,
                            mm.m21 * ex + mm.m22 * ey + mm.m23 * ez,
                            mm.m31 * ex + mm.m32 * ey + mm.m33 * ez);
                    }
                    var axes = [
                        { v: dir(1, 0, 0), c: "#e2483d", n: "X" },
                        { v: dir(0, 1, 0), c: "#34c266", n: "Y" },
                        { v: dir(0, 0, 1), c: "#2f6df6", n: "Z" }
                    ];
                    // eye-space +z points toward the viewer → draw far axes first
                    axes.sort(function(a, b) { return a.v.z - b.v.z; });

                    ctx.lineWidth = 2.2;
                    ctx.lineCap = "round";
                    ctx.font = "bold 11px 'IBM Plex Mono', monospace";
                    ctx.textAlign = "center";
                    ctx.textBaseline = "middle";

                    for (var i = 0; i < axes.length; ++i) {
                        var a = axes[i];
                        var tx = cx + a.v.x * L, ty = cy - a.v.y * L;  // screen y is down
                        var fade = 0.45 + 0.55 * ((a.v.z + 1) / 2);    // dim axes facing away
                        ctx.globalAlpha = fade;
                        ctx.strokeStyle = a.c;
                        ctx.beginPath(); ctx.moveTo(cx, cy); ctx.lineTo(tx, ty); ctx.stroke();
                        ctx.fillStyle = a.c;
                        ctx.beginPath(); ctx.arc(tx, ty, 2.2, 0, 2 * Math.PI); ctx.fill();
                        ctx.globalAlpha = Math.min(1, fade + 0.15);
                        ctx.fillText(a.n, cx + a.v.x * (L + 8), cy - a.v.y * (L + 8));
                    }
                    ctx.globalAlpha = 1.0;
                }
            }

            // Idle hint (no mesh loaded)
            Text {
                anchors.centerIn: parent
                text: "Sélectionnez un fichier en bas pour charger un maillage.\nGlisser-gauche pour pivoter, molette pour zoomer."
                color: Theme.textFaint
                font.family: Theme.fontSans
                font.pixelSize: 14
                horizontalAlignment: Text.AlignHCenter
                visible: !meshModel.loaded
            }

            // -- floating chrome --------------------------------------
            // (mesh stats live in the "Fiche technique" tool panel now.)

            // Top-right ghost buttons. Share is a stub; the sun/moon button
            // toggles the day/night theme.
            Row {
                anchors.top: parent.top
                anchors.right: parent.right
                anchors.margins: 20
                spacing: 8
                GhostBtn { icon: "share" }
                GhostBtn {
                    icon: Theme.dark ? "sun" : "moon"   // sun = switch to day, moon = switch to night
                    onClicked: Theme.dark = !Theme.dark
                }
            }

            // Floating dock, top-centre
            AnalysisDock {
                anchors.top: parent.top
                anchors.topMargin: 20
                anchors.horizontalCenter: parent.horizontalCenter
                activeTool: root.activeTool
                gridOn: root.gridOn
                onFitClicked: cgreView.resetView()
                onGridToggled: root.gridOn = !root.gridOn
                onOrbitClicked: { /* reserved: continuous orbit */ }
                onToolToggled: function(id) { root.toggleTool(id) }
            }

            // Legend card, bottom-left (above the gizmo)
            Loader {
                anchors.left: parent.left
                anchors.bottom: parent.bottom
                anchors.leftMargin: 20
                anchors.bottomMargin: 88
                active: root.activeTool.length > 0
                        && !!(Theme.toolById(root.activeTool) && Theme.toolById(root.activeTool).legend)
                visible: active
                sourceComponent: Component {
                    LegendCard {
                        tool: Theme.toolById(root.activeTool)
                        params: root.toolState[root.activeTool]
                        // Mesure / Point: show the live hovered coordinates here
                        pointMode: root.measurePoint
                        pointValid: cgreView.hoverValid
                        point: cgreView.hoverPoint
                        // Mesure / Distance: anchored first point → live hover
                        distanceMode: root.measureDistance
                        anchorValid: cgreView.anchorValid
                        anchorPoint: cgreView.anchorPoint
                    }
                }
            }

        }

        // Right tool pane — always present (symmetric with the left file
        // browser). Shows the active tool; defaults to the data sheet (spec).
        Rectangle {
            SplitView.preferredWidth: 312
            SplitView.minimumWidth: 260
            color: Theme.panel
            border.color: Theme.stroke
            border.width: 1

            AnalysisPanel {
                anchors.fill: parent
                anchors.margins: 1
                tool: Theme.toolById(root.activeTool)
                params: root.toolState[root.activeTool]
                specRows: root.meshSpec
                onParamChanged: function(key, value) {
                    root.setParam(root.activeTool, key, value)
                    // live recolour when the curvature type changes
                    if (root.activeTool === "curvature" && key === "type")
                        cgreView.recolorCurvature(value)
                    // switching measure mode drops any anchored first point
                    if (root.activeTool === "measure" && key === "mode")
                        cgreView.clearAnchor()
                }
                onResetClicked: {
                    root.resetToolParams(root.activeTool)
                    cgreView.clearAnalysis()   // no-op unless an analysis is shown
                    cgreView.clearAnchor()     // drop the distance first point
                }
                onExportClicked: console.log("export report (stub):", root.activeTool)
                onEvaluateClicked: {
                    if (root.activeTool === "curvature")
                        cgreView.evaluateCurvature(root.toolState["curvature"].type)
                    else if (root.activeTool === "thickness") {
                        var ts = root.toolState["thickness"]
                        cgreView.evaluateThickness(ts.method, ts.auto, ts.smin, ts.smax)
                    }
                }
            }
        }
    }

        // -- Bottom file browser strip ------------------------------

        Rectangle {
            id: fileBrowser
            SplitView.preferredHeight: 150
            SplitView.minimumHeight: 80
            color: Theme.panel
            border.color: Theme.stroke
            border.width: 1

        FolderListModel {
            id: folderModel
            folder: root.currentFolder
            nameFilters: ["*.obj", "*.stl", "*.ply", "*.glb", "*.gltf", "*.3ds", "*.off"]
            showDirs: false
            sortField: FolderListModel.Name
            caseSensitive: false
        }

        Column {
            anchors.fill: parent
            anchors.margins: 10
            spacing: 8

            // header: DOSSIER label + path (mono) + file count
            Item {
                width: parent.width
                height: 22

                AppIcon {
                    id: hdrIcon
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    name: "open"; size: 14; color: Theme.textSoft
                }
                Text {
                    id: hdrLabel
                    anchors.left: hdrIcon.right
                    anchors.leftMargin: 8
                    anchors.verticalCenter: parent.verticalCenter
                    text: "Dossier"
                    color: Theme.textSoft
                    font.family: Theme.fontSans
                    font.pixelSize: 11
                    font.weight: Font.DemiBold
                    font.capitalization: Font.AllUppercase
                    font.letterSpacing: 0.6
                }
                Text {
                    id: hdrCount
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    text: folderModel.count + (folderModel.count > 1 ? " fichiers" : " fichier")
                    color: Theme.textFaint
                    font.family: Theme.fontSans
                    font.pixelSize: 11
                }
                Text {
                    anchors.left: hdrLabel.right
                    anchors.leftMargin: 10
                    anchors.right: hdrCount.left
                    anchors.rightMargin: 10
                    anchors.verticalCenter: parent.verticalCenter
                    text: folderModel.folder.toString().replace(/^file:\/+/, "")
                    color: Theme.textFaint
                    font.family: Theme.fontMono
                    font.pixelSize: 11
                    elide: Text.ElideMiddle
                }
            }

            GridView {
                id: fileGrid
                width: parent.width
                height: parent.height - 30
                cellWidth: 138
                cellHeight: 104
                clip: true
                model: folderModel
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ThinScrollBar {}

                delegate: Item {
                    id: tileCell
                    width: fileGrid.cellWidth
                    height: fileGrid.cellHeight

                    required property string fileName
                    required property string fileSuffix
                    required property string filePath
                    required property url    fileUrl

                    Rectangle {
                        id: card
                        anchors.centerIn: parent
                        width: 130; height: 96; radius: Theme.rMd
                        readonly property bool selected: meshModel.source === tileCell.filePath
                        color: (card.selected || tileMouse.pressed) ? Theme.accentSoft : Theme.surface2
                        border.width: card.selected ? 1.5 : 1
                        border.color: card.selected ? Theme.accent
                                    : tileMouse.containsMouse ? Theme.strokeStrong : Theme.stroke
                        scale: tileMouse.pressed ? 0.97 : 1.0
                        Behavior on scale { NumberAnimation { duration: 80 } }

                        Column {
                            anchors.centerIn: parent
                            width: parent.width - 16
                            spacing: 7

                            // format: discreet colour dot + extension label
                            Row {
                                anchors.horizontalCenter: parent.horizontalCenter
                                spacing: 6
                                Rectangle {
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: 7; height: 7; radius: 3.5
                                    color: root.colorForExt(tileCell.fileSuffix)
                                }
                                Text {
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: tileCell.fileSuffix.toUpperCase()
                                    color: Theme.textSoft
                                    font.family: Theme.fontMono
                                    font.pixelSize: 9
                                    font.weight: Font.Medium
                                    font.letterSpacing: 0.5
                                }
                            }

                            Text {
                                width: parent.width
                                text: tileCell.fileName
                                color: Theme.text
                                font.family: Theme.fontSans
                                font.pixelSize: 12
                                font.weight: card.selected ? Font.DemiBold : Font.Normal
                                wrapMode: Text.Wrap
                                maximumLineCount: 3
                                elide: Text.ElideRight
                                horizontalAlignment: Text.AlignHCenter
                            }
                        }

                        MouseArea {
                            id: tileMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: meshModel.loadUrl(tileCell.fileUrl)
                        }
                    }
                }

                Text {
                    anchors.centerIn: parent
                    visible: folderModel.count === 0
                    text: "Aucun fichier de maillage dans ce dossier"
                    color: Theme.textFaint
                    font.family: Theme.fontSans
                    font.pixelSize: 12
                }
            }
        }
    }
    }

    // -- Resize borders (frameless) --------------------------------
    ResizeHandle { edges: Qt.TopEdge;    z: 100; height: 5; anchors { top: parent.top; left: parent.left; right: parent.right; rightMargin: winButtons.width } }
    ResizeHandle { edges: Qt.BottomEdge; z: 100; height: 5; anchors { bottom: parent.bottom; left: parent.left; right: parent.right } }
    ResizeHandle { edges: Qt.LeftEdge;   z: 100; width: 5;  anchors { left: parent.left; top: parent.top; bottom: parent.bottom } }
    ResizeHandle { edges: Qt.RightEdge;  z: 100; width: 5;  anchors { right: parent.right; top: titleBar.bottom; bottom: parent.bottom } }
    ResizeHandle { edges: Qt.TopEdge | Qt.LeftEdge;     z: 101; width: 9; height: 9; anchors { top: parent.top; left: parent.left } }
    ResizeHandle { edges: Qt.BottomEdge | Qt.LeftEdge;  z: 101; width: 9; height: 9; anchors { bottom: parent.bottom; left: parent.left } }
    ResizeHandle { edges: Qt.BottomEdge | Qt.RightEdge; z: 101; width: 9; height: 9; anchors { bottom: parent.bottom; right: parent.right } }

    // Discreet dark-theme scrollbar: a thin translucent handle that only
    // shows when the content overflows and brightens on hover/drag. No track.
    component ThinScrollBar: ScrollBar {
        id: sb
        policy: ScrollBar.AsNeeded
        background: Item {}
        contentItem: Rectangle {
            implicitWidth: 6
            implicitHeight: 6
            radius: 3
            color: Theme.textFaint
            opacity: sb.pressed ? 0.7
                   : sb.hovered ? 0.5
                   : (sb.size < 1.0 ? 0.25 : 0.0)
            Behavior on opacity { NumberAnimation { duration: 150 } }
        }
    }

    // Top-right ghost button (visual stub).
    component GhostBtn: Rectangle {
        id: gb
        property string icon
        signal clicked()
        width: 40; height: 40; radius: Theme.rMd
        color: gbHover.hovered ? Theme.surface2 : Theme.surface
        border.color: Theme.stroke
        border.width: 1
        scale: gbTap.pressed ? 0.96 : 1.0
        Behavior on scale { NumberAnimation { duration: 80 } }
        AppIcon {
            anchors.centerIn: parent
            name: gb.icon; size: 18
            color: gbHover.hovered ? Theme.text : Theme.textSoft
        }
        HoverHandler { id: gbHover }
        TapHandler { id: gbTap; onTapped: gb.clicked() }
    }

    // Window-control button for the custom title bar.
    component WinButton: Rectangle {
        id: wb
        property int code
        property color hoverColor: Theme.surface2
        signal clicked
        width: 46
        height: root.chromeBarHeight
        color: wbMouse.containsMouse ? wb.hoverColor : "transparent"
        Text {
            anchors.centerIn: parent
            text: String.fromCharCode(wb.code)
            font.family: "Segoe MDL2 Assets"
            font.pixelSize: 10
            color: Theme.text
        }
        MouseArea {
            id: wbMouse
            anchors.fill: parent
            hoverEnabled: true
            onClicked: wb.clicked()
        }
    }

    // Invisible edge/corner grab strip that hands resizing to the OS.
    component ResizeHandle: MouseArea {
        property int edges
        enabled: root.visibility !== Window.Maximized
        visible: enabled
        hoverEnabled: true
        cursorShape: {
            if (edges === (Qt.LeftEdge | Qt.TopEdge) || edges === (Qt.RightEdge | Qt.BottomEdge))
                return Qt.SizeFDiagCursor
            if (edges === (Qt.RightEdge | Qt.TopEdge) || edges === (Qt.LeftEdge | Qt.BottomEdge))
                return Qt.SizeBDiagCursor
            if (edges === Qt.LeftEdge || edges === Qt.RightEdge)
                return Qt.SizeHorCursor
            return Qt.SizeVerCursor
        }
        onPressed: winCtrl.startSystemResize(edges)
    }
}
