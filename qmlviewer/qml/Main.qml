import QtQuick
import QtQuick.Dialogs
import QmlViewer

Window {
    id: root
    width: 1280
    height: 800
    visible: true
    title: "QmlViewer — Vulkan / cgre2"
    color: "#0f1320"

    MeshModel { id: meshModel }

    readonly property var presets: [
        { label: "cube.obj",          file: "cube.obj"         },
        { label: "rabbit.obj",        file: "rabbit.obj"       },
        { label: "BunnyLowPoly.stl",  file: "BunnyLowPoly.stl" },
        { label: "Duck.glb",          file: "Duck.glb"         }
    ]

    function loadPreset(filename) {
        meshModel.load(dataDir + "/" + filename)
    }

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

    // -- Layout ----------------------------------------------------

    Row {
        anchors.fill: parent
        spacing: 0

        // Left panel: actions
        Rectangle {
            width: 240
            height: parent.height
            color: "#1a1f2e"
            border.color: "#2d3748"
            border.width: 1

            Column {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 8

                Text {
                    text: "Presets"
                    color: "#cbd5e0"
                    font.pixelSize: 13
                    font.bold: true
                }

                Repeater {
                    model: root.presets
                    delegate: Rectangle {
                        required property var modelData
                        width: parent.width
                        height: 30
                        radius: 4
                        color: presetMouse.pressed ? "#2c5282"
                              : presetMouse.containsMouse ? "#3182ce"
                              : "#2d3748"
                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.left: parent.left
                            anchors.leftMargin: 10
                            text: modelData.label
                            color: "white"
                            font.pixelSize: 12
                        }
                        MouseArea {
                            id: presetMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: root.loadPreset(modelData.file)
                        }
                    }
                }

                Item { width: 1; height: 6 }

                Rectangle {
                    width: parent.width; height: 34; radius: 4
                    color: openMouse.pressed ? "#22543d"
                          : openMouse.containsMouse ? "#2f855a" : "#38a169"
                    Text {
                        anchors.centerIn: parent
                        text: "Open file…"
                        color: "white"; font.pixelSize: 12; font.bold: true
                    }
                    MouseArea {
                        id: openMouse
                        anchors.fill: parent; hoverEnabled: true
                        onClicked: fileDialog.open()
                    }
                }
            }
        }

        // Center: cgre2 viewport + stats overlay
        Item {
            width: parent.width - 240
            height: parent.height

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
}
