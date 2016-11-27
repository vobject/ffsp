import os
import sys
import stat
import json
import datetime
import collections
import subprocess
from PyQt4 import QtGui, QtCore
import PyQt4.uic

def str2multiplicator(s):
    s = s.strip()
    if s == "GiB": return 1024 * 1024 * 1024
    elif s == "MiB": return 1024 * 1024
    elif s == "KiB": return 1024
    else: return 1


class InodesWidget():
    inode_data_flags = {
        0x01 : "DATA_EMB",
        0x02 : "DATA_CLIN",
        0x04 : "DATA_EBIN",
    }

    def __init__(self, ino_tbl):
        super().__init__()

        self.w = ino_tbl

    def reload(self, debug_dir, cl_id):
        print("InodesWidget.reload=" + debug_dir + " cl=" + str(cl_id))

        self.debug_dir = debug_dir
        self.cl_id = cl_id
        self.clear()

        try: self.w.currentItemChanged.disconnect(self.inoItemChanged)
        except TypeError: pass

        with open(os.path.join(debug_dir, "clusters.d", str(cl_id))) as f:
            cl_json = json.load(f, object_pairs_hook=collections.OrderedDict)
#                print(str(cl_json))

        inodes = cl_json["cluster"]["inodes"] if "inodes" in cl_json["cluster"] else []
        if not inodes:
            return
        ninodes = len(inodes)
        ncolumns = 11

        self.w.setColumnCount(ncolumns)
        self.w.setRowCount(ninodes)
        self.w.setHorizontalHeaderLabels(["Inode", "Size", "Flags", "Links", "Mode", "UID", "GID", "rdev", "atime", "ctime", "mtime"])

        for i, ino_id in enumerate(inodes):
            with open(os.path.join(debug_dir, "inodes.d", str(ino_id))) as f:
                ino_json = json.load(f, object_pairs_hook=collections.OrderedDict)

            ino = ino_json["inode"]

            self.w.setItem(i, 0, self.createItem(ino_id, str(ino["no"])))
            self.w.setItem(i, 1, self.createItem(ino_id, str(ino["size"])))
            self.w.setItem(i, 2, self.createItem(ino_id, self.flags2str(ino["flags"])))
            self.w.setItem(i, 3, self.createItem(ino_id, str(ino["nlink"])))
            self.w.setItem(i, 4, self.createItem(ino_id, self.mode2str(ino["mode"])))
            self.w.setItem(i, 5, self.createItem(ino_id, str(ino["uid"])))
            self.w.setItem(i, 6, self.createItem(ino_id, str(ino["gid"])))
            self.w.setItem(i, 7, self.createItem(ino_id, str(ino["rdev"])))
            self.w.setItem(i, 8, self.createItem(ino_id, self.time2str(ino["atime"])))
            self.w.setItem(i, 9, self.createItem(ino_id, self.time2str(ino["ctime"])))
            self.w.setItem(i, 10, self.createItem(ino_id, self.time2str(ino["mtime"])))

        self.w.currentItemChanged.connect(self.inoItemChanged)

        self.w.resizeColumnsToContents()
        self.w.resizeRowsToContents()

    def clear(self):
        self.w.setRowCount(0)
        self.w.setHorizontalHeaderLabels([])

    def inoItemChanged(self, currentItem, previousItem):
        if currentItem:
            print("inoItemChanged: ino_id={}".format(str(currentItem.ino_id)))
        else:
            print("inoItemChanged: None")

    def createItem(self, ino_id, txt):
            item = QtGui.QTableWidgetItem(txt)
            item.ino_id = ino_id
            return item

    def flags2str(self, flags):
        return InodesWidget.inode_data_flags[flags]

    def mode2str(self, mode):
        return stat.filemode(mode)

    def time2str(self, t):
        return datetime.datetime.fromtimestamp(t).strftime('%Y-%m-%d %H:%M:%S')


