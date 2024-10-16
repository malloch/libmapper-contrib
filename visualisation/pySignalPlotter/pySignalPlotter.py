from PySide6 import QtGui, QtWidgets, QtCore
import pyqtgraph as pg
import sys, math, time
from collections import deque
import libmapper as mpr
import os

signals = {}
sigs_to_free = []

display_sec = 10.0

use_2d = False

icon_path = os.getcwd() + '/clear.png'

print('icon_path', icon_path)

class TimeAxisItem(pg.AxisItem):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)

    def tickStrings(self, values, scale, spacing):
        return [time.strftime('%H:%M:%S', time.localtime(value)) for value in values]

def sig_handler(sig, event, id, val, tt):
    now = tt.get_double()

    if event == mpr.Signal.Event.REL_UPSTRM:
        sig.Instance(id).release()
    elif event != mpr.Signal.Event.UPDATE:
        return

    # Find signal
    try:
        match = signals[sig[mpr.Property.NAME]]
    except:
        print('signal not found')
        return

    vec_len = match['vec_len']

    if id not in match['vals']:
        # don't bother recording an instance release
        if event != mpr.Signal.Event.REL_UPSTRM:
            if isinstance(val, list):
                match['vals'][id] = [deque([val[el]]) for el in range(vec_len)]
            else:
                match['vals'][id] = [deque([val])]
            match['tts'][id] = deque([now])
            match['curves'][id] = None
        return
    elif event == mpr.Signal.Event.REL_UPSTRM:
        for el in range(vec_len):
            match['vals'][id][el].append(float('nan'))
    elif isinstance(val, list):
        for el in range(vec_len):
            match['vals'][id][el].append(val[el])
    else:
        match['vals'][id][0].append(val)
    match['tts'][id].append(now)

# TODO: handle convergent maps or explicitly disallow them
def on_map(type, map, event):
    src = map.signals(mpr.Location.SOURCE)[0]
    dst = map.signals(mpr.Location.DESTINATION)[0]
    if src[mpr.Property.IS_LOCAL]:
        map.release()
        return
    elif not dst[mpr.Property.IS_LOCAL]:
        return
    elif map[mpr.Property.NUM_SIGNALS_IN] > 1:
        map.release()
        return
    srcname = src.device()[mpr.Property.NAME]+'/'+src[mpr.Property.NAME]
    dstname = dst[mpr.Property.NAME]
    if event == mpr.Graph.Event.NEW:
        if dstname == srcname:
            return

        # check if this plot already exists
        match = dev.signals().filter(mpr.Property.NAME, srcname)
        if match:
            print('plot', srcname, 'already exists')
            map.release()
            return

        vec_len = src[mpr.Property.LENGTH]
        num_inst = src[mpr.Property.NUM_INSTANCES]
        newsig = dev.add_signal(mpr.Direction.INCOMING, srcname, vec_len, mpr.Type.FLOAT,
                                None, None, None, num_inst, sig_handler, mpr.Signal.Event.REL_UPSTRM | mpr.Signal.Event.UPDATE)
        if not newsig:
            print('  error creating signal', srcnamefull)
            return

        newsig[mpr.Property.USE_INSTANCES] = True
        newsig[mpr.Property.EPHEMERAL] = True

        signals[srcname] = {'sig'       : newsig,
                            'vec_len'   : vec_len,
                            'vals'      : {},
                            'tts'       : {},
                            'pens'      : [pg.mkPen(color=i, width=3) for i in range(vec_len)],
                            'curves'    : {},
                            'label'     : 0 }
        mpr.Map(src, newsig).push()
        map.release()
    elif event == mpr.Graph.Event.REMOVED or event == mpr.Graph.Event.EXPIRED:
        if dstname == 'connect_here' or dstname != srcname:
            return
        if dstname in signals:
            print('  freeing local signal', dstname)
            sigs_to_free.append(signals.pop(dstname))

graph = mpr.Graph()
graph.add_callback(on_map, mpr.Type.MAP)

