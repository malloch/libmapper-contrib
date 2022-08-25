from PySide6 import QtGui, QtWidgets, QtCore
import pyqtgraph as pg
import sys, math, time
from collections import deque
import libmapper as mpr

''' TODO
show network interfaces, allow setting
show metadata
'''

need_update = False

def on_event(type, obj, event):
    global need_update
    need_update = True

graph = mpr.Graph()
graph.add_callback(on_event, mpr.Type.DEVICE | mpr.Type.SIGNAL)

class Tree(QtWidgets.QTreeWidget):
    def __init__(self):
        super(Tree, self).__init__()
        self.setDragEnabled(True)

    def mouseMoveEvent(self, e):
        item = self.itemAt(e.pos())
        if item == None or item.parent() == None:
            return

        mimeData = QtCore.QMimeData()
        mimeData.setText('libmapper://' + item.parent().text(0) + '/' + item.text(0))
        drag = QtGui.QDrag(self)
        drag.setMimeData(mimeData)

        dropAction = drag.exec(QtCore.Qt.CopyAction)

class MainWindow(QtWidgets.QMainWindow):

    def __init__(self):
        super(MainWindow, self).__init__()

        self.setWindowTitle('libmapper signal browser')

        self.tree = Tree()
        self.tree.setHeaderLabels(['name', 'type', 'length', 'direction'])
        self.tree.setSortingEnabled(True)
        self.setCentralWidget(self.tree)

        self.timer = QtCore.QTimer()
        self.timer.setInterval(25)
        self.timer.timeout.connect(self.timer_event)
        self.timer.start()

    def timer_event(self):
        global need_update
        graph.poll(10)
        if need_update:
            self.update_tree()
            need_update = False

    def update_tree(self):
        # update list
        global graph
        self.tree.clear()
        for d in graph.devices():
            dev_item = QtWidgets.QTreeWidgetItem(self.tree, [d[mpr.Property.NAME]])
            for s in d.signals():
                s_item = QtWidgets.QTreeWidgetItem(dev_item)
                s_item.setText(0, s[mpr.Property.NAME])
                s_item.setText(1, s[mpr.Property.TYPE].name.lower())
                s_item.setText(2, "{}".format(s[mpr.Property.LENGTH]))
                s_item.setText(3, s[mpr.Property.DIRECTION].name.lower())

def remove_graph():
    global graph
    graph.free()

import atexit
atexit.register(remove_graph)

app = QtWidgets.QApplication(sys.argv)
main = MainWindow()
main.show()
app.exec()