class ClustersWidget():
    def __init__(self, cl_tbl, ino_tbl):
        super().__init__()

        self.w = cl_tbl
        self.iw = InodesWidget(ino_tbl)

    def reload(self, debug_dir, eb_id):
        print("ClustersWidget.reload=" + debug_dir + " eb=" + str(eb_id))

        self.debug_dir = debug_dir
        self.eb_id = eb_id

        self.iw.clear()
        self.w.clear()

        try: self.w.currentItemChanged.disconnect(self.clItemChanged)
        except TypeError: pass

        with open(os.path.join(debug_dir, "eraseblocks.d", str(eb_id))) as f:
            eb_json = json.load(f, object_pairs_hook=collections.OrderedDict)
#            print(str(eb_json))

        nclusters = len(eb_json["clusters"])
        ncl_per_col = 16
        ncl_per_row = int(nclusters / ncl_per_col)

        self.w.setColumnCount(ncl_per_col)
        self.w.setRowCount(ncl_per_row)

        for i, cl_id in enumerate(eb_json["clusters"]):
            with open(os.path.join(debug_dir, "clusters.d", str(cl_id))) as f:
                cl_json = json.load(f, object_pairs_hook=collections.OrderedDict)

            x = int(i / ncl_per_col)
            y = int(i % ncl_per_col)

            cl = cl_json["cluster"]
            inodes = cl["inodes"] if "inodes" in cl else []

            cl_id = cl["cl_id"]
            cl_offset = cl["cl_offset"]

            item_str = "{}".format(cl_id)
            item_tt = "cl_id: {}\nOffset: {}\nInodes: {}".format(cl_id, cl_offset, len(inodes))

            item = QtGui.QTableWidgetItem(item_str)
            item.setTextAlignment(QtCore.Qt.AlignCenter)
            item.setToolTip(item_tt)
            item.setBackground(QtGui.QColor(0, 255, 0, len(inodes)*4))
            item.cl_id = cl_id
            self.w.setItem(x, y, item)

        self.w.currentItemChanged.connect(self.clItemChanged)

        self.w.resizeColumnsToContents()
        self.w.resizeRowsToContents()

    def clear(self):
        self.iw.clear()
        self.w.setRowCount(0)

    def clItemChanged(self, currentItem, previousItem):
        if currentItem:
            print("clItemChanged: cl_id={}".format(str(currentItem.cl_id)))
            self.iw.reload(self.debug_dir, currentItem.cl_id)
        else:
            print("clItemChanged: None")


class EraseblocksWidget():
    def __init__(self, eb_tbl, cl_tbl, ino_tbl):
        super().__init__()

        self.w = eb_tbl
        self.cw = ClustersWidget(cl_tbl, ino_tbl)

    def reload(self, debug_dir):
        print("EraseblocksWidget.reload=" + debug_dir)

        eb2str = { 0x00:"super",
                   0x01:"dentry_inode",
                   0x02:"dentry_clin",
                   0x04:"file_inode",
                   0x08:"file_clin",
                   0x10:"ebin",
                   0x20:"empty" }
        eb2col = { 0x00:(QtCore.Qt.black, QtCore.Qt.yellow),
                   0x01:(QtCore.Qt.white, QtCore.Qt.blue),
                   0x02:(QtCore.Qt.white, QtCore.Qt.darkBlue),
                   0x04:(QtCore.Qt.black, QtCore.Qt.lightGray),
                   0x08:(QtCore.Qt.white, QtCore.Qt.darkGray),
                   0x10:(QtCore.Qt.white, QtCore.Qt.red),
                   0x20:(QtCore.Qt.white, QtCore.Qt.darkGreen) }

        self.debug_dir = debug_dir

        self.cw.clear()
        self.w.clear()

        try: self.w.currentItemChanged.disconnect(self.ebItemChanged)
        except TypeError: pass

        with open(os.path.join(debug_dir, "super")) as f:
            super_json = json.load(f, object_pairs_hook=collections.OrderedDict)
#            print(str(super_json))

        neraseblocks = len(super_json["eraseblocks"])
        neb_per_col = 16
        neb_per_row = int(neraseblocks / neb_per_col)

        self.w.setColumnCount(neb_per_col)
        self.w.setRowCount(neb_per_row)

        for i, eb_id in enumerate(super_json["eraseblocks"]):
            with open(os.path.join(debug_dir, "eraseblocks.d", str(eb_id))) as f:
                eb_json = json.load(f, object_pairs_hook=collections.OrderedDict)
