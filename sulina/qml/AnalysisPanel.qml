import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Sulina


// Reactive parameter panel for the active analysis tool (mockup `.panel`).
// `tool` is a Theme.tools entry; `params` is the {key: value} state for it.
// Controls are a faithful preview — they emit paramChanged but drive no
// cgre2 backend yet.
// Docked tool panel: always present on the right (symmetric with the left
// file browser). Shows the active tool's controls — or, for kind === "spec",
// the read-only mesh data sheet. Fills its parent; content scrolls if it
// overflows the pane height.
Item {
    id: panel

    property var tool: null
    property var params: ({})

    // read-only data sheet rows [{label, value}], used when tool.kind === "spec"
    property var specRows: []
    readonly property bool isSpec: !!(tool && tool.kind === "spec")

    // tool exposes a real computation (e.g. curvature → Desbrun)
    readonly property bool evaluable: !!(tool && tool.evaluable)

    signal paramChanged(string key, var value)
    signal resetClicked()
    signal exportClicked()
    signal evaluateClicked()

    implicitWidth: Theme.panelWidth

    Flickable {
        id: flick
        anchors.fill: parent
        contentWidth: width
        contentHeight: mainCol.implicitHeight
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        // discreet scrollbar (matches the rest of the dark UI)
        ScrollBar.vertical: ScrollBar {
            id: vsb
            policy: ScrollBar.AsNeeded
            background: Item {}
            contentItem: Rectangle {
                implicitWidth: 6; radius: 3; color: Theme.textFaint
                opacity: vsb.pressed ? 0.7 : vsb.hovered ? 0.5 : (vsb.size < 1.0 ? 0.25 : 0.0)
                Behavior on opacity { NumberAnimation { duration: 150 } }
            }
        }

        Column {
            id: mainCol
            width: flick.width

        // ---- head ----
        Item {
            width: parent.width
            height: 34 + Theme.panelPad + 13

            Rectangle {  // icon chip
                id: phIc
                x: Theme.panelPad
                y: Theme.panelPad
                width: 34; height: 34; radius: 9
                color: Theme.accentSoft
                border.color: Theme.accentSoft
                border.width: 1
                AppIcon {
                    anchors.centerIn: parent
                    name: panel.tool ? panel.tool.icon : "target"
                    size: 18
                    color: Theme.accent
                }
            }
            Text {
                anchors.left: phIc.right
                anchors.leftMargin: 11
                anchors.verticalCenter: phIc.verticalCenter
                text: panel.tool ? panel.tool.label : ""
                color: Theme.text
                font.family: Theme.fontSans
                font.pixelSize: 16
                font.weight: Font.DemiBold
            }
            Rectangle {  // kbd chip
                id: kbdChip
                visible: panel.tool && panel.tool.key
                anchors.left: lblMetrics.right
                anchors.leftMargin: 8
                anchors.verticalCenter: phIc.verticalCenter
                width: kbdT.implicitWidth + 10; height: 16; radius: 5
                color: Theme.surface2
                border.color: Theme.stroke; border.width: 1
                Text {
                    id: kbdT
                    anchors.centerIn: parent
                    text: panel.tool ? panel.tool.key : ""
                    color: Theme.textSoft
                    font.family: Theme.fontMono; font.pixelSize: 10
                }
            }
            // invisible metric to anchor the kbd chip just after the label
            Text {
                id: lblMetrics
                visible: false
                anchors.left: phIc.right
                anchors.leftMargin: 11
                text: panel.tool ? panel.tool.label : ""
                font.family: Theme.fontSans
                font.pixelSize: 16
                font.weight: Font.DemiBold
            }

            Rectangle {  // bottom divider
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                height: 1
                color: Theme.stroke2
            }
        }

        // ---- body ----
        Column {
            width: parent.width
            topPadding: Theme.panelPad
            leftPadding: Theme.panelPad
            rightPadding: Theme.panelPad
            bottomPadding: Theme.panelPad
            spacing: Theme.fieldGap

            Text {  // blurb
                width: parent.width - 2 * Theme.panelPad
                text: panel.tool ? panel.tool.blurb : ""
                color: Theme.textSoft
                font.family: Theme.fontSans
                font.pixelSize: 13
                lineHeight: 1.4
                wrapMode: Text.Wrap
            }

            Repeater {
                model: panel.tool ? panel.tool.params : []

                // one control per parameter
                Column {
                    id: ctl
                    required property var modelData
                    width: parent.width - 2 * Theme.panelPad
                    spacing: 8

                    readonly property var p: modelData
                    readonly property var val: (panel.params && panel.params[modelData.k] !== undefined)
                                               ? panel.params[modelData.k] : modelData.def

                    // uppercase label (hidden for the inline-labelled toggle)
                    Text {
                        visible: ctl.p.t !== "toggle"
                        text: ctl.p.label
                        color: Theme.textSoft
                        font.family: Theme.fontSans
                        font.pixelSize: 11
                        font.weight: Font.DemiBold
                        font.capitalization: Font.AllUppercase
                        font.letterSpacing: 0.6
                    }

                    // toggle: label left + switch right
                    Item {
                        visible: ctl.p.t === "toggle"
                        width: parent.width
                        height: 24
                        Text {
                            anchors.left: parent.left
                            anchors.verticalCenter: parent.verticalCenter
                            text: ctl.p.label
                            color: Theme.textSoft
                            font.family: Theme.fontSans
                            font.pixelSize: 11
                            font.weight: Font.DemiBold
                            font.capitalization: Font.AllUppercase
                            font.letterSpacing: 0.6
                        }
                        ToggleWidget {
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            value: ctl.val === true
                            onMoved: panel.paramChanged(ctl.p.k, v)
                        }
                    }

                    SliderWidget {
                        visible: ctl.p.t === "slider"
                        width: parent.width
                        p: ctl.p.t === "slider" ? ctl.p : null
                        value: ctl.p.t === "slider" ? ctl.val : 0
                        // Manual scale sliders are inert while "Échelle
                        // automatique" is on (thickness tool).
                        enabled: !(panel.tool && panel.tool.id === "thickness"
                                   && (ctl.p.k === "smin" || ctl.p.k === "smax")
                                   && panel.params && panel.params["auto"] === true)
                        onMoved: panel.paramChanged(ctl.p.k, v)
                    }
                    SegWidget {
                        visible: ctl.p.t === "seg"
                        width: parent.width
                        p: ctl.p.t === "seg" ? ctl.p : null
                        value: ctl.val
                        onMoved: panel.paramChanged(ctl.p.k, v)
                    }
                    SelectWidget {
                        visible: ctl.p.t === "select"
                        width: parent.width
                        p: ctl.p.t === "select" ? ctl.p : null
                        value: ctl.val
                        onMoved: panel.paramChanged(ctl.p.k, v)
                    }
                    FileWidget {
                        visible: ctl.p.t === "file"
                        width: parent.width
                        value: ctl.val
                    }
                }
            }

            // data sheet rows (kind === "spec") — read-only label / value
            Column {
                visible: panel.isSpec
                width: parent.width - 2 * Theme.panelPad
                spacing: 7
                Repeater {
                    model: panel.isSpec ? panel.specRows : []
                    delegate: Item {
                        required property var modelData
                        width: parent.width
                        implicitHeight: 20
                        Text {
                            id: specLbl
                            anchors.left: parent.left
                            anchors.verticalCenter: parent.verticalCenter
                            text: modelData.label
                            color: Theme.textSoft
                            font.family: Theme.fontSans
                            font.pixelSize: 12
                        }
                        Text {
                            anchors.left: specLbl.right
                            anchors.leftMargin: 12
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            text: modelData.value
                            color: Theme.text
                            font.family: Theme.fontMono
                            font.pixelSize: 12
                            horizontalAlignment: Text.AlignRight
                            elide: Text.ElideMiddle
                        }
                    }
                }
            }
        }

        // ---- result banner ----
        Loader {
            width: parent.width
            active: !!(panel.tool && panel.tool.result)
            visible: active
            sourceComponent: resultC
        }

        // ---- foot ---- (hidden for the read-only data sheet)
        Item {
            visible: !panel.isSpec
            width: parent.width
            implicitHeight: visible ? 40 + 14 + Theme.panelPad : 0
            RowLayout {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.leftMargin: Theme.panelPad
                anchors.rightMargin: Theme.panelPad
                anchors.topMargin: 14
                spacing: 9

                FootBtn {
                    primary: false
                    icon: "reset"
                    label: "Réinitialiser"
                    Layout.preferredWidth: implicitWidth
                    Layout.preferredHeight: 40
                    onClicked: panel.resetClicked()
                }
                FootBtn {
                    primary: true
                    icon: panel.evaluable ? "curvature" : "export"
                    label: panel.evaluable ? "Évaluer" : "Exporter le rapport"
                    Layout.fillWidth: true
                    Layout.preferredHeight: 40
                    onClicked: panel.evaluable ? panel.evaluateClicked() : panel.exportClicked()
                }
            }
        }
    }
    }

    // =============== result banner component ===============
    Component {
        id: resultC
        Item {
            implicitHeight: rBox.height + 12
            Rectangle {
                id: rBox
                x: Theme.panelPad
                width: parent.width - 2 * Theme.panelPad
                height: Math.max(38, rTxt.implicitHeight + 22)
                radius: Theme.rSm
                readonly property bool isOk: panel.tool && panel.tool.result && panel.tool.result.state === "ok"
                color: isOk ? "#1A33B07A" : "#1AE8B23A"
                border.width: 1
                border.color: isOk ? "#4733B07A" : "#4DE8B23A"

                Row {
                    anchors.fill: parent
                    anchors.margins: 11
                    spacing: 9
                    AppIcon {
                        anchors.verticalCenter: parent.verticalCenter
                        name: rBox.isOk ? "check" : "warning"
                        size: 16
                        color: rBox.isOk ? Theme.ok : Theme.warn
                    }
                    Text {
                        id: rTxt
                        width: parent.width - 25
                        anchors.verticalCenter: parent.verticalCenter
                        text: (panel.tool && panel.tool.result) ? panel.tool.result.text : ""
                        color: rBox.isOk ? Theme.ok : Theme.warn
                        font.family: Theme.fontSans
                        font.pixelSize: 13
                        wrapMode: Text.Wrap
                    }
                }
            }
        }
    }

    // =============== reusable controls ===============

    component ToggleWidget: Rectangle {
        id: tg
        property bool value: false
        signal moved(var v)
        width: 42; height: 24; radius: 13
        color: value ? Theme.accent : Theme.strokeStrong
        Behavior on color { ColorAnimation { duration: 150 } }
        Rectangle {
            width: 18; height: 18; radius: 9; color: "white"
            anchors.verticalCenter: parent.verticalCenter
            x: tg.value ? 21 : 3
            Behavior on x { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
        }
        TapHandler { onTapped: tg.moved(!tg.value) }
    }

    component SliderWidget: Item {
        id: sl
        property var p: null
        property real value: 0
        property bool enabled: true       // dimmed + inert when false
        signal moved(var v)
        implicitHeight: 22
        opacity: sl.enabled ? 1.0 : 0.35
        readonly property real trackW: width - 54
        readonly property real pct: p ? Math.max(0, Math.min(1, (value - p.min) / (p.max - p.min))) : 0

        function fmt(v) {
            if (p && p.step && p.step < 1) return v.toFixed(1).replace(".", ",");
            return Math.round(v).toString();
        }
        function setFromX(x) {
            if (!p) return;
            var t = Math.max(0, Math.min(1, x / sl.trackW));
            var v = p.min + t * (p.max - p.min);
            if (p.step) v = Math.round(v / p.step) * p.step;
            sl.moved(Number(v.toFixed(3)));
        }

        Rectangle {
            id: track
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            width: sl.trackW; height: 5; radius: 3
            color: Theme.strokeStrong
            Rectangle {
                width: parent.width * sl.pct; height: parent.height
                radius: 3; color: Theme.accent
            }
        }
        Rectangle {
            width: 16; height: 16; radius: 8; color: "white"
            border.color: "#33000000"; border.width: 1
            anchors.verticalCenter: parent.verticalCenter
            x: sl.trackW * sl.pct - 8
        }
        Text {
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            width: 48
            horizontalAlignment: Text.AlignRight
            text: sl.fmt(sl.value) + (sl.p && sl.p.unit ? " " + sl.p.unit : "")
            color: Theme.text
            font.family: Theme.fontMono
            font.pixelSize: 12
        }
        MouseArea {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: sl.trackW + 8
            enabled: sl.enabled
            onPressed: function(mouse) { sl.setFromX(mouse.x); }
            onPositionChanged: function(mouse) { if (pressed) sl.setFromX(mouse.x); }
        }
    }

    component SegWidget: Rectangle {
        id: seg
        property var p: null
        property var value
        signal moved(var v)
        implicitHeight: 34
        height: 34
        radius: Theme.rSm
        color: Theme.surface2
        border.color: Theme.stroke; border.width: 1
        RowLayout {
            anchors.fill: parent
            anchors.margins: 3
            spacing: 3
            Repeater {
                model: seg.p ? seg.p.opts : []
                Rectangle {
                    required property string modelData
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: 6
                    color: modelData === seg.value ? Theme.accent : "transparent"
                    Text {
                        anchors.centerIn: parent
                        text: modelData
                        color: modelData === seg.value ? "white"
                             : (segHover.hovered ? Theme.text : Theme.textSoft)
                        font.family: Theme.fontSans
                        font.pixelSize: 12
                        font.weight: Font.Medium
                    }
                    HoverHandler { id: segHover }
                    TapHandler { onTapped: seg.moved(modelData) }
                }
            }
        }
    }

    component SelectWidget: Rectangle {
        id: sel
        property var p: null
        property var value
        signal moved(var v)
        implicitHeight: 38
        height: 38
        radius: Theme.rSm
        color: Theme.surface2
        border.width: 1
        border.color: (selHover.hovered || pop.opened) ? Theme.strokeStrong : Theme.stroke
        Text {
            anchors.left: parent.left
            anchors.leftMargin: 11
            anchors.right: chev.left
            anchors.verticalCenter: parent.verticalCenter
            text: sel.value !== undefined ? sel.value : ""
            color: Theme.text
            font.family: Theme.fontSans
            font.pixelSize: 13
            elide: Text.ElideRight
        }
        AppIcon {
            id: chev
            anchors.right: parent.right
            anchors.rightMargin: 10
            anchors.verticalCenter: parent.verticalCenter
            name: "chevron"; size: 16
            color: pop.opened ? Theme.text : Theme.textSoft
            rotation: pop.opened ? 180 : 0
            Behavior on rotation { NumberAnimation { duration: 140; easing.type: Easing.OutCubic } }
        }
        HoverHandler { id: selHover }
        TapHandler { onTapped: pop.opened ? pop.close() : pop.open() }

        // drop-down list of options
        Popup {
            id: pop
            y: sel.height + 6
            x: 0
            width: sel.width
            padding: 4
            closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
            background: Rectangle {
                color: Theme.surfaceSolid
                radius: Theme.rSm
                border.color: Theme.stroke
                border.width: 1
            }
            contentItem: Column {
                spacing: 2
                Repeater {
                    model: sel.p ? sel.p.opts : []
                    delegate: Rectangle {
                        required property string modelData
                        width: pop.availableWidth
                        height: 30
                        radius: 6
                        color: optHover.hovered ? Theme.surface2 : "transparent"
                        Text {
                            anchors.left: parent.left
                            anchors.leftMargin: 9
                            anchors.right: ck.left
                            anchors.verticalCenter: parent.verticalCenter
                            text: modelData
                            color: modelData === sel.value ? Theme.accent : Theme.text
                            font.family: Theme.fontSans
                            font.pixelSize: 13
                            elide: Text.ElideRight
                        }
                        AppIcon {
                            id: ck
                            visible: modelData === sel.value
                            anchors.right: parent.right
                            anchors.rightMargin: 8
                            anchors.verticalCenter: parent.verticalCenter
                            name: "check"; size: 15; color: Theme.accent
                        }
                        HoverHandler { id: optHover }
                        TapHandler { onTapped: { sel.moved(modelData); pop.close() } }
                    }
                }
            }
        }
    }

    component FileWidget: Rectangle {
        id: fileW
        property var value
        implicitHeight: 38
        height: 38
        radius: Theme.rSm
        color: Theme.surface2
        border.width: 1
        border.color: fileHover.hovered ? Theme.strokeStrong : Theme.stroke
        Row {
            anchors.left: parent.left
            anchors.leftMargin: 11
            anchors.right: swap.left
            anchors.rightMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            spacing: 8
            AppIcon { anchors.verticalCenter: parent.verticalCenter
                      name: "layers"; size: 15; color: Theme.textSoft }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: fileW.value !== undefined ? fileW.value : ""
                color: Theme.text
                font.family: Theme.fontMono
                font.pixelSize: 12
                elide: Text.ElideMiddle
            }
        }
        Text {
            id: swap
            anchors.right: parent.right
            anchors.rightMargin: 11
            anchors.verticalCenter: parent.verticalCenter
            text: "Changer"
            color: Theme.accent
            font.family: Theme.fontSans
            font.pixelSize: 11
            font.weight: Font.Medium
        }
        HoverHandler { id: fileHover }
    }

    component FootBtn: Rectangle {
        id: fb
        property bool primary: false
        property string icon
        property string label
        signal clicked()

        implicitWidth: fbRow.implicitWidth + 24
        implicitHeight: 40
        radius: Theme.rSm
        color: primary ? Theme.accent : Theme.surface2
        border.width: primary ? 0 : 1
        border.color: Theme.stroke
        scale: fbTap.pressed ? 0.98 : 1.0
        Behavior on scale { NumberAnimation { duration: 80 } }

        Row {
            id: fbRow
            anchors.centerIn: parent
            spacing: 7
            AppIcon {
                anchors.verticalCenter: parent.verticalCenter
                name: fb.icon; size: 15
                color: fb.primary ? "white" : Theme.textSoft
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: fb.label
                color: fb.primary ? "white" : (fbHover.hovered ? Theme.text : Theme.textSoft)
                font.family: Theme.fontSans
                font.pixelSize: 13
                font.weight: Font.Medium
            }
        }
        HoverHandler { id: fbHover }
        TapHandler { id: fbTap; onTapped: fb.clicked() }
    }
}
