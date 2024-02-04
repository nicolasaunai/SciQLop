from PySide6 import QtWidgets, QtPrintSupport, QtOpenGL, QtQml, QtCore, QtGui
from typing import Dict, List, Optional, Tuple, Union, Any
from qasync import QEventLoop, QApplication
import asyncio
import sys
from io import StringIO

_STYLE_SHEET_ = """
QWidget:focus { border: 1px dashed light blue }
"""


class SciQLopApp(QApplication):
    quickstart_shortcuts_added = QtCore.Signal(str)
    panels_list_changed = QtCore.Signal(list)

    def __init__(self, args):
        from . import sciqlop_logging
        super(SciQLopApp, self).__init__(args)
        self.setOrganizationName("LPP")
        self.setOrganizationDomain("lpp.fr")
        self.setApplicationName("SciQLop")
        self.setStyleSheet(_STYLE_SHEET_)
        # sciqlop_logging.setup()
        self._quickstart_shortcuts: Dict[str, Dict[str, Any]] = {}

    def add_quickstart_shortcut(self, name: str, description: str, icon: QtGui.QPixmap or QtGui.QIcon,
                                callback: callable):
        self._quickstart_shortcuts[name] = (
            {"name": name, "description": description, "icon": icon, "callback": callback})
        self.quickstart_shortcuts_added.emit(name)

    @QtCore.Property(list)
    def quickstart_shortcuts(self) -> List[str]:
        return list(self._quickstart_shortcuts.keys())

    def quickstart_shortcut(self, name: str) -> Optional[Dict[str, Any]]:
        return self._quickstart_shortcuts.get(name, None)


def sciqlop_app() -> SciQLopApp:
    if QtWidgets.QApplication.instance() is None:
        app = SciQLopApp(sys.argv)
    else:
        app = QtWidgets.QApplication.instance()
    return app


class _SciQLopEventLoop(QEventLoop):
    def __init__(self):
        super().__init__(sciqlop_app())
        asyncio.set_event_loop(self)
        app = sciqlop_app()
        app_close_event = asyncio.Event()
        app.aboutToQuit.connect(app_close_event.set)


_event_loop = None


def sciqlop_event_loop() -> _SciQLopEventLoop:
    global _event_loop
    if _event_loop is None:
        _event_loop = _SciQLopEventLoop()
    return _event_loop