#                print(str(eb_json))

            x = int(i / neb_per_col)
            y = int(i % neb_per_col)

            eb = eb_json["eraseblock"]
            clusters = eb_json["clusters"]

            eb_id = eb["eb_id"]
            eb_type = eb["type"]
            eb_cvalid = eb["cvalid"]
            eb_writeops = eb["writeops"]
            item_str = "{}".format(eb_id)
            item_tt = "eb_id: {}\ntype: {}\nValid clusters: {}\nWrite ops: {}\nClusters: {}{}".format(eb_id, eb2str[eb["type"]], eb_cvalid, eb_writeops, len(clusters), " ({}-{})".format(clusters[0], clusters[-1]) if clusters else "")

            item = QtGui.QTableWidgetItem(item_str)
            item.setTextAlignment(QtCore.Qt.AlignCenter)
            item.setToolTip(item_tt)
            fg, bg = eb2col[eb_type]
            item.setForeground(fg)
            item.setBackground(bg)
            item.eb_id = eb_id
            self.w.setItem(x, y, item)

        self.w.currentItemChanged.connect(self.ebItemChanged)

        self.w.resizeColumnsToContents()
        self.w.resizeRowsToContents()

    def ebItemChanged(self, currentItem, previousItem):
        if currentItem:
            print("ebItemChanged: eb_id={}".format(str(currentItem.eb_id)))
            self.cw.reload(self.debug_dir, currentItem.eb_id)
        else:
            print("ebItemChanged: None")


class SuperblocksWidget():
    def __init__(self, super_tbl, metrics_tbl):
        super().__init__()

        self.w = super_tbl
        self.m = metrics_tbl

    def reload(self, debug_dir):
        print("SuperblocksWidget.reload=" + debug_dir)

        self.debug_dir = debug_dir

        self.w.clear()

        with open(os.path.join(debug_dir, "super")) as f:
            super_json = json.load(f, object_pairs_hook=collections.OrderedDict)
#            print(str(super_json))

        self.w.setColumnCount(2)
        self.w.setRowCount(11)

        sb = super_json["super"]
        self.w.setItem(0, 0, QtGui.QTableWidgetItem("fsid"))
        self.w.setItem(0, 1, QtGui.QTableWidgetItem((sb["fsid"]).to_bytes(4, byteorder="big").decode("utf-8")))
        self.w.setItem(1, 0, QtGui.QTableWidgetItem("flags"))
        self.w.setItem(1, 1, QtGui.QTableWidgetItem(str(sb["flags"])))
        self.w.setItem(2, 0, QtGui.QTableWidgetItem("neraseblocks"))
        self.w.setItem(2, 1, QtGui.QTableWidgetItem(str(sb["neraseblocks"])))
        self.w.setItem(3, 0, QtGui.QTableWidgetItem("nino"))
        self.w.setItem(3, 1, QtGui.QTableWidgetItem(str(sb["nino"])))
        self.w.setItem(4, 0, QtGui.QTableWidgetItem("blocksize"))
        self.w.setItem(4, 1, QtGui.QTableWidgetItem(str(sb["blocksize"])))
        self.w.setItem(5, 0, QtGui.QTableWidgetItem("clustersize"))
        self.w.setItem(5, 1, QtGui.QTableWidgetItem(str(sb["clustersize"])))
        self.w.setItem(6, 0, QtGui.QTableWidgetItem("erasesize"))
        self.w.setItem(6, 1, QtGui.QTableWidgetItem(str(sb["erasesize"])))
        self.w.setItem(7, 0, QtGui.QTableWidgetItem("ninoopen"))
        self.w.setItem(7, 1, QtGui.QTableWidgetItem(str(sb["ninoopen"])))
        self.w.setItem(8, 0, QtGui.QTableWidgetItem("neraseopen"))
        self.w.setItem(8, 1, QtGui.QTableWidgetItem(str(sb["neraseopen"])))
        self.w.setItem(9, 0, QtGui.QTableWidgetItem("nerasereserve"))
        self.w.setItem(9, 1, QtGui.QTableWidgetItem(str(sb["nerasereserve"])))
        self.w.setItem(10, 0, QtGui.QTableWidgetItem("nerasewrites"))
        self.w.setItem(10, 1, QtGui.QTableWidgetItem(str(sb["nerasewrites"])))

        self.m.clear()

        with open(os.path.join(debug_dir, "metrics")) as f:
            metrics_json = json.load(f, object_pairs_hook=collections.OrderedDict)
