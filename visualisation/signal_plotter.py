#!/usr/bin/python

import sys
from PySide import QtGui, QtCore
from collections import deque
import mapper, random

display_sec = 10.0

signals = {}
width = 800
split = 600
height = 600
cleared = 1

x_scale = split / display_sec

def recheck_ranges(record):
    min = None
    max = None
    for i in record['vals']:
        if not min or i < min:
            min = i
        if not max or i > max:
            max = i
    record['min'][0] = min
    record['max'][0] = max

def sig_handler(sig, id, val, tt):
    then = dev.now() - display_sec
    # Find signal
    match = signals[sig.get_name()]
    match['vals'].append(val)
    if match['min'][0] == None or val < match['min'][1]:
        match['min'][0] = val
    if match['max'][0] == None or val > match['max'][1]:
        match['max'][0] = val
    match['tts'].append(tt)
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

def on_connect(con, action):
    index = con['src_name'].find('/', 1)
    srcdev = con['src_name'][0:index]
    srcsig = con['src_name'][index:]
    index = con['dest_name'].find('/', 1)
    destdev = con['dest_name'][0:index]
    destsig = con['dest_name'][index:]
    if action == mapper.MDB_NEW and srcsig != destsig:
        newsig = dev.add_input(srcsig, con['src_length'], con['src_type'], None, None, None, sig_handler)
        mon.connect(con['src_name'], dev.get_name() + srcsig)
        mon.disconnect(con['src_name'], con['dest_name'])
        signals[srcsig] = {'sig' : newsig, 'vals' : deque([]), 'tts' : deque([]), 'len' : 0, 'min' : [None, 0], 'max' : [None, 1], 'pen' : QtGui.QPen(QtGui.QBrush(QtGui.QColor(random.randint(0,255), random.randint(0,255), random.randint(0,255))), 2), 'src_dev' : srcdev, 'label' : 0}
    elif action == mapper.MDB_REMOVE:
        if srcsig != destsig or destsig == '/connect_here':
            return
        dev.remove_input(signals[destsig]['sig'])
        del signals[srcsig]

def on_link(link, action):
    if action == mapper.MDB_REMOVE:
        expired = [i for i in signals if signals[i]['src_dev'] == link['src_name']]
        for i in expired:
            dev.remove_input(signals[i]['sig'])
            del signals[i]

admin = mapper.admin()
dev = mapper.device('signal_plotter', 0, admin)
dev.add_input('/connect_here')
mon = mapper.monitor(admin, 0)
mon.db.add_connection_callback(on_connect)

def resize(pos, index):
    global split, x_scale
    split = pos
    x_scale = split / display_sec

class labels(QtGui.QFrame):
    def __init__(self, parent):
        QtGui.QFrame.__init__(self, parent)
        self.setStyleSheet("background-color: white;");

    def paintEvent(self, event):
        qp = QtGui.QPainter(self)
        self.drawLabels(event, qp)

    def drawLabels(self, event, qp):
        for i in signals:
            qp.setPen(signals[i]['pen'])
            #string = i + ' ( ' + str((len(signals[i]['vals']) - 1) / display_sec) + ' Hz )'
            qp.drawText(5, signals[i]['label'], i)

class plotter(QtGui.QFrame):
    def __init__(self, parent):
        QtGui.QFrame.__init__(self, parent)
        self.setStyleSheet("background-color: white;");

    def paintEvent(self, event):
        qp = QtGui.QPainter(self)
        qp.setRenderHint(QtGui.QPainter.Antialiasing, True)
        self.drawGraph(event, qp)

    def drawGraph(self, event, qp):
        then = dev.now() - display_sec
        x_sub = display_sec+then
        for i in signals:
            if not len (signals[i]['vals']):
                continue

            target = signals[i]['min'][0]
            if target < 0:
                target *= 1.1
            else:
                target *= 0.9
            signals[i]['min'][1] *= 0.9
            signals[i]['min'][1] += target * 0.1
            target = signals[i]['max'][0]
            if target < 0:
                target *= 0.9
            else:
                target *= 1.1
            signals[i]['max'][1] *= 0.9
            signals[i]['max'][1] += target * 0.1

            y_offset = 0
            y_scale = signals[i]['max'][1] - signals[i]['min'][1]
            if y_scale == 0:
                y_scale = 1
            else:
                y_scale = -height / (signals[i]['max'][1] - signals[i]['min'][1])
                y_offset = -height * signals[i]['max'][1] / (signals[i]['min'][1] - signals[i]['max'][1])

            path = None
            vals = signals[i]['vals']
            tts = signals[i]['tts']
            x = 0
            y = 0
            label_y = 0
            label_y_count = 0
            for j in range(len(vals)):
                x = (x_sub-tts[j])*x_scale
                y = vals[j]*y_scale+y_offset
                point = QtCore.QPointF(x, y)
                if not j:
                    path = QtGui.QPainterPath(point)
                else:
                    path.lineTo(point)
                    if label_y_count < 3:
                        label_y += y
                        label_y_count += 1
            if label_y_count:
                label_y /= label_y_count
            signals[i]['label'] *= 0.95
            signals[i]['label'] += label_y * 0.05
            qp.strokePath(path, signals[i]['pen'])

class gui(QtGui.QMainWindow):
    def __init__(self):
        QtGui.QMainWindow.__init__(self)
        self.setGeometry(300, 100, width, height)
        self.setWindowTitle('libmapper signal plotter')
        self.splitter = QtGui.QSplitter(QtCore.Qt.Horizontal, self)

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

app = QtGui.QApplication(sys.argv)
gui = gui()
gui.show()
sys.exit(app.exec_())
