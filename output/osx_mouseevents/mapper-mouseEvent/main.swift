//
//  main.swift
//  mapper-mouseEvent
//
//  Created by malloch on 08/04/2019.
//  Copyright © 2019 Graphics and Experiential Media Lab. All rights reserved.
//

import Foundation
import AppKit

print("Hello, World!")

let displaySize = CGSize(width: 1920, height: 1080)
var running:Bool = true

class MprDevice {
    var device: Any
    var started: Bool = false

    init(name: String) {
        self.device = start_mpr_dev(name);
        started = true
    }

    public func receive() {
        if (started) {
            poll_mpr_dev(self.device as? mpr_dev)
        }
    }

    deinit {
        if (started == true) {
            quit_mpr_dev(device as? mpr_dev);
        }
        started = false
    }
}

@_cdecl("emit_mouse_evt")
public func emit_mouse_evt(type: Int32, _x: Float, _y: Float, count: Int) {
//    print("Got mouse evt", type, _x, _y)

    // check for NaN just in case
    if (_x != _x || _y != _y) {
        return
    }
    let point:CGPoint = CGPoint(x: CGFloat(_x) * displaySize.width,
                                y: CGFloat(_y) * displaySize.height)

    var evtType: CGEventType
    switch type {
    case MOUSE_MOVED:
        evtType = CGEventType.mouseMoved
        break
    case LEFT_BUTTON_UP:
        evtType = CGEventType.leftMouseUp
        break
    case LEFT_BUTTON_DOWN:
        evtType = CGEventType.leftMouseDown
        break
    case LEFT_MOUSE_DRAGGED:
        evtType = CGEventType.leftMouseDragged
        break
    case RIGHT_BUTTON_UP:
        evtType = CGEventType.rightMouseUp
        break
    case RIGHT_BUTTON_DOWN:
        evtType = CGEventType.rightMouseDown
        break
    case RIGHT_MOUSE_DRAGGED:
        evtType = CGEventType.rightMouseDragged
        break
    case SCROLL_WHEEL:
        post_scroll_evt(Int32(point.x), Int32(point.y))
        return
    default:
        print("Unknown event type", type)
        return
    }

    let evt = CGEvent(mouseEventSource: nil,
                      mouseType: evtType,
                      mouseCursorPosition: point,
                      mouseButton: CGMouseButton.left)
    if (count > 1) {
        evt?.setIntegerValueField(CGEventField.mouseEventClickState, value: Int64(count))
    }
    evt?.post(tap: .cghidEventTap)
}

private func close() {
    running = false
}

var device = MprDevice(name: "mouse")

signal(SIGINT) { s in close() }

while (running) {
    device.receive()
}