#            print(str(metrics_json))

        self.m.setColumnCount(2)
        self.m.setRowCount(6)

        di = metrics_json["debuginfo"]
        self.m.setItem(0, 0, QtGui.QTableWidgetItem("Read Raw"))
        self.m.setItem(0, 1, QtGui.QTableWidgetItem(str(di["read_raw"])))
        self.m.setItem(1, 0, QtGui.QTableWidgetItem("Write Raw"))
        self.m.setItem(1, 1, QtGui.QTableWidgetItem(str(di["write_raw"])))
        self.m.setItem(2, 0, QtGui.QTableWidgetItem("FUSE Read"))
        self.m.setItem(2, 1, QtGui.QTableWidgetItem(str(di["fuse_read"])))
        self.m.setItem(3, 0, QtGui.QTableWidgetItem("FUSE Write"))
        self.m.setItem(3, 1, QtGui.QTableWidgetItem(str(di["fuse_write"])))
        self.m.setItem(4, 0, QtGui.QTableWidgetItem("GC Read"))
        self.m.setItem(4, 1, QtGui.QTableWidgetItem(str(di["gc_read"])))
        self.m.setItem(5, 0, QtGui.QTableWidgetItem("GC Write"))
        self.m.setItem(5, 1, QtGui.QTableWidgetItem(str(di["gc_write"])))


