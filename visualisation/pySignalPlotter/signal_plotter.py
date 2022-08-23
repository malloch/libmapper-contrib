from PySide6.QtWidgets import QApplication, QMainWindow
from PySide6.QtCore import QTimer
import pyqtgraph as pg
import sys, math, random
from random import randint
from collections import deque
import libmapper as mpr

signals = {}
sigs_to_free = []
cleared = True

display_sec = 10.0
start = mpr.Time().get_double()

'''
todo/think about:
- use one plot per connected signal
    - add/remove plots as needed
- enable plotting vector signals
- use predicable colours instead of random
'''

def sig_handler(sig, event, id, val, tt):
    now = tt.get_double() - start

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

    if val == None:
        val = float('nan')

    if id in match['vals']:
        match['vals'][id].append(val)
        match['tts'][id].append(now)
    else:
        match['vals'][id] = deque([val])
        match['tts'][id] = deque([now])
        match['plots'][id] = None

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

        signals[srcname] = {'sig'   : newsig,
                            'vals' : {},
                            'tts'   : {},
                            'pen'   : pg.mkPen(color=(randint(0,255), randint(0,255), randint(0,255)),width=2),
                            'plots' : {},
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
#        self.splitter = QSplitter(QtCore.Qt.Horizontal, self)
#
#        self.setCentralWidget(self.splitter)

        self.graphWidget.setBackground('w')

        self.timer = QTimer()
        self.timer.setInterval(25)
        self.timer.timeout.connect(self.timer_event)
        self.timer.start()

    def update_graph(self):
        now = mpr.Time().get_double() - start
        then = now - display_sec   # 10 seconds ago
        for s in signals:
            if not len (signals[s]['vals']):
                # no active instances? remove this?
                continue

            for i in signals[s]['vals']:
                tts = signals[s]['tts'][i]
                vals = signals[s]['vals'][i]
                to_pop = 0
                for tt in tts:
                    if tt > then:
                        break
                    to_pop += 1

                # using deques instead of arrays seems to be slightly more efficient (~1% cpu)
                for j in range(to_pop):
                    vals.popleft()
                    tts.popleft()

                if not len (signals[s]['vals'][i]):
                    continue

                if signals[s]['plots'][i] == None:
                    signals[s]['plots'][i] = self.graphWidget.plot(tts, vals, pen=signals[s]['pen'], connect='finite')
                else:
                    signals[s]['plots'][i].setData(tts, vals, connect='finite')

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
