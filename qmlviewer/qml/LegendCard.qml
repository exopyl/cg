import QtQuick
import QmlViewer

// Bottom-left legend for the active tool (mockup `.legend-card`). Two
// shapes: a heatmap colour scale, or a single readout value.
GlassPanel {
    id: card

    property var tool: null
    property var params: ({})

    // "point" mode (Mesure / Point): show the hovered surface coordinates
    // here instead of the tool's static readout.
    property bool      pointMode: false
    property bool      pointValid: false
    property vector3d  point: Qt.vector3d(0, 0, 0)

    // "distance" mode (Mesure / Distance): an anchored first point + the live
    // hovered point. The readout shows their separation, live.
    property bool      distanceMode: false
    property bool      anchorValid: false
    property vector3d  anchorPoint: Qt.vector3d(0, 0, 0)

    // distance is meaningful only once a first point is clicked and the cursor
    // is over the surface; -1 means "not measurable yet".
    readonly property real distance: (distanceMode && anchorValid && pointValid)
        ? anchorPoint.minus(point).length()
        : -1

    readonly property var lg: tool ? tool.legend : null
    readonly property bool isScale: lg && lg.kind === "scale" && !pointMode && !distanceMode

    implicitWidth: 236
    implicitHeight: col.implicitHeight + 26
    radius: Theme.rMd
    fill: Theme.surface
    stroke: Theme.stroke

    // fade/slide up in
    opacity: 0
    Component.onCompleted: opacity = 1
    Behavior on opacity { NumberAnimation { duration: 220; easing.type: Easing.OutCubic } }

    function readoutValue() {
        if (!tool) return "";
        if (distanceMode)
            return distance >= 0 ? distance.toFixed(1).replace(".", ",") : "—";
        if (tool.id === "section" && params && params.position !== undefined)
            return (5 + (params.position / 100) * 40).toFixed(1).replace(".", ",");
        if (tool.id === "measure") return "42,7";
        return lg ? lg.value : "";
    }
    function readoutSub() {
        if (distanceMode) {
            if (!anchorValid) return "Cliquez un premier point sur la surface";
            if (!pointValid)  return "1er point fixé · survolez le second point";
            return "Du 1er point au point survolé";
        }
        if (tool && tool.id === "section" && params)
            return "Plan " + params.plan + " · " + (params.contour ? "contour visible" : "contour masqué");
        return lg ? lg.sub : "";
    }

    Column {
        id: col
        x: 14
        y: 13
        width: parent.width - 28
        spacing: 0

        // title (+ faint unit, scale legends only)
        Row {
            spacing: 6
            Text {
                text: card.pointMode ? "Point survolé" : (card.lg ? card.lg.title : "")
                color: Theme.textSoft
                font.family: Theme.fontSans
                font.pixelSize: 11
                font.weight: Font.DemiBold
                font.capitalization: Font.AllUppercase
                font.letterSpacing: 0.6
            }
            Text {
                visible: card.isScale && card.lg && card.lg.unit
                anchors.verticalCenter: parent.verticalCenter
                text: card.lg ? card.lg.unit : ""
                color: Theme.textFaint
                font.family: Theme.fontSans
                font.pixelSize: 11
                font.weight: Font.Medium
            }
        }

        Item { width: 1; height: 9; visible: card.isScale }

        // ---- scale: gradient bar + endpoints ----
        Canvas {
            id: bar
            visible: card.isScale
            width: parent.width
            height: 11
            onPaint: {
                var ctx = getContext("2d");
                ctx.clearRect(0, 0, width, height);
                if (!card.isScale) return;
                var stops = card.lg.stops;
                var g = ctx.createLinearGradient(0, 0, width, 0);
                for (var i = 0; i < stops.length; ++i)
                    g.addColorStop(stops.length === 1 ? 0 : i / (stops.length - 1), stops[i]);
                ctx.fillStyle = g;
                // rounded rect
                var r = 6;
                ctx.beginPath();
                ctx.moveTo(r, 0);
                ctx.arcTo(width, 0, width, height, r);
                ctx.arcTo(width, height, 0, height, r);
                ctx.arcTo(0, height, 0, 0, r);
                ctx.arcTo(0, 0, width, 0, r);
                ctx.closePath();
                ctx.fill();
            }
            Connections {
                target: card
                function onToolChanged() { bar.requestPaint(); }
            }
        }

        Item { width: 1; height: 6; visible: card.isScale }

        Item {  // endpoints
            visible: card.isScale
            width: parent.width
            height: ends.implicitHeight
            Text {
                id: ends
                anchors.left: parent.left
                text: card.lg ? card.lg.left : ""
                color: Theme.textSoft
                font.family: Theme.fontMono
                font.pixelSize: 11
            }
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                visible: card.lg && card.lg.mid
                text: card.lg ? card.lg.mid : ""
                color: Theme.textSoft
                font.family: Theme.fontMono
                font.pixelSize: 11
            }
            Text {
                anchors.right: parent.right
                text: card.lg ? card.lg.right : ""
                color: Theme.textSoft
                font.family: Theme.fontMono
                font.pixelSize: 11
            }
        }

        // ---- readout: big value + sub ----
        Row {
            visible: !card.isScale && !card.pointMode
            spacing: 5
            Text {
                text: card.readoutValue()
                color: Theme.text
                font.family: Theme.fontMono
                font.pixelSize: 30
                font.weight: Font.DemiBold
            }
            Text {
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 4
                text: card.lg ? card.lg.unit : ""
                color: Theme.textSoft
                font.family: Theme.fontSans
                font.pixelSize: 14
            }
        }
        Text {
            visible: !card.isScale && !card.pointMode
            topPadding: 5
            width: parent.width
            text: card.readoutSub()
            color: Theme.textSoft
            font.family: Theme.fontSans
            font.pixelSize: 12
            wrapMode: Text.Wrap
        }

        // ---- point mode: hovered X/Y/Z (mesh coordinates) ----
        Item { width: 1; height: 9; visible: card.pointMode }
        Column {
            visible: card.pointMode
            width: parent.width
            spacing: 5
            Repeater {
                model: card.pointValid
                    ? [ { a: "X", v: card.point.x },
                        { a: "Y", v: card.point.y },
                        { a: "Z", v: card.point.z } ]
                    : []
                delegate: Row {
                    required property var modelData
                    spacing: 8
                    Text {
                        // axis label — same font/size as the distance readout
                        anchors.baseline: value.baseline
                        text: modelData.a
                        color: Theme.textFaint
                        font.family: Theme.fontMono
                        font.pixelSize: 30
                        font.weight: Font.DemiBold
                        width: 24
                    }
                    Text {
                        id: value
                        // matches the "Distance" readout value (fontMono · 30 · DemiBold)
                        text: modelData.v.toFixed(4)
                        color: Theme.text
                        font.family: Theme.fontMono
                        font.pixelSize: 30
                        font.weight: Font.DemiBold
                    }
                }
            }
            Text {
                visible: !card.pointValid
                text: "Survolez la surface…"
                color: Theme.textFaint
                font.family: Theme.fontSans
                font.pixelSize: 12
            }
        }
    }
}
