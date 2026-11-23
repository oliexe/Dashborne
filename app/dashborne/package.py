"""Package the loader and separate patch definitions for a microSD card."""
import importlib.util
import json
from pathlib import Path
import re
import shutil

ROOT = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location('patch_package', ROOT.parent / 'injector/package.py')
module = importlib.util.module_from_spec(spec); spec.loader.exec_module(module)


def package(source, destination):
    destination.mkdir(parents=True, exist_ok=True)
    catalog, ids, keys, controls = [], set(), set(), set()
    for path in sorted(source.glob('*.json')):
        patch = json.loads(path.read_text())
        id = patch['id']
        if not re.fullmatch('[a-z0-9_-]{1,31}', id) or id in ids:
            raise ValueError('Invalid or duplicate patch ID')
        if patch['key'] in keys or patch['control']['id'] in controls:
            raise ValueError('Patch controls must use unique keys and IDs')
        ids.add(id); keys.add(patch['key']); controls.add(patch['control']['id'])
        title = patch.get('title', id.replace('-', ' ').title())
        description = patch.get('description', 'Compatible layout: ' + patch['base']['version'])
        if len(title.encode()) > 63 or len(description.encode()) > 127 or any(c in title + description for c in '\t\r\n'):
            raise ValueError('Invalid patch label')
        (destination / (id + '.dbp')).write_bytes(module.compile_patch(patch))
        catalog.append('\t'.join((id, title, description)))
    if len(catalog) > 16:
        raise ValueError('This loader supports up to 16 patches')
    (destination / 'catalog.tsv').write_text('\n'.join(catalog) + '\n')


if __name__ == '__main__':
    card = ROOT / 'build/card/Dashborne'
    package(ROOT.parent / 'injector/patches', card / 'patches')
    (card / 'ui').mkdir(parents=True, exist_ok=True)
    shutil.copyfile(ROOT / 'assets/icon.png', card / 'ui/icon.png')
