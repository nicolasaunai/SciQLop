from datetime import datetime

import tscat
from PySide6.QtCore import Slot, QObject
from PySide6.QtGui import QAction, QIcon
from PySide6.QtWidgets import QComboBox, QToolBar
from tscat_gui import TSCatGUI

from SciQLop.widgets.mainwindow import SciQLopMainWindow
from SciQLop.widgets.plots.time_sync_panel import TimeSyncPanel, TimeRange


def catalog_display_txt(catalog):
    if catalog is not None:
        return catalog.name
    return "None"


def index_of(catalogs, catalog):
    index = 0
    if catalog:
        for c in catalogs:
            if (c is not None) and (c.uuid == catalog.uuid):
                return index
            index += 1
    return 0


def zoom_out(start: datetime, stop: datetime, factor: float):
    delta = ((stop - start) / 2.) * factor
    return start - delta, stop + delta


def timestamps(start: datetime, stop: datetime):
    return start.timestamp(), stop.timestamp()


class CatalogSelector(QComboBox):
    def __init__(self, parent=None):
        super(CatalogSelector, self).__init__(parent)
        self.catalogs = [None]
        self.update_list()
        self.setSizeAdjustPolicy(QComboBox.SizeAdjustPolicy.AdjustToContents)

    def update_list(self):
        selected = self.catalogs[self.currentIndex()]
        self.catalogs = [None] + tscat.get_catalogues()
        self.clear()
        self.addItems(map(catalog_display_txt, self.catalogs))
        self.setCurrentIndex(index_of(self.catalogs, selected))


class PanelSelector(QComboBox):
    def __init__(self, parent=None):
        super(PanelSelector, self).__init__(parent)
        self.addItems(["None"])
        self.setSizeAdjustPolicy(QComboBox.SizeAdjustPolicy.AdjustToContents)

    def update_list(self, panels):
        selected = self.currentText()
        self.clear()
        self.addItems(["None"] + panels)
        self.setCurrentText(selected)


class CatalogGUISpawner(QAction):
    def __init__(self, catalog_gui, parent=None):
        super(CatalogGUISpawner, self).__init__(parent)
        self.catalog_gui = catalog_gui
        self.setIcon(QIcon("://icons/catalogue.png"))
        self.triggered.connect(self.show_catalogue_gui)

    def show_catalogue_gui(self):
        self.catalog_gui.show()


class Plugin(QObject):
    def __init__(self, main_window: SciQLopMainWindow):
        super(Plugin, self).__init__(main_window)
        self.ui = TSCatGUI()
        self.catalog_selector = CatalogSelector()
        self.panel_selector = PanelSelector()
        self.show_catalog = CatalogGUISpawner(self.ui)
        self.main_window = main_window
        self.last_event = None
        self.toolbar: QToolBar = main_window.addToolBar("Catalogs")

        self.toolbar.addAction(self.show_catalog)
        self.toolbar.addWidget(self.catalog_selector)
        self.toolbar.addWidget(self.panel_selector)

        main_window.central_widget.panels_list_changed.connect(self.panel_selector.update_list)

        self.ui.event_selected.connect(self.event_selected)

    @Slot()
    def event_selected(self, event):
        if self.panel_selector.currentText() != 'None':
            if self.last_event is not None:
                del self.last_event
            e = tscat.get_events(tscat.filtering.UUID(event))[0]
            print(e)
            time_range = TimeRange(*timestamps(*zoom_out(e.start, e.stop, 0.3)))
            print(time_range)
            if e:
                p: TimeSyncPanel = self.main_window.plot_panel(self.panel_selector.currentText())
                print(p)
                if p:
                    p.time_range = time_range
                    # self.last_event = EventTimeSpan(p, *timestamps(e.start, e.stop))
