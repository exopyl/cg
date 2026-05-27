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
    color: "#0f1320"

    // Frameless: we draw our own dark title bar (see `titleBar` below)
    // instead of stacking the QML menu bar under the native Windows
    // chrome. Native move/resize/snap are restored via WindowController
    // (startSystemMove / startSystemResize).
    flags: Qt.Window | Qt.FramelessWindowHint

    readonly property int chromeBarHeight: 38

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
        color: "#0b0e16"

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

            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: "◆ QmlViewer"
                color: "#e2e8f0"
                font.pixelSize: 13
                font.bold: true
                rightPadding: 14
            }

            Rectangle {
                id: fileBtn
                width: fileLabel.implicitWidth + 22
                height: root.chromeBarHeight
                anchors.verticalCenter: parent.verticalCenter
                color: (fileMouse.containsMouse || fileMenu.opened) ? "#1f2533" : "transparent"

                Text {
                    id: fileLabel
                    anchors.centerIn: parent
                    text: qsTr("File")
                    color: "#e2e8f0"
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

        // Center: window title (purely decorative). Constrained between the
        // two clusters so it elides instead of overlapping them on a narrow
        // window.
        Text {
            anchors.left: leftCluster.right
            anchors.right: winButtons.left
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            anchors.verticalCenter: parent.verticalCenter
            text: root.title
            color: "#566076"
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
                hoverColor: "#e81123"
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
            color: SplitHandle.pressed ? "#3182ce"
                 : SplitHandle.hovered ? "#4a5568"
                 : "#2d3748"
        }

        // Top area — inner horizontal split: [left folder tree] / [viewport].
        SplitView {
            SplitView.fillHeight: true
            SplitView.minimumHeight: 150
            orientation: Qt.Horizontal

            handle: Rectangle {
                implicitWidth: 6
                implicitHeight: 6
                color: SplitHandle.pressed ? "#3182ce"
                     : SplitHandle.hovered ? "#4a5568"
                     : "#2d3748"
            }

        // Left panel: folder navigator
        Rectangle {
            SplitView.preferredWidth: 240
            SplitView.minimumWidth: 120
            color: "#1a1f2e"
            border.color: "#2d3748"
            border.width: 1

            Column {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 6

                // Filesystem tree — drives + directories. Selection drives
                // root.currentFolder; mesh files of the selected folder show
                // up in the bottom strip.
                TreeView {
                    id: dirTree
                    width: parent.width
                    height: parent.parent.height - y - 24
                    clip: true

                    model: fsModel
                    selectionModel: ItemSelectionModel { model: fsModel }

                    // Critical: by default selectionBehavior is
                    // SelectionDisabled, so clicks don't set currentIndex
                    // and our Connections handler below never fires.
                    selectionBehavior: TableView.SelectRows
                    selectionMode: TableView.SingleSelection

                    // QFileSystemModel exposes 4 columns (Name / Size /
                    // Type / Date). Squash the non-Name columns to width 0
                    // so only the Name tree shows.
                    columnWidthProvider: function(column) {
                        return column === 0 ? dirTree.width : 0
                    }
                    onWidthChanged: forceLayout()

                    Component.onCompleted: {
                        // Expand to the initial data directory so the user
                        // sees their starting context unfolded. Pass the
                        // column explicitly: index(QString,int) is otherwise
                        // ambiguous with QAbstractItemModel::index(int,int,…).
                        var idx = fsModel.index(dataDir, 0)
                        if (idx.row >= 0)
                            dirTree.expandToIndex(idx)
                    }

                    Connections {
                        target: dirTree.selectionModel
                        function onCurrentChanged(currentIdx) {
                            // QFileSystemModel.filePath() isn't exposed as
                            // a QML-callable method in Qt 6.11; pull the
                            // path through the role API instead.
                            // FilePathRole = Qt::UserRole + 1.
                            var path = fsModel.data(currentIdx, Qt.UserRole + 1)
                            if (path && path.length > 0)
                                root.currentFolder = "file:///" + path
                        }
                    }

                    // QFileSystemModel populates directories asynchronously.
                    // When a directory finishes loading, the TreeView's row
                    // cache can be left out of sync (children rendered at the
                    // wrong depth / leaking to the parent level). Forcing a
                    // relayout once the load completes resynchronises it.
                    Connections {
                        target: fsModel
                        function onDirectoryLoaded(path) {
                            dirTree.forceLayout()
                        }
                    }

                    delegate: TreeViewDelegate {
                        id: dirDelegate
                        indentation: 16
                        implicitHeight: 24
                        rightPadding: 4
                        // When overriding `indicator`, the framework's
                        // depth-based binding on indicator.x is lost, so we
                        // bake depth*indentation into leftPadding ourselves.
                        leftPadding: dirDelegate.depth * dirDelegate.indentation + 18

                        // Expand chevron — right-aligned in its slot so its
                        // right edge lands exactly at contentItem.x (flush
                        // with the folder emoji).
                        indicator: Item {
                            implicitWidth: 16
                            implicitHeight: 16
                            x: dirDelegate.leftPadding - implicitWidth
                            // Center vertically in the row — the framework
                            // places overridden indicators at y=0 otherwise,
                            // making the chevron sit higher than the name.
                            y: (dirDelegate.height - implicitHeight) / 2
                            Text {
                                anchors.right: parent.right
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.verticalCenterOffset: 2
                                text: dirDelegate.expanded ? "▾" : "▸"
                                color: dirDelegate.hasChildren
                                       ? (dirDelegate.hovered ? "white" : "#a0aec0")
                                       : "transparent"
                                font.pixelSize: 16
                            }
                        }

                        background: Rectangle {
                            color: dirDelegate.highlighted ? "#264f78"
                                 : dirDelegate.hovered     ? "#2a2d2e"
                                 : "transparent"
                        }

                        // Single-text content — emoji + name as one string,
                        // avoids nested-positioner layout quirks.
                        contentItem: Text {
                            text: (dirDelegate.expanded ? "📂 " : "📁 ") + model.display
                            color: "white"
                            font.pixelSize: 12
                            verticalAlignment: Text.AlignVCenter
                            elide: Text.ElideRight
                        }
                    }

                    ScrollBar.vertical: ScrollBar { active: true }
                }
            }
        }

        // Center: cgre2 viewport + stats overlay
        Item {
            SplitView.fillWidth: true
            SplitView.minimumWidth: 250

            // cgre2-driven Vulkan render. Records draw commands into Qt's
            // scene-graph render pass via beforeRenderPassRecording.
            CgreQuickItem {
                id: cgreView
                anchors.fill: parent
                meshModel: meshModel
            }

            // Stats overlay (top-right)
            Rectangle {
                anchors.top: parent.top
                anchors.right: parent.right
                anchors.margins: 12
                width: 280
                color: "#1a1f2eee"
                radius: 6
                border.color: "#2d3748"
                visible: meshModel.loaded || meshModel.lastError.length > 0
                height: statsCol.implicitHeight + 24

                Column {
                    id: statsCol
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 6

                    Text {
                        text: meshModel.loaded ? meshModel.name : ""
                        color: "white"; font.pixelSize: 16; font.bold: true
                    }
                    Text {
                        text: meshModel.source
                        color: "#a0aec0"; font.pixelSize: 10
                        width: parent.width; elide: Text.ElideMiddle
                    }
                    Rectangle { width: parent.width; height: 1; color: "#2d3748" }

                    StatRow { label: "Sub-meshes"; value: meshModel.loaded ? meshModel.meshCount + "" : "—" }
                    StatRow { label: "Vertices";   value: meshModel.loaded ? meshModel.vertexCount + "" : "—" }
                    StatRow { label: "Faces";      value: meshModel.loaded ? meshModel.faceCount + "" : "—" }
                    StatRow { label: "Triangles?"; value: meshModel.loaded ? (meshModel.isTriangleMesh ? "yes" : "no") : "—" }
                    StatRow { label: "Diagonal";   value: meshModel.loaded ? meshModel.bboxDiagonal.toFixed(4) : "—" }
                    StatRow { label: "Load time";  value: meshModel.loaded ? meshModel.loadTimeMs + " ms" : "—" }

                    Text {
                        text: meshModel.lastError
                        color: "#fc8181"; font.pixelSize: 11
                        width: parent.width; wrapMode: Text.Wrap
                        visible: meshModel.lastError.length > 0
                    }
                }
            }

            // Idle hint
            Text {
                anchors.centerIn: parent
                text: "Select a preset on the left to load a mesh.\nLeft-drag to rotate, wheel to zoom."
                color: "#4a5568"
                font.pixelSize: 14
                horizontalAlignment: Text.AlignHCenter
                visible: !meshModel.loaded
            }
        }
    }

        // -- Bottom file browser strip ------------------------------

        Rectangle {
            id: fileBrowser
            SplitView.preferredHeight: 140
            SplitView.minimumHeight: 80
            color: "#0e1119"
            border.color: "#2d3748"
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
            anchors.margins: 8
            spacing: 6

            // Header: current folder path (folder is driven by the left
            // tree selection; mesh files are opened via File > Open).
            Item {
                width: parent.width
                height: 26

                Text {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    text: "Folder: " + folderModel.folder.toString().replace(/^file:\/+/, "")
                    color: "#a0aec0"
                    font.pixelSize: 11
                    elide: Text.ElideMiddle
                }
            }

            // Grid of file tiles — fills the panel width, wraps to extra
            // rows as the bottom strip grows, scrolls vertically on overflow.
            GridView {
                id: fileGrid
                width: parent.width
                height: parent.height - 32
                cellWidth: 138
                cellHeight: 104
                clip: true
                model: folderModel
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar { active: true }

                delegate: Item {
                    id: tileCell
                    width: fileGrid.cellWidth
                    height: fileGrid.cellHeight

                    required property string fileName
                    required property string fileSuffix
                    required property string filePath
                    required property url    fileUrl

                    Rectangle {
                    anchors.centerIn: parent
                    width: 130; height: 96; radius: 6
                    color: tileMouse.pressed ? "#1e3a5f"
                         : tileMouse.containsMouse ? "#374151" : "#1f2937"
                    border.width: meshModel.source === tileCell.filePath ? 2 : 1
                    border.color: meshModel.source === tileCell.filePath ? "#3182ce" : "#374151"

                    Column {
                        anchors.fill: parent
                        anchors.margins: 6
                        spacing: 4

                        // Small extension badge (centered chip)
                        Rectangle {
                            anchors.horizontalCenter: parent.horizontalCenter
                            width: 30; height: 16; radius: 3
                            color: root.colorForExt(fileSuffix)
                            Text {
                                anchors.centerIn: parent
                                text: fileSuffix.toUpperCase()
                                color: "white"
                                font.bold: true
                                font.pixelSize: 8
                            }
                        }

                        // Filename — bigger, wraps to a few lines.
                        Text {
                            width: parent.width
                            text: fileName
                            color: "white"
                            font.pixelSize: 12
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

                // Empty state
                Text {
                    anchors.centerIn: parent
                    visible: folderModel.count === 0
                    text: "No mesh files in this folder"
                    color: "#6b7280"
                    font.pixelSize: 12
                }
            }
        }
    }
    }

    // -- Resize borders (frameless) --------------------------------
    // Native resize via QWindow::startSystemResize. High z so they win
    // over content at the window edges; disabled while maximized. Edge
    // codes are Qt::Edges (Qt.TopEdge=1, LeftEdge=2, RightEdge=4,
    // BottomEdge=8) combined for corners.
    //
    // The window-control buttons sit in the top-right corner, so the top
    // edge stops short of them (rightMargin) and the right edge starts
    // below the title bar — otherwise their outer pixels would resize
    // instead of clicking. There's deliberately no top-right corner grip.
    ResizeHandle { edges: Qt.TopEdge;    z: 100; height: 5; anchors { top: parent.top; left: parent.left; right: parent.right; rightMargin: winButtons.width } }
    ResizeHandle { edges: Qt.BottomEdge; z: 100; height: 5; anchors { bottom: parent.bottom; left: parent.left; right: parent.right } }
    ResizeHandle { edges: Qt.LeftEdge;   z: 100; width: 5;  anchors { left: parent.left; top: parent.top; bottom: parent.bottom } }
    ResizeHandle { edges: Qt.RightEdge;  z: 100; width: 5;  anchors { right: parent.right; top: titleBar.bottom; bottom: parent.bottom } }
    ResizeHandle { edges: Qt.TopEdge | Qt.LeftEdge;     z: 101; width: 9; height: 9; anchors { top: parent.top; left: parent.left } }
    ResizeHandle { edges: Qt.BottomEdge | Qt.LeftEdge;  z: 101; width: 9; height: 9; anchors { bottom: parent.bottom; left: parent.left } }
    ResizeHandle { edges: Qt.BottomEdge | Qt.RightEdge; z: 101; width: 9; height: 9; anchors { bottom: parent.bottom; right: parent.right } }

    component StatRow: Row {
        property string label
        property string value
        spacing: 10
        Text {
            text: parent.label
            color: "#a0aec0"; font.pixelSize: 12
            width: 100
        }
        Text {
            text: parent.value
            color: "white"; font.pixelSize: 12
            font.family: "Consolas, monospace"
        }
    }

    // Window-control button for the custom title bar.
    component WinButton: Rectangle {
        id: wb
        property int code
        property color hoverColor: "#272d3d"
        signal clicked
        width: 46
        height: root.chromeBarHeight
        color: wbMouse.containsMouse ? wb.hoverColor : "transparent"
        Text {
            anchors.centerIn: parent
            text: String.fromCharCode(wb.code)
            font.family: "Segoe MDL2 Assets"
            font.pixelSize: 10
            color: "#e2e8f0"
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
