#!/usr/bin/env python

"""Switch between several configurations for a particular device pair."""

import mapper as mpr
import wx

configs = [
    {'name': 'Duty', 'maps': [('tkgui.1/signal0', 'tk_pwm.1/duty', {'expression': 'y=x'})]},
    {'name': 'Freq', 'maps': [('tkgui.1/signal0', 'tk_pwm.1/freq')]},
    {'name': 'Gain', 'maps': [('tkgui.1/signal0', 'tk_pwm.1/gain')]},
]

class SwitcherFrame(wx.Frame):
    def __init__(self, parent, title, devices, configurations):
        wx.Frame.__init__(self, parent, title=title, size=(300,500))

        self.graph = mpr.graph()

        self.timer = wx.Timer(self, -1)
        self.Bind(wx.EVT_TIMER, self.OnTimer, self.timer)
        self.timer.Start(100) # every 100 ms

        panel = wx.Panel(self)
        box = wx.BoxSizer(wx.VERTICAL)

        self.devices = devices
        self.configs = configurations
        self.selected = None

        self.buttons = {}
        for c in self.configs:
            button = wx.Button(panel, id=-1, label=c['name'],
                               pos=(8, 8), size=(280, 28))
            button.Bind(wx.EVT_BUTTON, self.on_click(c))
            box.Add(button, 0, wx.ALL, 10)
            self.buttons[c['name']] = button

        panel.SetSizer(box)
        panel.Layout()

    def OnExit(self, event):
        if self.OnSave(None):
            self.Destroy()

    def OnTimer(self, event):
        self.graph.poll(0)

    def on_click(self, config):
        def find_sig(fullname):
            names = fullname.split('/', 1)
            dev = self.graph.devices().filter(mpr.PROP_NAME, names[0]).next()
            if dev:
                sig = dev.signals().filter(mpr.PROP_NAME, names[1]).next()
                return sig
        def find_map(srckeys, dstkey):
            srcs = [find_sig(k) for k in srckeys]
            dst = find_sig(dstkey)
            if not (all(srcs) and dst):
                print(srckeys, ' and ', dstkey, ' not all found on network!')
                return
            intersect = dst.maps()
            for s in srcs:
                intersect = intersect.intersect(s.maps())
            return intersect.next()
        def handler(event):
            self.SetTitle("%s clicked"%config['name'])
            for b in self.buttons:
                self.buttons[b].SetForegroundColour("Black");
            self.buttons[config['name']].SetForegroundColour("Blue");

            if self.selected:
                # remove previous maps
                for prev_map in self.selected.maps:
                    map = find_map(prev_map[0], prev_map[1])
                    if map != None:
                        map.release()

            self.selected = config

            for new_map in self.selected.maps:
                src = find_sig(new_map[0])
                dst = find_sig(new_map[1])
                if src != None and dst != None:
                    if len(new_map) > 2:
                        map = map(src, dst, new_map[2])
                    else:
                        map = map(src, dst)
                    map.push()

        return handler
    
if __name__=="__main__":
    if not globals().has_key('app'):
        app = wx.App(False)
    frame = SwitcherFrame(None, "Mapper Switcher", devices, configs)
    frame.Show(True)
    frame.SetFocus()
    app.MainLoop()
