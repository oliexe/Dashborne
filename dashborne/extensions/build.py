"""Cross-build source extensions and package their own assets for the SD loader."""
import json
import os
from pathlib import Path
import re
import shutil
import subprocess

ROOT = Path(__file__).resolve().parent
REPO = ROOT.parents[1]


def manifest(path):
    data = json.loads(Path(path).read_text())
    if not re.fullmatch(r'[a-z0-9_-]{1,31}', data['id']) or data.get('abi') != 1:
        raise ValueError('Unsupported extension ID or ABI')
    for key, limit in [('title', 63), ('description', 127)]:
        text = data[key]
        if not text or len(text.encode()) > limit or any(c in text for c in '\t\r\n\0'):
            raise ValueError('Invalid extension label')
    return data


def build(source=REPO / 'extensions', destination=ROOT.parent / 'build/card/Dashborne/extensions'):
    clang = os.environ.get('DASHBORNE_CC', 'clang')
    linker = os.environ.get('DASHBORNE_LD', 'ld.lld')
    destination = Path(destination)
    if destination.exists(): shutil.rmtree(destination)
    seen = set()
    for file in sorted(Path(source).glob('*/manifest.json')):
        spec = manifest(file); folder = file.parent.resolve()
        if spec.get('core') or spec['id'] in ('extra-menu', 'widget-runtime'):
            raise ValueError('Core services are built into Dashborne')
        if spec['id'] in seen: raise ValueError('Duplicate extension ID')
        seen.add(spec['id'])
        output = Path(destination) / spec['id']; output.mkdir(parents=True, exist_ok=True)
        objects = ROOT / 'build' / spec['id']; objects.mkdir(parents=True, exist_ok=True)
        compiled = []
        sources = spec.get('sources', ['extension.c'])
        if not sources: raise ValueError('An extension needs source files')
        for i, name in enumerate(sources):
            source_file = (folder / name).resolve()
            if not source_file.is_relative_to(folder) or source_file.suffix != '.c':
                raise ValueError('The bundled builder accepts extension-local C sources')
            obj = objects / f'{i}.o'
            subprocess.run([clang, '--target=armv7-linux-gnueabihf', '-march=armv7-a',
                            '-mfpu=vfpv3-d16', '-mfloat-abi=hard', '-marm', '-fPIC',
                            '-ffreestanding', '-fno-stack-protector', '-fno-builtin',
                            '-fvisibility=hidden', '-Os', '-Wall', '-Wextra', '-Werror',
                            '-I', str(ROOT), '-c', str(source_file), '-o', str(obj)], check=True)
            compiled.append(str(obj))
        subprocess.run([linker, '-shared', '-z', 'noexecstack', '-soname', 'libextension.so',
                        *compiled, '-o', str(output / 'libextension.so')], check=True)
        for asset in sorted((folder / 'assets').rglob('*')):
            if asset.is_file():
                if not asset.resolve().is_relative_to(folder): raise ValueError('Asset escapes extension')
                target = output / asset.relative_to(folder); target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(asset, target)
        for shared in spec.get('asset_sources', []):
            if not re.fullmatch(r'[a-z0-9_-]{1,31}', shared['extension']):
                raise ValueError('Invalid shared asset extension')
            asset_root = (Path(source) / shared['extension'] / 'assets').resolve()
            if not asset_root.is_relative_to(Path(source).resolve()):
                raise ValueError('Shared asset source escapes extensions')
            for name in shared['paths']:
                relative = Path(name)
                origin = asset_root / relative
                if relative.is_absolute() or '..' in relative.parts or not origin.resolve().is_relative_to(asset_root):
                    raise ValueError('Shared asset path escapes assets')
                if not origin.exists(): raise ValueError('Shared asset missing')
                assets = sorted(origin.rglob('*')) if origin.is_dir() else [origin]
                for asset in assets:
                    if not asset.is_file(): continue
                    if not asset.resolve().is_relative_to(asset_root):
                        raise ValueError('Shared asset link escapes assets')
                    target = output / 'assets' / asset.relative_to(asset_root)
                    if target.exists(): raise ValueError('Duplicate shared asset destination')
                    target.parent.mkdir(parents=True, exist_ok=True)
                    shutil.copyfile(asset, target)
        (output / 'manifest.json').write_text(json.dumps(spec, indent=2) + '\n')


if __name__ == '__main__':
    build()
