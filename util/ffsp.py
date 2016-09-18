import os
import sys
import json
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

class MainWindow(QtGui.QMainWindow):
    def __init__(self):
        super().__init__()
        self.ui = PyQt4.uic.loadUi(r"ffsp.ui", self)

        self.mountIndicatorLbl = QtGui.QLabel()
        self.statusBar().addPermanentWidget(self.mountIndicatorLbl)

        self.updateControlsState()

        self.ui.createFsFileBtn.clicked.connect(self.createFsFile)
        self.ui.mkFsBtn.clicked.connect(self.mkfs)
        self.ui.mountFsBtn.clicked.connect(self.mount)
        self.ui.unmountFsBtn.clicked.connect(self.unmount)
        self.ui.reloadBtn.clicked.connect(self.reload)

        self.show()

    def updateControlsState(self):
        self.debugFilePath = os.path.join(os.path.expanduser(self.ui.mountpointPathEdit.text()), ".FFSP")

        fsFileExists = os.path.exists(os.path.expanduser(self.ui.fsPathEdit.text()))
        fsMounted = os.path.exists(self.debugFilePath)

        self.ui.createFsFileBtn.setEnabled(not fsMounted)
        self.ui.mkFsBtn.setEnabled(fsFileExists and not fsMounted)
        self.ui.mountFsBtn.setEnabled(fsFileExists and not fsMounted)
        self.ui.unmountFsBtn.setEnabled(fsMounted)
        self.ui.reloadBtn.setEnabled(fsMounted)

        self.mountIndicatorLbl.setText("/" if fsMounted else "X")

    def createFsFile(self):
        path = os.path.expanduser(self.fsPathEdit.text())
        size = self.ui.fsFileSizeSpinBox.value() * str2multiplicator(self.fsFileSizeSpinBox.suffix())
        rc = subprocess.call("dd if=/dev/zero of={} bs={} count=1".format(path, size).split())
        self.statusBar().showMessage("dd returned {}".format(rc))

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
        cmd.append(fsPath)
        cmd.append(moutPointPath)
        subprocess.call(cmd)

        self.updateControlsState()
        self.reload()

    def unmount(self):
        moutPointPath = os.path.expanduser(self.mountpointPathEdit.text())
        rc = subprocess.call(["fusermount", "-u", moutPointPath])
        self.statusBar().showMessage("fusermount -u returned {}".format(rc))
        self.updateControlsState()

    def reload(self):
        with open(self.debugFilePath) as f:
            self.fsinfo = json.load(f, object_pairs_hook=collections.OrderedDict)

        self.ui.superBlockTable.setItem(0, 0, QtGui.QTableWidgetItem("fd"))
        self.ui.superBlockTable.setItem(0, 1, QtGui.QTableWidgetItem(str(self.fsinfo["fd"])))
        self.ui.superBlockTable.setItem(1, 0, QtGui.QTableWidgetItem("fsid"))
        self.ui.superBlockTable.setItem(1, 1, QtGui.QTableWidgetItem((self.fsinfo["fsid"]).to_bytes(4, byteorder="big").decode("utf-8")))
        self.ui.superBlockTable.setItem(2, 0, QtGui.QTableWidgetItem("flags"))
        self.ui.superBlockTable.setItem(2, 1, QtGui.QTableWidgetItem(str(self.fsinfo["flags"])))
        self.ui.superBlockTable.setItem(3, 0, QtGui.QTableWidgetItem("neraseblocks"))
        self.ui.superBlockTable.setItem(3, 1, QtGui.QTableWidgetItem(str(self.fsinfo["neraseblocks"])))
        self.ui.superBlockTable.setItem(4, 0, QtGui.QTableWidgetItem("nino"))
        self.ui.superBlockTable.setItem(4, 1, QtGui.QTableWidgetItem(str(self.fsinfo["nino"])))
        self.ui.superBlockTable.setItem(5, 0, QtGui.QTableWidgetItem("blocksize"))
        self.ui.superBlockTable.setItem(5, 1, QtGui.QTableWidgetItem(str(self.fsinfo["blocksize"])))
        self.ui.superBlockTable.setItem(6, 0, QtGui.QTableWidgetItem("clustersize"))
        self.ui.superBlockTable.setItem(6, 1, QtGui.QTableWidgetItem(str(self.fsinfo["clustersize"])))
        self.ui.superBlockTable.setItem(7, 0, QtGui.QTableWidgetItem("erasesize"))
        self.ui.superBlockTable.setItem(7, 1, QtGui.QTableWidgetItem(str(self.fsinfo["erasesize"])))
        self.ui.superBlockTable.setItem(8, 0, QtGui.QTableWidgetItem("ninoopen"))
        self.ui.superBlockTable.setItem(8, 1, QtGui.QTableWidgetItem(str(self.fsinfo["ninoopen"])))
        self.ui.superBlockTable.setItem(9, 0, QtGui.QTableWidgetItem("neraseopen"))
        self.ui.superBlockTable.setItem(9, 1, QtGui.QTableWidgetItem(str(self.fsinfo["neraseopen"])))
        self.ui.superBlockTable.setItem(10, 0, QtGui.QTableWidgetItem("nerasereserve"))
        self.ui.superBlockTable.setItem(10, 1, QtGui.QTableWidgetItem(str(self.fsinfo["nerasereserve"])))
        self.ui.superBlockTable.setItem(11, 0, QtGui.QTableWidgetItem("nerasewrites"))
        self.ui.superBlockTable.setItem(11, 1, QtGui.QTableWidgetItem(str(self.fsinfo["nerasewrites"])))

        self.ui.metricsTable.setItem(0, 0, QtGui.QTableWidgetItem("Read Raw"))
        self.ui.metricsTable.setItem(0, 1, QtGui.QTableWidgetItem(str(self.fsinfo["debug_info"]["read_raw"])))
        self.ui.metricsTable.setItem(1, 0, QtGui.QTableWidgetItem("Write Raw"))
        self.ui.metricsTable.setItem(1, 1, QtGui.QTableWidgetItem(str(self.fsinfo["debug_info"]["write_raw"])))
        self.ui.metricsTable.setItem(2, 0, QtGui.QTableWidgetItem("FUSE Read"))
        self.ui.metricsTable.setItem(2, 1, QtGui.QTableWidgetItem(str(self.fsinfo["debug_info"]["fuse_read"])))
        self.ui.metricsTable.setItem(3, 0, QtGui.QTableWidgetItem("FUSE Write"))
        self.ui.metricsTable.setItem(3, 1, QtGui.QTableWidgetItem(str(self.fsinfo["debug_info"]["fuse_write"])))
        self.ui.metricsTable.setItem(4, 0, QtGui.QTableWidgetItem("GC Read"))
        self.ui.metricsTable.setItem(4, 1, QtGui.QTableWidgetItem(str(self.fsinfo["debug_info"]["gc_read"])))
        self.ui.metricsTable.setItem(5, 0, QtGui.QTableWidgetItem("GC Write"))
        self.ui.metricsTable.setItem(5, 1, QtGui.QTableWidgetItem(str(self.fsinfo["debug_info"]["gc_write"])))
        self.ui.metricsTable.setItem(6, 0, QtGui.QTableWidgetItem("Errors"))
        self.ui.metricsTable.setItem(6, 1, QtGui.QTableWidgetItem(str(self.fsinfo["debug_info"]["errors"])))

        eb2str = { 0x00:"super",
                   0x01:"dentry_inode",
                   0x02:"dentry_clin",
                   0x04:"file_inode",
                   0x08:"file_clin",
                   0x10:"ebin",
                   0x20:"empty" }
        eb2color = { 0x00:(QtCore.Qt.black, QtCore.Qt.yellow),
                     0x01:(QtCore.Qt.white, QtCore.Qt.blue),
                     0x02:(QtCore.Qt.white, QtCore.Qt.darkBlue),
                     0x04:(QtCore.Qt.black, QtCore.Qt.lightGray),
                     0x08:(QtCore.Qt.white, QtCore.Qt.darkGray),
                     0x10:(QtCore.Qt.white, QtCore.Qt.red),
                     0x20:(QtCore.Qt.white, QtCore.Qt.darkGreen) }
        for i, eb in enumerate(self.fsinfo["eb_usage"]):
            fg, bg = eb2color[eb["type"]]

            item = QtGui.QTableWidgetItem(eb2str[eb["type"]])
            item.setForeground(fg)
            item.setBackground(bg)
            self.ui.eraseblockUsageTable.setItem(i, 0, item)

            item = QtGui.QTableWidgetItem(str(eb["lastwrite"]))
            item.setForeground(fg)
            item.setBackground(bg)
            self.ui.eraseblockUsageTable.setItem(i, 1, item)

            item = QtGui.QTableWidgetItem(str(eb["cvalid"]))
            item.setForeground(fg)
            item.setBackground(bg)
            self.ui.eraseblockUsageTable.setItem(i, 2, item)

            item = QtGui.QTableWidgetItem(str(eb["writeops"]))
            item.setForeground(fg)
            item.setBackground(bg)
            self.ui.eraseblockUsageTable.setItem(i, 3, item)

        self.ui.jsonEdit.setPlainText(json.dumps(self.fsinfo, indent=2))

if __name__ == "__main__":
    app = QtGui.QApplication(sys.argv)
    #app.aboutToQuit.connect(app.deleteLater)

    wnd = MainWindow()
    wnd.show()

    sys.exit(app.exec_())
