from typing import Tuple, Optional
from SciQLop.backend.enums import DataOrder
from speasy.products import SpeasyVariable

providers = {}


class DataProvider:
    def __init__(self, name: str, data_order: DataOrder = DataOrder.X_FIRST):
        global providers
        providers[name] = self
        self._name = name
        self._data_order = data_order

    def __del__(self):
        global providers
        if providers is not None:
            providers.pop(self._name)

    @property
    def name(self) -> str:
        return self._name

    @property
    def data_order(self) -> DataOrder:
        return self._data_order

    def get_data(self, node, start, stop) -> Optional[SpeasyVariable]:
        pass
