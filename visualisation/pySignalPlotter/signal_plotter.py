#!/usr/bin/python

import sys, math
from PySide6 import QtGui, QtCore
from PySide6.QtWidgets import QApplication, QMainWindow, QFrame, QSplitter
from collections import deque
import libmapper as mpr, random

display_sec = 10.0

signals = {}
sigs_to_free = []
width = 800
split = 600
height = 600
cleared = 1

x_scale = split / display_sec

def sig_handler(sig, event, id, val, tt):
    now = tt.get_double()
    then = now - display_sec

    if event == mpr.Signal.Event.REL_UPSTRM:
        sig.instance(id).release()
    elif event == mpr.Signal.Event.INST_OFLW:
        return

    # Find signal
    try:
        match = signals[sig['name']]
    except:
        print('signal not found')
        return
    if id in match['vals']:
        match['vals'][id].append(val)
        match['tts'][id].append(now)
    else:
        match['vals'][id] = deque([val])
        match['tts'][id] = deque([now])
        match['pen'][id] = QtGui.QPen(QtGui.QBrush(QtGui.QColor(random.randint(0,255),
                                      random.randint(0,255), random.randint(0,255))), 2)
    if val is not None:
        if not math.isnan(val) and not math.isinf(val):
            if match['min'][0] == None or val < match['min'][1]:
                match['min'][0] = val
            if match['max'][0] == None or val > match['max'][1]:
                match['max'][0] = val

def on_map(type, map, event):
    print('on_map!')
    src = map.signals(mpr.Location.SOURCE)[0]
    dst = map.signals(mpr.Location.DESTINATION)[0]
    print(src['name'], '->', dst['name'])
    if src['is_local']:
        map.release()
        return
    elif not dst['is_local']:
        return
    srcname = src.device()['name']+'/'+src['name']
    dstname = dst['name']
    if event == mpr.Graph.Event.NEW:
        print('  ADDED map ', srcname, '->', '<self>/'+dstname)
        if dstname == srcname:
            return
        print('  rerouting...')
        newsig = dev.add_signal(mpr.Direction.INCOMING, srcname, 1, mpr.Type.FLOAT,
                                None, None, None, 10, sig_handler, mpr.Signal.Event.ALL)
        if not newsig:
            print('  error creating signal', srcnamefull)
            return
        color = QtGui.QColor(random.randint(0,255), random.randint(0,255), random.randint(0,255))
        signals[srcname] = {'sig' : newsig,
                            'vals' :{0: deque([])},
                            'tts' : {0: deque([])},
                            'min' : [None, 0],
                            'max' : [None, 1],
                            'color' : color,
                            'pen' : {0: QtGui.QPen(QtGui.QBrush(color), 2)},
                            'label' : 0}
        print('  signals:', signals)
        mpr.Map(src, newsig).push()
        map.release()
    elif event == mpr.Graph.Event.REMOVED or event == mpr.Graph.Event.EXPIRED:
        print('  REMOVED', srcname, '->', '<self>/'+dstname)
        if dstname == 'connect_here' or dstname != srcname:
            return
        if dstname in signals:
            print('  freeing local signal', dstname)
            sigs_to_free.append(signals[dstname]['sig'])
#            device.remove_signal(signals[dstname]['sig'])
            del signals[dstname]

dev = mpr.Device('signal_plotter')
dev.add_signal(mpr.Direction.INCOMING, 'connect_here')
dev.graph().add_callback(on_map, mpr.Type.MAP)

def resize(pos, index):
    global split, x_scale
    split = pos
    x_scale = split / display_sec

class labels(QFrame):
    def __init__(self, parent):
        QFrame.__init__(self, parent)
        self.setStyleSheet("background-color: white;");

    def paintEvent(self, event):
        qp = QtGui.QPainter(self)
        self.drawLabels(event, qp)

    def drawLabels(self, event, qp):
        for i in signals:
            qp.setPen(signals[i]['pen'][0])
            #string = i + ' ( ' + str((len(signals[i]['vals']) - 1) / display_sec) + ' Hz )'
            qp.drawText(5, signals[i]['label'], i)

