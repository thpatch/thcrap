#!/usr/bin/env python3

# Touhou Community Reliant Automatic Patcher
# Scripts
#
# ----
#
"""Utility functions shared among all the scripts."""

from collections import OrderedDict
try:
    import json5 as jsonloader
except:
    import json as jsonloader
import json
import os

json_load_params = {
    'object_pairs_hook': OrderedDict
}


json_dump_params = {
    'ensure_ascii': False,
    'indent': '\t',
    'separators': (',', ': '),
    'sort_keys': True
}


# Default parameters for JSON input and output
def json_load(fn, json_kwargs=json_load_params):
    with open(fn, 'r', encoding='utf-8') as file:
        return jsonloader.load(file, **json_kwargs)


def json_store(fn, obj, dirs=[''], json_kwargs=json_dump_params):
    """Saves the JSON object [obj] to [fn], creating all necessary
    directories in the process. If [dirs] is given, the function is
    executed for every root directory in the array."""
    for i in dirs:
        full_fn = os.path.join(i, fn)
        dir = os.path.dirname(full_fn)
        if dir.strip():
            os.makedirs(dir, exist_ok=True)
        with open(full_fn, 'w', newline='\n', encoding='utf-8') as file:
            json.dump(obj, file, **json_kwargs)
            file.write('\n')
