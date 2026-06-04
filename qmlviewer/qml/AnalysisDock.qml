import QtQuick
import QmlViewer

// Floating tool dock, centred at the top of the viewport. Two groups:
// real view controls (fit / orbit / grid) and the analysis-tool toggles
// (Theme.tools). Mirrors the mockup's `.dock`.
GlassPanel {
    id: dock

    property string activeTool: ""
    property bool   gridOn: true

    signal fitClicked()
    signal orbitClicked()
    signal gridToggled()
    signal toolToggled(string id)

    fill: Theme.surface
    stroke: Theme.strokeStrong
    radius: Theme.rLg

    implicitWidth: row.implicitWidth + 2 * Theme.dockPad
    implicitHeight: Theme.dockBtn + 2 * Theme.dockPad

    Row {
        id: row
        anchors.centerIn: parent
        spacing: 6

        // view controls
        Row {
            spacing: 4
            DockBtn { icon: "home";  label: "Recadrer"; onClicked: dock.fitClicked() }
            DockBtn { icon: "orbit"; label: "Orbite";   onClicked: dock.orbitClicked() }
            DockBtn { icon: "grid";  label: "Grille"; on: dock.gridOn; onClicked: dock.gridToggled() }
        }

        // divider
        Rectangle {
            width: 1; height: 28
            anchors.verticalCenter: parent.verticalCenter
            color: Theme.stroke
        }

        // analysis tools
        Row {
            spacing: 4
            Repeater {
                model: Theme.tools
                DockBtn {
                    required property var modelData
                    icon: modelData.icon
                    label: modelData.label
                    kbd: modelData.key
                    active: dock.activeTool === modelData.id
                    onClicked: dock.toolToggled(modelData.id)
                }
            }
        }
    }

    // ---- dock button ----
    component DockBtn: Rectangle {
        id: btn
        property string icon
        property string label
        property string kbd: ""
        property bool on: false       // toggled-on view control (subtle)
        property bool active: false   // selected analysis tool (accent)
        signal clicked()

        width: Theme.dockBtn
        height: Theme.dockBtn
        radius: Theme.rMd

        color: active ? Theme.accent
             : (on || hover.hovered) ? Theme.surface2
             : "transparent"
        border.width: 1
        border.color: active ? "transparent"
                     : on ? Theme.stroke : "transparent"

        scale: tap.pressed ? 0.94 : 1.0
        Behavior on scale { NumberAnimation { duration: 80 } }

        AppIcon {
            anchors.centerIn: parent
            name: btn.icon
            size: Theme.dockIcon
            color: btn.active ? "#FFFFFF"
                 : (btn.on || hover.hovered) ? Theme.text : Theme.textSoft
        }

        HoverHandler { id: hover }
        TapHandler {
            id: tap
            onTapped: btn.clicked()
        }

        // tooltip below the button
        Rectangle {
            id: tip
            visible: hover.hovered
            opacity: hover.hovered ? 1 : 0
            Behavior on opacity { NumberAnimation { duration: 120 } }
            anchors.top: parent.bottom
            anchors.topMargin: 12
            anchors.horizontalCenter: parent.horizontalCenter
            width: tipRow.implicitWidth + 18
            height: 28
            radius: 8
            color: Theme.surfaceSolid
            border.color: Theme.stroke
            border.width: 1

            Row {
                id: tipRow
                anchors.centerIn: parent
                spacing: 7
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: btn.label
                    color: Theme.text
                    font.family: Theme.fontSans
                    font.pixelSize: 12
                    font.weight: Font.Medium
                }
                Rectangle {
                    visible: btn.kbd.length > 0
                    anchors.verticalCenter: parent.verticalCenter
                    width: kbdT.implicitWidth + 10
                    height: 16
                    radius: 5
                    color: Theme.surface2
                    border.color: Theme.stroke
                    border.width: 1
                    Text {
                        id: kbdT
                        anchors.centerIn: parent
                        text: btn.kbd
                        color: Theme.textSoft
                        font.family: Theme.fontMono
                        font.pixelSize: 10
                    }
                }
            }
        }
    }
}
