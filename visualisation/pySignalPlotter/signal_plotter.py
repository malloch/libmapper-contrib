from PySide6.QtWidgets import QApplication, QMainWindow
from PySide6.QtCore import QTimer
import pyqtgraph as pg
import sys, math, time
from collections import deque
import libmapper as mpr

signals = {}
sigs_to_free = []

display_sec = 10.0

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

        self.layout = pg.GraphicsLayoutWidget(show=True, title='libmapper signal plotter')
        self.setCentralWidget(self.layout)

        self.setWindowTitle('libmapper signal plotter')

        self.timer = QTimer()
        self.timer.setInterval(25)
        self.timer.timeout.connect(self.timer_event)
        self.timer.start()

    def update_graph(self):
        now = mpr.Time().get_double()
        then = now - display_sec   # 10 seconds ago
        for s in signals:
            if not len (signals[s]['vals']):
                # no active instances? remove this?
                continue

            if 'plot' not in signals[s]:
                plot = self.layout.addPlot(row=self.layout.nextRow(), col=0)
                plot.setAxisItems({'bottom': TimeAxisItem(orientation='bottom')})

                units = signals[s]['sig'][mpr.Property.UNIT]
                if units == 'unknown':
                    units = ''
                else:
                    units = ' (' + units + ')'
                plot.setLabel('left', signals[s]['sig'][mpr.Property.NAME] + units)

                plot.showGrid(x=True, y=True)

                signals[s]['plot'] = plot

            for inst in signals[s]['vals']:
                tts = signals[s]['tts'][inst]
                vals = signals[s]['vals'][inst]
                vec_len = signals[s]['vec_len']
                to_pop = 0
                for tt in tts:
                    if tt > then:
                        break
                    to_pop += 1

                # TODO: use numpy to handle instances, vectors?

                for el in range(vec_len):
                    for j in range(to_pop):
                        vals[el].popleft()
                for j in range(to_pop):
                    tts.popleft()

                if not len (signals[s]['tts'][inst]):
                    continue

                if signals[s]['curves'][inst] == None:
                    signals[s]['curves'][inst] = [signals[s]['plot'].plot(tts, vals[el], pen=signals[s]['pens'][el], connect='finite') for el in range(vec_len)]
                else:
                    for el in range(vec_len):
                        signals[s]['curves'][inst][el].setData(tts, vals[el], connect='finite')
                signals[s]['plot'].setXRange(then, now)

    def timer_event(self):
        dev.poll(10)

        while len(sigs_to_free):
            var = sigs_to_free.pop()
            self.layout.removeItem(var['plot'])
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
