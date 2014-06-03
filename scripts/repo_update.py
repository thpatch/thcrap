#!/usr/bin/env python3

# Touhou Community Reliant Automatic Patcher
# Scripts
#
# ----
#
"""Construit et met à jour le dépôt des patchs pour le
Touhou Community Reliant Automatic Patcher."""

import shutil
import os
import argparse
import zlib
import utils

parser = argparse.ArgumentParser(
    description=__doc__
)
parser.add_argument(
    '-f', '--from',
    metavar="chemin",
    help="Chemin d'accès racine du dépôt. Reçoit aussi les copies du fichier "
         "files.js, les fichiers patch.js et repo.js ont été modifiés par ce "
         "script.",
    default='.',
    type=str,
    dest='f'
)

parser.add_argument(
    '-t', '--to',
    help="Dossier cible. S'il diffère du dossier source, "
         "tous les fichiers de tous les patchs sont copiés là.",
    metavar="chemin",
    default='.',
    type=str,
    dest='t'
)


def str_slash_normalize(string):
	return string.replace('\\', '/')


def enter_missing(obj, key, prompt):
    while not key in obj or not obj[key].strip():
        obj[key] = input(prompt)


def sizeof_fmt(num):
    for x in ['octet', 'Kio', 'Mio', 'Gio']:
        if num < 1024.0:
            return "%3.1f %s" % (num, x)
        num /= 1024.0
    return "%3.1f %s" % (num, 'Tio')


def patch_build(patch_id, servers, f, t):
    """Updates the patch in the [f]/[patch_id] directory.

    Ensures that patch.js contains all necessary keys and values, then updates
    the checksums in files.js and, if [t] differs from [f], copies all patch
    files from [f] to [t].

    Returns the contents of the patch ID key in repo.js."""
    f_path, t_path = [os.path.join(i, patch_id) for i in [f, t]]

    # Prepare patch.js.
    f_patch_fn = os.path.join(f_path, 'patch.js')
    patch_js = utils.json_load(f_patch_fn)

    enter_missing(
        patch_js, 'title',
        'Saisissez un joli titre pour "{}": '.format(patch_id)
    )
    patch_js['id'] = patch_id
    patch_js['servers'] = []
    # Delete obsolete keys.
    if 'files' in patch_js:
        del(patch_js['files'])

    for i in servers:
        url = os.path.join(i, patch_id) + '/'
        patch_js['servers'].append(str_slash_normalize(url))
    utils.json_store(f_patch_fn, patch_js)

    # Reset all old entries to a JSON null. This will delete any files on the
    # client side that no longer exist in the patch.
    try:
        files_js = utils.json_load(os.path.join(f_path, 'files.js'))
        for i in files_js:
            files_js[i] = None
    except FileNotFoundError:
        files_js = {}

    patch_size = 0
    print(patch_id, end='')
    for root, dirs, files in os.walk(f_path):
        for fn in utils.patch_files_filter(files):
            print('.', end='')
            f_fn = os.path.join(root, fn)
            patch_fn = f_fn[len(f_path) + 1:]
            t_fn = os.path.join(t_path, patch_fn)

            with open(f_fn, 'rb') as f_file:
                f_file_data = f_file.read()

            # Ensure Unix line endings for JSON input
            if fn.endswith(('.js', '.jdiff')) and b'\r\n' in f_file_data:
                f_file_data = f_file_data.replace(b'\r\n', b'\n')
                with open(f_fn, 'wb') as f_file:
                    f_file.write(f_file_data)

            f_sum = zlib.crc32(f_file_data) & 0xffffffff

            files_js[str_slash_normalize(patch_fn)] = f_sum
            patch_size += len(f_file_data)
            del(f_file_data)
            os.makedirs(os.path.dirname(t_fn), exist_ok=True)
            if f != t:
                shutil.copy2(f_fn, t_fn)

    utils.json_store('files.js', files_js, dirs=[f_path, t_path])
    print(
        '{num} files, {size}'.format(
            num=len(files_js), size=sizeof_fmt(patch_size)
        )
    )
    return patch_js['title']


def repo_build(f, t):
    try:
        f_repo_fn = os.path.join(f, 'repo.js')
        repo_js = utils.json_load(f_repo_fn)
    except FileNotFoundError:
        print(
            "Aucun fichier repo.js trouvé dans le dossier source. "
            "Création d'un nouveau dépôt."
        )
        repo_js = {}

    enter_missing(repo_js, 'id', "Saisissez l'identifiant d'un dépôt : ")
    enter_missing(repo_js, 'title', "Saisissez un joli titre de dépôt : ")
    enter_missing(
        repo_js, 'contact', "Saisissez une adresse courriel de contact : "
    )
    utils.json_store('repo.js', repo_js, dirs=[f, t])

    while not 'servers' in repo_js or not repo_js['servers'][0].strip():
        repo_js['servers'] = [input(
            "Saisissez l'URL publique de votre dépôt "
            "(le chemin d'accès qui contient le fichier repo.js) : "
        )]
    repo_js['patches'] = {}
    for root, dirs, files in os.walk(f):
        del(dirs)
        if 'patch.js' in files:
            patch_id = os.path.basename(root)
            repo_js['patches'][patch_id] = patch_build(
                patch_id, repo_js['servers'], f, t
            )
    print('Terminé.')
    utils.json_store('repo.js', repo_js, dirs=[f, t])


if __name__ == '__main__':
    arg = parser.parse_args()
    repo_build(arg.f, arg.t)