dev = mpr.Device('signal_plotter', graph)
dummy = dev.add_signal(mpr.Direction.INCOMING, 'connect_here')

class CloseButton(pg.ButtonItem):
    def __init__(self, parentItem):
        super(CloseButton, self).__init__(pg.icons.getGraphPixmap('auto'), 14, parentItem)

        self.setImageFile(icon_path)
        self.setScale(0.1)
        self.setOpacity(1)

class Plotter(pg.GraphicsLayoutWidget):
    def __init__(self):
        # TODO: pass in graph ref
        super(Plotter, self).__init__(show=True)
        self.setAcceptDrops(True)

    def add_plot(self, r, c, label):
        # TODO: pass in signal ref
        plot = self.addPlot(row=r, col=c)

        plot.setAxisItems({'bottom': TimeAxisItem(orientation='bottom')})
        plot.closeButton = CloseButton(plot)

        plot.showGrid(x=True, y=True)
        plot.setLabel('left', 'value')
        plot.setTitle(label)

        # TODO: add map display?

        return plot

    def remove_plot(self, plot):
        self.removeItem(plot)

    def next_row(self):
        self.nextRow()

class HLine(QtWidgets.QFrame):
    def __init__(self):
        super(HLine, self).__init__()
        self.setFrameShape(QtWidgets.QFrame.HLine)
        self.setFrameShadow(QtWidgets.QFrame.Sunken)

class MainWindow(QtWidgets.QMainWindow):

    def __init__(self):
        super(MainWindow, self).__init__()

        self.setAcceptDrops(True)

        time_label = QtWidgets.QLabel('Time Window:')

        time_spinbox = QtWidgets.QSpinBox(self)
        time_spinbox.setRange(1, 60)
        time_spinbox.setValue(10)
        time_spinbox.setSuffix(' seconds')
        time_spinbox.valueChanged.connect(self.update_time)

        button_use_2d = QtWidgets.QCheckBox('Draw 2D vectors on canvas', self)
        button_use_2d.stateChanged.connect(self.update_use_2d)

        self.plotter = Plotter()

#        addBelow = QtWidgets.QPushButton('+', self)
#        addBelow.setStyleSheet("font: 22px; border-radius: 5")
#        addBelow.clicked.connect(self.add_plot_below)
#
#        addRight = QtWidgets.QPushButton('+', self)
#        addRight.setStyleSheet("font: 22px; border-radius: 5")
#        addRight.clicked.connect(self.add_plot_right)

        self.setWindowTitle('libmapper signal plotter')

        grid = QtWidgets.QGridLayout()
        grid.setSpacing(10)
        grid.addWidget(time_label, 0, 0, QtCore.Qt.AlignmentFlag.AlignLeft)
        grid.addWidget(time_spinbox, 0, 1, QtCore.Qt.AlignmentFlag.AlignLeft)
        grid.addWidget(button_use_2d, 0, 3, QtCore.Qt.AlignmentFlag.AlignRight)
        grid.addWidget(HLine(), 1, 0, 1, 4)
        grid.addWidget(self.plotter, 2, 0, 1, 4)

        grid.setColumnStretch(0, 0)
        grid.setColumnStretch(1, 0)
        grid.setColumnStretch(2, 1)
        grid.setColumnStretch(3, 0)
        grid.setRowStretch(0, 0)
        grid.setRowStretch(1, 0)
        grid.setRowStretch(2, 1)

        centralWidget = QtWidgets.QWidget()
        centralWidget.setLayout(grid)
        self.setCentralWidget(centralWidget)

        self.timer = QtCore.QTimer()
        self.timer.setInterval(25)
        self.timer.timeout.connect(self.timer_event)
        self.timer.start()

    def dragEnterEvent(self, e):
        if e.mimeData().hasFormat("text/plain"):
            e.accept()

    def dropEvent(self, e):
        text = e.mimeData().text()

        if text.startswith('libmapper://signal '):
            e.setDropAction(QtCore.Qt.CopyAction)
            e.accept()

            # try extracting id first
            text = text[19:].split(' @id ')
            if len(text) == 2:
                id = int(text[1])
                print('id:', id)
                s = graph.signals().filter(mpr.Property.ID, id)
                if s:
                    s = s.next()
                    if s and not s[mpr.Property.IS_LOCAL]:
                        print('found signal by id')
                        mpr.Map(s, dummy).push()
                        return;
            if type(text) is list:
                text = text[0]

            # fall back to using device and signal names
            names = text.split('/', 1)
            print('names:', names)
            if len(names) != 2:
                print('error retrieving device and signal names')
                return

            d = graph.devices().filter(mpr.Property.NAME, names[0])
            if not d:
                print('error: could not find device', names[0])
                return
            s = d.next().signals().filter(mpr.Property.NAME, names[1])
            if not s:
                print('error: could not find signal', names)
                return
            s = s.next()
            if s[mpr.Property.IS_LOCAL]:
                print('error: plotting local signals is not allowed')
                return
            mpr.Map(s, dummy).push()

    def update_time(self, value):
        print('updating time window to', value)
        global display_sec
        display_sec = value

    def update_use_2d(self, value):
        print('updating use_2d to', value != 0)
        global use_2d
        use_2d = (value != 0)

        # TODO: clear and replot if necessary
