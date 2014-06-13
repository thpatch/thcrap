#!/usr/bin/env python3

# Touhou Community Reliant Automatic Patcher
# Scripts
#
# ----
#
"""Creates a digital signature for a thcrap release archive."""

from Crypto.Signature import PKCS1_v1_5
from Crypto.PublicKey import RSA
import argparse
import base64
import importlib
import utils

thcrap_hashes = [
    'SHA1', 'SHA256'
]

parser = argparse.ArgumentParser(
    description=__doc__
)

parser.add_argument(
    'arc',
    help='Archive à signer. Extension .sig ajoutée au nom du fichier '
	     'signature.'
)

parser.add_argument(
    '-a', '--alg',
    help='Algorithme de hachage à utiliser, en dehors de ceux supportés par '
         'thcrap.',
    choices=thcrap_hashes,
    default=thcrap_hashes[0]
)

parser.add_argument(
    '-k', '--key',
    help='Fichier contenant la clé privée.',
    required=True
)


def alg_mod_from_str(alg):
    """Returns the PyCrypto hash module that corresponds to [alg]."""
    if alg is 'SHA1':
        alg = 'SHA'
    return importlib.import_module('Crypto.Hash.' + alg)


def sign(arc_fn, alg, key_fn):
    alg_mod = alg_mod_from_str(alg)
    with open(key_fn, 'r') as key_file:
        key = RSA.importKey(key_file.read())
    signer = PKCS1_v1_5.new(key)
    ret = {'alg': alg}
    with open(arc_fn, 'rb') as arc_file:
        m = alg_mod.new(arc_file.read())
        signature = signer.sign(m)
        ret['sig'] = base64.b64encode(signature).decode('ascii')
    return ret


if __name__ == '__main__':
    arg = parser.parse_args()
    sig = sign(arg.arc, arg.alg, arg.key)
    utils.json_store(arg.arc + '.sig', sig)
