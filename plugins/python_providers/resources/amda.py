import traceback
import os
from datetime import datetime, timedelta, timezone
import PythonProviders
import pysciqlopcore
import numpy as np
import pandas as pds
import requests
import copy
from spwc.amda import AMDA

amda = AMDA()

def get_sample(metadata,start,stop):
    ts_type = pysciqlopcore.ScalarTimeSerie
    default_ctor_args = 1
    try:
        param_id = None
        for key,value in metadata:
            if key == 'xml:id':
                param_id = value
            elif key == 'type':
                if value == 'vector':
                    ts_type = pysciqlopcore.VectorTimeSerie
                elif value == 'multicomponent':
                    ts_type = pysciqlopcore.MultiComponentTimeSerie
                    default_ctor_args = (0,2)
        tstart=datetime.fromtimestamp(start, tz=timezone.utc)
        tend=datetime.fromtimestamp(stop, tz=timezone.utc)
        df = amda.get_parameter(start_time=tstart, stop_time=tend, parameter_id=param_id, method="REST")
        t = np.array([d.timestamp() for d in df.index])
        values = df.values
        return ts_type(t,values)
    except Exception as e:
        print(traceback.format_exc())
        print("Error in amda.py ",str(e))
        return ts_type(default_ctor_args)


if len(amda.component) is 0:
    amda.update_inventory()
parameters = copy.deepcopy(amda.parameter)
for name,component in amda.component.items():
    if 'components' in parameters[component['parameter']]:
        parameters[component['parameter']]['components'].append(component)
    else:
        parameters[component['parameter']]['components']=[component]

products = []
for key,parameter in parameters.items():
    path = f"/AMDA/{parameter['mission']}/{parameter.get('observatory','')}/{parameter['instrument']}/{parameter['dataset']}/{parameter['name']}"
    components = [component['name'] for component in parameter.get('components',[])]
    metadata = [ (key,item) for key,item in parameter.items() if key is not 'components' ]
    n_components = parameter.get('size',0)
    if n_components is '3':
        metadata.append(("type","vector"))
    elif n_components !=0:
        metadata.append(("type","multicomponent"))
    else:
        metadata.append(("type","scalar"))
    products.append( (path, components, metadata))

PythonProviders.register_product(products, get_sample)


