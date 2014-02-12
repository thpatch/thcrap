#!/usr/bin/env python3

# Touhou Community Reliant Automatic Patcher
# Scripts
#
# ----
#
"""Utility functions shared among all the scripts."""

import json
import os

# Default parameters for JSON input and output
def json_load(fn):
    with open(fn, 'r', encoding='utf-8') as file:
        return json.load(file)


def json_store(fn, obj, dirs=['']):
    """Saves the JSON object [obj] to [fn], creating all necessary
    directories in the process. If [dirs] is given, the function is
    executed for every root directory in the array."""
    for i in dirs:
        full_fn = os.path.join(i, fn)
        os.makedirs(os.path.dirname(full_fn), exist_ok=True)
        with open(full_fn, 'w', encoding='utf-8') as file:
            json.dump(
                obj, file, ensure_ascii=False, sort_keys=True, indent='\t',
                separators=(',', ': ')
            )
            file.write('\n')
