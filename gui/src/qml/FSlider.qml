// Slider in the Farside look, lying or upright.
import QtQuick
import QtQuick.Templates as T

T.Slider {
    id: slider

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset, implicitHandleWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset, implicitHandleHeight + topPadding + bottomPadding)
    padding: 8
    hoverEnabled: true
    opacity: enabled ? 1 : 0.45

    background: Rectangle {
        x: slider.leftPadding + (slider.horizontal ? 0 : (slider.availableWidth - width) / 2)
        y: slider.topPadding + (slider.horizontal ? (slider.availableHeight - height) / 2 : 0)
        implicitWidth: slider.horizontal ? 200 : 4
        implicitHeight: slider.horizontal ? 4 : 200
        width: slider.horizontal ? slider.availableWidth : 4
        height: slider.horizontal ? 4 : slider.availableHeight
        radius: 2
        color: "#34344a"

        Rectangle {
            y: slider.horizontal ? 0 : slider.visualPosition * parent.height
            width: slider.horizontal ? slider.visualPosition * parent.width : 4
            height: slider.horizontal ? 4 : parent.height - y
            radius: 2
            color: "#d4267e"
        }
    }
    handle: Rectangle {
        x: slider.leftPadding + (slider.horizontal ? slider.visualPosition * (slider.availableWidth - width) : (slider.availableWidth - width) / 2)
        y: slider.topPadding + (slider.horizontal ? (slider.availableHeight - height) / 2 : slider.visualPosition * (slider.availableHeight - height))
        implicitWidth: 20
        implicitHeight: 20
        radius: 10
        color: slider.visualFocus ? "#d4267e" : slider.pressed || slider.hovered ? "#ffffff" : "#f3f3f6"
        border.width: slider.visualFocus ? 3 : 0
        border.color: "#ffffff"
    }
}