class MainWindow(QtGui.QMainWindow):
    def __init__(self):
        super().__init__()
        self.ui = PyQt4.uic.loadUi(r"ffsp.ui", self)

        self.mountIndicatorLbl = QtGui.QLabel()
        self.statusBar().addPermanentWidget(self.mountIndicatorLbl)

        self.initControlsState()
        self.updateControlsState()

        self.ui.createFsFileBtn.clicked.connect(self.createFsFile)
        self.ui.removeFsFileBtn.clicked.connect(self.removeFsFile)
        self.ui.mkFsBtn.clicked.connect(self.mkfs)
        self.ui.mountFsBtn.clicked.connect(self.mount)
        self.ui.unmountFsBtn.clicked.connect(self.unmount)
        self.ui.reloadBtn.clicked.connect(self.reload)

        self.eb_tbl = EraseblocksWidget(self.ui.ebTable, self.ui.clTable, self.ui.inoTable)
        self.super_tbl = SuperblocksWidget(self.ui.sbTable, self.ui.metricsTable)

        self.show()

    def initControlsState(self):
        if sys.platform == "linux":
            self.fsPathEdit.setText(r"~/Development/ffsp-build/Default/fs")
            self.mountpointPathEdit.setText(r"~/Development/ffsp-build/Default/mnt")
            self.mkfsPathEdit.setText(r"~/Development/ffsp-build/Default/mkfs.ffsp")
            self.mountPathEdit.setText(r"~/Development/ffsp-build/Default/mount.ffsp")
        elif sys.platform == "win32":
            self.fsPathEdit.setText(r"D:\Development\fs\ffsp-build\fs")
            self.mountpointPathEdit.setText(r"D:\Development\fs\ffsp-build\mnt")
            self.mkfsPathEdit.setText(r"D:\Development\fs\ffsp-build\Debug\mkfs.ffsp.exe")
            self.mountPathEdit.setText(r"D:\Development\fs\ffsp-build\Debug\mount.ffsp.exe")

    def updateControlsState(self):
        self.debug_dir = os.path.join(os.path.expanduser(self.ui.mountpointPathEdit.text()), ".FFSP.d")

        fsFileExists = os.path.exists(os.path.expanduser(self.ui.fsPathEdit.text()))
        fsMounted = os.path.exists(self.debug_dir)

        self.ui.createFsFileBtn.setEnabled(not fsMounted)
        self.ui.removeFsFileBtn.setEnabled(fsFileExists)
        self.ui.mkFsBtn.setEnabled(fsFileExists and not fsMounted)
        self.ui.mountFsBtn.setEnabled(fsFileExists and not fsMounted)
        self.ui.unmountFsBtn.setEnabled(fsMounted)
        self.ui.reloadBtn.setEnabled(fsMounted)

        self.mountIndicatorLbl.setText("/" if fsMounted else "X")

    def createFsFile(self):
        path = os.path.expanduser(self.fsPathEdit.text())
        size = self.ui.fsFileSizeSpinBox.value()
        mul = str2multiplicator(self.fsFileSizeSpinBox.suffix())
        with open(path, "wb") as fs:
            for i in range(0, size):
                self.statusBar().showMessage("Creating file: {}/{}".format(i * mul, size * mul))
                fs.write(b'\0' * mul)
        self.updateControlsState()
        self.statusBar().showMessage("File system container successfully created")

    def removeFsFile(self):
        path = os.path.expanduser(self.fsPathEdit.text())
        if os.path.exists(path):
            os.remove(path)
        self.updateControlsState()
        self.statusBar().showMessage("File system container successfully removed")

    def mkfs(self):
        mkfsPath = os.path.expanduser(self.mkfsPathEdit.text())
        if not os.path.exists(mkfsPath):
            self.statusBar().showMessage("Invalid path for mkfs.ffsp")
            return

        fsPath = os.path.expanduser(self.fsPathEdit.text())
        if not os.path.exists(fsPath):
            self.statusBar().showMessage("Invalid file system container path")
            return

        clustersize = self.ui.clusterSizeSpinBox.value() * str2multiplicator(self.clusterSizeSpinBox.suffix())
        erasesize = self.ui.eraseblockSizeSpinBox.value() * str2multiplicator(self.eraseblockSizeSpinBox.suffix())
        ninoopen = self.ui.openInodesSpinBox.value()
        neraseopen = self.ui.openEraseblocksSpinBox.value()
        nerasereserve = self.ui.reservedEraseblocksSpinBox.value()
        nerasewrites = self.ui.gcTriggerSpinBox.value()

        rc = subprocess.call("{} --clustersize={} --erasesize={} --open-ino={} --open-eb={} --reserve-eb={} --write-eb={} {}".format(mkfsPath, clustersize, erasesize, ninoopen, neraseopen, nerasereserve, nerasewrites, fsPath).split())
        self.statusBar().showMessage("mkfs returned {}".format(rc))

    def mount(self):
        mountPath = os.path.expanduser(self.mountPathEdit.text())
        if not os.path.exists(mountPath):
            self.statusBar().showMessage("Invalid path for mount.ffsp")
            return

        fsPath = os.path.expanduser(self.fsPathEdit.text())
        if not os.path.exists(fsPath):
            self.statusBar().showMessage("Invalid file system container path")
            return

        moutPointPath = os.path.expanduser(self.mountpointPathEdit.text())
        if not os.path.isdir(moutPointPath):
            self.statusBar().showMessage("Invalid mount point directory")
            return

        cmd = [mountPath]
        if self.ui.debugModeCheckBox.isChecked():
            cmd.append("-d")
        if self.ui.singleThreadModeCheckBox.isChecked():
            cmd.append("-s")
        cmd.append("--logfile=ffsp_{}.log".format(datetime.datetime.now().strftime("%Y%m%dT%H%M%S")))
        cmd.append("-vvvv")
        cmd.append(fsPath)
        cmd.append(moutPointPath)
        subprocess.call(cmd)

        self.updateControlsState()
        self.reload()
        self.ui.tabWidget.setCurrentIndex(self.ui.tabWidget.currentIndex() + 1)

    def unmount(self):
        moutPointPath = os.path.expanduser(self.mountpointPathEdit.text())
        rc = subprocess.call(["fusermount", "-u", moutPointPath])
        self.statusBar().showMessage("fusermount -u returned {}".format(rc))
        self.updateControlsState()

    def reload(self):
        self.super_tbl.reload(self.debug_dir)
        self.eb_tbl.reload(self.debug_dir)

if __name__ == "__main__":
    app = QtGui.QApplication(sys.argv)
    #app.aboutToQuit.connect(app.deleteLater)

    wnd = MainWindow()
    wnd.show()

    sys.exit(app.exec_())
