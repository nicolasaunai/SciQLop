from typing import List

from PySide6.QtCore import Signal
from PySide6.QtGui import QIcon
from PySide6.QtWidgets import QFrame, QScrollArea, QVBoxLayout, QWidget

from SciQLop.backend.pipelines_model.auto_register import auto_register
from SciQLop.backend.pipelines_model.base import PipelineModelItem
from SciQLop.backend.pipelines_model.base import model as pipelines_model
from SciQLop.backend.icons import register_icon

from .abstract_plot_panel import MetaPlotPanel, PlotPanel, PanelContainer
from ...backend import sciqlop_logging
from ...backend.unique_names import make_simple_incr_name

log = sciqlop_logging.getLogger(__name__)

from matplotlib.backends.backend_qtagg import (
    FigureCanvas, NavigationToolbar2QT as NavigationToolbar)
from matplotlib.figure import Figure


class MetaMPLPlot(type(QFrame), type(PipelineModelItem)):
    pass


class MPLFigure(QFrame, PipelineModelItem, metaclass=MetaMPLPlot):
    def __init__(self, *args, parent=None, **kwargs):
        QFrame.__init__(self, parent=parent)
        self.setObjectName(make_simple_incr_name(base="MPLPlot"))
        self._parent_node = None
        self.setLayout(QVBoxLayout())
        self._canvas = FigureCanvas(Figure())
        self.layout().addWidget(NavigationToolbar(self._canvas, self))
        self.layout().addWidget(self._canvas)
        self.setMinimumHeight(300)

    def select(self):
        self.setStyleSheet("border: 3px dashed blue;")

    def unselect(self):
        self.setStyleSheet("")

    @property
    def name(self) -> str:
        return self.objectName()

    @name.setter
    def name(self, new_name: str):
        with pipelines_model.model_update_ctx():
            self.setObjectName(new_name)

    @property
    def parent_node(self) -> 'PipelineModelItem':
        return self._parent_node

    @parent_node.setter
    def parent_node(self, parent: 'PipelineModelItem'):
        with pipelines_model.model_update_ctx():
            if self._parent_node is not None:
                self._parent_node.remove_children_node(self)
            self._parent_node = parent
            if parent is not None:
                parent.add_children_node(self)

    @property
    def children_nodes(self) -> List['PipelineModelItem']:
        return []

    def remove_children_node(self, node: 'PipelineModelItem'):
        pass

    def add_children_node(self, node: 'PipelineModelItem'):
        pass

    def delete_node(self):
        self.close()

    def __eq__(self, other: 'PipelineModelItem') -> bool:
        return self is other

    @property
    def mpl_figure(self):
        return self._canvas.figure

    def refresh(self):
        self.mpl_figure.canvas.draw()


register_icon("MPL", QIcon("://icons/MPL.png"))


@auto_register
class MPLPanel(QScrollArea, PlotPanel, metaclass=MetaPlotPanel):
    delete_me = Signal(object)

    def __init__(self, name: str, parent=None):
        QScrollArea.__init__(self, parent)
        self.setObjectName(name)
        self.setContentsMargins(0, 0, 0, 0)
        self._name = name
        self._plot_container = PanelContainer(plot_type=MPLFigure, parent=self)
        self.setWidget(self._plot_container)
        self.setWidgetResizable(True)
        self._parent_node = None

    @property
    def name(self):
        return self.objectName()

    @name.setter
    def name(self, new_name):
        with pipelines_model.model_update_ctx():
            self.setObjectName(new_name)

    @property
    def icon(self) -> str:
        return "MPL"

    def select(self):
        self.setStyleSheet("border: 3px dashed blue")

    def unselect(self):
        self.setStyleSheet("")

    def count(self) -> int:
        return self._plot_container.count()

    def indexOf(self, widget: QWidget):
        return self._plot_container.indexOf(widget)

    def index_of(self, child):
        return self._plot_container.indexOf(child)

    def child_at(self, row: int):
        if 0 <= row < len(self._plot_container.plots):
            return self._plot_container.plots[row]
        return None

    @property
    def children_nodes(self) -> List['PipelineModelItem']:
        return self._plot_container.plots

    def remove_children_node(self, node: 'PipelineModelItem'):
        self._plot_container.remove_plot(node)

    def add_children_node(self, node: 'PipelineModelItem'):
        pass

    def delete_node(self):
        self._plot_container.close()
        self.delete_me.emit(self)

    def new_figure(self, *args, index=-1, **kwargs) -> MPLFigure:
        fig = MPLFigure(parent=self, *args, **kwargs)
        self._plot_container.add_widget(widget=fig, index=index)
        return fig

    @property
    def figures(self) -> List[MPLFigure]:
        return self._plot_container.plots

    @property
    def parent_node(self) -> 'PipelineModelItem':
        return self._parent_node

    @parent_node.setter
    def parent_node(self, parent: 'PipelineModelItem'):
        with pipelines_model.model_update_ctx():
            if self._parent_node is not None:
                self._parent_node.remove_children_node(self)
            self._parent_node = parent
            if parent is not None:
                parent.add_children_node(self)

    def __del__(self):
        log.debug("deleting MPLPanel")

    def __eq__(self, other: 'PipelineModelItem') -> bool:
        return self is other

    def __getitem__(self, index: int) -> MPLFigure:
        plots: List[MPLFigure] = filter(lambda w: isinstance(w, MPLFigure), self._plot_container.plots)
        return list(plots)[index]
