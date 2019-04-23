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

class MapperDevice {
    var device: Any
    var started: Bool = false

    init(name: String) {
        self.device = start_mapper_device(name);
        started = true
    }

    public func receive() {
        if (started) {
            poll_mapper_device(self.device as? mapper_device)
        }
    }

    deinit {
        if (started == true) {
            quit_mapper_device(device as? mapper_device);
        }
        started = false
    }
}

@_cdecl("emit_mouse_evt")
public func emit_mouse_evt(type: Int32, _x: Float, _y: Float, count: Int) {
//    print("Got mouse evt", type, _x, _y)
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
//        if (point.x > 10) {
//            point.x = 10
//        }
//        if (point.y > 10) {
//            point.y = 10
//        }
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

var device = MapperDevice(name: "mouse")

signal(SIGINT) { s in close() }

while (running) {
    device.receive()
}







