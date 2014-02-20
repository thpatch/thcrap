#!/usr/bin/env python3

# Touhou Community Reliant Automatic Patcher
# Scripts
#
# ----
#
"""Utility functions shared among all the scripts."""

from collections import OrderedDict
import json
import os

json_load_params = {
    'object_pairs_hook': OrderedDict
}

def patch_files_filter(files):
    """Filters all file names that can not be among the content of a patch."""
    for i in files:
        if i != 'files.js':
            yield i


json_dump_params = {
    'ensure_ascii': False,
    'indent': '\t',
    'separators': (',', ': '),
    'sort_keys': True
}

# Default parameters for JSON input and output
def json_load(fn):
    with open(fn, 'r', encoding='utf-8') as file:
        return json.load(file, **json_load_params)


def json_store(fn, obj, dirs=['']):
    """Saves the JSON object [obj] to [fn], creating all necessary
    directories in the process. If [dirs] is given, the function is
    executed for every root directory in the array."""
    for i in dirs:
        full_fn = os.path.join(i, fn)
        os.makedirs(os.path.dirname(full_fn), exist_ok=True)
        with open(full_fn, 'w', encoding='utf-8') as file:
            json.dump(obj, file, **json_dump_params)
            file.write('\n')
