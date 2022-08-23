from PySide6.QtWidgets import QApplication, QMainWindow
from PySide6.QtCore import QTimer
import pyqtgraph as pg
import sys, math, time
from collections import deque
import libmapper as mpr

signals = {}
sigs_to_free = []
cleared = True

display_sec = 10.0

'''
todo/think about:
- use one plot per connected signal
    - add/remove plots as needed
- enable plotting vector signals
- use predicable colours instead of random
'''

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
            match['plots'][id] = None
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
        newsig = dev.add_signal(mpr.Direction.INCOMING, srcname, vec_len, mpr.Type.FLOAT,
                                None, None, None, 10, sig_handler, mpr.Signal.Event.ALL)
        if not newsig:
            print('  error creating signal', srcnamefull)
            return

        signals[srcname] = {'sig'       : newsig,
                            'vec_len'   : vec_len,
                            'vals'      : {},
                            'tts'       : {},
                            'pens'      : [pg.mkPen(color=i, width=3) for i in range(vec_len)],
                            'plots'     : {},
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
            sigs_to_free.append(signals[dstname]['sig'])
            del signals[dstname]

dev = mpr.Device('signal_plotter')
dev.add_signal(mpr.Direction.INCOMING, 'connect_here')
dev.graph().add_callback(on_map, mpr.Type.MAP)

class MainWindow(QMainWindow):

    def __init__(self):
        super(MainWindow, self).__init__()

        self.graphWidget = pg.PlotWidget()
        self.setCentralWidget(self.graphWidget)

        self.setWindowTitle('libmapper signal plotter')

        self.graphWidget.setAxisItems({'bottom': TimeAxisItem(orientation='bottom')})
        self.graphWidget.showGrid(x=True, y=True)

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

            for inst in signals[s]['vals']:
                tts = signals[s]['tts'][inst]
                vals = signals[s]['vals'][inst]
                vec_len = signals[s]['vec_len']
                to_pop = 0
                for tt in tts:
                    if tt > then:
                        break
                    to_pop += 1

                # TODO: use numpy to handle instances, vectors

                # using deques instead of arrays seems to be slightly more efficient (~1% cpu)
                for el in range(vec_len):
                    for j in range(to_pop):
                        vals[el].popleft()
                for j in range(to_pop):
                    tts.popleft()

                if not len (signals[s]['tts'][inst]):
                    continue

                if signals[s]['plots'][inst] == None:
                    signals[s]['plots'][inst] = [self.graphWidget.plot(tts, vals[el], pen=signals[s]['pens'][el], connect='finite') for el in range(vec_len)]
                else:
                    for el in range(vec_len):
                        signals[s]['plots'][inst][el].setData(tts, vals[el], connect='finite')

        self.graphWidget.setXRange(then, now, padding=0)

    def timer_event(self):
        global cleared

        dev.poll(10)

        while len(sigs_to_free):
            dev.remove_signal(sigs_to_free.pop())

        if len(signals):
            self.update_graph()
            cleared = False
        elif not cleared:
            self.graphWidget.clear()
            cleared = True

def remove_dev():
    global dev
    dev.free()

import atexit
atexit.register(remove_dev)

app = QApplication(sys.argv)
main = MainWindow()
main.show()
app.exec()
