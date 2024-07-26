import libmapper as mpr
import numpy as np
import ntplib
import pandas
import sys, signal

#rate = 0
done = False

'''
need arg to specify column to interpret as dates?

TODO;
- sample interpolation
– add i64 support to libmapper?
'''

print('using libmapper version:', mpr.__version__, 'with' if mpr.has_numpy() else 'without', 'numpy support')

def handler_done(signum, frame):
    global done
    done = True

signal.signal(signal.SIGINT, handler_done)
signal.signal(signal.SIGTERM, handler_done)

# display fields by name, type, length
# allow users to easily combine columns into vectors, adjust namesm, add units/ranges
# allow users to easily specify which fields to omit, which ones to signalify

class Player():
    def __init__(self, filename, dates):
        print('Player.__init__', filename)
        devname = filename.split('/')[-1].split('.')[0]
        print('trying to create device with name:', devname)
        self.dev = mpr.Device(devname)
        print('trying to read file:', filename)
        if dates == None:
            dates = []
        elif not isinstance(dates, list):
            dates = [dates]
        self.data = pandas.read_csv(filename, parse_dates=dates)
        self.num_rows = self.data.shape[0]
        keys = self.data.keys()
        types = self.data.dtypes
        self.signals = []

        # create input signal for playback
        #   value is index?
        #   or rate, index (index is also updated internally)
        index = self.dev.add_signal(mpr.Direction.INCOMING, 'index', 1, mpr.Type.INT32, callback=self.index_handler)
        index.set_value(0)

        # create output signals for each field
        # retrieve a row to check datatypes
        # extract fields
        # calculate ranges?
        # convert ids/strings to ordinals
        # convert "YES"/"NO" to int
        # convert dates to ntp timetags?
        row = self.data.iloc[0]
        for i in range(len(keys)):
            if types[i] != 'object':
                print('key ', keys[i], 'has type', types[i])
                t = mpr.Type.NP_FLOAT
                col = row.iat[i]
                if type(col) == 'int64':
                    t = mpr.Type.NP_INT32
                print('using type', t, 'for signal', keys[i])
                self.signals.append(self.dev.add_signal(mpr.Direction.OUTGOING, keys[i].replace(' ', '_'), 1, t))
            else:
                self.signals.append(None)

    def index_handler(self, sig, event, id, val, time):
        # use modulo index for easy looping
        val %= self.num_rows

        row = self.data.iloc[val]
        for i in range(len(self.signals)):
            if self.signals[i] != None:
                col = row.iat[i]
                if type(col) == pandas._libs.tslibs.timestamps.Timestamp:
                    col = col.to_pydatetime().timestamp()
                if self.signals[i][mpr.Property.TYPE] == mpr.Type.NP_INT32:
                    col = int(col)
                elif self.signals[i][mpr.Property.TYPE] == mpr.Type.NP_FLOAT:
                    col = float(col)
                else:
                    continue
                self.signals[i].set_value(col)
        self.dev.update_maps()

    def poll(self):
        while not done:
            self.dev.poll(10)
#            if rate:
#                # need option to link rate to timestamps (if they exist) rather than index
#                global index, indexf
#                index_val = index.value()
#
#                # rate is in indexes/second
#                new_index_val = index_val + rate

#            if rate.value():
#                idx = index.value()
#                idx += rate.value()
#                index.set_value(idx)
#                row = d[idx]
#                for i in row:
#                    if sigs[i]:
#                        sigs[i].set_value(row[i])
#                dev.poll()
    def free(self):
        self.dev.free()

print('done')

if __name__=="__main__":
    argc = len(sys.argv)
    if argc > 1:
        filename = sys.argv[1]
        if argc > 2:
            player = Player(filename, int(sys.argv[2]))
        else:
            player = Player(filename, [])
        player.poll()
        player.free()
    else:
        print('dataset_player requires a dataset file')
