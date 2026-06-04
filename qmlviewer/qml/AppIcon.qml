import QtQuick
import QtQuick.Shapes
import QmlViewer

// Crisp 24px line icon, ported from the mockup's <HIcon>. Strokes an SVG
// path (looked up by name in Theme.icons) with round caps/joins. The path
// is authored in a 0..24 viewBox and scaled to `size` about its centre.
Item {
    id: root

    property string name: "target"
    property real   size: 22
    property color  color: Theme.textSoft
    property real   sw: 1.7   // stroke width in viewBox units

    implicitWidth: size
    implicitHeight: size

    Shape {
        anchors.centerIn: parent
        width: 24
        height: 24
        scale: root.size / 24        // scales about Item.Center by default
        antialiasing: true
        preferredRendererType: Shape.CurveRenderer

        ShapePath {
            strokeColor: root.color
            fillColor: "transparent"
            strokeWidth: root.sw
            capStyle: ShapePath.RoundCap
            joinStyle: ShapePath.RoundJoin
            PathSvg { path: Theme.iconPath(root.name) }
        }
    }
}