#        for s in signals:
#            data = signals[s]
#            if data['vec_len'] == 2 and data['plot']:
#                data['plot'].clear()
#                for inst in data['vals']:
#                    del data['curves'][inst]
#                    data['curves'][inst] = None

    def add_plot_below(self):
        print('add plot below!')

    def add_plot_right(self):
        print('add plot right!')

    def closePlot(self, signame):
        sigs_to_free.append(signals.pop(signame))

    def update_graph(self):
        now = mpr.Time().get_double()
        then = now - display_sec   # 10 seconds ago
        for s in signals:
            data = signals[s]

            if 'plot' in data:
                plot = data['plot']
            else:
                plot = self.plotter.add_plot(self.plotter.next_row(), 0, data['sig'][mpr.Property.NAME])
                plot.closeButton.clicked.connect(lambda: self.closePlot(s))
                data['plot'] = plot

            if not len (data['vals']):
                # no active instances? remove this?
                continue

            updated = False
            for inst in data['vals']:
                tts = data['tts'][inst]
                vals = data['vals'][inst]
                vec_len = data['vec_len']

                # pop samples older than window
                # TODO: use numpy to handle instances, vectors?
                to_pop = 0
                for tt in tts:
                    if tt > (then - 1):
                        break
                    to_pop += 1
                for el in range(vec_len):
                    for j in range(to_pop):
                        vals[el].popleft()
                for j in range(to_pop):
                    tts.popleft()

                if not len(tts):
                    continue

                if signals[s]['curves'][inst] == None:
                    if vec_len == 2 and use_2d == True:
                        signals[s]['curves'][inst] = [plot.plot(vals[0], vals[1], pen=pg.mkPen(color=inst, width=3), connect='finite')]
                    else:
                        signals[s]['curves'][inst] = [plot.plot(tts, vals[el], pen=pg.mkPen(color=inst*vec_len+el, width=3), connect='finite') for el in range(vec_len)]
                else:
                    if vec_len == 2 and use_2d == True:
                        signals[s]['curves'][inst][0].setData(vals[0], vals[1], connect='finite')
                    else:
                        for el in range(vec_len):
                            signals[s]['curves'][inst][el].setData(tts, vals[el], connect='finite')
                updated = True

            if vec_len != 2 or use_2d == False:
                plot.setXRange(then, now)
            elif not updated:
                signals[s]['plot'].clear()

    def timer_event(self):
        dev.poll(10)

        while len(sigs_to_free):
            var = sigs_to_free.pop()
            self.plotter.remove_plot(var['plot'])
            dev.remove_signal(var['sig'])

        if len(signals):
            self.update_graph()

def remove_dev():
    global dev
    dev.free()

import atexit
atexit.register(remove_dev)

app = QtWidgets.QApplication(sys.argv)
main = MainWindow()
main.show()
app.exec()
