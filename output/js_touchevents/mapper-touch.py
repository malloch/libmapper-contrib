#!/usr/bin/env python

import websockets
import json, re
import mapper

ws = websockets.serve("localhost", 8765)

def handler(sig, id, val, tt):
    try:
        print((sig.name, p, 'at T+', (tt-start).get_double()))
        ws.send({'touchMove': {'id': id, 'position': val}})
    except:
        print('exception')

def manage_instances(sig, id, flag, timetag):
    try:
        if flag == mapper.NEW_INSTANCE:
            ws.send({'touchNew': id})
        elif flag == mapper.UPSTREAM_RELEASE:
            print('RELEASE for sig', sig.name, 'instance', id)
            sig.release_instance(id)
            ws.send({'touchRelease': id})
    except:
        print('exception')

dev = mapper.device("js_touch_bridge")

# get display resolution?
touch = dev.add_input_signal("touch", 2, 'f', "normalized", 0, 1, handler)
touch.reserve_instances(10)
touch.set_instance_event_callback(manage_instances, mapper.INSTANCE_ALL)
