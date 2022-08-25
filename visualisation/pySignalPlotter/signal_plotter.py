from PySide6.QtWidgets import QApplication, QMainWindow, QGraphicsProxyWidget, QPushButton, QLabel, QSpinBox, QCheckBox
from PySide6.QtCore import QTimer
import pyqtgraph as pg
import sys, math, time
from collections import deque
import libmapper as mpr

signals = {}
sigs_to_free = []

display_sec = 10.0

use_2d = False

class TimeAxisItem(pg.AxisItem):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)

    def tickStrings(self, values, scale, spacing):
        return [time.strftime('%H:%M:%S', time.localtime(value)) for value in values]

def sig_handler(sig, event, id, val, tt):
    now = tt.get_double()

    if event == mpr.Signal.Event.REL_UPSTRM:
        sig.instance(id).release()
    elif event != mpr.Signal.Event.UPDATE:
        return

    # Find signal
    try:
        match = signals[sig['name']]
    except:
        print('signal not found')
        return

    vec_len = match['vec_len']

    if id not in match['vals']:
        # don't bother recording and instance release
        if val != None:
            if isinstance(val, list):
                match['vals'][id] = [deque([val[el]]) for el in range(vec_len)]
            else:
                match['vals'][id] = [deque([val])]
            match['tts'][id] = deque([now])
            match['curves'][id] = None
        return
    elif val == None:
        for el in range(vec_len):
            match['vals'][id][el].append(float('nan'))
    elif isinstance(val, list):
        for el in range(vec_len):
            match['vals'][id][el].append(val[el])
    else:
        match['vals'][id][0].append(val)
    match['tts'][id].append(now)

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
        vec_len = src[mpr.Property.LENGTH]
        num_inst = src[mpr.Property.NUM_INSTANCES]
        newsig = dev.add_signal(mpr.Direction.INCOMING, srcname, vec_len, mpr.Type.FLOAT,
                                None, None, None, num_inst, sig_handler, mpr.Signal.Event.ALL)
        if not newsig:
            print('  error creating signal', srcnamefull)
            return

        newsig[mpr.Property.EPHEMERAL] = src[mpr.Property.EPHEMERAL]

        signals[srcname] = {'sig'       : newsig,
                            'vec_len'   : vec_len,
                            'vals'      : {},
                            'tts'       : {},
                            'pens'      : [pg.mkPen(color=i, width=3) for i in range(vec_len)],
                            'curves'    : {},
                            'label'     : 0 }
        print('  signals:', signals)
        mpr.Map(src, newsig).push()
        map.release()
    elif event == mpr.Graph.Event.REMOVED or event == mpr.Graph.Event.EXPIRED:
        print('  REMOVED', srcname, '->', '<self>/'+dstname)
        if dstname == 'connect_here' or dstname != srcname:
            return
        if dstname in signals:
            print('  freeing local signal', dstname)
            sigs_to_free.append(signals.pop(dstname))

dev = mpr.Device('signal_plotter')
dev.add_signal(mpr.Direction.INCOMING, 'connect_here')
dev.graph().add_callback(on_map, mpr.Type.MAP)

class MainWindow(QMainWindow):

    def __init__(self):
        super(MainWindow, self).__init__()

        self.layout = pg.LayoutWidget()
        self.setCentralWidget(self.layout)

        self.time_label = QLabel('Time Window:')
        self.layout.addWidget(self.time_label, row=0, col=0)

        self.time = QSpinBox()
        self.time.setRange(1, 60)
        self.time.setValue(10)
        self.time.setSuffix(' seconds')
        self.time.valueChanged.connect(self.update_time)
        self.layout.addWidget(self.time, row=0, col=1)

        self.button_use_2d = QCheckBox('Draw 2D vectors on canvas', self)
        self.button_use_2d.stateChanged.connect(self.update_use_2d)
        self.layout.addWidget(self.button_use_2d, row=0, col=3)

        self.plots = pg.GraphicsLayoutWidget(show=True)
        self.layout.addWidget(self.plots, row=1, colspan=4)

        self.setWindowTitle('libmapper signal plotter')

        self.timer = QTimer()
        self.timer.setInterval(25)
        self.timer.timeout.connect(self.timer_event)
        self.timer.start()

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

    def update_graph(self):
        now = mpr.Time().get_double()
        then = now - display_sec   # 10 seconds ago
        for s in signals:
            data = signals[s]
            if not len (data['vals']):
                # no active instances? remove this?
                continue

            if 'plot' in data:
                plot = data['plot']
            else:
                plot = self.plots.addPlot(row=self.plots.nextRow(), col=0)
                plot.setAxisItems({'bottom': TimeAxisItem(orientation='bottom')})

                units = data['sig'][mpr.Property.UNIT]
                if units == 'unknown':
                    units = ''
                else:
                    units = ' (' + units + ')'
                plot.setLabel('left', data['sig'][mpr.Property.NAME] + units)

                plot.showGrid(x=True, y=True)
                data['plot'] = plot

            updated = False
            for inst in data['vals']:
                tts = data['tts'][inst]
                vals = data['vals'][inst]
                vec_len = data['vec_len']

                # pop samples older than window
                # TODO: use numpy to handle instances, vectors?
                to_pop = 0
                for tt in tts:
                    if tt > then:
                        break
                    to_pop += 1
                for el in range(vec_len):
                    for j in range(to_pop):
                        vals[el].popleft()
                for j in range(to_pop):
                    tts.popleft()

                if not len (data['tts'][inst]):
                    continue

                if signals[s]['curves'][inst] == None:
                    if vec_len == 2 and use_2d == True:
                        signals[s]['curves'][inst] = [plot.plot(vals[0], vals[1], pen=pg.mkPen(color=inst, width=3), connect='finite')]
                    else:
                        signals[s]['curves'][inst] = [plot.plot(tts, vals[el], pen=pg.mkPen(color=inst*el+el, width=3), connect='finite') for el in range(vec_len)]
                else:
                    if vec_len == 2 and use_2d == True:
                        signals[s]['curves'][inst][0].setData(vals[0], vals[1], connect='finite')
                    else:
                        for el in range(vec_len):
                            signals[s]['curves'][inst][el].setData(tts, vals[el], connect='finite')
                updated = True

            if vec_len != 2 or use_2d == False:
                plot.setXRange(then, now)

            if not updated:
                signals[s]['plot'].clear()

    def timer_event(self):
        dev.poll(10)

        while len(sigs_to_free):
            var = sigs_to_free.pop()
            self.plots.removeItem(var['plot'])
            dev.remove_signal(var['sig'])

        if len(signals):
            self.update_graph()

def remove_dev():
    global dev
    dev.free()

import atexit
atexit.register(remove_dev)

app = QApplication(sys.argv)
main = MainWindow()
main.show()
app.exec()
