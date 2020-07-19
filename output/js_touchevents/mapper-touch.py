#!/usr/bin/env python

import websockets
import json, re
import mapper as mpr

ws = websockets.serve("localhost", 8765)

def handler(sig, event, id, val, tt):
    try:
        if event == mpr.SIG_UPDATE:
            print((sig.name, p, 'at T+', (tt-start).get_double()))
            ws.send({'touchMove': {'id': id, 'position': val}})
        elif event == mpr.SIG_INST_NEW:
            print('NEW TOUCH for sig', sig.name, 'instance', id)
            ws.send({'touchNew': id})
        elif event == mpr.SIG_REL_UPSTRM:
            print('RELEASE for sig', sig.name, 'instance', id)
            sig.release_instance(id)
            ws.send({'touchRelease': id})
    except:
        print('exception')

dev = mpr.device("js_touch_bridge")

# get display resolution?
touch = dev.add_signal(mpr.DIR_IN, "touch", 2, mpr.FLT, "normalized", 0, 1,
                       10, handler, mpr.SIG_ALL);

while True:
    dev.poll(5)
