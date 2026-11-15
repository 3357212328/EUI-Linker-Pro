import QtQuick
import QtQuick.Shapes

Item {
    id: root
    property string pathData: ""
    property color iconColor: "#ffffff"
    property real iconSize: 18

    width: iconSize
    height: iconSize
    implicitWidth: iconSize
    implicitHeight: iconSize

    Shape {
        id: vecShape
        width: 24
        height: 24
        preferredRendererType: Shape.CurveRenderer
        transformOrigin: Item.TopLeft
        scale: root.iconSize / 24.0

        ShapePath {
            strokeWidth: -1
            fillColor: root.iconColor
            Behavior on fillColor { ColorAnimation { duration: 180; easing.type: Easing.OutQuad } }

            PathSvg {
                path: root.pathData
            }
        }
    }
}
