import QtQuick
import QmlViewer

// Frosted floating surface used by the dock, parameter panel, legend and
// stats card. True backdrop blur isn't available cheaply over the Vulkan
// viewport, so we approximate the mockup's glass look with a semi-opaque
// fill plus a hairline border.
Rectangle {
    id: root

    property color fill: Theme.surface
    property color stroke: Theme.stroke
    property int   strokeWidth: 1

    color: fill
    radius: Theme.rLg
    border.color: stroke
    border.width: strokeWidth
}