class plotter(QFrame):
    def __init__(self, parent):
        QFrame.__init__(self, parent)
        self.setStyleSheet("background-color: white;");

    def paintEvent(self, event):
        qp = QtGui.QPainter(self)
        qp.setRenderHint(QtGui.QPainter.Antialiasing, True)
        self.drawGraph(event, qp)

    def drawGraph(self, event, qp):
        global x_scale
        then = mpr.Time().get_double() - display_sec   # 10 seconds ago
        for s in signals:
            if not len (signals[s]['vals']):
                continue
            target = signals[s]['min'][0]
            if target == None:
                continue
            signals[s]['min'][1] *= 0.9
            signals[s]['min'][1] += target * 0.1
            target = signals[s]['max'][0]
            signals[s]['max'][1] *= 0.9
            signals[s]['max'][1] += target * 0.1

            y_offset = 0
            y_scale = signals[s]['max'][1] - signals[s]['min'][1]
            if y_scale == 0:
                y_scale = -0.5
            else:
                y_scale = -height / y_scale
                y_offset = height - signals[s]['min'][1] * y_scale

            path = None
            label_y = 0
            label_y_count = 0

            for i in signals[s]['vals']:
                tts = signals[s]['tts'][i]
                vals = signals[s]['vals'][i]
                to_pop = 0
                for tt in tts:
                    if tt > then:
                        break
                    to_pop += 1
                for j in range(to_pop):
                    vals.popleft()
                    tts.popleft()

                if not len (signals[s]['vals'][i]):
                    continue
                x = 0
                y = 0
                started = False
                wasNan = False
                tt = tts[0]
                for t in range(len(tts)):
                    if vals[t] == None or math.isnan(vals[t]) or math.isinf(vals[t]):
                        wasNan = True
                        continue;
                    x = (tts[t] - then) * x_scale
                    y = vals[t] * y_scale + y_offset
                    point = QtCore.QPointF(x, y)
                    diff = tts[t] - tt
                    tt = tts[t]
                    if not started:
                        path = QtGui.QPainterPath(point)
                        started = True
                    else:
                        if wasNan or diff > 1.0:
                            path.moveTo(point)
                        else:
                            path.lineTo(point)
                        if label_y_count < 3:
                            label_y += y
                            label_y_count += 1
                    wasNan = False
                if label_y_count:
                    label_y /= label_y_count
                qp.strokePath(path, signals[s]['pen'][i])
            signals[s]['label'] *= 0.99
            signals[s]['label'] += label_y * 0.01

class gui(QMainWindow):
    def __init__(self):
        QMainWindow.__init__(self)
        self.setGeometry(300, 100, width, height)
        self.setWindowTitle('libmapper signal plotter')
        self.splitter = QSplitter(QtCore.Qt.Horizontal, self)

        self.setCentralWidget(self.splitter)

        self.graph = plotter(self)

        self.labels = labels(self)
        self.splitter.addWidget(self.graph)
        self.splitter.addWidget(self.labels)
        self.splitter.setSizes([600,200])
        self.splitter.splitterMoved.connect(resize)

        self.timer = QtCore.QBasicTimer()
        self.timer.start(30, self)

    def timerEvent(self, event):
        global cleared
        dev.poll(10)
        while len(sigs_to_free):
            dev.remove_signal(sigs_to_free.pop())
        if len(signals):
            self.graph.update(0, 0, split, height)
            self.labels.update(0, 0, width-split, height)
            cleared = 0
        elif not cleared:
            self.graph.update(0, 0, split, height)
            self.labels.update(0, 0, width-split, height)
            cleared = 1

    def resizeEvent(self, event):
        global width, height, split
        size = self.size()
        width = size.width()
        height = size.height()
        split = self.splitter.sizes()[0]

def remove_dev():
    global dev
    dev.free()

import atexit
atexit.register(remove_dev)

app = QApplication(sys.argv)
gui = gui()
gui.show()
sys.exit(app.exec())
