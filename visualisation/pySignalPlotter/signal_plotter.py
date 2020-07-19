#!/usr/bin/python

import sys, math
from PySide2 import QtGui, QtCore
from PySide2.QtWidgets import QApplication, QMainWindow, QFrame, QSplitter
from collections import deque
import mpr as mapper, random

display_sec = 10.0

signals = {}
sigs_to_free = []
width = 800
split = 600
height = 600
cleared = 1

x_scale = split / display_sec

def recheck_ranges(record):
    min = None
    max = None
    for i in record['vals']:
        if math.isnan(i) or math.isinf(i):
            continue
        if not min or i < min:
            min = i
        if not max or i > max:
            max = i
    record['min'][0] = min
    record['max'][0] = max

def sig_handler(sig, event, id, val, tt):
    now = tt.get_double()
    then = now - display_sec
    # Find signal
    try:
        match = signals[sig['name']]
    except:
        print('signal not found')
        return
    match['vals'].append(val)
    if not math.isnan(val) and not math.isinf(val):
        if match['min'][0] == None or val < match['min'][1]:
            match['min'][0] = val
        if match['max'][0] == None or val > match['max'][1]:
            match['max'][0] = val
    match['tts'].append(now)
    to_pop = -1
    for i in match['tts']:
        if i < then:
            to_pop += 1
    if to_pop <= 0:
        return
    recheck = 0
    for i in range(to_pop):
        popped = match['vals'].popleft()
        if popped <= match['min'][0] or popped >= match['max'][0]:
            recheck = 1
        match['tts'].popleft()
    if recheck:
        recheck_ranges(match)

def on_map(type, map, event):
    print('on_map!')
    src = map.signal(mpr.LOC_SRC)
    dst = map.signal(mpr.LOC_DST)
    if src['is_local']:
        map.release()
        return
    elif not dst['is_local']:
        return
    srcname = src.device()['name']+'/'+src['name']
    dstname = dst['name']
    if event == mpr.OBJ_NEW:
        print('  ADDED map ', srcname, '->', '<self>/'+dstname)
        if dstname == srcname:
            return
        print('  rerouting...')
        newsig = dev.add_signal(mpr.DIR_IN, srcname, 1, mpr.FLT, None, None,
                                None, None, sig_handler)
        if not newsig:
            print('  error creating signal', srcnamefull)
            return
        signals[srcname] = {'sig' : newsig, 'vals' : deque([]), 'tts' : deque([]), 'len' : 0, 'min' : [None, 0], 'max' : [None, 1], 'pen' : QtGui.QPen(QtGui.QBrush(QtGui.QColor(random.randint(0,255), random.randint(0,255), random.randint(0,255))), 2), 'label' : 0}
        print('  signals:', signals)
        mpr.map(src, newsig).push()
        map.release()
    elif event == mpr.OBJ_REM or event == mpr.OBJ_EXP:
        print('  REMOVED', srcname, '->', '<self>/'+dstname)
        if dstname == 'connect_here' or dstname != srcname:
            return
        if dstname in signals:
            print('  freeing local signal', dstname)
            sigs_to_free.append(signals[dstname]['sig'])
#            device.remove_signal(signals[dstname]['sig'])
            del signals[dstname]

dev = mpr.device('signal_plotter')
dev.add_signal(mpr.DIR_IN, 'connect_here')
dev.graph().add_callback(on_map, mpr.MAP)

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
            qp.setPen(signals[i]['pen'])
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
        then = mpr.timetag().get_double() - display_sec   # 10 seconds ago
        for i in signals:
            if not len (signals[i]['vals']):
                continue
            target = signals[i]['min'][0]
            if target == None:
                continue
            signals[i]['min'][1] *= 0.9
            signals[i]['min'][1] += target * 0.1
            target = signals[i]['max'][0]
            signals[i]['max'][1] *= 0.9
            signals[i]['max'][1] += target * 0.1

            y_offset = 0
            y_scale = signals[i]['max'][1] - signals[i]['min'][1]
            if y_scale == 0:
                y_scale = -0.5
            else:
                y_scale = -height / y_scale
                y_offset = height - signals[i]['min'][1] * y_scale

            path = None
            vals = signals[i]['vals']
            tts = signals[i]['tts']
            x = 0
            y = 0
            started = False
            wasNan = False
            label_y = 0
            label_y_count = 0
            tt = tts[0]
            for j in range(len(vals)):
                if math.isnan(vals[j]) or math.isinf(vals[j]):
                    wasNan = True
                    continue;
                x = (tts[j] - then) * x_scale
                y = vals[j] * y_scale + y_offset
                point = QtCore.QPointF(x, y)
                diff = tts[j] - tt
                tt = tts[j]
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
            signals[i]['label'] *= 0.99
            signals[i]['label'] += label_y * 0.01
            qp.strokePath(path, signals[i]['pen'])

class gui(QMainWindow):
    def __init__(self):
        QMainWindow.__init__(self)
        self.setGeometry(300, 100, width, height)
        self.setWindowTitle('libmpr signal plotter')
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
    print('called remove_dev')
    global dev
    del dev

import atexit
atexit.register(remove_dev)

app = QApplication(sys.argv)
gui = gui()
gui.show()
sys.exit(app.exec_())
