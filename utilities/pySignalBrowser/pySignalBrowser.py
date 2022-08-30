from PySide6 import QtGui, QtWidgets, QtCore
import pyqtgraph as pg
import sys, math, time
from collections import deque
import libmapper as mpr

''' TODO
show network interfaces, allow setting
show metadata
'''

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

        self.graph = mpr.Graph(mpr.Type.DEVICE | mpr.Type.SIGNAL)
        self.graph.add_callback(self.on_event, mpr.Type.DEVICE | mpr.Type.SIGNAL)

        self.setWindowTitle('libmapper signal browser')

        self.setGeometry(300, 300, 375, 300)

        self.tree = Tree()
        self.tree.setHeaderLabels(['name', 'id', 'type', 'length', 'direction'])
        self.tree.setColumnHidden(1, True)
        self.tree.setColumnWidth(0, 200)
        self.tree.setColumnWidth(1, 60)
        self.tree.setColumnWidth(2, 50)
        self.tree.setColumnWidth(3, 50)
        self.tree.setSortingEnabled(True)
        self.setCentralWidget(self.tree)

        self.timer = QtCore.QTimer()
        self.timer.setInterval(25)
        self.timer.timeout.connect(self.timer_event)
        self.timer.start()

    def on_event(self, type, obj, event):
        if type == mpr.Type.DEVICE:
            if event == mpr.Graph.Event.NEW:
                dev_item = QtWidgets.QTreeWidgetItem(self.tree, [obj[mpr.Property.NAME]])
                dev_item.setText(1, str(obj[mpr.Property.ID]))
                dev_item.setExpanded(True)
            elif event == mpr.Graph.Event.REMOVED or event == mpr.Graph.Event.EXPIRED:
                items = self.tree.findItems(str(obj[mpr.Property.ID]), QtCore.Qt.MatchFixedString, 1)
                for item in items:
                    item = self.tree.takeTopLevelItem(int(self.tree.indexOfTopLevelItem(item)))
                    del item
        elif type == mpr.Type.SIGNAL:
            if event == mpr.Graph.Event.NEW:
                # find parent device
                dev_items = self.tree.findItems(str(obj.device()[mpr.Property.ID]), QtCore.Qt.MatchFixedString, 1)
                for dev_item in dev_items:
                    sig_item = QtWidgets.QTreeWidgetItem(dev_item, [obj[mpr.Property.NAME], str(obj[mpr.Property.ID]), obj[mpr.Property.TYPE].name.lower(), "{}".format(obj[mpr.Property.LENGTH]), "out" if (obj[mpr.Property.DIRECTION] == mpr.Direction.OUTGOING) else "in"])
            elif event == mpr.Graph.Event.REMOVED or event == mpr.Graph.Event.EXPIRED:
                sig_items = self.tree.findItems(str(obj[mpr.Property.ID]), QtCore.Qt.MatchFixedString | QtCore.Qt.MatchRecursive, 1)
                for sig_item in sig_items:
                    parent = sig_item.parent()
                    item = parent.takeChild(int(parent.indexOfChild(sig_item)))
                    del item

    def timer_event(self):
        self.graph.poll(10)

    def remove_graph(self):
        self.graph.free()

app = QtWidgets.QApplication(sys.argv)
main = MainWindow()

import atexit
atexit.register(main.remove_graph)

main.show()
app.exec()
