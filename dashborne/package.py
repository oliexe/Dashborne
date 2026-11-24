"""Package the loader and separate patch definitions for a microSD card."""
import importlib.util
import json
from pathlib import Path
import re
import shutil

ROOT = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location('patch_package', ROOT / 'patches/package.py')
module = importlib.util.module_from_spec(spec); spec.loader.exec_module(module)


def package(source, destination, extensions=None):
    destination.mkdir(parents=True, exist_ok=True)
    catalog, ids, keys, controls = [], set(), set(), set()
    for path in sorted(source.glob('*.json')):
        patch = json.loads(path.read_text())
        id = patch['id']
        if not re.fullmatch('[a-z0-9_-]{1,31}', id) or id in ids:
            raise ValueError('Invalid or duplicate patch ID')
        if patch['operation'] == 'append-root-control':
            if patch['key'] in keys or patch['control']['id'] in controls:
                raise ValueError('Patch controls must use unique keys and IDs')
            keys.add(patch['key']); controls.add(patch['control']['id'])
        ids.add(id)
        title = patch.get('title', id.replace('-', ' ').title())
        description = patch.get('description', 'Compatible layout: ' + patch['base']['version'])
        if len(title.encode()) > 63 or len(description.encode()) > 127 or any(c in title + description for c in '\t\r\n'):
            raise ValueError('Invalid patch label')
        (destination / (id + '.dbp')).write_bytes(module.compile_patch(patch))
        catalog.append('\t'.join((id, title, description)))
    if extensions is not None:
        for path in sorted(Path(extensions).glob('*/manifest.json')):
            extension = json.loads(path.read_text())
            id = extension['id']
            if not re.fullmatch('[a-z0-9_-]{1,31}', id) or id in ids or extension.get('abi') != 1:
                raise ValueError('Invalid or duplicate extension ID/ABI')
            ids.add(id)
            if extension.get('core') or id in ('extra-menu', 'widget-runtime'):
                raise ValueError('Core services cannot be packaged as extensions')
            title, description = extension['title'], extension['description']
            if not title or len(title.encode()) > 63 or len(description.encode()) > 127 or any(c in title + description for c in '\t\r\n\0'):
                raise ValueError('Invalid extension labels')
            catalog.append('\t'.join((id, title, description, 'native')))
    if len(catalog) > 64:
        raise ValueError('This loader supports up to 64 extensions')
    (destination / 'catalog.tsv').write_text('\n'.join(catalog) + '\n')


if __name__ == '__main__':
    card = ROOT / 'build/card/Dashborne'
    if (card / 'patches').exists(): shutil.rmtree(card / 'patches')
    package(ROOT / 'patches/definitions', card / 'patches', card / 'extensions')
    (card / 'ui').mkdir(parents=True, exist_ok=True)
    shutil.copyfile(ROOT / 'assets/icon.png', card / 'ui/icon.png')
    shutil.copytree(ROOT / 'core/extra-menu/assets', card / 'assets/extra-menu', dirs_exist_ok=True)
    shutil.copytree(ROOT / 'core/widget-runtime/assets', card / 'assets/widget-runtime', dirs_exist_ok=True)
